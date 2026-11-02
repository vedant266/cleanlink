#ifndef REPORT_SPOT_H
#define REPORT_SPOT_H

#include "waste_spot.h"
#include "city_hash.h"
#include "spot_list.h"

// Workers a spot of this size needs. Used when reporting and when a moderator corrects the size.
int workersForSize(WasteSize size);

class ReportSpot
{
private:
    CityHashTable cityTable;

public:
    // File every spot already saved in the .dat file under its city and locality
    void loadTables(SpotList &spots);

    void displayTables();

    // Funding walks City -> Locality -> spot through this table
    CityHashTable &getCityTable() { return cityTable; }

    void reportSpot(SpotList &spots);

    int saveReport(SpotList &spots, int reporterID,
                   double latitude, double longitude,
                   const char *photoPath, const char *wasteType,
                   WasteSize estimatedSize);
};

#endif