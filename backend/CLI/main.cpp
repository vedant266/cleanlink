#include <iostream>
#include <limits>
#include "report_spot.h"
#include "spot_list.h"
#include "funding.h"
#include "verification.h"
#include "worker_assignment.h"
using namespace std;

void showMenu() {
    cout << "\n===== CLEANLINK =====\n";
    cout << "1. Report Waste Spot\n";
    cout << "2. View Waste Spots\n";
    cout << "3. View Campaigns\n";
    cout << "4. Donate\n";
    cout << "5. Apply for a Job (Worker)\n";
    cout << "6. Exit\n";
    cout << "7. View City / Locality Tables\n";
    cout << "8. View Donations\n";
    cout << "9. Verify Spot (Moderator)\n";
    cout << "10. Assign Workers (Moderator)\n";
    cout << "Enter choice: ";
}

int main() {
    int choice;
    ReportSpot report;
    SpotList spots;
    Funding funding;
    Verification verification;
    WorkerAssignment assignment;

    spots.loadFromFile();
    report.loadTables(spots);
    funding.loadFromFile();
    // After the spots, so each application can find its spot
    assignment.loadFromFile(spots);

    do {
        showMenu();

        if (!(cin >> choice)) {
            // Input closed: stop instead of printing the menu forever
            if (cin.eof())
                break;

            // Letters instead of a number: throw the line away and ask again
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            choice = 0;
        }

        switch (choice) {
            case 1:

                report.reportSpot(spots);
                break;

            case 2:
                spots.display();
                break;

            case 3:
                funding.displayCampaigns(spots);
                break;

            case 4:
                funding.fundSpotMenu(report.getCityTable(), spots);
                break;

            case 5:
                assignment.applyMenu(report.getCityTable(), spots);
                break;

            case 6:
                cout << "Exiting...\n";
                break;

            case 7:
                report.displayTables();
                break;

            case 8:
                funding.displayDonations();
                break;

            case 9:
                verification.verifySpotMenu(spots, report.getCityTable());
                break;

            case 10:
                assignment.assignMenu(spots);
                break;

            default:
                cout << "Invalid choice.\n";
        }

    } while (choice != 6);

    return 0;
}
