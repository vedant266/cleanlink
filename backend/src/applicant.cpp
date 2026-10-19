#include "applicant.h"

Applicant::Applicant(int id,
                     double latitude,
                     double longitude,
                     double rating,
                     double dist,
                     long long time)
{
    workerID = id;
    workerLatitude = latitude;
    workerLongitude = longitude;
    workerRating = rating;
    distance = dist;
    applicationTime = time;
    next = nullptr;
}

int Applicant::getWorkerID() const
{
    return workerID;
}

double Applicant::getWorkerLatitude() const
{
    return workerLatitude;
}

double Applicant::getWorkerLongitude() const
{
    return workerLongitude;
}

double Applicant::getWorkerRating() const
{
    return workerRating;
}

double Applicant::getDistance() const
{
    return distance;
}

long long Applicant::getApplicationTime() const
{
    return applicationTime;
}

Applicant *Applicant::getNext() const
{
    return next;
}

void Applicant::setNext(Applicant *nextApplicant)
{
    next = nextApplicant;
}