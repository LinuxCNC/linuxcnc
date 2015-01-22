#!/usr/bin/python

import smbus


class ADS7828:

    # Config Register
    __ADS7828_CONFIG_SD_DIFFERENTIAL      = 0b00000000
    __ADS7828_CONFIG_SD_SINGLE            = 0b10000000
    __ADS7828_CONFIG_CS_CH0               = 0b00000000
    __ADS7828_CONFIG_CS_CH2               = 0b00010000
    __ADS7828_CONFIG_CS_CH4               = 0b00100000
    __ADS7828_CONFIG_CS_CH6               = 0b00110000
    __ADS7828_CONFIG_CS_CH1               = 0b01000000
    __ADS7828_CONFIG_CS_CH3               = 0b01010000
    __ADS7828_CONFIG_CS_CH5               = 0b01100000
    __ADS7828_CONFIG_CS_CH7               = 0b01110000
    __ADS7828_CONFIG_PD_OFF               = 0b00000000
    __ADS7828_CONFIG_PD_REFOFF_ADON       = 0b00000100
    __ADS7828_CONFIG_PD_REFON_ADOFF       = 0b00001000
    __ADS7828_CONFIG_PD_REFON_ADON        = 0b00001100

    def __init__(self, address=0x48, busId=2, debug=False):
        self.i2c = smbus.SMBus(busId)
        self.address = address
        self.debug = debug

    def readChannel(self, ch):
        config = 0
        config |= self.__ADS7828_CONFIG_SD_SINGLE
        config |= self.__ADS7828_CONFIG_PD_REFOFF_ADON

        if ch == 0:
            config |= self.__ADS7828_CONFIG_CS_CH0
        elif ch == 1:
            config |= self.__ADS7828_CONFIG_CS_CH1
        elif ch == 2:
            config |= self.__ADS7828_CONFIG_CS_CH2
        elif ch == 3:
            config |= self.__ADS7828_CONFIG_CS_CH3
        elif ch == 4:
            config |= self.__ADS7828_CONFIG_CS_CH4
        elif ch == 5:
            config |= self.__ADS7828_CONFIG_CS_CH5
        elif ch == 6:
            config |= self.__ADS7828_CONFIG_CS_CH6
        elif ch == 7:
            config |= self.__ADS7828_CONFIG_CS_CH7

        data = self.i2c.read_i2c_block_data(self. address, config, 2)
        return ((data[0] << 8) + data[1])

#adc = ADS7828(0x48, 2, True)
#test = []
#r2temp = R2Temp("semitec_103GT_2")
#while True:
#    #print(("ch0: " + str(adc.readChannel(0))))
#    time.sleep(0.3)
#    print(("ch0: " + str(adc.readChannel(0))))
#    print(("ch1: " + str(adc.readChannel(1))))
#    print(("ch2: " + str(adc.readChannel(2))))
#    print(("ch3: " + str(adc.readChannel(3))))
#    print(("ch4: " + str(adc.readChannel(4))))
#    print(("ch5: " + str(adc.readChannel(5))))
#    print(("ch6: " + str(adc.readChannel(6))))
#    print(("ch7: " + str(adc.readChannel(7))))
#    print(("---------------------------"))

#    adcValue = float(adc.readChannel(0))
#    test.append(adcValue)
#    if (len(test) > 20):
#        test.pop(0)
#    sum = 0.0
#    for value in test:
#        sum += value
#    sum /= len(test)
#    print sum
#    print 4095.0/sum
#    R1=4700.0
#    R2=R1/(max(4095.0/sum,0.000001)-1)
#    print round(r2temp.r2t(R2)*10.0)/10.0