#include "user_table.h"
#include "password.h"

#include <iostream>
#include <cstdio>
#include <cstring>

using namespace std;

// Names are stored in lower case, so "Asha" and "asha" are the same account
static string lowerCase(const string &text)
{
    string out = text;
    for (char &c : out)
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
    return out;
}

// 3 to 31 characters: letters, digits, '_', '.' or '-'
static bool validUsername(const string &name)
{
    if (name.size() < 3 || name.size() >= sizeof(((UserRecord *)0)->username))
        return false;

    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'))
            return false;
    return true;
}

static bool validRecord(const UserRecord &r)
{
    // Text fields must end inside their arrays
    return r.id >= 1 &&
           memchr(r.role, '\0', sizeof r.role) && memchr(r.username, '\0', sizeof r.username) &&
           memchr(r.salt, '\0', sizeof r.salt) && memchr(r.passwordHash, '\0', sizeof r.passwordHash);
}

UserTable::UserTable()
{
    nextUserID = 1;
    canSave = true;
}

UserTable::~UserTable()
{
    // The table's nodes hold pointers; the User objects themselves are ours to free
    byName.forEach([](User *user) { delete user; });
}

void UserTable::loadFromFile()
{
    FILE *fp = fopen(USER_FILE, "rb");

    // No file yet means nobody has signed up
    if (fp == NULL)
        return;

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);

    if (fileSize % sizeof(UserRecord) != 0)
    {
        cout << USER_FILE << " was saved by an older version of CleanLink.\n"
             << "Until it is renamed or deleted, new accounts will NOT be saved.\n";
        canSave = false;
        fclose(fp);
        return;
    }

    UserRecord record;
    int damaged = 0;

    while (fread(&record, sizeof(UserRecord), 1, fp) == 1)
    {
        User *user = validRecord(record)
            ? User::create(record.role, record.id, record.username, record.salt, record.passwordHash)
            : nullptr;

        if (user == nullptr || !byName.insert(user->getUsername(), user))
        {
            delete user;
            damaged++;
            continue;
        }

        if (record.id >= nextUserID)
            nextUserID = record.id + 1;
    }

    fclose(fp);

    cout << "Loaded " << byName.size() << " account(s) from " << USER_FILE << endl;

    if (damaged > 0)
    {
        cout << damaged << " damaged account record(s) were skipped.\n"
             << "Until the file is fixed, new accounts will NOT be saved.\n";
        canSave = false;
    }
}

bool UserTable::saveToFile(const User &user)
{
    if (!canSave)
        return false;

    UserRecord record;
    memset(&record, 0, sizeof record);  // no leftover memory in the file
    record.id = user.getID();
    strncpy(record.role, user.roleName(), sizeof record.role - 1);
    strncpy(record.username, user.getUsername().c_str(), sizeof record.username - 1);
    strncpy(record.salt, user.getSalt().c_str(), sizeof record.salt - 1);
    strncpy(record.passwordHash, user.getPasswordHash().c_str(), sizeof record.passwordHash - 1);

    FILE *fp = fopen(USER_FILE, "ab");
    if (fp == NULL)
    {
        cout << "Error opening " << USER_FILE << endl;
        return false;
    }

    bool saved = fwrite(&record, sizeof record, 1, fp) == 1;
    if (fclose(fp) != 0)
        saved = false;
    return saved;
}

UserResult UserTable::signUp(const string &username, const string &password,
                             const string &role, User *&created)
{
    string name = lowerCase(username);
    created = nullptr;

    if (!validUsername(name))
        return USER_BAD_NAME;
    if (password.size() < 8 || password.size() > 128)
        return USER_BAD_PASSWORD;
    if (byName.find(name) != nullptr)
        return USER_NAME_TAKEN;

    string salt = randomHex(16);
    User *user = User::create(role, nextUserID, name, salt, hashPassword(password, salt));
    if (user == nullptr)
        return USER_BAD_ROLE;

    // Save first, so the table never holds an account that isn't in the file
    if (!saveToFile(*user))
    {
        delete user;
        return USER_SAVE_FAILED;
    }

    byName.insert(name, user);
    nextUserID++;
    created = user;
    return USER_OK;
}

User *UserTable::logIn(const string &username, const string &password)
{
    User **found = byName.find(lowerCase(username));
    if (found == nullptr || !(*found)->checkPassword(password))
        return nullptr;
    return *found;
}

int UserTable::count() const
{
    return byName.size();
}

const char *UserTable::resultMessage(UserResult result)
{
    switch (result)
    {
        case USER_OK:           return "Account created.";
        case USER_BAD_NAME:     return "Username must be 3 to 31 letters, digits, '_', '.' or '-'.";
        case USER_BAD_PASSWORD: return "Password must be 8 to 128 characters.";
        case USER_BAD_ROLE:     return "Choose user or worker.";
        case USER_NAME_TAKEN:   return "That username is taken.";
        case USER_SAVE_FAILED:  return "Could not save the account. Try again later.";
    }
    return "Something went wrong.";
}
