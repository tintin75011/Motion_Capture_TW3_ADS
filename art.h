#pragma once

class DTrackSDK;

// Détecte l'interface locale, configure la sortie UDP du contrôleur et démarre le tracking.
// Retourne nullptr en cas d'échec (rien à libérer dans ce cas).
DTrackSDK* artInit();

// Arrête le tracking et libère l'objet
void artShutdown(DTrackSDK* dt);

// Thread de réception : reçoit, convertit et publie le dernier échantillon
void artThread(DTrackSDK* dt);