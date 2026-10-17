// Small local web server so CleanLink can be used from the browser.
// It uses the same SpotList / ReportSpot code as the terminal app.

#include "httplib.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "report_spot.h"
#include "spot_list.h"
#include "photo_location.h"

using json = nlohmann::json;

#define PORT 8080
#define UPLOAD_DIR "uploads"

SpotList spots;
ReportSpot report;
std::mutex spotsLock; // one report at a time

json spotToJson(const WasteSpot *spot)
{
    const char *sizeNames[] = {"Small", "Medium", "Large"};
    const char *statusNames[] = {"Pending", "Verified", "Funded", "Assigned", "Completed"};

    return {
        {"id", spot->spotID},
        {"reporterID", spot->reporterID},
        {"latitude", spot->latitude},
        {"longitude", spot->longitude},
        {"city", spot->cityName},
        {"locality", spot->localityName},
        {"wasteType", spot->wasteType},
        {"size", sizeNames[spot->estimatedSize]},
        {"workersNeeded", spot->workerLimit},
        {"status", statusNames[spot->status]},
        {"photo", spot->photoPath}
    };
}

void sendError(httplib::Response &res, const std::string &message)
{
    res.status = 400;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}

int main(int argc, char *argv[])
{
    // Run from the repo folder so frontend/, uploads/ and the .dat file are found
    std::string exe = argv[0];
    size_t slash = exe.rfind('/');
    if (slash != std::string::npos)
        chdir(exe.substr(0, slash).c_str());

    mkdir(UPLOAD_DIR, 0755);
    spots.loadFromFile();

    httplib::Server server;

    // List all reported spots
    server.Get("/api/spots", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> guard(spotsLock);

        json list = json::array();
        for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
            list.push_back(spotToJson(s));

        res.set_content(list.dump(), "application/json");
    });

    // Report a new spot (form with an uploaded photo)
    server.Post("/api/report", [](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_file("photo") || req.get_file_value("photo").content.empty())
            return sendError(res, "Upload a photo of the spot.");

        std::string reporter = req.get_file_value("reporterID").content;
        std::string wasteType = req.get_file_value("wasteType").content;
        std::string sizeValue = req.get_file_value("size").content;

        if (reporter.empty() || wasteType.empty() || sizeValue.empty())
            return sendError(res, "Fill in reporter ID, waste type and size.");

        // Save the uploaded photo into uploads/
        auto photo = req.get_file_value("photo");
        std::string ext = ".jpg";
        size_t dot = photo.filename.rfind('.');
        if (dot != std::string::npos)
            ext = photo.filename.substr(dot);

        std::string photoPath = std::string(UPLOAD_DIR) + "/spot_" + std::to_string(time(nullptr)) + ext;
        std::ofstream(photoPath, std::ios::binary) << photo.content;

        // Location from the photo, or from the form if the photo has none
        double latitude, longitude;
        if (!getPhotoLocation(photoPath, latitude, longitude))
        {
            std::string lat = req.get_file_value("latitude").content;
            std::string lon = req.get_file_value("longitude").content;

            if (lat.empty() || lon.empty())
            {
                remove(photoPath.c_str());
                return sendError(res, "No GPS in this photo. Enter latitude and longitude.");
            }

            try
            {
                latitude = std::stod(lat);
                longitude = std::stod(lon);
            }
            catch (const std::exception &)
            {
                remove(photoPath.c_str());
                return sendError(res, "Latitude and longitude must be numbers.");
            }
        }

        if (latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
        {
            remove(photoPath.c_str());
            return sendError(res, "Latitude must be -90 to 90 and longitude -180 to 180.");
        }

        std::lock_guard<std::mutex> guard(spotsLock);

        int id = report.saveReport(spots, std::stoi(reporter), latitude, longitude,
                                   photoPath.c_str(), wasteType.c_str(),
                                   (WasteSize)std::stoi(sizeValue));

        for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
            if (s->spotID == id)
                res.set_content(spotToJson(s).dump(), "application/json");
    });

    // Photo of a spot, wherever it was saved (uploads/ or a path typed in the terminal app)
    server.Get(R"(/api/photo/(\d+))", [](const httplib::Request &req, httplib::Response &res) {
        int id = std::stoi(req.matches[1]);

        for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
        {
            if (s->spotID != id) continue;

            std::ifstream file(s->photoPath, std::ios::binary);
            if (!file) break;

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            res.set_content(content, "image/jpeg");
            return;
        }

        res.status = 404;
    });

    server.set_mount_point("/", "frontend");

    // Render (or any host) tells us which port to use through the PORT variable
    int port = PORT;
    if (getenv("PORT"))
        port = atoi(getenv("PORT"));

    // 0.0.0.0 = accept visitors from outside this computer, not just localhost
    std::cout << "CleanLink running at http://localhost:" << port << std::endl;
    server.listen("0.0.0.0", port);

    return 0;
}
