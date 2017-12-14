#ifndef IOCTL_SPI_H
#define IOCTL_SPI_H

#include <linux/ioctl.h>

/*Structure representing the data containing 
all the patterns to be displayed */
typedef struct pattern {
    /*each row represents the pattern index
    & each column to the specific pattern 
    index contains the data bytes for pattern   
    display on led matrix*/
    unsigned char pat[10][8];
} pattern;

/*command given to ioctl system call when a specific pattern needs to be set*/
#define SET_PATTERN _IOW('p',10,pattern *)


#endif 
