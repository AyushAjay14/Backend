#include <bits/stdc++.h>
using namespace std;

class ThreadPool {
    queue<function<void()>> q;
    vector<thread> workers;
    mutex m;
    condition_variable hasJob, hasSpace;
    size_t capacity;
    bool done = false;

    int minWorkers, maxWorkers;
    int liveWorkers = 0;     // threads currently alive
    int idleWorkers = 0;     // threads waiting for a job
    chrono::milliseconds idleTimeout{2000};

    void worker() {
        while (true) {
            function<void()> task;
            {
                unique_lock<mutex> lock(m);
                idleWorkers++;
                while (q.empty() && !done) {
                    cv_status s = hasJob.wait_for(lock, idleTimeout);
                    // RECLAIM: idle too long and more threads than needed
                    if (s == cv_status::timeout && q.empty() && !done &&
                        liveWorkers > minWorkers) {
                        idleWorkers--;
                        liveWorkers--;
                        return;
                    }
                }
                idleWorkers--;
                if (q.empty()) {          // done and nothing left
                    liveWorkers--;
                    return;
                }
                task = move(q.front());
                q.pop();
            }
            hasSpace.notify_one();        // a slot freed at pop

            // EXCEPTIONS: a failing task must not kill the worker
            try {
                task();
            } catch (exception& e) {
                cout << ("task failed: " + string(e.what()) + "\n");
            } catch (...) {
                cout << "task failed: unknown error\n";
            }
        }
    }

    void startWorker() {                  // call with lock held
        liveWorkers++;
        workers.emplace_back(&ThreadPool::worker, this);
    }

public:
    ThreadPool(int minW, int maxW, size_t cap) {
        minWorkers = minW;
        maxWorkers = maxW;
        capacity = cap;
        lock_guard<mutex> lock(m);
        for (int i = 0; i < minWorkers; i++) startWorker();
    }

    // TIMEOUT: returns false if there's no space within timeoutMs
    bool enqueue(function<void()> task, int timeoutMs) {
        unique_lock<mutex> lock(m);
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(timeoutMs);
        while (q.size() >= capacity && !done) {
            if (hasSpace.wait_until(lock, deadline) == cv_status::timeout &&
                q.size() >= capacity) {
                return false;             // still full after the deadline
            }
        }
        if (done) return false;
        q.push(move(task));

        // GROW: everyone busy and we're allowed more threads
        if (idleWorkers == 0 && liveWorkers < maxWorkers) startWorker();

        lock.unlock();
        hasJob.notify_one();
        return true;
    }

    int liveCount() {
        lock_guard<mutex> lock(m);
        return liveWorkers;
    }

    ~ThreadPool() {
        {
            lock_guard<mutex> lock(m);
            done = true;
        }
        hasJob.notify_all();
        hasSpace.notify_all();
        for (auto& w : workers) w.join();
    }
};

// ---------- demo ----------
void goodJob() {
    cout << "good job done\n";
}
void badJob() {
    throw runtime_error("something broke");
}
void slowJob() {
    this_thread::sleep_for(chrono::seconds(1));
}

int main() {
    ThreadPool pool(1, 4, 2);   // min 1 thread, max 4, queue holds only 2

    // 1. Exception: the worker logs it and keeps going
    pool.enqueue(badJob, 1000);
    pool.enqueue(goodJob, 1000);

    // 2. Timeout: fill the pool and queue with slow jobs, then try one more
    for (int i = 0; i < 6; i++) pool.enqueue(slowJob, 1000);
    if (!pool.enqueue(goodJob, 100)) {
        cout << "queue full, gave up after 100 ms\n";
    }

    // 3. Grow under load, shrink when idle
    cout << "threads under load: " << pool.liveCount() << endl;
    this_thread::sleep_for(chrono::seconds(6));
    cout << "threads after idle: " << pool.liveCount() << endl;
}