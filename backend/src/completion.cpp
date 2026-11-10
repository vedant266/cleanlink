#include "completion.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <ctime>

using namespace std;

TaskCompletion::TaskCompletion()
{
    canSave = true;
}

static bool isAssigned(const WasteSpot *spot, int workerID)
{
    for (int i = 0; i < spot->assignedCount; i++)
    {
        if (spot->assignedWorkers[i] == workerID)
            return true;
    }

    return false;
}

// Same idea as the spot file: text must end inside its array, status in range
static bool isValidProof(const ProofRecord &p)
{
    return memchr(p.photoPath, '\0', sizeof(p.photoPath)) != NULL &&
           memchr(p.note, '\0', sizeof(p.note)) != NULL &&
           memchr(p.reason, '\0', sizeof(p.reason)) != NULL &&
           p.spotID >= 1 && p.workerID >= 1 &&
           p.status >= PROOF_WAITING && p.status <= PROOF_REJECTED;
}

void TaskCompletion::loadFromFile()
{
    FILE *fp = fopen(PROOF_FILE, "rb");

    // No file yet means no work has been sent in
    if (fp == NULL)
        return;

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);

    if (fileSize % sizeof(ProofRecord) != 0)
    {
        cout << PROOF_FILE << " was saved by an older version of CleanLink.\n"
             << "Delete or rename it to start fresh.\n"
             << "Until then, finished work will NOT be saved.\n";
        canSave = false;
        fclose(fp);
        return;
    }

    ProofRecord record;

    while (fread(&record, sizeof(ProofRecord), 1, fp) == 1)
    {
        // Records are found by their position, so a damaged one can't just be
        // skipped: treat the whole file as unusable
        if (!isValidProof(record))
        {
            cout << PROOF_FILE << " has a damaged record.\n"
                 << "Until it is fixed, finished work will NOT be saved.\n";
            proofs.clear();
            canSave = false;
            fclose(fp);
            return;
        }

        proofs.push_back(record);
    }

    fclose(fp);

    cout << "Loaded " << proofs.size() << " work proof(s) from " << PROOF_FILE << endl;
}

bool TaskCompletion::appendProof(const ProofRecord &record)
{
    if (!canSave)
        return false;

    FILE *fp = fopen(PROOF_FILE, "ab");

    if (fp == NULL)
    {
        cout << "Error opening " << PROOF_FILE << endl;
        return false;
    }

    bool saved = fwrite(&record, sizeof(ProofRecord), 1, fp) == 1;

    if (fclose(fp) != 0)
        saved = false;

    return saved;
}

// Record i is at position i, so jump straight to it and write over it
bool TaskCompletion::rewriteProof(int index)
{
    if (!canSave)
        return false;

    FILE *fp = fopen(PROOF_FILE, "r+b");

    if (fp == NULL)
    {
        cout << "Error opening " << PROOF_FILE << endl;
        return false;
    }

    bool saved = fseek(fp, (long)index * (long)sizeof(ProofRecord), SEEK_SET) == 0 &&
                 fwrite(&proofs[index], sizeof(ProofRecord), 1, fp) == 1;

    if (fclose(fp) != 0)
        saved = false;

    return saved;
}

int TaskCompletion::latestIndex(int spotID) const
{
    for (int i = (int)proofs.size() - 1; i >= 0; i--)
    {
        if (proofs[i].spotID == spotID)
            return i;
    }

    return -1;
}

const ProofRecord *TaskCompletion::latestProof(int spotID) const
{
    int index = latestIndex(spotID);
    return index == -1 ? nullptr : &proofs[index];
}

DoneResult TaskCompletion::submitProof(SpotList &spots, int spotID, int workerID,
                                       const string &photoPath, const string &note)
{
    if (workerID < 1)
        return DONE_BAD_WORKER;

    WasteSpot *spot = spots.findSpot(spotID);

    if (spot == nullptr)
        return DONE_NO_SPOT;

    if (spot->status == SUBMITTED)
        return DONE_ALREADY_SENT;

    if (spot->status == COMPLETED)
        return DONE_FINISHED;

    // Only a spot with its workers picked can be finished
    if (spot->status != ASSIGNED)
        return DONE_NOT_ASSIGNED;

    if (!isAssigned(spot, workerID))
        return DONE_NOT_YOUR_JOB;

    if (photoPath.empty())
        return DONE_NO_PHOTO;

    ProofRecord record = {};
    record.spotID = spotID;
    record.workerID = workerID;
    strncpy(record.photoPath, photoPath.c_str(), sizeof(record.photoPath) - 1);
    strncpy(record.note, note.c_str(), sizeof(record.note) - 1);
    record.submittedAt = time(nullptr);
    record.status = PROOF_WAITING;

    // The proof goes in first. If the spot can't be saved after that, the proof
    // is left over but harmless: the spot is still Assigned, the worker can
    // send again, and only the newest proof for a spot is ever used.
    if (!appendProof(record))
        return DONE_SAVE_FAILED;

    proofs.push_back(record);

    WasteSpot before = *spot;
    spot->status = SUBMITTED;

    if (!spots.updateSpot(*spot))
    {
        *spot = before;
        return DONE_SAVE_FAILED;
    }

    return DONE_OK;
}

DoneResult TaskCompletion::canCheck(SpotList &spots, int spotID, const Moderator &moderator,
                                    WasteSpot *&spot, int &index)
{
    if (moderator.getID() < 1)
        return DONE_BAD_MODERATOR;

    spot = spots.findSpot(spotID);

    if (spot == nullptr)
        return DONE_NO_SPOT;

    index = latestIndex(spotID);

    if (spot->status != SUBMITTED || index == -1 || proofs[index].status != PROOF_WAITING)
        return DONE_NOTHING_TO_CHECK;

    // Nobody checks work they were paid to do
    if (isAssigned(spot, moderator.getID()))
        return DONE_OWN_WORK;

    return DONE_OK;
}

DoneResult TaskCompletion::saveCheck(SpotList &spots, WasteSpot *spot, int index,
                                     SpotStatus newStatus, const ProofRecord &decided)
{
    // Two files change (the spot and the proof), so keep the old versions
    // to put back if the second save fails
    WasteSpot oldSpot = *spot;
    ProofRecord oldProof = proofs[index];

    spot->status = newStatus;

    if (!spots.updateSpot(*spot))
    {
        *spot = oldSpot;
        return DONE_SAVE_FAILED;
    }

    proofs[index] = decided;

    if (!rewriteProof(index))
    {
        proofs[index] = oldProof;
        *spot = oldSpot;
        spots.updateSpot(*spot);   // put the spot file back too
        return DONE_SAVE_FAILED;
    }

    return DONE_OK;
}

DoneResult TaskCompletion::approve(SpotList &spots, int spotID, const Moderator &moderator)
{
    WasteSpot *spot;
    int index;
    DoneResult check = canCheck(spots, spotID, moderator, spot, index);

    if (check != DONE_OK)
        return check;

    ProofRecord decided = proofs[index];
    decided.status = PROOF_APPROVED;
    decided.moderatorID = moderator.getID();
    decided.checkedAt = time(nullptr);

    // Section 21: the cleanup is finished and closed
    return saveCheck(spots, spot, index, COMPLETED, decided);
}

DoneResult TaskCompletion::reject(SpotList &spots, int spotID, const Moderator &moderator,
                                  const string &reason)
{
    WasteSpot *spot;
    int index;
    DoneResult check = canCheck(spots, spotID, moderator, spot, index);

    if (check != DONE_OK)
        return check;

    if (reason.empty())
        return DONE_NO_REASON;

    ProofRecord decided = proofs[index];
    decided.status = PROOF_REJECTED;
    decided.moderatorID = moderator.getID();
    strncpy(decided.reason, reason.c_str(), sizeof(decided.reason) - 1);
    decided.checkedAt = time(nullptr);

    // Back to the workers: they fix it and send a new photo
    return saveCheck(spots, spot, index, ASSIGNED, decided);
}

const char *TaskCompletion::proofStatusName(ProofStatus status)
{
    switch (status)
    {
        case PROOF_WAITING:  return "Waiting";
        case PROOF_APPROVED: return "Approved";
        case PROOF_REJECTED: return "Rejected";
    }

    return "Unknown";
}

const char *TaskCompletion::resultMessage(DoneResult result)
{
    switch (result)
    {
        case DONE_OK:               return "Saved.";
        case DONE_NO_SPOT:          return "No spot with that ID.";
        case DONE_BAD_WORKER:       return "Enter your worker ID.";
        case DONE_NOT_ASSIGNED:     return "This spot has no workers assigned yet.";
        case DONE_NOT_YOUR_JOB:     return "You are not one of the workers assigned to this spot.";
        case DONE_NO_PHOTO:         return "Add an \"after\" photo of the cleaned spot.";
        case DONE_ALREADY_SENT:     return "This work was already sent and is waiting for a moderator.";
        case DONE_FINISHED:         return "This spot is already finished.";
        case DONE_BAD_MODERATOR:    return "Enter your moderator ID.";
        case DONE_NOTHING_TO_CHECK: return "No finished work is waiting to be checked for this spot.";
        case DONE_OWN_WORK:         return "You cannot check work you were assigned to.";
        case DONE_NO_REASON:        return "Say why it is rejected, so the workers know what to fix.";
        case DONE_SAVE_FAILED:      return "Could not save. Try again.";
    }

    return "Unknown result.";
}
