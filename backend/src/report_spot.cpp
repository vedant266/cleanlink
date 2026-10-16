#include "../api/reverse_geocode.h"
#include "../api/photo_location.h"
#include "../include/report_spot.h"
#include <iostream>
#include <cstring>

using namespace std;

void ReportSpot::reportSpot(SpotList &spots)
{
    int reporterID;
    double latitude;
    double longitude;

    char photoPath[200];
    char wasteType[50];

    int sizeChoice;
    WasteSize estimatedSize;

    cout << "\n===== REPORT WASTE SPOT =====\n";

    cout << "Enter Reporter ID: ";
    cin >> reporterID;

    cout << "Enter Photo Path: ";
    cin >> photoPath;

    if (getPhotoLocation(photoPath, latitude, longitude))
    {
        cout << "GPS found in photo: " << latitude << ", " << longitude << endl;
    }
    else
    {
        cout << "No GPS in photo. Enter location manually.\n";

        cout << "Enter Latitude: ";
        cin >> latitude;

        cout << "Enter Longitude: ";
        cin >> longitude;
    }

    cout << "Enter Waste Type: ";
    cin >> wasteType;

    cout << "\nSelect Waste Size:\n";
    cout << "1. Small\n";
    cout << "2. Medium\n";
    cout << "3. Large\n";
    cout << "Enter choice: ";
    cin >> sizeChoice;

    if (sizeChoice == 1)
    {
        estimatedSize = SMALL;
    }
    else if (sizeChoice == 2)
    {
        estimatedSize = MEDIUM;
    }
    else if (sizeChoice == 3)
    {
        estimatedSize = LARGE;
    }
    else
    {
        cout << "Invalid size choice.\n";
        return;
    }

    int spotID = saveReport(spots, reporterID, latitude, longitude,
                            photoPath, wasteType, estimatedSize);

    cout << "\nWaste spot #" << spotID << " reported successfully!\n";
}

// Shared by the terminal menu and the web server
int ReportSpot::saveReport(SpotList &spots, int reporterID,
                           double latitude, double longitude,
                           const char *photoPath, const char *wasteType,
                           WasteSize estimatedSize)
{
    Location location = reverseGeocode(latitude, longitude);

    // Never save a blank city or locality
    bool cityKnown = !location.cityName.empty();

    if (!cityKnown)
        location.cityName = "Unknown";

    if (location.localityName.empty())
        location.localityName = location.cityName;

    cout << "\nLocation found:\n";
    cout << "City: " << location.cityName << endl;
    cout << "Locality: " << location.localityName << endl;

    if (cityKnown)
    {
        int cityIndex = cityTable.searchCity(location.cityName);

        if (cityIndex == -1)
        {
            cityTable.insertCity(location.cityName);
        }
        else
        {
            cout << "City already exists in City Hash Table.\n";
        }
    }

    WasteSpot spot = {};

    spot.reporterID = reporterID;
    spot.latitude = latitude;
    spot.longitude = longitude;

    strncpy(spot.cityName, location.cityName.c_str(), sizeof(spot.cityName) - 1);
    strncpy(spot.localityName, location.localityName.c_str(), sizeof(spot.localityName) - 1);

    strncpy(spot.photoPath, photoPath, sizeof(spot.photoPath) - 1);
    strncpy(spot.wasteType, wasteType, sizeof(spot.wasteType) - 1);

    spot.estimatedSize = estimatedSize;
    spot.status = PENDING;

    spot.next = NULL;
    spot.applicantHead = NULL;
    spot.assignedCount = 0;

    if (estimatedSize == SMALL)
    {
        spot.workerLimit = 2;
    }
    else if (estimatedSize == MEDIUM)
    {
        spot.workerLimit = 5;
    }
    else
    {
        spot.workerLimit = 10;
    }

    return spots.addSpot(spot);
}
