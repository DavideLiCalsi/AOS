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

#include <asm/uaccess.h>

#define FILE_CLASS "AOS_PS_IPC"
#define NEWTOPIC_NAME "newtopic"
#define MAX_TOPICS 100
#define MAX_SIG 30
#define MAX_TOPIC_LEN 50

MODULE_LICENSE("GPL");

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
	
    //Index of this topic
	int index;
	int open_count;

    //Current signal number
	int signal_nr;

    //Topic name
	char* name;

    //subscribed PIDs list
    struct list_head* pid_list;

    //file_operations structs
	struct file_operations subscribe_fo;
	struct file_operations signal_nr_fo;
    struct file_operations subscribers_fo;
};

static struct class* cl;
static dev_t newtopic_dev;
static struct cdev newtopic_cdev;

static struct topic_subscribe* subscribe_data[MAX_TOPICS];

static int topics_count = 0;

/*##################################################
#       Utility functions to ease development      #
###################################################*/

/*Search a topic by name*/
struct topic_subscribe* search_topic_subscribe(char* name){

    int i;

    for(i=0; i<topics_count; ++i){

        if (!strcmp(name, subscribe_data[i]->name) )
            return subscribe_data[i];
    }

    return NULL;
}

/*Convert a buffer of chars to an int. The function converts only
 the first sizeof(int) chars*/
/*int charbuf_to_int(char* buf){

    int res = 0;

    //If you don't have at least 4 chars, abort
    if( strlen(buf) < sizeof(int))
        return -1;

    int i;
    int factor = 1;

    for(i=0;i<sizeof(int); ++i, factor*=256){

        res += ( (int) buf[i] ) * factor;
    }

    return res;

}*/

void send_signal(int signal_nr, struct task_struct* task){

    struct siginfo siginfo;
    siginfo.si_signo = signal_nr;
    siginfo.si_code = SI_QUEUE;
    siginfo.si_int = 1;

    if ( send_sig_info(signal_nr, &siginfo,task ) < 0 )
        pr_err("Could not signal to process %d", pid);

}



/*##########################################################################################
#   Functions to pass to struct file_operations to manage the subscribe special file(s)  #
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
	
	return 0;
}


static int subscribe_release(struct inode * inode, struct file * filp){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_parent->d_name.name);

	pr_info("Releasing subscription file for topic %s\n", this_file);
	
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
    int pid = current->pid;

    //First retrieve the pid of the process writing to this file


    //Now create the struct pid_node to add the list of subscribers
    struct pid_node* new = kmalloc(sizeof(struct pid_node), GFP_KERNEL);
    //new->list = kmalloc(sizeof(struct list_head), GFP_KERNEL);

    //Fault if new is null
    if (new == NULL)
        return -EFAULT;

    new->pid = pid;

    //Retrieve the pid_list pointer in order to add the new pid to the linked list

    struct topic_subscribe* this_topic_subscribe = search_topic_subscribe( this_file);

    if( this_topic_subscribe == NULL) return -EFAULT;
    struct list_head* this_list = this_topic_subscribe->pid_list;

    //Perform the add
    list_add_tail(&new->list, this_list);

    pr_info("Process %d has succesfully subscribed!\n", pid);

    return sizeof(int);
}

/*##########################################################################################
#   Functions to pass to struct file_operations to manage the signal_nr special file(s)  #
###########################################################################################*/

static int signal_nr_open(struct inode * inode, struct file * filp);
static int signal_nr_release(struct inode * inode, struct file * filp);
static ssize_t signal_nr_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t signal_nr_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

static int signal_nr_open(struct inode * inode, struct file * filp){

	pr_info("Opening signal_nr file\n");
	return 0;
}

static int signal_nr_release(struct inode * inode, struct file * filp){

	pr_info("Releasing signal_nr file\n");
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
	
	//Convert the signal number to string
	char signal_as_string[5] = "";
	//sprintf(signal_as_string,"%d", signal_code);
    signal_as_string[0]=(char) signal_code;
	
	//Copy it to buffer
	int error_count = 0;
	
	error_count = copy_to_user(buffer, signal_as_string, 1);
	
	pr_info("The signal received for topic %s is %d\n", this_file, signal_code);
	
	return size;
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
	char signal_as_string[5];
    int signal_nr;
	
	not_copied = copy_from_user(signal_as_string, buffer, 1);
    signal_as_string[1]='\0';
    signal_nr = (int) signal_as_string[0];
	
	pr_info("Provided signal code: %d\n", signal_nr);
	
	
    if( signal_nr <= MAX_SIG)
        temp->signal_nr = signal_nr;
    else
        pr_err("Signal number is too high. Overwriting denied. \n");
	
	return size;
}

/*##########################################################################################
#   Functions to pass to struct file_operations to manage the subscribers special file(s)  #
###########################################################################################*/

static int subscribers_open(struct inode * inode, struct file * filp);
static int subscribers_release(struct inode * inode, struct file * filp);
static ssize_t subscribers_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t subscribers_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

static int subscribers_open(struct inode * inode, struct file * filp){
    pr_info("Opening subscribers file\n");
    return 0;
}

static int subscribers_release(struct inode * inode, struct file * filp){
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

    list_for_each(cursor,pids){

        struct pid_node* sub_process = list_entry(cursor,struct pid_node,list);
        pr_info("%d\n", sub_process->pid);
    }


    return 0;
}

static ssize_t subscribers_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){
    pr_info("Writing to subscribers file has no effect\n");
    return 0;
}


/*##################################################
#       Function that registers a new topic        #
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
	new_topic_subscribe->index=topics_count;
	new_topic_subscribe->open_count=0;
	new_topic_subscribe->signal_nr=-1; //Default, no signal
	new_topic_subscribe->name = (char *) kmalloc(sizeof(char)*30, GFP_KERNEL);
	strcpy(new_topic_subscribe->name, topic_name);
		
	/*Buffer containing the path of the "subscribe" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/subscribe"*/
	char topic_subscribe_path[60]="";
	strcat(topic_subscribe_path, "/topics/");
	strcat(topic_subscribe_path, topic_name);
	strcat(topic_subscribe_path, "/subscribe");
	
	/*Buffer containing the path of the "signal_nr" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/signal_nr"*/
	char topic_signal_path[60]="";
	strcat(topic_signal_path, "/topics/");
	strcat(topic_signal_path, topic_name);
	strcat(topic_signal_path, "/signal_nr");

    /*Buffer containing the path of the "subscribers" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/subscribers"*/
	char topic_subscribers_path[60]="";
	strcat(topic_subscribers_path, "/topics/");
	strcat(topic_subscribers_path, topic_name);
	strcat(topic_subscribers_path, "/subscribers");
	
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
	
	/*Allocating Major number*/
  	pr_info("Trying to allocate a major and minor number for %s device file\n", topic_subscribe_path);
  	pr_info("Trying to allocate a major and minor number for %s device file\n", topic_signal_path);
    pr_info("Trying to allocate a major and minor number for %s device file\n", topic_subscribers_path);
  
  	if( (alloc_chrdev_region(&new_topic_subscribe->signal_nr_dev, 0, 1, topic_signal_path)) < 0 || 
  		(alloc_chrdev_region(&new_topic_subscribe->subscribe_dev, 0, 1, topic_subscribe_path)) < 0 ||
  		(alloc_chrdev_region(&new_topic_subscribe->subscribers_dev, 0, 1, topic_subscribers_path)) < 0){
  		
      		pr_err("Cannot allocate major numbers for devices\n");
      		return -1;
  	}
  
  	pr_info("%s: Obtained Major = %d Minor = %d \n",topic_subscribe_path, MAJOR(new_topic_subscribe->subscribe_dev), MINOR(new_topic_subscribe->subscribe_dev));
  	pr_info("%s Obtained Major = %d Minor = %d \n",topic_signal_path, MAJOR(new_topic_subscribe->signal_nr_dev), MINOR(new_topic_subscribe->signal_nr_dev));
    pr_info("%s: Obtained Major = %d Minor = %d \n",topic_subscribers_path, MAJOR(new_topic_subscribe->subscribers_dev), MINOR(new_topic_subscribe->subscribers_dev));
  
  	//Initialize the cdev structure
  	cdev_init(&new_topic_subscribe->subscribe_cdev, &new_topic_subscribe->subscribe_fo);
  	cdev_init(&new_topic_subscribe->signal_nr_cdev, &new_topic_subscribe->signal_nr_fo);
    cdev_init(&new_topic_subscribe->subscribers_cdev, &new_topic_subscribe->subscribers_fo);
  
  	//Add the special file to the system
  	if (cdev_add(&new_topic_subscribe->subscribe_cdev, new_topic_subscribe->subscribe_dev,1) < 0 ||
  		cdev_add(&new_topic_subscribe->signal_nr_cdev, new_topic_subscribe->signal_nr_dev,1) < 0  ||
        cdev_add(&new_topic_subscribe->subscribers_cdev, new_topic_subscribe->subscribers_dev,1) < 0 )
  		
  		pr_err("Could not add special files to system\n");
  	
  	//Create the special files subscribe and signal_nr
  	if ( device_create(cl, NULL, new_topic_subscribe->subscribe_dev, NULL, topic_subscribe_path) == NULL ||
  		device_create(cl, NULL, new_topic_subscribe->signal_nr_dev, NULL, topic_signal_path) == NULL ||
        device_create(cl, NULL, new_topic_subscribe->subscribers_dev, NULL, topic_subscribers_path) == NULL ){
  		printk(KERN_ALERT "Could not create one of the special files\n");
  	}
  	else
  		printk(KERN_INFO "Succesfully created special files\n");

    //Initialize the list of subscriber's PIDs
    new_topic_subscribe->pid_list = kmalloc(sizeof(struct list_head), GFP_KERNEL);
    INIT_LIST_HEAD(new_topic_subscribe->pid_list);
  	
  	subscribe_data[topics_count]=new_topic_subscribe;
  	
  	topics_count++;
  	return 0;
}


/*##################################################################################################
#   Functions to pass to struct file_operations newtopic_fo to register the special file newtopic  #
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
static char topic[20];
static int open = 0;

static int newtopic_device_open(struct inode * inode, struct file * filp){

  if (open)
    return -EBUSY;

  ++open;
  printk(KERN_INFO "Special file %s opened\n", NEWTOPIC_NAME);
  return 0;//OPEN_SUCCESS;
}

static int newtopic_device_release(struct inode * inode, struct file * filp){
  printk(KERN_INFO "Special file %s released\n", NEWTOPIC_NAME);
  --open;
  return 0;//CLOSE_SUCCESS;
}

static ssize_t newtopic_device_read(struct file * filp, char* buffer, size_t size, loff_t * offset){
  printk(KERN_INFO "Attempt to read from special file %s, nothing done\n", NEWTOPIC_NAME);
  return 0;
}

static ssize_t newtopic_device_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){


  long not_copied;

  not_copied = copy_from_user(topic, buffer, size);

  printk(KERN_INFO "Writing to special file %s\n", NEWTOPIC_NAME);

  if (not_copied == 0){

    printk(KERN_INFO "New topic request: %s", topic);
    add_new_topic(topic);
    return size;
  }
  else
    return -EFAULT;
}

/*######################################
#   Module initialization and cleanup  #
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

 // Major = register_chrdev(DEFAULT_MAJOR, NEWTOPIC_NAME, &newtopic_fo);

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
  }
  
  //Destroy the class AOS_PS_IPC
  class_destroy(cl);
  
  //Destroy all cdev data structures
  cdev_del(&newtopic_cdev);
  for(j=0; j< topics_count; ++j){
  	cdev_del(&subscribe_data[j]->subscribe_cdev);
    cdev_del(&subscribe_data[j]->signal_nr_cdev);
    cdev_del(&subscribe_data[j]->subscribers_cdev);
  }
  
  printk(KERN_INFO "Module succesfully removed\n");

}
