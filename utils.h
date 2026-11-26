#pragma once
#include <string>

// Matrice de rotation (colonne par colonne) -> angles d'Euler ZYX (degrés)
// Convention : R = Rz(yaw) * Ry(pitch) * Rx(roll)
void rotToEulerDeg(const double rot[9], double euler[3]);

// Nom lisible d'un body
std::string bodyName(int id);