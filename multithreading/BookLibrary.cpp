/*
## File-Backed Book Library

Design a small **book library** that stores books **on disk**, so they survive program restarts. Each book has two parts:

- **metadata:** a JSON string, such as `{"title": "Dune", "author": "Herbert"}`
- **content:** the book's full text

Many threads may call the library **at the same time**, and the program may **crash at any moment**. Readers must never see a broken or half-written book.

### API

```cpp
class BookLibrary {
public:
    BookLibrary(const string& rootPath);   // directory where everything is stored

    int CreateBook(const string& metadata, const string& content);  // returns a new unique ID
    pair<string, string> GetBook(int id);                           // (metadata, content); empty if missing
    bool UpdateMetadata(int id, const string& newMetadata);         // content stays the same
    vector<int> ListBooks();                                        // all book IDs, sorted
};
```

### Requirements

1. **Unique IDs:** every `CreateBook` call returns a different ID, even when many threads create books at the same time.
2. **IDs persist:** after a restart, new IDs continue from where they left off. They never restart at 1 and never reuse an old ID.
3. **Atomic create:** a book appears **all at once**. No reader may ever see a book with metadata but no content, or a partially written file.
4. **Atomic update:** while metadata is being updated, a reader sees **either the old metadata or the new**, never a mix or an empty file.
5. **Crash safety:** if the program crashes partway through creating or updating a book, existing books must stay intact and readable.
6. **Thread safety:** all four operations can be called from many threads at once.
7. **Fast reads:** `GetBook` and `ListBooks` should not block each other, and ideally should not need locks at all.

### Storage layout (suggested)

```
root/
├── next_id              ← the last ID handed out
├── 1/
│   ├── metadata.json
│   └── content.txt
├── 2/
│   ├── metadata.json
│   └── content.txt
...
```

### Example

```
CreateBook("{\"title\":\"Dune\"}", "Desert planet...")      → 1
CreateBook("{\"title\":\"Emma\"}", "Emma Woodhouse...")     → 2
ListBooks()                                                → [1, 2]
UpdateMetadata(1, "{\"title\":\"Dune (2nd ed.)\"}")         → true
GetBook(1)                                                 → ({"title":"Dune (2nd ed.)"}, "Desert planet...")
UpdateMetadata(99, "...")                                  → false   (no such book)
GetBook(99)                                                → ("", "")
```

**Concurrency example:** 5 threads each call `CreateBook` at the same moment. The result must be 5 books with IDs **1–5**, each ID used exactly once, and `ListBooks()` returning `[1, 2, 3, 4, 5]`.

**Crash example:** the program crashes while writing book 3's content. After restarting, books 1 and 2 are fully readable, book 3 either doesn't exist or is complete, and the next `CreateBook` doesn't reuse an ID that was already handed out.

### Key idea

Writing a file directly is not safe: a reader or a crash can catch it half-written. The standard trick is **write to a temporary name, then rename**. On the same file system, a rename is **atomic**: the file switches from old to new in one step. Apply it to files (for updates) and to whole folders (for creating a book with both files at once).

### Things to think about (common follow-ups)

- **Which operations need locks, and which don't?** Why can reads skip locking entirely when every change is an atomic rename?
- **Two updates to the same book at once:** what happens if both write to the same temporary file name?
- **Multiple processes,** not just threads: a `mutex` doesn't work across processes. What does? (A file lock with `flock`, on a lock file that's **never renamed**.)
- **Updating different books in parallel:** how would you avoid one global update lock? (One lock per book.)
- **Durability:** a rename is atomic, but data can still be lost in a power cut. What does `fsync` add?
- **Leftovers after a crash:** a crash during creation leaves a `3.tmp` folder behind. How do you clean it up, and why must `ListBooks` ignore it?
*/


#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <thread>
using namespace std;
namespace fs = std::filesystem;

class BookLibrary {
    fs::path root;
    mutex idMtx;          // one thread at a time hands out IDs
    mutex updateMtx;      // one thread at a time updates metadata
    int nextId = 0;

    // Write to "<file>.tmp", then rename. Readers see the old file or the new one, never half.
    void atomicWrite(const fs::path& file, const string& data) {
        fs::path tmp = file;
        tmp += ".tmp";
        {
            ofstream out(tmp, ios::trunc);
            out << data;
        }
        fs::rename(tmp, file);
    }

    string readFile(const fs::path& file) {
        ifstream in(file);
        return string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    }

    int allocateId() {
        lock_guard<mutex> lock(idMtx);
        nextId++;
        atomicWrite(root / "next_id", to_string(nextId));   // survives restarts
        return nextId;
    }

public:
    BookLibrary(const string& path) : root(path) {
        fs::create_directories(root);
        if (fs::exists(root / "next_id")) nextId = stoi(readFile(root / "next_id"));
    }

    // Writes both files into a temp folder, then renames the folder into place.
    // Nobody can see the book until both files are complete.
    int CreateBook(const string& metadata, const string& content) {
        int id = allocateId();
        fs::path tmpDir = root / (to_string(id) + ".tmp");
        fs::create_directories(tmpDir);

        atomicWrite(tmpDir / "metadata.json", metadata);
        atomicWrite(tmpDir / "content.txt", content);

        fs::rename(tmpDir, root / to_string(id));       // the book "appears" here
        return id;
    }

    // No lock needed: every file is replaced atomically by rename
    pair<string, string> GetBook(int id) {
        fs::path dir = root / to_string(id);
        if (!fs::exists(dir)) return {"", ""};
        return {readFile(dir / "metadata.json"), readFile(dir / "content.txt")};
    }

    bool UpdateMetadata(int id, const string& newMetadata) {
        lock_guard<mutex> lock(updateMtx);               // two updates never collide on the .tmp file
        fs::path dir = root / to_string(id);
        if (!fs::exists(dir)) return false;
        atomicWrite(dir / "metadata.json", newMetadata);
        return true;
    }

    vector<int> ListBooks() {
        vector<int> ids;
        for (auto& entry : fs::directory_iterator(root)) {
            if (!entry.is_directory()) continue;              // skips next_id
            string name = entry.path().filename().string();
            if (name.find(".tmp") != string::npos) continue;  // book still being written
            ids.push_back(stoi(name));
        }
        sort(ids.begin(), ids.end());
        return ids;
    }
};

// ---------- demo ----------
BookLibrary library("library_data");

void addBook(int n) {
    library.CreateBook("{\"title\":\"Book " + to_string(n) + "\"}", "Text of book " + to_string(n));
}

int main() {
    vector<thread> t;
    for (int i = 1; i <= 5; i++) t.emplace_back(addBook, i);   // 5 books at the same time
    for (auto& x : t) x.join();

    for (int id : library.ListBooks()) cout << id << " ";
    cout << endl;

    library.UpdateMetadata(1, "{\"title\":\"Renamed\"}");
    pair<string, string> book = library.GetBook(1);
    cout << book.first << " | " << book.second << endl;
}