Team 26
Karthik Shylesh Kumar 1213169085
Dheemanth Bykere Mallikarjun 1213381271

List of files
1.matrix_tester.c:C file containg main function,pthreads for writting LED Matrix pattern to driver and read distance from Ultrasonic sensor driver function.
2.sensor.c:C driver file  for ultrasonic sensor consisting of funcitons to calculate distance,export gpio,free gpios,IRQ handler fucnction.
3.spi_device.c:C driver file consisting of spi device information,spi init function, spi exit funciton.
4.spi_driver.c:C driver file consisting of function to create,remove devices, transmit LED patterns.

--------------------------------------------------------------------------------------------------------------------------------------------------
Steps to Run the program
--------------------------------------------------------------------------------------------------------------------------------------------------
1.Pins used for SPI : MOSI pin IO11, SS pin IO12, CLK pin IO13.
2.Pins used for Ultrasonic sensor: Echo pin IO5, Trigger pin IO6.
3.Program runs for 120 seconds and exits,To increase the duration change the global value of MAIN_SLEEP.
4.Use make command "make all" to generate module files(spi_device.ko,spi_driver.ko,sensor.ko) and object file (matrix_tester.o).
5.Transfer spi_device.ko,spi_driver.ko,sensor.ko and matrix_tester.o files to board using command scp <file_name> root@<ip_address>.
6.Install modules spi_device.ko,spi_driver.ko,sensor.ko using command insmod <file_name>. Modules can be installed in any order.
7.patterns and delay used.(Total of 5 animation.Each animation contains 2 patterns).
  a.distance:40-35 	Animation:bounce ball game(bounce/hit) 	delay:500ms
  b.distance:35-30 	Animation:dog walk right(walk/still) 	delay:400ms
  c.distance:30-25 	Animation:dog walk left(walk/still)	delay:300ms
  d.distance:25-15	Animation:smiley(sad/happy)		delay:200ms
  e.distance:15-8	Animation:Person(Jump/stand)		delay:100ms
8.wait for 2minutes for program to exit or use kill command to exit.
9.unistall modules using rmmod <file_name>
