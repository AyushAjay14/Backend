#include <bits/stdc++.h>
using namespace std;

class SearchInsertDelete {
    // ---------- the data: a singly linked list ----------
    struct Node {
        int val;
        atomic<Node*> next{nullptr};      // atomic: an insert can link a node while searches read
        Node(int v) : val(v) {}
    };
    Node head{0};                         // dummy first node
    Node* tail = &head;                   // touched only by the (single) inserter or deleter

    // ---------- synchronization state ----------
    mutex m;
    condition_variable changed;
    int searchers = 0;                    // searches running now
    bool inserting = false;               // an insert running now
    bool deleting = false;                // a delete running now
    int waitingDeleters = 0;              // deletes waiting for their turn

    void startSearch() {
        unique_lock<mutex> lock(m);
        while (deleting || waitingDeleters > 0) changed.wait(lock);
        searchers++;
    }
    void endSearch() {
        bool last;
        {
            lock_guard<mutex> lock(m);
            searchers--;
            last = (searchers == 0);
        }
        if (last) changed.notify_all();   // a waiting deleter may go now
    }

    void startInsert() {
        unique_lock<mutex> lock(m);
        while (inserting || deleting || waitingDeleters > 0) changed.wait(lock);
        inserting = true;
    }
    void endInsert() {
        {
            lock_guard<mutex> lock(m);
            inserting = false;
        }
        changed.notify_all();
    }

    void startDelete() {
        unique_lock<mutex> lock(m);
        waitingDeleters++;                // from now on, new searches and inserts must wait
        while (deleting || inserting || searchers > 0) changed.wait(lock);
        waitingDeleters--;
        deleting = true;
    }
    void endDelete() {
        {
            lock_guard<mutex> lock(m);
            deleting = false;
        }
        changed.notify_all();
    }

public:
    bool search(int v) {
        startSearch();
        bool found = false;
        for (Node* cur = head.next.load(); cur != nullptr; cur = cur->next.load()) {
            if (cur->val == v) { found = true; break; }
        }
        endSearch();
        return found;
    }

    void insert(int v) {
        startInsert();
        Node* n = new Node(v);            // build the node completely first...
        tail->next.store(n);              // ...then link it in one atomic step
        tail = n;
        endInsert();
    }

    bool remove(int v) {
        startDelete();                    // nobody else is running now
        bool removed = false;
        Node* prev = &head;
        for (Node* cur = head.next.load(); cur != nullptr; prev = cur, cur = cur->next.load()) {
            if (cur->val == v) {
                prev->next.store(cur->next.load());
                if (tail == cur) tail = prev;
                delete cur;               // safe: no searcher can be holding it
                removed = true;
                break;
            }
        }
        endDelete();
        return removed;
    }
};

// ---------- demo ----------
SearchInsertDelete ds;

void inserter(int start) {
    for (int i = start; i < start + 5; i++) ds.insert(i);
}
void searcher(int v) {
    cout << ("search " + to_string(v) + ": " + (ds.search(v) ? "found" : "not found") + "\n");
}
void deleter(int v) {
    cout << ("delete " + to_string(v) + ": " + (ds.remove(v) ? "done" : "missing") + "\n");
}

int main() {
    thread a(inserter, 1);    // inserts 1..5
    thread b(inserter, 10);   // inserts 10..14
    a.join();
    b.join();

    vector<thread> t;
    t.emplace_back(searcher, 3);
    t.emplace_back(searcher, 12);
    t.emplace_back(deleter, 3);
    t.emplace_back(searcher, 3);
    t.emplace_back(inserter, 20);
    for (auto& x : t) x.join();
}