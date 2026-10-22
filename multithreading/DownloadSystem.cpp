/*
Crash-Safe Parallel File Downloader

You’re building part of a backup system. Given a list of file URLs, your program must download each file from the cloud, store it in local storage, and return
 each file’s metadata. The number of files and their sizes can be arbitrary, so downloads should run in parallel.

The program may crash at any moment. When restarted with the same list of URLs, it must resume where it left off and still return correct metadata for every file,
 without corrupting anything.

APIs you’re given (already implemented)
cpp
struct DownloadedFileMetadata {
    string fileName;
    unsigned long long size;
    string owner;
    string lastUpdate;
};

string BeginCreate(string fileUrl);
void DownloadFromCloudAndStoreLocally(string fileUrl, string handleId);
DownloadedFileMetadata FinishCreate(string handleId);

Each file must go through all three, in this order:

API	What it does	Danger if called twice
BeginCreate(url)	Creates a storage handle for the file and returns its handleId. Only one handle can exist per URL.	Fails: a handle already exists
DownloadFromCloudAndStoreLocally(url, handleId)	Downloads the content and appends it to the handle. Atomic: either all data is written, or none.	Data appended twice,
 so the file is corrupted
FinishCreate(handleId)	Closes the handle, marks the file as complete, and returns its metadata.	Fails: already finished

None of the three are idempotent: calling any of them a second time for the same file is an error.

Your task

Implement:

cpp
vector<DownloadedFileMetadata> DownloadAndStoreFiles(vector<string> fileUrls);
Requirements
Correct order: every file goes through BeginCreate → Download → FinishCreate.
Parallelism: different files are processed at the same time, for example with a thread pool.
Crash recovery: after a restart, never repeat a step that already succeeded for a file, and continue from the next step.
Complete result: return metadata for every URL, including files that finished before the crash.
Thread safety: shared state, such as the record of progress, must be safe to access from many threads.
Crash scenarios your solution must handle
Crash happens after…	On restart, the program should…
Nothing done yet for a file	Start from BeginCreate
BeginCreate succeeded	Reuse the saved handleId, skip to download
Download succeeded	Skip straight to FinishCreate
FinishCreate succeeded	Call nothing; return the saved metadata
Crash in the middle of a download	Download again: it’s atomic, so nothing was written
Example

URLs: a.txt, b.pdf, c.png

Run 1 crashes partway through:

a.txt: all 3 steps done
b.pdf: BeginCreate done, then crash
c.png: nothing done

Run 2 with the same URLs:

a.txt: no API calls, returns saved metadata
b.pdf: skips BeginCreate, downloads using the saved handle, then FinishCreate
c.png: all 3 steps

The result is metadata for all three files, with no API ever called twice for the same file.

Key idea

Progress must be stored somewhere that survives a crash, meaning on disk, not just in memory. After each step succeeds, record it, together with anything a later 
step needs: the handleId after BeginCreate, and the metadata after FinishCreate.

Things to think about (common interview follow-ups)
The gap between an API call and saving progress. If the program crashes after BeginCreate returns but before the progress is written, the restart thinks the step never
 happened and calls it again, which fails. The same gap with the download causes a double append. What would you ask the storage team for? Typical answers: an API to look
  up an existing handle, or one to get the current size of a handle.
How do you store progress safely? Why is an append-only log safer than rewriting the whole file? What does fsync give you that flush doesn’t?
Many processes, not just many threads. If two copies of the program run at once, a mutex isn’t enough. You’d need a file lock (flock) or a database.
A non-crash failure, such as a network error during download: retry with a backoff, and give up after N attempts.
Huge numbers of files: the pool limits how many downloads run at once. How would you choose the number of threads?
The progress file grows forever: periodically compact it to keep only the latest line per URL, or delete it once every file is finished.
*/
#include <bits/stdc++.h>
using namespace std;

// ================= Given types / APIs (stubs so it runs) =================
struct DownloadedFileMetadata {
    string fileName;
    unsigned long long size = 0;
    string owner;
    string lastUpdate;
};

string BeginCreate(string fileUrl) {
    return "handle_" + fileUrl;
}
void DownloadFromCloudAndStoreLocally(string fileUrl, string handleId) {
    this_thread::sleep_for(chrono::milliseconds(100));   // pretend to download
}
DownloadedFileMetadata FinishCreate(string handleId) {
    DownloadedFileMetadata m;
    m.fileName = handleId.substr(7);
    m.size = 1024;
    m.owner = "ayush";
    m.lastUpdate = "2026-10-17";
    return m;
}

// ================= Thread pool (unbounded, from earlier) =================
class ThreadPool {
    queue<function<void()>> q;
    vector<thread> workers;
    mutex m;
    condition_variable hasJob;
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
        }
        hasJob.notify_one();
    }
    ~ThreadPool() {   // finishes all queued jobs, then stops
        {
            lock_guard<mutex> lock(m);
            done = true;
        }
        hasJob.notify_all();
        for (auto& w : workers) w.join();
    }
};

// ================= Progress journal (survives crashes) =================
enum Step { NOT_STARTED = 0, CREATED = 1, DOWNLOADED = 2, FINISHED = 3 };

struct Progress {
    Step step = NOT_STARTED;
    string handleId = "-";
    DownloadedFileMetadata meta;
};

class Journal {
    string path;
    mutex m;                          // only one thread writes at a time
    map<string, Progress> progress;   // latest known state per URL

public:
    Journal(string p) {
        path = p;
    }

    // Read the journal at startup. Each line is one saved step;
    // a later line for the same URL replaces the earlier one.
    void load() {
        ifstream in(path);
        string url, handle, name, owner, updated;
        int step;
        unsigned long long size;
        while (in >> url >> step >> handle >> name >> size >> owner >> updated) {
            Progress p;
            p.step = (Step)step;
            p.handleId = handle;
            p.meta.fileName = name;
            p.meta.size = size;
            p.meta.owner = owner;
            p.meta.lastUpdate = updated;
            progress[url] = p;
        }
    }

    Progress get(string url) {
        lock_guard<mutex> lock(m);
        return progress[url];
    }

    // Record a finished step. Append-only, so a crash mid-write
    // never damages earlier lines.
    void save(string url, Progress p) {
        lock_guard<mutex> lock(m);
        progress[url] = p;
        ofstream out(path, ios::app);
        out << url << " " << p.step << " " << p.handleId << " "
            << (p.meta.fileName.empty() ? "-" : p.meta.fileName) << " "
            << p.meta.size << " "
            << (p.meta.owner.empty() ? "-" : p.meta.owner) << " "
            << (p.meta.lastUpdate.empty() ? "-" : p.meta.lastUpdate) << "\n";
        out.flush();
    }
};

// ================= Download system =================
class DownloadSystem {
    Journal journal;

    // Runs on a worker thread: takes one file through the remaining steps
    void processFile(string url, DownloadedFileMetadata* result) {
        Progress p = journal.get(url);

        if (p.step == NOT_STARTED) {
            p.handleId = BeginCreate(url);
            p.step = CREATED;
            journal.save(url, p);
        }
        if (p.step == CREATED) {
            DownloadFromCloudAndStoreLocally(url, p.handleId);
            p.step = DOWNLOADED;
            journal.save(url, p);
        }
        if (p.step == DOWNLOADED) {
            p.meta = FinishCreate(p.handleId);
            p.step = FINISHED;
            journal.save(url, p);   // metadata saved too, so a restart can still return it
        }

        *result = p.meta;   // each thread writes only its own slot, no lock needed
    }

public:
    DownloadSystem() : journal("download_journal.txt") {}

    vector<DownloadedFileMetadata> DownloadAndStoreFiles(vector<string> fileUrls) {
        journal.load();   // resume from wherever the last run stopped

        vector<DownloadedFileMetadata> results(fileUrls.size());
        {
            ThreadPool pool(4);
            for (int i = 0; i < fileUrls.size(); i++) {
                pool.enqueue(bind(&DownloadSystem::processFile, this, fileUrls[i], &results[i]));
            }
        }   // pool destructor waits for every file to finish

        return results;
    }
};

// ================= main =================
int main() {
    DownloadSystem system;
    vector<string> urls = {"a.txt", "b.pdf", "c.png", "d.mp4", "e.zip"};

    vector<DownloadedFileMetadata> files = system.DownloadAndStoreFiles(urls);

    for (auto& f : files) {
        cout << f.fileName << "  " << f.size << " bytes  owner=" << f.owner
             << "  updated=" << f.lastUpdate << endl;
    }
}