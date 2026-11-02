#include "donation.h"

#include <iostream>
#include <cstdio>
#include <ctime>

using namespace std;

DonationLog::DonationLog()
{
    nextDonationID = 1;
}

void DonationLog::loadFromFile()
{
    FILE *fp = fopen(DONATION_FILE, "rb");

    // No file yet means nobody has donated
    if (fp == NULL)
    {
        return;
    }

    Donation record;
    int count = 0;

    while (fread(&record, sizeof(Donation), 1, fp) == 1)
    {
        if (record.donationID >= nextDonationID)
        {
            nextDonationID = record.donationID + 1;
        }

        count++;
    }

    fclose(fp);

    cout << "Loaded " << count
         << " donation(s) from " << DONATION_FILE << endl;
}

bool DonationLog::addDonation(int spotID, int donorID, int amount)
{
    FILE *fp = fopen(DONATION_FILE, "ab");

    if (fp == NULL)
    {
        cout << "Error opening " << DONATION_FILE << endl;
        return false;
    }

    Donation record;

    record.donationID = nextDonationID;
    record.spotID = spotID;
    record.donorID = donorID;
    record.amount = amount;
    record.time = time(nullptr);

    bool saved = fwrite(&record, sizeof(Donation), 1, fp) == 1;

    fclose(fp);

    if (saved)
    {
        nextDonationID++;
    }

    return saved;
}

void DonationLog::display()
{
    cout << "\n===== DONATIONS =====\n";

    FILE *fp = fopen(DONATION_FILE, "rb");

    if (fp == NULL)
    {
        cout << "No donations yet.\n";
        return;
    }

    Donation record;
    int count = 0;
    int total = 0;

    while (fread(&record, sizeof(Donation), 1, fp) == 1)
    {
        char when[20];
        time_t t = (time_t)record.time;
        strftime(when, sizeof(when), "%d-%m-%Y %H:%M", localtime(&t));

        cout << "#" << record.donationID
             << " | " << when
             << " | Donor " << record.donorID
             << " -> Spot #" << record.spotID
             << " | Rs " << record.amount << endl;

        count++;
        total += record.amount;
    }

    fclose(fp);

    if (count == 0)
    {
        cout << "No donations yet.\n";
        return;
    }

    cout << "------------------\n";
    cout << "Total donated: Rs " << total << endl;
}
