#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "gpio_spi_header.h"

/*GPIO export Function*/
int gpio_export(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(GPIO_PATH "/export", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
 
	return 0;
}

/*GPIO unexport function*/
int gpio_unexport(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(GPIO_PATH "/unexport", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
	return 0;
}

/*GPIO Set direction*/
int gpio_set_direction(unsigned int gpio, unsigned int out_flag)
{
	int fd; //len;
	char buf[MAX_BUF];
 
    snprintf(buf, sizeof(buf), GPIO_PATH  "/gpio%d/direction", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/direction");
		return fd;
	}
 
	if (out_flag)
		write(fd, "out", 3);
	else
		write(fd, "in", 2);
 
	close(fd);
	return 0;
}

/*GPIO Set value*/
int gpio_set_value(unsigned int gpio, unsigned int value)
{
	int fd;// len;
	char buf[MAX_BUF];
 
    snprintf(buf, sizeof(buf), GPIO_PATH "/gpio%d/value", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-value");
		return fd;
	}
 
	if (value)
		write(fd, "1", 1);
	else
		write(fd, "0", 1);
 
	close(fd);
	return 0;
}

