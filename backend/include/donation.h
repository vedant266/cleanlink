#ifndef DONATION_H
#define DONATION_H

// Path is relative to the repo root, like SPOT_FILE
#define DONATION_FILE "backend/data/donations.dat"

// One donation. Records are only ever added to the file, never changed,
// so the file is a full history of where every rupee came from.
typedef struct
{
    int donationID;
    int spotID;
    int donorID;
    int amount;      // whole rupees
    long long time;
} Donation;

class DonationLog
{
private:
    int nextDonationID;

public:
    DonationLog();

    // Find the next free donation ID from the records already saved
    void loadFromFile();

    // Append one donation record. Returns false if the file can't be written.
    bool addDonation(int spotID, int donorID, int amount);

    void display();
};

#endif
