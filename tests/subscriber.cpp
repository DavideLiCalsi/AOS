#include <iostream>
#include <thread>
#include <fstream>
#include <string.h>
#include <vector>
#include <csignal>
#include <unistd.h>

#define TOPIC_PATH "topics.txt"

using std::ifstream;
using std::ofstream;

using std::vector;
using std::string;
using std::thread;

using std::cin;
using std::cout;
using std::endl;

void signalHandler(int signum){
    cout << "Received signal " << signum << endl;
}

void read_topics(vector<string> &topics){

    ifstream topic_file(TOPIC_PATH);
    string topic;

    while( getline(topic_file, topic) ){

        topics.push_back(topic);
    }

    topic_file.close();
}

void subscribe(string topic){

    string path = "/dev/topics/" + topic + "/subscribe";

    ofstream subscribe_file(path);

    subscribe_file << "i";

    subscribe_file.close();
}

void overwrite_signal(string topic){

    string path = "/dev/topics/" + topic + "/signal_nr";

    ofstream signal_file(path);

    signal_file << "\x0b";

    signal_file.close();
}

void thread_sub_and_sig(string topic){

    signal(0xb, signalHandler);

    cout << "Subscribing to " << topic << endl;

    subscribe(topic);

    cout << "Changin signal of " << topic << endl;
    overwrite_signal(topic);

    cout << "Thread for " << topic << endl;

    //Wait for an incoming signal
    int i=0;
    while(1){
        sleep(1);
        i++;

        if(i==100){
            cout << "Closing thread for topic "<<topic << endl;
            return;
        }
    }
}

int main(){

    vector<string> topics;

    read_topics(topics);

    for (auto i=topics.begin(); i!=topics.end(); ++i){

        cout << *i << " " << endl;
    }

    //Sequential testing, no multithreading
    for (int j=0; j<topics.size(); ++j){

        subscribe(topics.at(j));
        overwrite_signal(topics.at(j));
    }

    //Let's test a bit in the case of concurrency

    thread t1 = thread(thread_sub_and_sig, topics.at(0));
    thread t2 = thread(thread_sub_and_sig, topics.at(1));

    t1.join();
    t2.join();

    return 0;

}
