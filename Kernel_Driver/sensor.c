#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/version.h>
#include<linux/interrupt.h>
#include<linux/delay.h>
#include<linux/kthread.h>
#include<linux/gpio.h>
#include<linux/slab.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/fs.h>
#include<linux/math64.h>
#include<linux/spinlock.h>

#define CLASS_NAME "HCSR"
#define DEVICE_NAME "hcsr"

/*Structures representing sensor device class,
to hold the major& minor number corresponding to device,
*/
static struct class *hcsr_class;
static dev_t hcsr_dev_num;

/*@getCurrentTimestamp: Returns the current timestamp when requested
 * inline assembly function to return time-stamp counter value (64bit)
 * gives no.of cycles since boot-time till the triggered point
 */
static __inline__ unsigned long long getCurrentTimestamp(void) {
    uint32_t low, high;
    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t) (low) | (uint64_t) (high) << 32);
}


typedef struct hcsr04_device {
    /*embedded cdev to represent the device as character file*/
    struct cdev hcsr_cdev;
    char *name;
    /*lock for sync between interrupt and driver functions*/
    spinlock_t spin;
    /*flags to indicate if operation is going on,to indicate rising or falling edge & echo pin irq number*/
    int ongoing_sampling, edge_flag, echo_irq;
    /*To get the timestamps at rising,falling and interval*/
    unsigned long long start_time, end_time, pulse_width; 
    /*task struct representing the kernel thread which makes the triggering of pulses*/
    struct task_struct *triggerThread; 
    
} hcsr04_device_t;

/*A pointer to the driver data structure*/
hcsr04_device_t *hcsr_dev;

/*
 * Thread to send trig_pin pulse continuously until stopped by
 * write call by passing '0' from user space.
 */
static int trigger_signal(void *data) {
    
    hcsr04_device_t *hcsr_dev = (hcsr04_device_t *) data;
    printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);
    

    while (!kthread_should_stop()) {

        gpio_set_value_cansleep(1, 1);/*set the trig_pin pin*/ 
        udelay(10); /*keep trig_pin pin high for atleast 10 microseconds*/
        gpio_set_value_cansleep(1, 0); /*unset the trig_pin pin*/
        msleep(100);  /*sleep for 100 milliseconds before another pulse is sent*/
    }
    hcsr_dev->triggerThread=NULL;
    return 0;
}

/*@irq_handler: Registered interrupt to handle the echo response.
 * It gets the timestamp & calculates the pulse width in terms
 * of microseconds to store it in sensor driver data structure
 */
static irqreturn_t irq_handler(int irq, void *dev_id) {
    hcsr04_device_t *hcsr_dev = dev_id;
    unsigned long long pulse_diff = 0;
     /*condition to check if the rising edge has been detected*/
    if (hcsr_dev->edge_flag == 0) {
        hcsr_dev->start_time = getCurrentTimestamp(); /*condition to check if the rising edge has been detected*/
        hcsr_dev->edge_flag = 1; /*flag indicating rising edge*/
        hcsr_dev->ongoing_sampling = 1; /*flag set to indicate a sampling is going on*/
        irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING); /*after rising edge detection, make the interrupt trigger for next falling edge*/
    } else {

        hcsr_dev->end_time = getCurrentTimestamp(); /*get timestamp at falling edge*/
        hcsr_dev->edge_flag = 0; /*falling edge is detected*/
        hcsr_dev->ongoing_sampling = 0; /*next sampling can be initiated as the echo pulse is retrieved*/
        irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING); /*Set it again to rising edge interrupt trig_pin for the next cycle*/
        

        pulse_diff = (hcsr_dev->end_time - hcsr_dev->start_time);/*difference between the start and end timestamp proportional to the
        time taken by sound wave to return*/
         do_div(pulse_diff, 400); /*dividing the difference by clock speed 400MHz to get the time in microseconds*/
        spin_lock(&hcsr_dev->spin);/*locking before the latest measured distance is updated*/
        hcsr_dev->pulse_width=pulse_diff;/*storing the time proportional to distance in data structure*/
        spin_unlock(&hcsr_dev->spin);/*unlocking after the latest measured distance is updated*/
        
        
        }

    
    return IRQ_HANDLED;
}

/*
Read function, invoked when a read is made on the device file
@params:   file, buffer, count, pointer position
@function: To retrieve a distance measure (an integer in microseconds) saved in the data structure*/

static ssize_t hcsr_device_read(struct file *file, char __user *buf, size_t count, loff_t *f_ps) {
    int ret = 0;
    hcsr04_device_t *device = file->private_data;
   
    printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);
    if (device == NULL) {
        printk(KERN_ALERT "Read: NO device structure available..\n");
        printk(KERN_ALERT "check pointer number 1 in read operation");
        
        return -1;
    }

    
    /*check if ongoing sampling is going on ,if yes
    then return -1 indicating there is a fresh trigger pulse sent
    and its corresponding echo isnt received*/    
    if(device->ongoing_sampling)
        return -1;
    else{
    
        /*if there isnt any ongoing sampling ,acheive lock
        as data shared between interrupt handler and 
        get the distance measured in microseconds*/
        spin_lock(&device->spin);
        /*data being copied to user space buffer*/
        ret=copy_to_user((unsigned long long *)buf,&device->pulse_width,sizeof(unsigned long long ));
        if(ret!=0)
            {
                printk(KERN_INFO "Error while copying buffer to user space\n");
                return -1;
            }    
        /*lock being released*/
        spin_unlock(&device->spin);
        if(ret<0)
            return -EINVAL;
        }
    return 0;
}

/*
Write function
@params:   file, buffer, count, pointer position
@function:  The argument of the write call is an integer. It stops triggering action
if the argument is 0 else if it is 1 starts a new triggering cycle*/
 
static ssize_t hcsr_device_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {
    int write_flag = 0, ret = 0;
    hcsr04_device_t *device = file->private_data;

    printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);
    if (device == NULL) {
        printk(KERN_ALERT "Write: NO device structure available..\n");
        return -1;
    }
    /*copies the data to kernel space in order to operate*/
    ret = copy_from_user(&write_flag, (int *) buf, sizeof (int));
    if(ret!=0)
            {
                printk(KERN_INFO "Error while copying buffer from user space\n");
                return -1;
            }  
    
    /*if flag is zero, then stop the triggering action*/
    if (write_flag == 0) {
       if (device->triggerThread != NULL) {
                kthread_stop(device->triggerThread);
                device->triggerThread = NULL;
               
        
    }
    return 0;
   }
   
     /*if write flag is set but  if there is some sampling going on return error value*/
    if (device->ongoing_sampling) {
        return -EINVAL;
    }

    if (write_flag) {
        if (device->ongoing_sampling == 0) {
            /*stop the thread if it is running, while trying to initiate a new cycle*/
            if (device->triggerThread != NULL) {
                kthread_stop(device->triggerThread);
                device->triggerThread = NULL;
               
            }
            /*run the thread for a new cycle*/
            device->triggerThread = kthread_run(&trigger_signal, device, "Trigger thread"); 
        }
    }

    return 0;
}

/*invoked when an open function is called on this device file*/
static int hcsr_device_open(struct inode *inode, struct file *file) {
    /*gets the reference to data structure w.r.t inode structure*/
    hcsr04_device_t *device =container_of(inode->i_cdev,hcsr04_device_t,hcsr_cdev);
    file->private_data = device; /*stores the pointer to device specific structure in private field of it's corresponding file structure, to retreive it later*/
    printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);
    if (device != NULL) {
        printk(KERN_ALERT "Opened the device file %s\n", device->name);
    } else {
        printk(KERN_ALERT "Device is NOT present \n");
    }
    return 0;
}

/*Invoked when the device file is being closed*/
static int hcsr_device_release(struct inode *inode, struct file *file) {
    hcsr04_device_t *device = file->private_data;
    printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);
    /*stop the thread if it is running, while trying to close the device file*/
     if (device->triggerThread != NULL) {
                kthread_stop(device->triggerThread);
                device->triggerThread = NULL;
               
            }
    printk(KERN_ALERT "RELEASE: released the device file %s \n", device->name);
    return 0;
}

/*Linking the implemented file operations with the
sensor character device file being created*/
static const struct file_operations hcsr_fops = {
    .owner = THIS_MODULE,
    .open = hcsr_device_open,
    .release = hcsr_device_release,
    .write=hcsr_device_write,
    .read=hcsr_device_read,
};

/*The init function invoked when the driver is installed and
carries out all initialization tasks necessary to build the device file*/
static int hcsr_init(void) {
     int status = 0;
     int ret;   
    
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    /*allocates major and minor number dynamically*/
    if (alloc_chrdev_region(&hcsr_dev_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_DEBUG "Can't register device\n");
        return -1;
    }
    /*creates a class under which the device tends to be appear*/
    hcsr_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(hcsr_class)) {
        printk(KERN_INFO "Error creating class\n");
         unregister_chrdev_region(hcsr_dev_num, 1);
         return -1;
    }
      /*allocates the memory for driver data structure*/
    hcsr_dev = (hcsr04_device_t *)kmalloc(sizeof(hcsr04_device_t),GFP_KERNEL);
    if (!hcsr_dev) {
        printk(KERN_INFO "Memory allocation for kernel data structure failed\n");
        return -1;
    }
    memset(hcsr_dev,0,sizeof(hcsr04_device_t));
    /*fills the device information necessary for creating the node under/dev by udev
    daemon process*/
    if(device_create(hcsr_class, NULL, hcsr_dev_num,NULL, DEVICE_NAME)==NULL)
 
    {

        printk(KERN_INFO "Error creating device\n");
        
        class_destroy(hcsr_class);
        unregister_chrdev_region(hcsr_dev_num, 1);
        return -1;
    }
    hcsr_dev->name = "ultrasonic_sensor";
    /*Connects the file operations with the character device file
    being created*/
    cdev_init(&hcsr_dev->hcsr_cdev, &hcsr_fops);
    hcsr_dev->hcsr_cdev.owner = THIS_MODULE;
    /*Links the major -minor number with the file so that kernel
    can decode via VFS if it is device file or not
    while data is being written to read from this file*/
    status = cdev_add(&hcsr_dev->hcsr_cdev, hcsr_dev_num, 1);
    if (status < 0) {
        printk(KERN_INFO "Error adding the cdev\n");
        device_destroy(hcsr_class, hcsr_dev_num);
        class_destroy(hcsr_class);
        unregister_chrdev_region(hcsr_dev_num, 1);
        return -1;
    }
    /*requesting pins for echo signal*/
    gpio_request(0,"echo_pin");
    gpio_request(18,"echo_dir");
    gpio_request(66,"echo_mux");
    
    /*exporting the echo config pins to sysfs*/
    gpio_export(0,0);
    gpio_export(18,0);
    gpio_export(66,0);
    
    /*setting the direction of pin as input*/
    gpio_direction_output(18,1);
    gpio_set_value_cansleep(66,0);
    gpio_direction_input(0);
    
    /*requesting pins for trigger signal*/
    gpio_request(1,"trig_pin");
    gpio_request(20,"trig_dir");
    gpio_request(68,"trig_mux");
   
    /*exporting the trigger config pins to sysfs*/
    gpio_export(1,0);
    gpio_export(20,0);
    gpio_export(68,0);
    
    /*setting the direction of pin as output*/
    gpio_direction_output(20,0);
    gpio_set_value_cansleep(68,0);
    gpio_direction_output(1,0);
    
    /*Initialization of the lock being used*/
    spin_lock_init(&hcsr_dev->spin);
    /*gets the corresponding irq number for gpio pin*/
    hcsr_dev->echo_irq = gpio_to_irq(0); 
    /*register the interrupt handler for the interrupt number*/
    ret = request_irq(hcsr_dev->echo_irq, irq_handler, IRQ_TYPE_EDGE_RISING,hcsr_dev->name , hcsr_dev);

    if (ret < 0) {
        printk(KERN_ALERT " Error requesting irq: %d\n", hcsr_dev->echo_irq);
        return -1;
    } else 
        printk(KERN_ALERT " Handler allocated to irq: %d\n", hcsr_dev->echo_irq);
   hcsr_dev->triggerThread=NULL;
    
    
    printk(KERN_INFO "Successfull registration done\n");
    return 0;

}

/*Invoked when the driver is unloaded from kernel and carries out
all clean up tasks like freeing the resources, unlinking the registrations,
deleting the corresponding nodes under /dev */    
static void hcsr_exit(void)
{    
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);   
         if (hcsr_dev->triggerThread != NULL) 
         {
                    kthread_stop(hcsr_dev->triggerThread);
                    hcsr_dev->triggerThread = NULL;
            }
        if(hcsr_dev->echo_irq)
            free_irq(hcsr_dev->echo_irq,hcsr_dev);
        
        gpio_unexport(0);
        gpio_unexport(18);
        gpio_unexport(66);
        gpio_unexport(1);
        gpio_unexport(20);
        gpio_unexport(68);
        
        
        gpio_free(0);
        gpio_free(18);
        gpio_free(66);
        gpio_free(1);
        gpio_free(20);
        gpio_free(68);
       
        
            
        device_destroy(hcsr_class,hcsr_dev_num);
        cdev_del(&hcsr_dev->hcsr_cdev);
        kfree(hcsr_dev);
        class_destroy(hcsr_class);
        unregister_chrdev_region(hcsr_dev_num,1); 
        printk(KERN_INFO "Successfull cleanup done\n");
}

module_init(hcsr_init);
module_exit(hcsr_exit);
MODULE_LICENSE("GPL");
