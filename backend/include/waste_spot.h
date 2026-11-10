#ifndef WASTE_SPOT_H
#define WASTE_SPOT_H

#include "applicant.h"

// Waste size
typedef enum {
    SMALL,
    MEDIUM,
    LARGE
} WasteSize;

// Spot status
typedef enum {
    PENDING,
    VERIFIED,
    FUNDED,
    ASSIGNED,
    COMPLETED,
    // Added last so the numbers already saved in waste_spots.dat keep their meaning
    REJECTED,
    // Section 18: an assigned worker sent an "after" photo; waits for a moderator
    SUBMITTED
} SpotStatus;

// The last status above, for range checks
#define LAST_STATUS SUBMITTED

#define MAX_ASSIGNED_WORKERS 20

// Rupees paid to each worker. cleanupCost = workerLimit * WAGE_PER_WORKER
#define WAGE_PER_WORKER 500

typedef struct WasteSpot
{
    int spotID;
    int reporterID;

    double latitude;
    double longitude;

    // Filled by reverse geocoding
    char cityName[100];
    char localityName[100];

    char photoPath[200];
    char wasteType[50];

    WasteSize estimatedSize;
    int workerLimit;

    SpotStatus status;

    // Moderator who verified or rejected the report (0 = not checked yet)
    int moderatorID;

    // Funding, in whole rupees (int, not double, so totals add up exactly)
    int cleanupCost;
    int fundedAmount;

    // Linked list of applicants
    Applicant *applicantHead;

    // Workers selected by priority queue
    int assignedWorkers[MAX_ASSIGNED_WORKERS];
    int assignedCount;

    // Linked list of waste spots
    struct WasteSpot *next;

} WasteSpot;

#endif