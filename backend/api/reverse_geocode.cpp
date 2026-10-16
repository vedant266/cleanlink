#include "reverse_geocode.h"
#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// This function receives the data coming from the API
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output)
{
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Return the first of these address keys that has a value
static string firstOf(const json& address, initializer_list<const char*> keys)
{
    for (const char* key : keys)
    {
        if (address.contains(key) && address[key].is_string())
        {
            string value = address[key];
            if (!value.empty())
                return value;
        }
    }
    return "";
}

Location reverseGeocode(double latitude, double longitude)
{
    Location location;

    CURL* curl;
    CURLcode res;

    string response;

    curl = curl_easy_init();

    if (curl)
    {
        char coords[64];
        snprintf(coords, sizeof(coords), "&lat=%.6f&lon=%.6f", latitude, longitude);

        // zoom=18 asks for the most detailed match, accept-language keeps names in English
        string url =
            "https://nominatim.openstreetmap.org/reverse"
            "?format=json&zoom=18&accept-language=en" + string(coords);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Receive API response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Identify our application
        curl_easy_setopt(
            curl,
            CURLOPT_USERAGENT,
            "CleanLink/1.0"
        );

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (res == CURLE_OK && httpCode != 200)
        {
            cerr << "API returned HTTP " << httpCode << endl;
        }
        else if (res == CURLE_OK)
        {
            try
            {
                json data = json::parse(response);

                if (data.contains("error"))
                {
                    cerr << "No location found: " << data["error"] << endl;
                }
                else if (data.contains("address"))
                {
                    json address = data["address"];

                    // Smaller places have no "city" key, so fall back to bigger areas
                    location.cityName = firstOf(address, {
                        "city", "town", "village", "municipality",
                        "county", "state_district", "state"
                    });

                    // Pick the most specific area name that isn't the city itself
                    for (const char* key : {"neighbourhood", "suburb", "quarter", "hamlet",
                                            "city_district", "residential", "road"})
                    {
                        string value = firstOf(address, {key});
                        if (!value.empty() && value != location.cityName)
                        {
                            location.localityName = value;
                            break;
                        }
                    }
                }
            }
            catch (const exception& e)
            {
                cerr << "JSON parsing error: "
                     << e.what() << endl;
            }
        }
        else
        {
            cerr << "API request failed: "
                 << curl_easy_strerror(res)
                 << endl;
        }

        curl_easy_cleanup(curl);
    }

    return location;
}