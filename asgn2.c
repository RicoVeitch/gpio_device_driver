
/**
 * File: asgn2.c
 * Date: 13/03/2011
 * Author: Rico Veitch 
 * Version: 0.1
 *
 * This is a module which serves as a virtual ramdisk which disk size is
 * limited by the amount of memory available and serves as the requirement for
 * COSC440 assignment 1 in 2012.
 *
 * Note: multiple devices and concurrent modules are not supported in this
 *       version.
 */
 
/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/sched.h>

#include "gpio.h"

#define MYDEV_NAME "asgn2"
#define MYIOC_TYPE 'k'
#define CACHE_NAME "asgn2_cache"
#define PROC_NAME "asgn2_proc"
#define BUF_SIZE 128


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rico Veitch");
MODULE_DESCRIPTION("COSC440 asgn2");


/**
 * The node structure for the memory page linked list.
 */ 
typedef struct page_node_rec {
  struct list_head list;
  struct page *page;
} page_node;

typedef struct asgn2_dev_t {
  dev_t dev;            /* the device */
  struct cdev *cdev;
  struct list_head mem_list; 
  int num_pages;        /* number of memory pages this module currently holds */
  size_t data_size;     /* total data size in this module */
  atomic_t nprocs;      /* number of processes accessing this device */ 
  atomic_t max_nprocs;  /* max number of processes accessing this device */
  struct kmem_cache *cache;      /* cache memory */
  struct class *class;     /* the udev class */
  struct device *device;   /* the udev device node */


} asgn2_dev;

asgn2_dev asgn2_device;


typedef struct circular_buffer_t {
    char buffer[BUF_SIZE];
    int head;
    int tail;
    int count;

} circular_buffer;

circular_buffer cb;

typedef struct sessions_t {
    int *ends;
    int curr_session;
    int total_sessions;

} Sessions;

Session session;

int asgn2_major = 0;                      /* major number of module */  
int asgn2_minor = 0;                      /* minor number of module */
int asgn2_dev_count = 1;                  /* number of devices */

int is_sig = 1;
char curr_sig;

module_param(asgn2_major, int, S_IWUSR|S_IRUSR);

/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(void) {

  /**
   * Loop through the entire page list {
   *   if (node has a page) {
   *     free the page
   *   }
   *   remove the node from the page list
   *   free the node
   * }
   * reset device data size, and num_pages
   */
    page_node *curr;
    page_node *temp;

    list_for_each_entry_safe(curr, temp, &(asgn2_device.mem_list), list) {
        if(curr->page != NULL) {
            __free_page(curr->page);
        }
        list_del(&curr->list);
        kmem_cache_free(asgn2_device.cache, curr); 
    }
    asgn2_device.data_size = 0;
    asgn2_device.num_pages = 0;

}


/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
int asgn2_open(struct inode *inode, struct file *filp) {

    if(atomic_read(&asgn2_device.nprocs) > atomic_read(&asgn2_device.max_nprocs)) {
        printk(KERN_ALERT "To many active processes\n");
        return -EBUSY;      

    }

    atomic_inc(&asgn2_device.nprocs);
    // if open in write only, clear all pages
    if((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        printk(KERN_ALERT "Freeing memory pages\n");
        free_memory_pages();
    }

    printk(KERN_ALERT "opening device=%s", MYDEV_NAME);

    return 0; /* success */
}


/**
 * This function releases the virtual disk, but nothing needs to be done
 * in this case. 
 */      
int asgn2_release (struct inode *inode, struct file *filp) {
  /**
   * decrement process count
   */
    atomic_dec(&asgn2_device.nprocs);
    return 0;
}


/**
 * This function reads contents of the virtual disk and writes to the user 
 */
ssize_t asgn2_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos) {
    size_t size_read = 0;     /* size read from virtual disk in this function */
    size_t begin_offset;      /* the offset from the beginning of a page to
                       start reading */
    int begin_page_no = *f_pos / PAGE_SIZE; /* the first page which contains
                             the requested data */
    int curr_page_no = 0;     /* the current page number */
    size_t curr_size_read;    /* size read from the virtual disk in this round */

    size_t size_to_be_read;   /* size to be read in the current round in 
                       while loop */

    //struct list_head *ptr = asgn2_device.mem_list.next;
    page_node *curr;
    printk(KERN_INFO "read is called, request to write %i bytes, start at page =%i\n", count, begin_page_no);

    if(*f_pos > asgn2_device.data_size) {
        printk(KERN_ERR "File pointer out of range\n");
        return 0;
    }

    if(count > asgn2_device.data_size) {
        printk(KERN_WARNING "Read request excedes device memory\n");
        count = asgn2_device.data_size;
    }
    
    begin_offset = *f_pos % PAGE_SIZE;
    printk(KERN_ALERT "begin_offset=%i\n", begin_offset);
    /*Loop through each page till we hit our first disired page. The size we want to read
      in a given iteration is the minium of the remainig (curr) page, and the total amount left to read.
      Check to se if we have failed to read (size_to_be_read) amount, if so keep going.*/
    list_for_each_entry(curr, &(asgn2_device.mem_list), list) {
        if(curr_page_no >= begin_page_no) {
            //size_to_be_read = min((int)PAGE_SIZE - begin_offset, count - size_read);
            //printk(KERN_INFO "size to be read =%i\n", size_to_be_read);
            do {
                size_to_be_read = min((int)PAGE_SIZE - begin_offset, count - size_read);
                curr_size_read = size_to_be_read - copy_to_user(buf + size_read, page_address(curr->page) + begin_offset, 
                                    size_to_be_read);
                size_read += curr_size_read;
                size_to_be_read -= curr_size_read;
                begin_offset += curr_size_read;
            } while(size_to_be_read > 0);
            begin_offset = 0;
        }
        if(count <= size_read) {
            printk(KERN_ALERT "size_read=%i", size_read);
            break;
        }
        ++curr_page_no;
    }
    *f_pos += size_read;
    printk(KERN_INFO "Read %d bytes", size_read);
    return size_read;
}

void cb_write(char data) {
    cb.head = (cb.head + 1) % BUF_SIZE;

    if(cb.count == BUF_SIZE) {
       cb.tail = (cb.tail + 1) % BUF_SIZE; 
    } else {
        cb.count++;
    }
    cb.buffer[cb.head] = data;
    printk(KERN_INFO "Inserted %c into buf, head=%i, tail=%i, count=%i", data, cb.head, cb.tail, cb.count);
}

char cb_read(void) {
    char res;
    res = cb.buffer[cb.head];
    cb.head = (cb.head + 1) % BUF_SIZE;
    cb.count--;
    return res;
}

int cb_empty(void) {
    return cb.count == 0;
}


void asgn2_write(unsigned long data) {
    size_t begin_offset; 
    struct list_head *ptr = asgn2_device.mem_list.next;
    char to_write;

    page_node *curr;
    
    printk(KERN_INFO "write called");
    while (!cb_empty()) {
        begin_offset = asgn2_device.data_size % PAGE_SIZE; 

        curr = list_entry(ptr, page_node, list);
        if(ptr == &(asgn2_device.mem_list)) {
            printk(KERN_ALERT "Adding new page to device...\n");
            if((curr = kmem_cache_alloc(asgn2_device.cache, GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "System has run out of memory\n");
                return;
            }

            if((curr->page = alloc_page(GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "System has run out of memory\n");
                return;
            }
            list_add_tail(&(curr->list), &(asgn2_device.mem_list));
            printk(KERN_ALERT  "added to tail\n");
            asgn2_device.num_pages++;
            ptr = asgn2_device.mem_list.prev;
        }
        
        to_write = cb_read();
        memcpy(page_address(curr->page) + begin_offset, &to_write, 1);
        printk(KERN_INFO "wrote %c to page list", to_write);
        asgn2_device.data_size += sizeof(to_write);
    }
    
}


void print_bin(char c) {
    int i;
    for(i = 0; i < 8; i++) {
        printk(KERN_INFO "%d\n", !!((c << i) & 0x80));
    }
}

DECLARE_TASKLET(tasklet, asgn2_write, 1);
irqreturn_t dummyport_interrupt(int irq, void *dev_id) {
    char data;
    char res;
    data = read_half_byte();
    printk(KERN_INFO "interupt made\n");
    //print_bin(data);

    if(is_sig) {
        curr_sig = data;
        is_sig = 0;
    } else {
        res = (curr_sig << 4) | data;
        cb_write(res);
        printk(KERN_INFO "combined bit=%c", res);
        is_sig = 1;
    }

    tasklet_schedule(&tasklet);
    return IRQ_HANDLED; 
}

/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
/*ssize_t asgn2_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos) {
    size_t orig_f_pos = *f_pos;  //the original file position
    size_t size_written = 0;  // size written to virtual disk in this function
    size_t begin_offset;      // the offset from the beginning of a page to start writing
    int begin_page_no = *f_pos / PAGE_SIZE;  // the first page this finction
					      //should start writing to 

    int curr_page_no = 0;     the current page number 
    size_t curr_size_written;  size written to virtual disk in this round 
    size_t size_to_be_written;  // size to be read in the current round in 
				 //while loop 
  
    struct list_head *ptr = asgn2_device.mem_list.next;
    page_node *curr;
    
    printk(KERN_INFO "Write was called, begin=%i\n", begin_page_no);

    if(orig_f_pos > asgn2_device.data_size) {
        printk(KERN_ERR "Trying to access beyond device boundary\n");
    }

    begin_offset = *f_pos % PAGE_SIZE;

    //As we could be adding pages on the fly, it is sensible not to avoid having the outer loop
      //iterating over each entry. Instead just check if we still have more to read and then
      //get the curr page entry accordinly.

    while(size_written < count) {
        curr = list_entry(ptr, page_node, list);
        
       // We either currently have no pages, or we have have run out. 
            //Anycase we need to allocate more pages to the device, so add a page to the tail of the list.
            
        if(ptr == &(asgn2_device.mem_list)) {
            printk(KERN_ALERT "Adding new page to device...\n");
            if((curr = kmem_cache_alloc(asgn2_device.cache, GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "System has run out of memory\n");
                return -1;
            }

            if((curr->page = alloc_page(GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "System has run out of memory\n");
                return -1;
            }
            list_add_tail(&(curr->list), &(asgn2_device.mem_list));
            printk(KERN_ALERT  "added to tail\n");
            asgn2_device.num_pages++;
            ptr = asgn2_device.mem_list.prev;
            continue;
        } else if(curr_page_no >= begin_page_no) {
            //size_to_be_written = min((int)PAGE_SIZE - begin_offset, count - size_written); // remaining page or
            do {
                size_to_be_written = min((int)PAGE_SIZE - begin_offset, count - size_written); // remaining page or
                curr_size_written = size_to_be_written - copy_from_user(page_address(curr->page) + begin_offset, 
                    buf + size_written, size_to_be_written);
                printk(KERN_ALERT "curr_size_written=%i\n", curr_size_written);
                size_written += curr_size_written;
                size_to_be_written -= curr_size_written;
                begin_offset += curr_size_written;
            } while(size_to_be_written > 0); // sizetoebwritten > 0
             //if we still have more pages to write, we want to resest to the offset to the 
                //beggining of the next page
            begin_offset = 0; 
        }
        // move along
        curr_page_no++;
        ptr = ptr->next;
        
    }
 
    *f_pos += size_written;
    asgn2_device.data_size = max(asgn2_device.data_size,
                               orig_f_pos + size_written);
    printk(KERN_ALERT "data_size= %i", asgn2_device.data_size);
    return size_written;
} */

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int) 

struct file_operations asgn2_fops = {
  .owner = THIS_MODULE,
  .read = asgn2_read,
  .open = asgn2_open,
  .release = asgn2_release
};


static void *my_seq_start(struct seq_file *s, loff_t *pos) {
    if(*pos >= 1) return NULL;
    else return &asgn2_dev_count + *pos;
}


static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos) {
    (*pos)++;
    if(*pos >= 1) return NULL;
    else return &asgn2_dev_count + *pos;
}


static void my_seq_stop(struct seq_file *s, void *v) {
    /* There's nothing to do here! */
}


int my_seq_show(struct seq_file *s, void *v) {
  /**
   * use seq_printf to print some info to s
   */
    seq_printf(s, "Pages=%i, data_size=%i\n", asgn2_device.num_pages, asgn2_device.data_size);
    return 0;
}


static struct seq_operations my_seq_ops = {
.start = my_seq_start,
.next = my_seq_next,
.stop = my_seq_stop,
.show = my_seq_show
};

static int my_proc_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &my_seq_ops);
}

struct file_operations asgn2_proc_ops = {
.owner = THIS_MODULE,
.open = my_proc_open,
.llseek = seq_lseek,
.read = seq_read,
.release = seq_release,
};



/**
 * Initialise the module and create the master device
 */
int __init asgn2_init_module(void){
	int result; 
 	asgn2_device.dev = MKDEV(asgn2_major, asgn2_minor);
  /**
   * set nprocs and max_nprocs of the device
   *
   * allocate major number
   * allocate cdev, and set ops and owner field 
   * add cdev
   * initialize the page list
   * create proc entries
   */
    // allocate major number
    atomic_set(&asgn2_device.nprocs, 0);
    atomic_set(&asgn2_device.max_nprocs, 1);
    if(asgn2_major) {
       // assgn static major number specified by user 
        if((register_chrdev_region(asgn2_device.dev, asgn2_dev_count, "Rico's Device")) < 0) {
            printk(KERN_INFO "Could not allocate device with user specified major number=%i", asgn2_major);
        //kfree(curr);
     
            // cant statically allocate must do it dynamically
            if ((alloc_chrdev_region(&asgn2_device.dev, asgn2_minor, asgn2_dev_count, "Rico's Device")) < 0) {
                printk(KERN_INFO "Cannot allocate char device");
                return -1;
            }
            
            asgn2_major = MAJOR(asgn2_device.dev); // need to update major number. 
        }
    }else{
        // user did not specify major number, do it dynamically
         if((alloc_chrdev_region(&asgn2_device.dev, asgn2_minor, asgn2_dev_count, "Rico's Device")) < 0) {
            printk(KERN_INFO "Cannot allocate char device");
            return -1;
        }
         asgn2_major = MAJOR(asgn2_device.dev);   
    }
    printk(KERN_INFO "Successfuly allocated major number=%i for device", asgn2_major);

    // allocate cdev
    if((asgn2_device.cdev = cdev_alloc()) == NULL) {
        printk(KERN_ERR "Unable to allocate cdev\n");
        unregister_chrdev_region(asgn2_device.dev, asgn2_dev_count);
        return -1;
    }   
    
    cdev_init(asgn2_device.cdev, &asgn2_fops); 
    asgn2_device.cdev->owner = THIS_MODULE; // set owner feild    
    
    // add cdev    
    if((cdev_add(asgn2_device.cdev, asgn2_device.dev, asgn2_dev_count)) < 0) {
        printk(KERN_ERR "Cannot add device to system\n");
        goto ur_cdev;
    }

    // initialize the head of our list
    INIT_LIST_HEAD(&(asgn2_device.mem_list));

    // create cache
    if((asgn2_device.cache = kmem_cache_create(CACHE_NAME, sizeof(page_node), 0, 
        0, NULL)) == NULL) { //SLAB_HWCACHE_ALIGN,
        printk(KERN_ERR "Cannot create cache for device\n");
        goto ur_cache;
        goto ur_cdev; 
    }


    // create struct clas
    asgn2_device.class = class_create(THIS_MODULE, MYDEV_NAME);
    if (IS_ERR(asgn2_device.class)) {
        class_destroy(asgn2_device.class);
        goto ur_cache;
        goto ur_cdev;
    }
    
    // create device file
    asgn2_device.device = device_create(asgn2_device.class, NULL, 
                                      asgn2_device.dev, "%s", MYDEV_NAME);
    if (IS_ERR(asgn2_device.device)) {
        printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
        result = PTR_ERR(asgn2_device.device);
        goto fail_device;
    }

    result = gpio_dummy_init();
    if(result != 0) {
        goto fail_device;
    }

    cb.count = 0;
    cb.head = 0;
    cb.tail = 0;

    
    proc_create(PROC_NAME, 666, NULL, &asgn2_proc_ops);
  
    printk(KERN_WARNING "set up udev entry\n");
    printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
    return 0;


ur_cache:
    list_del_init(&asgn2_device.mem_list);
    (void)kmem_cache_destroy(asgn2_device.cache);

ur_cdev:
    cdev_del(asgn2_device.cdev);
    unregister_chrdev_region(asgn2_device.dev, asgn2_dev_count);
    return -1;
 

  /* cleanup code called when any of the initialization steps fail */
fail_device:
    device_destroy(asgn2_device.class, asgn2_device.dev);
    class_destroy(asgn2_device.class);
    (void)kmem_cache_destroy(asgn2_device.cache);
    list_del_init(&asgn2_device.mem_list);
    cdev_del(asgn2_device.cdev);
    unregister_chrdev_region(asgn2_device.dev, asgn2_dev_count);
 
    return result;
}


/**
 * Finalise the module
 */
void __exit asgn2_exit_module(void){
    remove_proc_entry(PROC_NAME, NULL);
    gpio_dummy_exit();
    device_destroy(asgn2_device.class, asgn2_device.dev);
    class_destroy(asgn2_device.class);

    free_memory_pages();    
    list_del_init(&asgn2_device.mem_list);
    (void)kmem_cache_destroy(asgn2_device.cache);
    
    cdev_del(asgn2_device.cdev);
    unregister_chrdev_region(asgn2_device.dev, asgn2_dev_count);
    
    printk(KERN_WARNING "cleaned up udev entry\n");  
    printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}


module_init(asgn2_init_module);
module_exit(asgn2_exit_module);






