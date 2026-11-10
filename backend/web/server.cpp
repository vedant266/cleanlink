// Small local web server so CleanLink can be used from the browser.
// It uses the same SpotList / ReportSpot code as the terminal app.

#include "httplib.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>
#include <atomic>
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
#include "completion.h"
#include "user_table.h"
#include "chained_hash.h"
#include "password.h"

using json = nlohmann::json;

#define PORT 8080
#define UPLOAD_DIR "uploads"

SpotList spots;
ReportSpot report;
Funding funding;
Verification verification;
WorkerAssignment assignment;
TaskCompletion completion;
std::mutex spotsLock; // one change to the spots at a time

UserTable users;
// Logged-in browsers: cookie token -> their user. Kept in memory only, so a
// restart logs everyone out.
// shortcut: sessions never expire on the server, only on logout or restart.
ChainedHashTable<User *> sessions;
std::mutex usersLock; // guards users and sessions

json spotToJson(const WasteSpot *spot)
{
    const char *sizeNames[] = {"Small", "Medium", "Large"};

    json j = {
        {"id", spot->spotID},
        {"reporterID", spot->reporterID},
        {"latitude", spot->latitude},
        {"longitude", spot->longitude},
        {"city", spot->cityName},
        {"locality", spot->localityName},
        {"wasteType", spot->wasteType},
        {"size", sizeNames[spot->estimatedSize]},
        {"workersNeeded", spot->workerLimit},
        {"status", statusName(spot->status)},
        {"moderatorID", spot->moderatorID},
        {"cleanupCost", spot->cleanupCost},
        {"fundedAmount", spot->fundedAmount},
        // Only how many applied and who was picked, never workers' locations
        {"applicants", WorkerAssignment::countApplicants(spot)},
        {"assignedWorkers", std::vector<int>(spot->assignedWorkers,
                                             spot->assignedWorkers + spot->assignedCount)},
        {"photo", spot->photoPath}
    };

    // Sections 18-19: the newest "work is done" proof, if there is one
    if (const ProofRecord *p = completion.latestProof(spot->spotID))
    {
        j["proof"] = {
            {"workerID", p->workerID},
            {"note", p->note},
            {"submittedAt", p->submittedAt},
            {"status", TaskCompletion::proofStatusName(p->status)},
            {"moderatorID", p->moderatorID},
            {"reason", p->reason},
            {"checkedAt", p->checkedAt}
        };
    }

    return j;
}

// Uploaded photos keep only a known image extension, nothing else from the file name
std::string imageExtension(const std::string &filename)
{
    std::string ext;
    size_t dot = filename.rfind('.');
    if (dot != std::string::npos)
        ext = filename.substr(dot);

    for (char &c : ext)
        c = (char)tolower((unsigned char)c);

    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".heic")
        return ext;

    return ".jpg";
}

// A path in uploads/ that no other upload uses. The time alone isn't enough: two
// uploads in the same second would get the same name, and one would overwrite
// (or, when a request is refused, delete) the other's photo.
std::string uniqueUploadPath(const std::string &prefix, const std::string &ext)
{
    static std::atomic<int> counter(0);

    std::string base = std::string(UPLOAD_DIR) + "/" + prefix + std::to_string(time(nullptr))
                     + "_" + std::to_string(++counter);
    std::string path = base + ext;

    // Also skip names left from an earlier run of the server
    for (int n = 2; access(path.c_str(), F_OK) == 0; n++)
        path = base + "_" + std::to_string(n) + ext;

    return path;
}

// Send an image file from disk, or 404 if it isn't there
void sendImage(httplib::Response &res, const std::string &path)
{
    std::ifstream file(path, std::ios::binary);

    if (path.empty() || !file)
    {
        res.status = 404;
        return;
    }

    const char *type = "image/jpeg";
    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".png") == 0) type = "image/png";
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".webp") == 0) type = "image/webp";
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".heic") == 0) type = "image/heic";

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    res.set_content(content, type);
}

bool isAssignedTo(const WasteSpot *spot, int workerID)
{
    for (int i = 0; i < spot->assignedCount; i++)
        if (spot->assignedWorkers[i] == workerID)
            return true;

    return false;
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

json userToJson(const User *user)
{
    return {{"id", user->getID()}, {"username", user->getUsername()}, {"role", user->roleName()}};
}

// The "session" cookie's value, or "". Cookies come as "a=1; session=abc; b=2".
std::string sessionToken(const httplib::Request &req)
{
    std::string cookies = req.get_header_value("Cookie");
    size_t start = 0;

    while (start < cookies.size())
    {
        size_t end = cookies.find(';', start);
        if (end == std::string::npos)
            end = cookies.size();

        size_t name = cookies.find_first_not_of(' ', start);
        if (name < end && cookies.compare(name, 8, "session=") == 0)
            return cookies.substr(name + 8, end - name - 8);

        start = end + 1;
    }
    return "";
}

// Who is logged in on this browser, or nullptr.
// Users are never deleted while the server runs, so the pointer stays valid.
User *currentUser(const httplib::Request &req)
{
    std::lock_guard<std::mutex> guard(usersLock);
    User **user = sessions.find(sessionToken(req));
    return user ? *user : nullptr;
}

// The logged-in user as a T (Reporter, Donor, Worker, Moderator). dynamic_cast
// answers "is this user a T?", so a Citizen passes as both Reporter and Donor.
// On nullptr the 401 / 403 error has already been sent.
template <typename T>
T *loggedInAs(const httplib::Request &req, httplib::Response &res, const char *wrongRole)
{
    User *user = currentUser(req);
    if (user == nullptr)
    {
        sendError(res, "Log in first.");
        res.status = 401;
        return nullptr;
    }

    T *asRole = dynamic_cast<T *>(user);
    if (asRole == nullptr)
    {
        sendError(res, wrongRole);
        res.status = 403;
    }
    return asRole;
}

// Call with usersLock held
void startSession(const httplib::Request &req, httplib::Response &res, User *user)
{
    std::string token = randomHex(32);
    sessions.insert(token, user);

    // HttpOnly: page scripts can't read it. SameSite=Lax: other sites can't post with it.
    std::string cookie = "session=" + token + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800";
    // Render puts HTTPS in front of us; then the cookie must never go over plain HTTP
    if (req.get_header_value("X-Forwarded-Proto") == "https")
        cookie += "; Secure";
    res.set_header("Set-Cookie", cookie);
}

// Same rule as the other routes: a failed save is the server's fault (500)
void sendWorkError(httplib::Response &res, WorkResult result)
{
    sendError(res, WorkerAssignment::resultMessage(result));
    if (result == WORK_SAVE_FAILED)
        res.status = 500;
}

void sendDoneError(httplib::Response &res, DoneResult result)
{
    sendError(res, TaskCompletion::resultMessage(result));
    if (result == DONE_SAVE_FAILED)
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

    // "Work is done" proofs and the moderators' decisions
    completion.loadFromFile();

    users.loadFromFile();

    httplib::Server server;

    // List all reported spots
    server.Get("/api/spots", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> guard(spotsLock);

        json list = json::array();
        for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
            list.push_back(spotToJson(s));

        res.set_content(list.dump(), "application/json");
    });

    // Make an account and log in: {"username", "password", "role": "user" | "worker"}
    server.Post("/api/signup", [](const httplib::Request &req, httplib::Response &res) {
        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the account as JSON.");

        // Anyone could pick moderator here, so moderators are made in the terminal app
        std::string role = textField(body, "role");
        if (role != "user" && role != "worker")
            return sendError(res, UserTable::resultMessage(USER_BAD_ROLE));

        std::lock_guard<std::mutex> guard(usersLock);

        User *user;
        UserResult result = users.signUp(textField(body, "username"), textField(body, "password"),
                                         role, user);
        if (result != USER_OK)
        {
            sendError(res, UserTable::resultMessage(result));
            if (result == USER_SAVE_FAILED)
                res.status = 500;
            return;
        }

        startSession(req, res, user);
        res.set_content(userToJson(user).dump(), "application/json");
    });

    // {"username", "password"}
    server.Post("/api/login", [](const httplib::Request &req, httplib::Response &res) {
        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the login as JSON.");

        std::lock_guard<std::mutex> guard(usersLock);

        User *user = users.logIn(textField(body, "username"), textField(body, "password"));
        if (user == nullptr)
        {
            sendError(res, "Wrong username or password.");
            res.status = 401;
            return;
        }

        startSession(req, res, user);
        res.set_content(userToJson(user).dump(), "application/json");
    });

    server.Post("/api/logout", [](const httplib::Request &req, httplib::Response &res) {
        {
            std::lock_guard<std::mutex> guard(usersLock);
            sessions.remove(sessionToken(req));
        }
        res.set_header("Set-Cookie", "session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
        res.set_content("{}", "application/json");
    });

    // Who is logged in: the user, or null
    server.Get("/api/me", [](const httplib::Request &req, httplib::Response &res) {
        User *user = currentUser(req);
        res.set_content(user ? userToJson(user).dump() : "null", "application/json");
    });

    // Report a new spot (form with an uploaded photo). The reporter is the logged-in user.
    server.Post("/api/report", [](const httplib::Request &req, httplib::Response &res) {
        Reporter *reporter = loggedInAs<Reporter>(req, res, "Only user accounts can report spots.");
        if (reporter == nullptr)
            return;

        if (!req.has_file("photo") || req.get_file_value("photo").content.empty())
            return sendError(res, "Upload a photo of the spot.");

        std::string wasteType = req.get_file_value("wasteType").content;
        std::string sizeValue = req.get_file_value("size").content;

        if (wasteType.empty() || sizeValue.empty())
            return sendError(res, "Fill in waste type and size.");

        // Save the uploaded photo into uploads/
        auto photo = req.get_file_value("photo");
        std::string photoPath = uniqueUploadPath("spot_", imageExtension(photo.filename));
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

        int id = report.saveReport(spots, reporter->getID(), latitude, longitude,
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

    // The logged-in moderator checks a pending report: {"spotID", "action": "approve" | "reject",
    // "size": 0-2, and "city" + "locality" only when the spot's city came back "Unknown"}
    server.Post("/api/verify", [](const httplib::Request &req, httplib::Response &res) {
        Moderator *loggedIn = loggedInAs<Moderator>(req, res, "Only moderator accounts can check reports.");
        if (loggedIn == nullptr)
            return;

        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the check as JSON.");

        int spotID = positiveField(body, "spotID");
        std::string action = textField(body, "action");

        std::lock_guard<std::mutex> guard(spotsLock);

        const Moderator &moderator = *loggedIn;
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

    // The logged-in donor gives money to a verified spot: {"spotID", "amount"}
    server.Post("/api/donate", [](const httplib::Request &req, httplib::Response &res) {
        Donor *donor = loggedInAs<Donor>(req, res, "Only user accounts can donate.");
        if (donor == nullptr)
            return;

        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the donation as JSON.");

        int spotID = positiveField(body, "spotID");
        int amount = positiveField(body, "amount");

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
                                             locality, spotID, *donor, amount);

        if (result != FUND_OK)
        {
            sendError(res, Funding::resultMessage(result));
            if (result == FUND_SAVE_FAILED)
                res.status = 500;
            return;
        }

        res.set_content(spotToJson(spot).dump(), "application/json");
    });

    // Section 12: jobs near ?lat=..&lon=.., nearest first, within MAX_JOB_DISTANCE_KM.
    // A logged-in worker doesn't see the spots they already applied for.
    server.Get("/api/jobs", [](const httplib::Request &req, httplib::Response &res) {
        double latitude, longitude;
        if (!readLocation(req, latitude, longitude))
            return sendError(res, WorkerAssignment::resultMessage(WORK_BAD_LOCATION));

        Worker *worker = dynamic_cast<Worker *>(currentUser(req));
        int workerID = worker ? worker->getID() : 0;

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

    // Section 13: the logged-in worker applies {"spotID", "latitude", "longitude"}
    server.Post("/api/apply", [](const httplib::Request &req, httplib::Response &res) {
        Worker *account = loggedInAs<Worker>(req, res, "Only worker accounts can apply for jobs.");
        if (account == nullptr)
            return;

        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the application as JSON.");

        int workerID = account->getID();
        int spotID = positiveField(body, "spotID");
        double latitude, longitude;

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
        // Each applicant's distance hints where they live, so only moderators see it
        if (loggedInAs<Moderator>(req, res, "Only moderator accounts can see applicants.") == nullptr)
            return;

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
        if (loggedInAs<Moderator>(req, res, "Only moderator accounts can assign workers.") == nullptr)
            return;

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
        std::string path;
        {
            std::lock_guard<std::mutex> guard(spotsLock);
            WasteSpot *spot = spots.findSpot(id);
            if (spot != nullptr)
                path = spot->photoPath;
        }
        sendImage(res, path);
    });

    // The newest "after" photo a worker sent for a spot
    server.Get(R"(/api/proof-photo/(\d+))", [](const httplib::Request &req, httplib::Response &res) {
        int id = std::stoi(req.matches[1]);
        std::string path;
        {
            std::lock_guard<std::mutex> guard(spotsLock);
            if (const ProofRecord *p = completion.latestProof(id))
                path = p->photoPath;
        }
        sendImage(res, path);
    });

    // Section 18: the logged-in, assigned worker sends the "after" photo (form: spotID, note, photo)
    server.Post("/api/proof", [](const httplib::Request &req, httplib::Response &res) {
        Worker *worker = loggedInAs<Worker>(req, res, "Only worker accounts can send work photos.");
        if (worker == nullptr)
            return;

        if (!req.has_file("photo") || req.get_file_value("photo").content.empty())
            return sendDoneError(res, DONE_NO_PHOTO);

        int workerID = worker->getID(), spotID = 0;
        try
        {
            spotID = std::stoi(req.get_file_value("spotID").content);
        }
        catch (const std::exception &) {}

        std::string note = req.get_file_value("note").content;
        auto photo = req.get_file_value("photo");
        std::string photoPath = uniqueUploadPath("proof_" + std::to_string(spotID) + "_",
                                                 imageExtension(photo.filename));

        std::lock_guard<std::mutex> guard(spotsLock);

        std::ofstream out(photoPath, std::ios::binary);
        out << photo.content;
        out.close();
        if (!out)
        {
            remove(photoPath.c_str());
            sendError(res, "Could not save the photo. Try again.");
            res.status = 500;
            return;
        }

        DoneResult result = completion.submitProof(spots, spotID, workerID, photoPath, note);

        if (result != DONE_OK)
        {
            remove(photoPath.c_str());
            return sendDoneError(res, result);
        }

        res.set_content(spotToJson(spots.findSpot(spotID)).dump(), "application/json");
    });

    // Sections 19 + 21: a moderator compares before/after
    // {"spotID", "action": "approve" | "reject", "reason"}, by the logged-in moderator
    server.Post("/api/check", [](const httplib::Request &req, httplib::Response &res) {
        Moderator *loggedIn = loggedInAs<Moderator>(req, res, "Only moderator accounts can check work.");
        if (loggedIn == nullptr)
            return;

        json body;
        if (!readBody(req, body))
            return sendError(res, "Send the check as JSON.");

        const Moderator &moderator = *loggedIn;
        int spotID = positiveField(body, "spotID");
        std::string action = textField(body, "action");

        std::lock_guard<std::mutex> guard(spotsLock);

        DoneResult result;
        if (action == "approve")
            result = completion.approve(spots, spotID, moderator);
        else if (action == "reject")
            result = completion.reject(spots, spotID, moderator, textField(body, "reason"));
        else
            return sendError(res, "Action must be approve or reject.");

        if (result != DONE_OK)
            return sendDoneError(res, result);

        res.set_content(spotToJson(spots.findSpot(spotID)).dump(), "application/json");
    });

    // The logged-in worker's own jobs. Each comes with "myState":
    // applied, not-picked, todo, redo (a moderator rejected the photo), checking, done
    server.Get("/api/myjobs", [](const httplib::Request &req, httplib::Response &res) {
        Worker *worker = loggedInAs<Worker>(req, res, "Only worker accounts have jobs.");
        if (worker == nullptr)
            return;

        int workerID = worker->getID();

        std::lock_guard<std::mutex> guard(spotsLock);

        json list = json::array();
        for (WasteSpot *s = spots.getHead(); s != nullptr; s = s->next)
        {
            bool assigned = isAssignedTo(s, workerID);
            if (!assigned && !WorkerAssignment::hasApplied(s, workerID))
                continue;

            std::string state;
            if (!assigned)
                state = s->status == FUNDED ? "applied" : "not-picked";
            else if (s->status == COMPLETED)
                state = "done";
            else if (s->status == SUBMITTED)
                state = "checking";
            else
            {
                const ProofRecord *p = completion.latestProof(s->spotID);
                state = (p != nullptr && p->status == PROOF_REJECTED) ? "redo" : "todo";
            }

            json item = spotToJson(s);
            item["myState"] = state;
            list.push_back(item);
        }

        res.set_content(list.dump(), "application/json");
    });

    server.set_mount_point("/", "frontend");

    // Render (or any host) tells us which port to use through the PORT variable
    int port = PORT;
    if (getenv("PORT"))
        port = atoi(getenv("PORT"));

    // The library's default (SO_REUSEPORT) lets a second program share the port, so
    // starting web.command twice quietly ran two servers, each with its own copy of
    // the spots in memory. SO_REUSEADDR only allows a quick restart, not two at once.
    server.set_socket_options([](socket_t sock) {
        int on = 1;
#ifdef _WIN32
        setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&on), sizeof(on));
#else
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif
    });

    // 0.0.0.0 = accept visitors from outside this computer, not just localhost
    if (!server.bind_to_port("0.0.0.0", port))
    {
        std::cerr << "Port " << port << " is already in use. Is CleanLink already running?\n"
                  << "Close the other CleanLink window first, then start it again.\n";
        return 1;
    }

    std::cout << "CleanLink running at http://localhost:" << port << std::endl;
    server.listen_after_bind();

    return 0;
}
