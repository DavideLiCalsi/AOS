# AOS

This repo contains code for the Advanced Operating Systems course @ Polimi, held by Professor Zaccaria. 
The project was conducted under the supervision of Federico Reghenzani.

The module is implemented in the ps_ipc.c file.

The file is structured as follows:

1) Utility structs: data structures used throughout the code.
2) Utility functions: useful functions written to improve code quality.
3) Functions for SUBSCRIBE: declaration and implementation of functions that should be passed.
to the struct file_operations for the subscribe character device drive.
4) Functions for SIGNAL_NR: same as above, but for the signal_nr specia file.
5) Functions for SUBSCRIBERS: Same as above, but for the subscribers special file.
6) Functions for ENDPOINT: Same as above, but for the endpoint special file.
7) add_new_topic: function that creates a new topic and all the metadata required for it.
8) Functions for NEWTOPIC: same as section 4, but for the newtopic special file.
9) Initialization and cleanup: contains the init_module and cleanu_module functions.

How to use it:

1) write any string to /dev/newtopic in order to create a topic with that name
2) the module will create a /dev/topics/<topic_name> folder
3) write any string to /dev/topics/<topic_name>/subscribe to subscribe to a topic
4) you can read the subscribers' list from /dev/topics/<topic_name>/subscribers
5) write an int to /dev/topics/<topic_name>/signal_nr to select which signal is sent to subscribers
6) you can check which signal is sent by reading the aforementioned file
7) write to /dev/topic/<topic_name>/endpoint to overwrite the message for that topic
8) the module will alert the subscribers by sending them the desired signal
9) you can read the last message written for a topic by reading /dev/topics/<topic_name>/endpoint

In the AOS folder: "make" to compile, resulting in the ps_ipc.ko file.
You need to manually insert the module with "sudo insmod ps_ipc.ko".
In the tests folder: make test to copile test programs. After this step, run creator to create some topics, subscriber to subscribe.
Afterwards, if you open another tab write anything to /dev/topics/<topic_name>/endpoint,
with <topic_name> being a string in topics.txt, the subscriber will report that it has received a signal.

NOTE: right now only processes that subscribed to a topic can read its content.
If you wish to disable this feature, search the "DELETE FROM HERE TO ALLOW EVEN NON-SUBSCRIBED PROCESSES TO READ"
comment and delete 3 lines of code afterwards to fix this.


