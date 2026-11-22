
#ifndef CITY_HASH_H
#define CITY_HASH_H

#include <string>
#include "locality_hash.h"
#include "chained_hash.h"

using namespace std;

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
    // Separate Chaining instead of the old array
    ChainedHashTable<City*> table;

    int nextCityID;

public:
    CityHashTable();  // Constructor

    ~CityHashTable(); // Destructor

    int hashFunction(string name);

    // Adds the city if it is new
    City* insertCity(string name);

    int searchCity(string name);

    City* getCity(string name);

    int getCityID(string name);

    // Adds locality inside the respective city
    Locality* addLocality(string cityName, string localityName);

    // Adds waste spot inside its city and locality
    void addSpot(WasteSpot* spot);

    void display();
};

#endif