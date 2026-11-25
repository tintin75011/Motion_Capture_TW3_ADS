#include "DTrackSDK.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <csignal>
#include <map>
#include <string>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include "TcAdsDef.h"
#include "TcAdsAPI.h"
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

// ============================================================
//  Configuration
// ============================================================
static const char*          CONTROLLER_IP  = "192.168.1.1";   // IP du contrôleur DTrack
static const unsigned short DTRACK_PORT    = 5000;            // port UDP local de réception
static const int            TARGET_BODY_ID = 1;               // body à transmettre à TwinCAT
static const unsigned long  ADS_PORT       = 350;             // port du runtime hébergeant le symbole
static char                 ADS_SYMBOL[]   = "Object1 (Module1).Body_Loc.Body_1";

// ============================================================
//  Structure échangée avec TwinCAT (disposition mémoire IDENTIQUE à la STRUCT côté TwinCAT)
//  15 x LREAL = 120 octets
// ============================================================
#pragma pack(push, 1)
struct BodyData {
    double loc[3];     // x, y, z (mm)
    double euler[3];   // roll, pitch, yaw (deg)
    double rot[9];     // matrice 3x3, colonne par colonne : rot[col*3 + ligne]
};
#pragma pack(pop)
static_assert(sizeof(BodyData) == 15 * sizeof(double), "taille de BodyData inattendue");

// ============================================================
//  Données partagées entre les threads
// ============================================================
static std::atomic<bool> g_stop{ false };
static void onSignal(int) { g_stop = true; }

static std::mutex              g_mtx;
static std::condition_variable g_cv;
static BodyData                g_last{};          // dernier échantillon reçu
static uint64_t                g_seq = 0;         // numéro du dernier échantillon (protégé par g_mtx)

// Statistiques (lecture sans verrou)
static std::atomic<uint64_t> g_nbReceived{ 0 };
static std::atomic<uint64_t> g_nbWritten{ 0 };
static std::atomic<uint64_t> g_nbTimeouts{ 0 };
static std::atomic<uint64_t> g_nbSlowAds{ 0 };

// ============================================================
//  Utilitaires
// ============================================================

// Matrice de rotation (colonne par colonne) -> angles d'Euler ZYX (degrés)
// Convention : R = Rz(yaw) * Ry(pitch) * Rx(roll)
static void rotToEulerDeg(const double rot[9], double euler[3])
{
    auto R = [&](int ligne, int col) -> double { return rot[col * 3 + ligne]; };

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

static void printAmsAddr(const AmsAddr& addr)
{
    std::cout << "adresse AMS : ";
    for (int i = 0; i < 6; ++i) {
        std::cout << static_cast<int>(addr.netId.b[i]);
        if (i < 5) std::cout << ".";
    }
    std::cout << ":" << addr.port << std::endl;
}

// ============================================================
//  THREAD 1 : réception DTrack
//  Ne fait QUE recevoir, convertir et publier le dernier échantillon.
//  Aucun print, aucun appel ADS => les paquets UDP sont lus au plus vite.
// ============================================================
static void dtrackThread(DTrackSDK* dt)
{
    while (!g_stop) {
        if (!dt->receive()) {
            if (dt->getLastDataError() == DTrackSDK::ERR_TIMEOUT) {
                g_nbTimeouts++;
            } else if (dt->getLastDataError() == DTrackSDK::ERR_NET) {
                std::cerr << "[DTrack] erreur reseau\n";
            } else if (dt->getLastDataError() == DTrackSDK::ERR_PARSE) {
                std::cerr << "[DTrack] erreur parsing\n";
            }
            continue;
        }

        for (int i = 0; i < dt->getNumBody(); i++) {
            const DTrackBody* body = dt->getBody(i);
            if (!body || !body->isTracked() || body->id != TARGET_BODY_ID) continue;

            BodyData d;
            std::copy(body->loc, body->loc + 3, d.loc);
            std::copy(body->rot, body->rot + 9, d.rot);
            rotToEulerDeg(body->rot, d.euler);

            {
                std::lock_guard<std::mutex> lk(g_mtx);
                g_last = d;
                g_seq++;
            }
            g_cv.notify_one();
            g_nbReceived++;
        }
    }
    g_cv.notify_all();
}

// ============================================================
//  THREAD 2 : écriture TwinCAT ADS
//  Attend un nouvel échantillon, écrit TOUTE la structure en une seule requête ADS.
//  Si ADS est lent, on saute les échantillons intermédiaires (on écrit toujours le plus récent).
// ============================================================
static void adsThread(long nPort, AmsAddr addr, unsigned long hBody)
{
    uint64_t lastSeq = 0;

    while (!g_stop) {
        BodyData d;
        {
            std::unique_lock<std::mutex> lk(g_mtx);
            // réveil périodique pour tester g_stop
            if (!g_cv.wait_for(lk, std::chrono::milliseconds(100),
                               [&] { return g_seq != lastSeq || g_stop; }))
                continue;
            if (g_stop) break;
            d = g_last;
            lastSeq = g_seq;
        }

        auto t0 = std::chrono::steady_clock::now();
        long nErr = AdsSyncWriteReqEx(nPort, &addr,
                                      ADSIGRP_SYM_VALBYHND, hBody,
                                      sizeof(d), &d);
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0).count();

        if (nErr) {
            std::cerr << "[ADS] Write Error: " << nErr << '\n';
            g_stop = true;
            break;
        }
        if (ms > 10.0) g_nbSlowAds++;
        g_nbWritten++;
    }
}

// ============================================================
//  main : initialisation, lancement des threads, affichage ralenti (5 Hz)
// ============================================================
int main()
{
    // --- Winsock ---
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Erreur WSAStartup\n";
        return 1;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    // --- TwinCAT ADS ---
    long nPort = AdsPortOpenEx();
    AmsAddr addr;
    long nErr = AdsGetLocalAddressEx(nPort, &addr);
    if (nErr) {
        std::cerr << "Erreur AdsGetLocalAddressEx : " << nErr << '\n';
        AdsPortCloseEx(nPort); WSACleanup();
        return 1;
    }
    addr.port = ADS_PORT;
    printAmsAddr(addr);

    unsigned long hBody = 0;
    unsigned long nRead = 0;
    nErr = AdsSyncReadWriteReqEx2(nPort, &addr,
                                  ADSIGRP_SYM_HNDBYNAME, 0,
                                  sizeof(hBody), &hBody,
                                  sizeof(ADS_SYMBOL), ADS_SYMBOL,
                                  &nRead);
    if (nErr) {
        std::cerr << "Erreur handle ADS : " << nErr << '\n';
        AdsPortCloseEx(nPort); WSACleanup();
        return 1;
    }

    // --- DTrack ---
    DTrackSDK* dt = new DTrackSDK(CONTROLLER_IP, DTRACK_PORT);

    if (!dt->isCommandInterfaceValid()) {
        std::cerr << "Connexion au controleur impossible\n";
        AdsSyncWriteReqEx(nPort, &addr, ADSIGRP_SYM_RELEASEHND, 0, sizeof(hBody), &hBody);
        AdsPortCloseEx(nPort);
        delete dt; WSACleanup();
        return 1;
    }
    if (!dt->startMeasurement()) {
        std::cerr << "Demarrage du tracking impossible\n";
        AdsSyncWriteReqEx(nPort, &addr, ADSIGRP_SYM_RELEASEHND, 0, sizeof(hBody), &hBody);
        AdsPortCloseEx(nPort);
        delete dt; WSACleanup();
        return 1;
    }

    // --- Lancement des deux threads ---
    std::thread tDtrack(dtrackThread, dt);
    std::thread tAds(adsThread, nPort, addr, hBody);

    // --- Affichage ralenti dans le thread principal (n'impacte ni la réception ni l'écriture) ---
    std::cout << std::fixed << std::setprecision(3);
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        BodyData d;
        uint64_t seq;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            d = g_last;
            seq = g_seq;
        }
        if (seq == 0) {
            std::cout << "En attente de donnees (timeouts DTrack : " << g_nbTimeouts << ")\n";
            continue;
        }

        std::cout << "=== " << bodyName(TARGET_BODY_ID) << " ===  "
                  << "recus=" << g_nbReceived << "  ecrits=" << g_nbWritten
                  << "  timeouts=" << g_nbTimeouts << "  ADS>10ms=" << g_nbSlowAds << "\n";
        std::cout << "Position [mm]        : x=" << d.loc[0] << "  y=" << d.loc[1] << "  z=" << d.loc[2] << "\n";
        std::cout << "Rotation Euler [deg] : roll=" << d.euler[0] << "  pitch=" << d.euler[1] << "  yaw=" << d.euler[2] << "\n";
        std::cout << "Matrice de rotation  :\n";
        for (int l = 0; l < 3; l++) {
            std::cout << "  [ " << std::setw(9) << d.rot[l]
                      << "  " << std::setw(9) << d.rot[l + 3]
                      << "  " << std::setw(9) << d.rot[l + 6] << " ]\n";
        }
        std::cout << "\n";
    }

    // --- Arrêt propre ---
    g_cv.notify_all();
    tDtrack.join();
    tAds.join();

    dt->stopMeasurement();
    delete dt;

    AdsSyncWriteReqEx(nPort, &addr, ADSIGRP_SYM_RELEASEHND, 0, sizeof(hBody), &hBody);
    AdsPortCloseEx(nPort);
    WSACleanup();
    return 0;
}