class UnisexBathroom{
    mutex mtx;
    condition_variable cv_male, cv_female;
    int counter = 0;
    string usedBy = "";
    public:
    UnisexBathroom(){

    }
    void useBathroom(string gender){
        cout<<"Person of gender "<<gender<<" is using the bathroom"<<endl;
        this_thread::sleep_for(chrono::milliseconds(1000));
        cout<<"Person of gender "<<gender<<" has finished using the bathroom"<<endl;
    }

    void maleUseBathroom() {
        {
            unique_lock<mutex> lock(mtx);          // inside the braces
            while ((usedBy != "" && usedBy != "male") || counter == 3) {
                cv_male.wait(lock);
            }
            counter++;
            usedBy = "male";
        }                                          // lock released HERE
    
        useBathroom("male");                       // no lock held
    
        {
            lock_guard<mutex> guard(mtx);          // inside the braces
            counter--;
            cv_male.notify_all();
            if (counter == 0) {
                usedBy = "";
                cv_female.notify_all();
            }
        }                                          // released HERE
    }

    void femaleUseBathroom(){
        {
            unique_lock<mutex> lock(mtx);
            while((usedBy != "" && usedBy != "female") || counter == 3){
                cv_female.wait(lock);
            }
            counter++;
            usedBy = "female";
        }

        useBathroom("female");

        {
            lock_guard<mutex> guard(mtx);
            counter--;
            cv_female.notify_all();
            if(counter == 0){
                usedBy = "";
                cv_male.notify_all();
            }
        }
    }
};