#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>

void read_topics(vector<string> &topics){

    ifstream topic_file(TOPIC_PATH);
    string topic;

    while( getline(topic_file, topic) ){

        topics.push_back(topic);
    }

    topic_file.close();
}

void create_topic(string topic){

    string path = "/dev/newtopic";

    ofstream newtopic_file(path);

    newtopic_file << topic;

    newtopic_file.close();
}

void write_to_topic(string topic){

    string path = "/dev/topics/" + topic + "/endpoint";
    string msg = "Message of " + topic + ".";

    ofstream endpoint_file(path);

    endpoint_file << msg;

    endpoint_file.close();
}

int main(){

    vector<string> topics;

    read_topics(topics);

    for (auto i=topics.begin(); i!=topics.end(); ++i){

        cout << *i << " " << endl;
    }

    //Create all topics
    for (int j=0; j<topics.size(); ++j){

        create_topic(topics.at(j));
    }

    //Publish some content
    for (int z=0; j<topics.size(); ++j){

        write_to_topic(topics.at(j));
    }

    return 0;
}
