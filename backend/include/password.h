#ifndef PASSWORD_H
#define PASSWORD_H

#include <string>
using namespace std;

// Passwords are never saved. We save a random salt and
// PBKDF2-HMAC-SHA256(password, salt), both as hex text.
#define PASSWORD_ITERATIONS 100000

string sha256Hex(const string &data);

// Derived key (32 bytes) as 64 hex characters
string pbkdf2Sha256Hex(const string &password, const string &salt, int iterations);

// Random bytes from the OS, as hex (16 bytes -> 32 characters)
string randomHex(int bytes);

string hashPassword(const string &password, const string &salt);

// Compares every character, so the time taken doesn't hint how much matched
bool samePassword(const string &password, const string &salt, const string &savedHash);

#endif
