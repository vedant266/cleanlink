#ifndef PHOTO_LOCATION_H
#define PHOTO_LOCATION_H

#include <string>

// Reads GPS latitude/longitude from a JPEG photo's EXIF data.
// Returns false if the photo has no GPS info.
bool getPhotoLocation(const std::string &photoPath, double &latitude, double &longitude);

#endif
