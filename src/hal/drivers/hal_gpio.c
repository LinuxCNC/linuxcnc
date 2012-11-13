/********************************************************************
* Description:  hal_gpio.c
*               Driver for the Raspberry Pi GPIO pins
*
* Author: Michael Haberler
* License: GPL Version 2
*    
* Copyright (c) 2012 All rights reserved.
*
* Last change: 
********************************************************************/

/*

 The driver creates a HAL pin and if it run in realtime a function
 as follows:

 Pin: 'skeleton.<portnum>.pin-<pinnum>-out'
 Function: 'skeleton.<portnum>.write'

 This skeleton driver also doesn't use arguments you can pass to the driver
 at startup. Please look at the parport driver how to implement this if you need
 this for your driver.

 (added 17 Nov 2006)
 The approach used for writing HAL drivers has evolved quite a bit over the
 three years since this was written.  Driver writers should consult the HAL
 User Manual for information about canonical device interfaces, and should
 examine some of the more complex drivers, before using this as a basis for
 a new driver.

*/


#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_bitops.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
                                /* this also includes config.h */
#include "hal.h"		/* HAL public API decls */

#if !defined(BUILD_SYS_USER_DSO) 
#error "This driver is for usermode threads only"
#endif
#if !defined(TARGET_PLATFORM_RASPBERRY)
#error "This driver is for the Raspberry platform only"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>


#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

// http://elinux.org/index.php?title=RPi_Low-level_peripherals&printable=yes
// Rev 1 Raspberry:
static unsigned char rev1_pins[] = {0, 1, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 21, 22, 23, 24, 25};
// Rev2 Raspberry:
static unsigned char rev2_pins[] = {2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27};
static int npins;

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
static int  mem_fd;
static unsigned char *gpio_mem, *gpio_malloc, *gpio_map;
// I/O access
volatile unsigned *gpio;
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(gpio, g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(gpio, g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(gpio,g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
#define GPIO_SET(gpio) (*(gpio+7))  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR(gpio) (*(gpio+10)) // clears bits which are 1 ignores bits which are 0

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Driver for Raspberry Pi GPIO pins");
MODULE_LICENSE("GPL");

// port direction bits, 1=output
static char *dir = "0x1ff"; // all output
RTAPI_MP_STRING(dir, "port direction, 1=output");
static unsigned dir_map;

static int comp_id;		/* component ID */
static unsigned char *pins;
hal_bit_t **port_data;

static void write_port(void *arg, long period);
static void read_port(void *arg, long period);

// figure Raspberry board revision number
static int rpi_revision()
{
  char *path = "/proc/cpuinfo",  *s, line[1024];
  int rev = -1;
  char *rev_line = "Revision";

  // parse /proc/cpuinfo for the line:
  // Revision        : 0003
  FILE *f = fopen(path,"r");
  if (!f) {
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL_GPIO: can't open %s: %d - %s\n",
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

static int  setup_gpio_access()
{
  // open /dev/mem 
  if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR,"HAL_GPIO: can't open /dev/mem \n");
    return -EPERM;
  }
  // mmap GPIO - allocate MAP block
  if ((gpio_mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL_GPIO: can't malloc(%d)\n", 
		    BLOCK_SIZE + (PAGE_SIZE-1));
    return -ENOMEM;
  }
  gpio_malloc = gpio_mem;
  // Make sure pointer is on 4K boundary
  if ((unsigned long)gpio_mem % PAGE_SIZE)
    gpio_mem += PAGE_SIZE - ((unsigned long)gpio_mem % PAGE_SIZE);

  // Now map it
  gpio_map = (unsigned char *)mmap(
				   (caddr_t)gpio_mem,
				   BLOCK_SIZE,
				   PROT_READ|PROT_WRITE,
				   MAP_SHARED|MAP_FIXED,
				   mem_fd,
				   GPIO_BASE
				   );

  if ((long)gpio_map < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL_GPIO: mmap failed: %d - %s\n", 
		    errno, strerror(errno));
    return -ENOMEM;;
  }
  // Always use volatile pointer!
  gpio = (volatile unsigned *)gpio_map;
  return 0;
} 

int rtapi_app_main(void)
{
    int n, retval = 0;
    int rev, pinno;
    char *endptr;

    if ((rev = rpi_revision()) < 0)
      return -1;

    switch (rev) {
    case 3:
      rtapi_print_msg(RTAPI_MSG_INFO, 
		      "Raspberry Model B Revision 1.0 + ECN0001 (no fuses, D14 removed)\n");
      pins = rev1_pins;
      npins = sizeof(rev1_pins);
      break;
    case 2:
      rtapi_print_msg(RTAPI_MSG_INFO, 
		      "Raspberry Model B Revision 1.0\n");
      pins = rev1_pins;
      npins = sizeof(rev1_pins);
      break;
      
    case 4:
    case 5:
    case 6:
      rtapi_print_msg(RTAPI_MSG_INFO, 
		      "Raspberry Model B Revision 2.0\n");
      pins = rev2_pins;
      npins = sizeof(rev2_pins);
      break;

    default:
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HAL_GPIO: ERROR: board revision %d not supported\n", rev);
	return -1;
    }
    port_data = hal_malloc(npins * sizeof(hal_bit_t));
    if (port_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_GPIO: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    if (dir == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL_GPIO: ERROR: no config string\n");
	return -1;
    }
    dir_map = strtoul(dir, &endptr,0);
    if (*endptr) {
	rtapi_print_msg(RTAPI_MSG_ERR, 
			"HAL_GPIO: dir=%s - trailing garbage: '%s'\n",
			dir, endptr);
	return -1;
    }

    if (setup_gpio_access())
      return -1;

    comp_id = hal_init("hal_gpio");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_GPIO: ERROR: hal_init() failed\n");
	return -1;
    }

    for (n = 0; n < npins; n++) {
      pinno = pins[n];
      if (dir_map & _BIT(n)) {
	if ((retval = hal_pin_bit_newf(HAL_OUT, &(port_data[n]),
				       comp_id, "hal_gpio.pin-%02d-out", pinno)) < 0)
	  break;
      } else {
	if ((retval = hal_pin_bit_newf(HAL_IN, &(port_data[n]),
				       comp_id, "hal_gpio.pin-%02d-in", pinno)) < 0)
	  break;
      }
    }
    if (retval < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
		      "HAL_GPIO: ERROR: pin %d export failed with err=%i\n", 
		      n,retval);
      hal_exit(comp_id);
      return -1;
    }

    retval = hal_export_funct("hal_gpio.write", write_port, port_data,
			      0, 0, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_GPIO: ERROR: write funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("hal_gpio.read", read_port, port_data,
			      0, 0, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_GPIO: ERROR: read funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"HAL_GPIO: installed driver\n");
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
  if (gpio_map)
    munmap(gpio_map, BLOCK_SIZE);
  close(mem_fd);
  if (gpio_malloc)
    free(gpio_malloc);
  hal_exit(comp_id);
}

static void write_port(void *arg, long period)
{
  //hal_gpio_t *port;
  //unsigned char outdata;
    // port = arg;

    //outdata = *(port->data_out) & 0xFF;
    /* write it to the hardware */
    // rtapi_outb(outdata, 0x378);
}

static void read_port(void *arg, long period)
{
  //hal_gpio_t *port;
  // unsigned char outdata;
    // port = arg;

    //outdata = *(port->data_out) & 0xFF;
    /* write it to the hardware */
    // rtapi_outb(outdata, 0x378);
}
