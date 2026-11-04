#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <iterator>
#include <thread>
#include <algorithm>
using namespace std;
namespace fs = std::filesystem;

enum class ChangeType { ADDED, MODIFIED, REMOVED };

struct Node {
    string hash;
    bool isDir = false;
    map<string, unique_ptr<Node>> children;
};

class MerkleTree {
    unique_ptr<Node> root;

    // ---------- building (given, single-threaded) ----------
    unique_ptr<Node> build(fs::path p) {
        unique_ptr<Node> n(new Node());
        hash<string> h;
        if (fs::is_directory(p)) {
            n->isDir = true;
            for (auto& e : fs::directory_iterator(p))
                n->children[e.path().filename().string()] = build(e.path());
            string combined;
            for (auto& c : n->children) combined += c.second->hash;
            n->hash = to_string(h(combined));
        } else {
            ifstream in(p, ios::binary);
            string data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
            n->hash = to_string(h(data));
        }
        return n;
    }

    // ---------- comparing ----------
    Node* child(Node* n, const string& name) {
        auto it = n->children.find(name);
        return it == n->children.end() ? nullptr : it->second.get();
    }

    void collect(Node* n, string path, ChangeType t, vector<pair<string, ChangeType>>& out) {
        if (!n->isDir) { out.push_back({path, t}); return; }
        for (auto& c : n->children) collect(c.second.get(), path + "/" + c.first, t, out);
    }

    // Normal single-threaded comparison of one part of the tree
    void compare(Node* a, Node* b, string path, vector<pair<string, ChangeType>>& out) {
        if (a && b && a->hash == b->hash) return;            // identical: skip
        if (!a) { collect(b, path, ChangeType::ADDED, out); return; }
        if (!b) { collect(a, path, ChangeType::REMOVED, out); return; }
        if (!a->isDir || !b->isDir) {
            if (!a->isDir && !b->isDir) out.push_back({path, ChangeType::MODIFIED});
            else {
                collect(a, path, ChangeType::REMOVED, out);
                collect(b, path, ChangeType::ADDED, out);
            }
            return;
        }
        set<string> names;
        for (auto& c : a->children) names.insert(c.first);
        for (auto& c : b->children) names.insert(c.first);
        for (auto& name : names) compare(child(a, name), child(b, name), path + "/" + name, out);
    }

    // What each thread runs: compare one subtree, write the changes into *out
    void compareSubtree(Node* a, Node* b, string path, vector<pair<string, ChangeType>>* out) {
        compare(a, b, path, *out);
    }

public:
    MerkleTree(const string& dir) : root(build(dir)) {}

    vector<pair<string, ChangeType>> diff(MerkleTree& other) {
        Node* a = root.get();
        Node* b = other.root.get();
        vector<pair<string, ChangeType>> out;
        if (a->hash == b->hash) return out;                  // nothing changed

        // all top-level names from both sides
        set<string> nameSet;
        for (auto& c : a->children) nameSet.insert(c.first);
        for (auto& c : b->children) nameSet.insert(c.first);
        vector<string> names(nameSet.begin(), nameSet.end());

        // one result list per thread, so threads never share a list
        vector<vector<pair<string, ChangeType>>> results(names.size());

        // create one thread per top-level child
        vector<thread> threads;
        for (int i = 0; i < names.size(); i++) {
            threads.emplace_back(&MerkleTree::compareSubtree, this,
                                 child(a, names[i]), child(b, names[i]),
                                 "/" + names[i], &results[i]);
        }

        // wait for all threads
        for (auto& t : threads) t.join();

        // merge everything
        for (auto& r : results) out.insert(out.end(), r.begin(), r.end());
        sort(out.begin(), out.end());
        return out;
    }
};