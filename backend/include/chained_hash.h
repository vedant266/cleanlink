#ifndef CHAINED_HASH_H
#define CHAINED_HASH_H

#include <string>
using namespace std;

// Hash table with separate chaining: each bucket is a linked list, so two keys
// with the same hash just share a list (the city table probes for a free slot
// instead). A template, so one table can hold users by name and another can
// hold logged-in sessions by token.
//
// shortcut: fixed bucket count, no resize. Lists get long past a few thousand
// keys; rehash into a bigger array then.
template <typename T>
class ChainedHashTable
{
private:
    static const int BUCKETS = 101;  // prime, so keys spread over every bucket

    struct Node
    {
        string key;
        T value;
        Node *next;
    };

    Node *buckets[BUCKETS];
    int count;

    // djb2: hash = hash * 33 + c. Unlike adding up the letters, "ab" and "ba" differ.
    int hashFunction(const string &key) const
    {
        unsigned long hash = 5381;
        for (unsigned char c : key)
            hash = hash * 33 + c;
        return hash % BUCKETS;
    }

public:
    ChainedHashTable() : count(0)
    {
        for (int i = 0; i < BUCKETS; i++)
            buckets[i] = nullptr;
    }

    ~ChainedHashTable()
    {
        for (int i = 0; i < BUCKETS; i++)
            while (buckets[i] != nullptr)
            {
                Node *next = buckets[i]->next;
                delete buckets[i];
                buckets[i] = next;
            }
    }

    // The table owns its nodes, so it must not be copied
    ChainedHashTable(const ChainedHashTable &) = delete;
    ChainedHashTable &operator=(const ChainedHashTable &) = delete;

    // Adds at the front of the key's list. False if the key is already there.
    bool insert(const string &key, const T &value)
    {
        if (find(key) != nullptr)
            return false;

        int index = hashFunction(key);
        buckets[index] = new Node{key, value, buckets[index]};
        count++;
        return true;
    }

    // Walk the key's list. nullptr if it isn't there.
    T *find(const string &key)
    {
        for (Node *n = buckets[hashFunction(key)]; n != nullptr; n = n->next)
            if (n->key == key)
                return &n->value;
        return nullptr;
    }

    // Unlink the node from its list. False if the key isn't there.
    bool remove(const string &key)
    {
        Node **link = &buckets[hashFunction(key)];
        while (*link != nullptr)
        {
            if ((*link)->key == key)
            {
                Node *gone = *link;
                *link = gone->next;
                delete gone;
                count--;
                return true;
            }
            link = &(*link)->next;
        }
        return false;
    }

    // Call visit(value) for every value, bucket by bucket
    template <typename F>
    void forEach(F visit)
    {
        for (int i = 0; i < BUCKETS; i++)
            for (Node *n = buckets[i]; n != nullptr; n = n->next)
                visit(n->value);
    }

    int size() const { return count; }
};

#endif
