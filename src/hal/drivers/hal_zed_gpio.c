/********************************************************************
* Description:  zed_gpio.c
* Test driver for the ZedBoard GPIO pins
*
* \author Claudio Lorini (claudio.lorini@iit.it)
* License: GPL Version 2
* Copyright (c) 2014.
*
* code derived from the hal_gpio.c by:
* Author: Michael Haberler
* License: GPL Version 2
* Copyright (c) 2012.
*
* some code  taken from the bcm2835 library by:
* Author: Mike McCauley (mikem@open.com.au)
* Copyright (C) 2011 Mike McCauley    
*
********************************************************************/

/**
 \brief Test driver for the ZedBoard GPIO pins.

 \details This driver profides a series of digital IO connected to the
   PMods connectors JA1, JB1, JC1, JD1, JE1. The IO access uses the xilinx 
   gpio driver.
   This uncivilized method is an plain insult to optimization and performance
   and goes like this: 
   - export each IO pin writing in /sys/class/gpio/export the number <NN> 
     of the IO to use. This create a file in the /sys/class/gpio/ directory 
     for each pin.
   - set the direction of the pin writing in /sys/class/gpio/gpio<NN>/direction 
     'out' or 'in' according to direction.
   - Access each IO pin writing/reading '0' or '1' characters to the 
     /sys/class/gpio/gpio<NN>/value file. For unknown reasons, to read
     from the gpio<NN> file you have each time to fopen() and fclose(). 
     Continuous read from file returns EOF after the first read.
     Fseek(), rewind() ecc... doesn't work. 
   
 \par How to compile.
  In order to compile, this driver should be placed in the 
  /machinekit/src/hal/drivers/ directory
  in the the Makefile in the /machinekit/src directory add the following:
    ifeq ($(TARGET_PLATFORM), zedboard)
    obj-$(CONFIG_ZED_GPIO) += zed_gpio.o
    zed_gpio-objs := hal/drivers/zed_gpio.o
    endif
  in the '# Subdirectory: hal/drivers' section

  and add 
    ifeq ($(TARGET_PLATFORM),zedboard)
    $(RTLIBDIR)/zed_gpio$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(zed_gpio-objs))
    endif
  in the '# build kernel RTAPI modules' section

  in the the Makefile.in in the /machinekit/src directory add the following:
    CONFIG_ZED_GPIO=m
  in the '# HAL drivers' section

 \par IO connection table:
  IO0 JA1.1
  IO1 JA1.2
  ...

 \par Revision history:
 \date 22.01.2014 started development from hal_gpio.c files
 \version 00
 \date  07.02.2014 compiled from source tree and loaded successfully
 \version 01
 \date  12.02.2014 Inputs working at last! 
 \version 02

 \note
 \bug
 \todo
 \warning
 \pre 
 \param 
 \return
*/

#include "rtapi.h"         // rtapi_print_msg()
#include "rtapi_bitops.h"  // RTAPI_BIT(n)    
#include "rtapi_app.h"     // 
#include "hal.h"

#if !defined(BUILD_SYS_USER_DSO) 
    #error "This driver is for usermode threads only"
#endif
#if !defined(TARGET_PLATFORM_ZEDBOARD)
    #error "This driver is for the Zedboard platform only"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

MODULE_AUTHOR("Claudio Lorini");
MODULE_DESCRIPTION("Driver for Zedboard GPIOs");
MODULE_LICENSE("GPL");

// RT component ID
static int comp_id;

// array of gpio access file descriptors
static int *gpio_fd_map;

// number of available gpio
static int ngpio;

/*
Pin assignment with CAN8 board (Vita)
55,56,57,58,59,60,61,62  - Led CAN      (out)
63,64,65,66,67,68,69,70  - DIP Switch 1 (in) 
71,72,73,74,75,76,77,78  - J8 Outputs   (out)
79,80,81,82,83,84,85     - J6 Inputs    (in)
86,87,88,89,90,91,92     - J7 Inputs    (in)
*/

// Rev 01 Zedboard gpio assignment
// I1..I7 
static unsigned char gpio_list[] = {79,80,81,82,83,84,85};
//
static unsigned char *gpios;

// port direction as string, 1=output 0=input
static char *dir_list = "0"; // 00000000 -> I0, I1, I2, I3, I4, I5, I6, I7
// port map as word
static unsigned dir_map;
RTAPI_MP_STRING(dir_list, "port direction, 1=output");

// exclude list for gpios as string, 1=exclude 0=included
static char *exclude_list = "0"; // 00000000 -> all used
// exclude list word
static unsigned exclude_map;
RTAPI_MP_STRING(exclude_list, "exclude gpio, 1=dont use");

// array of bits, used for both input and output
hal_bit_t **port_data;

// used for debug pourpouses
hal_u32_t *dbg;

/**
 \brief Write IO function exported to hal
 \pre ngpio must be initialized */
static void write_port(void *arg, long period)
{
    int n;

    for (n = 0; n < ngpio; n++) {
        // check if IO in exclusion list
        if ( 0 != (exclude_map & RTAPI_BIT(n)) ) {
            continue;
        }
        if ( 0 != (dir_map & RTAPI_BIT(n)) ) {
            // n-th gpio is an output 
            if (*(port_data[n])) { 
                write(gpio_fd_map[n], "1", 2);
// \todo controlli su scrittura files o anche no per performance
            } 
            else {
                write(gpio_fd_map[n], "0", 2);
// \todo controlli su scrittura files o anche no per performance
            }
        }
    }
}

/**
 \brief Read IO function exported to hal
 \pre ngpio must be initialized 
 \bug the gpio file must be opne and closed each time, fseek or 
   rewind doesn't work */
static void read_port(void *arg, long period)
{
    int n=0, resp;
    static char value=0;
    char buf[50]; 

    for (n = 0; n < ngpio; n++) {
        // ignore if in exclusion list
        if ( 0 != (exclude_map & RTAPI_BIT(n)) ) {
            continue;
        }
        if ( 0 == (dir_map & RTAPI_BIT(n)) ) {
            /// \todo can be optimized creating the filenames in an array once and for all in setup_gpio_access()
            // inputs, filename to open is:
            sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio_list[n]);
            gpio_fd_map[n] = open(buf, O_RDONLY|O_SYNC);
            if ( gpio_fd_map[n] < 0 ) {
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't open /sys/class/gpio/gpio%d/value", gpio_list[n]);
            }
            // n-th gpio is an input 
            resp = read(gpio_fd_map[n], &value, 1);
            // less than 1 byte read from gpio-file 
            if ( 1 != resp ) {
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't read from gpio file");
            }
            // '0'
            if( 48 == value ) {
                *port_data[n] = 0x0;
            }
            else {
                *port_data[n] = 0x1;
            }

            // close the file
            close(gpio_fd_map[n]);
        }
    }
}

/**
 \brief Export and set direction of GPIOs
 \details For each GPIO in use the following operations should be performed:
   - write in /sys/class/gpio/export the number <NN> of the IO to use
   - write in /sys/class/gpio/gpio<NN>/direction 'out' or 'in' according to direction
   - open the /sys/class/gpio/gpio<NN>/value file for read or write to access GPIO data 

 \pre ngpio must be initialized first
 \return 
   0: everything ok.
   ....: failed to open gpio structures */
static int  setup_gpio_access()
{
    int fd, i;   
    char buf[50]; 
    
    // allocate space for file descriptors
    gpio_fd_map = hal_malloc(ngpio * sizeof(int));
// \todo controlli su allocazione memoria

    // export gpios
    if( ( fd = open("/sys/class/gpio/export", O_WRONLY|O_SYNC) ) >= 0 ) {  
        for( i = 0 ; i < ngpio; i++ ) { 
            // check if IO in exclusion list
            if ( 1 == (exclude_map & RTAPI_BIT(i)) ) {
                continue;
            }
            // if out of exclusion list export the pin
            sprintf(buf, "%d", gpio_list[i]);       
            if ( write(fd, buf, strlen(buf) ) != strlen(buf) ) {
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't write %d to /sys/class/gpio/export", gpio_list[i]);
                return -EPERM;   
            }
        }
        // close exports
        close(fd);
    }
    else {
        // unable to open gpio/export
        rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't open /sys/class/gpio/export \n");
        return -EPERM;
    }

    // setup IO directions   
    for( i = 0 ; i < ngpio; i++ ) { 
        // check if IO in exclusion list
        if ( 0 != (exclude_map & RTAPI_BIT(i)) ) {
            continue;
        }

        // open the gpio direction files to set IO direction
        sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio_list[i]);
        fd = open(buf, O_WRONLY|O_SYNC);
        if ( fd < 0 ) {
            rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't open /sys/class/gpio/gpio%d/direction", gpio_list[i]);
            return -EPERM;
        }
        // setup pin direction
        if ( 0 != (dir_map & RTAPI_BIT(i)) ) {
            // out direction
            if( write(fd, "out", 4) !=4 ) {
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't write 'out' to /sys/class/gpio/gpio%d/direction", gpio_list[i]);
                return -EPERM;
            } 
            rtapi_print_msg(RTAPI_MSG_DBG,"HAL_ZED_GPIO: written 'out' to /sys/class/gpio/gpio%d/direction", gpio_list[i]);
        } 
        else {
            // Set in direction
            if( write(fd, "in", 3) != 3 ) {
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't write 'in' to /sys/class/gpio/gpio%d/direction", gpio_list[i]);
                return -EPERM;
            } 
            rtapi_print_msg(RTAPI_MSG_DBG,"HAL_ZED_GPIO: written 'in' to /sys/class/gpio/gpio%d/direction", gpio_list[i]);
        }
        // close directions
        close(fd);

        // check pin direction
        if ( 0 != (dir_map & RTAPI_BIT(i)) ) {
            // outputs, files can be open once and left open
            // prepare name of file to open for gpio
            sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio_list[i]);
            // open as optput 
            gpio_fd_map[i] = open(buf, O_WRONLY|O_SYNC);
            if ( gpio_fd_map[i] < 0 ) {
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't open /sys/class/gpio/gpio%d/value", gpio_list[i]);
                return -EPERM;
            }
        }
        // else: for inputs the file must be opne and closed each access in read_port() 
    }
    return 0;
}

/**
 \brief   Determine Zynq revision
 \details Parse data in /proc/cpuinfo for 'Revision'
 \return 
    -1: unable to parse /proc/cpuinfo
    -1: unable to parse a version number
    nnnn: the one and only revision */
static int zynq_revision()
{
    char *path = "/proc/cpuinfo",  *s, line[1024];
    int rev = -1;
    // parse /proc/cpuinfo for the line: Revision
    char *rev_line = "Revision";
    FILE *f = fopen(path,"r");
  
    if (!f) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: can't open %s: %d - %s\n",
          path, errno, strerror(errno));
        return -1;
    }
  
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, rev_line, strlen(rev_line))) {
            s = strchr(line, ':');
            if (s && 1 == sscanf(s, ":%d", &rev)) {
            fclose(f);
            return rev;
            }
        }
    }
    fclose(f);
    return -1;
}

/**
 \brief   Determine Zedboard FPGA HW revision
 \details The FPGA can contain different resources, a version register determine 
   the available resources
 \todo    Do register read for FPGA versioning  
 \return 
    01: the one and only revision */
static int zb_revision()
{
    return 01;
}

/**
 \brief main realtime task */
int rtapi_app_main(void)
{
    // zynq and FPGA code revision
    int rev, zrev;
    // save messaging level 
    static int msg_level;
    int n, retval = 0;
    char *endptr;
    
    // save message level on entering 
    msg_level = rtapi_get_msg_level();
    
    /* setup messaging level in:
    RTAPI_MSG_NONE,
    RTAPI_MSG_ERR,
    RTAPI_MSG_WARN,
    RTAPI_MSG_INFO,
    RTAPI_MSG_DBG,
    RTAPI_MSG_ALL */
    rtapi_set_msg_level(RTAPI_MSG_ALL);

    // check Zynq revision 
    if ((zrev = zynq_revision()) < 0) {
        // unable to determine zynq revision 
        return -1;
    }
    // notify zynq revision
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: Zynq Revision %d \n", zrev);

    // check Zedboard FPGA hardware revision 
    rev = zb_revision();
  
    // do revision specific configuration
    switch (rev) {
        case 01:
            rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: Zedboard FPGA Revision 01\n");
            gpios = gpio_list;
            ngpio = sizeof(gpio_list);
        break;

        default:
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: FPGA revision %d not (yet) supported\n", rev);
        return -1;
        break;
    }
 
    // allocate space for port data
    port_data = hal_malloc(ngpio * sizeof(void *));
    if ( 0 == port_data ) {
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: hal_malloc() failed\n");
    hal_exit(comp_id);
    return -1;
    }

    // check direction configuration
    if ( 0 == dir_list ) {
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: no config string\n");
    return -1;
    }
    
    // pin direction
    dir_map = strtoul(dir_list, &endptr,0);
    if (*endptr) {
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: dir=%s - trailing garbage: '%s'\n",
          dir_list, endptr);
    return -1;
    }

    // check exclude list
    if ( 0 == exclude_list ) {
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: no exclude string\n");
    return -1;
    }

    // decode exclude list checking string format
    exclude_map = strtoul(exclude_list, &endptr,0);
    if (*endptr) {
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: exclude=%s - trailing garbage: '%s'\n",
          exclude_list, endptr);
    return -1;
    }

    // export and configure gpios
    if ( 0 != setup_gpio_access() ) {
      return -1;
    }

    // try to init the component
    comp_id = hal_init("hal_zed_gpio");
    if (comp_id < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: hal_init() failed\n");
    return -1;
    }

    // make available all gpios in hal eccept those in the exclude list
    for (n = 0; n < ngpio; n++) {
        if (exclude_map & RTAPI_BIT(n)) {
            continue;
        }
        
        if ( 0 != (dir_map & RTAPI_BIT(n)) ) {
            // phisical output pin component have an input pin! yes, it's counterintuitive but correct. 
        if ( (retval = hal_pin_bit_newf(HAL_IN, &port_data[n], comp_id, "hal_zed_gpio.pin-%02d-out", n) ) < 0) {
            break;
            } 
        } 
        else {
            // same as above, input pins have output pins! he, he, he kindafunny.
        if ( (retval = hal_pin_bit_newf(HAL_OUT, &port_data[n], comp_id, "hal_zed_gpio.pin-%02d-in", n) ) < 0) {
            break;
            }
        }
    }
    
    // allocate space for debug data
    dbg = hal_malloc(sizeof(hal_u32_t));
    // instantiate a debug port 
    n = hal_param_u32_newf(HAL_RW, dbg, comp_id, "hal_zed_gpio.debug-io");
    if(n != 0) return n;
    *dbg=0xCACA;

    // check for failed pin mapping
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: pin %d export failed with err=%i\n", n,retval);
        hal_exit(comp_id);
        return -1;
    }

    // export the read_port and write_port functions as hal_zed_gpio.read and hal_zed_gpio.write in hal
    retval = hal_export_funct("hal_zed_gpio.write", write_port, 0, 0, 0, comp_id);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: write funct export failed\n");
    hal_exit(comp_id);
    return -1;
    }
    retval = hal_export_funct("hal_zed_gpio.read", read_port, 0, 0, 0, comp_id);
    if (retval < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: read funct export failed\n");
    hal_exit(comp_id);
    return -1;
    }

    // all operations succeded
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: driver installed successfully.\n");
    hal_ready(comp_id);

    // return to previous mesaging level
    rtapi_set_msg_level(msg_level);

    return 0;
}

/** 
 \brief Exit component closing all gpio file descriptors 
 \pre ngpio must be initialized */
void rtapi_app_exit(void)
{
    int i, fd;
    char buf[50];
    
    // close gpio output files
    for( i = 0 ; i < ngpio; i++ ) { 
    /// \todo only outputs files have to be closed, inputs are opened and closed each time
        close(gpio_fd_map[i]);
    }

    // unexport gpios
    if( ( fd = open("/sys/class/gpio/unexport", O_WRONLY|O_SYNC) ) >= 0 ) {  
        for( i = 0 ; i < ngpio; i++ ) { 
            // check if IO in exclusion list
            if ( 0 != (exclude_map & RTAPI_BIT(i)) ) {
                continue;
            }
            // if out of exclusion list unexport the pin
            sprintf(buf, "%d", gpio_list[i]);       
            if ( write(fd, buf, strlen(buf)) != strlen(buf) ) {
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't write %d to /sys/class/gpio/unexport", gpio_list[i]);
            } 
        }           
        // close unexports
        close(fd);
    }
    else {
        // unable to open gpio/unexport
        rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_GPIO: can't open /sys/class/gpio/unexport \n");
    }

    // notify clean termination
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: component terminated successfully \n");

    hal_exit(comp_id);
}

