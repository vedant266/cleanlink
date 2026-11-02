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

        // Free this spot's applicant list before freeing the spot
        Applicant *applicant = temp->applicantHead;
        while (applicant != nullptr)
        {
            Applicant *nextApplicant = applicant->getNext();
            delete applicant;
            applicant = nextApplicant;
        }

        delete temp;
    }

    tail = nullptr;
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

    // Every record is the same size. If the file doesn't divide evenly, it was
    // saved before WasteSpot changed, and reading it would give garbage.
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);

    if (fileSize % sizeof(WasteSpot) != 0)
    {
        cout << SPOT_FILE << " was saved by an older version of CleanLink.\n"
             << "Delete or rename it to start fresh.\n";
        fclose(fp);
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

// Records are a fixed size, so find the spot's record and write over it
bool SpotList::updateSpot(const WasteSpot &spot)
{
    FILE *fp = fopen(SPOT_FILE, "r+b");

    if (fp == NULL)
    {
        cout << "Error opening " << SPOT_FILE << endl;
        return false;
    }

    WasteSpot record;

    while (fread(&record, sizeof(WasteSpot), 1, fp) == 1)
    {
        if (record.spotID != spot.spotID)
            continue;

        // Step back to the start of this record and overwrite it
        fseek(fp, -(long)sizeof(WasteSpot), SEEK_CUR);

        record = spot;
        record.next = NULL;
        record.applicantHead = NULL;

        bool saved = fwrite(&record, sizeof(WasteSpot), 1, fp) == 1;

        fclose(fp);
        return saved;
    }

    fclose(fp);
    return false;
}

WasteSpot *SpotList::findSpot(int spotID)
{
    for (WasteSpot *current = head; current != nullptr; current = current->next)
    {
        if (current->spotID == spotID)
            return current;
    }

    return nullptr;
}

void SpotList::display()
{
    const char *sizeNames[] = {"Small", "Medium", "Large"};
    const char *statusNames[] = {"Pending", "Verified", "Funded", "Assigned", "Completed", "Rejected"};

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

        if (current->moderatorID != 0)
            cout << "Checked by Moderator: " << current->moderatorID << endl;

        cout << "Funded: Rs " << current->fundedAmount
             << " / Rs " << current->cleanupCost << endl;
        cout << "Photo: " << current->photoPath << endl;
        cout << "------------------\n";

        current = current->next;
    }
}