#include <bits/stdc++.h>
using namespace std;

atomic<long> totalLines(0);

// Each thread counts non-empty lines that START inside [start, end)
void countLines(string filename, long start, long end) {
    ifstream file(filename, ios::binary);
    if (!file) return;

    string line;
    if (start == 0) {
        file.seekg(0);
    } else {
        file.seekg(start - 1);   // step back one byte...
        getline(file, line);     // ...and skip to the start of the next full line
    }

    long count = 0;
    while (file.tellg() < end && getline(file, line)) {
        if (!line.empty()) count++;
    }

    totalLines += count;
}

int main() {
    string filename = "rubrik_data.txt";
    int numThreads = 4;

    ifstream temp(filename, ios::binary | ios::ate);
    if (!temp) {
        cout << "Cannot open file" << endl;
        return 1;
    }
    long fileSize = temp.tellg();
    temp.close();

    long chunkSize = fileSize / numThreads;
    vector<thread> workers;

    for (int i = 0; i < numThreads; i++) {
        long start = i * chunkSize;
        long end = (i == numThreads - 1) ? fileSize : start + chunkSize;
        workers.emplace_back(countLines, filename, start, end);
    }

    for (auto& t : workers) t.join();

    cout << "File size: " << fileSize << " bytes" << endl;
    cout << "Total lines: " << totalLines << endl;
}