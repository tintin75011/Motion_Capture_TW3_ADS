#pragma once
#include <winsock2.h>
#include <windows.h>
#include "TcAdsDef.h"
#include "TcAdsAPI.h"

// Contexte de connexion ADS
struct AdsContext {
    long          port  = 0;
    AmsAddr       addr{};
    unsigned long hBody = 0;   // handle du symbole
};

// Ouvre le port, récupère l'adresse AMS et le handle du symbole. false si erreur.
bool adsInit(AdsContext& ctx);

// Libère le handle et ferme le port (sans danger si adsInit a échoué)
void adsRelease(AdsContext& ctx);

// Thread d'écriture : écrit toujours le dernier échantillon disponible
void adsThread(AdsContext ctx);
