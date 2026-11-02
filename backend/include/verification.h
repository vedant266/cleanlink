#ifndef VERIFICATION_H
#define VERIFICATION_H

#include <string>
#include "city_hash.h"
#include "spot_list.h"
#include "user.h"

using namespace std;

// What happened when a moderator tried to check a report
typedef enum {
    VERIFY_OK,
    VERIFY_NO_SPOT,
    VERIFY_NOT_PENDING,
    VERIFY_OWN_REPORT,
    VERIFY_NEED_CITY,
    VERIFY_CITY_KNOWN,
    VERIFY_BAD_NAME,
    VERIFY_BAD_SIZE,
    VERIFY_SAVE_FAILED
} VerifyResult;

class Verification
{
private:
    // True if reverse geocoding found a real city for this spot
    static bool hasRealCity(const WasteSpot *spot);

    // Moderator opens the report: photo, map link, place, waste and size
    void showReport(const WasteSpot *spot);

    int showPendingSpots(SpotList &spots);

    void rejectFromMenu(SpotList &spots, int spotID, const Moderator &moderator);

public:
    // Checks every action shares: the spot exists, is still pending,
    // and was not reported by this moderator
    VerifyResult canVerify(SpotList &spots, int spotID, const Moderator &moderator);

    // Only for spots whose city came back "Unknown": the moderator reads the
    // map link and types the real city and locality
    VerifyResult setLocation(SpotList &spots, CityHashTable &cities, int spotID,
                             const Moderator &moderator,
                             string cityName, string localityName);

    // Location and waste are real: save actual size, workers, cost and Verified
    VerifyResult approveSpot(SpotList &spots, int spotID,
                             const Moderator &moderator, WasteSize actualSize);

    // Wrong location or no real waste
    VerifyResult rejectSpot(SpotList &spots, int spotID, const Moderator &moderator);

    // Terminal: the full moderator flow
    void verifySpotMenu(SpotList &spots, CityHashTable &cities);

    static const char *resultMessage(VerifyResult result);
};

#endif
