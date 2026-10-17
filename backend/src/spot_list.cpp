#include "spot_list.h"

#include <iostream>
#include <cstdio>

using namespace std;

SpotList::SpotList()
{
    head = nullptr;
    tail = nullptr;
    nextSpotID = 1;
}

SpotList::~SpotList()
{
    while (head != nullptr)
    {
        WasteSpot *temp = head;
        head = head->next;
        delete temp;
    }
}

// Copy the spot into a new node at the end of the linked list
void SpotList::appendNode(const WasteSpot &spot)
{
    WasteSpot *newSpot = new WasteSpot;

    *newSpot = spot;
    newSpot->next = nullptr;
    newSpot->applicantHead = nullptr;

    if (head == nullptr)
    {
        head = newSpot;
    }
    else
    {
        tail->next = newSpot;
    }

    tail = newSpot;

    if (spot.spotID >= nextSpotID)
    {
        nextSpotID = spot.spotID + 1;
    }
}

// Append one record to the .dat file
void SpotList::saveToFile(const WasteSpot &spot)
{
    FILE *fp = fopen(SPOT_FILE, "ab");

    if (fp == NULL)
    {
        cout << "Error opening " << SPOT_FILE << endl;
        return;
    }

    // Pointers are meaningless after the program exits, so store them as NULL
    WasteSpot record = spot;
    record.next = NULL;
    record.applicantHead = NULL;

    fwrite(&record, sizeof(WasteSpot), 1, fp);

    fclose(fp);
}

void SpotList::loadFromFile()
{
    FILE *fp = fopen(SPOT_FILE, "rb");

    // No file yet means no spots have been reported
    if (fp == NULL)
    {
        return;
    }

    WasteSpot spot;
    int count = 0;

    while (fread(&spot, sizeof(WasteSpot), 1, fp) == 1)
    {
        appendNode(spot);
        count++;
    }

    fclose(fp);

    cout << "Loaded " << count
         << " waste spot(s) from " << SPOT_FILE << endl;
}

int SpotList::addSpot(WasteSpot spot)
{
    spot.spotID = nextSpotID;

    appendNode(spot);
    saveToFile(spot);

    return spot.spotID;
}

void SpotList::display()
{
    const char *sizeNames[] = {"Small", "Medium", "Large"};
    const char *statusNames[] = {"Pending", "Verified", "Funded", "Assigned", "Completed"};

    cout << "\n===== WASTE SPOTS =====\n";

    if (head == nullptr)
    {
        cout << "No waste spots reported yet.\n";
        return;
    }

    WasteSpot *current = head;

    while (current != nullptr)
    {
        cout << "Spot ID: " << current->spotID << endl;
        cout << "Reporter ID: " << current->reporterID << endl;
        cout << "Location: " << current->latitude
             << ", " << current->longitude << endl;
        // %.6f keeps 6 decimals so the pin lands on the exact spot
        printf("Map: https://www.google.com/maps?q=%.6f,%.6f\n",
               current->latitude, current->longitude);
        cout << "City: " << current->cityName << endl;
        cout << "Locality: " << current->localityName << endl;
        cout << "Waste Type: " << current->wasteType << endl;
        cout << "Size: " << sizeNames[current->estimatedSize] << endl;
        cout << "Workers Needed: " << current->workerLimit << endl;
        cout << "Status: " << statusNames[current->status] << endl;
        cout << "Photo: " << current->photoPath << endl;
        cout << "------------------\n";

        current = current->next;
    }
}
