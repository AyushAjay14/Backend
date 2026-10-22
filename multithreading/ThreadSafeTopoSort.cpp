#include <bits/stdc++.h>
using namespace std;

class ThreadSafeTopoSort {
    int n;
    vector<vector<int>> adj;
    vector<int> indegree;
    vector<int> result;
    queue<int> q;
    int busy = 0;             // nodes currently being processed
    bool done = false;
    int noOfWorkers;
    mutex mtx;
    condition_variable cv;

    void processNode(int node) {                        // the real work for a node
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    void worker() {
        while (true) {
            int node;
            {
                unique_lock<mutex> lock(mtx);
                while (q.empty() && !done) cv.wait(lock);
                if (q.empty()) return;                  // done and nothing left
                node = q.front();
                q.pop();
                busy++;
            }

            processNode(node);                          // OUTSIDE the lock, runs in parallel

            {
                lock_guard<mutex> lock(mtx);
                result.push_back(node);
                for (int next : adj[node]) {
                    indegree[next]--;
                    if (indegree[next] == 0) q.push(next);
                }
                busy--;
                if (q.empty() && busy == 0) done = true;   // nothing queued, nothing running
            }
            cv.notify_all();
        }
    }

public:
    ThreadSafeTopoSort(int n, vector<vector<int>> edges, int workers) {
        this->n = n;
        noOfWorkers = workers;
        adj.resize(n);
        indegree.assign(n, 0);
        for (auto& e : edges) {
            adj[e[0]].push_back(e[1]);
            indegree[e[1]]++;
        }
        for (int i = 0; i < n; i++) {
            if (indegree[i] == 0) q.push(i);
        }
        if (q.empty()) done = true;                     // no starting node (cycle or empty)
    }

    // Returns a topological order, or an empty vector if there's a cycle
    vector<int> run() {
        vector<thread> workers;
        for (int i = 0; i < noOfWorkers; i++) {
            workers.emplace_back(&ThreadSafeTopoSort::worker, this);
        }
        for (auto& w : workers) w.join();

        if (result.size() < n) return {};               // some nodes never reached 0: cycle
        return result;
    }
};

int main() {
    //  0 → 2,  1 → 2,  2 → 3,  1 → 4
    ThreadSafeTopoSort sorter(5, {{0, 2}, {1, 2}, {2, 3}, {1, 4}}, 3);
    vector<int> order = sorter.run();

    if (order.empty()) cout << "cycle detected" << endl;
    for (int x : order) cout << x << " ";
    cout << endl;
}