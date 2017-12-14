#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/spi/spi.h>
#include<linux/fs.h>
#include <linux/version.h>
#include<linux/gpio.h>
#include<linux/cdev.h>
#include<linux/delay.h>
#include<linux/semaphore.h>
#include "ioctl_spi.h"

#define CLASS_NAME "SPI"
#define DEVICE_NAME "ledmatrix"

/*macro to send the data transmission synchronously*/
#define SPI_MSG_SEND \
do\
{\
    spi_message_init(&spi->spi_led_msg);\
    spi_message_add_tail(&spi->spi_led_txr,&spi->spi_led_msg);\
    spi_sync(spi_led_dev,&spi->spi_led_msg);\
  }\
  while(0)  
  

/*a structure to hold the pattern to be
displayed and the amount of time in milliseconds
that the pattern to be displayed*/    
struct seq_time
{
    int m_sec;
    int ptrn;
 };   
    

/*Structures representing spi class,
to hold the major& minor number corresponding to device,
the detected spi device and the task that needs to operated
in background*/    
static struct class *spi_class;
static dev_t spi_dev_num;
static struct spi_device *spi_led_dev;
static struct task_struct *btask;

/*The data structure representing the spi driver as character
file and the message container to be transmitted across*/

typedef struct spi_led {
    struct cdev spi_cdev;
    char *spi_name;
    /*flag to indicate a spi transmission 
    is taking place*/
    int spi_ongoing;
    /*holds the pattern set to be displayed*/
    pattern ptr;
    struct semaphore spi_sem;
    /*structure to determine what pattern 
    to be displayed ,for how much time at 
    specific instance*/
    struct seq_time sq_t;
    struct spi_message spi_led_msg;
    struct spi_transfer spi_led_txr;
} spi_led_data;

/*A pointer to the driver data structure*/
spi_led_data *spi;

/*function to clear the led matrix*/
static int spi_led_reset(spi_led_data *spi) {
    /*intermediate transmission and receiving buffers*/
    unsigned char tx[2];
    unsigned char rx[2];
    int i=0;
     printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    /*giving the reference of transmit ,receive buffers
    ,configuring to send 2 bytes for 1 cycle of transmission,
    configuring to send 8 bits per word and setting the speed of
    transmission as 10MHz*/
    spi->spi_led_txr.tx_buf = tx;
    spi->spi_led_txr.rx_buf =rx;
    spi->spi_led_txr.len = 2;
    spi->spi_led_txr.cs_change = 1;
    spi->spi_led_txr.bits_per_word = 8;
    spi->spi_led_txr.speed_hz = 10000000;
    for ( i = 1; i < 9; ++i)
			{
				tx[0] = i;
		    	tx[1] = 0x00;/*all 0 bytes to clear*/
		    	gpio_set_value(15, 0);
		    	SPI_MSG_SEND;
               
		    	gpio_set_value(15, 1);
			}	
	return 0;		
    
}

/*The background task which actually transmits the pattern data
to glow led strip*/
static int glow_colors(void *data) {
    
    /*intermediate transmission and receiving buffers*/
    unsigned char tx[2];
    unsigned char rx[2];
    int i=0,ter_flag=1;
    spi_led_data *spi=(spi_led_data *)data;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    /*giving the reference of transmit ,receive buffers
    ,configuring to send 2 bytes for 1 cycle of transmission,
    configuring to send 8 bits per word and setting the speed of
    transmission as 10MHz*/
    spi->spi_led_txr.tx_buf = tx;
    spi->spi_led_txr.rx_buf =rx;
    spi->spi_led_txr.len = 2;
    spi->spi_led_txr.cs_change = 1;
    spi->spi_led_txr.bits_per_word = 8;
    spi->spi_led_txr.speed_hz = 10000000;
    
    while(!kthread_should_stop()&&(ter_flag))
    {
        /*send pattern while pattern and time arent 
        zero together*/
        if((spi->sq_t.m_sec) || (spi->sq_t.ptrn))
    {
        /*pattern to be displayed is being transmitted
        for each row of led matrix*/
        for ( i = 1; i < 9; ++i)
			{
				tx[0] = i;
		    	tx[1] = spi->ptr.pat[spi->sq_t.ptrn][i-1];
		    	gpio_set_value(15, 0);/*slave selected before transmission*/
		    	SPI_MSG_SEND;
                
		    	gpio_set_value(15, 1);/*slave released after transmission*/
			}
		   /*sleep for specified time to
		   retain the pattern in display*/ 
			msleep(spi->sq_t.m_sec);
	    }
	 else
	 {
	    /*if pattern and time both are zero, clearing the matrix*/
	      spi_led_reset(spi);
		}		
            
	ter_flag=0;		
    }
    
    /*locking ,resetting the flag to indicate the
    transmission is complete and releasing the lock*/
    up(&spi->spi_sem);
    spi->spi_ongoing=0;
    down(&spi->spi_sem); 

    btask = NULL;
    
    return 0;
}



/*Invoked when the device file is opened and stores the
reference of the driver data structure in file's private
data for caching purpose*/
int spi_open(struct inode *inode, struct file *fileptr) {
    spi_led_data *spi;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    spi = container_of(inode->i_cdev, spi_led_data, spi_cdev);
    fileptr->private_data = spi;
    return 0;
}


/*Invoked when the file descriptor representing
the device file is being closed,stops the display thread and clears the matrix*/

int spi_close(struct inode *inode, struct file *fileptr) {
    spi_led_data *spi;
    spi = (spi_led_data*) fileptr->private_data;
    printk(KERN_INFO "device %s called close\n", spi->spi_name);
    if(btask!=NULL)
            {
                kthread_stop(btask);
                btask=NULL;
            } 
    spi_led_reset(spi);        
            
    return 0;
}

/*Conditional check to differentiate between ioctl prototype w.r.t kernel
 * version and to account for portability
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int spi_ioctl(struct inode *i, struct file *spi_file, unsigned int cmd, unsigned long arg)
#else
/*ioctl to configure the pattern set to be displayed*/
static long spi_ioctl(struct file *spi_file, unsigned int cmd, unsigned long arg)
#endif
{
    spi_led_data *spi=spi_file->private_data;
    
    printk(KERN_INFO "Inside %s function\n",__FUNCTION__);
    switch (cmd)
    {
        case SET_PATTERN:
        if(spi->spi_ongoing==0)
        {   /*stop display thread before changing pattern set*/
            if(btask!=NULL)
            {
                kthread_stop(btask);
                btask=NULL;
            }   
            /*copy the data from user space to kernel space*/ 
            if (copy_from_user(&spi->ptr, (pattern *)arg, sizeof(pattern))!=0)
            {   
                printk(KERN_INFO "Error copying from user space in ioctl\n");
                return -EACCES;
            }
         }
         else
         {  
                printk(KERN_INFO "Error setting ioctl due to ongoing spi transfer\n");
                return -1; 
            }
            
            break;
        /*return error code when an invalid command is sent*/    
        default:
            printk(KERN_INFO "Invalid ioctl command\n");
            return -EINVAL;
        
        }
      
    return 0;
}

/*Invoked when some data representing the pattern to be displayed
and the time to retain the pattern is written to device file*/ 

ssize_t spi_led_write(struct file *spi_file,const char __user *seq,size_t length,loff_t *pos) 
{
    spi_led_data *spi=(spi_led_data *)spi_file->private_data;
    printk(KERN_INFO "Inside %s function\n",__FUNCTION__);
    if(spi_led_dev == NULL){
        printk("No SPI Device found:\n");
        return -3;
    }
    /*if spi transmission isnt going on*/
    if(spi->spi_ongoing==0)
    {
        if(btask!=NULL)
        {
            kthread_stop(btask);
            btask=NULL;
        } 
        
        /*copy the data from user space to kernel space*/  
    if (copy_from_user(&spi->sq_t, (struct seq_time *)seq, sizeof(struct seq_time))!=0)
    {
        printk(KERN_INFO "Error copying from user space in write\n");
        return -EACCES;
     }   
    else
        {
            /*if pattern to be displayed is out of range then
            return error code*/
            if((spi->sq_t.ptrn<0)||(spi->sq_t.ptrn>10))
                return -2;
             /*locking ,setting the flag to indicate the
               transmission is going on and releasing the lock*/    
            up(&spi->spi_sem);
            spi->spi_ongoing=1;
            down(&spi->spi_sem);
            /*run the display thread*/
            if(btask==NULL)
                btask=kthread_run(&glow_colors,spi,"glow_task");
            if(IS_ERR(btask))
            {
                printk(KERN_INFO "Error creating display Thread");
                
                up(&spi->spi_sem);
                spi->spi_ongoing=0;
                down(&spi->spi_sem);  
             }
         }
      }
      else
      { 
        printk(KERN_INFO "Error writing ,Ongoing spi transfer\n");
        return -1;            
            
      }      
         
    return 0;
    
    
} 

/*Linking the implemented file operations with the
spi character device file being created*/

static struct file_operations spi_led_fops = {
    .open = spi_open,
    .release = spi_close,
    .owner = THIS_MODULE,
    .write = spi_led_write,
     #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = spi_ioctl
#else
    .unlocked_ioctl = spi_ioctl
#endif
};

/*Probe function called when the spi driver and its corresponding
device match each other*/
static int spi_driver_probe(struct spi_device *spi_dev) {
    unsigned char tx[2];
    unsigned char rx[2];
    spi_led_dev = spi_dev;
    
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    /*Setting the master output slave input pin as output
    with all the auxiliary setting of direction,multiplexer
    pins to attain  the objective*/
    gpio_request(24, "mosi");
    gpio_export(24, 0);
    gpio_direction_output(24, 0);

    gpio_request(25, "mosi_pull_up");
    gpio_export(25, 0);
    gpio_direction_output(25, 0);

    gpio_request(44, "mosi_mux_1");
    gpio_export(44, 0);
    gpio_direction_output(44, 1);

    gpio_request(72, "mosi_mux_2");
    gpio_export(72, 0);
    gpio_set_value_cansleep(72, 0);
    
    /*configurations to be made for slave 
    select pin*/
    gpio_request(15, "slave_select");
    gpio_export(15, 0);
    gpio_direction_output(15,1);
    
    gpio_request(42, "slave_dir");
    gpio_export(42, 0);
    gpio_direction_output(42, 0);
    
    /*configurations to be made for clock pin
    to provide timing sync for data transmission*/
    gpio_request(30, "clock_dir");
    gpio_export(30, 0);
    gpio_direction_output(30, 0);

    gpio_request(31, "clock_pull_up");
    gpio_export(31, 0);
    gpio_set_value_cansleep(31,0);
    
    gpio_request(46, "clock_mux");
    gpio_export(46, 0);
    gpio_direction_output(46,1);
    
    spi->spi_led_txr.tx_buf = tx;
    spi->spi_led_txr.rx_buf =rx;
    spi->spi_led_txr.len = 2;
    spi->spi_led_txr.cs_change = 1;
    spi->spi_led_txr.bits_per_word = 8;
    spi->spi_led_txr.speed_hz = 10000000; 
    
    
    tx[0] = 0x09;
    tx[1] = 0x00;
    gpio_set_value(15, 0);
    SPI_MSG_SEND;
    gpio_set_value(15, 1);
    udelay(10000);
    
    /* intensity level medium */
    tx[0] = 0x0A;
    tx[1] = 0x00;
    gpio_set_value(15, 0);
    SPI_MSG_SEND;
    gpio_set_value(15, 1);
    udelay(10000);
    
    /* scan all the data register for displaying */
    tx[0] = 0x0B;
    tx[1] = 0x07;
    gpio_set_value(15, 0);
    SPI_MSG_SEND;
    gpio_set_value(15, 1);
    udelay(10000);
    
    /* shutdown register - select normal operation */
    tx[0] = 0x0C;
    tx[1] = 0x01;
    gpio_set_value(15, 0);
    SPI_MSG_SEND;
    gpio_set_value(15, 1);
    udelay(10000);
    
     if(btask!=NULL)
            {
                kthread_stop(btask);
                btask=NULL;
            } 
    spi_led_reset(spi); 
    
    return 0;
}

/*function to free the gpio pins, in order to clean up
resources,and also unexports gpio pin files from sysfs*/
static void free_gpios(void) {
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    gpio_unexport(24);
    gpio_free(24);
    gpio_unexport(25);
    gpio_free(25);
    gpio_unexport(44);
    gpio_free(44);
    gpio_unexport(72);
    gpio_free(72);
    gpio_unexport(30);
    gpio_free(30);
    gpio_unexport(15);
    gpio_free(15);
    gpio_unexport(46);
    gpio_free(46);
    gpio_unexport(42);
    gpio_free(42);
}

/*Invoked when the device is removed from system, which
inturn free the gpio pins that were allocated and stops the display thread*/
static int spi_driver_remove(struct spi_device *spi_dev) {
    
  
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    
    if(btask!=NULL)
    {
        kthread_stop(btask);
        btask=NULL;
    }   
    if(spi_led_dev != NULL){
    spi_led_reset(spi); 
    spi_led_dev = NULL;
    }  
    free_gpios();
    
    
    
    return 0;
}

/*Populating the neccesary data to represent the spi driver
as in, the name which is used to match for device-driver connection,
the probe and remove function to be called when the device is found or
removed from the system*/
static struct spi_driver spi_custom_driver = {
    .driver =
    {
        .name = "spi_dev",
        .owner = THIS_MODULE,
    },
    .probe = spi_driver_probe,
    .remove = spi_driver_remove,
};

/*The init function invoked when the driver is installed and
carries out all initialization tasks necessary to build the device file*/
static int spi_driver_init(void) {
    int status = 0;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    /*allocates major and minor number dynamically*/
    if (alloc_chrdev_region(&spi_dev_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_DEBUG "Can't register device\n");
        return -1;
    }
     /*creates a class under which the device tends to be appear*/
    spi_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(spi_class)) {
        printk(KERN_INFO "Error creating class\n");
        unregister_chrdev_region(spi_dev_num, 1);
        return PTR_ERR(spi_class);
    }
     /*allocates the memory for driver data structure*/
    spi = (spi_led_data*) kmalloc(sizeof (spi_led_data), GFP_KERNEL);
    if (!spi) {
        printk(KERN_INFO "Memory allocation for kernel data structure failed\n");
        return -1;
    }
    memset(spi,0,sizeof(spi_led_data));
    btask=NULL;
    /*fills the device information necessary for creating the node under/dev by udev
    daemon process*/
    if (device_create(spi_class, NULL, spi_dev_num, NULL, DEVICE_NAME) == NULL) {
        printk(KERN_INFO "Error creating device\n");
        class_destroy(spi_class);
        unregister_chrdev_region(spi_dev_num, 1);
        return -1;
    }
    spi->spi_name = "spi_led_strip";
    /*Connects the file operations with the character device file
    being created*/
    cdev_init(&spi->spi_cdev, &spi_led_fops);
    spi->spi_cdev.owner = THIS_MODULE;
    /*Links the major -minor number with the file so that kernel
    can decode via VFS if it is device file or not
    while data is being written to read from this file*/
    status = cdev_add(&spi->spi_cdev, spi_dev_num, 1);
    if (status < 0) {
        printk(KERN_INFO "Error adding the cdev\n");
        device_destroy(spi_class, spi_dev_num);
        class_destroy(spi_class);
        unregister_chrdev_region(spi_dev_num, 1);
        return -1;
    }

    /*register the driver as spi driver and holds an entry in table
    to be searched while getting a match for spi device*/
    status = spi_register_driver(&spi_custom_driver);
    if (status < 0) {
        printk(KERN_INFO "Error registering spi driver\n");

        cdev_del(&spi->spi_cdev);
        device_destroy(spi_class, spi_dev_num);
        kfree(spi);
        class_destroy(spi_class);
        unregister_chrdev_region(spi_dev_num, 1);
        return -1;
    }
    /*Initializing the semaphore to be used for sync
    while manipulating critical section*/
    sema_init(&spi->spi_sem,0);
    spi->spi_ongoing=0;
    
    printk(KERN_INFO "Successfull registration done\n");
    return 0;
}

/*Invoked when the driver is unloaded from kernel and carries out
all clean up tasks like freeing the resources, unlinking the registrations,
deleting the corresponding nodes under /dev */
static void spi_driver_exit(void) {
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    if(btask!=NULL)
    {
        kthread_stop(btask);
        btask=NULL;
    }   
    if(spi_led_dev!=NULL){
    spi_led_reset(spi); 
    spi_led_dev = NULL;
    }
    free_gpios();
    spi_unregister_driver(&spi_custom_driver);
    device_destroy(spi_class, spi_dev_num);
    cdev_del(&spi->spi_cdev);
    kfree(spi);
    class_destroy(spi_class);
    unregister_chrdev_region(spi_dev_num, 1);
}

module_init(spi_driver_init);
module_exit(spi_driver_exit);
MODULE_LICENSE("GPL");
