#include <bits/stdc++.h>
using namespace std;

struct Job {
    int id = 0;
    string name;
    int durationMs = 0;
};

// ---------- timing helper: prints "[ 1.0s]" ----------
chrono::steady_clock::time_point START = chrono::steady_clock::now();

string t() {
    double secs = chrono::duration<double>(chrono::steady_clock::now() - START).count();
    char buf[16];
    snprintf(buf, sizeof(buf), "[%4.1fs]", secs);
    return buf;
}

// ---------- scheduler ----------
class JobScheduler {
    mutex m;
    condition_variable cv;
    queue<Job> jobQueue;
    set<int> finished;        // ids of jobs that completed successfully
    set<int> enqueued;        // ids already queued, so no job is queued twice
    int activeJobs = 0;       // jobs currently running
    bool done = false;
    int numWorkers;
    function<vector<Job>(set<int>)> getNextJobs;

    // Ask which jobs are ready and queue the new ones. Call with the lock held.
    void addReadyJobs() {
        vector<Job> ready = getNextJobs(finished);
        for (Job& j : ready) {
            if (!enqueued.count(j.id)) {
                enqueued.insert(j.id);
                jobQueue.push(j);
            }
        }
    }

    void execute(Job job, int workerId) {
        string me = "worker-" + to_string(workerId);
        cout << (t() + " " + me + " START  " + job.name + "\n");
        this_thread::sleep_for(chrono::milliseconds(job.durationMs));
        // To test failure handling, uncomment:
        // if (job.name == "C") throw runtime_error("C failed");
        cout << (t() + " " + me + " FINISH " + job.name + "\n");
    }

    void worker(int workerId) {
        while (true) {
            Job job;
            {
                unique_lock<mutex> lock(m);
                while (jobQueue.empty() && !done) {
                    cv.wait(lock);
                }
                if (jobQueue.empty()) return;      // done and nothing left
                job = jobQueue.front();
                jobQueue.pop();
                activeJobs++;
            }

            bool ok = true;
            try {
                execute(job, workerId);            // outside the lock
            } catch (exception& e) {
                ok = false;
                cout << (t() + " worker-" + to_string(workerId) + " FAILED " +
                         job.name + ": " + e.what() + "\n");
            }

            {
                lock_guard<mutex> lock(m);
                if (ok) {
                    finished.insert(job.id);
                    addReadyJobs();                // children may be ready now
                }
                activeJobs--;
                if (jobQueue.empty() && activeJobs == 0) {
                    done = true;                   // nothing queued, nothing running
                }
            }
            cv.notify_all();
        }
    }

public:
    JobScheduler(function<vector<Job>(set<int>)> fn, int workers) {
        getNextJobs = fn;
        numWorkers = workers;
    }

    void runAll() {
        {
            lock_guard<mutex> lock(m);
            addReadyJobs();                        // jobs with no dependencies
            if (jobQueue.empty()) done = true;
        }

        vector<thread> workers;
        for (int i = 0; i < numWorkers; i++) {
            workers.emplace_back(&JobScheduler::worker, this, i);
        }
        for (auto& w : workers) w.join();
    }
};

// ---------- the job graph ----------
// Graph: A->C, B->C, C->D, B->E
map<int, Job> jobs;
map<int, vector<int>> parents;     // job id -> ids it depends on

// Returns every unfinished job whose parents are all finished
vector<Job> getNextJobs(set<int> finished) {
    vector<Job> ready;
    for (auto& entry : jobs) {
        Job& j = entry.second;
        if (finished.count(j.id)) continue;

        bool allParentsDone = true;
        for (int p : parents[j.id]) {
            if (!finished.count(p)) allParentsDone = false;
        }
        if (allParentsDone) ready.push_back(j);
    }
    return ready;
}

int main() {
    jobs[1] = {1, "A", 1000};
    jobs[2] = {2, "B", 3000};
    jobs[3] = {3, "C", 1000};
    jobs[4] = {4, "D", 1000};
    jobs[5] = {5, "E", 1000};

    parents[1] = {};
    parents[2] = {};
    parents[3] = {1, 2};
    parents[4] = {3};
    parents[5] = {2};

    JobScheduler scheduler(getNextJobs, 3);
    scheduler.runAll();
    cout << t() << " all done" << endl;
}