#ifndef FUNDING_H
#define FUNDING_H

#include <string>
#include "city_hash.h"
#include "spot_list.h"
#include "donation.h"
#include "user.h"

using namespace std;

// What happened when someone tried to fund a spot
typedef enum {
    FUND_OK,
    FUND_NO_CITY,
    FUND_NO_LOCALITY,
    FUND_NO_SPOT,
    FUND_NOT_VERIFIED,
    FUND_CLOSED,
    FUND_BAD_AMOUNT,
    FUND_TOO_MUCH,
    FUND_SAVE_FAILED
} FundResult;

class Funding
{
private:
    DonationLog donations;

    // City -> Locality -> that locality's spot list. Sets spot when found.
    FundResult findSpot(CityHashTable &cities, string cityName,
                        string localityName, int spotID, WasteSpot *&spot);

    // Print every spot that can still take money
    int showOpenSpots(SpotList &spots);

public:
    void loadFromFile();

    // Shared by the terminal menu and the web server
    FundResult fundSpot(CityHashTable &cities, SpotList &spots,
                        string cityName, string localityName,
                        int spotID, const Donor &donor, int amount);

    // Terminal: ask for donor, spot ID and amount
    void fundSpotMenu(CityHashTable &cities, SpotList &spots);

    // Every spot that is verified (still raising money) or already funded
    void displayCampaigns(SpotList &spots);

    void displayDonations();

    static const char *resultMessage(FundResult result);
};

#endif
