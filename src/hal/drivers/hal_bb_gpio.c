/********************************************************************
* Description:  hal_bb_gpio.c
*               Driver for the BeagleBone GPIO pins
*
* Author: Ian McMahon <imcmahon@prototechnical.com>
* License: GPL Version 2
* Copyright (c) 2013.
* Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
*
********************************************************************/


#include "gomc_env.h"
#include "gomc_log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "beaglebone_gpio.h"

#define MODNAME "hal_bb_gpio"

#define HEADERS              2
#define PINS_PER_HEADER  46

typedef struct {
    gomc_hal_bit_t* led_pins[4];
    gomc_hal_bit_t* input_pins[1 + PINS_PER_HEADER * HEADERS]; // array of pointers to bivts
    gomc_hal_bit_t* output_pins[1 + PINS_PER_HEADER * HEADERS]; // array of pointers to bits
    gomc_hal_bit_t  *led_inv[4];
    gomc_hal_bit_t  *input_inv[1 + PINS_PER_HEADER * HEADERS];
    gomc_hal_bit_t  *output_inv[1 + PINS_PER_HEADER * HEADERS];
} port_data_t;

typedef struct {
    cmod_t cmod;
    const cmod_env_t *env;
    const gomc_log_t *log;
    const gomc_rtapi_t *rtapi;
    int comp_id;

    port_data_t *port_data;
    int num_ports;

    char *user_leds;
    char *input_pins;
    char *output_pins;
} inst_t;

static const char *modname = MODNAME;

static void write_port(void *arg, long period);
static void read_port(void *arg, long period);

static off_t start_addr_for_port(int port);
static void configure_pin(bb_gpio_pin *pin, char mode);
static void hal_bb_gpio_destroy(cmod_t *self);

int configure_control_module(inst_t *inst) {
    int fd = open("/dev/mem", O_RDWR);

    control_module = mmap(0, CONTROL_MODULE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CONTROL_MODULE_START_ADDR);

    if(control_module == MAP_FAILED) {
        gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: Unable to map Control Module: %s", strerror(errno));
        return -errno;
    }

    close(fd);
    return 0;
}

int configure_gpio_port(inst_t *inst, int n) {
    const gomc_hal_t *hal = inst->env->hal;
    volatile void *cm_per;  // pointer to clock manager registers
    volatile unsigned int *regptr;
    unsigned int regvalue;

    int fd = open("/dev/mem", O_RDWR);

    gpio_ports[n] = hal->malloc(hal->ctx, sizeof(bb_gpio_port));

    // need to verify that port is enabled and clocked before accessing it
    // port 0 is always mapped, the others need checked
    if ( n > 0 ) {
	cm_per = mmap(0,CM_PER_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CM_PER_ADDR);
	if(cm_per == MAP_FAILED) {
	    gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: Unable to map Clock Module: %s", strerror(errno));
	    return -errno;
	}
	// point at CM_PER_GPIOn_CLKCTRL register for port n
	regptr = cm_per + CM_PER_GPIO1_CLKCTRL_OFFSET + 4*(n-1);
	regvalue = *regptr;
	// check for port enabled
	if ( (regvalue & CM_PER_GPIO_CLKCTRL_MODMODE_MASK ) != CM_PER_GPIO_CLKCTRL_MODMODE_ENABLED ) {
	    gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: GPIO Port %d is not enabled in device tree", n);
	    return -ENODEV;
	}
	munmap((void *)cm_per, CM_PER_LEN);
    }

    gpio_ports[n]->gpio_addr = mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, start_addr_for_port(n));

    if(gpio_ports[n]->gpio_addr == MAP_FAILED) {
        gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: Unable to map GPIO: %s", strerror(errno));
        return -errno;
    }

    gpio_ports[n]->oe_reg = gpio_ports[n]->gpio_addr + GPIO_OE;
    gpio_ports[n]->setdataout_reg = gpio_ports[n]->gpio_addr + GPIO_SETDATAOUT;
    gpio_ports[n]->clrdataout_reg = gpio_ports[n]->gpio_addr + GPIO_CLEARDATAOUT;
    gpio_ports[n]->datain_reg = gpio_ports[n]->gpio_addr + GPIO_DATAIN;


    gomc_log_infof(inst->log, "hal_bb_gpio", "memmapped gpio port %d to %p, oe: %p, set: %p, clr: %p", n, gpio_ports[n]->gpio_addr, gpio_ports[n]->oe_reg, gpio_ports[n]->setdataout_reg, gpio_ports[n]->clrdataout_reg);

    close(fd);
    return 0;
}

static void parse_argv(inst_t *inst, int argc, const char **argv) {
    for (int i = 0; i < argc; i++) {
	if (strncmp(argv[i], "user_leds=", 10) == 0) {
	    inst->user_leds = (char *)argv[i] + 10;
	} else if (strncmp(argv[i], "input_pins=", 11) == 0) {
	    inst->input_pins = (char *)argv[i] + 11;
	} else if (strncmp(argv[i], "output_pins=", 12) == 0) {
	    inst->output_pins = (char *)argv[i] + 12;
	}
    }
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    const gomc_hal_t *hal = env->hal;
    char fname[GOMC_HAL_NAME_LEN + 1];
    int n, retval;
    char *data, *token;

    inst_t *inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;
    inst->env = env;
    inst->log = env->log;
    inst->rtapi = env->rtapi;

    parse_argv(inst, argc, argv);

    inst->num_ports = 1;
    n = 0; // port number... only one for now

    // init driver
    int r = hal->init(hal->ctx, modname, env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if(r < 0) {
        gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: hal_init() failed");
        inst->rtapi->free(inst->rtapi->ctx, inst);
        return -1;
    }
    inst->comp_id = r;

    // allocate port memory
    inst->port_data = hal->malloc(hal->ctx, inst->num_ports * sizeof(port_data_t));
    if(inst->port_data == 0) {
        gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: hal_malloc() failed");
        hal->exit(hal->ctx, inst->comp_id);
        inst->rtapi->free(inst->rtapi->ctx, inst);
        return -1;
    }

    port_data_t *port_data = inst->port_data;

    // map control module memory
    int result = configure_control_module(inst);
    if(result < 0) {
        hal->exit(hal->ctx, inst->comp_id);
        inst->rtapi->free(inst->rtapi->ctx, inst);
        return result;
    }

    // configure userleds
    if(inst->user_leds != NULL) {
        data = inst->user_leds;
        while((token = strtok(data, ",")) != NULL) {
            int led = strtol(token, NULL, 10);

            data = NULL;

            if(user_led_gpio_pins[led].claimed != 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: userled%d is not available as a GPIO.", led);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            // Add HAL pin
            retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &(port_data->led_pins[led]), inst->comp_id, "bb_gpio.userled%d", led);

            if(retval < 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: userled %d could not export pin, err: %d", led, retval);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            // Add HAL pin
            retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &(port_data->led_inv[led]), inst->comp_id, "bb_gpio.userled%d-invert", led);

            if(retval < 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: userled %d could not export pin, err: %d", led, retval);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            // Initialize HAL pin
            *(port_data->led_inv[led]) = 0;

            int gpio_num = user_led_gpio_pins[led].port_num;
            // configure gpio port if necessary
            if(gpio_ports[gpio_num] == NULL) {
                int result = configure_gpio_port(inst, gpio_num);
                if(result < 0) {
                    hal->exit(hal->ctx, inst->comp_id);
                    inst->rtapi->free(inst->rtapi->ctx, inst);
                    return result;
                }
            }

            user_led_gpio_pins[led].port = gpio_ports[gpio_num];

            configure_pin(&user_led_gpio_pins[led], 'O');
        }
    }

    // configure input pins
    if(inst->input_pins != NULL) {
        data = inst->input_pins;
        while((token = strtok(data, ",")) != NULL) {
            int pin = strtol(token, NULL, 10);
            int header;
            bb_gpio_pin *bbpin;

            // Fixup old pin numbering scheme:
            // P8/P9 was 1xx/2xx, now 8xx/9xx
            if (pin < 300)
                pin += 700;

            if(pin < 801 || pin > 946 || (pin > 846 && pin < 901)) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: invalid pin number '%d'.  Valid pins are 801-846 for P8 pins, 901-946 for P9 pins.", pin);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            if(pin < 900) {
                pin -= 800;
                bbpin = &p8_pins[pin];
                header = 8;
            } else {
                pin -= 900;
                bbpin = &p9_pins[pin];
                header = 9;
            }

            if(bbpin->claimed != 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: pin p%d.%02d is not available as a GPIO.", header, pin);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            data = NULL; // after the first call, subsequent calls to strtok need to be on NULL

            // Add HAL pin
            retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &(port_data->input_pins[pin + (header - 8)*PINS_PER_HEADER]), inst->comp_id, "bb_gpio.p%d.in-%02d", header, pin);

            if(retval < 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: pin p%d.%02d could not export pin, err: %d", header, pin, retval);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            // Add HAL pin
            retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &(port_data->input_inv[pin + (header - 8)*PINS_PER_HEADER]), inst->comp_id, "bb_gpio.p%d.in-%02d-invert", header, pin);

            if(retval < 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: pin p%d.%02d could not export pin, err: %d", header, pin, retval);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            // Initialize HAL pin
            *(port_data->input_inv[pin + (header - 8)*PINS_PER_HEADER]) = 0;

            int gpio_num = bbpin->port_num;

            // configure gpio port if necessary
            if(gpio_ports[gpio_num] == NULL) {
                int result = configure_gpio_port(inst, gpio_num);
                if(result < 0) {
                    hal->exit(hal->ctx, inst->comp_id);
                    inst->rtapi->free(inst->rtapi->ctx, inst);
                    return result;
                }
            }

            bbpin->port = gpio_ports[gpio_num];

            configure_pin(bbpin, 'U');
            gomc_log_infof(inst->log, "hal_bb_gpio", "pin %d maps to pin %d-%d, mode %d", pin, bbpin->port_num, bbpin->pin_num, bbpin->claimed);
        }
    }

    // configure output pins
    if(inst->output_pins != NULL) {
        data = inst->output_pins;
        while((token = strtok(data, ",")) != NULL) {
            int pin = strtol(token, NULL, 10);
            int header;
            bb_gpio_pin *bbpin;

            // Fixup old pin numbering scheme:
            // P8/P9 was 1xx/2xx, now 8xx/9xx
            if (pin < 300)
                pin += 700;

            if(pin < 801 || pin > 946 || (pin > 846 && pin < 901)) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: invalid pin number '%d'.  Valid pins are 801-846 for P8 pins, 901-946 for P9 pins.", pin);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            if(pin < 900) {
                pin -= 800;
                bbpin = &p8_pins[pin];
                header = 8;
            } else {
                pin -= 900;
                bbpin = &p9_pins[pin];
                header = 9;
            }

            if(bbpin->claimed != 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: pin p%d.%02d is not available as a GPIO.", header, pin);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            data = NULL; // after the first call, subsequent calls to strtok need to be on NULL

            // Add HAL pin
            retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &(port_data->output_pins[pin + (header - 8)*PINS_PER_HEADER]), inst->comp_id, "bb_gpio.p%d.out-%02d", header, pin);

            if(retval < 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: pin p%d.%02d could not export pin, err: %d", header, pin, retval);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            // Add HAL pin
            retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &(port_data->output_inv[pin + (header - 8)*PINS_PER_HEADER]), inst->comp_id, "bb_gpio.p%d.out-%02d-invert", header, pin);

            if(retval < 0) {
                gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: pin p%d.%02d could not export pin, err: %d", header, pin, retval);
                hal->exit(hal->ctx, inst->comp_id);
                inst->rtapi->free(inst->rtapi->ctx, inst);
                return -1;
            }

            // Initialize HAL pin
            *(port_data->output_inv[pin + (header - 8)*PINS_PER_HEADER]) = 0;

            int gpio_num = bbpin->port_num;

            // configure gpio port if necessary
            if(gpio_ports[gpio_num] == NULL) {
                int result = configure_gpio_port(inst, gpio_num);
                if(result < 0) {
                    hal->exit(hal->ctx, inst->comp_id);
                    inst->rtapi->free(inst->rtapi->ctx, inst);
                    return result;
                }
            }

            bbpin->port = gpio_ports[gpio_num];

            configure_pin(bbpin, 'O');
        }
    }


    // export functions
    snprintf(fname, sizeof(fname), "bb_gpio.write");
    retval = hal->export_funct(hal->ctx, fname, write_port, port_data, 0, 0, inst->comp_id);
    if(retval < 0) {
        gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: port %d write funct export failed", n);
        hal->exit(hal->ctx, inst->comp_id);
        inst->rtapi->free(inst->rtapi->ctx, inst);
        return -1;
    }

    snprintf(fname, sizeof(fname), "bb_gpio.read");
    retval = hal->export_funct(hal->ctx, fname, read_port, port_data, 0, 0, inst->comp_id);
    if(retval < 0) {
        gomc_log_errorf(inst->log, "hal_bb_gpio", "ERROR: port %d read funct export failed", n);
        hal->exit(hal->ctx, inst->comp_id);
        inst->rtapi->free(inst->rtapi->ctx, inst);
        return -1;
    }

    gomc_log_infof(inst->log, "hal_bb_gpio", "installed driver");

    hal->ready(hal->ctx, inst->comp_id);

    inst->cmod.Destroy = hal_bb_gpio_destroy;
    inst->cmod.priv = inst;
    *out = &inst->cmod;
    return 0;
}

static void hal_bb_gpio_destroy(cmod_t *self) {
    inst_t *inst = self->priv;
    const gomc_hal_t *hal = inst->env->hal;

    hal->exit(hal->ctx, inst->comp_id);
    inst->rtapi->free(inst->rtapi->ctx, inst);
}

static void write_port(void *arg, long period) {
    (void)period;
    int i;
    port_data_t *port = (port_data_t *)arg;

    // set userled states
    for(i=0; i<4; i++) {
        if(port->led_pins[i] == NULL) continue; // short circuit if hal hasn't malloc'd a bit at this location

        bb_gpio_pin pin = user_led_gpio_pins[i];

        if(pin.claimed != 'O') continue; // if we somehow get here but the pin isn't claimed as output, short circuit

        if((*port->led_pins[i] ^ *(port->led_inv[i])) == 0)
            *(pin.port->clrdataout_reg) = (1 << pin.pin_num);
        else
            *(pin.port->setdataout_reg) = (1 << pin.pin_num);
    }

    // set output states
    for(i=1; i<=HEADERS*PINS_PER_HEADER; i++) {
        if(port->output_pins[i] == NULL) continue; // short circuit if hal hasn't malloc'd a bit at this location

        bb_gpio_pin pin;

        if(i<PINS_PER_HEADER)
            pin = p8_pins[i];
        else
            pin = p9_pins[i - PINS_PER_HEADER];

        if(pin.claimed != 'O') continue; // if we somehow get here but the pin isn't claimed as output, short circuit

        if((*port->output_pins[i] ^ *(port->output_inv[i])) == 0)
            *(pin.port->clrdataout_reg) = (1 << pin.pin_num);
        else
            *(pin.port->setdataout_reg) = (1 << pin.pin_num);
    }
}


static void read_port(void *arg, long period) {
    (void)period;
    int i;
    port_data_t *port = (port_data_t *)arg;

    // read input states
    for(i=1; i<=HEADERS*PINS_PER_HEADER; i++) {
        if(port->input_pins[i] == NULL) continue; // short circuit if hal hasn't malloc'd a bit at this location

        bb_gpio_pin pin;

        if(i<PINS_PER_HEADER) {
            pin = p8_pins[i];
        } else {
            pin = p9_pins[i - PINS_PER_HEADER];
        }


        if(!(pin.claimed == 'I' || pin.claimed == 'U' || pin.claimed == 'D')) continue; // if we get here but the pin isn't claimed as input, short circuit

        *port->input_pins[i] = ((*(pin.port->datain_reg) & (1 << pin.pin_num))  >> pin.pin_num) ^ *(port->input_inv[i]);
    }
}



off_t start_addr_for_port(int port) {
    switch(port) {
        case 0:
            return GPIO0_START_ADDR;
            break;
        case 1:
            return GPIO1_START_ADDR;
            break;
        case 2:
            return GPIO2_START_ADDR;
            break;
        case 3:
            return GPIO3_START_ADDR;
            break;
        default:
            return -1;
            break;
    }
}


void configure_pin(bb_gpio_pin *pin, char mode) {
    volatile unsigned int *control_reg = control_module + pin->control_offset;
    pin->claimed = mode;
    switch(mode) {
        case 'O':
            *(pin->port->oe_reg) &= ~(1 << pin->pin_num); // 0 in OE is output enable
            *control_reg = PIN_MODE7 | PIN_PULLUD_DISABLED | PIN_RX_DISABLED;
            break;
        case 'I':
            *(pin->port->oe_reg) |= (1 << pin->pin_num); // 1 in OE is input
            *control_reg = PIN_MODE7 | PIN_PULLUD_DISABLED | PIN_RX_ENABLED;
            break;
        case 'U':
            *(pin->port->oe_reg) |= (1 << pin->pin_num); // 1 in OE is input
            *control_reg = PIN_MODE7 | PIN_PULLUD_ENABLED | PIN_PULLUP | PIN_RX_ENABLED;
            break;
        case 'D':
            *(pin->port->oe_reg) |= (1 << pin->pin_num); // 1 in OE is input
            *control_reg = PIN_MODE7 | PIN_PULLUD_ENABLED | PIN_PULLDOWN | PIN_RX_ENABLED;
            break;
        default:
            break;
    }
}
