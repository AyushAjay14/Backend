/*
One-Lane Bridge with Emergency Priority

A narrow bridge connects the North and South banks of a river. The bridge has only one lane, so traffic can flow in only one direction at a time. Each vehicle is a separate 
thread that calls arrive() before crossing and leave() after crossing.

Design a thread-safe class:

cpp
class OneWayBridge {
public:
    OneWayBridge(int capacity);
    void arrive(int direction, bool isAmbulance);   // blocks until it's safe to cross
    void leave();                                   // called after crossing
};

direction is 0 for North → South and 1 for South → North.

Rules
One direction at a time. Vehicles going opposite ways can never be on the bridge together.
Capacity. At most capacity cars can be on the bridge at once.
Ambulances have priority.
An ambulance must cross alone: it waits until the bridge is completely empty.
While any ambulance is waiting or crossing, no new cars may get on. Cars already on the bridge finish crossing normally.
Fairness between directions. If cars are flowing in one direction and 3 or more cars are waiting on the opposite side, new cars in the current direction must stop joining.
 Once the bridge empties, the other side gets its turn.
No busy waiting. Blocked vehicles must sleep (using a condition variable), not spin in a loop.
Example

capacity = 2

Time	Event	Result
t0	Car A arrives going N→S	Bridge empty, so A enters. Direction is now N→S.
t1	Car B arrives going N→S	Same direction, room available, so B enters.
t2	Car C arrives going N→S	Bridge full (2/2), so C waits.
t3	Car D arrives going S→N	Wrong direction, so D waits.
t4	Ambulance X arrives going S→N	Bridge not empty, so X waits. Now no cars may enter.
t5	A and B leave	Bridge empty. Only X can enter: C and D are blocked because an ambulance is waiting.
t6	X crosses alone, then leaves	Bridge empty again. C or D (whoever gets the lock first) enters.
Fairness example

capacity = 5, with cars flowing N→S and 3 cars waiting to go S→N. A new N→S car arrives. There’s room and it’s the same direction, but it must wait, because the other side has 
3 cars waiting. Once the N→S cars on the bridge leave, the S→N cars go.

Things to think about (common interview follow-ups)
Why notify_all instead of notify_one? Waiters wait for different conditions (different directions, ambulance vs. car). Waking just one might wake a vehicle that still can’t go,
 while the one that could go keeps sleeping.
Can a direction still starve? Yes. If the other side has only 1 or 2 cars waiting, rule 4 never kicks in, so a steady stream in the current direction can keep them waiting forever.
 Fixes include a ticket system like the playground problem, a time limit, or switching after N cars.
Can cars starve because of ambulances? If ambulances keep arriving, cars never get a turn. That’s usually acceptable for emergency vehicles, but be ready to discuss it.
Two ambulances from opposite sides? Both wait for an empty bridge. Whichever grabs the lock first goes, then the other.
*/

#include <bits/stdc++.h>
using namespace std;

class OneWayBridge {
    mutex m;
    condition_variable cv;
    int capacity;
    int active = 0;                 // cars currently on the bridge
    int direction = -1;             // -1 = empty, 0 = N->S, 1 = S->N
    int waiting[2] = {0, 0};        // normal cars waiting in each direction
    int waitingAmbulances = 0;
    bool ambulanceOnBridge = false;

    bool canEnter(int dir, bool ambulance) {
        if (ambulance) {
            return active == 0;                         // ambulance needs an empty bridge
        }
        int other = 1 - dir;
        if (waitingAmbulances > 0 || ambulanceOnBridge) return false;   // ambulances first
        if (active == 0) return true;                   // bridge empty
        if (direction != dir) return false;             // traffic going the other way
        if (active == capacity) return false;           // bridge full
        if (waiting[other] >= 3) return false;          // let the other side have a turn
        return true;
    }

public:
    OneWayBridge(int cap) {
        capacity = cap;
    }

    void arrive(int dir, bool ambulance) {
        unique_lock<mutex> lock(m);

        if (ambulance) waitingAmbulances++;
        else waiting[dir]++;

        while (!canEnter(dir, ambulance)) {
            cv.wait(lock);
        }

        if (ambulance) {
            waitingAmbulances--;
            ambulanceOnBridge = true;
        } else {
            waiting[dir]--;
        }
        active++;
        direction = dir;
    }

    void leave() {
        {
            lock_guard<mutex> lock(m);
            active--;
            if (active == 0) {
                direction = -1;
                ambulanceOnBridge = false;
            }
        }
        cv.notify_all();
    }
};

OneWayBridge bridge(2);

void vehicle(int id, int dir, bool ambulance) {
    bridge.arrive(dir, ambulance);
    cout << (ambulance ? "Ambulance " : "Car ") << id
         << (dir == 0 ? " crossing N->S" : " crossing S->N") << endl;
    this_thread::sleep_for(chrono::milliseconds(100));
    bridge.leave();
}

int main() {
    vector<thread> v;
    for (int i = 0; i < 4; i++) {
        v.emplace_back(vehicle, i, i % 2, false);
    }
    v.emplace_back(vehicle, 99, 0, true);
    for (auto& t : v) t.join();
}