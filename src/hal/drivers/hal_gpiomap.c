/********************************************************************
 * Description: gpiomap.c
 *              helper module to coordinate GPIO pin access
 *
 * Author: Michael Haberler <git@mah.priv.at>
 * License: GPL 2
 * Copyright (c) 2013.
 *
 *  Usage:
 *  the gpiomap module should be loaded like so:
 *
 *  halcmd loadrt gpiomap platform=<platform> [reserve=<list of extra reserved pins>]
 *
 * example: reserve all GPIO pins unavailable in the BeagleBone Black; ontop of those,
 * block pins 42 and 45 from use by other HAL drivers:
 *
 *  halcmd loadrt gpiomap platform=bbb reserve=42,45
 *
 * Using code (HAL drivers using GPIO pins):
 *
 *  a using module should include "gpiomap.h"
 *
 *  number of pins described for the current platform: 
 *       int gpio_npins(void);
 *
 *  test pin availability (to test & allocated, pass reserve=1):
 *       int gpio_test_pin(int pinno, int reserve);
 *       this returns 0 on success and < 0 if the pin already was allocated
 *
 * free a pin (usage example: rtapi_app_exit() of a HAL driver):
 *       int gpio_free_pin(int pinno);
 *       this returns 0 on success and < 0 if the pin was unallocated
 *
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_bitops.h"
#include "hal.h"
#include "hal_gpiomap.h"

// maps of reserved pin numbers used otherwise (eg mmc flash, HDMI etc)
// and hence not available for HAL drivers

// to be completed by the platform gurus, please:
int bbw_map[] =   { 1,2,3,4 };
int bbb_map[] =   { 1,2,3,4 };
int rpiv1_map[] = { 1,2,3,4 };
int rpiv2_map[] = { 1,2,3,4 };

static struct {
    char *name;
    char *comment;
    int npins;
    int *reserved_pins;
    int mapsize;
} pinmap[] = {
    // extend as needed for board of the week:
    { "bbw", "BeagleBone white", 200, bbw_map, sizeof(bbw_map)/sizeof(bbw_map[0]) },
    { "bbb", "BeagleBone black", 200, bbb_map, sizeof(bbb_map)/sizeof(bbb_map[0]) },
    { "rpiv1", "Raspberry Model B Revision 1.0",  200, rpiv1_map,
      sizeof(rpiv1_map)/sizeof(rpiv1_map[0])},
    { "rpiv2", "Raspberry Model B Revision 2.0",  200, rpiv2_map,
      sizeof(rpiv2_map)/sizeof(rpiv2_map[0])},
    { NULL, NULL },
};

#define MODNAME "gpiomap"
#define DELIMITERS ",;:"

#ifdef EXPORT_BITMAP_SYMBOLS
#define STATIC 
#else
#define STATIC static
#endif

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Helper module for GPIO pin coordination between drivers");
MODULE_LICENSE("GPL");

static const char *modname = MODNAME;
static int comp_id;
static unsigned long gpio_bitmap_mutex;

STATIC int gpio_bitmap_size = GPIO_BITMAP_SIZE;
STATIC RTAPI_DECLARE_BITMAP(gpio_bitmap, GPIO_BITMAP_SIZE);

static char *platform = "bbb";
RTAPI_MP_STRING(platform, "platform to preset the GPIO bitmap and size for");

static char *reserve = "";
RTAPI_MP_STRING(reserve, "list of extra pins to reserve; example: reserve=42,47,192");

static char *description = "(unset)";

int rtapi_app_main(void)
{
    int n = 0, i, pin;
    char *token, *cp;
    int fail = 0;

    while (1) {
	if (pinmap[n].name == NULL) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "%s: ERROR: no such platform '%s'; valid platforms are:\n", 
			    modname, platform);
	    for (n = 0; pinmap[n].name != NULL; n++)
		rtapi_print_msg(RTAPI_MSG_ERR, "\t%s\t - %s\n", 
				pinmap[n].name, pinmap[n].comment);
	    return -EINVAL;
	}
	if (!strcmp(platform,pinmap[n].name))
	    break;
	n++;
    }
    // reserve pins not available for HAL drivers as per platform map
    for (i = 0; i < pinmap[n].mapsize; i++) {
	pin = pinmap[n].reserved_pins[i];
	if (gpio_test_pin(pin, 1)) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: preallocate pin %d failed\n", 
		    modname, pin);
	    return -EINVAL;
	}
    }
    // reserve pins passed in as per 'reserve=<pinlist>'
    token = strtok (reserve, DELIMITERS);
    while (token) {
	cp = token;
	pin = strtoul(token, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    // invalid chars in string
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: reserve: value '%s' invalid for int (%s)\n",
			    modname, token, cp);
	    return -EINVAL;
	}
	if (gpio_test_pin(pin, 1)) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: pin %d already reserved\n", 
			    modname, pin);
	    fail++;
	}
	token = strtok (NULL, DELIMITERS);
    }
    if (fail)
	return -EINVAL;
    description = pinmap[n].comment;

    comp_id = hal_init(modname);
    if(comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "%s: gpiomap %s - %s loaded\n", 
		    modname, pinmap[n].name, pinmap[n].comment);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

static void autorelease_mutex(void *variable)
{
    rtapi_mutex_give(&gpio_bitmap_mutex);
}

int gpio_test_pin(int pinno, int reserve)
{
    int dummy __attribute__((cleanup(autorelease_mutex)));

    rtapi_mutex_get(&gpio_bitmap_mutex);
    if ((pinno < 0) || (pinno > gpio_bitmap_size-1)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin number %d invalid (0..%d)\n",
			modname, pinno, gpio_bitmap_size-1);
	return -EINVAL;
    }
    if (RTAPI_BIT_TEST(gpio_bitmap, pinno)) {
	rtapi_print_msg(RTAPI_MSG_DBG, "%s: gpio_test_pin(%d) - pin already allocated\n",
			modname, pinno);
	return -EINVAL;
    }
    if (reserve)
	RTAPI_BIT_SET(gpio_bitmap, pinno);
    return 0;
}

int gpio_free_pin(int pinno)
{
    int dummy __attribute__((cleanup(autorelease_mutex)));

    if ((pinno < 0) || (pinno > gpio_bitmap_size-1)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin number %d invalid (0..%d)\n",
			modname, pinno, gpio_bitmap_size-1);
	return -EINVAL;
    }
    if (!RTAPI_BIT_TEST(gpio_bitmap, pinno)) {
	rtapi_print_msg(RTAPI_MSG_DBG, "%s: gpio_free_pin(%d) - pin unallocated\n",
			modname, pinno);
	return -EINVAL;
    }
    RTAPI_BIT_CLEAR(gpio_bitmap, pinno);
    return 0;
}

int gpio_npins(void)
{
    return gpio_bitmap_size;
}

char *gpio_platform(void)
{
    return description;
}

#ifdef EXPORT_BITMAP_SYMBOLS
EXPORT_SYMBOL(gpio_bitmap);
EXPORT_SYMBOL(gpio_bitmap_size);
#endif

EXPORT_SYMBOL(gpio_npins);
EXPORT_SYMBOL(gpio_platform);
EXPORT_SYMBOL(gpio_free_pin);
EXPORT_SYMBOL(gpio_test_pin);
