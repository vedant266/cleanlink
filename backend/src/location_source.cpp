#include "location_source.h"
#include "../api/photo_location.h"

#include <iostream>

using namespace std;

bool PhotoLocationSource::getLocation(const std::string &photoPath,
                                      double &latitude,
                                      double &longitude)
{
    if (getPhotoLocation(photoPath, latitude, longitude))
    {
        cout << "GPS found in photo: " << latitude << ", " << longitude << endl;
        return true;
    }

    return false;
}

bool ManualLocationSource::getLocation(const std::string &photoPath,
                                       double &latitude,
                                       double &longitude)
{
    // The photo path isn't needed here, the user types the location
    (void)photoPath;

    cout << "No GPS in photo. Enter location manually.\n";

    cout << "Enter Latitude: ";
    cin >> latitude;

    cout << "Enter Longitude: ";
    cin >> longitude;

    return true;
}