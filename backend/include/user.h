#ifndef USER_H
#define USER_H

// Base class for everyone who uses CleanLink
class User
{
protected:
    int id;

public:
    User(int userID);

    int getID() const;
};

// A person who reports a waste spot
class Reporter : public User
{
public:
    Reporter(int reporterID);
};

// A person who applies to clean a waste spot
class Worker : public User
{
private:
    double latitude;
    double longitude;
    double rating;

public:
    Worker(int workerID, double workerLatitude,
           double workerLongitude, double workerRating);

    double getLatitude() const;
    double getLongitude() const;
    double getRating() const;
};

#endif