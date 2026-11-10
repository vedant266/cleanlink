#ifndef ARRAY_SORT_H
#define ARRAY_SORT_H

struct WorkerDistance
{
    int workerID;
    double distance;
};

void sortWorkersByDistance(WorkerDistance arr[], int n);

struct WasteSpot;

// A spot and how far it is from a worker, for the worker's job list
struct SpotDistance
{
    WasteSpot *spot;
    double distance;
};

// Insertion sort, nearest spot first
void sortSpotsByDistance(SpotDistance arr[], int n);

#endif