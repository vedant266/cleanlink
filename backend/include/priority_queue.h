#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "applicant.h"

#define MAX_APPLICANTS 100

// Workers within the same whole kilometre count as equally close,
// so their rating can decide between them
#define DISTANCE_BAND_KM 1.0

class PriorityQueue
{
private:
    Applicant *heap[MAX_APPLICANTS];
    int size;

    bool higherPriority(Applicant *a, Applicant *b);

    void swap(Applicant *&a, Applicant *&b);

    void heapifyUp(int index);
    void heapifyDown(int index);

public:
    PriorityQueue();

    bool isEmpty();

    bool isFull();

    void push(Applicant *applicant);

    Applicant *pop();

    int getSize();
};

#endif