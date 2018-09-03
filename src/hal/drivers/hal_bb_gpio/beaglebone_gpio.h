#ifndef _BEAGLEBONE_GPIO_H_
#define _BEAGLEBONE_GPIO_H_

#define CONTROL_MODULE_START_ADDR	0x44E10000
#define CONTROL_MODULE_END_ADDR	0x44E11FFF
#define CONTROL_MODULE_SIZE	(CONTROL_MODULE_END_ADDR - CONTROL_MODULE_START_ADDR)

#define CM_PER_ADDR 0x44E00000
#define CM_PER_LEN 1024
#define CM_PER_GPIO1_CLKCTRL_OFFSET 0xAC
#define CM_PER_GPIO_CLKCTRL_MODMODE_MASK     0x00000003
#define CM_PER_GPIO_CLKCTRL_MODMODE_ENABLED  0x00000002
#define CM_PER_GPIO_CLKCTRL_MODMODE_DISABLED 0x00000000

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
#define GPIO2_START_ADDR 	0x481AC000
#define GPIO3_START_ADDR 	0x481AE000

#define GPIO_OE 				0x134
#define GPIO_SETDATAOUT 	0x194
#define GPIO_CLEARDATAOUT 	0x190
#define GPIO_DATAIN			0x138

#define HEADERS 			 2

typedef struct {
    volatile void *gpio_addr;
    volatile unsigned int *oe_reg;
    volatile unsigned int *setdataout_reg;
    volatile unsigned int *clrdataout_reg;
    volatile unsigned int *datain_reg;
} bb_gpio_port;

typedef struct {
    bb_gpio_port *port;
    char port_num;
    char pin_num;
    unsigned short control_offset;
    char claimed;
} bb_gpio_pin;

volatile void *control_module;

bb_gpio_port *gpio_ports[4];

bb_gpio_pin user_led_gpio_pins[4] = {
    { NULL, 1, 21, 0x854, 0 }, // led0, gpmc_a5
    { NULL, 1, 22, 0x858, 0 }, // led1, gpmc_a6
    { NULL, 1, 23, 0x85C, 0 }, // led2, gpmc_a7
    { NULL, 1, 24, 0x860, 0 }	// led3, gpmc_a8
};

// Max pins per header for all boards
#define MAX_PINS_PER_HEADER 46

// board ID:  Must set this in rtapi_app_main()
extern int board_id;

// Per-board info macros; depend on board_id
// - Add new boards to BOARD_MACRO as needed
# define BOARD_MACRO(name) (						\
	(board_id == BEAGLEBONE) ? BEAGLEBONE_ ## name			\
	: POCKETBEAGLE_ ## name						\
	)
#define HLO_PINS BOARD_MACRO(HLO_PINS)
#define HHI_PINS BOARD_MACRO(HHI_PINS)
#define PINS_PER_HEADER BOARD_MACRO(PINS_PER_HEADER)
#define HLO_HEADER BOARD_MACRO(HLO_HEADER)
#define HHI_HEADER BOARD_MACRO(HHI_HEADER)
#define HLO_BASEPIN (HLO_HEADER * 100)
#define HHI_BASEPIN (HHI_HEADER * 100)
#define HLO_FIRSTPIN (HLO_HEADER * 100 + 1)
#define HHI_FIRSTPIN (HHI_HEADER * 100 + 1)
#define HLO_LASTPIN (HLO_HEADER * 100 + PINS_PER_HEADER)
#define HHI_LASTPIN (HHI_HEADER * 100 + PINS_PER_HEADER)


// BeagleBone definitions
#define BEAGLEBONE 0  // board ID
#define BEAGLEBONE_PINS_PER_HEADER 46
#define BEAGLEBONE_HLO_HEADER 8
#define BEAGLEBONE_HHI_HEADER 9

bb_gpio_pin BEAGLEBONE_HLO_PINS[BEAGLEBONE_PINS_PER_HEADER+1] = {
    { NULL, -1, -1, -1, 1 }, // 0 unused
    { NULL, -1, -1, -1, 1 }, // 1 GND 
    { NULL, -1, -1, -1, 1 }, // 2 GND 
    { NULL, 1,  6, 0x818, 0 }, // pin 3, gpmc_ad6
    { NULL, 1,  7, 0x81C, 0 }, // pin 4, gpmc_ad7
    { NULL, 1,  2, 0x808, 0 }, // pin 5, gpmc_ad2
    { NULL, 1,  3, 0x80C, 0 }, // pin 6, gpmc_ad3
    { NULL, 2,  2, 0x890, 0 }, // pin 7, gpmc_advn_ale
    { NULL, 2,  3, 0x894, 0 }, // pin 8, gpmc_oen_ren
    { NULL, 2,  5, 0x89C, 0 }, // pin 9, gpmc_ben0_cle
    { NULL, 2,  4, 0x898, 0 }, // pin 10, gpmc_wen
    { NULL, 1, 13, 0x834, 0 }, // pin 11, gpmc_ad13
    { NULL, 1, 12, 0x830, 0 }, // pin 12, GPMC_AD12
    { NULL, 0, 23, 0x824, 0 }, // pin 13, gpmc_ad9
    { NULL, 0, 26, 0x828, 0 }, // pin 14, gpmc_ad10
    { NULL, 1, 15, 0x83C, 0 }, // pin 15, gpmc_ad15
    { NULL, 1, 14, 0x838, 0 }, // pin 16, gpmc_ad14
    { NULL, 0, 27, 0x82C, 0 }, // pin 17, gpmc_ad11
    { NULL, 2,  1, 0x88C, 0 }, // pin 18, gpmc_clk_mux0
    { NULL, 0, 22, 0x820, 0 }, // pin 19, gpmc_ad8
    { NULL, 1, 31, 0x884, 0 }, // pin 20, gpmc_csn2
    { NULL, 1, 30, 0x880, 0 }, // pin 21, gpmc_csn1
    { NULL, 1,  5, 0x814, 0 }, // pin 22, gpmc_ad5
    { NULL, 1,  4, 0x810, 0 }, // pin 23, gpmc_ad4
    { NULL, 1,  1, 0x804, 0 }, // pin 24, gpmc_ad1
    { NULL, 1,  0, 0x800, 0 }, // pin 25, gpmc_ad0
    { NULL, 1, 29, 0x87C, 0 }, // pin 26, gpmc_csn0
    { NULL, 2, 22, 0x8E0, 0 }, // pin 27, lcd_vsync
    { NULL, 2, 24, 0x8E8, 0 }, // pin 28, lcd_pclk
    { NULL, 2, 23, 0x8E4, 0 }, // pin 29, lcd_hsync
    { NULL, 2, 25, 0x8EC, 0 }, // pin 30, lcd_ac_bias_en
    { NULL, 0, 10, 0x8D8, 0 }, // pin 31, lcd_data14
    { NULL, 0, 11, 0x8DC, 0 }, // pin 32, lcd_data15
    { NULL, 0,  9, 0x8D4, 0 }, // pin 33, lcd_data13
    { NULL, 2, 17, 0x8CC, 0 }, // pin 34, lcd_data11
    { NULL, 0,  8, 0x8D0, 0 }, // pin 35, lcd_data12
    { NULL, 2, 16, 0x8C8, 0 }, // pin 36, lcd_data10
    { NULL, 2, 14, 0x8C0, 0 }, // pin 37, lcd_data8
    { NULL, 2, 15, 0x8C4, 0 }, // pin 38, lcd_data9
    { NULL, 2, 12, 0x8B8, 0 }, // pin 39, lcd_data6
    { NULL, 2, 13, 0x8BC, 0 }, // pin 40, lcd_data7
    { NULL, 2, 10, 0x8B0, 0 }, // pin 41, lcd_data4
    { NULL, 2, 11, 0x8B4, 0 }, // pin 42, lcd_data5
    { NULL, 2,  8, 0x8A8, 0 }, // pin 43, lcd_data2
    { NULL, 2,  9, 0x8AC, 0 }, // pin 44, lcd_data3
    { NULL, 2,  6, 0x8A0, 0 }, // pin 45, lcd_data0
    { NULL, 2,  7, 0x8A4, 0 }  // pin 46, lcd_data1
};

bb_gpio_pin BEAGLEBONE_HHI_PINS[BEAGLEBONE_PINS_PER_HEADER+1] = {
    { NULL, -1, -1, -1, 1 }, // 0 unused
    { NULL, -1, -1, -1, 1 }, // 1 GND 
    { NULL, -1, -1, -1, 1 }, // 2 GND 
    { NULL, -1, -1, -1, 1 }, // 3 3.3v
    { NULL, -1, -1, -1, 1 }, // 4 3.v
    { NULL, -1, -1, -1, 1 }, // 5 Vdd 5v
    { NULL, -1, -1, -1, 1 }, // 6 Vdd 5v
    { NULL, -1, -1, -1, 1 }, // 7 Sys 5v
    { NULL, -1, -1, -1, 1 }, // 8 Sys 5v
    { NULL, -1, -1, -1, 1 }, // 9 power button
    { NULL, -1, -1, -1, 1 }, // 10 sys_reset
    { NULL, 0, 30, 0x870, 0 }, // pin 11, gpmc_wait0
    { NULL, 1, 28, 0x878, 0 }, // pin 12, gpmc_ben1
    { NULL, 0, 31, 0x874, 0 }, // pin 13, gpmc_wpn
    { NULL, 1, 18, 0x848, 0 }, // pin 14, gpmc_a2
    { NULL, 1, 16, 0x840, 0 }, // pin 15, gpmc_a0
    { NULL, 1, 19, 0x84C, 0 }, // pin 16, gpmc_a3
    { NULL, 0,  5, 0x95C, 0 }, // pin 17, spi0_cs0
    { NULL, 0,  4, 0x958, 0 }, // pin 18, spi0_d1
    { NULL, 0, 13, 0x97C, 0 }, // pin 19, uart1_rtsn
    { NULL, 0, 12, 0x978, 0 }, // pin 20, uart1_ctsn
    { NULL, 0,  3, 0x954, 0 }, // pin 21, spi0_d0
    { NULL, 0,  2, 0x950, 0 }, // pin 22, spi0_sclk
    { NULL, 1, 17, 0x844, 0 }, // pin 23, gpmc_a1
    { NULL, 0, 15, 0x984, 0 }, // pin 24, uart1_txd
    { NULL, 3, 21, 0x9AC, 0 }, // pin 25, mcasp0_ahclkx
    { NULL, 0, 14, 0x980, 0 }, // pin 26, uart1_rxd
    { NULL, 3, 19, 0x9A4, 0 }, // pin 27, mcasp0_fsr
    { NULL, 3, 17, 0x99C, 0 }, // pin 28, mcasp0_ahclkr
    { NULL, 3, 15, 0x994, 0 }, // pin 29, mcasp0_fsx
    { NULL, 3, 16, 0x998, 0 }, // pin 30, mcasp0_axr0
    { NULL, 3, 14, 0x990, 0 }, // pin 31, mcasp0_aclkx
    { NULL, -1, -1, -1, 1 }, // 32 VADC
    { NULL, -1, -1, -1, 1 }, // 33 AIN4
    { NULL, -1, -1, -1, 1 }, // 34 AGND
    { NULL, -1, -1, -1, 1 }, // 35 AIN6
    { NULL, -1, -1, -1, 1 }, // 36 AIN5
    { NULL, -1, -1, -1, 1 }, // 37 AIN2
    { NULL, -1, -1, -1, 1 }, // 38 AIN3
    { NULL, -1, -1, -1, 1 }, // 39 AIN0
    { NULL, -1, -1, -1, 1 }, // 40 AIN1
//  NOTE 41 and 42 each have two signals connected
//    { NULL, 3, 20, 0x9A8, 0 }, // pin 41, mcasp0_axr1
    { NULL, 0, 20, 0x9B4, 0 }, // pin 41, xdma_event_intr1
    { NULL, 0,  7, 0x964, 0 }, // pin 42, ecap0_in_pwm0_out
//    { NULL, 3, 18, 0x9A0, 0 }, // pin 42, mcasp0_aclkr
    { NULL, -1, -1, -1, 1 }, // 43 GND 
    { NULL, -1, -1, -1, 1 }, // 44 GND 
    { NULL, -1, -1, -1, 1 }, // 45 GND 
    { NULL, -1, -1, -1, 1 }, // 46 GND 
};

// PocketBeagle definitions
#define POCKETBEAGLE 1  // board ID
#define POCKETBEAGLE_PINS_PER_HEADER 36
#define POCKETBEAGLE_HLO_HEADER 1
#define POCKETBEAGLE_HHI_HEADER 2

bb_gpio_pin POCKETBEAGLE_HLO_PINS[POCKETBEAGLE_PINS_PER_HEADER+1] = {
    { NULL, -1, -1,    -1, 0 }, // pin 0
    { NULL, -1, -1,    -1, 0 }, // pin 1
    { NULL,  2, 23, 0x8e4, 0 }, // pin 2
    { NULL, -1, -1,    -1, 0 }, // pin 3
    { NULL,  2, 25, 0x8ec, 0 }, // pin 4
    { NULL, -1, -1,    -1, 0 }, // pin 5
    { NULL,  0,  5, 0x95c, 0 }, // pin 6
    { NULL, -1, -1,    -1, 0 }, // pin 7
    { NULL,  0,  2, 0x950, 0 }, // pin 8
    { NULL, -1, -1,    -1, 0 }, // pin 9
    { NULL,  0,  3, 0x954, 0 }, // pin 10
    { NULL, -1, -1,    -1, 0 }, // pin 11
    { NULL,  0,  4, 0x958, 0 }, // pin 12
    { NULL, -1, -1,    -1, 0 }, // pin 13
    { NULL, -1, -1,    -1, 0 }, // pin 14
    { NULL, -1, -1,    -1, 0 }, // pin 15
    { NULL, -1, -1,    -1, 0 }, // pin 16
    { NULL, -1, -1,    -1, 0 }, // pin 17
    { NULL, -1, -1,    -1, 0 }, // pin 18
    { NULL, -1, -1,    -1, 0 }, // pin 19
    { NULL,  0, 20, 0x9b4, 0 }, // pin 20
    { NULL, -1, -1,    -1, 0 }, // pin 21
    { NULL, -1, -1,    -1, 0 }, // pin 22
    { NULL, -1, -1,    -1, 0 }, // pin 23
    { NULL, -1, -1,    -1, 0 }, // pin 24
    { NULL, -1, -1,    -1, 0 }, // pin 25
    { NULL,  0, 12, 0x978, 0 }, // pin 26
    { NULL, -1, -1,    -1, 0 }, // pin 27
    { NULL,  0, 13, 0x97c, 0 }, // pin 28
    { NULL,  3, 21, 0x9ac, 0 }, // pin 29
    { NULL,  1, 11, 0x974, 0 }, // pin 30
    { NULL,  3, 18, 0x9a0, 0 }, // pin 31
    { NULL,  1, 10, 0x970, 0 }, // pin 32
    { NULL,  3, 15, 0x994, 0 }, // pin 33
    { NULL,  0, 26, 0x828, 0 }, // pin 34
    { NULL,  2, 24, 0x8e8, 0 }, // pin 35
    { NULL,  3, 14, 0x990, 0 }, // pin 36
};

bb_gpio_pin POCKETBEAGLE_HHI_PINS[POCKETBEAGLE_PINS_PER_HEADER+1] = {
    { NULL, -1, -1,    -1, 0 }, // pin 0
    { NULL,  1, 18, 0x848, 0 }, // pin 1
    { NULL,  1, 27, 0x86c, 0 }, // pin 2
    { NULL,  0, 23, 0x824, 0 }, // pin 3
    { NULL,  1, 26, 0x868, 0 }, // pin 4
    { NULL,  0, 30, 0x870, 0 }, // pin 5
    { NULL,  1, 25, 0x864, 0 }, // pin 6
    { NULL,  0, 31, 0x874, 0 }, // pin 7
    { NULL,  1, 28, 0x878, 0 }, // pin 8
    { NULL,  0, 15, 0x984, 0 }, // pin 9
    { NULL,  1, 20, 0x850, 0 }, // pin 10
    { NULL,  0, 14, 0x980, 0 }, // pin 11
    { NULL, -1, -1,    -1, 0 }, // pin 12
    { NULL, -1, -1,    -1, 0 }, // pin 13
    { NULL, -1, -1,    -1, 0 }, // pin 14
    { NULL, -1, -1,    -1, 0 }, // pin 15
    { NULL, -1, -1,    -1, 0 }, // pin 16
    { NULL,  2,  1, 0x88c, 0 }, // pin 17
    { NULL,  1, 15, 0x83c, 0 }, // pin 18
    { NULL,  0, 27, 0x82c, 0 }, // pin 19
    { NULL,  2,  0, 0x888, 0 }, // pin 20
    { NULL, -1, -1,    -1, 0 }, // pin 21
    { NULL,  1, 14, 0x838, 0 }, // pin 22
    { NULL, -1, -1,    -1, 0 }, // pin 23
    { NULL,  1, 16, 0x840, 0 }, // pin 24
    { NULL,  1,  9, 0x96c, 0 }, // pin 25
    { NULL, -1, -1,    -1, 0 }, // pin 26
    { NULL,  1,  8, 0x968, 0 }, // pin 27
    { NULL,  3, 20, 0x9a8, 0 }, // pin 28
    { NULL,  0,  7, 0x964, 0 }, // pin 29
    { NULL,  3, 17, 0x99c, 0 }, // pin 30
    { NULL,  0, 19, 0x9b0, 0 }, // pin 31
    { NULL,  3, 16, 0x998, 0 }, // pin 32
    { NULL,  1, 13, 0x834, 0 }, // pin 33
    { NULL,  3, 19, 0x9a4, 0 }, // pin 34
    { NULL,  2, 22, 0x8e0, 0 }, // pin 35
    { NULL, -1, -1,    -1, 0 }, // pin 36
};


#endif
