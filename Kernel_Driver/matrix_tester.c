#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <sys/ioctl.h>
#include<pthread.h>
#include "ioctl_spi.h"

#define PATTERN_NUM 10 //Define number of patterns to send
#define DATA_BYTES  8 //Length of each pattern

pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER; //pthread mutext initialization

int Thread_Run = 0; //Initialize All threads to run

unsigned int Main_Sleep = 120; //Main sleeps for 2 minute and exits program.

double G_distance = 40; //Global variable  to track distance from ultrasonic sensor 

/*Hex code for 10 patterns */
unsigned char my_pixel[PATTERN_NUM][DATA_BYTES] = {{ 0x00, 0x06, 0x06, 0x46, 0x46, 0x56, 0x06, 0x00 },
                                                  { 0x00, 0x06, 0x06, 0x46, 0x66, 0x42, 0x06, 0x00 },
                                                  { 0x08, 0x90, 0xF0, 0x10, 0x10, 0x37, 0xDF, 0x98 },
                                                  { 0x20, 0x10, 0x70, 0xD0, 0x10, 0x97, 0xFF, 0x18 },
                                                  { 0x98, 0xDF, 0x37, 0x10, 0x10, 0xF0, 0x90, 0x08 },
                                                  { 0x18, 0xFF, 0x97, 0x10, 0xD0, 0x70, 0x10, 0x20 },                                                                                         
                                                  { 0x3c, 0x42, 0x91, 0xa5, 0xa5, 0x91, 0x42, 0x3c },
                                                  { 0x3c, 0x42, 0xa1, 0x95, 0x95, 0xa1, 0x42, 0x3c },
                                                  { 0x00, 0x00, 0x90, 0xcb, 0x3f, 0xcb, 0x90, 0x00 },
                                                  { 0x00, 0x00, 0x64, 0x4b, 0x3f, 0x4b, 0x64, 0x00 }};

/*structure to hold the pattern to be displayed and the amount of time in milliseconds
that the pattern to be displayed*/ 
struct seq_time
{
    int m_sec;
    int ptrn;
    };
    
/*Function to write patterns and dealy to driver program*/
int write_pattern(int file_desc,int mil_sec,int anim) //Arguments(file descriptor, dealy for animation, animation pattern)
{
    int ret;
    struct seq_time s;
    s.m_sec=mil_sec;
    s.ptrn=anim;
    ret=write(file_desc,(char *)&s,sizeof(struct seq_time)); //Write to LED driver
    return ret;
}   

/*Function to copy user patterns to pattern structure(driver) */
int copy_pixel(pattern *p,unsigned char pixel[PATTERN_NUM][DATA_BYTES])
{   
     int i,j;
     for(i=0;i<PATTERN_NUM;i++) 
     {
        for(j=0;j<DATA_BYTES;j++)
        {
            p->pat[i][j]=pixel[i][j]; //copy all patterns to  pattern structure
        }
     } 
 
  return 0;
}          

/*Thread for Transmitting patterns to driver*/
void *LED_Matrix(void *arg)
 {
  int fd,ret;
  double L_distance ; //local varibale to get the distance value
  pattern p;
  copy_pixel(&p,my_pixel); //Call to copy the user patterns to driver

   fd=open("/dev/ledmatrix",O_RDWR);//open the file descriptor for led matrix
   if (ioctl(fd, SET_PATTERN, &p) == -1) //IOCTL cunction to set the LED patterns
        perror("query_apps ioctl set");
    else
     printf("Ioctl data sent successfully\n"); 

/*Run the LED matrix patterns continuously until timeout(2 minutes)*/
  while(!(*(int *)arg)){
        /*Pthread lock to get ditance from global variable */
        pthread_mutex_lock(&m_lock);  
        L_distance = G_distance;
        pthread_mutex_unlock(&m_lock);

  /*if distance is between 40 and 35 write pattern(bounce ball game) with 500ms delay between patterns*/
    if(L_distance < 40 && L_distance > 35)   
     {
      /*write pattern arguments:(file descriptor fd, delay in  ms,pattern(pattern array index from pixel array)*/
      ret=write_pattern(fd,500,0); 
      usleep(600000); //delay before the next transmission to avoid overlapping of sequence

        if(ret<0)/*Error handling if there was file write error*/
            printf("write error in pattern : \n");

      ret=write_pattern(fd,500,1);
      usleep(600000);

        if(ret<0)
            printf("write error in pattern : \n");
      }     

   /*if distance is between 35 and 30 write pattern(dog walk right) with 400ms delay between patterns*/
    else if(L_distance < 35 && L_distance > 30)
      {
        ret=write_pattern(fd,400,2);
        usleep(500000);

          if(ret<0)
              printf("write error in pattern : \n");

        ret=write_pattern(fd,400,3);
        usleep(500000);

          if(ret<0)
              printf("write error in pattern : \n");
      }  

   /*if distance is between 30 and 28 write pattern(dog walk left) with 300ms delay between patterns*/    
    else if(L_distance < 30 && L_distance > 25)
      { 
        ret=write_pattern(fd,300,4);
        usleep(400000);

          if(ret<0)
              printf("write error in pattern : \n");

        ret=write_pattern(fd,300,5);
        usleep(400000);

          if(ret<0)
              printf("write error in pattern : \n");
        }

   /*if distance is between 25 and 15 write pattern(simley) with 200ms delay between patterns*/
    else if(L_distance < 25 && L_distance > 15 )
      {
        ret=write_pattern(fd,200,6);
        usleep(300000);

          if(ret<0)
              printf("write error in pattern : \n");

        ret=write_pattern(fd,200,7);
        usleep(300000);

          if(ret<0)
              printf("write error in pattern : \n");
        }

   /*if distance is between 15 and 8 write pattern(jumping man) with 100ms delay between patterns*/
    else if(L_distance < 15 && L_distance > 8)
      {
        ret=write_pattern(fd,100,8);
        usleep(200000);

          if(ret<0)
              printf("write error in pattern : \n");

        ret=write_pattern(fd,100,9);
        usleep(200000);

          if(ret<0)
              printf("write error in pattern : \n");
      }

   /*if distance is greater than 40 and less than 8 clear LED display*/
    else
      {
        ret=write_pattern(fd,0,0);
        usleep(200000);

          if(ret<0)
              printf("write error in pattern : \n");
      }
    
  }
  /*Clear LED display on exit*/
  ret=write_pattern(fd,0,0);
  usleep(200000);
  //exit thread
  pthread_exit(0); 
}

/*Thread fucntion to read distance from driver and update the global distance variable*/
void *Dist_Calc(void *arg)
{
    int fd,ret;
   int write_flag=1; //Write flag to start(1) and stop(0) sensor function in driver
   unsigned long long L_distance; //Local variable to keep track of distance
   fd=open("/dev/hcsr",O_RDWR); //Open Sensor file descriptor
   ret=write(fd,(const char *)&write_flag,sizeof(int));// Initiate sensor to read distance
   /*Error handling if sensor is busy*/
   if(ret<0)
        printf("on going sampling,trigger cant be sent\n");
   //usleep(100000);
  /*Read sensor value from driver untill timeout*/
   while(!(*(int *)arg))
   {
     /*Read the value from driver and update it to Local variable L_distance*/
     ret=read(fd,&L_distance,sizeof(unsigned long long));

     /*Convert microsecond to corresponding distance in mm*/
     L_distance = L_distance * 0.017 ;
  
     /*Error handling during read from driver*/
    if(ret<0)
            printf("error reading data byte\n");
    /*Assing local varibale distance calculated to 
    Global varible G_distance by locking using mutex*/      
    else
       {
            pthread_mutex_lock(&m_lock);
            G_distance = L_distance;
            printf("The distance measured is %llu\n",L_distance);
            pthread_mutex_unlock(&m_lock);
        } 
        usleep(200000);
    } 
    /*Stop distance detection in driver*/
    write_flag=0;
    ret=write(fd,(const char *)&write_flag,sizeof(int));
    if(ret<0)
        printf("Not able to stop\n");
    /*Exit pthread*/
    pthread_exit(0);
}


/*Main function to create threads*/
int main(void)
{
  pthread_t t_led, t_dist; 
  int ret;
  ret = pthread_create(&t_led, NULL, LED_Matrix, &Thread_Run);//Create pthread t_led running sub function LED matrix and SPI transmission
  if(ret)//If the thread was not created successfuuly print error and exit
        {
            fprintf(stderr,"error creating thread : %d\n",ret);
            exit(EXIT_FAILURE);
        }

    ret = pthread_create(&t_dist, NULL, Dist_Calc, &Thread_Run);//Create pthread t_dist running sub function Ultrasonic sensor distance measurement
  if(ret)
        {
            fprintf(stderr,"error creating thread : %d\n",ret);
            exit(EXIT_FAILURE);
        }
    sleep(Main_Sleep);//sleep for 2 minute
    Thread_Run = 1;// exit threads

     /*Wait untill the thread exits*/
    pthread_join(t_dist, NULL);
    pthread_join(t_led, NULL);
    printf("All threads exited.Exiting Main\n");
    return 0; 
}            
