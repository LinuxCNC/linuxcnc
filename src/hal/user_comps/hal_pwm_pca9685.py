#!/usr/bin/python2
# encoding: utf-8
"""
Pwm.py

Created by Alexander Rössler on 2014-03-24.
"""

from drivers.PCA9685 import PCA9685

import argparse
import time

import hal


class Pin:
    def __init__(self):
        self.enable = False
        self.value = 0.0
        self.pin = 0
        self.halValuePin = 0
        self.halEnablePin = 0

    def reset(self):
        self.enable = False
        self.value = 0.0


def getHalName(pin):
    return "out-" + '{0:02d}'.format(pin.pin)


parser = argparse.ArgumentParser(description="""
HAL component to control the PCA9685 LED driver PWM IC

pins:
   bit out error          indicates an I2C error conditions
   bit out no-error       inverse of the error pin
   bit out watchdog       toggles with every cycle
   bit in  reset          resets the I2C component on falling edge
   bit in  out-nn.enable  enables PWM nn
   bit in  out-nn.value   duty cycles for the PWM output nn
""")
parser.add_argument('-n', '--name', help='HAL component name', required=True)
parser.add_argument('-b', '--bus_id', help='I2C bus id', default=2)
parser.add_argument('-a', '--address', help='I2C device address', default=0x20)
parser.add_argument('-i', '--interval', help='I2C update interval', default=0.05)
parser.add_argument('-d', '--delay', help='Delay before the i2c should be updated', default=0.0)
args = parser.parse_args()

updateInterval = float(args.interval)
delayInterval = float(args.delay)
error = True
watchdog = True
reset = False
resetTriggered = False

pwm = PCA9685(busId=int(args.bus_id),
                address=int(args.address))

# Create pins
pins = []
PINCOUNT = 16
for i in range(0, PINCOUNT):
    pin = Pin()
    pin.pin = i
    pins.append(pin)

# Initialize HAL
h = hal.component(args.name)
frequencyValue = 1000
frequencyPin = h.newpin("frequency", hal.HAL_FLOAT, hal.HAL_IN)
frequencyPin.value = 1000
for pin in pins:
    pin.halValuePin = h.newpin(getHalName(pin) + ".value", hal.HAL_FLOAT, hal.HAL_IN)
    pin.halEnablePin = h.newpin(getHalName(pin) + ".enable", hal.HAL_BIT, hal.HAL_IN)
halErrorPin = h.newpin("error", hal.HAL_BIT, hal.HAL_OUT)
halNoErrorPin = h.newpin("no-error", hal.HAL_BIT, hal.HAL_OUT)
halWatchdogPin = h.newpin("watchdog", hal.HAL_BIT, hal.HAL_OUT)
halResetPin = h.newpin("reset", hal.HAL_BIT, hal.HAL_IN)
h.ready()

halErrorPin.value = error
halNoErrorPin.value = not error
halWatchdogPin.value = watchdog
halResetPin.value = reset

try:
    time.sleep(delayInterval)
    while (True):
        updatePin = 0
        try:
            if (halResetPin.value != reset):
                reset = halResetPin.value
                if not reset:  # falling edge
                    resetTriggered = True

            if error or resetTriggered:
                pwm.init()
                for pin in pins:
                    pin.reset()
                error = False
                resetTriggered = False

            updated = False  # when idle update a pin to monitor the I2C state

            if (frequencyPin.value != frequencyValue):
                pwm.setPwmClock(frequencyValue)
                frequencyValue = frequencyPin.value
                updated = True

            for pin in pins:
                value = pin.halValuePin.value
                enable = pin.halEnablePin.value
                if (enable != pin.enable):
                    if (enable):
                        pwm.setPwmDuty(pin.pin, value)
                        pin.value = value
                    else:
                        pwm.setPwmDuty(pin.pin, 0.0)
                    pin.enable = enable
                    updated = True
                elif (value != pin.value):
                    if (pin.enable):
                        pwm.setPwmDuty(pin.pin, value)
                        updated = True
                    pin.value = value

            if not updated:
                pin = pins[updatePin]
                if (pin.enable):
                    pwm.setPwmDuty(pin.pin, pin.value)
                else:
                    pwm.setPwmDuty(pin.pin, 0.0)
                if updatePin < (len(pins) - 1):
                    updatePin += 1
                else:
                    updatePin = 0
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
