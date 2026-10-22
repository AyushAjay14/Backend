#include <bits/stdc++.h>
using namespace std;

class BackupSystem {
    struct Backup {
        long endTime = 0;
        long retention = 0;
        int parent = -1;          // -1 = full backup (no parent)
        set<int> children;        // backups that depend on this one
    };

    unordered_map<int, Backup> backups;
    shared_mutex mtx;             // many readers OR one writer

    bool isExpired(const Backup& b, long now) {
        return b.endTime + b.retention <= now;
    }

public:
    bool addBackup(int id, long endTime, long retention) {
        unique_lock<shared_mutex> lock(mtx);          // write
        if (backups.count(id)) return false;
        Backup b;
        b.endTime = endTime;
        b.retention = retention;
        backups[id] = b;
        return true;
    }

    // 'child' is an incremental built on top of 'parent'
    bool addDependency(int child, int parent) {
        unique_lock<shared_mutex> lock(mtx);          // write
        if (!backups.count(child) || !backups.count(parent)) return false;
        if (backups[child].parent != -1) return false;    // already has a parent

        // no cycles: walking up from parent must never reach child
        for (int cur = parent; cur != -1; cur = backups[cur].parent) {
            if (cur == child) return false;
        }

        backups[child].parent = parent;
        backups[parent].children.insert(child);
        return true;
    }

    // Backups to restore, in order: full backup first, 'id' last
    vector<int> findRecoveryChain(int id) {
        shared_lock<shared_mutex> lock(mtx);          // read only
        vector<int> chain;
        if (!backups.count(id)) return chain;

        for (int cur = id; cur != -1; cur = backups.at(cur).parent) {
            chain.push_back(cur);
        }
        reverse(chain.begin(), chain.end());
        return chain;
    }

    // Delete every expired backup that nothing depends on. Returns deleted ids.
    vector<int> expireBackups(long now) {
        unique_lock<shared_mutex> lock(mtx);          // write
        vector<int> removed;
        queue<int> ready;

        for (auto& entry : backups) {
            if (isExpired(entry.second, now) && entry.second.children.empty()) {
                ready.push(entry.first);
            }
        }

        while (!ready.empty()) {
            int id = ready.front();
            ready.pop();

            int parent = backups[id].parent;
            backups.erase(id);
            removed.push_back(id);

            // deleting a child may free its parent
            if (parent != -1) {
                Backup& p = backups[parent];
                p.children.erase(id);
                if (p.children.empty() && isExpired(p, now)) ready.push(parent);
            }
        }
        return removed;
    }
};

void print(string label, vector<int> v) {
    cout << label;
    for (int x : v) cout << x << " ";
    cout << endl;
}

int main() {
    BackupSystem sys;
    //            id  endTime  retention
    sys.addBackup(1,  0,       10);    // full
    sys.addBackup(2,  5,       10);    // incremental
    sys.addBackup(3,  8,       100);   // incremental
    sys.addBackup(4,  3,       5);     // incremental

    //  1 ── 2 ── 3
    //  └─── 4
    sys.addDependency(2, 1);
    sys.addDependency(3, 2);
    sys.addDependency(4, 1);

    print("Recovery chain for 3: ", sys.findRecoveryChain(3));
    print("Expired at t=20:      ", sys.expireBackups(20));
    print("Recovery chain for 3: ", sys.findRecoveryChain(3));
    print("Expired at t=200:     ", sys.expireBackups(200));
}