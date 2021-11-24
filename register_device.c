#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>

#include <asm/uaccess.h>
#include "constants.h"

#define FILE_CLASS "AOS_PS_IPC"
#define NEWTOPIC_NAME "newtopic"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Davide Li Calsi");
MODULE_DESCRIPTION("Publish/Subscribe IPC mechanism");
MODULE_VERSION("0.0.1");

/*##################################################
#   Global variables to create character dev file  #
###################################################*/

static struct class* cl;
static dev_t newtopic_dev;

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
  printk(KERN_INFO "Special file opened\n");
  return 0;//OPEN_SUCCESS;
}

static int newtopic_device_release(struct inode * inode, struct file * filp){
  printk(KERN_INFO "Special file deleted\n");
  return 0;//CLOSE_SUCCESS;
}

static ssize_t newtopic_device_read(struct file * filp, char* buffer, size_t size, loff_t * offset){
  printk(KERN_INFO "Read from special file\n");
  return size;
}

static ssize_t newtopic_device_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){


  long not_copied;

  not_copied = copy_from_user(topic, buffer, size);

  printk(KERN_INFO "Writing to special file\n");

  if (not_copied == 0){

    printk(KERN_INFO "New topic request: %s", topic);
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
  	
  //Create the special file newtopic
  if ( device_create(cl, NULL, newtopic_dev, NULL, NEWTOPIC_NAME) == NULL){
  	printk(KERN_ALERT "Could not create the special file %s\n", NEWTOPIC_NAME);
  }
  else
  	printk(KERN_INFO "Succesfully created special file %s\n", NEWTOPIC_NAME);

 // Major = register_chrdev(DEFAULT_MAJOR, NEWTOPIC, &newtopic_fo);

  printk(KERN_INFO "Special file newtopic was assigned Major %d", Major);
  return 0;
}

void cleanup_module(void){
  
  pr_info("Starting cleanup\n");
  device_destroy(cl, newtopic_dev);
  class_destroy(cl);
  printk(KERN_INFO "Module succesfully removed\n");

}
