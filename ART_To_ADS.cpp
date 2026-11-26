#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <csignal>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include "ads.h"
#include "art.h"
#include "config.h"
#include "shared.h"
#include "utils.h"

#pragma comment(lib, "ws2_32.lib")

static void onSignal(int) { g_state.stop = true; }

// Affichage ralenti (5 Hz) : n'impacte ni la réception ni l'écriture
static void displayLoop()
{
    SharedState& s = g_state;
    std::cout << std::fixed << std::setprecision(3);

    while (!s.stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        BodyData d;
        uint64_t seq;
        {
            std::lock_guard<std::mutex> lk(s.mtx);
            d = s.last;
            seq = s.seq;
        }
        if (seq == 0) {
            std::cout << "En attente de donnees (timeouts DTrack : " << s.nbTimeouts << ")\n";
            continue;
        }

        std::cout << "=== " << bodyName(TARGET_BODY_ID) << " ===  "
                  << "recus=" << s.nbReceived << "  ecrits=" << s.nbWritten
                  << "  timeouts=" << s.nbTimeouts << "  ADS>10ms=" << s.nbSlowAds << "\n";
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
}

int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Erreur WSAStartup\n";
        return 1;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    // --- TwinCAT ADS ---
    AdsContext ads;
    if (!adsInit(ads)) {
        WSACleanup();
        return 1;
    }

    // --- DTrack ---
    DTrackSDK* dt = artInit();
    if (!dt) {
        adsRelease(ads);
        WSACleanup();
        return 1;
    }

    // --- Threads ---
    std::thread tArt(artThread, dt);
    std::thread tAds(adsThread, ads);

    displayLoop();

    // --- Arrêt propre ---
    g_state.cv.notify_all();
    tArt.join();
    tAds.join();

    artShutdown(dt);
    adsRelease(ads);
    WSACleanup();
    return 0;
}
