#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/spi/spi.h>
#include<linux/module.h>

/*Device specific information,
->The driver which needs to handle this device,
->The frequency of transmission,
->The bus number to which it is connected,
->The chip select pin to opt for to activate the device,
->Mode of operation: Here the data is latched on rising edge of clock
*/
static struct spi_board_info spi_device_info=
{
    .modalias="spi_dev",
    .max_speed_hz=10000000,
    .bus_num=1,
    .chip_select=1,
    .mode=0,
 };

/*Structures to represent the spi device and the master
of the spi bus to which the device is connected to*/
static struct spi_device *spi_custom_dev; 
struct spi_master *master;

/*The spi device initialization and registration function*/
static int spi_dev_init(void)
{
    int ret=0;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
     /*Finds the master which handles the specific spi bus*/
    master=spi_busnum_to_master(spi_device_info.bus_num);
    if(!master)
        return -ENODEV;
   /*Assigns the master to the device structure and instantiates
    the new spi device with the specified information */        
   spi_custom_dev=spi_new_device(master,&spi_device_info);
   if(!spi_custom_dev)
   {
        printk(KERN_INFO "Error in adding new spi device\n");
        return -1;
   }   
  /*Sets the no of bits to be transmitted in a word*/ 
   spi_custom_dev->bits_per_word=8;
   /*spi_setup - setup SPI mode and clock rate*/
   ret=spi_setup(spi_custom_dev);
   if(ret)
   spi_unregister_device(spi_custom_dev);
   else
   printk(KERN_INFO "spi device registered successfully\n");
   return ret;
}

/*The spi device cleanup and unregistration function*/
static void spi_dev_exit(void)
{
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    spi_unregister_device(spi_custom_dev);
}

module_init(spi_dev_init);
module_exit(spi_dev_exit);
MODULE_LICENSE("GPL");
                
