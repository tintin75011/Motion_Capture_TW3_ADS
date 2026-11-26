#include "ads.h"
#include "config.h"
#include "shared.h"
#include <chrono>
#include <iostream>

static void printAmsAddr(const AmsAddr& addr)
{
    std::cout << "adresse AMS : ";
    for (int i = 0; i < 6; ++i) {
        std::cout << static_cast<int>(addr.netId.b[i]);
        if (i < 5) std::cout << ".";
    }
    std::cout << ":" << addr.port << std::endl;
}

bool adsInit(AdsContext& ctx)
{
    ctx.port = AdsPortOpenEx();
    long nErr = AdsGetLocalAddressEx(ctx.port, &ctx.addr);
    if (nErr) {
        std::cerr << "Erreur AdsGetLocalAddressEx : " << nErr << '\n';
        adsRelease(ctx);
        return false;
    }
    ctx.addr.port = ADS_PORT;
    printAmsAddr(ctx.addr);

    unsigned long nRead = 0;
    nErr = AdsSyncReadWriteReqEx2(ctx.port, &ctx.addr,
                                  ADSIGRP_SYM_HNDBYNAME, 0,
                                  sizeof(ctx.hBody), &ctx.hBody,
                                  sizeof(ADS_SYMBOL), const_cast<char*>(ADS_SYMBOL),
                                  &nRead);
    if (nErr) {
        std::cerr << "Erreur handle ADS : " << nErr << '\n';
        ctx.hBody = 0;
        adsRelease(ctx);
        return false;
    }
    return true;
}

void adsRelease(AdsContext& ctx)
{
    if (ctx.hBody) {
        AdsSyncWriteReqEx(ctx.port, &ctx.addr, ADSIGRP_SYM_RELEASEHND, 0,
                          sizeof(ctx.hBody), &ctx.hBody);
        ctx.hBody = 0;
    }
    if (ctx.port) {
        AdsPortCloseEx(ctx.port);
        ctx.port = 0;
    }
}

// Attend un nouvel échantillon, écrit TOUTE la structure en une seule requête ADS.
// Si ADS est lent, on saute les échantillons intermédiaires.
void adsThread(AdsContext ctx)
{
    SharedState& s = g_state;
    uint64_t lastSeq = 0;

    while (!s.stop) {
        BodyData d;
        {
            std::unique_lock<std::mutex> lk(s.mtx);
            if (!s.cv.wait_for(lk, std::chrono::milliseconds(100),
                               [&] { return s.seq != lastSeq || s.stop; }))
                continue;
            if (s.stop) break;
            d = s.last;
            lastSeq = s.seq;
        }

        auto t0 = std::chrono::steady_clock::now();
        long nErr = AdsSyncWriteReqEx(ctx.port, &ctx.addr,
                                      ADSIGRP_SYM_VALBYHND, ctx.hBody,
                                      sizeof(d), &d);
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0).count();

        if (nErr) {
            std::cerr << "[ADS] Write Error: " << nErr << '\n';
            s.stop = true;
            break;
        }
        if (ms > 10.0) s.nbSlowAds++;
        s.nbWritten++;
    }
}