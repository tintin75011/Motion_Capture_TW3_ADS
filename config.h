#pragma once
// ============================================================
//  Configuration (tous les paramètres modifiables au même endroit)
// ============================================================

// --- DTrack (ART) ---
static const char*          CONTROLLER_IP  = "192.168.1.1";   // IP du contrôleur DTrack
static const unsigned short DTRACK_PORT    = 5000;            // port UDP local de réception
static const int            TARGET_BODY_ID = 1;               // body à transmettre à TwinCAT

// --- TwinCAT ADS ---
static const unsigned long  ADS_PORT       = 350;             // port du runtime hébergeant le symbole
static const char           ADS_SYMBOL[]   = "Object1 (Module1).Body_Loc.Body_1";
