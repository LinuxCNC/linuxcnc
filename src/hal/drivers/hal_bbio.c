#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_bitops.h"	
#include "rtapi_app.h"		/* RTAPI realtime module decls */
                                /* this also includes config.h */
#include "hal.h"		/* HAL public API decls */
#include "beaglebone_gpio.h"	

#if !defined(BUILD_SYS_USER_DSO) 
#error "This driver is for usermode threads only"
#endif

#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#define N_LEDS 4
#define N_PWM  4
#define N_ADC  6
#define N_GPIO 32

typedef struct {
  hal_bit_t	 *leds[N_LEDS];
  hal_s32_t 	 *pwms[N_PWM];
  hal_s32_t 	 *adc[N_ADC];
  hal_bit_t 	 *gpio[N_GPIO];
} hal_data, *hal_data_ptr;


static int npins;
static int  mem_fd;
// I/O access
volatile unsigned *gpio;

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Driver for Beaglebone GPIO pins");
MODULE_LICENSE("GPL");

#define MODNAME "hal_gpio"
static const char *modname = MODNAME;

// pwms - USR0 = 1,... USR3 = 8
static int leds = 15; 
RTAPI_MP_STRING(leds, "leds to export USR0=1,USR1=2,USR2=4,USR3=8");

// pmw 0-3
static int pwm = 15; 
RTAPI_MP_STRING(pwm, "mask of PMW outputs");

// ain 0-6
static int ain = 63; 
RTAPI_MP_STRING(ain, "mask of analog inputs");

// port direction bits, 1=output
static char *dir = "-1"; // all output
RTAPI_MP_STRING(dir, "port direction, 1=output");
static unsigned dir_map;

// exclude pins from usage
static char *exclude = "0"; // all used
RTAPI_MP_STRING(exclude, "exclude pins, 1=dont use");
static unsigned exclude_map;

static int comp_id;		/* component ID */
static unsigned char *pins, *gpios;
hal_data_ptr hal_data;

static void write_port(void *arg, long period);
static void read_port(void *arg, long period);
int sysfs_write(const char *value, const char *fmt, ...);

static volatile void *gpio = NULL;
static volatile unsigned int *gpio_oe_addr = NULL;
static volatile unsigned int *gpio_setdataout_addr = NULL;
static volatile unsigned int *gpio_cleardataout_addr = NULL;

static int  setup_gpio_access()
{
  if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) { 
    rtapi_print_msg(RTAPI_MSG_ERR,"%s: can't open /dev/mem\n", modname);
    return -EPERM;
  }
  gpio = mmap(NULL,  GPIO1_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, GPIO1_START_ADDR);
  if (gpio == MAP_FAILED) {
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "%s: mmap failed: %d - %s\n", modname,
		    errno, strerror(errno));
    return -ENOMEM;;
  }
  return 0;
} 

int rtapi_app_main(void)
{
    int n, retval = 0;
    int rev, pinno;
    char *endptr;

    hal_data = hal_malloc(sizeof(hal_data));
    if (hal_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "%s: ERROR: hal_malloc() failed\n", modname);
	hal_exit(comp_id);
	return -1;
    }
    if (dir == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: no config string\n", modname);
	return -1;
    }
    dir_map = strtoul(dir, &endptr,0);
    if (*endptr) {
	rtapi_print_msg(RTAPI_MSG_ERR, 
			"%s: dir=%s - trailing garbage: '%s'\n",
			modname, dir, endptr);
	return -1;
    }
    if (exclude == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: no exclude string\n", modname);
	return -1;
    }
    exclude_map = strtoul(exclude, &endptr,0);
    if (*endptr) {
	rtapi_print_msg(RTAPI_MSG_ERR, 
			"%s: exclude=%s - trailing garbage: '%s'\n", 
			modname, exclude, endptr);
	return -1;
    }
    if (setup_gpio_access())
      return -1;

    comp_id = hal_init("hal_gpio");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_init() failed\n", modname);
	return -1;
    }

    if (leds) {
      // disconnect leds from kernel activity display
      retval = sysfs_write("leds-gpio\n","/sys/bus/platform/drivers/leds-gpio/unbind");
      if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
		      "HAL_GPIO: ERROR: pin %d export failed with err=%i\n", 
			n,retval);
	hal_exit(comp_id);
	return -1;
      }
      for (n = 0; n < N_LEDS; n++) {
	if (!(leds & _BIT(n)))
	continue;
	if ((retval = hal_pin_bit_newf(HAL_IN, &hal_data->led[n],
				       comp_id, "%s.led-%02d", modname, n)) < 0) {
	  rtapi_print_msg(RTAPI_MSG_ERR,
			  "%s: ERROR: led pin export failed with err=%d\n", 
			  retval);
	  hal_exit(comp_id);
	  return -1;
	}
      }
    }
#if 0
    for (n = 0; n < npins; n++) {
      if (exclude_map & _BIT(n))
	continue;
      pinno = pins[n];
      if (dir_map & _BIT(n)) {
	bcm2835_gpio_fsel(gpios[n], BCM2835_GPIO_FSEL_OUTP);
	if ((retval = hal_pin_bit_newf(HAL_IN, &port_data[n],
				       comp_id, "hal_gpio.pin-%02d-out", pinno)) < 0)
	  break;
      } else {
	bcm2835_gpio_fsel(gpios[n], BCM2835_GPIO_FSEL_INPT);
	if ((retval = hal_pin_bit_newf(HAL_OUT, &port_data[n],
				       comp_id, "hal_gpio.pin-%02d-in", pinno)) < 0)
	  break;
      }
    }
    if (retval < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
		      "%s: ERROR: pin %d export failed with err=%i\n", 
		      n,retval);
      hal_exit(comp_id);
      return -1;
    }
#endif
    retval = hal_export_funct(MODNAME ## ".write", write_port, 0,
			      0, 0, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "%s: ERROR: write funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct(MODNAME ## ".read", read_port, 0,
			      0, 0, comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "%s: ERROR: read funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"%s: installed driver\n");
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
  // reattach to kernel activity
  if (leds)
    sysfs_write("leds-gpio\n","/sys/bus/platform/drivers/leds-gpio/bind");

  if (gpio)
    munmap((void *) gpio, BCM2835_BLOCK_SIZE);
  close(mem_fd);
  hal_exit(comp_id);
}

int sysfs_write(const char *value, const char *fmt, ...)
{
  va_list ap;
  char path[PATH_MAX];
  int fd;

  va_start(ap, fmt);
  vsnprintf(path, LINELENPATH_MAX, fmt, ap);
  va_end(ap);
  if (fd = open(path, O_RDWR) < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "%s: can't open %s: %d - %s\n", modname,
		    path, errno, strerror(errno));
    return -1;
  }
  n = write(fd, value, strlen(value));
  if (n != strlen(value)) {
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "%s: error writing '%s' to '%s': %d - %s\n", modname,
		    value, path, errno, strerror(errno));
    return -1;
  }
  close(fd);
  return 0;
}

static void write_port(void *arg, long period)
{
  int n;

  // FIXME optimize this
  for (n = 0; n < npins; n++) {
    if (exclude_map & _BIT(n)) 
      continue;
    if (dir_map & _BIT(n)) {
      if (*(port_data[n])) { 
	bcm2835_gpio_set(gpios[n]);
      } else {
	bcm2835_gpio_clr(gpios[n]);
      }
    }
  }
}

static void read_port(void *arg, long period)
{
  int n;

  // FIXME optimize this
  for (n = 0; n < npins; n++) {
    if ((~dir_map & _BIT(n)) && (~exclude_map & _BIT(n)))
      *port_data[n] = bcm2835_gpio_lev(gpios[n]);
  }
}

#if 0
static __inline__ uint32_t bcm2835_peri_read(volatile uint32_t* paddr)
{
  // Make sure we dont return the _last_ read which might get lost
  // if subsequent code changes to a different peripheral
  uint32_t ret = *paddr;
  uint32_t dummy = *paddr;
  return ret;
}

// Read input pin
static __inline__ uint8_t bcm2835_gpio_lev(uint8_t pin)
{
  volatile uint32_t* paddr = gpio + BCM2835_GPLEV0/4 + pin/32;
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
static __inline__ void bcm2835_gpio_set(uint8_t pin)
{
  volatile uint32_t* paddr = gpio + BCM2835_GPSET0/4 + pin/32;
  uint8_t shift = pin % 32;
  bcm2835_peri_write(paddr, 1 << shift);
}

// Clear output pin
static __inline__ void bcm2835_gpio_clr(uint8_t pin)
{
  volatile uint32_t* paddr = gpio + BCM2835_GPCLR0/4 + pin/32;
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

/
void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode)
{
  // Function selects are 10 pins per 32 bit word, 3 bits per pin
  volatile uint32_t* paddr = gpio + BCM2835_GPFSEL0/4 + (pin/10);
  uint8_t   shift = (pin % 10) * 3;
  uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
  uint32_t  value = mode << shift;
  bcm2835_peri_set_bits(paddr, value, mask);
}

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
		    "%s: can't open %s: %d - %s\n",
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
#endif
