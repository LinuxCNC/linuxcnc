#!/usr/bin/python

import time
import smbus


class PCA9685:
    __PCA9685_REG_MODE1        = 0x00    # Mode register 1
    __PCA9685_REG_MODE2        = 0x01    # Mode register 2
    __PCA9685_REG_SUBADR1      = 0x02    # I2C-bus subaddress 1
    __PCA9685_REG_SUBADR2      = 0x03    # I2C-bus subaddress 2
    __PCA9685_REG_SUBADR3      = 0x04    # I2C-bus subaddress 3
    __PCA9685_REG_ALLCALLADR   = 0x05    # LED All Call I2C-bus address

    __PCA9685_REG_LED_BASE     = 0x06    # LED output and brightness control base address
    __PCA9685_REG_LED_ON_L     = 0x00    #
    __PCA9685_REG_LED_ON_H     = 0x01    #
    __PCA9685_REG_LED_OFF_L    = 0x02    #
    __PCA9685_REG_LED_OFF_H    = 0x03    #
    __PCA9685_REG_LED_SIZE     = 0x04    #

    __PCA9685_REG_LED0_ON_L    = 0x06    # LED0 output and brightness control byte 0
    __PCA9685_REG_LED0_ON_H    = 0x07    # LED0 output and brightness control byte 1
    __PCA9685_REG_LED0_OFF_L   = 0x08    # LED0 output and brightness control byte 2
    __PCA9685_REG_LED0_OFF_H   = 0x09    # LED0 output and brightness control byte 3
    __PCA9685_REG_LED1_ON_L    = 0x0A    # LED1 output and brightness control byte 0
    __PCA9685_REG_LED1_ON_H    = 0x0B    # LED1 output and brightness control byte 1
    __PCA9685_REG_LED1_OFF_L   = 0x0C    # LED1 output and brightness control byte 2
    __PCA9685_REG_LED1_OFF_H   = 0x0D    # LED1 output and brightness control byte 3

    __PCA9685_REG_ALL_LED_ON_L = 0xFA    # All LED output and brightness control byte 0
    __PCA9685_REG_ALL_LED_ON_H = 0xFB    # All LED output and brightness control byte 1
    __PCA9685_REG_ALL_LED_OFF_L= 0xFC    # All LED output and brightness control byte 2
    __PCA9685_REG_ALL_LED_OFF_H= 0xFD    # All LED output and brightness control byte 3
    __PCA9685_REG_PRE_SCALE    = 0xFE    # Prescaler for output frequency
    __PCA9685_REG_TESTMODE     = 0xFF    # Defines the test mode to be entered


    __PCA9685_MODE1_RESTART_ENABLED    = 0b10000000    # Restart enabled
    __PCA9685_MODE1_RESTART_DISABLED   = 0b00000000    # Restart disabled
    __PCA9685_MODE1_EXTCLK_ENABLED     = 0b01000000    # External clock enabled
    __PCA9685_MODE1_EXTCLK_DISABLED    = 0b00000000    # External clock disabled
    __PCA9685_MODE1_AI_ENABLED         = 0b00100000    # Register Auto-increment enabled
    __PCA9685_MODE1_AI_DISABLED        = 0b00000000    # Register Auto-increment disabled
    __PCA9685_MODE1_SLEEP_ENABLED      = 0b00010000    # Low power mode, oscillator off
    __PCA9685_MODE1_SLEEP_DISABLED     = 0b00000000    # Normal mode
    __PCA9685_MODE1_SUB1_ENABLED       = 0b00001000    # I2C subaddress 1 enabled
    __PCA9685_MODE1_SUB1_DISABLED      = 0b00000000    # I2C subaddress 1 disabled
    __PCA9685_MODE1_SUB2_ENABLED       = 0b00000100    # I2C subaddress 2 enabled
    __PCA9685_MODE1_SUB2_DISABLED      = 0b00000000    # I2C subaddress 2 disabled
    __PCA9685_MODE1_SUB3_ENABLED       = 0b00000010    # I2C subaddress 3 enabled
    __PCA9685_MODE1_SUB3_DISABLED      = 0b00000000    # I2C subaddress 3 disabled
    __PCA9685_MODE1_ALLCALL_ENABLED    = 0b00000001    # I2C All Call address enabled
    __PCA9685_MODE1_ALLCALL_DISABLED   = 0b00000000    # I2C All Call address disabled

    __PCA9685_MODE2_INVRT_ENABLED      = 0b00010000    # Output logic state inverted when OE=0
    __PCA9685_MODE2_INVRT_DISABLED     = 0b00000000    # Output logic state not inverted when OE=0
    __PCA9685_MODE2_OCH_STOP           = 0b00000000    # Outputs change on STOP command
    __PCA9685_MODE2_OCH_ACK            = 0b00001000    # Outputs change on ACK command
    __PCA9685_MODE2_OUTDRV_OPENDRAIN   = 0b00000000    # Outputs are configured to open drain structure
    __PCA9685_MODE2_OUTDRV_TOTEMPOLE   = 0b00000100    # Outputs are configured to totem pole structure
    __PCA9685_MODE2_OUTNE_0            = 0b00000000    # LEDn=0 when OE=1
    __PCA9685_MODE2_OUTNE_1            = 0b00000001    # LEDn=1 when OE=1 and OUTDRV=1 or high-impedance when OUTDRV=0
    __PCA9685_MODE2_OUTNE_2            = 0b00000010    # LEDn=high-impedance

    def __init__(self, address=0x46, busId=2, debug=False):
        self.i2c = smbus.SMBus(busId)
        self.address = address
        self.debug = debug

    def init(self):
        config = 0
        config |= self.__PCA9685_MODE1_RESTART_DISABLED
        config |= self.__PCA9685_MODE1_EXTCLK_DISABLED
        config |= self.__PCA9685_MODE1_AI_DISABLED
        config |= self.__PCA9685_MODE1_SLEEP_DISABLED
        config |= self.__PCA9685_MODE1_SUB1_DISABLED
        config |= self.__PCA9685_MODE1_SUB1_DISABLED
        config |= self.__PCA9685_MODE1_SUB3_DISABLED
        config |= self.__PCA9685_MODE1_ALLCALL_DISABLED
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_MODE1, config)

        config = 0
        config |= self.__PCA9685_MODE2_INVRT_DISABLED
        config |= self.__PCA9685_MODE2_OCH_STOP
        config |= self.__PCA9685_MODE2_OUTDRV_TOTEMPOLE
        config |= self.__PCA9685_MODE2_OUTNE_0
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_MODE2, config)

        self.setAllPwm(0,0)

    def setPwm(self, ch, on, off):
        regAddress = self.__PCA9685_REG_LED_BASE + self.__PCA9685_REG_LED_SIZE * ch
        on_l = on & 0xFF
        on_h = (on >> 8) & 0x0F
        off_l = off & 0xFF
        off_h = (off >> 8) & 0x0F
        self.i2c.write_byte_data(self.address, regAddress + self.__PCA9685_REG_LED_ON_L, on_l)
        self.i2c.write_byte_data(self.address, regAddress + self.__PCA9685_REG_LED_ON_H, on_h)
        self.i2c.write_byte_data(self.address, regAddress + self.__PCA9685_REG_LED_OFF_L, off_l)
        self.i2c.write_byte_data(self.address, regAddress + self.__PCA9685_REG_LED_OFF_H, off_h)

    def setAllPwm(self, on, off):
        on_l = on & 0xFF
        on_h = (on >> 8) & 0x0F
        off_l = off & 0xFF
        off_h = (off >> 8) & 0x0F
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_ALL_LED_ON_L, on_l)
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_ALL_LED_ON_H, on_h)
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_ALL_LED_OFF_L, off_l)
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_ALL_LED_OFF_H, off_h)

    def setPwmDuty(self, ch, duty):
        on = 0
        off = int(duty * 4095)
        self.setPwm(ch, on, off)

    def setPrescaler(self, prescaler):
        mode1Save = self.i2c.read_byte_data(self.address, self.__PCA9685_REG_MODE1)
        config = mode1Save | self.__PCA9685_MODE1_SLEEP_ENABLED
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_MODE1, config)    # sleep
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_PRE_SCALE, prescaler)    # set prescaler
        self.i2c.write_byte_data(self.address, self.__PCA9685_REG_MODE1, mode1Save) # restore settings

    def setPwmClock(self, clk):
        clk = max(100, clk)
        clk = min(1000, clk)
        prescaler = int(round(25E6 / (4096 * clk) - 1))
        self.setPrescaler(prescaler)

#pwm = PCA9685(0x46, 2, True)
#pwm.setPwmClock(1000)
#pwm.setPwmDuty(0,0.001)