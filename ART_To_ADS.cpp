#include "DTrackSDK.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <csignal>
#include <map>
#include <string>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include "TcAdsDef.h"
#include "TcAdsAPI.h"
#include <cstdint>



#pragma comment(lib, "ws2_32.lib")

// --- Arrêt propre avec Ctrl+C ---
static volatile sig_atomic_t g_stop = 0;
static void onSignal(int) { g_stop = 1; }

// --- Conversion matrice de rotation (stockée colonne par colonne) -> angles d'Euler ZYX (degrés) ---
// Convention : R = Rz(yaw) * Ry(pitch) * Rx(roll)
static void rotToEulerDeg(const double rot[9], double euler[3])
{
    auto R = [&](int ligne, int col) -> double { return rot[col * 3 + ligne]; }; // rot est "column-wise"

    double r20 = R(2, 0);
    double pitch = std::asin(std::max(-1.0, std::min(1.0, -r20)));
    double roll, yaw;

    if (std::fabs(r20) < 0.9999999) {
        roll = std::atan2(R(2, 1), R(2, 2));
        yaw  = std::atan2(R(1, 0), R(0, 0));
    } else { // gimbal lock
        yaw  = 0.0;
        roll = std::atan2((r20 < 0.0 ? -1.0 : 1.0) * R(0, 1), R(1, 1));
    }

    const double rad2deg = 180.0 / 3.14159265358979323846;
    euler[0] = roll  * rad2deg;
    euler[1] = pitch * rad2deg;
    euler[2] = yaw   * rad2deg;
}

// --- Noms des bodies 
static std::string bodyName(int id)
{
    static const std::map<int, std::string> names = {
        {0, "Body_0"}, 
        {1, "Body_1"},
        {2, "Body_2(plateforme)"},
    };
    auto it = names.find(id);
    return (it != names.end()) ? it->second : ("Body_" + std::to_string(id));
}

void printAmsAddr(const AmsAddr& addr) {
    std::cout << "addresse AMS : ";
    for (int i = 0; i < 6; ++i) {
        std::cout << static_cast<int>(addr.netId.b[i]);
        if (i < 5) {
            std::cout << ".";
        }
    }
    std::cout << ":" << addr.port << std::endl;
}

int main()
{
    //************************************ 
    //*********DTRACK INITIALIZATION **** 
    //********************************** 
    const char* controllerIp = "192.168.1.1";   // IP du contrôleur DTrack
    const unsigned short port = 5000;           // port UDP local de réception DTrack

    // --- Initialisation Winsock ---
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Erreur WSAStartup\n";
        return 1;
    }

    std::signal(SIGINT, onSignal);   // Ctrl+C -> arrêt propre
    std::signal(SIGTERM, onSignal);

    // --- Initialisation DTrack ---
    DTrackSDK* dt = new DTrackSDK(controllerIp, port);

    if (!dt->isCommandInterfaceValid()) {
        std::cerr << "Connexion au controleur impossible\n";
        delete dt; WSACleanup();
        return 1;
    }

    if (!dt->startMeasurement()) {
        std::cerr << "Demarrage du tracking impossible\n";
        delete dt; WSACleanup();
        return 1;
    }

    std::cout << std::fixed << std::setprecision(3);

    //*****************************************
    //*********TwinCAT ADS INITIALIZATION **** 
    //*************************************** 
    // étape 1 : ouvrir le port local
    long nPort, nErr = 0;
    nPort = AdsPortOpenEx();
    AmsAddr addr;
    PAmsAddr paddr = &addr;
    //étape 2 : Construire l'addresse de la cible
    nErr = AdsGetLocalAddressEx(nPort,paddr);
    addr.port = 350;
    printAmsAddr(addr);
    if(nErr != 0){
        std::cerr << "Erreur lors de l'ouverture du port Local : " << nErr << '\n' ; 
    }

    // étape 3 : obtenir les handleInputs des variables

    char nameInputSymbol[] = "Object1 (Module1).Inputs.Value";
    unsigned long handleInput =0;
    unsigned long nRead = 0;
    // get handleInput
    nErr = AdsSyncReadWriteReqEx2(nPort, paddr,
                            ADSIGRP_SYM_HNDBYNAME, 0,
                            sizeof(handleInput), &handleInput,
                            sizeof(nameInputSymbol), nameInputSymbol,
                            &nRead);
    if (nErr) { std::cerr << "nerr Input handle: " << nErr << '\n'; return 1; }

    // étape 4 : boucle de lecture des données DTrack et écriture dans TwinCAT ADS
    while (!g_stop) {
        if (!dt->receive()) continue;   // timeout ou paquet ignoré

        for (int i = 0; i < dt->getNumBody(); i++) {
            const DTrackBody* body = dt->getBody(i);
            if (!body || !body->isTracked()) continue;

            double euler[3];
            rotToEulerDeg(body->rot, euler);

            std::cout << "=== " << bodyName(body->id)
                      << " (id " << body->id << ", qualite " << body->quality << ") ===\n";
            
            double x = body->loc[0];

            // Position (mm)
            std::cout << "Position [mm]        : x=" << body->loc[0]
                      << "  y=" << body->loc[1]
                      << "  z=" << body->loc[2] << "\n";

            nErr = AdsSyncWriteReqEx(nPort, paddr,
                        ADSIGRP_SYM_VALBYHND,   // écriture par handleInputdans TwinCAT
                        handleInput,                 
                        sizeof(body->loc[0]), &x);  // écriture de la position x

            if (nErr) { std::cerr << "Write: " << nErr << '\n'; return 1; }

            // Rotation en angles d'Euler (degrés)
            std::cout << "Rotation Euler [deg] : roll=" << euler[0]
                      << "  pitch=" << euler[1]
                      << "  yaw=" << euler[2] << "\n";

            // Matrice de rotation 3x3, affichée ligne par ligne
            // (rot[] est column-wise : la ligne L contient rot[L], rot[L+3], rot[L+6])
            std::cout << "Matrice de rotation  :\n";
            for (int l = 0; l < 3; l++) {
                std::cout << "  [ " << std::setw(9) << body->rot[l]
                          << "  " << std::setw(9) << body->rot[l + 3]
                          << "  " << std::setw(9) << body->rot[l + 6] << " ]\n";
            }
            std::cout << "\n";
        }
    }

    // --- Nettoyage  ---
    dt->stopMeasurement();
    delete dt;
    WSACleanup();
    return 0;
}