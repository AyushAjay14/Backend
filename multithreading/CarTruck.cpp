#include <bits/stdc++.h>
using namespace std;

class ParkingLot {
    vector<int> spots;                     // -1 = free, else vehicle id
    map<int, pair<int,int>> where;         // id -> (start, size)
    mutex m;
    condition_variable freed;

    // Smallest free gap that fits (keeps pairs free for trucks). -1 if none.
    int findSpot(int size) {
        int best = -1, bestLen = INT_MAX;
        for (int i = 0; i < spots.size(); ) {
            if (spots[i] != -1) { i++; continue; }
            int start = i;
            while (i < spots.size() && spots[i] == -1) i++;
            int len = i - start;
            if (len >= size && len < bestLen) { best = start; bestLen = len; }
        }
        return best;
    }

public:
    ParkingLot(int n) : spots(n, -1) {}

    int park(int id, int size) {               // size: 1 = car, 2 = truck. Waits for room.
        unique_lock<mutex> lock(m);
        int start;
        while ((start = findSpot(size)) == -1) freed.wait(lock);
        for (int i = start; i < start + size; i++) spots[i] = id;
        where[id] = {start, size};
        return start;
    }

    void leave(int id) {
        {
            lock_guard<mutex> lock(m);
            auto it = where.find(id);
            if (it == where.end()) return;
            for (int i = it->second.first; i < it->second.first + it->second.second; i++) spots[i] = -1;
            where.erase(it);
        }
        freed.notify_all();                    // cars and trucks wait for different things
    }
};

ParkingLot lot(4);

void vehicle(int id, int size) {
    int pos = lot.park(id, size);
    cout << ((size == 1 ? "car " : "truck ") + to_string(id) + " at " + to_string(pos) + "\n");
    this_thread::sleep_for(chrono::milliseconds(100));
    lot.leave(id);
}

int main() {
    vector<thread> t;
    for (int i = 1; i <= 6; i++) t.emplace_back(vehicle, i, (i % 2) + 1);   // alternate truck/car
    for (auto& x : t) x.join();
}