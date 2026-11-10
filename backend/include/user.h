#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

// Base class for everyone who uses CleanLink. Abstract: every kind of user
// says what it is through roleName(), so a User* can be any of them.
//
// Reporter and Donor inherit User "virtually": a Citizen is both, and without
// virtual inheritance it would hold two separate Users (two IDs, two names).
class User
{
protected:
    int id;

    // Only filled for accounts. The terminal menus still make users from a typed ID.
    string username;
    string salt;
    string passwordHash;

public:
    User(int userID, const string &name = "", const string &passwordSalt = "",
         const string &hash = "");
    virtual ~User();

    int getID() const;
    const string &getUsername() const;
    const string &getSalt() const;
    const string &getPasswordHash() const;

    bool checkPassword(const string &password) const;

    // What is saved in users.dat and shown on the website
    virtual const char *roleName() const = 0;

    // Makes the right kind of user from a saved role name (the factory for
    // logging in). nullptr if the role is not one people can sign up as.
    static User *create(const string &role, int userID, const string &name,
                        const string &passwordSalt, const string &hash);
};

// A person who reports a waste spot
class Reporter : public virtual User
{
public:
    Reporter(int reporterID);
    const char *roleName() const override;
};

// A person or organisation that donates money to clean a verified spot
class Donor : public virtual User
{
public:
    Donor(int donorID);
    const char *roleName() const override;
};

// A "user" account on the website: reports spots and donates to them.
// One object, one ID, usable anywhere a Reporter or a Donor is expected.
class Citizen : public Reporter, public Donor
{
public:
    Citizen(int userID, const string &name, const string &passwordSalt, const string &hash);

    // Reporter and Donor each give a roleName, so Citizen must pick its own
    const char *roleName() const override;
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

    // An account: location and rating are given each time they apply
    Worker(int workerID, const string &name, const string &passwordSalt, const string &hash);

    double getLatitude() const;
    double getLongitude() const;
    double getRating() const;
    const char *roleName() const override;
};

// A person who checks a report before it can be funded
class Moderator : public User
{
public:
    Moderator(int moderatorID);
    Moderator(int moderatorID, const string &name, const string &passwordSalt, const string &hash);
    const char *roleName() const override;
};

#endif
