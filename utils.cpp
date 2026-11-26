#include "utils.h"
#include <algorithm>
#include <cmath>
#include <map>

void rotToEulerDeg(const double rot[9], double euler[3])
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

std::string bodyName(int id)
{
    static const std::map<int, std::string> names = {
        {0, "Body_0"},
        {1, "Body_1"},
        {2, "Body_2(plateforme)"},
    };
    auto it = names.find(id);
    return (it != names.end()) ? it->second : ("Body_" + std::to_string(id));
}
