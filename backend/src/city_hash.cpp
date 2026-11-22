#include <iostream>
#include <cstring>
#include "city_hash.h"
#include "waste_spot.h"
#include "chained_hash.h"


using namespace std;

// Constructor
CityHashTable::CityHashTable()
{
    nextCityID = 1;
}

// Destructor
CityHashTable::~CityHashTable()
{
    // Delete all dynamically created City objects
    table.forEach([](City* city)
    {
        delete city;
    });
}

// DJB2 hash function
int CityHashTable::hashFunction(string name)
{
    unsigned long hash = 5381;

    for(unsigned char c : name)
        hash = hash * 33 + c;

    return hash % 101;
}

// Insert a new city
City* CityHashTable::insertCity(string name)
{
    // Check whether the city already exists
    City* existingCity = getCity(name);

    if(existingCity != nullptr)
        return existingCity;

    // Create a new city dynamically
    City* city = new City;

    city->id = nextCityID;
    city->name = name;

    // Insert city into the chained hash table
    if(!table.insert(name, city))
    {
        delete city;
        return nullptr;
    }

    cout << "City added: "
         << name
         << " | ID: "
         << nextCityID
         << endl;

    nextCityID++;

    return city;
}

// Get city object
City* CityHashTable::getCity(string name)
{
    City** found = table.find(name);

    if(found != nullptr)
        return *found;

    return nullptr;
}

// Get city ID
int CityHashTable::getCityID(string name)
{
    City* city = getCity(name);

    if(city != nullptr)
        return city->id;

    return -1;
}

// Add locality inside a city
Locality* CityHashTable::addLocality(string cityName, string localityName)
{
    City* city = getCity(cityName);

    if(city == nullptr)
        city = insertCity(cityName);

    if(city == nullptr)
        return nullptr;

    return city->localities.insertLocality(localityName);
}

// Add waste spot
void CityHashTable::addSpot(WasteSpot* spot)
{
    if(spot == nullptr)
        return;

    if(spot->cityName[0] == '\0' ||
       strcmp(spot->cityName, "Unknown") == 0)
        return;

    // Use city name if locality name is missing
    string localityName = spot->localityName;

    if(localityName.empty())
        localityName = spot->cityName;

    Locality* locality = addLocality(spot->cityName, localityName);

    if(locality == nullptr)
        return;

    City* city = getCity(spot->cityName);

    if(city != nullptr)
        city->localities.addSpot(locality, spot);
}

// Display all cities
void CityHashTable::display()
{
    cout << "\nCITY HASH TABLE\n";

    table.forEach([](City* city)
    {
        cout << "ID: "
             << city->id
             << " - "
             << city->name
             << " | localities: "
             << city->localities.count()
             << endl;

        city->localities.display();
    });
}
