#include "funding.h"

#include <iostream>

using namespace std;

void Funding::loadFromFile()
{
    donations.loadFromFile();
}

FundResult Funding::findSpot(CityHashTable &cities, string cityName,
                             string localityName, int spotID, WasteSpot *&spot)
{
    City *city = cities.getCity(cityName);

    if (city == nullptr)
        return FUND_NO_CITY;

    Locality *locality = city->localities.getLocality(localityName);

    if (locality == nullptr)
        return FUND_NO_LOCALITY;

    // Walk this locality's own spot list
    for (SpotNode *node = locality->spotHead; node != nullptr; node = node->next)
    {
        if (node->spot->spotID == spotID)
        {
            spot = node->spot;
            return FUND_OK;
        }
    }

    return FUND_NO_SPOT;
}

FundResult Funding::fundSpot(CityHashTable &cities, SpotList &spots,
                             string cityName, string localityName,
                             int spotID, const Donor &donor, int amount)
{
    // Find City -> Locality -> WasteSpot
    WasteSpot *spot = nullptr;
    FundResult found = findSpot(cities, cityName, localityName, spotID, spot);

    if (found != FUND_OK)
        return found;

    // Only a verified spot that still needs money can be funded
    if (spot->status == PENDING)
        return FUND_NOT_VERIFIED;

    if (spot->status != VERIFIED)
        return FUND_CLOSED;

    if (amount <= 0)
        return FUND_BAD_AMOUNT;

    // Never take more than the cleanup still needs
    if (amount > spot->cleanupCost - spot->fundedAmount)
        return FUND_TOO_MUCH;

    // Save the donation record first, so every rupee in fundedAmount has a record
    if (!donations.addDonation(spotID, donor.getID(), amount))
        return FUND_SAVE_FAILED;

    spot->fundedAmount += amount;

    if (spot->fundedAmount >= spot->cleanupCost)
        spot->status = FUNDED;

    // The spot is the same node SpotList holds, so this saves the change to disk
    if (!spots.updateSpot(*spot))
        return FUND_SAVE_FAILED;

    return FUND_OK;
}

int Funding::showOpenSpots(SpotList &spots)
{
    int shown = 0;

    cout << "\nSpots that need funding:\n";

    for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
    {
        if (s->status != VERIFIED)
            continue;

        cout << "Spot #" << s->spotID
             << " | " << s->cityName << ", " << s->localityName
             << " | " << s->wasteType
             << " | Rs " << s->fundedAmount << " / Rs " << s->cleanupCost
             << " (needs Rs " << s->cleanupCost - s->fundedAmount << ")\n";

        shown++;
    }

    if (shown == 0)
        cout << "None right now.\n";

    return shown;
}

void Funding::fundSpotMenu(CityHashTable &cities, SpotList &spots)
{
    int donorID;
    int spotID;
    int amount;

    cout << "\n===== FUND A SPOT =====\n";

    if (showOpenSpots(spots) == 0)
        return;

    cout << "\nEnter Donor ID: ";
    cin >> donorID;

    cout << "Enter Spot ID: ";
    cin >> spotID;

    WasteSpot *spot = spots.findSpot(spotID);

    if (spot == nullptr)
    {
        cout << resultMessage(FUND_NO_SPOT) << endl;
        return;
    }

    cout << "Enter Amount (Rs): ";
    cin >> amount;

    // fundSpot still goes City -> Locality -> spot, using the spot's own names.
    // Old spots may have no locality saved; the city table files those under the city name.
    string localityName = spot->localityName;

    if (localityName.empty())
        localityName = spot->cityName;

    Donor donor(donorID);

    FundResult result = fundSpot(cities, spots, spot->cityName, localityName,
                                 spotID, donor, amount);

    cout << resultMessage(result) << endl;

    if (result != FUND_OK)
        return;

    cout << "Spot #" << spotID << " funded: Rs " << spot->fundedAmount
         << " / Rs " << spot->cleanupCost << endl;

    if (spot->status == FUNDED)
        cout << "Spot #" << spotID << " is now fully funded!\n";
}

void Funding::displayCampaigns(SpotList &spots)
{
    cout << "\n===== CAMPAIGNS =====\n";

    int shown = 0;

    for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
    {
        if (s->status != VERIFIED && s->status != FUNDED)
            continue;

        cout << "Spot #" << s->spotID
             << " | " << s->cityName << ", " << s->localityName
             << " | Rs " << s->fundedAmount << " / Rs " << s->cleanupCost
             << " | " << (s->status == FUNDED ? "Funded" : "Raising") << endl;

        shown++;
    }

    if (shown == 0)
        cout << "No verified spots yet.\n";
}

void Funding::displayDonations()
{
    donations.display();
}

const char *Funding::resultMessage(FundResult result)
{
    switch (result)
    {
        case FUND_OK:           return "Thank you! Donation saved.";
        case FUND_NO_CITY:      return "City not found.";
        case FUND_NO_LOCALITY:  return "Locality not found in that city.";
        case FUND_NO_SPOT:      return "No spot with that ID.";
        case FUND_NOT_VERIFIED: return "This spot has not been verified yet.";
        case FUND_CLOSED:       return "This spot is not taking donations (funded, in progress or rejected).";
        case FUND_BAD_AMOUNT:   return "Amount must be more than 0.";
        case FUND_TOO_MUCH:     return "That is more than the spot still needs.";
        case FUND_SAVE_FAILED:  return "Could not save the donation. Try again.";
    }

    return "Unknown result.";
}
