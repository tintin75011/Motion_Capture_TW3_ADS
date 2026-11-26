#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

// Structure échangée avec TwinCAT (disposition mémoire IDENTIQUE à la STRUCT côté TwinCAT)
// 15 x LREAL = 120 octets
#pragma pack(push, 1)
struct BodyData {
    double loc[3];     // x, y, z (mm)
    double euler[3];   // roll, pitch, yaw (deg)
    double rot[9];     // matrice 3x3, colonne par colonne : rot[col*3 + ligne]
};
#pragma pack(pop)
static_assert(sizeof(BodyData) == 15 * sizeof(double), "taille de BodyData inattendue");

// Données partagées entre les threads (réception DTrack / écriture ADS / affichage)
struct SharedState {
    std::atomic<bool>       stop{ false };

    std::mutex              mtx;
    std::condition_variable cv;
    BodyData                last{};      // dernier échantillon reçu (protégé par mtx)
    uint64_t                seq = 0;     // n° du dernier échantillon (protégé par mtx)

    // Statistiques (lecture sans verrou)
    std::atomic<uint64_t>   nbReceived{ 0 };
    std::atomic<uint64_t>   nbWritten{ 0 };
    std::atomic<uint64_t>   nbTimeouts{ 0 };
    std::atomic<uint64_t>   nbSlowAds{ 0 };
};

extern SharedState g_state;   // défini dans shared.cpp