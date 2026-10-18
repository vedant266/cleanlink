#include <iostream>
#include <cstring>
#include "city_hash.h"
#include "waste_spot.h"

using namespace std;

CityHashTable::CityHashTable()
{
    nextCityID = 1;

    for(int i = 0; i < CITY_SIZE; i++)
    {
        table[i].id = 0;
        table[i].name = "";
    }
}

int CityHashTable::hashFunction(string name)
{
    int sum = 0;

    // unsigned char so names like "Haridwār" never give a negative sum
    for(int i = 0; i < name.length(); i++)
        sum += (unsigned char)name[i];

    return sum % CITY_SIZE;
}

City* CityHashTable::insertCity(string name)
{
    // Check if city already exists
    int found = searchCity(name);

    if(found != -1)
        return &table[found];

    int index = hashFunction(name);
    int startIndex = index;

    while(table[index].id != 0)
    {
        index = (index + 1) % CITY_SIZE;

        // Every slot is taken
        if(index == startIndex)
        {
            cout << "City Hash Table is Full\n";
            return nullptr;
        }
    }

    table[index].id = nextCityID;
    table[index].name = name;

    cout << "City added: "
         << name
         << " | ID: "
         << nextCityID
         << endl;

    nextCityID++;

    return &table[index];
}

int CityHashTable::searchCity(string name)
{
    int index = hashFunction(name);
    int startIndex = index;

    while(table[index].id != 0)
    {
        if(table[index].name == name)
            return index;

        index = (index + 1) % CITY_SIZE;

        // Prevent infinite loop if table is full
        if(index == startIndex)
            break;
    }

    return -1;
}

City* CityHashTable::getCity(string name)
{
    int index = searchCity(name);

    if(index != -1)
        return &table[index];

    return nullptr;
}

int CityHashTable::getCityID(string name)
{
    int index = searchCity(name);

    if(index != -1)
        return table[index].id;

    return -1;
}

Locality* CityHashTable::addLocality(string cityName, string localityName)
{
    City* city = getCity(cityName);

    if(city == nullptr)
        city = insertCity(cityName);

    if(city == nullptr)
        return nullptr;

    return city->localities.insertLocality(localityName);
}

void CityHashTable::addSpot(WasteSpot* spot)
{
    if(spot->cityName[0] == '\0' || strcmp(spot->cityName, "Unknown") == 0)
        return;

    // Old spots may have no locality saved, so use the city name instead
    string localityName = spot->localityName;

    if(localityName.empty())
        localityName = spot->cityName;

    Locality* locality = addLocality(spot->cityName, localityName);

    if(locality == nullptr)
        return; // a table is full

    getCity(spot->cityName)->localities.addSpot(locality, spot);
}

void CityHashTable::display()
{
    cout << "\nCITY HASH TABLE\n";

    for(int i = 0; i < CITY_SIZE; i++)
    {
        if(table[i].id != 0)
        {
            cout << i << " : "
                 << table[i].id
                 << " - "
                 << table[i].name
                 << " | localities: "
                 << table[i].localities.count()
                 << endl;

            table[i].localities.display();
        }
    }
}
