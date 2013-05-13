/********************************************************************
 * Description: gpiomap.c
 *              helper module to coordinate GPIO pin access
 *
 * Author: Michael Haberler <git@mah.priv.at>
 * License: GPL 2
 * Copyright (c) 2013.
 *
 ********************************************************************/


#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_bitops.h"
#include "hal.h"

#if !defined(BUILD_SYS_USER_DSO) 
#error "This driver is for usermode threads only"
#endif

#define GPIO_BITMAP_SIZE 1000

#define MODNAME "gpiomap"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Helper module for GPIO pin coordination between drivers");
MODULE_LICENSE("GPL");

static const char *modname = MODNAME;
static int comp_id;

int gpio_bitmap_size = GPIO_BITMAP_SIZE;
EXPORT_SYMBOL(gpio_bitmap_size);

_DECLARE_BITMAP(gpio_bitmap, bits);
EXPORT_SYMBOL(gpio_bitmap);

static char *platform = "bbb";
RTAPI_MP_STRING(platform, "platform to preset the GPIO bitmap and size for");


int rtapi_app_main(void) {
	comp_id = hal_init(modname);
	if(comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
		return -1;
	}
	rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", modname);
	hal_ready(comp_id);
	return 0;
}

void rtapi_app_exit(void) {
	hal_exit(comp_id);
}

