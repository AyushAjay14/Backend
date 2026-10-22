#include <bits/stdc++.h>
using namespace std;

class RateLimiter {
    typedef chrono::steady_clock Clock;

    int capacity;
    // expire times of accepted requests, soonest on top (min-heap)
    priority_queue<Clock::time_point, vector<Clock::time_point>,
                   greater<Clock::time_point>> active;
    mutex m;

    void removeExpired(Clock::time_point now) {    // call with lock held
        while (!active.empty() && active.top() <= now) {
            active.pop();
        }
    }

public:
    RateLimiter(int cap) {
        capacity = cap;
    }

    // Non-blocking: returns true and takes a slot if one is free
    bool canAccept(int ttlMs) {
        lock_guard<mutex> lock(m);
        Clock::time_point now = Clock::now();
        removeExpired(now);

        if (active.size() >= capacity) return false;
        active.push(now + chrono::milliseconds(ttlMs));
        return true;
    }
};

// ---------- demo ----------
RateLimiter limiter(2);

void request(int id) {
    bool ok = limiter.canAccept(1000);   // each request holds a slot for 1 s
    cout << ("request " + to_string(id) + (ok ? " accepted\n" : " rejected\n"));
}

int main() {
    vector<thread> t;
    for (int i = 1; i <= 4; i++) t.emplace_back(request, i);
    for (auto& x : t) x.join();

    this_thread::sleep_for(chrono::milliseconds(1100));   // let them expire
    request(5);
}