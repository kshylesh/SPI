#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/poll.h>
#include <sys/types.h>
#include "gpio_spi_header.h"

int Thread_Run = 0; //Initialize All threads to run

unsigned int Main_Sleep = 120; //Main sleeps for 2 minute and exits program.

pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER; //pthread mutext initialization

double G_distance = 30; //Global variable  to track distance from ultrasonic sensor 


/*Timer function*/
static __inline__ unsigned long long tsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

/* LED Matrix thread*/
void *tx_data(void *arg)
{ 
	int i,fd,ret;
	double L_distance; //local varibale to get the distance value
	uint8_t utx_buf[2] = {0}; //user defined SPI transmission buffer
	uint8_t urx_buf[2] = {0,0};//user defined SPI receiver buffer

	int SPI_MOSI [4]= {24, 25, 44, 72};	//PIN configuration for MOSI pin (IO11)
	int SPI_SCLK [3]= {30, 31, 46}; //PIN configuration for CLK pin (IO13)
	int SPI_SS [2]= {15, 42};	//PIN configuration fro Chip select pin (IO12)


	uint8_t Pattern_Lstill[] = { 0x98, 0xDF, 0x37, 0x10, 0x10, 0xF0, 0x90, 0x08 }; //LED pattern to make DOG still image in right direction
	uint8_t Pattern_Lrun[] = { 0x18, 0xFF, 0x97, 0x10, 0xD0, 0x70, 0x10, 0x20 }; //LED pattern to make DOG run image in right direction
	uint8_t Pattern_Rstill[] = { 0x08, 0x90, 0xF0, 0x10, 0x10, 0x37, 0xDF, 0x98 };//LED pattern to make DOG still image in left direction
	uint8_t Pattern_Rrun[] = { 0x20, 0x10, 0x70, 0xD0, 0x10, 0x97, 0xFF, 0x18 }; //LED pattern to make DOG run image in left direction
	
	/*mapping of user space buffer to kernel spi_transfer structure*/
	struct spi_ioc_transfer tfr = {
		.tx_buf = (unsigned long)utx_buf,
		.rx_buf = (unsigned long)urx_buf,
		.len = 2,
		.cs_change = 1,
		.delay_usecs= 1,
		.speed_hz = 10000000,
		.bits_per_word = 8,
	};

	/*GPIO export/direction/Mux settings for MOSI*/
	gpio_export(SPI_MOSI[0]);
	gpio_set_direction(SPI_MOSI[0],1);
	gpio_set_value(SPI_MOSI[0],0);

	gpio_export(SPI_MOSI[1]);
	gpio_set_direction(SPI_MOSI[1],1);
	gpio_set_value(SPI_MOSI[1],0);

	gpio_export(SPI_MOSI[2]);
	gpio_set_direction(SPI_MOSI[2],1);
	gpio_set_value(SPI_MOSI[2],1);

	gpio_export(SPI_MOSI[3]);
	gpio_set_value(SPI_MOSI[3],0);

	/*GPIO export/direction/Mux settings for CLK*/
	gpio_export(SPI_SCLK[0]);
	gpio_set_direction(SPI_SCLK[0],1);
	gpio_set_value(SPI_SCLK[0],0);

	gpio_export(SPI_SCLK[1]);
	gpio_set_direction(SPI_SCLK[1],1);
	gpio_set_value(SPI_SCLK[1],0);

	gpio_export(SPI_SCLK[2]);
	gpio_set_direction(SPI_SCLK[2],1);
	gpio_set_value(SPI_SCLK[2],1);

	/*GPIO export/direction/Mux settings for Slave select*/
	gpio_export(SPI_SS[0]);
	gpio_set_direction(SPI_SS[0],1);
	gpio_set_value(SPI_SS[0],0);

	gpio_export(SPI_SS[1]);
	gpio_set_direction(SPI_SS[1],1);
	gpio_set_value(SPI_SS[1],0);
	
	fd = open(SPI_NAME, O_RDWR); //Open the /dev/spidev1.0 file

	/*Check if the SPI device is open properly*/
	if(fd<0) 
	{
		printf("Error:Opening SPI device\n");
		return 0;
	}

	
	/*LED Matrix Initialization*/
	/*Set Decode mode to no Decode*/
	utx_buf[0] = 0x09;//Address
    utx_buf[1] = 0x00;//Data
    gpio_set_value(SPI_SS[0], 0); //Switch slave select pin to 0 to enable data transmission
    ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);//Perform SPI transfer
    if (ret < 0)//Check if SPI transfer was successful
	    {
	    	printf("Error:Setting Decode mode\n");
	    }
    gpio_set_value(SPI_SS[0], 1);//Switch slave select pin to 1 to disable data transmission

    usleep(10000);

    /*Set Intensity of the LED Matrix*/
   	utx_buf[0] = 0x0A;//Address
    utx_buf[1] = 0x00;//Data
    gpio_set_value(SPI_SS[0], 0);
    ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);
    if (ret < 0)
	    {
	    	printf("Error:Setting Decode mode\n");
	    }
    gpio_set_value(SPI_SS[0], 1);
    
    usleep(10000);

    /* Set Scan mode of the LED Matrix */
    utx_buf[0] = 0x0B;
    utx_buf[1] = 0x07;
    gpio_set_value(SPI_SS[0], 0);
    ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);
    if (ret < 0)
	    {
	    	printf("Error:Setting Scan mode\n");
	    }
    gpio_set_value(SPI_SS[0], 1);

    usleep(10000);

    /*Set Shutdown registe to operate in normal mode */
    utx_buf[0] = 0x0C;
    utx_buf[1] = 0x01;
    gpio_set_value(SPI_SS[0], 0);
    ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);
    if (ret < 0)
	    {
	    	printf("Error:Setting Run mode\n");
	    }
    gpio_set_value(SPI_SS[0], 1);

	usleep(10000);

	/*Continously perform SPI operation to print DOG image on LED Matrix*/
	while(!(*(int *)arg))
	{
		pthread_mutex_lock(&m_lock);//Lock mutex to get distance from Ultrasonic sensor
		L_distance = G_distance;	//Copy data to local variable
		pthread_mutex_unlock(&m_lock);//Unlock mutex

		/*If distance is lesser than 30cm make dog run*/
		if(L_distance < 30)
		{
			for ( i = 1; i < 9; ++i)//For 8 rows of LED matrix
			{
				utx_buf[0] = i;//Send address (row)
		    	utx_buf[1] = Pattern_Rstill[i-1];//Send data from the array (Right still array)
		    	gpio_set_value(SPI_SS[0], 0);
		    	ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);
		    	gpio_set_value(SPI_SS[0], 1);
			}
			usleep(L_distance * 8000); //usleep propotional to distance(If object is near the dog runs faster)

			for ( i = 1; i < 9; ++i)
			{
				utx_buf[0] = i;//Send address (row)
				utx_buf[1] = Pattern_Rrun[i-1];//Send data from the array (Right run array)
		    	gpio_set_value(SPI_SS[0], 0);
		    	ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);
		    	gpio_set_value(SPI_SS[0], 1);
			}
			usleep(L_distance * 8000);//usleep propotional to distance(If object is near the dog runs faster)
		}
		
		/*For distance greater than 30cm make dog walk towards left*/
		else if(L_distance > 30)
		{
			for ( i = 1; i < 9; ++i) 
			{
				utx_buf[0] = i;//Send address (row)
		    	utx_buf[1] = Pattern_Lstill[i-1];//Send data from the array (Left still array)
		    	gpio_set_value(SPI_SS[0], 0);
		    	ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);
		    	gpio_set_value(SPI_SS[0], 1);
			}
			usleep(1000000);//usleep  for 1 second (walk animation)

			for ( i = 1; i < 9; ++i)
			{
				utx_buf[0] = i;//Send address (row)
				utx_buf[1] = Pattern_Lrun[i-1];//Send data from the array (Left run array)
		    	gpio_set_value(SPI_SS[0], 0);
		    	ret = ioctl(fd,SPI_IOC_MESSAGE(1), &tfr);
		    	gpio_set_value(SPI_SS[0], 1);
			}
			usleep(1000000); //usleep for 1 second (walk animation)
		}
	}
	
	/*Unexport all GPIO Pins*/
	for(i = 0 ; i < 3 ; ++i ){
		gpio_unexport(SPI_MOSI[i]);
		gpio_unexport(SPI_SCLK[i]);
	}
	for ( i = 0; i < 2; ++i)
		gpio_export(SPI_SS[i]);

	//exit thread
	pthread_exit(0); 
}

void *calc_distance(void *arg)
{
	int echo_value,echo_edge,i; //file desciptor for echo pin value and edge respectively
	double L_distance; //Local variable distance

	struct pollfd poll_pulse={0}; //poll function variable declaration

	unsigned long long start_time,stop_time; //Start and stop time declaration
	unsigned char buffer[2];	//Buffer to clear the poll file descriptors

	int echo[3] = {0, 18, 66};	//Echo pin GPIO settings(Pin 5)
	int trig[3] = {1, 20, 68};	//Trigger pin GPIO setttings(Pin 6)

	/*GPIO Export for echo pin*/
	gpio_export(echo[0]);
	gpio_export(echo[1]);
	gpio_export(echo[2]);

	/*GPIO set direction for echo pin*/
	gpio_set_direction(echo[0],0);
	gpio_set_direction(echo[1],1);

	/*GPIO set value for echo pin*/
	gpio_set_value(echo[1],1);
	gpio_set_value(echo[2],0);


	/*GPIO export for trigger pin*/
	gpio_export(trig[0]);
	gpio_export(trig[1]);
	gpio_export(trig[2]);


	/*GPIO set direction for trigger pin*/
	gpio_set_direction(trig[0],1);
	gpio_set_direction(trig[1],1);

	/*GPIO set direction for trigger pin*/
	gpio_set_value(trig[1],0);
	gpio_set_value(trig[2],0);

	
	/*Get file descriptor for echo pin value and edge*/
	echo_edge=open("/sys/class/gpio/gpio0/edge",O_RDWR);
	echo_value=open("/sys/class/gpio/gpio0/value",O_RDONLY|O_NONBLOCK);

	/*Assigning echo pin value attribute to pollfd file descriptor*/
	poll_pulse.fd=echo_value;
	/*Set poll events*/
	poll_pulse.events=POLLPRI|POLLERR;
	 
	/*Continously monitor distance*/
	while(!(*(int *)arg))
	{  

	    pread(poll_pulse.fd, &buffer, sizeof(buffer), 0);/*Clearing pollfd file descriptor before setting the edge attribute*/
		write(echo_edge,"rising",6);	/*Set edge attribute to detect rising edge*/

		gpio_set_value(trig[0], 1);		/*Send a value high to trigger pin*/
		usleep(12);						/*Minimum period of high pulse is about 10us (12us is given considering overhead)*/
		gpio_set_value(trig[0], 0);		/*Send a value low to trigger pin*/
		    
		poll(&poll_pulse,1,1000);	 //poll for echo signal with time out of 1sec
		
		/*If event returns true then start timer and read second pulse*/
		if(poll_pulse.revents & POLLPRI)	
		    {   
		        start_time=tsc();	//Starts timer

		        pread(poll_pulse.fd, &buffer, sizeof(buffer), 0); //Clear the poll file descriptor to read next value
		        
		        write(echo_edge,"falling",7);	//Set edge attribute to falling edge
		        
		        poll(&poll_pulse,1,1000);
		        
		        stop_time=tsc();	//Stop the timer once the polling is finished 
		        
		         	/*If the second poll was successfull calculate distance*/
		          if((poll_pulse.revents & POLLPRI))
		            {
		              pthread_mutex_lock(&m_lock); //Mutex lock to modify the global variable distance
		              G_distance=((stop_time-start_time)/CPU_CYCLE_PER_MICROSECOND)*0.017; //Calculate distance CPU_PER_MICROSECOND is set to 400
		         	  L_distance = G_distance;	//Copy the global distance to local variable
		              pthread_mutex_unlock(&m_lock);	//Unlock the mutex
		              printf("distance is %0.2f cms\n",L_distance);	//Print the distance on console
		             }
		        
		         	/*If second poll was unsccessfull (if object <2cm or >400cm)*/
		          else{
		             printf("Distance to be measured is out of range\n");
		              }
		       }
		    /*If the first poll was unsuccessfull : failed to detect rising edge*/
		    else
		        printf("Rising edge not detected\n");    
		 
		  usleep(60000);//Period of 60ms between the trigger pulse to operate the sensor effectively
	} 

	/*Unexport all GPIO Pins*/
	for(i = 0 ; i < 3 ; ++i ){
		gpio_unexport(echo[i]);
		gpio_unexport(trig[i]);
	}

	//Exit the thread program                    
	pthread_exit(0);
}   

/*Main function to create threads*/
int main()
{
	pthread_t t_led, t_dist; 
	int ret;
	ret = pthread_create(&t_led, NULL, tx_data, &Thread_Run);//Create pthread t_led running sub function LED matrix and SPI transmission
 	if(ret)//If the thread was not created successfuuly print error and exit
        {
            fprintf(stderr,"error creating thread : %d\n",ret);
            exit(EXIT_FAILURE);
        }

    ret = pthread_create(&t_dist, NULL, calc_distance, &Thread_Run);//Create pthread t_dist running sub function Ultrasonic sensor distance measurement
 	if(ret)
        {
            fprintf(stderr,"error creating thread : %d\n",ret);
            exit(EXIT_FAILURE);
        }
    sleep(Main_Sleep);//sleep for 2 minute
    Thread_Run = 1;// exit threads

     /*Wait untill the thread exits*/
    pthread_join(t_led, NULL);
    pthread_join(t_dist, NULL);
    printf("All threads exited.Exiting Main\n");
    return 0; 
}