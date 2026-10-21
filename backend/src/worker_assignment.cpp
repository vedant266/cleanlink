#include "worker_assignment.h"
#include "priority_queue.h"

#include <iostream>
#include <cmath>

using namespace std;

const double PI = 3.14159265358979323846;

double toRadians(double degree)
{
    return degree * PI / 180.0;
}

double calculateDistance(
    double lat1,
    double lon1,
    double lat2,
    double lon2
)
{
    const double EARTH_RADIUS = 6371.0;

    double dLat = toRadians(lat2 - lat1);
    double dLon = toRadians(lon2 - lon1);

    double a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(toRadians(lat1)) *
        cos(toRadians(lat2)) *
        sin(dLon / 2) *
        sin(dLon / 2);

    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return EARTH_RADIUS * c;
}


void applyForSpot(
    WasteSpot *spot,
    const Worker &worker
)
{
    if (spot == nullptr)
    {
        return;
    }

    // Calculate distance between worker and waste spot
    double distance = calculateDistance(
        spot->latitude,
        spot->longitude,
        worker.getLatitude(),
        worker.getLongitude()
    );

    // Simple increasing application time
    static long long applicationCounter = 1;

    Applicant *newApplicant = new Applicant(
        worker.getID(),
        worker.getLatitude(),
        worker.getLongitude(),
        worker.getRating(),
        distance,
        applicationCounter++
    );

    // Insert at beginning of linked list
    newApplicant->setNext(spot->applicantHead);

    spot->applicantHead = newApplicant;

    cout << "Worker " << worker.getID()
         << " applied for Spot " << spot->spotID << endl;

    cout << "Distance: "
         << newApplicant->getDistance()
         << " km" << endl;
}


void displayApplicants(WasteSpot *spot)
{
    if (spot == nullptr)
    {
        return;
    }

    Applicant *current = spot->applicantHead;

    cout << "\n--- Applicant Linked List ---\n";

    if (current == nullptr)
    {
        cout << "No applicants.\n";
        return;
    }

    while (current != nullptr)
    {
        cout << "Worker ID: "
             << current->getWorkerID() << endl;

        cout << "Distance: "
             << current->getDistance()
             << " km" << endl;

        cout << "Rating: "
             << current->getWorkerRating() << endl;

        cout << "Application Time: "
             << current->getApplicationTime() << endl;

        cout << "------------------\n";

        current = current->getNext();
    }
}


void assignWorkers(WasteSpot *spot)
{
    if (spot == nullptr)
    {
        return;
    }

    PriorityQueue pq;

    Applicant *current = spot->applicantHead;

    // Put all applicants into priority queue
    while (current != nullptr)
    {
        pq.push(current);

        current = current->getNext();
    }

    spot->assignedCount = 0;

    cout << "\n--- Priority Queue Assignment ---\n";

    // Select workers according to priority
    while (!pq.isEmpty() &&
           spot->assignedCount < spot->workerLimit &&
           spot->assignedCount < MAX_ASSIGNED_WORKERS)
    {
        Applicant *selected = pq.pop();

        spot->assignedWorkers[spot->assignedCount] =
            selected->getWorkerID();

        cout << "Selected Worker: "
             << selected->getWorkerID() << endl;

        cout << "Distance: "
             << selected->getDistance()
             << " km" << endl;

        cout << "Rating: "
             << selected->getWorkerRating() << endl;

        cout << "------------------\n";

        spot->assignedCount++;
    }

    if (spot->assignedCount > 0)
    {
        spot->status = ASSIGNED;
    }
}


void displayAssignedWorkers(WasteSpot *spot)
{
    if (spot == nullptr)
    {
        return;
    }

    cout << "\n--- Assigned Workers ---\n";

    for (int i = 0; i < spot->assignedCount; i++)
    {
        cout << "Worker "
             << spot->assignedWorkers[i]
             << endl;
    }
}