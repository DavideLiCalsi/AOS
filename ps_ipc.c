#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <linux/list.h>
#include <linux/rwlock.h>
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>

#define FILE_CLASS "AOS_PS_IPC"
#define NEWTOPIC_NAME "newtopic"
#define MAX_TOPICS 100
#define MAX_SIG 64
#define MAX_TOPIC_LEN 50
#define MAX_SUBSCRIBERS 300
#define MAX_TOPIC_NAME_LEN 20
#define MAX_MESSAGE_LEN 500

#define MIN(x,y) (x<=y? x:y)
#define MAX(x,y) (x>=y? x:y)

MODULE_LICENSE("GPL");

/*##################################################
# 1)      Utility structs to ease development        #
###################################################*/


/*Utility struct to implement a list of integers. It will store
 the PIDs of the processes that subscribe to a topic*/
struct pid_node{
    int pid;
    struct list_head list;
};

/*Utility struct containing all data related to a topic*/
struct topic_subscribe{

	//dev_t and cdev for the 'subscribe' file
	dev_t subscribe_dev;
	struct cdev subscribe_cdev;
	
	//dev_t and cdev for the 'signal_nr' file
	dev_t signal_nr_dev;
	struct cdev signal_nr_cdev;
	
	//dev_t and cdev for the 'subscribers' file
	dev_t subscribers_dev;
	struct cdev subscribers_cdev;

    //dev_t and cdev for the 'subscribers' file
	dev_t endpoint_dev;
	struct cdev endpoint_cdev;
	

    //Current signal number
	int signal_nr;

    //Topic name
	char* name;

    //Latest message content
    char msg[MAX_MESSAGE_LEN];

    //subscribed PIDs list
    struct list_head* pid_list;

    //file_operations structs
	struct file_operations subscribe_fo;
	struct file_operations signal_nr_fo;
    struct file_operations subscribers_fo;
    struct file_operations endpoint_fo;

    //Locks
    rwlock_t subscribe_lock;
    spinlock_t signal_nr_lock;
    spinlock_t endpoint_lock;

    struct mutex subscribe_mutex;
    struct mutex signal_nr_mutex;
    struct mutex endpoint_mutex;
};

static struct class* cl;


static dev_t newtopic_dev;
static struct cdev newtopic_cdev;

static struct topic_subscribe* subscribe_data[MAX_TOPICS];

static int topics_count = 0;

/*This mutex protects the array of topics. It is locked in two occasions:
 * 1st: at the beginning and end of the search_topic_subscribe function
 * 2nd: in the add_new_topic, when a new topic is added
 */
DEFINE_MUTEX(topic_list_mutex);

/*##################################################
# 2)      Utility functions to ease development    #
###################################################*/

/*Search a topic by name*/
struct topic_subscribe* search_topic_subscribe(char* name){

    int i;

    mutex_lock(&topic_list_mutex);
    for(i=0; i<topics_count; ++i){

        if (!strcmp(name, subscribe_data[i]->name) ){
            mutex_unlock(&topic_list_mutex);
            return subscribe_data[i];
        }
    }
    mutex_unlock(&topic_list_mutex);

    return NULL;
}



/*Send signal signal_nr to task*/
void send_signal(int signal_nr, int pid){

    struct siginfo info;
    pr_info("Requested sending signal %d to task %d\n",signal_nr, pid);

    //Sending signal to app
    memset(&info, 0, sizeof(struct siginfo));
    info.si_signo = signal_nr;
    info.si_code = SI_QUEUE;
    info.si_int = 1;

    struct task_struct* task = get_pid_task(find_get_pid(pid),PIDTYPE_PID);
    if (task != NULL) {
        printk(KERN_INFO "Sending signal %d to app %d\n",signal_nr, pid);
        if(send_sig(signal_nr, task,1) < 0) {
            printk(KERN_INFO "Unable to send signal\n");
        }
    }
}

void signal_subscribers(int signal_nr, struct list_head* pid_list){

    if (signal_nr <= 0){
        pr_err("Invalid signal code %d, please specify a valid one\n", signal_nr);
        return;
    }

    struct list_head* cursor;

    list_for_each(cursor,pid_list){

        struct pid_node* sub_process = list_entry(cursor,struct pid_node,list);
        send_signal(signal_nr, sub_process->pid);

    }


}

/*Searches for a pid in the list of processes that are subbscribers of a topic. Returns
 pid if the process was found, -1 otherwise*/
int find_pid(struct list_head* pid_list, int pid){

    struct list_head* cursor;

    list_for_each(cursor,pid_list){

        struct pid_node* sub_process = list_entry(cursor,struct pid_node,list);

        if( sub_process->pid == pid )
            return pid;
    }

    return -1;
}

/*Resets the topic buffer to 0*/
void reset_string(char* topic, int len){

    int i;

    for(i=0; i<len; ++i){
        topic[i] = '\0';
    }
}

 int subscribers_count(struct list_head* pid_list){

     int count=0;
     struct list_head* cursor;

     list_for_each(cursor,pid_list){

        count++;
    }

    return count;
 }

 /*Allocates major and minor numbers for the character special files required by each topic*/
 int allocate_chrdev(struct topic_subscribe* new_topic_subscribe, char* topic_subscribe_path,  char* topic_signal_path,char* topic_subscribers_path, char* topic_endpoint_path ){

     /*Allocating Major number*/
  	pr_info("Trying to allocate a major and minor number for %s device file\n", topic_subscribe_path);
  	pr_info("Trying to allocate a major and minor number for %s device file\n", topic_signal_path);
    pr_info("Trying to allocate a major and minor number for %s device file\n", topic_subscribers_path);
    pr_info("Trying to allocate a major and minor number for %s device file\n", topic_endpoint_path);

  	if( (alloc_chrdev_region(&new_topic_subscribe->signal_nr_dev, 0, 1, topic_signal_path)) < 0 ||
  		(alloc_chrdev_region(&new_topic_subscribe->subscribe_dev, 0, 1, topic_subscribe_path)) < 0 ||
  		(alloc_chrdev_region(&new_topic_subscribe->subscribers_dev, 0, 1, topic_subscribers_path)) < 0 ||
  		(alloc_chrdev_region(&new_topic_subscribe->endpoint_dev, 0, 1, topic_endpoint_path)) < 0){

      		pr_err("Cannot allocate major numbers for devices\n");
      		return -1;
  	}

  	return 0;
 }

 void init_chrdev(struct topic_subscribe* new_topic_subscribe){

     //Initialize the cdev structure
  	cdev_init(&new_topic_subscribe->subscribe_cdev, &new_topic_subscribe->subscribe_fo);
  	cdev_init(&new_topic_subscribe->signal_nr_cdev, &new_topic_subscribe->signal_nr_fo);
    cdev_init(&new_topic_subscribe->subscribers_cdev, &new_topic_subscribe->subscribers_fo);
    cdev_init(&new_topic_subscribe->endpoint_cdev, &new_topic_subscribe->endpoint_fo);
 }

 void add_cdevs(struct topic_subscribe* new_topic_subscribe){

     //Add the special file to the system
  	if (cdev_add(&new_topic_subscribe->subscribe_cdev, new_topic_subscribe->subscribe_dev,1) < 0 ||
  		cdev_add(&new_topic_subscribe->signal_nr_cdev, new_topic_subscribe->signal_nr_dev,1) < 0  ||
        cdev_add(&new_topic_subscribe->subscribers_cdev, new_topic_subscribe->subscribers_dev,1) < 0 ||
        cdev_add(&new_topic_subscribe->endpoint_cdev, new_topic_subscribe->endpoint_dev,1) < 0 )

  		pr_err("Could not add special files to system\n");
 }

 void create_devices(struct topic_subscribe* new_topic_subscribe, char* topic_subscribe_path, char* topic_signal_path, char* topic_subscribers_path, char* topic_endpoint_path ){

     //Create the special files subscribe and signal_nr
  	if ( device_create(cl, NULL, new_topic_subscribe->subscribe_dev, NULL, topic_subscribe_path) == NULL ||
  		device_create(cl, NULL, new_topic_subscribe->signal_nr_dev, NULL, topic_signal_path) == NULL ||
        device_create(cl, NULL, new_topic_subscribe->subscribers_dev, NULL, topic_subscribers_path) == NULL ||
        device_create(cl, NULL, new_topic_subscribe->endpoint_dev, NULL, topic_endpoint_path) == NULL ){
  		printk(KERN_ALERT "Could not create one of the special files\n");
  	}
  	else
  		printk(KERN_INFO "Succesfully created special files\n");

 }

 void init_locks(struct topic_subscribe* new_topic_subscribe){

     mutex_init(&new_topic_subscribe->subscribe_mutex);
     mutex_init(&new_topic_subscribe->endpoint_mutex);
     mutex_init(&new_topic_subscribe->signal_nr_mutex);
 }

 void free_memory(struct topic_subscribe* to_free){

     struct list_head* pids = to_free->pid_list;
     struct list_head* cursor;

     //Free every node in the list of subscribers
     list_for_each(cursor,pids){

        struct pid_node* sub_process = list_entry(cursor,struct pid_node,list);
        kfree(sub_process);

    }

    //Free the list_head itself
    kfree(pids);

    //Free the topic's name
    kfree(to_free->name);

    //Free the struct topic_subscribe
    kfree(to_free);
 }
/*##########################################################################################
# 3)  Functions to pass to struct file_operations to manage the SUBSCRIBE special file(s)  #
###########################################################################################*/

static int subscribe_open(struct inode * inode, struct file * filp);
static int subscribe_release(struct inode * inode, struct file * filp);
static ssize_t subscribe_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t subscribe_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

static int subscribe_open(struct inode * inode, struct file * filp){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

	pr_info("Opening subscription file for topic %s\n", this_file);

    struct topic_subscribe* this_topic_subscribe = search_topic_subscribe( this_file);

    if (this_topic_subscribe==NULL){
        pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;
    }

	return 0;
}


static int subscribe_release(struct inode * inode, struct file * filp){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

	pr_info("Releasing subscription file for topic %s\n", this_file);

    struct topic_subscribe* this_topic_subscribe = search_topic_subscribe( this_file);
	
	return 0;
}

static ssize_t subscribe_read(struct file * filp, char* buffer, size_t size, loff_t * offset){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

	pr_info("Reading subscription file for topic %s\n", this_file);
	
	return 0;
}

static ssize_t subscribe_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){

	//Get the file name of this special file
	char this_file[50], topic_name[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

	pr_info("Writing subscription file for topic %s\n", this_file);

    //Create the struct pid_node to add to the list

    //First retrieve the pid of the process writing to this file
    int pid = current->pid;

    //Now create the struct pid_node to add the list of subscribers
    struct pid_node* new = kmalloc(sizeof(struct pid_node), GFP_KERNEL);

    //Fault if new is null
    if (new == NULL)
        return -EFAULT;

    new->pid = pid;

    //Retrieve the pid_list pointer in order to add the new pid to the linked list
    struct topic_subscribe* this_topic_subscribe = search_topic_subscribe( this_file );

    if( this_topic_subscribe == NULL) {
        pr_err("Topic not found, subscription aborted\n");
        return -EFAULT;
    }

    mutex_lock(&this_topic_subscribe->subscribe_mutex);

    struct list_head* this_list = this_topic_subscribe->pid_list;

    //Abort if the process is already subscribed to this topic
    if ( find_pid(this_list, pid) != -1){
        pr_err("Process is already a subscriber, subscription aborted\n");
        mutex_unlock(&this_topic_subscribe->subscribe_mutex);
        return -EFAULT;
    }

    //Abort if the list is already full
    if ( subscribers_count(this_list) == MAX_SUBSCRIBERS ){
        pr_err("Too many subscribers, subscription aborted\n");
        mutex_unlock(&this_topic_subscribe->subscribe_mutex);
        return -EFAULT;
    }

    //Once the previous checks were passed, the process can be added to the subscribers' list
    list_add_tail(&new->list, this_list);
    mutex_unlock(&this_topic_subscribe->subscribe_mutex);

    pr_info("Process %d has succesfully subscribed!\n", pid);

    return sizeof(int);
}

/*##########################################################################################
# 4)  Functions to pass to struct file_operations to manage the SIGNAL_NR special file(s)  #
###########################################################################################*/

static int signal_nr_open(struct inode * inode, struct file * filp);
static int signal_nr_release(struct inode * inode, struct file * filp);
static ssize_t signal_nr_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t signal_nr_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

static int signal_nr_open(struct inode * inode, struct file * filp){

    //Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

    pr_info("Opening signal_nr file\n");


    struct topic_subscribe* this_topic_subscribe = search_topic_subscribe( this_file);

    if (this_topic_subscribe==NULL){

        pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;

    }

	return 0;
}

static int signal_nr_release(struct inode * inode, struct file * filp){
    //Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

	pr_info("Releasing signal_nr file\n");

    struct topic_subscribe* this_topic_subscribe = search_topic_subscribe( this_file);

    if (this_topic_subscribe==NULL){

        pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;

    }

	return 0;
}

static ssize_t signal_nr_read(struct file * filp, char* buffer, size_t size, loff_t * offset){

	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);
	struct topic_subscribe* temp = NULL;

	temp=search_topic_subscribe(this_file);

	
	if ( temp == NULL){
		pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;
	}


	
	int signal_code = temp->signal_nr;
	
	//Convert the signal number to char
	char signal_as_string;
    signal_as_string=(char) signal_code;


    if ( *offset != 1){
	
        //Copy it to buffer
        int error_count = 0;
	
        mutex_lock(&temp->signal_nr_mutex);

        error_count = copy_to_user(buffer, &signal_as_string, 1);

        mutex_unlock(&temp->signal_nr_mutex);

        if (error_count != 0){

            pr_err("Unexpected error during read operation. Abort\n");

            return -EFAULT;
        }
	
        pr_info("The signal sent to subscribers for topic %s is %d\n", this_file, signal_code);
        *offset =1;

        return 1;
    }
    else{
        return 0;
    }
}

static ssize_t signal_nr_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){
	
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);
	
	pr_info("Attempting to overwrite signal_nr for topic %s\n", this_file);
	
	struct topic_subscribe* temp = NULL;
	
    temp=search_topic_subscribe(this_file);


	if ( temp == NULL){
		pr_err("Anomaly detected! Topic not found in the system\n");
		return 0;
	}


	
	long not_copied;
	char signal_as_string[1];
    int signal_nr;

	mutex_lock(&temp->signal_nr_mutex);

	not_copied = copy_from_user(signal_as_string, buffer, 1);


    if (not_copied != 0){

        pr_err("Unexpected error during write operation. Abort\n");
        mutex_unlock(&temp->signal_nr_mutex);

        return -EFAULT;
    }


    signal_nr = (int) signal_as_string[0];
	
	pr_info("Provided signal code: %d\n", signal_nr);
	
	
    if( signal_nr <= MAX_SIG || signal_nr < 0){

        temp->signal_nr = signal_nr;
        pr_info("Signal code succesfully updated!\n");

    }
    else
        pr_err("Invalid signal number. Overwriting denied. \n");

	mutex_unlock(&temp->signal_nr_mutex);

	return size;
}

/*##########################################################################################
# 5)  Functions to pass to struct file_operations to manage the SUBSCRIBERS special file(s)  #
###########################################################################################*/

static int subscribers_open(struct inode * inode, struct file * filp);
static int subscribers_release(struct inode * inode, struct file * filp);
static ssize_t subscribers_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t subscribers_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

static int subscribers_open(struct inode * inode, struct file * filp){

    char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

    struct topic_subscribe* temp = NULL;

    temp=search_topic_subscribe(this_file);

	if ( temp == NULL ){
		pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;
	}

    pr_info("Opening subscribers file\n");

    return 0;
}

static int subscribers_release(struct inode * inode, struct file * filp){

    char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

    struct topic_subscribe* temp = NULL;

    temp=search_topic_subscribe(this_file);

	if ( temp == NULL || temp->pid_list == NULL){
		pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;
	}

    pr_info("Releasing subscribers file\n");

    return 0;
}

static ssize_t subscribers_read(struct file * filp, char* buffer, size_t size, loff_t * offset){

    char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

    pr_info("Here's a list of the subscribers to topic %s\n", this_file);

    struct topic_subscribe* temp = NULL;

    temp=search_topic_subscribe(this_file);

	if ( temp == NULL || temp->pid_list == NULL){
		pr_err("Anomaly detected! Topic not found in the system or list is not initialized\n");
		return -EFAULT;
	}


	struct list_head* pids = temp->pid_list;
    struct list_head* cursor;

    int subscribers[MAX_SUBSCRIBERS];
    int i=0;

    mutex_lock(&temp->subscribe_mutex);

    if (*offset ==0) {
        list_for_each(cursor,pids){

            struct pid_node* sub_process = list_entry(cursor,struct pid_node,list);
            pr_info("%d\n", sub_process->pid);
            subscribers[i]=sub_process->pid;
            i++;
        }
    }

    int chars_to_read = MIN(size, i);

    if ( chars_to_read != *offset){

        long not_copied;

        not_copied = copy_to_user(buffer, subscribers, sizeof(int)*chars_to_read );

        if (not_copied != 0){

            pr_err("Unexpected error during read operation. Abort\n");
            mutex_unlock(&temp->subscribe_mutex);

            return -EFAULT;
        }

        *offset +=chars_to_read;
        mutex_unlock(&temp->subscribe_mutex);

        return sizeof(int)*chars_to_read;
    }
    else{
        mutex_unlock(&temp->subscribe_mutex);

        return 0;
    }
}

static ssize_t subscribers_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){
    pr_info("Writing to subscribers file has no effect\n");
    return 0;
}


/*##########################################################################################
#  6) Functions to pass to struct file_operations to manage the ENDPOINT special file(s)   #
###########################################################################################*/

static int endpoint_open(struct inode * inode, struct file * filp);
static int endpoint_release(struct inode * inode, struct file * filp);
static ssize_t endpoint_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t endpoint_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

static int endpoint_open(struct inode * inode, struct file * filp){

     char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

    pr_info("Opening endpoint file\n");

    struct topic_subscribe* temp = NULL;

    temp=search_topic_subscribe(this_file);

    //If the topic does not exist or there is no subscribers list, signal anomaly
	if ( temp == NULL ){

		pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;

	}

    return 0;
}

static int endpoint_release(struct inode * inode, struct file * filp){
    char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

    pr_info("Releasing endpoint file\n");

    struct topic_subscribe* temp = NULL;

    temp=search_topic_subscribe(this_file);

    //If the topic does not exist or there is no subscribers list, signal anomaly
	if ( temp == NULL ){
		pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;
	}


    return 0;
}

static ssize_t endpoint_read(struct file * filp, char* buffer, size_t size, loff_t * offset){

    char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

    pr_info("Reading message for topic %s\n", this_file);

    struct topic_subscribe* temp = NULL;

    temp=search_topic_subscribe(this_file);

    //If the topic does not exist or there is no subscribers list, signal anomaly
	if ( temp == NULL || temp->pid_list == NULL){
		pr_err("Anomaly detected! Topic not found in the system or list is not initialized\n");
		return -EFAULT;
	}

	struct list_head* pids = temp->pid_list;

    //If the reading process is not a suscriber to this topic, stop
    //DELETE FROM HERE TO ALLOW EVEN NON-SUBSCRIBED PROCESSES TO READ
    if (find_pid(pids, current->pid) == -1){
        pr_err("Process %d is not subscribed to %s\n", current->pid, this_file);
        return 0;
    }
    //STOP DELETION

    //Perform the reading. to_read stores the number of characters that should be read
    int to_read=MIN(size, MAX(MAX_MESSAGE_LEN - *offset,0) );

    if (*offset != MAX_MESSAGE_LEN){

        mutex_lock(&temp->endpoint_mutex);
        //Keep on reading if you have not finished
        long not_copied=copy_to_user(buffer,temp->msg, to_read);

        if (not_copied != 0){

            pr_err("Unexpected error during read operation. Abort\n");

            mutex_unlock(&temp->endpoint_mutex);

            return -EFAULT;
        }

        *offset += to_read;
        mutex_unlock(&temp->endpoint_mutex);

        return to_read;
    }
    else{

        return 0;
    }

}

static ssize_t endpoint_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){

    char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);
    pr_info("Writing message for topic %s\n", this_file);

    struct topic_subscribe* temp = NULL;

    temp=search_topic_subscribe(this_file);

    //If the topic does not exist, signal anomaly
	if ( temp == NULL ){
		pr_err("Anomaly detected! Topic not found in the system\n");
		return -EFAULT;
	}

    //The number of bytes to write
	int to_write = MIN(MAX_MESSAGE_LEN, size);

    mutex_lock(&temp->endpoint_mutex);

    reset_string(temp->msg, MAX_MESSAGE_LEN);
    long not_copied = copy_from_user(temp->msg, buffer, to_write);

    mutex_unlock(&temp->endpoint_mutex);

    if (not_copied != 0){

            pr_err("Unexpected error during write operation. Abort\n");
            mutex_unlock(&temp->endpoint_mutex);

            return -EFAULT;
        }


    mutex_lock(&temp->subscribe_mutex);

    signal_subscribers(temp->signal_nr, temp->pid_list);

    mutex_unlock(&temp->subscribe_mutex);


    return size;
}


/*##################################################
# 7)      Function that registers a new topic        #
###################################################*/

/*Adds a new topic with name topic_name.
This function takes care of creating the folder /dev/'topic_name' and populates it
with the files subscribe, subscribers_list, signal_nr and endpoint
*/
int add_new_topic(char* topic_name);

int add_new_topic(char* topic_name){
	
	pr_info("The topic %s will now be created\n", topic_name);
	
	if (cl == NULL)
		pr_err("Class %s does not exist. Aborting\n", topic_name);
		
	struct topic_subscribe* new_topic_subscribe;
	new_topic_subscribe = (struct topic_subscribe*) kmalloc(sizeof(struct topic_subscribe), GFP_KERNEL);
	new_topic_subscribe->signal_nr=-1; //Default, no signal
	new_topic_subscribe->name = (char *) kmalloc(sizeof(char)*30, GFP_KERNEL);
	strcpy(new_topic_subscribe->name, topic_name);
		
	/*Buffer containing the path of the "subscribe" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/subscribe"*/
	char topic_subscribe_path[60]="/topics/";
	strcat(topic_subscribe_path, topic_name);
	strcat(topic_subscribe_path, "/subscribe");
	
	/*Buffer containing the path of the "signal_nr" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/signal_nr"*/
	char topic_signal_path[60]="/topics/";
	strcat(topic_signal_path, topic_name);
	strcat(topic_signal_path, "/signal_nr");

    /*Buffer containing the path of the "subscribers" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/subscribers"*/
	char topic_subscribers_path[60]="/topics/";
	strcat(topic_subscribers_path, topic_name);
	strcat(topic_subscribers_path, "/subscribers");

    /*Buffer containing the path of the "subscribers" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/subscribers"*/
	char topic_endpoint_path[60]="/topics/";
	strcat(topic_endpoint_path, topic_name);
	strcat(topic_endpoint_path, "/endpoint");
	
	/*Initialize the file operations structs*/
	struct file_operations fo = {
		.read=subscribe_read,
		.open=subscribe_open,
		.write=subscribe_write,
		.release=subscribe_release
	};
	new_topic_subscribe->subscribe_fo = fo; 
	
	struct file_operations signal_fo = {
		.read=signal_nr_read,
		.open=signal_nr_open,
		.write=signal_nr_write,
		.release=signal_nr_release
	};
	new_topic_subscribe->signal_nr_fo = signal_fo;

    struct file_operations subscribers_fo = {
        .read=subscribers_read,
        .open=subscribers_open,
        .write=subscribers_write,
        .release=subscribers_release
    };
    new_topic_subscribe->subscribers_fo = subscribers_fo;

    struct file_operations endpoint_fo = {
        .read=endpoint_read,
        .open=endpoint_open,
        .write=endpoint_write,
        .release=endpoint_release
    };
    new_topic_subscribe->endpoint_fo = endpoint_fo;
	
	/*Allocating Major number*/

    allocate_chrdev(new_topic_subscribe, topic_subscribe_path, topic_signal_path, topic_subscribers_path, topic_endpoint_path);

  	pr_info("%s: Obtained Major = %d Minor = %d \n",topic_subscribe_path, MAJOR(new_topic_subscribe->subscribe_dev), MINOR(new_topic_subscribe->subscribe_dev));
  	pr_info("%s Obtained Major = %d Minor = %d \n",topic_signal_path, MAJOR(new_topic_subscribe->signal_nr_dev), MINOR(new_topic_subscribe->signal_nr_dev));
    pr_info("%s: Obtained Major = %d Minor = %d \n",topic_subscribers_path, MAJOR(new_topic_subscribe->subscribers_dev), MINOR(new_topic_subscribe->subscribers_dev));
    pr_info("%s: Obtained Major = %d Minor = %d \n",topic_endpoint_path, MAJOR(new_topic_subscribe->endpoint_dev), MINOR(new_topic_subscribe->endpoint_dev));
  
  	//Initialize the cdev structure

    init_chrdev(new_topic_subscribe);
  
  	//Add the special file to the system

    add_cdevs(new_topic_subscribe);
  	
  	//Create the special files subscribe and signal_nr

    create_devices(new_topic_subscribe, topic_subscribe_path, topic_signal_path, topic_subscribers_path, topic_endpoint_path);

    //Initialize the list of subscriber's PIDs
    new_topic_subscribe->pid_list = kmalloc(sizeof(struct list_head), GFP_KERNEL);
    INIT_LIST_HEAD(new_topic_subscribe->pid_list);

    //Initialize locks
    init_locks(new_topic_subscribe);
  	
    mutex_lock(&topic_list_mutex);

  	subscribe_data[topics_count]=new_topic_subscribe;
  	topics_count++;

    mutex_unlock(&topic_list_mutex);

  	return 0;
}


/*##################################################################################################
# 8) Functions to pass to struct file_operations newtopic_fo to register the special file newtopic #
##################################################################################################*/
static int newtopic_device_open(struct inode * inode, struct file * filp);
static int newtopic_device_release(struct inode * inode, struct file * filp);
static ssize_t newtopic_device_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t newtopic_device_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

struct file_operations newtopic_fo = {
  .read = newtopic_device_read,
  .write = newtopic_device_write,
  .open = newtopic_device_open,
  .release = newtopic_device_release
};

static int Major;
static char topic[MAX_TOPIC_NAME_LEN];
//DEFINE_SPINLOCK(newtopic_lock);
DEFINE_MUTEX(newtopic_mutex);

static int newtopic_device_open(struct inode * inode, struct file * filp){

  printk(KERN_INFO "Special file %s opened\n", NEWTOPIC_NAME);
  return 0;//OPEN_SUCCESS;
}

static int newtopic_device_release(struct inode * inode, struct file * filp){
  printk(KERN_INFO "Special file %s released\n", NEWTOPIC_NAME);
  return 0;//CLOSE_SUCCESS;
}

static ssize_t newtopic_device_read(struct file * filp, char* buffer, size_t size, loff_t * offset){
  printk(KERN_INFO "Attempt to read from special file %s, nothing done\n", NEWTOPIC_NAME);
  return 0;
}

static ssize_t newtopic_device_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){


  long not_copied;

  mutex_lock(&newtopic_mutex);

  not_copied = copy_from_user(topic, buffer, MIN(size, MAX_TOPIC_NAME_LEN - 1));

  printk(KERN_INFO "Writing to special file %s\n", NEWTOPIC_NAME);

  if (not_copied == 0){

    printk(KERN_INFO "New topic request: %s", topic);

    if (search_topic_subscribe(topic) == NULL ){
        add_new_topic(topic);
        reset_string(topic, MAX_TOPIC_NAME_LEN);

        mutex_unlock(&newtopic_mutex);
        return size;
    }
    else{

        mutex_unlock(&newtopic_mutex);
        pr_err("Topic already exists. Creation failed\n");
        return size;
    }

  }

  else{
        mutex_unlock(&newtopic_mutex);
        return -EFAULT;
  }
}

/*######################################
# 9)Module initialization and cleanup  #
######################################*/


int init_module(void){

  printk(KERN_INFO "Module succesfully loaded\n");
  
  /*Allocating Major number*/
  pr_info("Trying to allocate a major and minor number for %s device file\n", NEWTOPIC_NAME);
  
  if((alloc_chrdev_region(&newtopic_dev, 0, 1, NEWTOPIC_NAME)) < 0){
      pr_err("Cannot allocate major number for device\n");
      return -1;
  }
  
  pr_info("Obtained Major = %d Minor = %d \n",MAJOR(newtopic_dev), MINOR(newtopic_dev));
  
  //Create the class for the newtopic device file
  printk(KERN_INFO "Creating the class %s\n", FILE_CLASS);
  
  cl = class_create(NULL, FILE_CLASS);
  
  if ( cl == NULL )
  	printk(KERN_ALERT "Could not create class %s\n", FILE_CLASS);
  else
  	printk(KERN_INFO "Succesfully created class %s\n", FILE_CLASS);
  	
  //Initialize the cdev structure
  cdev_init(&newtopic_cdev, &newtopic_fo);
  
  //Add the special file to the system
  if (cdev_add(&newtopic_cdev, newtopic_dev,1) < 0)
  	pr_err("Could not add special file %s to system\n", NEWTOPIC_NAME);
  	
  //Create the special file newtopic
  if ( device_create(cl, NULL, newtopic_dev, NULL, NEWTOPIC_NAME) == NULL){
  	printk(KERN_ALERT "Could not create the special file %s\n", NEWTOPIC_NAME);
  }
  else
  	printk(KERN_INFO "Succesfully created special file %s\n", NEWTOPIC_NAME);

  printk(KERN_INFO "Special file newtopic was assigned Major %d", Major);
  return 0;
}

void cleanup_module(void){
  
  pr_info("Starting cleanup\n");
  
  //Destroying all character device files
  device_destroy(cl, newtopic_dev);
  int i,j;
  
  for(i=0; i< topics_count; ++i){
  	device_destroy(cl, subscribe_data[i]->subscribe_dev);
    device_destroy(cl, subscribe_data[i]->signal_nr_dev);
    device_destroy(cl, subscribe_data[i]->subscribers_dev);
    device_destroy(cl, subscribe_data[i]->endpoint_dev);
  }
  
  //Destroy the class AOS_PS_IPC
  class_destroy(cl);
  
  //Destroy all cdev data structures
  cdev_del(&newtopic_cdev);
  for(j=0; j< topics_count; ++j){
  	cdev_del(&subscribe_data[j]->subscribe_cdev);
    cdev_del(&subscribe_data[j]->signal_nr_cdev);
    cdev_del(&subscribe_data[j]->subscribers_cdev);
    cdev_del(&subscribe_data[j]->endpoint_cdev);
  }

  //Free all allocated memory
  pr_info("Freeing all allocated memory\n");
  for(j=0; j<topics_count;++j){
      free_memory(subscribe_data[j]);
  }
  
  printk(KERN_INFO "Module succesfully removed\n");

}
