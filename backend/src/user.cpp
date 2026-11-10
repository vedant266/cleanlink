#include "user.h"
#include "password.h"

User::User(int userID, const string &name, const string &passwordSalt, const string &hash)
{
    id = userID;
    username = name;
    salt = passwordSalt;
    passwordHash = hash;
}

User::~User()
{
}

int User::getID() const
{
    return id;
}

const string &User::getUsername() const
{
    return username;
}

const string &User::getSalt() const
{
    return salt;
}

const string &User::getPasswordHash() const
{
    return passwordHash;
}

bool User::checkPassword(const string &password) const
{
    // A user made from a typed ID has no password, so it can never log in
    return !passwordHash.empty() && samePassword(password, salt, passwordHash);
}

User *User::create(const string &role, int userID, const string &name,
                   const string &passwordSalt, const string &hash)
{
    if (role == "user")
        return new Citizen(userID, name, passwordSalt, hash);
    if (role == "worker")
        return new Worker(userID, name, passwordSalt, hash);
    if (role == "moderator")
        return new Moderator(userID, name, passwordSalt, hash);
    return nullptr;
}

Reporter::Reporter(int reporterID) : User(reporterID)
{
}

const char *Reporter::roleName() const
{
    return "reporter";
}

Donor::Donor(int donorID) : User(donorID)
{
}

const char *Donor::roleName() const
{
    return "donor";
}

// With virtual inheritance the most-derived class builds the one shared User,
// so the User(...) here is the one that counts, not the ones in Reporter / Donor
Citizen::Citizen(int userID, const string &name, const string &passwordSalt, const string &hash)
    : User(userID, name, passwordSalt, hash), Reporter(userID), Donor(userID)
{
}

const char *Citizen::roleName() const
{
    return "user";
}

Worker::Worker(int workerID, double workerLatitude,
               double workerLongitude, double workerRating)
    : User(workerID)
{
    latitude = workerLatitude;
    longitude = workerLongitude;
    rating = workerRating;
}

Worker::Worker(int workerID, const string &name, const string &passwordSalt, const string &hash)
    : User(workerID, name, passwordSalt, hash)
{
    latitude = 0;
    longitude = 0;
    rating = 0;
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

const char *Worker::roleName() const
{
    return "worker";
}

Moderator::Moderator(int moderatorID) : User(moderatorID)
{
}

Moderator::Moderator(int moderatorID, const string &name, const string &passwordSalt, const string &hash)
    : User(moderatorID, name, passwordSalt, hash)
{
}

const char *Moderator::roleName() const
{
    return "moderator";
}
