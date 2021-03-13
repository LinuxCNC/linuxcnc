// bcm2835.h
//
// C and C++ support for Broadcom BCM 2835 as used in Raspberry Pi
// http://elinux.org/RPi_Low-level_peripherals
// http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
//
// Author: Mike McCauley (mikem@open.com.au)
// Copyright (C) 2011 Mike McCauley
// $Id: bcm2835.h,v 1.4 2012/07/16 23:57:59 mikem Exp mikem $
//
/// \mainpage C library for Broadcom BCM 2835 as used in Raspberry Pi
///
/// This is a C library for Raspberry Pi (RPi). It provides access to
/// GPIO and other IO functions on the Broadcom BCM 2835 chip,
/// allowing access to the GPIO pins on the
/// 26 pin IDE plug on the RPi board so you can control and interface with various external devices.
///
/// It provides functions for reading digital inputs and setting digital outputs.
/// Pin event detection is supported by polling (interrupts not supported).
///
/// It is C++ compatible, and installs as a header file and non-shared library on
/// any Linux-based distro (but clearly is no use except on Raspberry Pi or another board with
/// BCM 2835).
///
/// The latest version of this documentation can be downloaded from
/// http://www.open.com.au/mikem/bcm2835
///
/// The version of the package that this documentation refers to can be downloaded
/// from http://www.open.com.au/mikem/bcm2835/bcm2835-1.12.tar.gz
/// You can find the latest version at http://www.open.com.au/mikem/bcm2835
///
/// Several example programs are provided.
///
/// Based on data in http://elinux.org/RPi_Low-level_peripherals and
/// http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
/// and http://www.scribd.com/doc/101830961/GPIO-Pads-Control2
///
/// You can also find online help and discussion at http://groups.google.com/group/bcm2835
/// Please use that group for all questions and discussions on this topic.
/// Do not contact the author directly, unless it is to discuss commercial licensing.
///
/// Tested on debian6-19-04-2012, 2012-07-15-wheezy-raspbian and Occidentalisv01
/// CAUTION: it has been observed that when detect enables such as bcm2835_gpio_len()
/// are used and the pin is pulled LOW
/// it can cause temporary hangs on 2012-07-15-wheezy-raspbian and Occidentalisv01.
/// Reason for this is not yet determined, but suspect that an interrupt handler is
/// hitting a hard loop on those OSs.
/// If you must use bcm2835_gpio_len() and friends, make sure you disable the pins with
/// bcm2835_gpio_cler_len() and friends after use.
///
/// \par Installation
///
/// This library consists of a single non-shared library and header file, which will be
/// installed in the usual places by make install
///
/// tar zxvf bcm2835-1.0.tar.gz
/// cd bcm2835-1.0
/// ./configure
/// make
/// # as root:
/// make check
/// make install
///
/// \par Physical Addresses
///
/// The functions bcm2845_peri_read(), bcm2845_peri_write() and bcm2845_peri_set_bits()
/// are low level peripheral register access functions. They are designed to use
/// physical addresses as described in section 1.2.3 ARM physical addresses
/// of the BCM2835 ARM Peripherals manual.
/// Physical addresses range from 0x20000000 to 0x20FFFFFF for peripherals. The bus
/// addresses for peripherals are set up to map onto the peripheral bus address range starting at
/// 0x7E000000. Thus a peripheral advertised in the manual at bus address 0x7Ennnnnn is available at
/// physical address 0x20nnnnnn.
///
/// \par Pin Numbering
///
/// The GPIO pin numbering as used by RPi is different to and inconsistent with the underlying
/// BCM 2835 chip pin numbering. http://elinux.org/RPi_BCM2835_GPIOs
///
/// RPi has a 26 pin IDE header that provides access to some of the GPIO pins on the BCM 2835,
/// as well as power and ground pins. Not all GPIO pins on the BCM 2835 are available on the
/// IDE header.
///
/// The functions in this librray are disgned to be passed the BCM 2835 GPIO pin number and _not_
/// the RPi pin number. There are symbolic definitions for each of the available pins
/// that you should use for convenience. See \ref RPiGPIOPin.
///
/// \par SPI Pins
///
/// The bcm2835_spi_* functions allow you to control the BCM 2835 SPI0 interface,
/// allowing you to send and received data by SPI (Serial Peripheral Interface).
/// For more information about SPI, see http://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus
///
/// When bcm2835_spi_begin() is called it changes the bahaviour of the SPI interface pins from their
/// default GPIO behaviour in order to support SPI. While SPI is in use, you will not be able
/// to control the state of the SPI pins through the usual bcm2835_spi_gpio_write().
/// When bcm2835_spi_end() is called, the SPI pins will all revert to inputs, and can then be
/// configured and controled with the usual bcm2835_gpio_* calls.
///
/// The Raspberry Pi GPIO pins used for SPI are:
///
/// - P1-19 (MOSI)
/// - P1-21 (MISO)
/// - P1-23 (CLK)
/// - P1-24 (CE0)
/// - P1-26 (CE1)
///
/// \par Open Source Licensing GPL V2
///
/// This is the appropriate option if you want to share the source code of your
/// application with everyone you distribute it to, and you also want to give them
/// the right to share who uses it. If you wish to use this software under Open
/// Source Licensing, you must contribute all your source code to the open source
/// community in accordance with the GPL Version 2 when your application is
/// distributed. See http://www.gnu.org/copyleft/gpl.html and COPYING
///
/// \par Acknowledgements
///
/// Some of this code has been inspired by Dom and Gert.
///
/// \par Revision History
///
/// \version 1.0 Initial release
/// \version 1.1 Minor bug fixes
/// \version 1.2 Added support for SPI
/// \version 1.3 Added bcm2835_spi_transfern()
/// \version 1.4 Fixed a problem that prevented SPI CE1 being used. Reported by David Robinson.
/// \version 1.5 Added bcm2835_close() to deinit the library. Suggested by C?sar Ortiz
/// \version 1.6 Document testing on 2012-07-15-wheezy-raspbian and Occidentalisv01
///              Functions bcm2835_gpio_ren(), bcm2835_gpio_fen(), bcm2835_gpio_hen()
///               bcm2835_gpio_len(), bcm2835_gpio_aren() and bcm2835_gpio_afen() now
///               changes only the pin specified. Other pins that were already previoulsy
///               enabled stay enabled.
///              Added  bcm2835_gpio_clr_ren(), bcm2835_gpio_clr_fen(), bcm2835_gpio_clr_hen()
///                bcm2835_gpio_clr_len(), bcm2835_gpio_clr_aren(), bcm2835_gpio_clr_afen()
///                to clear the enable for individual pins, suggested by Andreas Sundstrom.
/// \version 1.7 Added bcm2835_spi_transfernb to support different buffers for read and write.
/// \version 1.8 Improvements to read barrier, as suggested by maddin.
/// \version 1.9 Improvements contributed by mikew:
///              I noticed that it was mallocing memory for the mmaps on /dev/mem.
///              It's not necessary to do that, you can just mmap the file directly,
///              so I've removed the mallocs (and frees).
///              I've also modified delayMicroseconds() to use nanosleep() for long waits,
///              and a busy wait on a high resolution timer for the rest. This is because
///              I've found that calling nanosleep() takes at least 100-200 us.
///              You need to link using '-lrt' using this version.
///              I've added some unsigned casts to the debug prints to silence compiler
///              warnings I was getting, fixed some typos, and changed the value of
///              BCM2835_PAD_HYSTERESIS_ENABLED to 0x08 as per Gert van Loo's doc at
///              http://www.scribd.com/doc/101830961/GPIO-Pads-Control2
///              Also added a define for the passwrd value that Gert says is needed to
///              change pad control settings.
/// \version 1.10 Changed the names of the delay functions to bcm2835_delay()
///              and bcm2835_delayMicroseconds() to prevent collisions with wiringPi.
///              Macros to map delay()-> bcm2835_delay() and
///              Macros to map delayMicroseconds()-> bcm2835_delayMicroseconds(), which
///              can be disabled by defining BCM2835_NO_DELAY_COMPATIBILITY
/// \version 1.11 Fixed incorrect link to download file
/// \version 1.12 New GPIO pin definitions for RPi version 2 (which has a diffrent GPIO mapping)
///
/// \author  Mike McCauley (mikem@open.com.au)



// Defines for BCM2835
#ifndef BCM2835_H
#define BCM2835_H

#include <stdint.h>

/// \defgroup constants Constants for passing to and from library functions
/// The values here are designed to be passed to various functions in the bcm2835 library.
/// @{


/// This means pin HIGH, true, 3.3volts on a pin.
#define HIGH 0x1
/// This means pin LOW, false, 0volts on a pin.
#define LOW  0x0

// Physical addresses for various peripheral regiser sets
/// Base Physical Address of the BCM 2835 peripheral registers
#define BCM2835_PERI_BASE               0x20000000
/// Base Physical Address of the Pads registers
#define BCM2835_GPIO_PADS               (BCM2835_PERI_BASE + 0x100000)
/// Base Physical Address of the Clock/timer registers
#define BCM2835_CLOCK_BASE              (BCM2835_PERI_BASE + 0x101000)
/// Base Physical Address of the GPIO registers
#define BCM2835_GPIO_BASE               (BCM2835_PERI_BASE + 0x200000)
/// Base Physical Address of the SPI0 registers
#define BCM2835_SPI0_BASE                (BCM2835_PERI_BASE + 0x204000)
/// Base Physical Address of the PWM registers
#define BCM2835_GPIO_PWM                (BCM2835_PERI_BASE + 0x20C000)

/// Size of memory page on RPi
#define BCM2835_PAGE_SIZE               (4*1024)
/// Size of memory block on RPi
#define BCM2835_BLOCK_SIZE              (4*1024)


// Defines for GPIO
// The BCM2835 has 54 GPIO pins.
//      BCM2835 data sheet, Page 90 onwards.
/// GPIO register offsets from BCM2835_GPIO_BASE. Offsets into the GPIO Peripheral block in bytes per 6.1 Register View
#define BCM2835_GPFSEL0                      0x0000 ///< GPIO Function Select 0
#define BCM2835_GPFSEL1                      0x0004 ///< GPIO Function Select 1
#define BCM2835_GPFSEL2                      0x0008 ///< GPIO Function Select 2
#define BCM2835_GPFSEL3                      0x000c ///< GPIO Function Select 3
#define BCM2835_GPFSEL4                      0x0010 ///< GPIO Function Select 4
#define BCM2835_GPFSEL5                      0x0014 ///< GPIO Function Select 5
#define BCM2835_GPSET0                       0x001c ///< GPIO Pin Output Set 0
#define BCM2835_GPSET1                       0x0020 ///< GPIO Pin Output Set 1
#define BCM2835_GPCLR0                       0x0028 ///< GPIO Pin Output Clear 0
#define BCM2835_GPCLR1                       0x002c ///< GPIO Pin Output Clear 1
#define BCM2835_GPLEV0                       0x0034 ///< GPIO Pin Level 0
#define BCM2835_GPLEV1                       0x0038 ///< GPIO Pin Level 1
#define BCM2835_GPEDS0                       0x0040 ///< GPIO Pin Event Detect Status 0
#define BCM2835_GPEDS1                       0x0044 ///< GPIO Pin Event Detect Status 1
#define BCM2835_GPREN0                       0x004c ///< GPIO Pin Rising Edge Detect Enable 0
#define BCM2835_GPREN1                       0x0050 ///< GPIO Pin Rising Edge Detect Enable 1
#define BCM2835_GPFEN0                       0x0048 ///< GPIO Pin Falling Edge Detect Enable 0
#define BCM2835_GPFEN1                       0x005c ///< GPIO Pin Falling Edge Detect Enable 1
#define BCM2835_GPHEN0                       0x0064 ///< GPIO Pin High Detect Enable 0
#define BCM2835_GPHEN1                       0x0068 ///< GPIO Pin High Detect Enable 1
#define BCM2835_GPLEN0                       0x0070 ///< GPIO Pin Low Detect Enable 0
#define BCM2835_GPLEN1                       0x0074 ///< GPIO Pin Low Detect Enable 1
#define BCM2835_GPAREN0                      0x007c ///< GPIO Pin Async. Rising Edge Detect 0
#define BCM2835_GPAREN1                      0x0080 ///< GPIO Pin Async. Rising Edge Detect 1
#define BCM2835_GPAFEN0                      0x0088 ///< GPIO Pin Async. Falling Edge Detect 0
#define BCM2835_GPAFEN1                      0x008c ///< GPIO Pin Async. Falling Edge Detect 1
#define BCM2835_GPPUD                        0x0094 ///< GPIO Pin Pull-up/down Enable
#define BCM2835_GPPUDCLK0                    0x0098 ///< GPIO Pin Pull-up/down Enable Clock 0
#define BCM2835_GPPUDCLK1                    0x009c ///< GPIO Pin Pull-up/down Enable Clock 1

/// \brief bcm2835PortFunction
/// Port function select modes for bcm2845_gpio_fsel()
typedef enum
{
    BCM2835_GPIO_FSEL_INPT  = 0b000,   ///< Input
    BCM2835_GPIO_FSEL_OUTP  = 0b001,   ///< Output
    BCM2835_GPIO_FSEL_ALT0  = 0b100,   ///< Alternate function 0
    BCM2835_GPIO_FSEL_ALT1  = 0b101,   ///< Alternate function 1
    BCM2835_GPIO_FSEL_ALT2  = 0b110,   ///< Alternate function 2
    BCM2835_GPIO_FSEL_ALT3  = 0b111,   ///< Alternate function 3
    BCM2835_GPIO_FSEL_ALT4  = 0b011,   ///< Alternate function 4
    BCM2835_GPIO_FSEL_ALT5  = 0b010,   ///< Alternate function 5
    BCM2835_GPIO_FSEL_MASK  = 0b111    ///< Function select bits mask
} bcm2835FunctionSelect;

/// \brief bcm2835PUDControl
/// Pullup/Pulldown defines for bcm2845_gpio_pud()
typedef enum
{
    BCM2835_GPIO_PUD_OFF     = 0b00,   ///< Off ? disable pull-up/down
    BCM2835_GPIO_PUD_DOWN    = 0b01,   ///< Enable Pull Down control
    BCM2835_GPIO_PUD_UP      = 0b10    ///< Enable Pull Up control
} bcm2835PUDControl;

/// Pad control register offsets from BCM2835_GPIO_PADS
#define BCM2835_PADS_GPIO_0_27               0x002c ///< Pad control register for pads 0 to 27
#define BCM2835_PADS_GPIO_28_45              0x0030 ///< Pad control register for pads 28 to 45
#define BCM2835_PADS_GPIO_46_53              0x0034 ///< Pad control register for pads 46 to 53

/// Pad Control masks
#define BCM2835_PAD_PASSWRD                  (0x5A << 24)  ///< Password to enable setting pad mask
#define BCM2835_PAD_SLEW_RATE_UNLIMITED      0x10 ///< Slew rate unlimited
#define BCM2835_PAD_HYSTERESIS_ENABLED       0x08 ///< Hysteresis enabled
#define BCM2835_PAD_DRIVE_2mA                0x00 ///< 2mA drive current
#define BCM2835_PAD_DRIVE_4mA                0x01 ///< 4mA drive current
#define BCM2835_PAD_DRIVE_6mA                0x02 ///< 6mA drive current
#define BCM2835_PAD_DRIVE_8mA                0x03 ///< 8mA drive current
#define BCM2835_PAD_DRIVE_10mA               0x04 ///< 10mA drive current
#define BCM2835_PAD_DRIVE_12mA               0x05 ///< 12mA drive current
#define BCM2835_PAD_DRIVE_14mA               0x06 ///< 14mA drive current
#define BCM2835_PAD_DRIVE_16mA               0x07 ///< 16mA drive current

/// \brief bcm2835PadGroup
/// Pad group specification for bcm2845_gpio_pad()
typedef enum
{
    BCM2835_PAD_GROUP_GPIO_0_27         = 0, ///< Pad group for GPIO pads 0 to 27
    BCM2835_PAD_GROUP_GPIO_28_45        = 1, ///< Pad group for GPIO pads 28 to 45
    BCM2835_PAD_GROUP_GPIO_46_53        = 2  ///< Pad group for GPIO pads 46 to 53
} bcm2835PadGroup;

/// \brief RPiGPIOPin
/// Here we define Raspberry Pin GPIO pins on P1 in terms of the underlying BCM GPIO pin numbers.
/// These can be passed as a pin number to any function requiring a pin.
/// Not all pins on the RPi 26 bin IDE plug are connected to GPIO pins
/// and some can adopt an alternate function.
/// RPi version 2 has some slightly different pinouts, and these are values RPI_V2_*.
/// At bootup, pins 8 and 10 are set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
/// When SPI0 is in use (ie after bcm2835_spi_begin()), pins 19, 21, 23, 24, 26 are dedicated to SPI
/// and cant be controlled independently
typedef enum
{
    RPI_GPIO_P1_03        =  0,  ///< Version 1, Pin P1-03
    RPI_GPIO_P1_05        =  1,  ///< Version 1, Pin P1-05
    RPI_GPIO_P1_07        =  4,  ///< Version 1, Pin P1-07
    RPI_GPIO_P1_08        = 14,  ///< Version 1, Pin P1-08, defaults to alt function 0 UART0_TXD
    RPI_GPIO_P1_10        = 15,  ///< Version 1, Pin P1-10, defaults to alt function 0 UART0_RXD
    RPI_GPIO_P1_11        = 17,  ///< Version 1, Pin P1-11
    RPI_GPIO_P1_12        = 18,  ///< Version 1, Pin P1-12
    RPI_GPIO_P1_13        = 21,  ///< Version 1, Pin P1-13
    RPI_GPIO_P1_15        = 22,  ///< Version 1, Pin P1-15
    RPI_GPIO_P1_16        = 23,  ///< Version 1, Pin P1-16
    RPI_GPIO_P1_18        = 24,  ///< Version 1, Pin P1-18
    RPI_GPIO_P1_19        = 10,  ///< Version 1, Pin P1-19, MOSI when SPI0 in use
    RPI_GPIO_P1_21        =  9,  ///< Version 1, Pin P1-21, MISO when SPI0 in use
    RPI_GPIO_P1_22        = 25,  ///< Version 1, Pin P1-22
    RPI_GPIO_P1_23        = 11,  ///< Version 1, Pin P1-23, CLK when SPI0 in use
    RPI_GPIO_P1_24        =  8,  ///< Version 1, Pin P1-24, CE0 when SPI0 in use
    RPI_GPIO_P1_26        =  7,  ///< Version 1, Pin P1-26, CE1 when SPI0 in use

    // RPi Version 2
    RPI_V2_GPIO_P1_03     =  2,  ///< Version 2, Pin P1-03
    RPI_V2_GPIO_P1_05     =  3,  ///< Version 2, Pin P1-05
    RPI_V2_GPIO_P1_07     =  4,  ///< Version 2, Pin P1-07
    RPI_V2_GPIO_P1_08     = 14,  ///< Version 2, Pin P1-08, defaults to alt function 0 UART0_TXD
    RPI_V2_GPIO_P1_10     = 15,  ///< Version 2, Pin P1-10, defaults to alt function 0 UART0_RXD
    RPI_V2_GPIO_P1_11     = 17,  ///< Version 2, Pin P1-11
    RPI_V2_GPIO_P1_12     = 18,  ///< Version 2, Pin P1-12
    RPI_V2_GPIO_P1_13     = 27,  ///< Version 2, Pin P1-13
    RPI_V2_GPIO_P1_15     = 22,  ///< Version 2, Pin P1-15
    RPI_V2_GPIO_P1_16     = 23,  ///< Version 2, Pin P1-16
    RPI_V2_GPIO_P1_18     = 24,  ///< Version 2, Pin P1-18
    RPI_V2_GPIO_P1_19     = 10,  ///< Version 2, Pin P1-19, MOSI when SPI0 in use
    RPI_V2_GPIO_P1_21     =  9,  ///< Version 2, Pin P1-21, MISO when SPI0 in use
    RPI_V2_GPIO_P1_22     = 25,  ///< Version 2, Pin P1-22
    RPI_V2_GPIO_P1_23     = 11,  ///< Version 2, Pin P1-23, CLK when SPI0 in use
    RPI_V2_GPIO_P1_24     =  8,  ///< Version 2, Pin P1-24, CE0 when SPI0 in use
    RPI_V2_GPIO_P1_26     =  7   ///< Version 2, Pin P1-26, CE1 when SPI0 in use
} RPiGPIOPin;

/// Defines for SPI
/// GPIO register offsets from BCM2835_SPI0_BASE.
/// Offsets into the SPI Peripheral block in bytes per 10.5 SPI Register Map
#define BCM2835_SPI0_CS                      0x0000 ///< SPI Master Control and Status
#define BCM2835_SPI0_FIFO                    0x0004 ///< SPI Master TX and RX FIFOs
#define BCM2835_SPI0_CLK                     0x0008 ///< SPI Master Clock Divider
#define BCM2835_SPI0_DLEN                    0x000c ///< SPI Master Data Length
#define BCM2835_SPI0_LTOH                    0x0010 ///< SPI LOSSI mode TOH
#define BCM2835_SPI0_DC                      0x0014 ///< SPI DMA DREQ Controls

// Register masks for SPI0_CS
#define BCM2835_SPI0_CS_LEN_LONG             0x02000000 ///< Enable Long data word in Lossi mode if DMA_LEN is set
#define BCM2835_SPI0_CS_DMA_LEN              0x01000000 ///< Enable DMA mode in Lossi mode
#define BCM2835_SPI0_CS_CSPOL2               0x00800000 ///< Chip Select 2 Polarity
#define BCM2835_SPI0_CS_CSPOL1               0x00400000 ///< Chip Select 1 Polarity
#define BCM2835_SPI0_CS_CSPOL0               0x00200000 ///< Chip Select 0 Polarity
#define BCM2835_SPI0_CS_RXF                  0x00100000 ///< RXF - RX FIFO Full
#define BCM2835_SPI0_CS_RXR                  0x00080000 ///< RXR RX FIFO needs Reading ( full)
#define BCM2835_SPI0_CS_TXD                  0x00040000 ///< TXD TX FIFO can accept Data
#define BCM2835_SPI0_CS_RXD                  0x00020000 ///< RXD RX FIFO contains Data
#define BCM2835_SPI0_CS_DONE                 0x00010000 ///< Done transfer Done
#define BCM2835_SPI0_CS_TE_EN                0x00008000 ///< Unused
#define BCM2835_SPI0_CS_LMONO                0x00004000 ///< Unused
#define BCM2835_SPI0_CS_LEN                  0x00002000 ///< LEN LoSSI enable
#define BCM2835_SPI0_CS_REN                  0x00001000 ///< REN Read Enable
#define BCM2835_SPI0_CS_ADCS                 0x00000800 ///< ADCS Automatically Deassert Chip Select
#define BCM2835_SPI0_CS_INTR                 0x00000400 ///< INTR Interrupt on RXR
#define BCM2835_SPI0_CS_INTD                 0x00000200 ///< INTD Interrupt on Done
#define BCM2835_SPI0_CS_DMAEN                0x00000100 ///< DMAEN DMA Enable
#define BCM2835_SPI0_CS_TA                   0x00000080 ///< Transfer Active
#define BCM2835_SPI0_CS_CSPOL                0x00000040 ///< Chip Select Polarity
#define BCM2835_SPI0_CS_CLEAR                0x00000030 ///< Clear FIFO Clear RX and TX
#define BCM2835_SPI0_CS_CLEAR_RX             0x00000020 ///< Clear FIFO Clear RX
#define BCM2835_SPI0_CS_CLEAR_TX             0x00000010 ///< Clear FIFO Clear TX
#define BCM2835_SPI0_CS_CPOL                 0x00000008 ///< Clock Polarity
#define BCM2835_SPI0_CS_CPHA                 0x00000004 ///< Clock Phase
#define BCM2835_SPI0_CS_CS                   0x00000003 ///< Chip Select

/// \brief bcm2835SPIBitOrder
/// Specifies the SPI data bit ordering
typedef enum
{
    BCM2835_SPI_BIT_ORDER_LSBFIRST = 0,  ///< LSB First
    BCM2835_SPI_BIT_ORDER_MSBFIRST = 1   ///< MSB First
}bcm2835SPIBitOrder;

/// \brief bcm2835SPIMode
/// Specify the SPI data mode
typedef enum
{
    BCM2835_SPI_MODE0 = 0,  ///< CPOL = 0, CPHA = 0
    BCM2835_SPI_MODE1 = 1,  ///< CPOL = 0, CPHA = 1
    BCM2835_SPI_MODE2 = 2,  ///< CPOL = 1, CPHA = 0
    BCM2835_SPI_MODE3 = 3,  ///< CPOL = 1, CPHA = 1
}bcm2835SPIMode;

/// \brief bcm2835SPIChipSelect
/// Specify the SPI chip select pin(s)
typedef enum
{
    BCM2835_SPI_CS0 = 0,     ///< Chip Select 0
    BCM2835_SPI_CS1 = 1,     ///< Chip Select 1
    BCM2835_SPI_CS2 = 2,     ///< Chip Select 2 (ie pins CS1 and CS2 are asserted)
    BCM2835_SPI_CS_NONE = 3, ///< No CS, control it yourself
} bcm2835SPIChipSelect;

/// \brief bcm2835SPIClockDivider
/// Specifies the divider used to generate the SPI clock from the system clock.
/// Figures below give the divider, clock period and clock frequency.
typedef enum
{
    BCM2835_SPI_CLOCK_DIVIDER_65536 = 0,       ///< 65536 = 256us = 4kHz
    BCM2835_SPI_CLOCK_DIVIDER_32768 = 32768,   ///< 32768 = 126us = 8kHz
    BCM2835_SPI_CLOCK_DIVIDER_16384 = 16384,   ///< 16384 = 64us = 15.625kHz
    BCM2835_SPI_CLOCK_DIVIDER_8192  = 8192,    ///< 8192 = 32us = 31.25kHz
    BCM2835_SPI_CLOCK_DIVIDER_4096  = 4096,    ///< 4096 = 16us = 62.5kHz
    BCM2835_SPI_CLOCK_DIVIDER_2048  = 2048,    ///< 2048 = 8us = 125kHz
    BCM2835_SPI_CLOCK_DIVIDER_1024  = 1024,    ///< 1024 = 4us = 250kHz
    BCM2835_SPI_CLOCK_DIVIDER_512   = 512,     ///< 512 = 2us = 500kHz
    BCM2835_SPI_CLOCK_DIVIDER_256   = 256,     ///< 256 = 1us = 1MHz
    BCM2835_SPI_CLOCK_DIVIDER_128   = 128,     ///< 128 = 500ns = = 2MHz
    BCM2835_SPI_CLOCK_DIVIDER_64    = 64,      ///< 64 = 250ns = 4MHz
    BCM2835_SPI_CLOCK_DIVIDER_32    = 32,      ///< 32 = 125ns = 8MHz
    BCM2835_SPI_CLOCK_DIVIDER_16    = 16,      ///< 16 = 50ns = 20MHz
    BCM2835_SPI_CLOCK_DIVIDER_8     = 8,       ///< 8 = 25ns = 40MHz
    BCM2835_SPI_CLOCK_DIVIDER_4     = 4,       ///< 4 = 12.5ns 80MHz
    BCM2835_SPI_CLOCK_DIVIDER_2     = 2,       ///< 2 = 6.25ns = 160MHz
    BCM2835_SPI_CLOCK_DIVIDER_1     = 1,       ///< 0 = 256us = 4kHz
} bcm2835SPIClockDivider;


/// @}


// Defines for PWM
#define BCM2835_PWM_CONTROL 0
#define BCM2835_PWM_STATUS  1
#define BCM2835_PWM0_RANGE  4
#define BCM2835_PWM0_DATA   5
#define BCM2835_PWM1_RANGE  8
#define BCM2835_PWM1_DATA   9

#define BCM2835_PWMCLK_CNTL     40
#define BCM2835_PWMCLK_DIV      41

#define BCM2835_PWM1_MS_MODE    0x8000  /// Run in MS mode
#define BCM2835_PWM1_USEFIFO    0x2000  /// Data from FIFO
#define BCM2835_PWM1_REVPOLAR   0x1000  /// Reverse polarity
#define BCM2835_PWM1_OFFSTATE   0x0800  /// Ouput Off state
#define BCM2835_PWM1_REPEATFF   0x0400  /// Repeat last value if FIFO empty
#define BCM2835_PWM1_SERIAL     0x0200  /// Run in serial mode
#define BCM2835_PWM1_ENABLE     0x0100  /// Channel Enable

#define BCM2835_PWM0_MS_MODE    0x0080  /// Run in MS mode
#define BCM2835_PWM0_USEFIFO    0x0020  /// Data from FIFO
#define BCM2835_PWM0_REVPOLAR   0x0010  /// Reverse polarity
#define BCM2835_PWM0_OFFSTATE   0x0008  /// Ouput Off state
#define BCM2835_PWM0_REPEATFF   0x0004  /// Repeat last value if FIFO empty
#define BCM2835_PWM0_SERIAL     0x0002  /// Run in serial mode
#define BCM2835_PWM0_ENABLE     0x0001  /// Channel Enable

// Historical name compatibility
#ifndef BCM2835_NO_DELAY_COMPATIBILITY
#define delay(x) bcm2835_delay(x)
#define delayMicroseconds(x) bcm2835_delayMicroseconds(x)
#endif

#if defined(BCM2835_FUNCS)
#ifdef __cplusplus
extern "C" {
#endif

    /// \defgroup init Library initialisation and management
    /// These functions allow you to intialise and control the bcm2835 library
    /// @{

    /// Initialise the library by opening /dev/mem and getting pointers to the
    /// internal memory for BCM 2835 device registers. You must call this (successfully)
    /// before calling any other
    /// functions in this library (except bcm2835_set_debug).
    /// If bcm2835_init() fails by returning 0,
    /// calling any other function may result in crashes or other failures.
    /// Prints messages to stderr in case of errors.
    /// \return 1 if successful else 0
    extern int bcm2835_init(void);

    /// Close the library, deallocating any allocated memory and closing /dev/mem
    /// \return 1 if successful else 0
    extern int bcm2835_close(void);

    /// Sets the debug level of the library.
    /// A value of 1 prevents mapping to /dev/mem, and makes the library print out
    /// what it would do, rather than accessing the GPIO registers.
    /// A value of 0, the default, causes normal operation.
    /// Call this before calling bcm2835_init();
    /// \param[in] debug The new debug level. 1 means debug
    extern void  bcm2835_set_debug(uint8_t debug);

    /// @} // end of init

    /// \defgroup lowlevel Low level register access
    /// These functions provide low level register access, and should not generally
    /// need to be used
    ///
    /// @{

    /// Reads 32 bit value from a peripheral address
    /// The read is done twice, and is therefore always safe in terms of
    /// manual section 1.3 Peripheral access precautions for correct memory ordering
    /// \param[in] paddr Physical address to read from. See BCM2835_GPIO_BASE etc.
    /// \return the value read from the 32 bit register
    /// \sa Physical Addresses
    extern uint32_t bcm2835_peri_read(volatile uint32_t* paddr);


    /// Reads 32 bit value from a peripheral address without the read barrier
    /// You should only use this when your code has previously called bcm2835_peri_read()
    /// within the same peripheral, and no other peripheral access has occurred since.
    /// \param[in] paddr Physical address to read from. See BCM2835_GPIO_BASE etc.
    /// \return the value read from the 32 bit register
    /// \sa Physical Addresses
    extern uint32_t bcm2835_peri_read_nb(volatile uint32_t* paddr);


    /// Writes 32 bit value from a peripheral address
    /// The write is done twice, and is therefore always safe in terms of
    /// manual section 1.3 Peripheral access precautions for correct memory ordering
    /// \param[in] paddr Physical address to read from. See BCM2835_GPIO_BASE etc.
    /// \param[in] value The 32 bit value to write
    /// \sa Physical Addresses
    extern void bcm2835_peri_write(volatile uint32_t* paddr, uint32_t value);

    /// Writes 32 bit value from a peripheral address without the write barrier
    /// You should only use this when your code has previously called bcm2835_peri_write()
    /// within the same peripheral, and no other peripheral access has occurred since.
    /// \param[in] paddr Physical address to read from. See BCM2835_GPIO_BASE etc.
    /// \param[in] value The 32 bit value to write
    /// \sa Physical Addresses
    extern void bcm2835_peri_write_nb(volatile uint32_t* paddr, uint32_t value);

    /// Alters a number of bits in a 32 peripheral regsiter.
    /// It reads the current valu and then alters the bits deines as 1 in mask,
    /// according to the bit value in value.
    /// All other bits that are 0 in the mask are unaffected.
    /// Use this to alter a subset of the bits in a register.
    /// The write is done twice, and is therefore always safe in terms of
    /// manual section 1.3 Peripheral access precautions for correct memory ordering
    /// \param[in] paddr Physical address to read from. See BCM2835_GPIO_BASE etc.
    /// \param[in] value The 32 bit value to write, masked in by mask.
    /// \param[in] mask Bitmask that defines the bits that will be altered in the register.
    /// \sa Physical Addresses
    extern void bcm2835_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask);
    /// @} // end of lowlevel

    /// \defgroup gpio GPIO register access
    /// These functions allow you to control the GPIO interface. You can set the
    /// function of each GPIO pin, read the input state and set the output state.
    /// @{

    /// Sets the Function Select register for the given pin, which configures
    /// the pin as Input, Output or one of the 6 alternate functions.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from RPiGPIOPin.
    /// \param[in] mode Mode to set the pin to, one of BCM2835_GPIO_FSEL_* from \ref bcm2835FunctionSelect
    extern void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode);

    /// Sets the specified pin output to
    /// HIGH.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    /// \sa bcm2835_gpio_write()
    extern void bcm2835_gpio_set(uint8_t pin);

    /// Sets the specified pin output to
    /// LOW.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    /// \sa bcm2835_gpio_write()
    extern void bcm2835_gpio_clr(uint8_t pin);

    /// Reads the current level on the specified
    /// pin and returns either HIGH or LOW. Works whether or not the pin
    /// is an input or an output.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    /// \return the current level  either HIGH or LOW
    extern uint8_t bcm2835_gpio_lev(uint8_t pin);

    /// Event Detect Status.
    /// Tests whether the specified pin has detected a level or edge
    /// as requested by bcm2835_gpio_ren(), bcm2835_gpio_fen(), bcm2835_gpio_hen(),
    /// bcm2835_gpio_len(), bcm2835_gpio_aren(), bcm2835_gpio_afen().
    /// Clear the flag for a given pin by calling bcm2835_gpio_set_eds(pin);
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    /// \return HIGH if the event detect status for th given pin is true.
    extern uint8_t bcm2835_gpio_eds(uint8_t pin);

    /// Sets the Event Detect Status register for a given pin to 1,
    /// which has the effect of clearing the flag. Use this afer seeing
    /// an Event Detect Status on the pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_set_eds(uint8_t pin);

    /// Enable Rising Edge Detect Enable for the specified pin.
    /// When a rising edge is detected, sets the appropriate pin in Event Detect Status.
    /// The GPRENn registers use
    /// synchronous edge detection. This means the input signal is sampled using the
    /// system clock and then it is looking for a ?011? pattern on the sampled signal. This
    /// has the effect of suppressing glitches.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_ren(uint8_t pin);

    /// Disable Rising Edge Detect Enable for the specified pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_clr_ren(uint8_t pin);

    /// Enable Falling Edge Detect Enable for the specified pin.
    /// When a falling edge is detected, sets the appropriate pin in Event Detect Status.
    /// The GPRENn registers use
    /// synchronous edge detection. This means the input signal is sampled using the
    /// system clock and then it is looking for a ?100? pattern on the sampled signal. This
    /// has the effect of suppressing glitches.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_fen(uint8_t pin);

    /// Disable Falling Edge Detect Enable for the specified pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_clr_fen(uint8_t pin);

    /// Enable High Detect Enable for the specified pin.
    /// When a HIGH level is detected on the pin, sets the appropriate pin in Event Detect Status.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_hen(uint8_t pin);

    /// Disable High Detect Enable for the specified pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_clr_hen(uint8_t pin);

    /// Enable Low Detect Enable for the specified pin.
    /// When a LOW level is detected on the pin, sets the appropriate pin in Event Detect Status.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_len(uint8_t pin);

    /// Disable Low Detect Enable for the specified pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_clr_len(uint8_t pin);

    /// Enable Asynchronous Rising Edge Detect Enable for the specified pin.
    /// When a rising edge is detected, sets the appropriate pin in Event Detect Status.
    /// Asynchronous means the incoming signal is not sampled by the system clock. As such
    /// rising edges of very short duration can be detected.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_aren(uint8_t pin);

    /// Disable Asynchronous Rising Edge Detect Enable for the specified pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_clr_aren(uint8_t pin);

    /// Enable Asynchronous Falling Edge Detect Enable for the specified pin.
    /// When a falling edge is detected, sets the appropriate pin in Event Detect Status.
    /// Asynchronous means the incoming signal is not sampled by the system clock. As such
    /// falling edges of very short duration can be detected.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_afen(uint8_t pin);

    /// Disable Asynchronous Falling Edge Detect Enable for the specified pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    extern void bcm2835_gpio_clr_afen(uint8_t pin);

    /// Sets the Pull-up/down register for the given pin. This is
    /// used with bcm2835_gpio_pudclk() to set the  Pull-up/down resistor for the given pin.
    /// However, it is usually more convenient to use bcm2835_gpio_set_pud().
    /// \param[in] pud The desired Pull-up/down mode. One of BCM2835_GPIO_PUD_* from bcm2835PUDControl
    /// \sa bcm2835_gpio_set_pud()
    extern void bcm2835_gpio_pud(uint8_t pud);

    /// Clocks the Pull-up/down value set earlier by bcm2835_gpio_pud() into the pin.
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    /// \param[in] on HIGH to clock the value from bcm2835_gpio_pud() into the pin.
    /// LOW to remove the clock.
    /// \sa bcm2835_gpio_set_pud()
    extern void bcm2835_gpio_pudclk(uint8_t pin, uint8_t on);

    /// Reads and returns the Pad Control for the given GPIO group.
    /// \param[in] group The GPIO pad group number, one of BCM2835_PAD_GROUP_GPIO_*
    /// \return Mask of bits from BCM2835_PAD_* from \ref bcm2835PadGroup
    extern uint32_t bcm2835_gpio_pad(uint8_t group);

    /// Sets the Pad Control for the given GPIO group.
    /// \param[in] group The GPIO pad group number, one of BCM2835_PAD_GROUP_GPIO_*
    /// \param[in] control Mask of bits from BCM2835_PAD_* from \ref bcm2835PadGroup
    extern void bcm2835_gpio_set_pad(uint8_t group, uint32_t control);

    /// Delays for the specified number of milliseconds.
    /// Uses nanosleep(), and therefore does not use CPU until the time is up.
    /// \param[in] millis Delay in milliseconds
    extern void bcm2835_delay (unsigned int millis);

    /// Delays for the specified number of microseconds.
    /// Uses nanosleep(), and therefore does not use CPU until the time is up.
    /// However, you are at the mercy of nanosleep(). From the manual for nanosleep:
    /// If the interval specified in req is not an exact multiple of the granularity
    /// underlying  clock  (see  time(7)),  then  the  interval will be
    /// rounded up to the next multiple.  Furthermore,  after  the  sleep  com-
    /// pletes,  there may still be a delay before the CPU becomes free to once
    /// again execute the calling thread.
    /// For times less than about 450 microseconds, uses a busy wait on a high resolution timer.
    /// It is reported that a delay of 0 microseconds on RaspberryPi will in fact
    /// result in a dleay of about 80 microseconds. Your mileage may vary.
    /// \param[in] micros Delay in microseconds
    extern void bcm2835_delayMicroseconds (unsigned int micros);

    /// Sets the output state of the specified pin
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    /// \param[in] on HIGH sets the output to HIGH and LOW to LOW.
    extern void bcm2835_gpio_write(uint8_t pin, uint8_t on);

    /// Sets the Pull-up/down mode for the specified pin. This is more convenient than
    /// clocking the mode in with bcm2835_gpio_pud() and bcm2835_gpio_pudclk().
    /// \param[in] pin GPIO number, or one of RPI_GPIO_P1_* from \ref RPiGPIOPin.
    /// \param[in] pud The desired Pull-up/down mode. One of BCM2835_GPIO_PUD_* from bcm2835PUDControl
    extern void bcm2835_gpio_set_pud(uint8_t pin, uint8_t pud);

    /// @}

    /// \defgroup spi SPI access
    /// These functions let you use SPI0 (Serial Peripheral Interface) to
    /// interface with an external SPI device.
    /// @{

    /// Start SPI operations.
    /// Forces RPi SPI0 pins P1-19 (MOSI), P1-21 (MISO), P1-23 (CLK), P1-24 (CE0) and P1-26 (CE1)
    /// to alternate function ALT0, which enables those pins for SPI interface.
    /// You should call bcm2835_spi_end() when all SPI funcitons are complete to return the pins to
    /// their default functions
    /// \sa  bcm2835_spi_end()
    extern void bcm2835_spi_begin(void);

    /// End SPI operations.
    /// SPI0 pins P1-19 (MOSI), P1-21 (MISO), P1-23 (CLK), P1-24 (CE0) and P1-26 (CE1)
    /// are returned to their default INPUT behaviour.
    extern void bcm2835_spi_end(void);

    /// Sets the SPI bit order
    /// NOTE: has no effect. Not supported by SPI0.
    /// Defaults to
    /// \param[in] order The desired bit order, one of BCM2835_SPI_BIT_ORDER_*,
  /// see \ref bcm2835SPIBitOrder
    extern void bcm2835_spi_setBitOrder(uint8_t order);

    /// Sets the SPI clock divider and therefore the
    /// SPI clock speed.
    /// \param[in] divider The desired SPI clock divider, one of BCM2835_SPI_CLOCK_DIVIDER_*,
  /// see \ref bcm2835SPIClockDivider
    extern void bcm2835_spi_setClockDivider(uint16_t divider);

    /// Sets the SPI data mode
    /// Sets the clock polariy and phase
    /// \param[in] mode The desired data mode, one of BCM2835_SPI_MODE*,
  /// see \ref bcm2835SPIMode
    extern void bcm2835_spi_setDataMode(uint8_t mode);

    /// Sets the chip select pin(s)
    /// When an bcm2835_spi_transfer() is made, the selected pin(s) will be asserted during the
    /// transfer.
    /// \param[in] cs Specifies the CS pins(s) that are used to activate the desired slave.
    ///   One of BCM2835_SPI_CS*, see \ref bcm2835SPIChipSelect
    extern void bcm2835_spi_chipSelect(uint8_t cs);

    /// Sets the chip select pin polarity for a given pin
    /// When an bcm2835_spi_transfer() occurs, the currently selected chip select pin(s)
    /// will be asserted to the
    /// value given by active. When transfers are not happening, the chip select pin(s)
    /// return to the complement (inactive) value.
    /// \param[in] cs The chip select pin to affect
    /// \param[in] active Whether the chip select pin is to be active HIGH
    extern void bcm2835_spi_setChipSelectPolarity(uint8_t cs, uint8_t active);

    /// Transfers one byte to and from the currently selected SPI slave.
    /// Asserts the currently selected CS pins (as previously set by bcm2835_spi_chipSelect)
    /// during the transfer.
    /// Clocks the 8 bit value out on MOSI, and simultaneously clocks in data from MISO.
    /// Returns the read data byte from the slave.
    /// Uses polled transfer as per section 10.6.1 of the BCM 2835 ARM Peripherls manual
    /// \param[in] value The 8 bit data byte to write to MOSI
    /// \return The 8 bit byte simultaneously read from  MISO
    /// \sa bcm2835_spi_transfern()
    extern uint8_t bcm2835_spi_transfer(uint8_t value);

    /// Transfers any number of bytes to and from the currently selected SPI slave.
    /// Asserts the currently selected CS pins (as previously set by bcm2835_spi_chipSelect)
    /// during the transfer.
    /// Clocks the len 8 bit bytes out on MOSI, and simultaneously clocks in data from MISO.
    /// The data read read from the slave is placed into rbuf. rbuf must be at least len bytes long
    /// Uses polled transfer as per section 10.6.1 of the BCM 2835 ARM Peripherls manual
    /// \param[in] tbuf Buffer of bytes to send.
    /// \param[out] rbuf Received bytes will by put in this buffer
    /// \param[in] len Number of bytes in the tbuf buffer, and the number of bytes to send/received
    /// \sa bcm2835_spi_transfer()
    extern void bcm2835_spi_transfernb(char* tbuf, char* rbuf, uint32_t len);

    /// Transfers any number of bytes to and from the currently selected SPI slave
    /// using bcm2835_spi_transfernb.
    /// The returned data from the slave replaces the transmitted data in the buffer.
    /// \param[in,out] buf Buffer of bytes to send. Received bytes will replace the contents
    /// \param[in] len Number of bytes int eh buffer, and the number of bytes to send/received
    /// \sa bcm2835_spi_transfer()
    extern void bcm2835_spi_transfern(char* buf, uint32_t len);


    /// @}

#ifdef __cplusplus
}
#endif

#endif // DECLS

#endif // BCM2835_H

/// @example blink.c
/// Blinks RPi GPIO pin 11 on and off

/// @example input.c
/// Reads the state of an RPi input pin

/// @example event.c
/// Shows how to use event detection on an input pin

/// @example spi.c
/// Shows how to use SPI interface to transfer a byte to and from an SPI device

/// @example spin.c
/// Shows how to use SPI interface to transfer a number of bytes to and from an SPI device
