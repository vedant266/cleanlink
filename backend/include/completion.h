#ifndef COMPLETION_H
#define COMPLETION_H

#include <string>
#include <vector>
#include "spot_list.h"
#include "user.h"

using namespace std;

// Path is relative to the repo root, like SPOT_FILE
#define PROOF_FILE "backend/data/proofs.dat"

// What a moderator decided about a proof
typedef enum {
    PROOF_WAITING,
    PROOF_APPROVED,
    PROOF_REJECTED
} ProofStatus;

// Section 18: one "the work is done" submission from an assigned worker.
// The report photo is the "before" picture; this holds the "after".
// Kept in its own file so WasteSpot (and waste_spots.dat) keep their size.
typedef struct
{
    int spotID;
    int workerID;
    char photoPath[200];
    char note[200];
    long long submittedAt;

    // Section 19: filled in when a moderator checks it
    ProofStatus status;
    int moderatorID;     // 0 until checked
    char reason[200];    // why it was rejected, so the worker knows what to fix
    long long checkedAt;
} ProofRecord;

// What happened when a worker sent a proof, or a moderator checked one
typedef enum {
    DONE_OK,
    DONE_NO_SPOT,
    DONE_BAD_WORKER,
    DONE_NOT_ASSIGNED,
    DONE_NOT_YOUR_JOB,
    DONE_NO_PHOTO,
    DONE_ALREADY_SENT,
    DONE_FINISHED,
    DONE_BAD_MODERATOR,
    DONE_NOTHING_TO_CHECK,
    DONE_OWN_WORK,
    DONE_NO_REASON,
    DONE_SAVE_FAILED
} DoneResult;

class TaskCompletion
{
private:
    // Every proof ever sent, in file order (record i is at position i in the file)
    vector<ProofRecord> proofs;

    // False when proofs.dat could not be read, so nothing is written into it
    bool canSave;

    bool appendProof(const ProofRecord &record);
    bool rewriteProof(int index);

    // Index of the newest proof for this spot, or -1
    int latestIndex(int spotID) const;

    // Shared checks before a moderator approves or rejects
    DoneResult canCheck(SpotList &spots, int spotID, const Moderator &moderator,
                        WasteSpot *&spot, int &index);

    // Save the spot's new status and the moderator's decision, or neither
    DoneResult saveCheck(SpotList &spots, WasteSpot *spot, int index,
                         SpotStatus newStatus, const ProofRecord &decided);

public:
    TaskCompletion();

    void loadFromFile();

    // Section 18: an assigned worker says the cleanup is done, with an "after" photo.
    // The spot becomes Submitted and waits for a moderator.
    DoneResult submitProof(SpotList &spots, int spotID, int workerID,
                           const string &photoPath, const string &note);

    // Section 19 + 21: the moderator compares before/after. Approve -> Completed.
    DoneResult approve(SpotList &spots, int spotID, const Moderator &moderator);

    // Reject -> back to Assigned, with a reason, so the worker can send a new photo
    DoneResult reject(SpotList &spots, int spotID, const Moderator &moderator,
                      const string &reason);

    // Newest proof for the spot, or nullptr
    const ProofRecord *latestProof(int spotID) const;

    static const char *proofStatusName(ProofStatus status);
    static const char *resultMessage(DoneResult result);
};

#endif
