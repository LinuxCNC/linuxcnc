/********************************************************************
 * Description: gpiouse.c
 *              example module to show coordinated GPIO pin access
 *
 * Author: Michael Haberler <git@mah.priv.at>
 * License: GPL 2
 * Copyright (c) 2013.
 *
 *  Usage:
 *  first load hal_gpiomap - see comments in src/hal/drivers/hal_gpiomap.c
 *
 *  the gpiouse module should be loaded like so:
 *
 *  halcmd loadrt gpiouse claim=<list of pins to be claimed by this driver>
 *  if one of the pins is reserved by hal_gpiomap or some other driver
 *  loaded before, the loadrt should fail with an error message
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_bitops.h"
#include "hal.h"
#include "hal_gpiomap.h"

#define MODNAME "gpiouse"
#define DELIMITERS ",;:"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("demo module for GPIO pin coordination between drivers");
MODULE_LICENSE("GPL");

static const char *modname = MODNAME;
static int comp_id;

static char *claim = "";
RTAPI_MP_STRING(claim, "list of pins to claim - example: claim=42,47,192");

static RTAPI_DECLARE_BITMAP(my_bitmap, GPIO_BITMAP_SIZE);

static void free_my_pins()
{
    int i;
    for (i = 0; i < gpio_npins(); i++) 
	if (RTAPI_BIT_TEST(my_bitmap, i))
	    gpio_free_pin(i);
}

int rtapi_app_main(void)
{
    int pin;
    char *token, *cp;
    int fail = 0;

    token = strtok (claim, DELIMITERS);
    while (token) {
	cp = token;
	pin = strtoul(token, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    // invalid chars in string
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: claim: value '%s' invalid for int (%s)\n",
			    modname, token, cp);
	    return -EINVAL;
	}
	if (gpio_test_pin(pin, 1) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: pin %d already reserved\n", 
			    modname, pin);
	    fail++;
	} else
	    RTAPI_BIT_SET(my_bitmap, pin);
	token = strtok (NULL, DELIMITERS);
    }
    if (fail) {
	free_my_pins(); // free pins allocated before exiting
	return -EINVAL;
    }
    comp_id = hal_init(modname);
    if(comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
	return -1;
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    free_my_pins();
    hal_exit(comp_id);
}

