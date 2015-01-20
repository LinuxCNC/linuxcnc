#!/usr/bin/python
# encoding: utf-8
"""
Gpio.py

Created by Alexander Rössler on 2014-03-24.
"""

from drivers.MCP23017 import MCP23017

import argparse
import time
import sys

import hal


class Pin:
    def __init__(self):
        self.port = MCP23017.PORT_A
        self.direction = MCP23017.DIR_IN
        self.pullup = MCP23017.PULLUP_DIS
        self.pin = 0
        self.halPin = 0
        self.halPullupPin = 0
        self.halInvertedPin = 0


def parseInputPin(pinRaw, direction):
    if (len(pinRaw) != 3):
        print(("wrong input"))
        sys.exit(1)

    pin = Pin()
    if (pinRaw[0] == 'A'):
        pin.port = MCP23017.PORT_A
    elif (pinRaw[0] == 'B'):
        pin.port = MCP23017.PORT_B
    else:
        print(("wrong port input"))
        sys.exit(1)
    pin.pin = int(pinRaw[1:3])
    pin.direction = direction
    return pin


def getHalName(pin):
    portName = ""
    dirName = ""
    if (pin.direction == MCP23017.DIR_IN):
        dirName = "in"
    else:
        dirName = "out"
    if (pin.port == MCP23017.PORT_A):
        portName = "A"
    else:
        portName = "B"
    return portName + "." + dirName + "-" + '{0:02d}'.format(pin.pin)


parser = argparse.ArgumentParser(description='HAL component to read LSM303 Accelerometer values')
parser.add_argument('-n', '--name', help='HAL component name', required=True)
parser.add_argument('-b', '--bus_id', help='I2C bus id', default=2)
parser.add_argument('-a', '--address', help='I2C device address', default=0x20)
parser.add_argument('-i', '--interval', help='I2C update interval', default=0.05)
parser.add_argument('-op', '--output_pins', help='Komma separated list of output pins e.g. A01,B02', default="")
parser.add_argument('-ip', '--input_pins', help='Komma separated list of input pins e.g. A01,B02', default="")
parser.add_argument('-d', '--delay', help='Delay before the i2c should be updated', default=0.0)
args = parser.parse_args()

updateInterval = float(args.interval)
delayInterval = float(args.delay)
error = True
watchdog = True

gpio = MCP23017(busId=int(args.bus_id),
                address=int(args.address))

# Parse arguments
pins = []

if (args.output_pins != ""):
    outputPinsRaw = args.output_pins.split(',')
    for pinRaw in outputPinsRaw:
        pins.append(parseInputPin(pinRaw, MCP23017.DIR_OUT))
if (args.input_pins != ""):
    inputPinsRaw = args.input_pins.split(',')
    for pinRaw in inputPinsRaw:
        pins.append(parseInputPin(pinRaw, MCP23017.DIR_IN))

if (len(pins) == 0):
    print(("No pins specified"))
    sys.exit(1)

# Initialize HAL
h = hal.component(args.name)
for pin in pins:
    if (pin.direction == MCP23017.DIR_IN):
        pin.halPin = h.newpin(getHalName(pin), hal.HAL_BIT, hal.HAL_OUT)
    else:
        pin.halPin = h.newpin(getHalName(pin), hal.HAL_BIT, hal.HAL_IN)
    pin.halPullupPin = h.newpin(getHalName(pin) + ".pullup", hal.HAL_BIT, hal.HAL_IN)
    pin.halInvertedPin = h.newpin(getHalName(pin) + ".invert", hal.HAL_BIT, hal.HAL_IN)
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
            if (error):
                gpio.init()
                error = False

            gpio.read()  # read
            for pin in pins:
                gpio.setDir(pin.port, pin.pin, pin.direction)
                if (pin.direction == MCP23017.DIR_IN):
                    pin.halPin.value = gpio.getValue(pin.port, pin.pin) != pin.halInvertedPin.value
                else:
                    gpio.setValue(pin.port, pin.pin, pin.halPin.value != pin.halInvertedPin.value)
                pullup = pin.halPullupPin.value
                if (pullup):
                    gpio.setPullup(pin.port, pin.pin, MCP23017.PULLUP_EN)
                else:
                    gpio.setPullup(pin.port, pin.pin, MCP23017.PULLUP_DIS)
            gpio.write()  # write
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
