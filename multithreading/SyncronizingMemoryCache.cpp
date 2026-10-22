#include <bits/stdc++.h>
using namespace std;

struct Value {
    string data;
    long version;
};

struct Node {
    unordered_map<string, Value> store;
    vector<vector<string>> buckets;        // keys, sorted, per bucket
    vector<vector<uint64_t>> digests;      // one digest per key, same order
    vector<uint64_t> bucketDigest;         // one digest per bucket
};

uint64_t digestOf(const Value& v) {
    hash<string> hs;
    hash<long> hl;
    return hs(v.data) ^ (hl(v.version) * 0x9E3779B97F4A7C15ULL);
}

// Sort one bucket and fingerprint it. Each thread handles different buckets: no locks.
void prepareBuckets(Node* n, int start, int step) {
    for (int b = start; b < n->buckets.size(); b += step) {
        sort(n->buckets[b].begin(), n->buckets[b].end());
        uint64_t combined = 0;
        for (string& key : n->buckets[b]) {
            uint64_t d = digestOf(n->store[key]);
            n->digests[b].push_back(d);
            combined = combined * 31 + d;
        }
        n->bucketDigest[b] = combined;
    }
}

void prepare(Node& n, int B, int threads) {
    n.buckets.assign(B, {});
    n.digests.assign(B, {});
    n.bucketDigest.assign(B, 0);
    hash<string> h;
    for (auto& e : n.store) n.buckets[h(e.first) % B].push_back(e.first);

    vector<thread> t;
    for (int i = 0; i < threads; i++) t.emplace_back(prepareBuckets, &n, i, threads);
    for (auto& x : t) x.join();
}

long long syncNodes(Node& usa, Node& eu) {
    long long bytesSent = 0;
    int B = usa.buckets.size();

    // a. USA -> EU: one digest per bucket
    bytesSent += (long long)B * 8;

    vector<pair<int,int>> mismatches;             // (bucket, position)
    for (int b = 0; b < B; b++) {
        if (usa.bucketDigest[b] == eu.bucketDigest[b]) continue;   // whole bucket matches

        // b. USA -> EU: per-entry digests for this bucket
        bytesSent += (long long)usa.digests[b].size() * 8;
        for (int i = 0; i < usa.digests[b].size(); i++) {
            if (usa.digests[b][i] != eu.digests[b][i]) mismatches.push_back({b, i});
        }
    }

    // c. EU -> USA: position + EU version for each mismatch
    bytesSent += (long long)mismatches.size() * 16;

    // d. Resolve; only the winner's data travels
    for (auto& m : mismatches) {
        string& key = usa.buckets[m.first][m.second];   // same key on both sides
        Value& u = usa.store[key];
        Value& e = eu.store[key];
        if (e.version > u.version) {                     // EU newer: EU -> USA
            u = e;
            bytesSent += e.data.size();
        } else {                                         // USA newer, or tie: USA -> EU
            e = u;
            bytesSent += u.data.size();
        }
    }
    cout << "Mismatched entries: " << mismatches.size() << endl;
    return bytesSent;
}

int main() {
    Node usa, eu;
    int N = 100000;
    for (int i = 0; i < N; i++) {
        string key = "key" + to_string(i);
        usa.store[key] = {string(200, 'a' + i % 26), 1};
    }
    eu.store = usa.store;

    // Make 5% differ: some EU newer, some USA newer, some same version + different data
    for (int i = 0; i < N; i += 20) {
        string key = "key" + to_string(i);
        if (i % 60 == 0)      eu.store[key] = {string(200, 'x'), 2};    // EU wins
        else if (i % 60 == 20) usa.store[key] = {string(200, 'y'), 3};  // USA wins
        else                   eu.store[key].data = string(200, 'z');   // tie: USA wins
    }

    long long naive = 0;
    for (auto& e : usa.store) naive += e.first.size() + e.second.data.size() + 8;

    prepare(usa, 1 << 14, 4);
    prepare(eu, 1 << 14, 4);
    long long sent = syncNodes(usa, eu);

    bool same = true;
    for (auto& e : usa.store) {
        Value& o = eu.store[e.first];
        if (o.version != e.second.version || o.data != e.second.data) same = false;
    }

    cout << "Naive transfer: " << naive << " bytes" << endl;
    cout << "Actual transfer: " << sent << " bytes" << endl;
    cout << "Replicas identical: " << (same ? "yes" : "no") << endl;
}