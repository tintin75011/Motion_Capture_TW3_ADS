#include "art.h"
#include "DTrackSDK.hpp"
#include "IpFinder.h"
#include "config.h"
#include "shared.h"
#include "utils.h"
#include <algorithm>
#include <iostream>
#include <string>

DTrackSDK* artInit()
{
    DTrackSDK* dt = new DTrackSDK(CONTROLLER_IP, DTRACK_PORT);

    IpFinder finder(CONTROLLER_IP);
    if (!finder.findInterfaceReachingTarget()) {
        std::cerr << "Aucune interface locale ne peut atteindre " << CONTROLLER_IP << "\n";
        delete dt;
        return nullptr;
    }
    std::string monIp = finder.reachingIp();
    std::cout << "Interface locale trouvee : " << finder.reachingInterface().name
              << " (" << monIp << ")\n";

    std::string command  = "dtrack2 set output net ch01 udp " + monIp + " " + std::to_string(DTRACK_PORT);
    std::string response;
    dt->sendDTrack2Command(command, &response);

    if (!dt->isCommandInterfaceValid()) {
        std::cerr << "Connexion au controleur impossible\n";
        delete dt;
        return nullptr;
    }
    if (!dt->startMeasurement()) {
        std::cerr << "Demarrage du tracking impossible\n";
        delete dt;
        return nullptr;
    }
    return dt;
}

void artShutdown(DTrackSDK* dt)
{
    if (!dt) return;
    dt->stopMeasurement();
    delete dt;
}

// Ne fait QUE recevoir, convertir et publier : aucun print, aucun appel ADS.
void artThread(DTrackSDK* dt)
{
    SharedState& s = g_state;

    while (!s.stop) {
        if (!dt->receive()) {
            switch (dt->getLastDataError()) {
                case DTrackSDK::ERR_TIMEOUT: s.nbTimeouts++; break;
                case DTrackSDK::ERR_NET:     std::cerr << "[DTrack] erreur reseau\n";  break;
                case DTrackSDK::ERR_PARSE:   std::cerr << "[DTrack] erreur parsing\n"; break;
                default: break;
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
                std::lock_guard<std::mutex> lk(s.mtx);
                s.last = d;
                s.seq++;
            }
            s.cv.notify_one();
            s.nbReceived++;
        }
    }
    s.cv.notify_all();
}