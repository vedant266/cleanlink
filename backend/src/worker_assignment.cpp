#include "worker_assignment.h"
#include "haversine.h"

#include <iostream>
#include <cstdio>
#include <ctime>
#include <limits>

using namespace std;

WorkerAssignment::WorkerAssignment()
{
    canSave = true;
}

// Read one whole number from the terminal. On bad input (letters), clear it
// so the next prompt works, and return false.
static bool readInt(int &value)
{
    if (cin >> value)
        return true;

    if (!cin.eof())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    cout << "Please enter a number.\n";
    return false;
}

static bool readDouble(double &value)
{
    if (cin >> value)
        return true;

    if (!cin.eof())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    cout << "Please enter a number.\n";
    return false;
}

static void printTime(long long when)
{
    char text[20];
    time_t t = (time_t)when;
    strftime(text, sizeof(text), "%d-%m-%Y %H:%M", localtime(&t));
    cout << text;
}

int WorkerAssignment::countApplicants(const WasteSpot *spot)
{
    int count = 0;

    for (Applicant *a = spot->applicantHead; a != nullptr; a = a->getNext())
        count++;

    return count;
}

void WorkerAssignment::loadFromFile(SpotList &spots)
{
    FILE *fp = fopen(APPLICANT_FILE, "rb");

    // No file yet means nobody has applied
    if (fp == NULL)
        return;

    // Same check as the spot file: records are a fixed size
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);

    if (fileSize % sizeof(ApplicantRecord) != 0)
    {
        cout << APPLICANT_FILE << " was saved by an older version of CleanLink.\n"
             << "Delete or rename it to start fresh.\n"
             << "Until then, new applications will NOT be saved.\n";
        canSave = false;
        fclose(fp);
        return;
    }

    ApplicantRecord record;
    int count = 0;
    int skipped = 0;

    while (fread(&record, sizeof(ApplicantRecord), 1, fp) == 1)
    {
        WasteSpot *spot = spots.findSpot(record.spotID);

        // The spot is gone (e.g. the spot file was replaced)
        if (spot == nullptr)
        {
            skipped++;
            continue;
        }

        double distance = haversineDistance(spot->latitude, spot->longitude,
                                            record.latitude, record.longitude);

        Applicant *applicant = new Applicant(record.workerID,
                                             record.latitude, record.longitude,
                                             record.rating, distance,
                                             record.applicationTime);

        // Insert at the front of the spot's linked list
        applicant->setNext(spot->applicantHead);
        spot->applicantHead = applicant;

        count++;
    }

    fclose(fp);

    cout << "Loaded " << count << " application(s) from " << APPLICANT_FILE << endl;

    if (skipped > 0)
        cout << skipped << " application(s) were for spots that no longer exist.\n";
}

bool WorkerAssignment::saveApplicant(const ApplicantRecord &record)
{
    if (!canSave)
        return false;

    FILE *fp = fopen(APPLICANT_FILE, "ab");

    if (fp == NULL)
    {
        cout << "Error opening " << APPLICANT_FILE << endl;
        return false;
    }

    bool saved = fwrite(&record, sizeof(ApplicantRecord), 1, fp) == 1;

    if (fclose(fp) != 0)
        saved = false;

    return saved;
}

WasteSpot *WorkerAssignment::findSpot(CityHashTable &cities, string cityName,
                                      string localityName, int spotID)
{
    City *city = cities.getCity(cityName);

    if (city == nullptr)
        return nullptr;

    Locality *locality = city->localities.getLocality(localityName);

    if (locality == nullptr)
        return nullptr;

    // Walk this locality's own spot list
    for (SpotNode *node = locality->spotHead; node != nullptr; node = node->next)
    {
        if (node->spot->spotID == spotID)
            return node->spot;
    }

    return nullptr;
}

WorkResult WorkerAssignment::applyForSpot(CityHashTable &cities,
                                          string cityName, string localityName,
                                          int spotID, const Worker &worker)
{
    // 0 means "nobody", so real IDs start at 1
    if (worker.getID() < 1)
        return WORK_BAD_WORKER;

    if (worker.getLatitude() < -90 || worker.getLatitude() > 90 ||
        worker.getLongitude() < -180 || worker.getLongitude() > 180)
        return WORK_BAD_LOCATION;

    // Find City -> Locality -> WasteSpot
    WasteSpot *spot = findSpot(cities, cityName, localityName, spotID);

    if (spot == nullptr)
        return WORK_NO_SPOT;

    // Workers can only apply once the cleanup is paid for
    if (spot->status == PENDING || spot->status == VERIFIED)
        return WORK_NOT_FUNDED;

    if (spot->status != FUNDED)
        return WORK_CLOSED;

    // One application per worker, and no more than the priority queue holds
    int count = 0;

    for (Applicant *a = spot->applicantHead; a != nullptr; a = a->getNext())
    {
        if (a->getWorkerID() == worker.getID())
            return WORK_ALREADY_APPLIED;

        count++;
    }

    if (count >= MAX_APPLICANTS)
        return WORK_FULL;

    ApplicantRecord record = {};
    record.spotID = spotID;
    record.workerID = worker.getID();
    record.latitude = worker.getLatitude();
    record.longitude = worker.getLongitude();
    record.rating = worker.getRating();
    record.applicationTime = time(nullptr);

    // Save first, so the list never holds an application that isn't in the file
    if (!saveApplicant(record))
        return WORK_SAVE_FAILED;

    double distance = haversineDistance(spot->latitude, spot->longitude,
                                        record.latitude, record.longitude);

    Applicant *applicant = new Applicant(record.workerID,
                                         record.latitude, record.longitude,
                                         record.rating, distance,
                                         record.applicationTime);

    // Insert at the front of the spot's linked list
    applicant->setNext(spot->applicantHead);
    spot->applicantHead = applicant;

    return WORK_OK;
}

int WorkerAssignment::rankApplicants(const WasteSpot *spot, Applicant *ranked[MAX_APPLICANTS])
{
    PriorityQueue pq;

    // Put every applicant from the linked list into the priority queue
    for (Applicant *a = spot->applicantHead; a != nullptr; a = a->getNext())
        pq.push(a);

    // Pop them back out: best first
    int count = 0;

    while (!pq.isEmpty())
        ranked[count++] = pq.pop();

    return count;
}

WorkResult WorkerAssignment::assignWorkers(SpotList &spots, int spotID)
{
    WasteSpot *spot = spots.findSpot(spotID);

    if (spot == nullptr)
        return WORK_NO_SPOT;

    if (spot->status == PENDING || spot->status == VERIFIED)
        return WORK_NOT_FUNDED;

    // Already assigned, completed or rejected
    if (spot->status != FUNDED)
        return WORK_CLOSED;

    if (spot->applicantHead == nullptr)
        return WORK_NO_APPLICANTS;

    // Kept so the change can be undone if it can't be saved,
    // otherwise memory and the file would disagree
    WasteSpot before = *spot;

    Applicant *ranked[MAX_APPLICANTS];
    int count = rankApplicants(spot, ranked);

    spot->assignedCount = 0;

    // Take the best applicants until the spot has all the workers it needs
    for (int i = 0; i < count &&
                    spot->assignedCount < spot->workerLimit &&
                    spot->assignedCount < MAX_ASSIGNED_WORKERS; i++)
    {
        spot->assignedWorkers[spot->assignedCount] = ranked[i]->getWorkerID();
        spot->assignedCount++;
    }

    spot->status = ASSIGNED;

    if (!spots.updateSpot(*spot))
    {
        *spot = before;
        return WORK_SAVE_FAILED;
    }

    return WORK_OK;
}

// List funded spots. With the worker's GPS, also show how far each one is.
int WorkerAssignment::showFundedSpots(SpotList &spots, bool withDistance,
                                      double latitude, double longitude)
{
    int shown = 0;

    for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
    {
        if (s->status != FUNDED)
            continue;

        cout << "Spot #" << s->spotID
             << " | " << s->cityName << ", " << s->localityName
             << " | " << s->wasteType
             << " | needs " << s->workerLimit << " worker(s) at Rs " << WAGE_PER_WORKER
             << " | " << countApplicants(s) << " applied";

        if (withDistance)
        {
            double km = haversineDistance(s->latitude, s->longitude, latitude, longitude);
            printf(" | %.1f km away", km);
        }

        cout << endl;
        shown++;
    }

    return shown;
}

void WorkerAssignment::applyMenu(CityHashTable &cities, SpotList &spots)
{
    cout << "\n===== APPLY FOR A JOB =====\n";

    if (showFundedSpots(spots, false, 0, 0) == 0)
    {
        cout << "No funded spots need workers right now.\n";
        return;
    }

    int workerID;
    double latitude;
    double longitude;
    int spotID;

    cout << "\nEnter Worker ID: ";
    if (!readInt(workerID)) return;

    cout << "Enter your Latitude: ";
    if (!readDouble(latitude)) return;

    cout << "Enter your Longitude: ";
    if (!readDouble(longitude)) return;

    cout << "\nFunded spots and how far they are from you:\n";
    showFundedSpots(spots, true, latitude, longitude);

    cout << "\nEnter Spot ID: ";
    if (!readInt(spotID)) return;

    WasteSpot *spot = spots.findSpot(spotID);

    if (spot == nullptr)
    {
        cout << resultMessage(WORK_NO_SPOT) << endl;
        return;
    }

    // Same lookup as funding: City -> Locality -> spot, using the spot's own names.
    // Old spots may have no locality saved; the city table files those under the city name.
    string localityName = spot->localityName;

    if (localityName.empty())
        localityName = spot->cityName;

    Worker worker(workerID, latitude, longitude, DEFAULT_WORKER_RATING);

    WorkResult result = applyForSpot(cities, spot->cityName, localityName,
                                     spotID, worker);

    cout << resultMessage(result) << endl;

    if (result != WORK_OK)
        return;

    printf("Worker %d applied for Spot #%d (%.1f km away). %d applicant(s) so far.\n",
           workerID, spotID, spot->applicantHead->getDistance(), countApplicants(spot));
}

void WorkerAssignment::assignMenu(SpotList &spots)
{
    cout << "\n===== ASSIGN WORKERS =====\n";

    if (showFundedSpots(spots, false, 0, 0) == 0)
    {
        cout << "No funded spots are waiting for workers.\n";
        return;
    }

    int spotID;

    cout << "\nEnter Spot ID: ";
    if (!readInt(spotID)) return;

    WasteSpot *spot = spots.findSpot(spotID);

    WorkResult result = assignWorkers(spots, spotID);

    if (result != WORK_OK)
    {
        cout << resultMessage(result) << endl;
        return;
    }

    // Show the priority queue's order and who got a place
    Applicant *ranked[MAX_APPLICANTS];
    int count = rankApplicants(spot, ranked);

    cout << "\nApplicants, best first (closer km band, then higher rating, then earlier):\n";

    for (int i = 0; i < count; i++)
    {
        printf("%d. Worker %d | %.1f km | rating %.1f | applied ",
               i + 1, ranked[i]->getWorkerID(), ranked[i]->getDistance(),
               ranked[i]->getWorkerRating());
        printTime(ranked[i]->getApplicationTime());
        cout << (i < spot->assignedCount ? "  <- assigned" : "") << endl;
    }

    cout << "\nSpot #" << spotID << " is now Assigned: "
         << spot->assignedCount << " of " << spot->workerLimit << " place(s) filled.\n";

    if (spot->assignedCount < spot->workerLimit)
        cout << "Fewer workers applied than the spot needs.\n";
}

const char *WorkerAssignment::resultMessage(WorkResult result)
{
    switch (result)
    {
        case WORK_OK:              return "Done.";
        case WORK_NO_SPOT:         return "No spot with that ID.";
        case WORK_NOT_FUNDED:      return "This spot is not fully funded yet.";
        case WORK_CLOSED:          return "This spot already has its workers (or was rejected).";
        case WORK_BAD_WORKER:      return "Worker ID must be 1 or more.";
        case WORK_BAD_LOCATION:    return "Latitude must be -90 to 90 and longitude -180 to 180.";
        case WORK_ALREADY_APPLIED: return "You have already applied for this spot.";
        case WORK_FULL:            return "This spot has the most applicants it can take.";
        case WORK_NO_APPLICANTS:   return "Nobody has applied for this spot yet.";
        case WORK_SAVE_FAILED:     return "Could not save. Try again.";
    }

    return "Unknown result.";
}
