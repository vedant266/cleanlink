#ifndef CITY_HASH_H
#define CITY_HASH_H

#include <string>
#include "locality_hash.h"
using namespace std;

const int CITY_SIZE = 10;

struct City
{
    int id;
    string name;

    // Every city keeps its own table of localities
    LocalityHashTable localities;
};

class CityHashTable
{
private:
    City table[CITY_SIZE];
    int nextCityID;

public:
    CityHashTable();

    int hashFunction(string name);

    // Adds the city if it is new. Returns nullptr if the table is full.
    City* insertCity(string name);

    int searchCity(string name);

    City* getCity(string name);

    int getCityID(string name);

    // Adds the locality into that city's own locality table.
    // The city is added first if it is new. Returns nullptr if a table is full.
    Locality* addLocality(string cityName, string localityName);

    // Put a spot into its city -> locality -> that locality's spot list.
    // Spots with no real city ("Unknown") are skipped.
    void addSpot(WasteSpot* spot);

    void display();
};

#endif
