#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>
using namespace std;

class DelayedScheduler {
    typedef chrono::steady_clock Clock;

    struct Item {
        Clock::time_point runAt;
        function<void()> task;
    };

    // makes priority_queue a MIN-heap on runAt (earliest task on top)
    struct Later {
        bool operator()(const Item& a, const Item& b) const {
            return a.runAt > b.runAt;
        }
    };

    priority_queue<Item, vector<Item>, Later> pq;
    mutex m;
    condition_variable cv;
    bool done = false;
    vector<thread> workers;

   void worker() {
        unique_lock<mutex> lock(m);
        while (true) {
            // wait until: shutting down, OR there's a task AND it's due
            while (!done && (pq.empty() || Clock::now() < pq.top().runAt)) {
                if (pq.empty()) cv.wait(lock);
                else cv.wait_until(lock, pq.top().runAt);   // deadline re-read every pass
            }
            if (done) return;

            function<void()> task = pq.top().task;
            pq.pop();

            lock.unlock();
            try {
                task();
            } catch (...) {
                cout << "task failed\n";
            }
            lock.lock();
        }
    }

public:
    DelayedScheduler(int numWorkers) {
        for (int i = 0; i < numWorkers; i++)
            workers.emplace_back(&DelayedScheduler::worker, this);
    }

    // Non-blocking: just records the task and returns
    void schedule(function<void()> task, int delayMs) {
        {
            lock_guard<mutex> lock(m);
            Item item;
            item.runAt = Clock::now() + chrono::milliseconds(delayMs);
            item.task = task;
            pq.push(item);
        }
        cv.notify_one();    // a sleeping worker re-checks: this task may be the earliest now
    }

    ~DelayedScheduler() {
        {
            lock_guard<mutex> lock(m);
            done = true;
        }
        cv.notify_all();
        for (auto& w : workers) w.join();
    }
};

// ---------- demo ----------
chrono::steady_clock::time_point START = chrono::steady_clock::now();

void say(string msg) {
    long ms = chrono::duration_cast<chrono::milliseconds>(
                  chrono::steady_clock::now() - START).count();
    cout << ("[" + to_string(ms) + " ms] " + msg + "\n");
}

int main() {
    DelayedScheduler scheduler(2);

    scheduler.schedule(bind(say, "C (asked for 3000 ms)"), 3000);
    scheduler.schedule(bind(say, "A (asked for 1000 ms)"), 1000);
    scheduler.schedule(bind(say, "B (asked for 2000 ms)"), 2000);
    say("all schedule() calls returned immediately");

    this_thread::sleep_for(chrono::seconds(4));
}   // destructor stops the workers