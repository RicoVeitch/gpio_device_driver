
/**
 * File: asgn1.c
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

#define MYDEV_NAME "asgn1"
#define MYIOC_TYPE 'k'
#define CACHE_NAME "asgn1_cache"
#define PROC_NAME "asgn1_proc"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rico Veitch");
MODULE_DESCRIPTION("COSC440 asgn1");


/**
 * The node structure for the memory page linked list.
 */ 
typedef struct page_node_rec {
  struct list_head list;
  struct page *page;
} page_node;

typedef struct asgn1_dev_t {
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
} asgn1_dev;

asgn1_dev asgn1_device;


int asgn1_major = 0;                      /* major number of module */  
int asgn1_minor = 0;                      /* minor number of module */
int asgn1_dev_count = 1;                  /* number of devices */

module_param(asgn1_major, int, S_IWUSR|S_IRUSR);

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

    list_for_each_entry_safe(curr, temp, &(asgn1_device.mem_list), list) {
        if(curr->page != NULL) {
            __free_page(curr->page);
        }
        list_del(&curr->list);
        kmem_cache_free(asgn1_device.cache, curr); 
    }
    asgn1_device.num_pages = 0;

}


/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
int asgn1_open(struct inode *inode, struct file *filp) {
  /**
   * Increment process count, if exceeds max_nprocs, return -EBUSY
   *
   * if opened in write-only mode, free all memory pages
   *
   */
    if(atomic_read(&asgn1_device.nprocs) >= atomic_read(&asgn1_device.max_nprocs)) {
        printk(KERN_ERR "To many active processes\n");
        return -EBUSY;      

    }

    atomic_inc(&asgn1_device.nprocs);
    if(filp->f_mode == FMODE_WRITE) {
        free_memory_pages();
    }

    return 0; /* success */
}


/**
 * This function releases the virtual disk, but nothing needs to be done
 * in this case. 
 */      
int asgn1_release (struct inode *inode, struct file *filp) {
  /**
   * decrement process count
   */
    atomic_dec(&asgn1_device.nprocs);
    return 0;
}


/**
 * This function reads contents of the virtual disk and writes to the user 
 */
ssize_t asgn1_read(struct file *filp, char __user *buf, size_t count,
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

    struct list_head *ptr = asgn1_device.mem_list.next;
    page_node *curr;
    printk(KERN_INFO "read was called");
  /**
   * check f_pos, if beyond data_size, return 0
   * 
   * Traverse the list, once the first requested page is reached,
   *   - use copy_to_user to copy the data to the user-space buf page by page
   *   - you also need to work out the start / end offset within a page
   *   - Also needs to handle the situation where copy_to_user copy less
   *       data than requested, and
   *       copy_to_user should be called again to copy the rest of the
   *       unprocessed data, and the second and subsequent calls still
   *       need to check whether copy_to_user copies all data requested.
   *       This is best done by a while / do-while loop.
   *
   * if end of data area of ramdisk reached before copying the requested
   *   return the size copied to the user space so far
   */

    return size_read;
}




static loff_t asgn1_lseek (struct file *file, loff_t offset, int cmd)
{
    loff_t testpos;

    size_t buffer_size = asgn1_device.num_pages * PAGE_SIZE;

    /**
     * set testpos according to the command
     *
     * if testpos larger than buffer_size, set testpos to buffer_size
     * 
     * if testpos smaller than 0, set testpos to 0
     *
     * set file->f_pos to testpos
     */

    switch(cmd){
        case SEEK_SET:
            break;
        case SEEK_CUR:
            break;
        case SEEK_END:
            break;
    }

    if(testpos > buffer_size) {
        testpos = buffer_size;
    } else if(testpos < 0) {
        testpos = 0;
    }
    
    file->f_pos = testpos;    

    printk (KERN_INFO "Seeking to pos=%ld\n", (long)testpos);
    return testpos;
}


/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
ssize_t asgn1_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos) {
  size_t orig_f_pos = *f_pos;  /* the original file position */
  size_t size_written = 0;  /* size written to virtual disk in this function */
  size_t begin_offset;      /* the offset from the beginning of a page to
			       start writing */
  int begin_page_no = *f_pos / PAGE_SIZE;  /* the first page this finction
					      should start writing to */

  int curr_page_no = 0;     /* the current page number */
  size_t curr_size_written; /* size written to virtual disk in this round */
  size_t size_to_be_written;  /* size to be read in the current round in 
				 while loop */
  
  struct list_head *ptr = asgn1_device.mem_list.next;
  page_node *curr;
    printk(KERN_INFO "Write was called\n");
  /**
   * Traverse the list until the first page reached, and add nodes if necessary
   *
   * Then write the data page by page, remember to handle the situation
   *   when copy_from_user() writes less than the amount you requested.
   *   a while loop / do-while loop is recommended to handle this situation. 
   */


  asgn1_device.data_size = max(asgn1_device.data_size,
                               orig_f_pos + size_written);
  return size_written;
}

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int) 

/**
 * The ioctl function, which nothing needs to be done in this case.
 */
long asgn1_ioctl (struct file *filp, unsigned cmd, unsigned long arg) {
  int nr;
  int new_nprocs;
  int result;

  /** 
   * check whether cmd is for our device, if not for us, return -EINVAL 
   *
   * get command, and if command is SET_NPROC_OP, then get the data, and
     set max_nprocs accordingly, don't forget to check validity of the 
     value before setting max_nprocs
   */

    if(_IOC_TYPE(cmd) != MYIOC_TYPE) return -EINVAL;

    if(_IOC_TYPE(cmd) == SET_NPROC_OP){

        if(get_user(new_nprocs, (int*)&arg) < 0){
            printk(KERN_ERR "could not read users arg\n");
            return -1;
        }
        
        if(atomic_read(&asgn1_device.nprocs) < new_nprocs || new_nprocs < 1){
            printk(KERN_ERR "Invalid number for max_nprocs\n");
            return -EINVAL;
        }

        atomic_set(&asgn1_device.max_nprocs, new_nprocs);
        return 0;
    }
    printk(KERN_ERR "Invalid command:%d, this device only accepts cmd=%d", _IOC_TYPE(cmd), SET_NPROC_OP);
    return -ENOTTY;
}


static int asgn1_mmap (struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long len = vma->vm_end - vma->vm_start;
    unsigned long ramdisk_size = asgn1_device.num_pages * PAGE_SIZE;
    page_node *curr;
    unsigned long index = 0;

    /**
     * check offset and len
     *
     * loop through the entire page list, once the first requested page
     *   reached, add each page with remap_pfn_range one by one
     *   up to the last requested page
     */
    if(offset > ramdisk_size || len + offset > ramdisk_size) {
        printk(KERN_ERR "Provided address goes pass avaible device memory\n");
        return -EAGAIN;
    }else if(len % PAGE_SIZE != 0) {
        printk(KERN_ERR "Length must be a factor of PAGE_SIZE\n");
        return -EAGAIN;
    }else if(offset % PAGE_SIZE != 0) {
        printk(KERN_ERR "Offset must be a factor of PAGE_SIZE\n");
        return -EAGAIN;
    }

    list_for_each_entry(curr, &asgn1_device.mem_list, list) {
        if(index >= vma->vm_pgoff) {
            pfn = page_to_pfn(curr->page);
            if(remap_pfn_range(vma, vma->vm_start + (PAGE_SIZE*index), pfn, PAGE_SIZE, vma->vm_page_prot)) {
                return -EAGAIN;
            }
        }

        ++index;
        if(len <= (PAGE_SIZE * (index - vma->vm_pgoff))) { // finished
            return 0;
        }
    }
    return 0;
}


struct file_operations asgn1_fops = {
  .owner = THIS_MODULE,
  .read = asgn1_read,
  .write = asgn1_write,
  .unlocked_ioctl = asgn1_ioctl,
  .open = asgn1_open,
  .mmap = asgn1_mmap,
  .release = asgn1_release,
  .llseek = asgn1_lseek
};


static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
if(*pos >= 1) return NULL;
else return &asgn1_dev_count + *pos;
}
static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
(*pos)++;
if(*pos >= 1) return NULL;
else return &asgn1_dev_count + *pos;
}
static void my_seq_stop(struct seq_file *s, void *v)
{
/* There's nothing to do here! */
}

int my_seq_show(struct seq_file *s, void *v) {
  /**
   * use seq_printf to print some info to s
   */
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

struct file_operations asgn1_proc_ops = {
.owner = THIS_MODULE,
.open = my_proc_open,
.llseek = seq_lseek,
.read = seq_read,
.release = seq_release,
};



/**
 * Initialise the module and create the master device
 */
int __init asgn1_init_module(void){
	int result; 
 	asgn1_device.dev = MKDEV(asgn1_major, asgn1_minor);
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
    atomic_set(&asgn1_device.nprocs, 0);
    atomic_set(&asgn1_device.max_nprocs, 1);
    if(asgn1_major){
       // assgn static major number specified by user 
        if((register_chrdev_region(asgn1_device.dev, asgn1_dev_count, "Rico's Device")) < 0){
            printk(KERN_INFO "Cannot allocate device with user specified major number=%i", asgn1_major);
     
            // cant statically allocate must do it dynamically
            if ((alloc_chrdev_region(&asgn1_device.dev, asgn1_minor, asgn1_dev_count, "Rico's Device")) < 0){
                printk(KERN_INFO "Cannot allocate char device");
                return -1;
            }
            
            asgn1_major = MAJOR(asgn1_device.dev); 
        }
    }else{
        // user did not specify major number, do it dynamically
         if((alloc_chrdev_region(&asgn1_device.dev, asgn1_minor, asgn1_dev_count, "Rico's Device")) < 0){
            printk(KERN_INFO "Cannot allocate char device");
            return -1;
        }
         asgn1_major = MAJOR(asgn1_device.dev);   
    }
    printk(KERN_INFO "Successfuly allocated major number=%i for device", asgn1_major);

    // allocate cdev
    if((asgn1_device.cdev = cdev_alloc()) == NULL){
        printk(KERN_ERR "Unable to allocate cdev\n");
        goto ur_chrdev;
    }   
    
    cdev_init(asgn1_device.cdev, &asgn1_fops); 
    asgn1_device.cdev->owner = THIS_MODULE; // set owner feild    
    // add cdev    
    if((cdev_add(asgn1_device.cdev, asgn1_device.dev, asgn1_dev_count)) < 0){
        printk(KERN_ERR "Cannot add device to system\n");
        goto ur_chrdev;
    }


    // create cache
    if((asgn1_device.cache = kmem_cache_create(CACHE_NAME, sizeof(page_node), 0, SLAB_HWCACHE_ALIGN, NULL)) == NULL){
        printk(KERN_ERR "Cannot create cache for device\n");
        goto ur_chrdev; 
    }


    // create struct clas
    asgn1_device.class = class_create(THIS_MODULE, MYDEV_NAME);
    if (IS_ERR(asgn1_device.class)) {
        goto ur_chrdev;
    }
    
    // create device file
    asgn1_device.device = device_create(asgn1_device.class, NULL, 
                                      asgn1_device.dev, "%s", MYDEV_NAME);
    if (IS_ERR(asgn1_device.device)) {
        printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
        result = PTR_ERR(asgn1_device.device);
        goto fail_device;
    }

    
    proc_create(PROC_NAME, 666, NULL, &asgn1_proc_ops);
  
  printk(KERN_WARNING "set up udev entry\n");
  printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
  return 0;


ur_chrdev:
    unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);
    return -1;

  /* cleanup code called when any of the initialization steps fail */
fail_device:
    class_destroy(asgn1_device.class);
    unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);
 
  /* PLEASE PUT YOUR CLEANUP CODE HERE, IN REVERSE ORDER OF ALLOCATION */

    return result;
}


/**
 * Finalise the module
 */
void __exit asgn1_exit_module(void){
    remove_proc_entry(PROC_NAME, NULL);
    device_destroy(asgn1_device.class, asgn1_device.dev);
    class_destroy(asgn1_device.class);

    free_memory_pages();    
    (void)kmem_cache_destroy(asgn1_device.cache);

    cdev_del(asgn1_device.cdev);
    unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);
    printk(KERN_WARNING "cleaned up udev entry\n");
  
  /* COMPLETE ME */
  /**
   * free all pages in the page list 
   * cleanup in reverse order
   */
    printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}


module_init(asgn1_init_module);
module_exit(asgn1_exit_module);


