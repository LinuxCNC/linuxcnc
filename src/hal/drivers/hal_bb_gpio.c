/********************************************************************
* Description:  hal_bb_gpio.c
*               Driver for the BeagleBone GPIO pins
*
* Author: Ian McMahon <imcmahon@prototechnical.com>
* License: GPL Version 2
* Copyright (c) 2013.
*
********************************************************************/


#include "rtapi.h"		
#include "rtapi_app.h"	

#include "hal.h"	

#if !defined(BUILD_SYS_USER_DSO) 
#error "This driver is for usermode threads only"
#endif
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the BeagleBone platform only"
#endif

#define MODNAME "hal_bb_gpio"

MODULE_AUTHOR("Ian McMahon");
MODULE_DESCRIPTION("Driver for BeagleBone GPIO pins");
MODULE_LICENSE("GPL");

#define PORTS 			 2
#define PINS_PER_PORT 46

typedef struct {
	hal_bit_t* pins[PINS_PER_PORT]; // array of pointers to bits
} port_data_t;

static port_data_t *port_data;

static const char *modname = MODNAME;

static void write_port(void *arg, long period);
//static void read_port(void *arg, long period);

static int comp_id; 
static int num_ports;

int rtapi_app_main(void) {
	char name[HAL_NAME_LEN + 1];
	int i, n, retval;

	num_ports = 1;
	n = 0; // port number... only one for now

	// init driver
	comp_id = hal_init(modname);
	if(comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
		return -1;
	}

	// allocate port memory
	port_data = hal_malloc(num_ports * sizeof(port_data_t));
	if(port_data == 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}

	for(i=0; i<PINS_PER_PORT; i++) {
		port_data[n].pins[i] = hal_malloc(sizeof(hal_bit_t));
		if(port_data[n].pins[i] == 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", modname);
			hal_exit(comp_id);
			return -1;
		}
	}

	// export pins
	for(i=0; i<PINS_PER_PORT; i++) {
		rtapi_print("%s: pointer to the pin: %p\n", modname, (port_data[n].pins[i]));
		rtapi_print("%s: pointer to the pointer to the pin: %p\n", modname, &(port_data[n].pins[i]));
		retval = hal_pin_bit_newf(HAL_IN, &(port_data[n].pins[i]), comp_id, "bb_gpio.%d.in-%02d", n, i);
		if(retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: port %d, pin %02d could not export pin, err: %d\n", modname, n, i, retval);
			hal_exit(comp_id);
			return -1;
		}
	}

	// export functions
	rtapi_snprintf(name, sizeof(name), "bb_gpio.%d.write", n);
	retval = hal_export_funct(name, write_port, &(port_data[n]), 0, 0, comp_id);
	if(retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: port %d write funct export failed\n", modname, n);
		hal_exit(comp_id);
		return -1;
	}

	rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", modname);

	hal_ready(comp_id);

	return 0;
}

void rtapi_app_exit(void) {
	hal_exit(comp_id);
}

static void write_port(void *arg, long period) {

}


/*
static void read_port(void *arg, long period) {

}
*/
