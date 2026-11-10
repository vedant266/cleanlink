#include "spot_list.h"

#include <iostream>
#include <cstdio>
#include <cstring>

using namespace std;

// True if a record read from the file has values the program can use.
// display() uses status and size as array indexes, so a damaged value
// would read past the end of those arrays.
static bool isValidRecord(const WasteSpot &spot)
{
    // Text fields must end inside their arrays
    if (memchr(spot.cityName, '\0', sizeof(spot.cityName)) == NULL ||
        memchr(spot.localityName, '\0', sizeof(spot.localityName)) == NULL ||
        memchr(spot.photoPath, '\0', sizeof(spot.photoPath)) == NULL ||
        memchr(spot.wasteType, '\0', sizeof(spot.wasteType)) == NULL)
        return false;

    return spot.spotID >= 1 &&
           spot.status >= PENDING && spot.status <= REJECTED &&
           spot.estimatedSize >= SMALL && spot.estimatedSize <= LARGE &&
           spot.workerLimit >= 0 &&
           spot.assignedCount >= 0 && spot.assignedCount <= MAX_ASSIGNED_WORKERS;
}

SpotList::SpotList()
{
    head = nullptr;
    tail = nullptr;
    nextSpotID = 1;
    canSave = true;
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
bool SpotList::saveToFile(const WasteSpot &spot)
{
    if (!canSave)
        return false;

    FILE *fp = fopen(SPOT_FILE, "ab");

    if (fp == NULL)
    {
        cout << "Error opening " << SPOT_FILE << endl;
        return false;
    }

    // Pointers are meaningless after the program exits, so store them as NULL
    WasteSpot record = spot;
    record.next = NULL;
    record.applicantHead = NULL;

    bool saved = fwrite(&record, sizeof(WasteSpot), 1, fp) == 1;

    // fclose writes out anything still buffered, so it can fail too
    if (fclose(fp) != 0)
        saved = false;

    return saved;
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
             << "Delete or rename it to start fresh.\n"
             << "Until then, new reports and changes will NOT be saved.\n";
        canSave = false;
        fclose(fp);
        return;
    }

    WasteSpot spot;
    int count = 0;
    int damaged = 0;

    while (fread(&spot, sizeof(WasteSpot), 1, fp) == 1)
    {
        if (!isValidRecord(spot))
        {
            damaged++;
            continue;
        }

        appendNode(spot);
        count++;
    }

    fclose(fp);

    cout << "Loaded " << count
         << " waste spot(s) from " << SPOT_FILE << endl;

    // Same rule as a wrong-sized file: don't write into a file we can't fully read
    if (damaged > 0)
    {
        cout << damaged << " damaged record(s) in " << SPOT_FILE << " were skipped.\n"
             << "Until the file is fixed, new reports and changes will NOT be saved.\n";
        canSave = false;
    }
}

int SpotList::addSpot(WasteSpot spot)
{
    spot.spotID = nextSpotID;

    // Save first, so the list never holds a spot that isn't in the file
    if (!saveToFile(spot))
        return 0;

    appendNode(spot);

    return spot.spotID;
}

// Records are a fixed size, so find the spot's record and write over it
bool SpotList::updateSpot(const WasteSpot &spot)
{
    if (!canSave)
        return false;

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

        if (fclose(fp) != 0)
            saved = false;

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

        if (current->assignedCount > 0)
        {
            cout << "Assigned Workers:";
            for (int i = 0; i < current->assignedCount; i++)
                cout << " " << current->assignedWorkers[i];
            cout << endl;
        }

        cout << "Photo: " << current->photoPath << endl;
        cout << "------------------\n";

        current = current->next;
    }
}