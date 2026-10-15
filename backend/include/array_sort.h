#ifndef ARRAY_SORT_H
#define ARRAY_SORT_H

struct WorkerDistance
{
    int workerID;
    double distance;
};

void sortWorkersByDistance(WorkerDistance arr[], int n);

#endif