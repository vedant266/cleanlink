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

    // False when the .dat file could not be loaded. Writing to it then would
    // mix new records with ones we can't read, and spot IDs would repeat.
    bool canSave;

    void appendNode(const WasteSpot &spot);

    // Append one record. Returns false if it was not fully written.
    bool saveToFile(const WasteSpot &spot);

public:
    SpotList();
    ~SpotList();

    void loadFromFile();

    // Returns the new spot's ID, or 0 if it could not be saved
    // (the spot is then not added to the list either)
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
