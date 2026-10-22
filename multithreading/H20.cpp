class H2O {
    mutex mtx;
    condition_variable cvHydrogen;   // hydrogens waiting for room
    condition_variable cvOxygen;     // oxygen waiting for 2 hydrogens
    int h_count = 0;

public:
    void hydrogen(function<void()> releaseHydrogen) {
        unique_lock<mutex> lock(mtx);
        while (h_count == 2) {
            cvHydrogen.wait(lock);
        }

        releaseHydrogen();
        h_count++;

        if (h_count == 2) {
            cvOxygen.notify_one();     // molecule has its 2 H, so one O can go
        }
    }

    void oxygen(function<void()> releaseOxygen) {
        unique_lock<mutex> lock(mtx);
        while (h_count < 2) {
            cvOxygen.wait(lock);
        }

        releaseOxygen();
        h_count = 0;                   // molecule complete, start a new one

        cvHydrogen.notify_all();       // 2 slots opened for hydrogen
    }
};