from threading import Thread

class Publisher(Thread):

    def __init__(self, topic_list, message_list):
        Thread.__init__(self)
        self.topic_list = topic_list
        self.message_list=message_list

    def run(self):
        newtopic=open("/dev/newtopic", "w")
        for t in self.topic_list:
            newtopic.write(t)

        newtopic.close()
        sleep(2)
        for t in self.topic_list:
            signal_nr = open("/dev/topics" + t + "/signal_nr", "w")
            signal_nr.write("\x0b")
            signal_nr.close()

            for m in self.message_list:
                signal_nr = open("/dev/topics" + t + "/endpoint", "w")
                signal_nr.write(m)
                signal_nr.close()

class Subscriber(Thread):

    def __init__(self, topic):
        Thread.__init__(self)
        self.topic=topic

    def run(self):
        subscribe = open("/dev/topics" + self.topic + "/subscribe","w")
        subscribe.write("a")
        subscribe.close()

t_list = ["casa","mare","monti","alert","hacker"]
m_list = ["a","b","c","d","e", "1","2","3","4"]

sub_list = []

for t in t_list:
    sub = Subscriber(t)
    sub_list.append(sub)

pub = Publisher(t_list,m_list)
pub.start()
sub.start()



