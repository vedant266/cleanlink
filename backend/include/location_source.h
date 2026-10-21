#ifndef LOCATION_SOURCE_H
#define LOCATION_SOURCE_H

#include <string>

// Base class: any way of getting a waste spot's coordinates
class LocationSource
{
public:
    virtual ~LocationSource() {}

    // Returns true if it found a location and stored it in latitude/longitude
    virtual bool getLocation(const std::string &photoPath,
                             double &latitude,
                             double &longitude) = 0;
};

// Reads the location from the photo's GPS data
class PhotoLocationSource : public LocationSource
{
public:
    bool getLocation(const std::string &photoPath,
                     double &latitude,
                     double &longitude) override;
};

// Asks the user to type the location
class ManualLocationSource : public LocationSource
{
public:
    bool getLocation(const std::string &photoPath,
                     double &latitude,
                     double &longitude) override;
};

#endif