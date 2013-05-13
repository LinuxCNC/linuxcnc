// methods exported by gpiomap

#ifndef GPIOMAP_H
#define GPIOMAP_H
#if defined(RTAPI)

#define GPIO_BITMAP_SIZE 1000

// for usage see comment in src/hal/drivers/hal_gpiomap.c

extern int gpio_npins(void);
extern int gpio_test_pin(int pinno, int reserve);
extern int gpio_free_pin(int pinno);
extern char *gpio_platform(void);
#endif

#endif
