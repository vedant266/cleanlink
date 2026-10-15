#include "photo_location.h"
#include <fstream>
#include <vector>
#include <cstring>

using namespace std;

static vector<unsigned char> bytes;
static size_t tiffStart;
static bool littleEndian;

static unsigned int read16(size_t pos)
{
    if (pos + 2 > bytes.size()) return 0;
    if (littleEndian)
        return bytes[pos] | (bytes[pos + 1] << 8);
    return (bytes[pos] << 8) | bytes[pos + 1];
}

static unsigned int read32(size_t pos)
{
    if (pos + 4 > bytes.size()) return 0;
    if (littleEndian)
        return read16(pos) | (read16(pos + 2) << 16);
    return (read16(pos) << 16) | read16(pos + 2);
}

// GPS stores degrees, minutes, seconds as 3 fractions
static double readDegrees(unsigned int offset)
{
    double parts[3];

    for (int i = 0; i < 3; i++)
    {
        size_t pos = tiffStart + offset + i * 8;
        unsigned int num = read32(pos);
        unsigned int den = read32(pos + 4);
        parts[i] = den ? (double)num / den : 0;
    }

    return parts[0] + parts[1] / 60 + parts[2] / 3600;
}

bool getPhotoLocation(const string &photoPath, double &latitude, double &longitude)
{
    ifstream file(photoPath, ios::binary);
    if (!file) return false;

    bytes.assign(istreambuf_iterator<char>(file), istreambuf_iterator<char>());

    // JPEG files start with FF D8
    if (bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) return false;

    // Find the EXIF block (APP1 marker FF E1)
    size_t pos = 2;
    tiffStart = 0;

    while (pos + 10 < bytes.size() && bytes[pos] == 0xFF)
    {
        unsigned int marker = bytes[pos + 1];
        unsigned int length = (bytes[pos + 2] << 8) | bytes[pos + 3];

        if (marker == 0xE1 && memcmp(&bytes[pos + 4], "Exif\0\0", 6) == 0)
        {
            tiffStart = pos + 10;
            break;
        }

        if (marker == 0xDA) break; // image data starts, no EXIF found
        pos += 2 + length;
    }

    if (tiffStart == 0) return false;

    littleEndian = (bytes[tiffStart] == 'I');

    // Look for the GPS section pointer (tag 0x8825) in the first IFD
    size_t ifd = tiffStart + read32(tiffStart + 4);
    unsigned int count = read16(ifd);
    unsigned int gpsOffset = 0;

    for (unsigned int i = 0; i < count; i++)
    {
        size_t entry = ifd + 2 + i * 12;
        if (read16(entry) == 0x8825)
            gpsOffset = read32(entry + 8);
    }

    if (gpsOffset == 0) return false;

    // Read latitude/longitude and their N/S, E/W refs
    size_t gps = tiffStart + gpsOffset;
    count = read16(gps);

    char latRef = 'N', lonRef = 'E';
    bool hasLat = false, hasLon = false;

    for (unsigned int i = 0; i < count; i++)
    {
        size_t entry = gps + 2 + i * 12;
        unsigned int tag = read16(entry);

        if (tag == 1) latRef = bytes[entry + 8];
        if (tag == 2) { latitude = readDegrees(read32(entry + 8)); hasLat = true; }
        if (tag == 3) lonRef = bytes[entry + 8];
        if (tag == 4) { longitude = readDegrees(read32(entry + 8)); hasLon = true; }
    }

    if (latRef == 'S') latitude = -latitude;
    if (lonRef == 'W') longitude = -longitude;

    return hasLat && hasLon;
}
