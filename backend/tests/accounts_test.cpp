// Checks the account code: SHA-256 / PBKDF2 against known answers, the chained
// hash table, and which roles each kind of user can act as.
//
// From the repo root:
//   g++ -std=c++17 -Ibackend/include backend/tests/accounts_test.cpp backend/src/password.cpp backend/src/user.cpp -o /tmp/accounts_test && /tmp/accounts_test

#include <cassert>
#include <iostream>
#include "password.h"
#include "chained_hash.h"
#include "user.h"

int main()
{
    // FIPS 180-2 / RFC 7914 vectors (the same answers as Node's crypto module)
    assert(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(pbkdf2Sha256Hex("password", "salt", 1) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    assert(pbkdf2Sha256Hex("password", "salt", 4096) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    // A key longer than one block is hashed first
    assert(pbkdf2Sha256Hex(string(100, 'k'), "NaCl", 2) == "de343c8855f07ca7aabe689a4007d9663341a47f0b925305e3e93616acc49535");

    string salt = randomHex(16);
    assert(salt.size() == 32 && salt != randomHex(16));
    string hash = hashPassword("correct horse", salt);
    assert(samePassword("correct horse", salt, hash));
    assert(!samePassword("correct hors", salt, hash));

    // Chained hash table: more keys than buckets, so lists must chain
    ChainedHashTable<int> table;
    for (int i = 0; i < 500; i++)
        assert(table.insert("key" + to_string(i), i));
    assert(!table.insert("key7", 0));  // no duplicates
    assert(table.size() == 500 && *table.find("key499") == 499);
    assert(table.remove("key250") && table.find("key250") == nullptr && !table.remove("key250"));
    assert(table.size() == 499 && *table.find("key251") == 251);

    // Roles: a Citizen is one User that is both a Reporter and a Donor
    User *citizen = User::create("user", 1, "asha", salt, hash);
    User *moderator = User::create("moderator", 2, "ravi", salt, hash);
    User *worker = User::create("worker", 3, "kiran", salt, hash);
    assert(User::create("admin", 4, "x", salt, hash) == nullptr);

    assert(dynamic_cast<Reporter *>(citizen) && dynamic_cast<Donor *>(citizen));
    assert(!dynamic_cast<Moderator *>(citizen));
    assert(dynamic_cast<Moderator *>(moderator) && !dynamic_cast<Donor *>(moderator));
    assert(dynamic_cast<Worker *>(worker) && !dynamic_cast<Reporter *>(worker));
    // Virtual inheritance: both paths reach the same User, with the same ID
    assert(dynamic_cast<Donor *>(citizen)->getID() == 1 && dynamic_cast<Reporter *>(citizen)->getID() == 1);
    assert(string(citizen->roleName()) == "user" && citizen->checkPassword("correct horse"));
    assert(!Moderator(9).checkPassword(""));  // made from a typed ID: no password, no login

    delete citizen;
    delete moderator;
    delete worker;

    std::cout << "accounts_test: all passed\n";
    return 0;
}
