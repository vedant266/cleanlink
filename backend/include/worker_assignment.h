#ifndef WORKER_ASSIGNMENT_H
#define WORKER_ASSIGNMENT_H

#include <string>
#include <vector>
#include "waste_spot.h"
#include "array_sort.h"
#include "spot_list.h"
#include "city_hash.h"
#include "user.h"
#include "priority_queue.h"

using namespace std;

// Path is relative to the repo root, like SPOT_FILE
#define APPLICANT_FILE "backend/data/applicants.dat"

// Every worker gets this rating until CleanScore exists.
// Workers never type their own rating, or everyone would pick 5.
#define DEFAULT_WORKER_RATING 3.0

// A worker is only shown jobs this close to them
#define MAX_JOB_DISTANCE_KM 20.0

// One saved application. The distance is not saved: it is worked out again
// from the spot's GPS and the worker's GPS when the file is loaded.
typedef struct
{
    int spotID;
    int workerID;
    double latitude;     // where the worker was when they applied
    double longitude;
    double rating;
    long long applicationTime;
} ApplicantRecord;

// What happened when a worker applied, or when workers were assigned
typedef enum {
    WORK_OK,
    WORK_NO_SPOT,
    WORK_NOT_FUNDED,
    WORK_CLOSED,
    WORK_BAD_WORKER,
    WORK_BAD_LOCATION,
    WORK_ALREADY_APPLIED,
    WORK_FULL,
    WORK_NO_APPLICANTS,
    WORK_SAVE_FAILED
} WorkResult;

class WorkerAssignment
{
private:
    // False when applicants.dat could not be loaded, so new applications
    // are not mixed into a file we can't read
    bool canSave;

    bool saveApplicant(const ApplicantRecord &record);

    // City -> Locality -> that locality's spot list. nullptr if not found.
    WasteSpot *findSpot(CityHashTable &cities, string cityName,
                        string localityName, int spotID);

    int showFundedSpots(SpotList &spots, bool withDistance,
                        double latitude, double longitude);

public:
    WorkerAssignment();

    // Read applicants.dat and rebuild each spot's applicant linked list.
    // Call after the spots are loaded.
    void loadFromFile(SpotList &spots);

    // Section 13: a worker applies to a funded spot. Shared by the terminal
    // menu and (later) the web server.
    WorkResult applyForSpot(CityHashTable &cities,
                            string cityName, string localityName,
                            int spotID, const Worker &worker);

    // Section 14: the priority queue picks the best applicants, up to the
    // spot's worker limit, and the spot becomes Assigned
    WorkResult assignWorkers(SpotList &spots, int spotID);

    // Every applicant of the spot, best first (fills ranked, returns how many)
    static int rankApplicants(const WasteSpot *spot, Applicant *ranked[MAX_APPLICANTS]);

    static int countApplicants(const WasteSpot *spot);

    // True if this worker has already applied for the spot
    static bool hasApplied(const WasteSpot *spot, int workerID);

    // Section 12 (first version): funded spots within MAX_JOB_DISTANCE_KM of
    // the worker that they haven't applied for yet, nearest first.
    // workerID 0 = don't leave any out.
    static vector<SpotDistance> findJobs(SpotList &spots, double latitude,
                                         double longitude, int workerID);

    // Terminal menus
    void applyMenu(CityHashTable &cities, SpotList &spots);
    void assignMenu(SpotList &spots);

    static const char *resultMessage(WorkResult result);
};

#endif
