// Small local web server so CleanLink can be used from the browser.
// It uses the same SpotList / ReportSpot code as the terminal app.

#include "httplib.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>
#include <ctime>
#include <cstdlib>
#include <climits>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "report_spot.h"
#include "spot_list.h"
#include "photo_location.h"
#include "funding.h"
#include "verification.h"
#include "worker_assignment.h"

using json = nlohmann::json;

#define PORT 8080
#define UPLOAD_DIR "uploads"

SpotList spots;
ReportSpot report;
Funding funding;
Verification verification;
WorkerAssignment assignment;
std::mutex spotsLock; // one change to the spots at a time

json spotToJson(const WasteSpot *spot)
{
    const char *sizeNames[] = {"Small", "Medium", "Large"};
    const char *statusNames[] = {"Pending", "Verified", "Funded", "Assigned", "Completed", "Rejected"};

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
        {"moderatorID", spot->moderatorID},
        {"cleanupCost", spot->cleanupCost},
        {"fundedAmount", spot->fundedAmount},
        // Only how many applied and who was picked, never workers' locations
        {"applicants", WorkerAssignment::countApplicants(spot)},
        {"assignedWorkers", std::vector<int>(spot->assignedWorkers,
                                             spot->assignedWorkers + spot->assignedCount)},
        {"photo", spot->photoPath}
    };
}

// What a moderator sees about one applicant: no GPS, only how far away they are
json applicantToJson(const Applicant *a, bool assigned)
{
    return {
        {"workerID", a->getWorkerID()},
        {"distanceKm", a->getDistance()},
        {"rating", a->getWorkerRating()},
        {"appliedAt", a->getApplicationTime()},
        {"assigned", assigned}
    };
}

// Every applicant of a spot, best first (the priority queue's order)
json rankedApplicants(const WasteSpot *spot)
{
    Applicant *ranked[MAX_APPLICANTS];
    int count = WorkerAssignment::rankApplicants(spot, ranked);

    json list = json::array();

    for (int i = 0; i < count; i++)
    {
        bool assigned = false;
        for (int j = 0; j < spot->assignedCount; j++)
            if (spot->assignedWorkers[j] == ranked[i]->getWorkerID())
                assigned = true;

        list.push_back(applicantToJson(ranked[i], assigned));
    }

    return list;
}

// Latitude / longitude from the query string (?lat=..&lon=..). False if missing or not a number.
bool readLocation(const httplib::Request &req, double &latitude, double &longitude)
{
    try
    {
        latitude = std::stod(req.get_param_value("lat"));
        longitude = std::stod(req.get_param_value("lon"));
    }
    catch (const std::exception &)
    {
        return false;
    }

    return latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180;
}

void sendError(httplib::Response &res, const std::string &message)
{
    res.status = 400;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}

// Donate and verify send JSON like {"spotID": 1, "amount": 500}
bool readBody(const httplib::Request &req, json &body)
{
    body = json::parse(req.body, nullptr, false);  // false = no exception on bad JSON
    return !body.is_discarded() && body.is_object();
}

// IDs and amounts are whole numbers from 1 up. Anything else (missing, text,
// decimal, too big for an int) comes back as 0, which the checks then refuse.
int positiveField(const json &body, const char *key)
{
    if (!body.contains(key) || !body[key].is_number_integer())
        return 0;

    long long value = body[key].get<long long>();

    if (value < 1 || value > INT_MAX)
        return 0;

    return (int)value;
}

std::string textField(const json &body, const char *key)
{
    if (!body.contains(key) || !body[key].is_string())
        return "";

    return body[key].get<std::string>();
}

// A decimal number such as a latitude. False if missing or not a number.
bool numberField(const json &body, const char *key, double &value)
{
    if (!body.contains(key) || !body[key].is_number())
        return false;

    value = body[key].get<double>();
    return true;
}

// Same rule as the other routes: a failed save is the server's fault (500)
void sendWorkError(httplib::Response &res, WorkResult result)
{
    sendError(res, WorkerAssignment::resultMessage(result));
    if (result == WORK_SAVE_FAILED)
        res.status = 500;
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
    report.loadTables(spots);

    // Finds the next free donation ID, so IDs don't start again at 1
    funding.loadFromFile();

    // After the spots, so each application can find its spot
    assignment.loadFromFile(spots);

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

        if (id == 0)
        {
            remove(photoPath.c_str());
            sendError(res, "Could not save the report. Nothing was added.");
            res.status = 500;  // the server failed, not the form
            return;
        }

        for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
            if (s->spotID == id)
                res.set_content(spotToJson(s).dump(), "application/json");
    });

    // Moderator checks a pending report: {"moderatorID", "spotID", "action": "approve" | "reject",
    // "size": 0-2, and "city" + "locality" only when the spot's city came back "Unknown"}
    server.Post("/api/verify", [](const httplib::Request &req, httplib::Response &res) {
        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the check as JSON.");

        int moderatorID = positiveField(body, "moderatorID");
        int spotID = positiveField(body, "spotID");
        std::string action = textField(body, "action");

        if (moderatorID == 0)
            return sendError(res, "Enter your moderator ID.");

        std::lock_guard<std::mutex> guard(spotsLock);

        Moderator moderator(moderatorID);
        VerifyResult result;

        if (action == "reject")
        {
            result = verification.rejectSpot(spots, spotID, moderator);
        }
        else if (action == "approve")
        {
            // Check the size before saving anything, so a bad size can't leave
            // a spot with a new city but still pending
            if (!body.contains("size") || !body["size"].is_number_integer() ||
                body["size"].get<long long>() < SMALL || body["size"].get<long long>() > LARGE)
                return sendError(res, Verification::resultMessage(VERIFY_BAD_SIZE));

            WasteSize actualSize = (WasteSize)body["size"].get<int>();
            std::string city = textField(body, "city");
            std::string locality = textField(body, "locality");

            result = VERIFY_OK;

            // The moderator typed the real place from the map link
            if (!city.empty() || !locality.empty())
                result = verification.setLocation(spots, report.getCityTable(), spotID,
                                                  moderator, city, locality);

            if (result == VERIFY_OK)
                result = verification.approveSpot(spots, spotID, moderator, actualSize);
        }
        else
        {
            return sendError(res, "Action must be approve or reject.");
        }

        if (result != VERIFY_OK)
        {
            sendError(res, Verification::resultMessage(result));
            if (result == VERIFY_SAVE_FAILED)
                res.status = 500;
            return;
        }

        res.set_content(spotToJson(spots.findSpot(spotID)).dump(), "application/json");
    });

    // Donor gives money to a verified spot: {"donorID", "spotID", "amount"}
    server.Post("/api/donate", [](const httplib::Request &req, httplib::Response &res) {
        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the donation as JSON.");

        int donorID = positiveField(body, "donorID");
        int spotID = positiveField(body, "spotID");
        int amount = positiveField(body, "amount");

        if (donorID == 0)
            return sendError(res, "Enter your donor ID.");

        std::lock_guard<std::mutex> guard(spotsLock);

        WasteSpot *spot = spots.findSpot(spotID);
        if (spot == nullptr)
            return sendError(res, Funding::resultMessage(FUND_NO_SPOT));

        // fundSpot goes City -> Locality -> spot, like the terminal menu.
        // Old spots may have no locality saved; the city table files those under the city name.
        std::string locality = spot->localityName;
        if (locality.empty())
            locality = spot->cityName;

        FundResult result = funding.fundSpot(report.getCityTable(), spots, spot->cityName,
                                             locality, spotID, Donor(donorID), amount);

        if (result != FUND_OK)
        {
            sendError(res, Funding::resultMessage(result));
            if (result == FUND_SAVE_FAILED)
                res.status = 500;
            return;
        }

        res.set_content(spotToJson(spot).dump(), "application/json");
    });

    // Section 12: jobs for a worker at ?lat=..&lon=.. (and &workerID=.. to leave out
    // spots they already applied for), nearest first, within MAX_JOB_DISTANCE_KM
    server.Get("/api/jobs", [](const httplib::Request &req, httplib::Response &res) {
        double latitude, longitude;
        if (!readLocation(req, latitude, longitude))
            return sendError(res, WorkerAssignment::resultMessage(WORK_BAD_LOCATION));

        int workerID = 0;
        if (req.has_param("workerID"))
        {
            try { workerID = std::stoi(req.get_param_value("workerID")); }
            catch (const std::exception &) { workerID = 0; }
        }

        std::lock_guard<std::mutex> guard(spotsLock);

        json list = json::array();
        for (const SpotDistance &job : WorkerAssignment::findJobs(spots, latitude, longitude, workerID))
        {
            json item = spotToJson(job.spot);
            item["distanceKm"] = job.distance;
            list.push_back(item);
        }

        res.set_content(list.dump(), "application/json");
    });

    // Section 13: a worker applies {"workerID", "spotID", "latitude", "longitude"}
    server.Post("/api/apply", [](const httplib::Request &req, httplib::Response &res) {
        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the application as JSON.");

        int workerID = positiveField(body, "workerID");
        int spotID = positiveField(body, "spotID");
        double latitude, longitude;

        if (workerID == 0)
            return sendError(res, "Enter your worker ID.");

        if (!numberField(body, "latitude", latitude) || !numberField(body, "longitude", longitude))
            return sendError(res, "Share your location or type your latitude and longitude.");

        std::lock_guard<std::mutex> guard(spotsLock);

        WasteSpot *spot = spots.findSpot(spotID);
        if (spot == nullptr)
            return sendWorkError(res, WORK_NO_SPOT);

        // applyForSpot goes City -> Locality -> spot, like the terminal menu.
        // Old spots may have no locality saved; the city table files those under the city name.
        std::string locality = spot->localityName;
        if (locality.empty())
            locality = spot->cityName;

        Worker worker(workerID, latitude, longitude, DEFAULT_WORKER_RATING);
        WorkResult result = assignment.applyForSpot(report.getCityTable(), spot->cityName,
                                                    locality, spotID, worker);

        if (result != WORK_OK)
            return sendWorkError(res, result);

        json reply = spotToJson(spot);
        reply["distanceKm"] = spot->applicantHead->getDistance();  // the new applicant is at the front
        res.set_content(reply.dump(), "application/json");
    });

    // Applicants of a spot for the moderator, best first: ?spotID=..
    server.Get("/api/applicants", [](const httplib::Request &req, httplib::Response &res) {
        int spotID = 0;
        try { spotID = std::stoi(req.get_param_value("spotID")); }
        catch (const std::exception &) { spotID = 0; }

        std::lock_guard<std::mutex> guard(spotsLock);

        WasteSpot *spot = spots.findSpot(spotID);
        if (spot == nullptr)
            return sendWorkError(res, WORK_NO_SPOT);

        res.set_content(rankedApplicants(spot).dump(), "application/json");
    });

    // Section 14: the priority queue picks the best applicants {"spotID"}
    server.Post("/api/assign", [](const httplib::Request &req, httplib::Response &res) {
        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the spot as JSON.");

        int spotID = positiveField(body, "spotID");

        std::lock_guard<std::mutex> guard(spotsLock);

        WorkResult result = assignment.assignWorkers(spots, spotID);

        if (result != WORK_OK)
            return sendWorkError(res, result);

        WasteSpot *spot = spots.findSpot(spotID);
        json reply = spotToJson(spot);
        reply["ranked"] = rankedApplicants(spot);
        res.set_content(reply.dump(), "application/json");
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
