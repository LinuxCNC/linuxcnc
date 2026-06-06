/********************************************************************
* Description:  hal_gpio.c
*               Driver for the Raspberry Pi GPIO pins
*
* Author: Michael Haberler
* License: GPL Version 2
* Copyright (c) 2012.
*
* some code  taken from the bcm2835 library by::
*
* Author: Mike McCauley (mikem@open.com.au)
* Copyright (C) 2011 Mike McCauley
* see http://www.open.com.au/mikem/bcm2835/
* Copyright (c) 2012 Ben Croston - cpuinfo.*
*
* made work for Raspberry2 9/2015 Michael Haberler
* Last change: Modify for Pi5 10/2019 andypugh
* Last change: Modify for Pi400 3/2022 elovalvo
s********************************************************************/

#include "gomc_env.h"		/* cmod API */
#include "gomc_log.h"
#include "bcm2835.h"
#include "cpuinfo.h"

#define BCM2708_PERI_BASE   0x20000000
#define BCM2708_GPIO_BASE   (BCM2708_PERI_BASE + 0x200000)
#define BCM2709_PERI_BASE   0x3F000000
#define BCM2709_GPIO_BASE   (BCM2709_PERI_BASE + 0x200000)

#define RTAPI_BIT(nr)           (1UL << (nr))

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

// http://elinux.org/index.php?title=RPi_Low-level_peripherals&printable=yes
// Rev 1 Raspberry:
static unsigned char rev1_gpios[] = {0, 1, 4, 7,   8,  9, 10, 11, 14, 15, 17, 18, 21, 22, 23, 24, 25};
static unsigned char rev1_pins[] =  {3, 5, 7, 26, 24, 21, 19, 23,  8, 10, 11, 12, 13, 15, 16, 18, 22};

// Rev2 Raspberry:
static unsigned char rev2_gpios[] = {2, 3, 4,  7,  8,  9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27};
static unsigned char rev2_pins[] =  {3, 5, 7, 26, 24, 21, 19, 23, 8,  10, 11, 12, 15, 16, 18, 22, 13};

// Raspberry2/3:
static unsigned char rpi2_gpios[] = {2, 3, 4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 };
static unsigned char rpi2_pins[] =  {3, 5, 7, 29, 31, 26, 24, 21, 19, 23, 32, 33,  8, 10, 36, 11, 12, 35, 38, 40, 15, 16, 18, 22, 37, 13 };

typedef struct {
    cmod_t cmod;
    const cmod_env_t *env;
    const gomc_log_t *log;
    const gomc_rtapi_t *rtapi;
    int comp_id;

    int npins;
    int mem_fd;
    volatile unsigned *gpio;
    unsigned dir_map;
    unsigned exclude_map;
    unsigned char *pins;
    unsigned char *gpios;
    gomc_hal_bit_t **port_data;

    char *dir;
    char *exclude;
} inst_t;

static void write_port(void *arg, long period);
static void read_port(void *arg, long period);
static void hal_pi_gpio_destroy(cmod_t *self);

static __inline__ uint32_t bcm2835_peri_read(volatile uint32_t* paddr)
{
  // Make sure we dont return the _last_ read which might get lost
  // if subsequent code changes to a different peripheral
  volatile uint32_t ret = *paddr;
  return ret;
}

// Read input pin
static __inline__ uint8_t bcm2835_gpio_lev(inst_t *inst, uint8_t pin)
{
  volatile uint32_t* paddr = inst->gpio + BCM2835_GPLEV0/4 + pin/32;
  uint8_t shift = pin % 32;
  uint32_t value = bcm2835_peri_read(paddr);
  return (value & (1 << shift)) ? HIGH : LOW;
}

static __inline__ void bcm2835_peri_write(volatile uint32_t* paddr, uint32_t value)
{
  // Make sure we don't rely on the first write, which may get
  // lost if the previous access was to a different peripheral.
  *paddr = value;
  *paddr = value;
}

// Set output pin
static __inline__ void bcm2835_gpio_set(inst_t *inst, uint8_t pin)
{
  volatile uint32_t* paddr = inst->gpio + BCM2835_GPSET0/4 + pin/32;
  uint8_t shift = pin % 32;
  bcm2835_peri_write(paddr, 1 << shift);
}

// Clear output pin
static __inline__ void bcm2835_gpio_clr(inst_t *inst, uint8_t pin)
{
  volatile uint32_t* paddr = inst->gpio + BCM2835_GPCLR0/4 + pin/32;
  uint8_t shift = pin % 32;
  bcm2835_peri_write(paddr, 1 << shift);
}

// Set/clear only the bits in value covered by the mask
static __inline__ void bcm2835_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask)
{
  uint32_t v = bcm2835_peri_read(paddr);
  v = (v & ~mask) | (value & mask);
  bcm2835_peri_write(paddr, v);
}

// Function select
// pin is a BCM2835 GPIO pin number NOT RPi pin number
//      There are 6 control registers, each control the functions of a block
//      of 10 pins.
//      Each control register has 10 sets of 3 bits per GPIO pin:
//
//      000 = GPIO Pin X is an input
//      001 = GPIO Pin X is an output
//      100 = GPIO Pin X takes alternate function 0
//      101 = GPIO Pin X takes alternate function 1
//      110 = GPIO Pin X takes alternate function 2
//      111 = GPIO Pin X takes alternate function 3
//      011 = GPIO Pin X takes alternate function 4
//      010 = GPIO Pin X takes alternate function 5
//
// So the 3 bits for port X are:
//      X / 10 + ((X % 10) * 3)
void bcm2835_gpio_fsel(inst_t *inst, uint8_t pin, uint8_t mode)
{
  // Function selects are 10 pins per 32 bit word, 3 bits per pin
  volatile uint32_t* paddr = inst->gpio + BCM2835_GPFSEL0/4 + (pin/10);
  uint8_t   shift = (pin % 10) * 3;
  uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
  uint32_t  value = mode << shift;
  bcm2835_peri_set_bits(paddr, value, mask);
}

static int setup_gpiomem_access(inst_t *inst)
{
  if ((inst->mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC)) < 0) {
    gomc_log_errorf(inst->log, "hal_pi_gpio","HAL_PI_GPIO: can't open /dev/gpiomem:  %d - %s"
        "If the error is 'permission denied' then try adding the user who runs"
        "LinuxCNC to the gpio group: sudo gpasswd -a username gpio", errno, strerror(errno));
    return -1;
  }

  inst->gpio = mmap(NULL, BCM2835_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, inst->mem_fd, 0);

  if (inst->gpio == MAP_FAILED) {
    close(inst->mem_fd);
    inst->mem_fd = -1;
    gomc_log_errorf(inst->log, "hal_pi_gpio", "HAL_PI_GPIO: mmap failed: %d - %s", errno, strerror(errno));
    return -1;
  }

  return 0;
}

static int  setup_gpio_access(inst_t *inst, int rev, int ncores)
{
  // open /dev/mem
  if ((inst->mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      gomc_log_errorf(inst->log, "hal_pi_gpio","HAL_PI_GPIO: can't open /dev/mem:  %d - %s",
		      errno, strerror(errno));
    return -1;
  }

  if (rev <= 2  || ncores <= 2)
       inst->gpio = mmap(NULL, BCM2835_BLOCK_SIZE, PROT_READ|PROT_WRITE,
		   MAP_SHARED, inst->mem_fd, BCM2708_GPIO_BASE);
    else
       inst->gpio = mmap(NULL, BCM2835_BLOCK_SIZE, PROT_READ|PROT_WRITE,
		   MAP_SHARED, inst->mem_fd, BCM2709_GPIO_BASE);

  if (inst->gpio == MAP_FAILED) {
    gomc_log_errorf(inst->log, "hal_pi_gpio",
		    "HAL_PI_GPIO: mmap failed: %d - %s",
		    errno, strerror(errno));
    return -1;;
  }
  return 0;
}

static int number_of_cores(const gomc_log_t *log)
{
    char str[256];
    int procCount = 0;
    FILE *fp;

    if( (fp = fopen("/proc/cpuinfo", "r")) ) {
	while(fgets(str, sizeof str, fp))
	    if( !memcmp(str, "processor", 9) ) procCount++;
    }
    if ( !procCount ) {
	gomc_log_errorf(log, "hal_pi_gpio","Unable to get proc count. Defaulting to 2");
	procCount = 2;
    }
    return procCount;
}

static void parse_argv(inst_t *inst, int argc, const char **argv) {
    for (int i = 0; i < argc; i++) {
	if (strncmp(argv[i], "dir=", 4) == 0) {
	    inst->dir = (char *)argv[i] + 4;
	} else if (strncmp(argv[i], "exclude=", 8) == 0) {
	    inst->exclude = (char *)argv[i] + 8;
	}
    }
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    const gomc_hal_t *hal = env->hal;
    int n, retval = 0;
    int rev, ncores, pinno;
    char *endptr;

    inst_t *inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;
    inst->env = env;
    inst->log = env->log;
    inst->rtapi = env->rtapi;
    inst->mem_fd = -1;

    // defaults
    inst->dir = "-1";
    inst->exclude = "0";

    parse_argv(inst, argc, argv);

    if ((rev = get_rpi_revision(inst->log)) < 0) {
      gomc_log_errorf(inst->log, "hal_pi_gpio",
		      "unrecognized Raspberry revision, see /proc/cpuinfo");
      inst->rtapi->free(inst->rtapi->ctx, inst);
      return -EINVAL;
    }
    ncores = number_of_cores(inst->log);
    gomc_log_infof(inst->log, "hal_pi_gpio", "%d cores rev %d", ncores, rev);

    switch (rev) {
     case 1:
      inst->pins = rev1_pins;
      inst->gpios = rev1_gpios;
      inst->npins = sizeof(rev1_pins);
      break;
     case 2:
      inst->pins = rev2_pins;
      inst->gpios = rev2_gpios;
      inst->npins = sizeof(rev2_pins);
      break;
     default: // This will need to change if there is a V3 pinout
      inst->pins = rpi2_pins;
      inst->gpios = rpi2_gpios;
      inst->npins = sizeof(rpi2_pins);
      if (rev > 20){ // Rev 20 is Compute Module 4
	rev = get_rpi_revision(inst->log);
	gomc_log_infof(inst->log, "hal_pi_gpio", "The Pi model %i is not known to "
	      "work with this driver but will be assumed to be be using "
	      "the RPi2+ layout 40 pin connector", rev);
      }
      break;
    }

    if (inst->dir == 0) {
	gomc_log_errorf(inst->log, "hal_pi_gpio", "HAL_PI_GPIO: ERROR: no config string");
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }
    inst->dir_map = strtoul(inst->dir, &endptr,0);
    if (*endptr) {
	gomc_log_errorf(inst->log, "hal_pi_gpio",
			"HAL_PI_GPIO: dir=%s - trailing garbage: '%s'",
			inst->dir, endptr);
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }

    if (inst->exclude == 0) {
	gomc_log_errorf(inst->log, "hal_pi_gpio", "HAL_PI_GPIO: ERROR: no exclude string");
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }
    inst->exclude_map = strtoul(inst->exclude, &endptr,0);
    if (*endptr) {
	gomc_log_errorf(inst->log, "hal_pi_gpio",
			"HAL_PI_GPIO: exclude=%s - trailing garbage: '%s'",
			inst->exclude, endptr);
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }

    if (setup_gpiomem_access(inst)) {
      if (setup_gpio_access(inst, rev, ncores)) {
        inst->rtapi->free(inst->rtapi->ctx, inst);
        return -1;
      }
    }

    int r = hal->init(hal->ctx, "hal_pi_gpio", env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (r < 0) {
	gomc_log_errorf(inst->log, "hal_pi_gpio",
	    "HAL_PI_GPIO: ERROR: hal_init() failed");
	if (inst->gpio)
	    munmap((void *)inst->gpio, BCM2835_BLOCK_SIZE);
	if (inst->mem_fd > -1)
	    close(inst->mem_fd);
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }
    inst->comp_id = r;

    inst->port_data = hal->malloc(hal->ctx, inst->npins * sizeof(void *));
    if (inst->port_data == 0) {
	gomc_log_errorf(inst->log, "hal_pi_gpio",
	    "HAL_PI_GPIO: ERROR: hal->malloc(hal->ctx, ) failed");
	hal->exit(hal->ctx, inst->comp_id);
	if (inst->gpio)
	    munmap((void *)inst->gpio, BCM2835_BLOCK_SIZE);
	if (inst->mem_fd > -1)
	    close(inst->mem_fd);
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }

    for (n = 0; n < inst->npins; n++) {
      if (inst->exclude_map & RTAPI_BIT(n))
	continue;
      pinno = inst->pins[n];
      if (inst->dir_map & RTAPI_BIT(n)) {
	bcm2835_gpio_fsel(inst, inst->gpios[n], BCM2835_GPIO_FSEL_OUTP);
	if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &inst->port_data[n],
				       inst->comp_id, "hal_pi_gpio.pin-%02d-out", pinno)) < 0)
	  break;
      } else {
	bcm2835_gpio_fsel(inst, inst->gpios[n], BCM2835_GPIO_FSEL_INPT);
	if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &inst->port_data[n],
				       inst->comp_id, "hal_pi_gpio.pin-%02d-in", pinno)) < 0)
	  break;
      }
    }
    if (retval < 0) {
      gomc_log_errorf(inst->log, "hal_pi_gpio",
		      "HAL_PI_GPIO: ERROR: pin %d export failed with err=%i",
		      n,retval);
      hal->exit(hal->ctx, inst->comp_id);
      if (inst->gpio)
	  munmap((void *)inst->gpio, BCM2835_BLOCK_SIZE);
      if (inst->mem_fd > -1)
	  close(inst->mem_fd);
      inst->rtapi->free(inst->rtapi->ctx, inst);
      return -1;
    }

    retval = hal->export_funct(hal->ctx, "hal_pi_gpio.write", write_port, inst,
			      0, 0, inst->comp_id);
    if (retval < 0) {
	gomc_log_errorf(inst->log, "hal_pi_gpio",
	    "HAL_PI_GPIO: ERROR: write funct export failed");
	hal->exit(hal->ctx, inst->comp_id);
	if (inst->gpio)
	    munmap((void *)inst->gpio, BCM2835_BLOCK_SIZE);
	if (inst->mem_fd > -1)
	    close(inst->mem_fd);
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }
    retval = hal->export_funct(hal->ctx, "hal_pi_gpio.read", read_port, inst,
			      0, 0, inst->comp_id);
    if (retval < 0) {
	gomc_log_errorf(inst->log, "hal_pi_gpio",
	    "HAL_PI_GPIO: ERROR: read funct export failed");
	hal->exit(hal->ctx, inst->comp_id);
	if (inst->gpio)
	    munmap((void *)inst->gpio, BCM2835_BLOCK_SIZE);
	if (inst->mem_fd > -1)
	    close(inst->mem_fd);
	inst->rtapi->free(inst->rtapi->ctx, inst);
	return -1;
    }

    gomc_log_infof(inst->log, "hal_pi_gpio",
	"HAL_PI_GPIO: installed driver");
    hal->ready(hal->ctx, inst->comp_id);

    inst->cmod.Destroy = hal_pi_gpio_destroy;
    inst->cmod.priv = inst;
    *out = &inst->cmod;
    return 0;
}

static void hal_pi_gpio_destroy(cmod_t *self)
{
    inst_t *inst = self->priv;
    const gomc_hal_t *hal = inst->env->hal;

    if (inst->gpio)
	munmap((void *)inst->gpio, BCM2835_BLOCK_SIZE);
    if (inst->mem_fd > -1)
	close(inst->mem_fd);
    hal->exit(hal->ctx, inst->comp_id);
    inst->rtapi->free(inst->rtapi->ctx, inst);
}

static void write_port(void *arg, long period)
{
  (void)period;
  inst_t *inst = arg;
  int n;

  for (n = 0; n < inst->npins; n++) {
    if (inst->exclude_map & RTAPI_BIT(n))
      continue;
    if (inst->dir_map & RTAPI_BIT(n)) {
      if (*(inst->port_data[n])) {
	bcm2835_gpio_set(inst, inst->gpios[n]);
      } else {
	bcm2835_gpio_clr(inst, inst->gpios[n]);
      }
    }
  }
}

static void read_port(void *arg, long period)
{
  (void)period;
  inst_t *inst = arg;
  int n;

  for (n = 0; n < inst->npins; n++) {
    if ((~inst->dir_map & RTAPI_BIT(n)) && (~inst->exclude_map & RTAPI_BIT(n)))
      *inst->port_data[n] = bcm2835_gpio_lev(inst, inst->gpios[n]);
  }
}
