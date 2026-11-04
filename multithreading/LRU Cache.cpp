#include <iostream>
#include <string>
#include <list>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
using namespace std;

class LRUCache {
    struct Entry {
        string value;
        list<string>::iterator pos;        // where this key sits in accessOrder
    };

    int capacity;
    unordered_map<string, Entry> cache;    // key -> (value, position)
    list<string> accessOrder;              // keys, front = most recently used
    mutex m;

    // Move a key to the front in O(1): no searching
    void moveToFront(const string& key, Entry& e) {
        accessOrder.erase(e.pos);          // remove directly at the known position
        accessOrder.push_front(key);
        e.pos = accessOrder.begin();       // remember the new position
    }

public:
    LRUCache(int cap) : capacity(cap) {}

    bool get(const string& key, string& value) {
        lock_guard<mutex> lock(m);
        auto it = cache.find(key);
        if (it == cache.end()) return false;

        moveToFront(key, it->second);
        value = it->second.value;
        return true;
    }

    void put(const string& key, const string& value) {
        lock_guard<mutex> lock(m);
        auto it = cache.find(key);
        if (it != cache.end()) {                       // update existing
            it->second.value = value;
            moveToFront(key, it->second);
            return;
        }
        if ((int)cache.size() >= capacity && !accessOrder.empty()) {
            cache.erase(accessOrder.back());          // evict least recently used
            accessOrder.pop_back();
        }
        accessOrder.push_front(key);                   // add new
        cache[key] = {value, accessOrder.begin()};
    }
};

// ---------- demo ----------
LRUCache lru(3);

void worker(int threadId) {
    string key = "key" + to_string(threadId);
    lru.put(key, "value" + to_string(threadId));
    cout << ("Thread-" + to_string(threadId) + " put " + key + "\n");

    for (int j = 1; j <= 5; j++) {
        string k = "key" + to_string(j);
        string value;
        string shown = lru.get(k, value) ? value : "null";
        cout << ("Thread-" + to_string(threadId) + " got " + k + ": " + shown + "\n");
    }
}

int main() {
    vector<thread> threads;
    for (int i = 1; i <= 5; i++) threads.emplace_back(worker, i);
    for (auto& t : threads) t.join();
}