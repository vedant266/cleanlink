#include <iostream>
#include "locality_hash.h"
#include "waste_spot.h"

using namespace std;

LocalityHashTable::LocalityHashTable()
{
    nextLocalityID = 1;

    for (int i = 0; i < LOCALITY_SIZE; i++)
    {
        table[i].id = 0;
        table[i].name = "";

        table[i].minLat = 0;
        table[i].maxLat = 0;
        table[i].minLon = 0;
        table[i].maxLon = 0;

        table[i].spotCount = 0;
        table[i].spotHead = nullptr;
    }
}

LocalityHashTable::~LocalityHashTable()
{
    // Free only the list nodes. The spots themselves belong to SpotList.
    for (int i = 0; i < LOCALITY_SIZE; i++)
    {
        SpotNode* current = table[i].spotHead;

        while (current != nullptr)
        {
            SpotNode* temp = current;
            current = current->next;
            delete temp;
        }
    }
}

int LocalityHashTable::hashFunction(string name)
{
    int sum = 0;

    // unsigned char so names like "Haridwār" never give a negative sum
    for (int i = 0; i < name.length(); i++)
    {
        sum = sum + (unsigned char)name[i];
    }

    return sum % LOCALITY_SIZE;
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

        index = (index + 1) % LOCALITY_SIZE;

        if (index == start)
        {
            break;
        }
    }

    return -1;
}

Locality* LocalityHashTable::insertLocality(string name)
{
    int found = searchLocality(name);

    if (found != -1)
    {
        return &table[found];
    }

    int index = hashFunction(name);
    int start = index;

    while (table[index].id != 0)
    {
        index = (index + 1) % LOCALITY_SIZE;

        if (index == start)
        {
            cout << "Locality Hash Table is Full\n";
            return nullptr;
        }
    }

    table[index].id = nextLocalityID++;
    table[index].name = name;

    cout << "Locality added: " << name
         << " | ID: " << table[index].id << endl;

    return &table[index];
}

Locality* LocalityHashTable::getLocality(string name)
{
    int index = searchLocality(name);

    if (index == -1)
    {
        return nullptr;
    }

    return &table[index];
}

void LocalityHashTable::addSpot(Locality* locality, WasteSpot* spot)
{
    SpotNode* node = new SpotNode;
    node->spot = spot;
    node->next = nullptr;

    // Add at the end so spots stay in the order they were reported
    if (locality->spotHead == nullptr)
    {
        locality->spotHead = node;
    }
    else
    {
        SpotNode* last = locality->spotHead;

        while (last->next != nullptr)
        {
            last = last->next;
        }

        last->next = node;
    }

    // First spot sets the GPS box, later spots stretch it
    if (locality->spotCount == 0)
    {
        locality->minLat = locality->maxLat = spot->latitude;
        locality->minLon = locality->maxLon = spot->longitude;
    }
    else
    {
        if (spot->latitude < locality->minLat) locality->minLat = spot->latitude;
        if (spot->latitude > locality->maxLat) locality->maxLat = spot->latitude;
        if (spot->longitude < locality->minLon) locality->minLon = spot->longitude;
        if (spot->longitude > locality->maxLon) locality->maxLon = spot->longitude;
    }

    locality->spotCount++;
}

int LocalityHashTable::count()
{
    int total = 0;

    for (int i = 0; i < LOCALITY_SIZE; i++)
    {
        if (table[i].id != 0)
        {
            total++;
        }
    }

    return total;
}

void LocalityHashTable::display()
{
    for (int i = 0; i < LOCALITY_SIZE; i++)
    {
        if (table[i].id == 0)
        {
            continue;
        }

        cout << "    " << i << " : "
             << table[i].id << " - "
             << table[i].name
             << " | spots: " << table[i].spotCount;

        if (table[i].spotHead != nullptr)
        {
            cout << " (";

            for (SpotNode* n = table[i].spotHead; n != nullptr; n = n->next)
            {
                cout << "#" << n->spot->spotID;

                if (n->next != nullptr)
                {
                    cout << ", ";
                }
            }

            cout << ")";
        }

        cout << endl;
    }
}
