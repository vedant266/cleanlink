#ifndef WORKER_ASSIGNMENT_H
#define WORKER_ASSIGNMENT_H

#include "waste_spot.h"
#include "user.h"

double calculateDistance(
    double lat1,
    double lon1,
    double lat2,
    double lon2
);

void applyForSpot(WasteSpot *spot, const Worker &worker);

void displayApplicants(WasteSpot *spot);

void assignWorkers(WasteSpot *spot);

void displayAssignedWorkers(WasteSpot *spot);

#endif