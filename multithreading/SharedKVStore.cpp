#include <unordered_map>
#include <string>
#include <vector>
#include <functional>

using namespace std;

class ShardedKVStore {
private:
    struct Shard {
        unordered_map<string, string> data;
        mutex m; // Each shard has its own independent lock
    };

    static const int SHARD_COUNT = 16;
    Shard shards[SHARD_COUNT];

    int getShardIndex(const string& key) {
        // std::hash is built-in C++ for distributing keys uniformly
        return hash<string>{}(key) % SHARD_COUNT;
    }

public:
    void set(string key, string value) {
        int idx = getShardIndex(key);
        lock_guard<mutex> lock(shards[idx].m); // Only lock the specific shard
        shards[idx].data[key] = value;
    }

    string get(string key) {
        int idx = getShardIndex(key);
        lock_guard<mutex> lock(shards[idx].m);
        if (shards[idx].data.count(key)) {
            return shards[idx].data[key];
        }
        return "NOT_FOUND";
    }
};