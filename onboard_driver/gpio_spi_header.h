#ifndef __GPIO_FUNC_H__


 /****************************************************************
 * Constants
 ****************************************************************/
 
#define GPIO_PATH "/sys/class/gpio" //GPIO base pathe
#define MAX_BUF 64	//Buffer to hold the path 
#define SPI_NAME "/dev/spidev1.0"	//SPI device path  
#define ARRAY_SIZE(ar) (sizeof(ar) / sizeof((ar)[0])) //Macro to calcuate array length
#define CPU_CYCLE_PER_MICROSECOND 400 //Board clock frequency in Mhz


/****************************************************************
 * Functions
 ****************************************************************/

int gpio_export(unsigned int gpio);
int gpio_unexport(unsigned int gpio);
int gpio_set_direction(unsigned int gpio, unsigned int out_flag);
int gpio_set_value(unsigned int gpio, unsigned int value);
#endif /* __GPIO_FUNC_H__ */
