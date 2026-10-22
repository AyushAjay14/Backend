class BoundedBlockingQueue{
    queue<int> q;
    int capacity;
    int maxCap;
    condition_variable has_space , has_value;
    mutex mtx;

    public:
    BoundedBlockingQueue(int cap){
        this->maxCap = cap;
        capacity = 0;
    }

    void enqueue(int value){
        unique_lock<mutex> lock(mtx);
        while(capacity == maxCap){
            has_space.wait(lock);
        }
       q.push(value);
       capacity++;
       has_value.notify_one();
    }

    int dequeue(){
        unique_lock<mutex> lock(mtx);
        while(capacity == 0){
            has_value.wait(lock);
        }
        int value = q.front();
        q.pop();
        capacity--;
        has_space.notify_one();
        return value;
    }
};