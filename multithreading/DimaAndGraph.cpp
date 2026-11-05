#include <bits/stdc++.h>
using namespace std;

struct Edge {
    int a, b, l, r;
};

vector<int> parent;

int findRoot(int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];   // path compression
        x = parent[x];
    }
    return x;
}

void unite(int a, int b) {
    parent[findRoot(a)] = findRoot(b);
}

bool largerR(const Edge& x, const Edge& y) {
    return x.r > y.r;
}

int main() {
    int n, m;
    cin >> n >> m;

    vector<Edge> edges(m);
    for (int i = 0; i < m; i++) {
        cin >> edges[i].a >> edges[i].b >> edges[i].l >> edges[i].r;
    }

    // Edges with the largest right end come first
    sort(edges.begin(), edges.end(), largerR);

    // The overlap must start at some edge's left end, so try each one
    set<int> candidates;
    for (Edge& e : edges) candidates.insert(e.l);

    int best = 0;
    for (int L : candidates) {
        parent.assign(n + 1, 0);
        for (int i = 0; i <= n; i++) parent[i] = i;

        for (Edge& e : edges) {
            if (e.r < L) break;          // all remaining edges end before L
            if (e.l > L) continue;       // this edge doesn't contain L
            unite(e.a, e.b);
            if (findRoot(1) == findRoot(n)) {
                best = max(best, e.r - L + 1);   // first connection gives the largest R
                break;
            }
        }
    }

    if (best == 0) cout << "Nice work, Dima!" << endl;
    else cout << best << endl;
}