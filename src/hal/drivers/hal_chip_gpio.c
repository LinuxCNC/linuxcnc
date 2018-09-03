/********************************************************************
* Description:  hal_chip_gpio.c
*               Driver for the C.H.I.P. GPIO pins
*
* Author: Alexander Rössler <mail@roessler.systems>
* License: GPL Version 2
* Copyright (c) 2016.
*
********************************************************************/


#include "rtapi.h"
#include "rtapi_app.h"

#include "hal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

#include "chip/libsoc_mmap_gpio.h"

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif
#if !defined(TARGET_PLATFORM_CHIP)
#error "This driver is for the BeagleBone platform only"
#endif

#define MODNAME "hal_chip_gpio"

MODULE_AUTHOR("Alexander Rössler");
MODULE_DESCRIPTION("Driver for C.H.I.P. GPIO pins");
MODULE_LICENSE("GPL");

#define HEADERS          1
#define PINS_PER_HEADER  8

typedef struct {
    hal_bit_t* input_pins[PINS_PER_HEADER * HEADERS]; // array of pointers to bivts
    hal_bit_t* output_pins[PINS_PER_HEADER * HEADERS]; // array of pointers to bits
    hal_bit_t  *input_inv[PINS_PER_HEADER * HEADERS];
    hal_bit_t  *output_inv[PINS_PER_HEADER * HEADERS];
} port_data_t;

static port_data_t *port_data;
static const char *modname = MODNAME;
static mmap_gpio pins[PINS_PER_HEADER * HEADERS];

static void write_port(void *arg, long period);
static void read_port(void *arg, long period);

static int comp_id;
static int num_ports;

static char *input_pins;
RTAPI_MP_STRING(input_pins, "input pins, comma separated.  P0-P7 use 0-7");

static char *output_pins;
RTAPI_MP_STRING(output_pins, "output pins, comma separated.  P0-P7 use 0-7");

static int get_gpio(mmap_gpio *gpio, int raw_pin)
{
    int pin_id = raw_pin + 132;
    char port = (char)(pin_id / 32) + 'A';
    int pin = pin_id % 32;

    return libsoc_mmap_gpio_request(gpio, port, pin);
}

static void configure_control_module() {
    int ret;

    ret = libsoc_mmap_gpio_init();
    if (ret != 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: Unable to map Control Module: %s", modname, strerror(errno));
        exit(1);
    }
}

int rtapi_app_main(void) {
    char name[HAL_NAME_LEN + 1];
    int n, retval;
    char *data, *token;

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

    // map control module memory
    configure_control_module();

    // configure input pins
    if(input_pins != NULL) {
        data = input_pins;
        while((token = strtok(data, ",")) != NULL) {
            int pin = strtol(token, NULL, 10);
            mmap_gpio *gpio_pin;

            if((pin < 0) || (pin > 7)) {
                rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: invalid pin number '%d'. Valid pins are 0-7 for P0-P7.\n", modname, pin);
                hal_exit(comp_id);
                return -1;
            }

            data = NULL; // after the first call, subsequent calls to strtok need to be on NULL

            // Add HAL pin
            retval = hal_pin_bit_newf(HAL_OUT, &(port_data->input_pins[pin]), comp_id, "chip_gpio.in-%02d", pin);

            if(retval < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %02d could not export pin, err: %d\n", modname, pin, retval);
                hal_exit(comp_id);
                return -1;
            }

            // Add HAL pin
            retval = hal_pin_bit_newf(HAL_IN, &(port_data->input_inv[pin]), comp_id, "chip_gpio.in-%02d.invert", pin);

            if(retval < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %02d could not export pin, err: %d\n", modname, pin, retval);
                hal_exit(comp_id);
                return -1;
            }

            // Initialize HAL pin
            *(port_data->input_inv[pin]) = 0;

            // Initialize GPIO pin
	    gpio_pin = &pins[pin];
            if (!get_gpio(gpio_pin, pin))
            {
                rtapi_print("%s: ERROR: failed to open GPIO pin %d", modname, pin);
                hal_exit(comp_id);
                return -1;
            }

            retval = libsoc_mmap_gpio_set_direction(gpio_pin, INPUT);
            if (retval == DIRECTION_ERROR)
            {
                rtapi_print("%s: ERROR: failed to set GPIO direction %d", modname, pin);
                hal_exit(comp_id);
                return -1;
            }
            rtapi_print("pin %d setup with mode input\n", pin);
        }
    }


    // configure output pins
    if(output_pins != NULL) {
        data = output_pins;
        while((token = strtok(data, ",")) != NULL) {
            int pin = strtol(token, NULL, 10);
            mmap_gpio *gpio_pin;

            if((pin < 0) || (pin > 7)) {
                rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: invalid pin number '%d'. Valid pins are 0-7 for P0-P7.\n", modname, pin);
                hal_exit(comp_id);
                return -1;
            }

            data = NULL; // after the first call, subsequent calls to strtok need to be on NULL

            // Add HAL pin
            retval = hal_pin_bit_newf(HAL_IN, &(port_data->output_pins[pin]), comp_id, "chip_gpio.out-%02d", pin);

            if(retval < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %02d could not export pin, err: %d\n", modname, pin, retval);
                hal_exit(comp_id);
                return -1;
            }

            // Add HAL pin
            retval = hal_pin_bit_newf(HAL_IN, &(port_data->output_inv[pin]), comp_id, "chip_gpio.out-%02d.invert", pin);

            if(retval < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %02d could not export pin, err: %d\n", modname, pin, retval);
                hal_exit(comp_id);
                return -1;
            }

            // Initialize HAL pin
            *(port_data->output_inv[pin]) = 0;

            // Initialize GPIO pin
	    gpio_pin = &pins[pin];
            if (!get_gpio(gpio_pin, pin))
            {
                rtapi_print("%s: ERROR: failed to open GPIO pin %d", modname, pin);
                hal_exit(comp_id);
                return -1;
            }

            retval = libsoc_mmap_gpio_set_direction(gpio_pin, OUTPUT);
            if (retval == DIRECTION_ERROR)
            {
                rtapi_print("%s: ERROR: failed to set GPIO direction %d", modname, pin);
                hal_exit(comp_id);
                return -1;
            }
            rtapi_print("pin %d setup with mode output\n", pin);
        }
    }


    // export functions
    rtapi_snprintf(name, sizeof(name), "chip_gpio.write");
    retval = hal_export_funct(name, write_port, port_data, 0, 0, comp_id);
    if(retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: port %d write funct export failed\n", modname, n);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_snprintf(name, sizeof(name), "chip_gpio.read");
    retval = hal_export_funct(name, read_port, port_data, 0, 0, comp_id);
    if(retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: port %d read funct export failed\n", modname, n);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", modname);

    hal_ready(comp_id);

    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
    libsoc_mmap_gpio_shutdown();
}

static void write_port(void *arg, long period)
{
    int i;
    port_data_t *port = (port_data_t *)arg;

    // set output states
    for(i=0; i<HEADERS*PINS_PER_HEADER; i++) {
        int retval;

        if (port->output_pins[i] == NULL) {
            continue; // short circuit if hal hasn't malloc'd a bit at this location
        }

        mmap_gpio *pin = &pins[i];

        if((*port->output_pins[i] ^ *(port->output_inv[i])) == 0) {
            retval = libsoc_mmap_gpio_set_level(pin, LOW);
        }
        else {
            retval = libsoc_mmap_gpio_set_level(pin, HIGH);
        }
        if (retval == LEVEL_ERROR) {
            rtapi_print("%s: ERROR: failed to set GPIO pin %d", modname, i);
            return;
        }
    }
}

static void read_port(void *arg, long period)
{
    int i;
    port_data_t *port = (port_data_t *)arg;

    // read input states
    for(i=0; i<HEADERS*PINS_PER_HEADER; i++) {
        if(port->input_pins[i] == NULL) {
            continue; // short circuit if hal hasn't malloc'd a bit at this location
        }

        mmap_gpio *pin;
        mmap_gpio_level level;

        pin = &pins[i];
        level = libsoc_mmap_gpio_get_level(pin);

        if (level == LEVEL_ERROR)
        {
            rtapi_print("%s: ERROR: failed to read GPIO pin %d", modname, i);
            return;
        }
        else
        {
            *port->input_pins[i] = (hal_bit_t)(level == HIGH) ^ *(port->input_inv[i]);
        }
    }
}
