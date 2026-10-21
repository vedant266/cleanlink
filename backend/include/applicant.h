#ifndef APPLICANT_H
#define APPLICANT_H

class Applicant
{
private:
    int workerID;

    double workerLatitude;
    double workerLongitude;

    double workerRating;

    double distance;

    long long applicationTime;

    Applicant *next;

public:
    Applicant(int id,
              double latitude,
              double longitude,
              double rating,
              double dist,
              long long time);

    int getWorkerID() const;
    double getWorkerLatitude() const;
    double getWorkerLongitude() const;
    double getWorkerRating() const;
    double getDistance() const;
    long long getApplicationTime() const;

    Applicant *getNext() const;
    void setNext(Applicant *nextApplicant);
};

#endif