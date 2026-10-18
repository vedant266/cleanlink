#ifndef LOCALITY_HASH_H
#define LOCALITY_HASH_H

#include <iostream>
#include <string>
using namespace std;

const int LOCALITY_SIZE = 10;

struct WasteSpot;

// One node in a locality's own spot list. It points to the real WasteSpot,
// which stays in SpotList (a spot's own "next" is already used by SpotList).
struct SpotNode
{
    WasteSpot* spot;
    SpotNode* next;
};

struct Locality
{
    int id;
    string name;

    double minLat;
    double maxLat;
    double minLon;
    double maxLon;

    int spotCount;
    SpotNode* spotHead;
};

class LocalityHashTable
{
private:
    Locality table[LOCALITY_SIZE];
    int nextLocalityID;

    int hashFunction(string name);

public:
    LocalityHashTable();
    ~LocalityHashTable();

    // Each table owns its spot nodes, so it must not be copied
    LocalityHashTable(const LocalityHashTable&) = delete;
    LocalityHashTable& operator=(const LocalityHashTable&) = delete;

    // Index of the locality in the table, or -1
    int searchLocality(string name);

    // Adds the locality if it is new. Returns nullptr if the table is full.
    Locality* insertLocality(string name);

    Locality* getLocality(string name);

    // Add a spot to the end of this locality's spot list
    void addSpot(Locality* locality, WasteSpot* spot);

    int count();

    void display();
};

#endif
