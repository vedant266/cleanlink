#ifndef SPOT_LIST_H
#define SPOT_LIST_H

#include "waste_spot.h"

// Path is relative to the repo root (run.command starts the app from there)
#define SPOT_FILE "backend/data/waste_spots.dat"

class SpotList
{
private:
    WasteSpot *head;
    WasteSpot *tail;
    int nextSpotID;

    void appendNode(const WasteSpot &spot);

    void saveToFile(const WasteSpot &spot);

public:
    SpotList();
    ~SpotList();

    void loadFromFile();

    int addSpot(WasteSpot spot);

    // Save a changed spot (e.g. new status) over its old record in the .dat file
    bool updateSpot(const WasteSpot &spot);

    // Walk the list for this ID. Returns nullptr if there is no such spot.
    WasteSpot *findSpot(int spotID);

    void display();

    WasteSpot *getHead() { return head; }

    WasteSpot *getTail() { return tail; }
};

#endif
