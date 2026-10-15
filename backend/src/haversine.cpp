#include <iostream>
#include <cmath>
#include "haversine.h"

using namespace std;

double haversineDistance(
    double lat1, double lon1,
    double lat2, double lon2)
{
    const double R = 6371.0;
    const double PI = 3.14159265358979323846;

    double dLat = (lat2 - lat1) * PI / 180.0;
    double dLon = (lon2 - lon1) * PI / 180.0;

    lat1 = lat1 * PI / 180.0;
    lat2 = lat2 * PI / 180.0;

    double a = sin(dLat / 2) * sin(dLat / 2)
             + cos(lat1) * cos(lat2)
             * sin(dLon / 2) * sin(dLon / 2);

    if (a > 1.0)
        a = 1.0;

    if (a < 0.0)
        a = 0.0;

    double c = 2 * atan2(sqrt(a), sqrt(1.0 - a));

    return R * c;
}