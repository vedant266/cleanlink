// SHA-256 (FIPS 180-4), HMAC (RFC 2104) and PBKDF2 (RFC 8018), written out
// because not every machine that builds CleanLink has OpenSSL.
// Checked against the published test vectors in backend/tests/password_test.cpp.

#include "password.h"
#include <cstdint>
#include <cstdio>
#include <random>

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

// Raw 32-byte digest
static string sha256(const string &data)
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    // Padding: a 1 bit, zeros, then the length in bits, to a multiple of 64 bytes
    string msg = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    msg += (char)0x80;
    while (msg.size() % 64 != 56)
        msg += (char)0;
    for (int i = 7; i >= 0; i--)
        msg += (char)(bits >> (i * 8));

    for (size_t block = 0; block < msg.size(); block += 64)
    {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)(unsigned char)msg[block + i * 4] << 24 |
                   (uint32_t)(unsigned char)msg[block + i * 4 + 1] << 16 |
                   (uint32_t)(unsigned char)msg[block + i * 4 + 2] << 8 |
                   (uint32_t)(unsigned char)msg[block + i * 4 + 3];
        for (int i = 16; i < 64; i++)
        {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++)
        {
            uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
            uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    string out;
    for (int i = 0; i < 8; i++)
        for (int j = 3; j >= 0; j--)
            out += (char)(h[i] >> (j * 8));
    return out;
}

static string toHex(const string &bytes)
{
    string hex;
    char two[3];
    for (unsigned char c : bytes)
    {
        snprintf(two, sizeof two, "%02x", c);
        hex += two;
    }
    return hex;
}

static string hmacSha256(const string &key, const string &message)
{
    string k = key.size() > 64 ? sha256(key) : key;
    k.resize(64, (char)0);

    string inner(64, 0), outer(64, 0);
    for (int i = 0; i < 64; i++)
    {
        inner[i] = k[i] ^ 0x36;
        outer[i] = k[i] ^ 0x5c;
    }
    return sha256(outer + sha256(inner + message));
}

string sha256Hex(const string &data)
{
    return toHex(sha256(data));
}

// Only the first 32-byte block of PBKDF2, which is all we store
string pbkdf2Sha256Hex(const string &password, const string &salt, int iterations)
{
    string u = hmacSha256(password, salt + string("\0\0\0\1", 4));
    string result = u;

    for (int i = 1; i < iterations; i++)
    {
        u = hmacSha256(password, u);
        for (int j = 0; j < 32; j++)
            result[j] ^= u[j];
    }
    return toHex(result);
}

string randomHex(int bytes)
{
    std::random_device device;  // the OS's random source
    string raw;
    for (int i = 0; i < bytes; i++)
        raw += (char)(device() & 0xff);
    return toHex(raw);
}

string hashPassword(const string &password, const string &salt)
{
    return pbkdf2Sha256Hex(password, salt, PASSWORD_ITERATIONS);
}

bool samePassword(const string &password, const string &salt, const string &savedHash)
{
    string hash = hashPassword(password, salt);
    if (hash.size() != savedHash.size())
        return false;

    unsigned char diff = 0;
    for (size_t i = 0; i < hash.size(); i++)
        diff |= hash[i] ^ savedHash[i];
    return diff == 0;
}
