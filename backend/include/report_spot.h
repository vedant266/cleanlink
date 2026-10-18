#ifndef REPORT_SPOT_H
#define REPORT_SPOT_H

#include "waste_spot.h"
#include "city_hash.h"
#include "spot_list.h"

class ReportSpot
{
private:
    CityHashTable cityTable;

public:
    // File every spot already saved in the .dat file under its city and locality
    void loadTables(SpotList &spots);

    void displayTables();

    void reportSpot(SpotList &spots);

    int saveReport(SpotList &spots, int reporterID,
                   double latitude, double longitude,
                   const char *photoPath, const char *wasteType,
                   WasteSize estimatedSize);
};

#endif