#include <bits/stdc++.h>
using namespace std;

// ================= Thread pool with waitAll() =================
class ThreadPool {
    queue<function<void()>> q;
    vector<thread> workers;
    mutex m;
    condition_variable hasJob, allDone;
    int pending = 0;          // tasks queued or running
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

// ================= Web crawler =================
class WebCrawler {
    ThreadPool pool;
    unordered_set<string> visited;
    mutex m;
    string targetHost;
    unordered_map<string, vector<string>>* links;   // stands in for the HTML parser

    string getHost(string url) {
        size_t start = url.find("//");
        start = (start == string::npos) ? 0 : start + 2;
        size_t end = url.find('/', start);
        return url.substr(start, end - start);      // npos end just takes the rest
    }

    // One task: read one page and schedule the new links found on it
    void crawlPage(string url) {
        vector<string> found;
        auto it = links->find(url);                 // read only, so safe from many threads
        if (it != links->end()) found = it->second;

        vector<string> newUrls;
        {
            lock_guard<mutex> lock(m);
            for (string& next : found) {
                if (getHost(next) == targetHost && !visited.count(next)) {
                    visited.insert(next);           // claim it, so nobody else schedules it
                    newUrls.push_back(next);
                }
            }
        }                                           // unlock BEFORE enqueuing

        for (string& next : newUrls) {
            pool.enqueue(bind(&WebCrawler::crawlPage, this, next));
        }
    }

public:
    WebCrawler(int threads) : pool(threads) {}

    vector<string> crawl(string startUrl, unordered_map<string, vector<string>>& parser) {
        links = &parser;
        targetHost = getHost(startUrl);
        visited.insert(startUrl);

        pool.enqueue(bind(&WebCrawler::crawlPage, this, startUrl));
        pool.waitAll();                             // returns when no task is queued or running

        return vector<string>(visited.begin(), visited.end());
    }
};

int main() {
    unordered_map<string, vector<string>> parser;
    parser["http://news.yahoo.com"]      = {"http://news.yahoo.com/news", "http://news.yahoo.com/us", "http://google.com"};
    parser["http://news.yahoo.com/news"] = {"http://news.yahoo.com/news/topics", "http://news.yahoo.com/us"};
    parser["http://news.yahoo.com/us"]   = {"http://news.yahoo.com/news"};

    WebCrawler crawler(4);
    vector<string> result = crawler.crawl("http://news.yahoo.com", parser);

    sort(result.begin(), result.end());
    for (string& url : result) cout << url << endl;
    cout << "Total: " << result.size() << endl;
}