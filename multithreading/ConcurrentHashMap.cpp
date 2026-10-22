class ConcurrentHashMap {
private:
    vector<shared_mutex> locks;
    vector<unordered_map<int, int>> buckets;

    int stripeFor(int key) {
        long long count = static_cast<long long>(locks.size());
        long long stripe = static_cast<long long>(key) % count;
        return static_cast<int>(stripe < 0 ? stripe + count : stripe);
    }

public:
    ConcurrentHashMap(int stripeCount)
        : locks(static_cast<size_t>(stripeCount)),
          buckets(static_cast<size_t>(stripeCount)) {}

    void put(int key, int value) {
        int stripe = stripeFor(key);
        unique_lock<shared_mutex> lock(locks[stripe]);
        buckets[stripe][key] = value;
    }

    int get(int key) {
        int stripe = stripeFor(key);
        shared_lock<shared_mutex> lock(locks[stripe]);
        auto found = buckets[stripe].find(key);
        return found == buckets[stripe].end() ? -1 : found->second;
    }

    void remove(int key) {
        int stripe = stripeFor(key);
        unique_lock<shared_mutex> lock(locks[stripe]);
        buckets[stripe].erase(key);
    }

    int increment(int key, int delta) {
        int stripe = stripeFor(key);
        unique_lock<shared_mutex> lock(locks[stripe]);
        return buckets[stripe][key] += delta;
    }

    int size() {
        for (shared_mutex& lock : locks) {
            lock.lock_shared();
        }
        int count = 0;
        for (const auto& bucket : buckets) {
            count += static_cast<int>(bucket.size());
        }
        for (auto lock = locks.rbegin(); lock != locks.rend(); ++lock) {
            lock->unlock_shared();
        }
        return count;
    }
};