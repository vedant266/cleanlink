#include "photo_location.h"
#include <fstream>
#include <vector>
#include <cstring>

using namespace std;

// Everything one call needs while reading one photo. Each call has its own,
// so the web server can read two photos at the same time. (These used to be
// static variables shared by every call, and two reports arriving together
// overwrote each other's buffer and crashed the server.)
struct Exif
{
    vector<unsigned char> bytes;
    size_t tiffStart = 0;
    bool littleEndian = false;
};

// One byte, or 0 if the position is past the end of the file
static unsigned int byteAt(const Exif &exif, size_t pos)
{
    return pos < exif.bytes.size() ? exif.bytes[pos] : 0;
}

static unsigned int read16(const Exif &exif, size_t pos)
{
    if (pos + 2 > exif.bytes.size()) return 0;
    if (exif.littleEndian)
        return exif.bytes[pos] | (exif.bytes[pos + 1] << 8);
    return (exif.bytes[pos] << 8) | exif.bytes[pos + 1];
}

static unsigned int read32(const Exif &exif, size_t pos)
{
    if (pos + 4 > exif.bytes.size()) return 0;
    if (exif.littleEndian)
        return read16(exif, pos) | (read16(exif, pos + 2) << 16);
    return (read16(exif, pos) << 16) | read16(exif, pos + 2);
}

// GPS stores degrees, minutes, seconds as 3 fractions
static double readDegrees(const Exif &exif, unsigned int offset)
{
    double parts[3];

    for (int i = 0; i < 3; i++)
    {
        size_t pos = exif.tiffStart + offset + i * 8;
        unsigned int num = read32(exif, pos);
        unsigned int den = read32(exif, pos + 4);
        parts[i] = den ? (double)num / den : 0;
    }

    return parts[0] + parts[1] / 60 + parts[2] / 3600;
}

bool getPhotoLocation(const string &photoPath, double &latitude, double &longitude)
{
    ifstream file(photoPath, ios::binary);
    if (!file) return false;

    Exif exif;
    vector<unsigned char> &bytes = exif.bytes;
    bytes.assign(istreambuf_iterator<char>(file), istreambuf_iterator<char>());

    // JPEG files start with FF D8
    if (bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) return false;

    // Find the EXIF block (APP1 marker FF E1)
    size_t pos = 2;

    while (pos + 10 < bytes.size() && bytes[pos] == 0xFF)
    {
        unsigned int marker = bytes[pos + 1];
        unsigned int length = (bytes[pos + 2] << 8) | bytes[pos + 3];

        if (marker == 0xE1 && memcmp(&bytes[pos + 4], "Exif\0\0", 6) == 0)
        {
            exif.tiffStart = pos + 10;
            break;
        }

        if (marker == 0xDA) break; // image data starts, no EXIF found
        pos += 2 + length;
    }

    if (exif.tiffStart == 0) return false;

    exif.littleEndian = (byteAt(exif, exif.tiffStart) == 'I');

    // Look for the GPS section pointer (tag 0x8825) in the first IFD
    size_t ifd = exif.tiffStart + read32(exif, exif.tiffStart + 4);
    unsigned int count = read16(exif, ifd);
    unsigned int gpsOffset = 0;

    for (unsigned int i = 0; i < count; i++)
    {
        size_t entry = ifd + 2 + i * 12;
        if (read16(exif, entry) == 0x8825)
            gpsOffset = read32(exif, entry + 8);
    }

    if (gpsOffset == 0) return false;

    // Read latitude/longitude and their N/S, E/W refs
    size_t gps = exif.tiffStart + gpsOffset;
    count = read16(exif, gps);

    char latRef = 'N', lonRef = 'E';
    bool hasLat = false, hasLon = false;

    for (unsigned int i = 0; i < count; i++)
    {
        size_t entry = gps + 2 + i * 12;
        unsigned int tag = read16(exif, entry);

        if (tag == 1) latRef = (char)byteAt(exif, entry + 8);
        if (tag == 2) { latitude = readDegrees(exif, read32(exif, entry + 8)); hasLat = true; }
        if (tag == 3) lonRef = (char)byteAt(exif, entry + 8);
        if (tag == 4) { longitude = readDegrees(exif, read32(exif, entry + 8)); hasLon = true; }
    }

    if (latRef == 'S') latitude = -latitude;
    if (lonRef == 'W') longitude = -longitude;

    return hasLat && hasLon;
}
