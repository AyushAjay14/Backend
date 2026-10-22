#include <bits/stdc++.h>
using namespace std;

class LogicalMemory {
    struct Page {
        vector<int> history;     // all values ever written, oldest first
        mutex m;                 // protects this page's history
    };

    unordered_map<int, Page> pages;
    shared_mutex mapLock;        // protects the map itself

public:
    // Add a new value for a page (creates the page if missing)
    void putPage(int pageId, int value) {
        unique_lock<shared_mutex> mapGuard(mapLock);    // write lock on map
        Page& pg = pages[pageId];                       // creates if missing

        lock_guard<mutex> pageGuard(pg.m);
        pg.history.push_back(value);
    }

    // Most recent value of a page, or -1 if none
    int getPage(int pageId) {
        shared_lock<shared_mutex> mapGuard(mapLock);    // read lock on map
        auto it = pages.find(pageId);
        if (it == pages.end()) return -1;

        Page& pg = it->second;
        lock_guard<mutex> pageGuard(pg.m);
        if (pg.history.empty()) return -1;
        return pg.history.back();
    }

    // Last k values of a page, oldest first
    vector<int> getLastK(int pageId, int k) {
        shared_lock<shared_mutex> mapGuard(mapLock);    // read lock on map
        auto it = pages.find(pageId);
        if (it == pages.end() || k <= 0) return {};

        Page& pg = it->second;
        lock_guard<mutex> pageGuard(pg.m);

        int n = pg.history.size();
        int start = max(0, n - k);   // asked for more than exist? start at 0
        vector<int> result;
        for (int i = start; i < n; i++) {
            result.push_back(pg.history[i]);
        }
        return result;
    }
};

LogicalMemory mem;

void writer(int pageId, int startValue) {
    for (int i = 0; i < 5; i++) {
        mem.putPage(pageId, startValue + i);
    }
}

int main() {
    thread t1(writer, 1, 100);   // page 1 gets 100..104
    thread t2(writer, 2, 200);   // page 2 gets 200..204
    t1.join();
    t2.join();

    cout << "latest page 1: " << mem.getPage(1) << endl;
    cout << "last 3 of page 2: ";
    for (int v : mem.getLastK(2, 3)) cout << v << " ";
    cout << endl;
    cout << "missing page: " << mem.getPage(9) << endl;
}