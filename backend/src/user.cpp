#include "user.h"

User::User(int userID)
{
    id = userID;
}

int User::getID() const
{
    return id;
}

Reporter::Reporter(int reporterID) : User(reporterID)
{
}

Worker::Worker(int workerID, double workerLatitude,
               double workerLongitude, double workerRating)
    : User(workerID)
{
    latitude = workerLatitude;
    longitude = workerLongitude;
    rating = workerRating;
}

double Worker::getLatitude() const
{
    return latitude;
}

double Worker::getLongitude() const
{
    return longitude;
}

double Worker::getRating() const
{
    return rating;
}