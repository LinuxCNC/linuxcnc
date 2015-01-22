#!/usr/bin/python
# encoding: utf-8
"""
Temperature.py

Created by Alexander Rössler on 2014-03-24.
"""

from drivers.ADS7828 import ADS7828
from fdm.r2temp import R2Temp

import argparse
import time
import sys

import hal


class Pin:
    def __init__(self):
        self.pin = 0
        self.r2temp = None
        self.halValuePin = 0
        self.halRawPin = 0
        self.filterSamples = []
        self.filterSize = 10
        self.rawValue = 0.0
        self.filterSamples = []
        self.rawValue = 0.0

    def addSample(self, value):
        self.filterSamples.append(value)
        if (len(self.filterSamples) > self.filterSize):
            self.filterSamples.pop(0)
        sampleSum = 0.0
        for sample in self.filterSamples:
            sampleSum += sample
        self.rawValue = sampleSum / len(self.filterSamples)


def getHalName(pin):
    return "ch-" + '{0:02d}'.format(pin.pin)


def adc2Temp(pin):
    R1 = 4700.0
    R2 = R1 / max(4095.0 / pin.rawValue - 1.0, 0.000001)
    return round(pin.r2temp.r2t(R2) * 10.0) / 10.0


parser = argparse.ArgumentParser(description='HAL component to read Temperature values over I2C')
parser.add_argument('-n', '--name', help='HAL component name', required=True)
parser.add_argument('-b', '--bus_id', help='I2C bus id', default=2)
parser.add_argument('-a', '--address', help='I2C device address', default=0x20)
parser.add_argument('-i', '--interval', help='I2C update interval', default=0.05)
parser.add_argument('-c', '--channels', help='Komma separated list of channels and thermistors to use e.g. 01:semitec_103GT_2,02:epcos_B57560G1104', required=True)
parser.add_argument('-f', '--filter_size', help='Size of the low pass filter to use', default=10)
parser.add_argument('-d', '--delay', help='Delay before the i2c should be updated', default=0.0)

args = parser.parse_args()

updateInterval = float(args.interval)
delayInterval = float(args.delay)
filterSize = int(args.filter_size)
error = True
watchdog = True

adc = ADS7828(busId=int(args.bus_id),
                address=int(args.address))

# Create pins
pins = []

if (args.channels != ""):
    channelsRaw = args.channels.split(',')
    for channel in channelsRaw:
        pinRaw = channel.split(':')
        if (len(pinRaw) != 2):
            print(("wrong input"))
            sys.exit(1)
        pin = Pin()
        pin.pin = int(pinRaw[0])
        if ((pin.pin > 7) or (pin.pin < 0)):
            print(("Pin not available"))
            sys.exit(1)
        if (pinRaw[1] != "none"):
            pin.r2temp = R2Temp(pinRaw[1])
        pin.filterSize = filterSize
        pins.append(pin)

# Initialize HAL
h = hal.component(args.name)
for pin in pins:
    pin.halRawPin = h.newpin(getHalName(pin) + ".raw", hal.HAL_FLOAT, hal.HAL_OUT)
    if (pin.r2temp is not None):
        pin.halValuePin = h.newpin(getHalName(pin) + ".value", hal.HAL_FLOAT, hal.HAL_OUT)
halErrorPin = h.newpin("error", hal.HAL_BIT, hal.HAL_OUT)
halNoErrorPin = h.newpin("no-error", hal.HAL_BIT, hal.HAL_OUT)
halWatchdogPin = h.newpin("watchdog", hal.HAL_BIT, hal.HAL_OUT)
h.ready()

halErrorPin.value = error
halNoErrorPin.value = not error
halWatchdogPin.value = watchdog

try:
    time.sleep(delayInterval)
    while (True):
        try:
            for pin in pins:
                value = float(adc.readChannel(pin.pin))
                pin.addSample(value)
                pin.halRawPin.value = pin.rawValue
                if (pin.r2temp is not None):
                    pin.halValuePin.value = adc2Temp(pin)
            error = False
        except IOError as e:
            error = True

        halErrorPin.value = error
        halNoErrorPin.value = not error
        watchdog = not watchdog
        halWatchdogPin.value = watchdog
        time.sleep(updateInterval)
except:
    print(("exiting HAL component " + args.name))
    h.exit()
