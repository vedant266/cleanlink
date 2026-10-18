#include <iostream>
#include "report_spot.h"
#include "spot_list.h"
using namespace std;

void showMenu() {
    cout << "\n===== CLEANLINK =====\n";
    cout << "1. Report Waste Spot\n";
    cout << "2. View Waste Spots\n";
    cout << "3. View Campaigns\n";
    cout << "4. Donate\n";
    cout << "5. View Jobs\n";
    cout << "6. Exit\n";
    cout << "7. View City / Locality Tables\n";
    cout << "Enter choice: ";
}

int main() {
    int choice;
    ReportSpot report;
    SpotList spots;

    spots.loadFromFile();
    report.loadTables(spots);

    do {
        showMenu();
        cin >> choice;
        
        switch (choice) {
            case 1:
                
                report.reportSpot(spots);
                break;

            case 2:
                spots.display();
                break;

            case 3:
                cout << "View Campaigns\n";
                break;

            case 4:
                cout << "Donate\n";
                break;

            case 5:
                cout << "View Jobs\n";
                break;

            case 6:
                cout << "Exiting...\n";
                break;

            case 7:
                report.displayTables();
                break;

            default:
                cout << "Invalid choice.\n";
        }

    } while (choice != 6);

    return 0;
}