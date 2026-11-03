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
#include <future>
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
    static const int PARALLEL_DEPTH = 2;      // only the top 2 levels get their own threads

    unique_ptr<Node> build(fs::path p, int depth) {
        unique_ptr<Node> n(new Node());
        hash<string> h;
        if (fs::is_directory(p)) {
            n->isDir = true;
            vector<pair<string, future<unique_ptr<Node>>>> running;

            for (auto& e : fs::directory_iterator(p)) {
                string name = e.path().filename().string();
                if (depth < PARALLEL_DEPTH) {
                    // build this child on a new thread
                    running.push_back({name, async(launch::async, &MerkleTree::build,
                                                   this, e.path(), depth + 1)});
                } else {
                    n->children[name] = build(e.path(), depth + 1);   // deeper: same thread
                }
            }
            for (auto& r : running) n->children[r.first] = r.second.get();   // wait for each

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

    void collect(Node* n, string path, ChangeType t, vector<pair<string, ChangeType>>& out) {
        if (!n->isDir) { out.push_back({path, t}); return; }
        for (auto& c : n->children) collect(c.second.get(), path + "/" + c.first, t, out);
    }

    Node* child(Node* n, const string& name) {
        auto it = n->children.find(name);              // find: read-only, safe across threads
        return it == n->children.end() ? nullptr : it->second.get();
    }

    // Returns its own result list, so threads never write to shared data
    vector<pair<string, ChangeType>> compare(Node* a, Node* b, string path, int depth) {
        vector<pair<string, ChangeType>> out;
        if (a && b && a->hash == b->hash) return out;
        if (!a) { collect(b, path, ChangeType::ADDED, out); return out; }
        if (!b) { collect(a, path, ChangeType::REMOVED, out); return out; }
        if (!a->isDir && !b->isDir) { out.push_back({path, ChangeType::MODIFIED}); return out; }
        if (a->isDir != b->isDir) {
            collect(a, path, ChangeType::REMOVED, out);
            collect(b, path, ChangeType::ADDED, out);
            return out;
        }

        set<string> names;
        for (auto& c : a->children) names.insert(c.first);
        for (auto& c : b->children) names.insert(c.first);

        vector<future<vector<pair<string, ChangeType>>>> running;
        for (auto& name : names) {
            Node* ca = child(a, name);
            Node* cb = child(b, name);
            if (depth < PARALLEL_DEPTH) {
                running.push_back(async(launch::async, &MerkleTree::compare,
                                        this, ca, cb, path + "/" + name, depth + 1));
            } else {
                vector<pair<string, ChangeType>> sub = compare(ca, cb, path + "/" + name, depth + 1);
                out.insert(out.end(), sub.begin(), sub.end());
            }
        }
        for (auto& r : running) {                      // merge each thread's result
            vector<pair<string, ChangeType>> sub = r.get();
            out.insert(out.end(), sub.begin(), sub.end());
        }
        return out;
    }

public:
    MerkleTree(const string& dir) : root(build(dir, 0)) {}

    vector<pair<string, ChangeType>> diff(MerkleTree& other) {
        vector<pair<string, ChangeType>> out = compare(root.get(), other.root.get(), "", 0);
        sort(out.begin(), out.end());
        return out;
    }
};