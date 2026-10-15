#include <iostream>
#include "locality_hash.h"

using namespace std;

LocalityHashTable::LocalityHashTable()
{
    nextLocalityID = 1;

    for (int i = 0; i < SIZE; i++)
    {
        table[i].id = 0;
        table[i].name = "";

        table[i].minLat = 0;
        table[i].maxLat = 0;
        table[i].minLon = 0;
        table[i].maxLon = 0;

        table[i].head = nullptr;
    }
}

int LocalityHashTable::hashFunction(string name)
{
    int sum = 0;

    for (int i = 0; i < name.length(); i++)
    {
        sum = sum + name[i];
    }

    return sum % SIZE;
}

int LocalityHashTable::searchLocality(string name)
{
    int index = hashFunction(name);
    int start = index;

    while (table[index].id != 0)
    {
        if (table[index].name == name)
        {
            return index;
        }

        index = (index + 1) % SIZE;

        if (index == start)
        {
            break;
        }
    }

    return -1;
}

void LocalityHashTable::insertLocality(
    string name,
    double minLat,
    double maxLat,
    double minLon,
    double maxLon,
    WasteSpot* head)
{
    if (searchLocality(name) != -1)
    {
        cout << "Locality already exists\n";
        return;
    }

    int index = hashFunction(name);
    int start = index;

    while (table[index].id != 0)
    {
        index = (index + 1) % SIZE;

        if (index == start)
        {
            cout << "Locality Hash Table is Full\n";
            return;
        }
    }

    table[index].id = nextLocalityID++;
    table[index].name = name;

    table[index].minLat = minLat;
    table[index].maxLat = maxLat;
    table[index].minLon = minLon;
    table[index].maxLon = maxLon;

    table[index].head = head;

    cout << "Locality inserted successfully\n";
}