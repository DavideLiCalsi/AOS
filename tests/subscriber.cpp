#include <iostream>
#include <thread>
#include <fstream>
#include <string.h>
#include <vector>

#define TOPIC_PATH "topics.txt"

using std::ifstream;
using std::ofstream;

using std::vector;
using std::string;
using std::thread;

using std::cin;
using std::cout;
using std::endl;

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

    signal_file << 21;

    signal_file.close();
}

int main(){

    vector<string> topics;

    read_topics(topics);

    for (auto i=topics.begin(); i!=topics.end(); ++i){

        cout << *i << " " << endl;
    }

    for (int j=0; j<topics.size(); ++j){

        subscribe(topics.at(j));
        overwrite_signal(topics.at(j));
    }

    return 0;

}
