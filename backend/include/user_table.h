#ifndef USER_TABLE_H
#define USER_TABLE_H

#include <string>
#include "user.h"
#include "chained_hash.h"

using namespace std;

// Path is relative to the repo root, like SPOT_FILE
#define USER_FILE "backend/data/users.dat"

// One saved account. Fixed-size text, like WasteSpot, so the file is plain records.
typedef struct
{
    int id;
    char role[12];          // "user", "worker" or "moderator"
    char username[32];
    char salt[33];          // 16 random bytes as hex
    char passwordHash[65];  // PBKDF2-SHA256 as hex
} UserRecord;

typedef enum {
    USER_OK,
    USER_BAD_NAME,
    USER_BAD_PASSWORD,
    USER_BAD_ROLE,
    USER_NAME_TAKEN,
    USER_SAVE_FAILED
} UserResult;

// Every account, found by username in a chained hash table.
// The table owns the User objects (Citizen, Worker or Moderator).
class UserTable
{
private:
    ChainedHashTable<User *> byName;
    int nextUserID;

    // False when users.dat could not be loaded, so new accounts are not
    // mixed into a file we can't read (same rule as SpotList)
    bool canSave;

    bool saveToFile(const User &user);

public:
    UserTable();
    ~UserTable();

    UserTable(const UserTable &) = delete;
    UserTable &operator=(const UserTable &) = delete;

    void loadFromFile();

    // Make an account and save it. On USER_OK, created points at the new user.
    UserResult signUp(const string &username, const string &password,
                      const string &role, User *&created);

    // The user, if the name exists and the password is right. Otherwise nullptr.
    User *logIn(const string &username, const string &password);

    int count() const;

    static const char *resultMessage(UserResult result);
};

#endif
