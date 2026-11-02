#include "verification.h"
#include "report_spot.h"

#include <iostream>
#include <cstdio>
#include <cstring>

using namespace std;

bool Verification::hasRealCity(const WasteSpot *spot)
{
    return spot->cityName[0] != '\0' && strcmp(spot->cityName, "Unknown") != 0;
}

VerifyResult Verification::canVerify(SpotList &spots, int spotID, const Moderator &moderator)
{
    WasteSpot *spot = spots.findSpot(spotID);

    if (spot == nullptr)
        return VERIFY_NO_SPOT;

    if (spot->status != PENDING)
        return VERIFY_NOT_PENDING;

    // Nobody checks their own report
    if (spot->reporterID == moderator.getID())
        return VERIFY_OWN_REPORT;

    return VERIFY_OK;
}

VerifyResult Verification::setLocation(SpotList &spots, CityHashTable &cities, int spotID,
                                       const Moderator &moderator,
                                       string cityName, string localityName)
{
    VerifyResult check = canVerify(spots, spotID, moderator);

    if (check != VERIFY_OK)
        return check;

    WasteSpot *spot = spots.findSpot(spotID);

    // A spot with a real city is already in that locality's list. Changing its
    // names here would leave it in the old list too.
    if (hasRealCity(spot))
        return VERIFY_CITY_KNOWN;

    if (cityName.empty() || localityName.empty() || cityName == "Unknown")
        return VERIFY_BAD_NAME;

    strncpy(spot->cityName, cityName.c_str(), sizeof(spot->cityName) - 1);
    strncpy(spot->localityName, localityName.c_str(), sizeof(spot->localityName) - 1);

    if (!spots.updateSpot(*spot))
        return VERIFY_SAVE_FAILED;

    // It was skipped when loaded because its city was Unknown; file it now
    cities.addSpot(spot);

    return VERIFY_OK;
}

VerifyResult Verification::approveSpot(SpotList &spots, int spotID,
                                       const Moderator &moderator, WasteSize actualSize)
{
    VerifyResult check = canVerify(spots, spotID, moderator);

    if (check != VERIFY_OK)
        return check;

    if (actualSize < SMALL || actualSize > LARGE)
        return VERIFY_BAD_SIZE;

    WasteSpot *spot = spots.findSpot(spotID);

    // Donors find spots through City -> Locality, so it needs a real city first
    if (!hasRealCity(spot))
        return VERIFY_NEED_CITY;

    // Actual size -> workers needed -> cleanup cost
    spot->estimatedSize = actualSize;
    spot->workerLimit = workersForSize(actualSize);
    spot->cleanupCost = spot->workerLimit * WAGE_PER_WORKER;

    spot->moderatorID = moderator.getID();
    spot->status = VERIFIED;

    if (!spots.updateSpot(*spot))
        return VERIFY_SAVE_FAILED;

    return VERIFY_OK;
}

VerifyResult Verification::rejectSpot(SpotList &spots, int spotID, const Moderator &moderator)
{
    VerifyResult check = canVerify(spots, spotID, moderator);

    if (check != VERIFY_OK)
        return check;

    WasteSpot *spot = spots.findSpot(spotID);

    spot->moderatorID = moderator.getID();
    spot->status = REJECTED;

    if (!spots.updateSpot(*spot))
        return VERIFY_SAVE_FAILED;

    return VERIFY_OK;
}

int Verification::showPendingSpots(SpotList &spots)
{
    const char *sizeNames[] = {"Small", "Medium", "Large"};
    int shown = 0;

    cout << "\nReports waiting for a moderator:\n";

    for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
    {
        if (s->status != PENDING)
            continue;

        cout << "Spot #" << s->spotID
             << " | " << s->cityName << ", " << s->localityName
             << " | " << s->wasteType
             << " | " << sizeNames[s->estimatedSize] << endl;

        shown++;
    }

    if (shown == 0)
        cout << "None right now.\n";

    return shown;
}

void Verification::showReport(const WasteSpot *spot)
{
    const char *sizeNames[] = {"Small", "Medium", "Large"};

    cout << "\n--- Report #" << spot->spotID << " ---\n";
    cout << "Reporter ID: " << spot->reporterID << endl;
    cout << "Photo: " << spot->photoPath << endl;
    printf("Map: https://www.google.com/maps?q=%.6f,%.6f\n",
           spot->latitude, spot->longitude);
    cout << "City: " << spot->cityName << endl;
    cout << "Locality: " << spot->localityName << endl;
    cout << "Waste Type: " << spot->wasteType << endl;
    cout << "Reported Size: " << sizeNames[spot->estimatedSize] << endl;
}

void Verification::rejectFromMenu(SpotList &spots, int spotID, const Moderator &moderator)
{
    VerifyResult result = rejectSpot(spots, spotID, moderator);

    if (result == VERIFY_OK)
        cout << "Spot #" << spotID << " rejected.\n";
    else
        cout << resultMessage(result) << endl;
}

void Verification::verifySpotMenu(SpotList &spots, CityHashTable &cities)
{
    int moderatorID;
    int spotID;
    int sizeChoice;
    char answer;

    cout << "\n===== VERIFY SPOT =====\n";

    cout << "Enter Moderator ID: ";
    cin >> moderatorID;

    if (showPendingSpots(spots) == 0)
        return;

    cout << "\nEnter Spot ID: ";
    cin >> spotID;

    Moderator moderator(moderatorID);

    VerifyResult check = canVerify(spots, spotID, moderator);

    if (check != VERIFY_OK)
    {
        cout << resultMessage(check) << endl;
        return;
    }

    WasteSpot *spot = spots.findSpot(spotID);
    showReport(spot);

    // 1. Verify location
    cout << "\nDoes the map link match the photo? (y/n): ";
    cin >> answer;

    if (answer != 'y' && answer != 'Y')
    {
        rejectFromMenu(spots, spotID, moderator);
        return;
    }

    if (!hasRealCity(spot))
    {
        string cityName;
        string localityName;

        cout << "No city was found for this location. Read it from the map link.\n";

        // getline, because names like "Rajpur Road" have spaces
        cout << "Enter City: ";
        getline(cin >> ws, cityName);

        cout << "Enter Locality: ";
        getline(cin >> ws, localityName);

        VerifyResult result = setLocation(spots, cities, spotID, moderator,
                                          cityName, localityName);

        if (result != VERIFY_OK)
        {
            cout << resultMessage(result) << endl;
            return;
        }
    }

    // 2. Verify waste
    cout << "Is there real waste in the photo? (y/n): ";
    cin >> answer;

    if (answer != 'y' && answer != 'Y')
    {
        rejectFromMenu(spots, spotID, moderator);
        return;
    }

    // 3. Verify actual size
    cout << "\nActual Size:\n";
    cout << "1. Small\n";
    cout << "2. Medium\n";
    cout << "3. Large\n";
    cout << "Enter choice: ";
    cin >> sizeChoice;

    if (sizeChoice < 1 || sizeChoice > 3)
    {
        cout << "Invalid size choice.\n";
        return;
    }

    // 1 -> SMALL, 2 -> MEDIUM, 3 -> LARGE
    WasteSize actualSize = (WasteSize)(sizeChoice - 1);

    // 4. Update workerLimit, estimate cleanupCost, status = Verified
    VerifyResult result = approveSpot(spots, spotID, moderator, actualSize);

    if (result != VERIFY_OK)
    {
        cout << resultMessage(result) << endl;
        return;
    }

    cout << "\nSpot #" << spotID << " verified.\n";
    cout << "Workers Needed: " << spot->workerLimit << endl;
    cout << "Cleanup Cost: Rs " << spot->cleanupCost << endl;
    cout << "It is now open for donations.\n";
}

const char *Verification::resultMessage(VerifyResult result)
{
    switch (result)
    {
        case VERIFY_OK:          return "Saved.";
        case VERIFY_NO_SPOT:     return "No spot with that ID.";
        case VERIFY_NOT_PENDING: return "This spot has already been checked.";
        case VERIFY_OWN_REPORT:  return "You cannot verify your own report.";
        case VERIFY_NEED_CITY:   return "Enter the real city and locality before verifying.";
        case VERIFY_CITY_KNOWN:  return "This spot already has a city and cannot be moved.";
        case VERIFY_BAD_NAME:    return "City and locality cannot be empty or \"Unknown\".";
        case VERIFY_BAD_SIZE:    return "Size must be Small, Medium or Large.";
        case VERIFY_SAVE_FAILED: return "Could not save the change. Try again.";
    }

    return "Unknown result.";
}
