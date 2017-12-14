CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-gcc


IOT_HOME = /opt/iot-devkit/1.7.2/sysroots


PATH := $(PATH):$(IOT_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

SROOT=$(IOT_HOME)/i586-poky-linux

#obj-m = rbt530_drv.o
app=Test_SPI
all: main_spi.o gpio_spi_func.o
	
	$(CROSS_COMPILE) main_spi.o gpio_spi_func.o -o $(app) --sysroot=$(SROOT) -pthread
gpio_spi_func.o: gpio_spi_func.c
	$(CROSS_COMPILE) -Wall -c -o gpio_spi_func.o --sysroot=$(SROOT) gpio_spi_func.c
main_spi.o: main_spi.c
	$(CROSS_COMPILE) -Wall -pthread -c -o main_spi.o --sysroot=$(SROOT) main_spi.c



clean:
	rm -f *.o
	rm -f $(app)
