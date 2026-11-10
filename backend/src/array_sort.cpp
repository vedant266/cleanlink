#include "array_sort.h"

void sortWorkersByDistance(WorkerDistance arr[], int n)
{
    for (int i = 1; i < n; i++)
    {
        WorkerDistance key = arr[i];
        int j = i - 1;

        while (j >= 0 && arr[j].distance > key.distance)
        {
            arr[j + 1] = arr[j];
            j--;
        }

        arr[j + 1] = key;
    }
}

// Same insertion sort as above, for spots instead of workers
void sortSpotsByDistance(SpotDistance arr[], int n)
{
    for (int i = 1; i < n; i++)
    {
        SpotDistance key = arr[i];
        int j = i - 1;

        while (j >= 0 && arr[j].distance > key.distance)
        {
            arr[j + 1] = arr[j];
            j--;
        }

        arr[j + 1] = key;
    }
}