#include <bits/stdc++.h>
using namespace std;

// ================= Thread pool with waitAll() =================
class ThreadPool {
    queue<function<void()>> q;
    vector<thread> workers;
    mutex m;
    condition_variable hasJob, allDone;
    int pending = 0;
    bool done = false;

    void worker() {
        while (true) {
            function<void()> task;
            {
                unique_lock<mutex> lock(m);
                while (q.empty() && !done) hasJob.wait(lock);
                if (q.empty()) return;
                task = move(q.front());
                q.pop();
            }
            task();
            bool last;
            {
                lock_guard<mutex> lock(m);
                pending--;
                last = (pending == 0);
            }
            if (last) allDone.notify_all();
        }
    }

public:
    ThreadPool(int n) {
        for (int i = 0; i < n; i++) workers.emplace_back(&ThreadPool::worker, this);
    }
    void enqueue(function<void()> task) {
        {
            lock_guard<mutex> lock(m);
            q.push(move(task));
            pending++;
        }
        hasJob.notify_one();
    }
    void waitAll() {
        unique_lock<mutex> lock(m);
        while (pending > 0) allDone.wait(lock);
    }
    ~ThreadPool() {
        {
            lock_guard<mutex> lock(m);
            done = true;
        }
        hasJob.notify_all();
        for (auto& w : workers) w.join();
    }
};

// ================= Merkle tree =================
struct Node {
    bool isDir = true;
    string hash;
    map<string, Node*> children;   // sorted, so folder hashes are consistent
};

// Add "/a/b/c.txt" with its file hash, creating folders on the way
void insertFile(Node* root, string path, string fileHash) {
    Node* cur = root;
    string name;
    for (int i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            if (!cur->children.count(name)) cur->children[name] = new Node();
            cur = cur->children[name];
            name = "";
        } else {
            name += path[i];
        }
    }
    cur->isDir = false;
    cur->hash = fileHash;
}

// Bottom-up: a folder's hash comes from its children's names + hashes
void computeHash(Node* node) {
    if (!node->isDir) return;
    string combined;
    for (auto& c : node->children) {
        computeHash(c.second);
        combined += c.first + ":" + c.second->hash + ";";
    }
    hash<string> h;
    node->hash = to_string(h(combined));
}

// ================= Results =================
mutex resultMtx;
vector<string> added, deleted, modified;
atomic<int> foldersCompared(0);

string fullPath(string parent, string name, Node* n) {
    return parent + name + (n->isDir ? "/" : "");
}

// Add a node and everything below it to a list
void collectAll(Node* node, string path, vector<string>& out) {
    out.push_back(path);
    for (auto& c : node->children) {
        collectAll(c.second, fullPath(path, c.first, c.second), out);
    }
}

// Compare two folders whose hashes differ
void compare(Node* a, Node* b, string path, ThreadPool* pool) {
    foldersCompared++;
    vector<string> add, del, mod;

    for (auto& c : a->children) {
        Node* oldChild = c.second;
        string p = fullPath(path, c.first, oldChild);

        if (!b->children.count(c.first)) {           // only in old
            collectAll(oldChild, p, del);
            continue;
        }
        Node* newChild = b->children[c.first];
        if (oldChild->hash == newChild->hash) continue;   // identical: SKIP whole subtree

        if (oldChild->isDir && newChild->isDir) {
            mod.push_back(p);
            pool->enqueue(bind(compare, oldChild, newChild, p, pool));   // dig in, in parallel
        } else if (!oldChild->isDir && !newChild->isDir) {
            mod.push_back(p);                         // file content changed
        } else {                                      // file became folder, or the reverse
            collectAll(oldChild, p, del);
            collectAll(newChild, fullPath(path, c.first, newChild), add);
        }
    }

    for (auto& c : b->children) {
        if (!a->children.count(c.first)) {           // only in new
            collectAll(c.second, fullPath(path, c.first, c.second), add);
        }
    }

    lock_guard<mutex> lock(resultMtx);                // merge once per task
    added.insert(added.end(), add.begin(), add.end());
    deleted.insert(deleted.end(), del.begin(), del.end());
    modified.insert(modified.end(), mod.begin(), mod.end());
}

void printList(string title, vector<string>& items) {
    sort(items.begin(), items.end());
    cout << title << endl;
    if (items.empty()) cout << "  (none)" << endl;
    for (string& s : items) cout << "  " << s << endl;
}

int main() {
    Node* oldRoot = new Node();
    insertFile(oldRoot, "/Downloads/resume.pdf",              "r1");
    insertFile(oldRoot, "/Downloads/folder/file.txt",         "x1");
    insertFile(oldRoot, "/Downloads/folder2/folder/file.txt", "y1");
    insertFile(oldRoot, "/Downloads/photos/a.jpg",            "p1");
    insertFile(oldRoot, "/Downloads/photos/b.jpg",            "p2");

    Node* newRoot = new Node();
    insertFile(newRoot, "/Downloads/resume.pdf",              "r1");   // same
    insertFile(newRoot, "/Downloads/folder/file.txt",         "x2");   // changed
    insertFile(newRoot, "/Downloads/file.txt",                "n1");   // added
    insertFile(newRoot, "/Downloads/photos/a.jpg",            "p1");   // same
    insertFile(newRoot, "/Downloads/photos/b.jpg",            "p2");   // same

    computeHash(oldRoot);
    computeHash(newRoot);

    ThreadPool pool(4);
    if (oldRoot->hash != newRoot->hash) {
        pool.enqueue(bind(compare, oldRoot, newRoot, string("/"), &pool));
    }
    pool.waitAll();

    printList("Added:", added);
    printList("Deleted:", deleted);
    printList("Modified:", modified);
    cout << "Folders compared: " << foldersCompared << endl;
}