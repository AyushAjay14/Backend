#include <bits/stdc++.h>
using namespace std;

class ParkingLot {
    vector<int> spots;                    // -1 = free, otherwise the vehicle id parked there
    map<int, pair<int,int>> parked;       // vehicle id -> (first spot, size)
    mutex m;
    condition_variable spaceFreed;

    // Best fit: pick the SMALLEST free gap that fits, to keep big gaps for trucks.
    // Returns the start spot, or -1 if nothing fits. Call with the lock held.
    int findSpot(int size) {
        int bestStart = -1, bestLen = INT_MAX;
        int i = 0, n = spots.size();
        while (i < n) {
            if (spots[i] != -1) { i++; continue; }
            int start = i;
            while (i < n && spots[i] == -1) i++;     // walk to the end of this free gap
            int len = i - start;
            if (len >= size && len < bestLen) {
                bestLen = len;
                bestStart = start;
            }
        }
        return bestStart;
    }

    void place(int id, int start, int size) {      // call with the lock held
        for (int i = start; i < start + size; i++) spots[i] = id;
        parked[id] = {start, size};
    }

public:
    ParkingLot(int n) : spots(n, -1) {}

    // Non-blocking: returns the first spot used, or -1 if the lot has no room
    int tryPark(int id, int size) {
        lock_guard<mutex> lock(m);
        if (parked.count(id)) return -1;           // already parked
        int start = findSpot(size);
        if (start == -1) return -1;
        place(id, start, size);
        return start;
    }

    // Blocking: waits until there is room, then parks
    int park(int id, int size) {
        unique_lock<mutex> lock(m);
        int start = findSpot(size);
        while (start == -1) {
            spaceFreed.wait(lock);
            start = findSpot(size);
        }
        place(id, start, size);
        return start;
    }

    bool leave(int id) {
        {
            lock_guard<mutex> lock(m);
            auto it = parked.find(id);
            if (it == parked.end()) return false;
            int start = it->second.first, size = it->second.second;
            for (int i = start; i < start + size; i++) spots[i] = -1;
            parked.erase(it);
        }
        spaceFreed.notify_all();                   // cars AND trucks may be waiting
        return true;
    }

};

// ---------- demo ----------
ParkingLot lot(6);

void vehicle(int id, int size) {
    int pos = lot.park(id, size);
    string type = (size == 1 ? "car " : "truck ");
    cout << (type + to_string(id) + " parked at " + to_string(pos) + "\n");
    this_thread::sleep_for(chrono::milliseconds(200));
    lot.leave(id);
    cout << (type + to_string(id) + " left\n");
}

int main() {
    vector<thread> t;
    t.emplace_back(vehicle, 1, 2);   // truck
    t.emplace_back(vehicle, 2, 1);   // car
    t.emplace_back(vehicle, 3, 2);   // truck
    t.emplace_back(vehicle, 4, 1);   // car
    t.emplace_back(vehicle, 5, 2);   // truck: may have to wait
    t.emplace_back(vehicle, 6, 1);   // car
    for (auto& x : t) x.join();
}