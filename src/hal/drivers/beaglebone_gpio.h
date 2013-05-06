#ifndef _BEAGLEBONE_GPIO_H_
#define _BEAGLEBONE_GPIO_H_

#define CONTROL_MODULE_START_ADDR	0x44E10000
#define CONTROL_MODULE_END_ADDR	0x44E11FFF
#define CONTROL_MODULE_SIZE	(CONTROL_MODULE_END_ADDR - CONTROL_MODULE_START_ADDR)

#define PIN_RX_DISABLED 0
#define PIN_RX_ENABLED  (1<<5)

#define PIN_PULLUD_DISABLED 0
#define PIN_PULLUD_ENABLED (1<<3)

#define PIN_PULLUP (1<<4)
#define PIN_PULLDOWN 0

#define PIN_SLEW_FAST 0
#define PIN_SLEW_SLOW (1<<6)


#define PIN_MODE0	0
#define PIN_MODE1 1 
#define PIN_MODE2 2 
#define PIN_MODE3 3 
#define PIN_MODE4 4 
#define PIN_MODE5 5 
#define PIN_MODE6 6 
#define PIN_MODE7 7 


#define CONF_GPIO1_28 0x878



#define GPIO_SIZE			 	0x2000
#define GPIO0_START_ADDR 	0x44E07000
#define GPIO1_START_ADDR 	0x4804C000
#define GPIO2_START_ADDR 	0x4804C000
#define GPIO3_START_ADDR 	0x481AE000

#define GPIO_OE 				0x134
#define GPIO_SETDATAOUT 	0x194
#define GPIO_CLEARDATAOUT 	0x190
#define GPIO_DATAIN			0x138

typedef struct {
	volatile void *gpio_addr;
	volatile unsigned int *oe_reg;
	volatile unsigned int *setdataout_reg;
	volatile unsigned int *clrdataout_reg;
} bb_gpio_port;

typedef struct {
	bb_gpio_port *port;
	unsigned char port_num;
	unsigned char pin_num;
	unsigned short control_offset;
} bb_gpio_pin;

volatile void *control_module;

bb_gpio_port *gpio_ports[4];

bb_gpio_pin user_led_gpio_pins[4] = {
	{ NULL, 1, 21, 0x854 },
	{ NULL, 1, 22, 0x858 },
	{ NULL, 1, 23, 0x85C },
	{ NULL, 1, 24, 0x860 }
};

#endif
