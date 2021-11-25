#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#define FILE_CLASS "AOS_PS_IPC"
#define NEWTOPIC_NAME "newtopic"
#define MAX_TOPICS 100

MODULE_LICENSE("GPL");

//Github access token:
//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

struct topic_subscribe{
	dev_t subscribe_dev;
	struct cdev subscribe_cdev;
	int index;
	int open_count;
	char* name;
	struct file_operations subscribe_fo;
};

/*##################################################
#   Global variables to create character dev files  #
###################################################*/

static struct class* cl;
static dev_t newtopic_dev;
static struct cdev newtopic_cdev;

static struct topic_subscribe* subscribe_data[MAX_TOPICS];

static int topics_count = 0;

static int subscribe_open(struct inode * inode, struct file * filp);
static int subscribe_release(struct inode * inode, struct file * filp);
static ssize_t subscribe_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t subscribe_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

static int subscribe_open(struct inode * inode, struct file * filp){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_name.name);

	pr_info("Opening subscription file %s\n", this_file);
	
	return 0;
}


static int subscribe_release(struct inode * inode, struct file * filp){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_name.name);

	pr_info("Releasing subscription file %s\n", this_file);
	
	return 0;
}

static ssize_t subscribe_read(struct file * filp, char* buffer, size_t size, loff_t * offset){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_name.name);

	pr_info("Reading subscription file %s\n", this_file);
	
	return 0;
}

static ssize_t subscribe_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){

	//Get the file name of this special file
	char this_file[50];
	strcpy(this_file, filp->f_path.dentry->d_name.name);

	pr_info("Writing subscription file %s\n", this_file);
	
	return 0;
}


/*##################################################
#   Utility functions for better code readability  #
###################################################*/
int add_new_topic(char* topic_name);

int add_new_topic(char* topic_name){
	
	pr_info("The topic %s will now be created\n", topic_name);
	
	if (cl == NULL)
		pr_err("Class %s does not exist. Aborting\n", topic_name);
		
	struct topic_subscribe* new_topic_subscribe;
	new_topic_subscribe = (struct topic_subscribe*) kmalloc(sizeof(struct topic_subscribe), GFP_KERNEL);
		
	/*Buffer containing the path of the "subscribe" special file for the
	requested topic, e.g if topic_name = "news", topic_subscribe="/dev/topics/news/subscribe"*/
	char topic_subscribe_path[60]="";
	strcat(topic_subscribe_path, "/topics/");
	strcat(topic_subscribe_path, topic_name);
	strcat(topic_subscribe_path, "/subscribe");
	
	/*Initialize the file operations struct*/
	struct file_operations fo = {
		.read=subscribe_read,
		.open=subscribe_open,
		.write=subscribe_write,
		.release=subscribe_release
	};
	new_topic_subscribe->subscribe_fo = fo; 
	
	/*Allocating Major number*/
  	pr_info("Trying to allocate a major and minor number for %s device file\n", topic_subscribe_path);
  
  	if( (alloc_chrdev_region(&new_topic_subscribe->subscribe_dev, 0, 1, topic_subscribe_path)) < 0){
      		pr_err("Cannot allocate major number for device\n");
      		return -1;
  	}
  
  	pr_info("Obtained Major = %d Minor = %d \n",MAJOR(newtopic_dev), MINOR(newtopic_dev));
  
  
  	//Initialize the cdev structure
  	cdev_init(&new_topic_subscribe->subscribe_cdev, &new_topic_subscribe->subscribe_fo);
  
  	//Add the special file to the system
  	if (cdev_add(&new_topic_subscribe->subscribe_cdev, new_topic_subscribe->subscribe_dev,1) < 0)
  		pr_err("Could not add special file %s to system\n", topic_subscribe_path);
  	
  	//Create the special file newtopic
  	if ( device_create(cl, NULL, new_topic_subscribe->subscribe_dev, NULL, topic_subscribe_path) == NULL){
  		printk(KERN_ALERT "Could not create the special file %s\n", topic_subscribe_path);
  	}
  	else
  		printk(KERN_INFO "Succesfully created special file %s\n", topic_subscribe_path);

 	// Major = register_chrdev(DEFAULT_MAJOR, NEWTOPIC_NAME, &newtopic_fo);

  	//printk(KERN_INFO "Special file %s was assigned Major %d",topic_subscribe_path, Major);
  	
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
  device_destroy(cl, newtopic_dev);
  int i,j;
  
  for(i=0; i< topics_count; ++i){
  	device_destroy(cl, subscribe_data[i]->subscribe_dev);
  }
  class_destroy(cl);
  cdev_del(&newtopic_cdev);
  for(j=0; j< topics_count; ++j){
  	cdev_del(&subscribe_data[j]->subscribe_cdev);
  }
  printk(KERN_INFO "Module succesfully removed\n");

}
