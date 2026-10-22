#include <bits/stdc++.h>
using namespace std;

class RWLock {
    mutex m;
    condition_variable cv;
    int readers = 0;            // readers currently reading
    int waitingWriters = 0;     // writers waiting for their turn
    bool writing = false;       // is a writer writing right now?

public:
    void readLock() {
        unique_lock<mutex> lock(m);
        while (writing || waitingWriters > 0) {     // writers get priority
            cv.wait(lock);
        }
        readers++;
    }

    void readUnlock() {
        bool lastReader;
        {
            lock_guard<mutex> lock(m);
            readers--;
            lastReader = (readers == 0);
        }
        if (lastReader) cv.notify_all();
    }

    void writeLock() {
        unique_lock<mutex> lock(m);
        waitingWriters++;
        while (writing || readers > 0) {
            cv.wait(lock);
        }
        waitingWriters--;
        writing = true;
    }

    void writeUnlock() {
        {
            lock_guard<mutex> lock(m);
            writing = false;
        }
        cv.notify_all();
    }
};

RWLock rw;
int sharedData = 0;

void reader(int id) {
    rw.readLock();
    cout << "Reader " << id << " reads " << sharedData << endl;
    this_thread::sleep_for(chrono::milliseconds(100));
    rw.readUnlock();
}

void writer(int id) {
    rw.writeLock();
    sharedData++;
    cout << "Writer " << id << " writes " << sharedData << endl;
    this_thread::sleep_for(chrono::milliseconds(100));
    rw.writeUnlock();
}

int main() {
    vector<thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back(reader, i);
        threads.emplace_back(writer, i);
    }
    for (auto& t : threads) t.join();
}