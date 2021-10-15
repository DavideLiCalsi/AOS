/*
#
#     PATH'S MACROS
#
*/

//Base path that should contain all the special files needed for the IPC
#define PATH "/dev/psipc/"

//Ending of the path of the special file needed to declare a new topic
#define NEWTOPIC "newtopic"


/*
#
#     DEVICE DRIVER's MACROS
#
*/


//Default major number to be used when calling int __register_chrdev.
//By using 0 the OS will automatically find a major number for you.
#define DEFAULT_MAJOR 0
