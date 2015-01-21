#!/usr/bin/python

import smbus


class Port:
    def __init__(self):
        self.reset()

    def reset(self):
        self.value = 0x00
        self.dir = 0xFF
        self.pullup = 0x00


class MCP23017:

    __MCP23017_REG_BASE_A       = 0x00    # Base address for Port A registers
    __MCP23017_REG_BASE_B       = 0x10    # Base address for Port B registers
    __MCP23017_REG_IODIR        = 0x00    # I/O direction register
    __MCP23017_REG_IOPOL        = 0x01    # Input polarity register
    __MCP23017_REG_GPINTEN      = 0x02    # Interrupt on change register
    __MCP23017_REG_DEFVAL       = 0x03    # Default value register
    __MCP23017_REG_INTCON       = 0x04    # Interrupt on change control register
    __MCP23017_REG_IOCON        = 0x05    # Configuration register
    __MCP23017_REG_GPPU         = 0x06    # GPIO pull-up resistor register
    __MCP23017_REG_INTF         = 0x07    # Interrupt flag register
    __MCP23017_REG_INTCAP       = 0x08    # Interrupt capture register
    __MCP23017_REG_GPIO         = 0x09    # Port register
    __MCP23017_REG_OLAT         = 0x0A    # Output latch register

    __MCP23017_IO_IN            = 1       # I/O direction is input
    __MCP23017_IO_OUT           = 0       # I/O direction is output
    __MCP23017_IOPOL_INV        = 1       # Input polarity is inverted
    __MCP23017_IOPOL_NINV       = 0       # Input polarity is not inverted
    __MCP23017_GPINTEN_EN       = 1       # GPIO interrupt enabled
    __MCP23017_GPINTEN_DIS      = 0       # GPIO interrupt disabled
    __MCP23017_INTCON_0         = 0       # Pin value is compared against previous value
    __MCP23017_INTCON_1         = 1       # Pin value is compared against DEFVAL
    __MCP23017_IOCON_BANK0      = 0b00000000       # A/B registers are paired
    __MCP23017_IOCON_BANK1      = 0b10000000       # Registers associated with each port are segregated
    __MCP23017_IOCON_MIRROR_DIS = 0b00000000       # INT pins are seperated
    __MCP23017_IOCON_MIRROR_EN  = 0b01000000       # INT pins are functionally OR'ed
    __MCP23017_IOCON_SEQOP_DIS  = 0b00000000       # Address pointer does not automaticall increment
    __MCP23017_IOCON_SEQOP_EN   = 0b00100000       # Address pointer does automaticall increment
    __MCP23017_IOCON_DISSLW_DIS = 0b00000000       # Slew rate control is disabled
    __MCP23017_IOCON_DISSLW_EN  = 0b00010000       # Slew rate control is enabled
    __MCP23017_IOCON_HAEN_DIS   = 0b00000000       # Hardware address is disabled A2,A1,A0 = 0
    __MCP23017_IOCON_HAEN_EN    = 0b00001000       # Hardware address is enabled
    __MCP23017_IOCON_ODR_DIS    = 0b00000000       # Disables interrupt for open drain pins
    __MCP23017_IOCON_ODR_EN     = 0b00000100       # Enables interrupt for open drain pins
    __MCP23017_IOCON_INTPOL_HIGH= 0b00000000       # Polarity of INT pin is set to active high
    __MCP23017_IOCON_INTPOL_LOW = 0b00000010       # Polarity of INT pin is set to active low
    __MCP23017_GPPU_EN          = 1                # Pull-up resistor is enabled
    __MCP23017_GPPU_DIS         = 0                # Pull-up resistor is disabled

    DIR_IN = __MCP23017_IO_IN
    DIR_OUT = __MCP23017_IO_OUT

    PORT_A = 0
    PORT_B = 1

    PULLUP_EN = __MCP23017_GPPU_EN
    PULLUP_DIS = __MCP23017_GPPU_DIS

    def __init__(self, address=0x20, busId=2, debug=False):
        self.port = [Port(), Port()]
        self.portOld = [Port(), Port()]
        self.portBase = [self.__MCP23017_REG_BASE_A, self.__MCP23017_REG_BASE_B]
        self.i2c = smbus.SMBus(busId)
        self.address = address
        self.debug = debug

    def init(self):
        config = 0
        config |= self.__MCP23017_IOCON_BANK1
        config |= self.__MCP23017_IOCON_MIRROR_DIS
        config |= self.__MCP23017_IOCON_SEQOP_DIS
        config |= self.__MCP23017_IOCON_DISSLW_EN
        config |= self.__MCP23017_IOCON_HAEN_EN
        config |= self.__MCP23017_IOCON_ODR_DIS
        config |= self.__MCP23017_IOCON_INTPOL_HIGH
        self.i2c.write_byte_data(self.address, self.__MCP23017_REG_IOCON, config)

        # Enable all latches
        for i in range(0, 2):
            reg = self.portBase[i] + self.__MCP23017_REG_OLAT
            self.i2c.write_byte_data(self.address, reg, 0xFF)

            self.port[i].reset()
            self.portOld[i].reset()
            self.updateDir(i, self.port[i].dir)
            self.updatePullup(i, self.port[i].pullup)
            self.updateValue(i, self.port[i].value)

    def setDir(self, port, pin, dir):
        newDir = self.port[port].dir
        newDir &= ~(1 << pin)
        newDir |= (dir << pin)
        self.port[port].dir = newDir

    def setValue(self, port, pin, value):
        newValue = self.port[port].value
        newValue &= ~(1 << pin)
        newValue |= (value << pin)
        self.port[port].value = newValue

    def setPullup(self, port, pin, pullup):
        newPullup = self.port[port].pullup
        newPullup &= ~(1 << pin)
        newPullup |= (pullup << pin)
        self.port[port].pullup = newPullup

    def getValue(self, port, pin):
        return bool(self.port[port].value & (1 << pin))

    def updateValue(self, port, value):
        reg = self.portBase[port] + self.__MCP23017_REG_GPIO
        self.i2c.write_byte_data(self.address, reg, value)
        if (self.debug):
            print(("wrote value: " + "{0:b}".format(value)))

    def updateDir(self, port, dir):
        reg = self.portBase[port] + self.__MCP23017_REG_IODIR
        self.i2c.write_byte_data(self.address, reg, dir)
        if (self.debug):
            print(("wrote dir value: " + "{0:b}".format(dir)))

    def updatePullup(self, port, pullup):
        reg = self.portBase[port] + self.__MCP23017_REG_GPPU
        self.i2c.write_byte_data(self.address, reg, pullup)
        if (self.debug):
            print(("wrote pullup value: " + "{0:b}".format(pullup)))

    def readValues(self, port):
        reg = self.portBase[port] + self.__MCP23017_REG_GPIO
        values = self.i2c.read_byte_data(self.address, reg)
        if (self.debug):
            print(("read values: " + "{0:b}".format(values)))
        return values

    def read(self):
        for i in range(0, 2):
            self.port[i].value = self.readValues(i)
            self.portOld[i].value = self.port[i].value

    def write(self):
        for i in range(0, 2):
            if (self.port[i].dir != self.portOld[i].dir):
                self.updateDir(i, self.port[i].dir)
                self.portOld[i].dir = self.port[i].dir

            if (self.port[i].pullup != self.portOld[i].pullup):
                self.updatePullup(i, self.port[i].pullup)
                self.portOld[i].pullup = self.port[i].pullup

            if (self.port[i].value != self.portOld[i].value):
                self.updateValue(i, self.port[i].value)
                self.portOld[i].value = self.port[i].value


#gpio = MCP23017(0x20, 2, True)
#for i in range(0, 6):
#    gpio.setDir(MCP23017.PORT_A, i, MCP23017.DIR_IN)
#    gpio.setPullup(MCP23017.PORT_A, i, MCP23017.PULLUP_EN)

#while True:
##    time.sleep(0.01)
#    gpio.update()
#    for i in range(0, 6):
#        print "pin" + str(i) + "value: " + str(gpio.getValue(MCP23017.PORT_A,i))
