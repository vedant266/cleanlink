#ifndef LOCALITY_HASH_H
#define LOCALITY_HASH_H

#include <iostream>
#include <string>
using namespace std;

#define SIZE 10

struct WasteSpot;

struct Locality
{
    int id;
    string name;

    double minLat;
    double maxLat;
    double minLon;
    double maxLon;

    WasteSpot* head;
};

class LocalityHashTable
{
private:
    Locality table[SIZE];
    int nextLocalityID;

    int hashFunction(string name);

public:
    LocalityHashTable();

    int searchLocality(string name);

    void insertLocality(string name,
                        double minLat,
                        double maxLat,
                        double minLon,
                        double maxLon,
                        WasteSpot* head = nullptr);
};

#endif