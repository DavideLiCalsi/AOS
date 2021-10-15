#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include "constants.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Davide Li Calsi");
MODULE_DESCRIPTION("Register a sample special file");
MODULE_VERSION("0.0.1");

static int device_open(struct inode * inode, struct file * filp);
static int device_release(struct inode * inode, struct file * filp);
static ssize_t device_read(struct file * filp, char* buffer, size_t size, loff_t * offset);
static ssize_t device_write(struct file * filp, const char* buffer, size_t size, loff_t * offset);

struct file_operations newtopic_fo = {
  .read = device_read,
  .write = device_write,
  .open = device_open,
  .release = device_release
};

static int Major;
static char topic[20];
static int open = 0;

static int device_open(struct inode * inode, struct file * filp){

  if (open)
    return -EBUSY;

  ++open;
  printk(KERN_INFO "Special file opened\n");
  return 0;//OPEN_SUCCESS;
}

static int device_release(struct inode * inode, struct file * filp){
  printk(KERN_INFO "Special file deleted\n");
  return 0;//CLOSE_SUCCESS;
}

static ssize_t device_read(struct file * filp, char* buffer, size_t size, loff_t * offset){
  printk(KERN_INFO "Read from special file\n");
  return size;
}

static ssize_t device_write(struct file * filp, const char* buffer, size_t size, loff_t * offset){


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

int init_module(void){

  printk(KERN_INFO "Module succesfully loaded\n");

  Major = register_chrdev(DEFAULT_MAJOR, NEWTOPIC, &newtopic_fo);

  printk(KERN_INFO "Special file succesfully created with major %d", Major);
  return 0;
}

void cleanup_module(void){

  --open;
  printk(KERN_INFO "Module succesfully removed\n");

}
