class Playground{
    int currentTicket;
    int nowServing;
    int currentCapacity;
    string currentTeam;
    mutex mtx;
    condition_variable changed;

    public:
    Playground(int capacity) : currentTicket(0), nowServing(0), currentCapacity(capacity), currentTeam("empty") {}

    void enter(string team){
       unique_lock<mutex> lock(mtx);
       {
        int myTicket = currentTicket++;
        while(myTicket != nowServing || (currentTeam != team && currentTeam != "empty") || currentCapacity == 10){
            changed.wait(lock);
        }
        nowServing++;
        currentTeam = team;
        currentCapacity++;
       }
        changed.notify_all(); // required due to nowServing otherwise players whose ticket is not equal will wait forever
    }
    void leaver(){
        unique_lock<mutex> lock(mtx);
        {
        currentCapacity--;
        if(currentCapacity == 0){
            currentTeam = "";
        }
    }
        changed.notify_all();
    }
};