#!/usr/bin/python
# encoding: utf-8

########################################################################
# Description: temp.py                                                 #
# This code reads an ADC input on the BeagleBone and converts the      #
# resulting value into a temperature according to the thermistor       #
# type, accounting for the analog input circuty as implemented on      #
# the BeBoPr cape                                                      #
#                                                                      #
# Author(s): Charles Steinkuehler                                      #
# License: GNU GPL Version 2.0 or (at your option) any later version.  #
#                                                                      #
# Major Changes:                                                       #
# 2013-June   Charles Steinkuehler                                     #
#             Initial version                                          #
# 2014-July   Alexander Roessler                                       #
#             Port to the R2Temp component                             #
########################################################################
# Copyright (C) 2013  Charles Steinkuehler                             #
#                     <charles AT steinkuehler DOT net>                #
#                                                                      #
# This program is free software; you can redistribute it and/or        #
# modify it under the terms of the GNU General Public License          #
# as published by the Free Software Foundation; either version 2       #
# of the License, or (at your option) any later version.               #
#                                                                      #
# This program is distributed in the hope that it will be useful,      #
# but WITHOUT ANY WARRANTY; without even the implied warranty of       #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        #
# GNU General Public License for more details.                         #
#                                                                      #
# You should have received a copy of the GNU General Public License    #
# along with this program; if not, write to the Free Software          #
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA        #
# 02110-1301, USA.                                                     #
#                                                                      #
# THE AUTHORS OF THIS PROGRAM ACCEPT ABSOLUTELY NO LIABILITY FOR       #
# ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE   #
# TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of      #
# harming persons must have provisions for completely removing power   #
# from all motors, etc, before persons enter any danger area.  All     #
# machinery must be designed to comply with local and national safety  #
# codes, and the authors of this software can not, and do not, take    #
# any responsibility for such compliance.                              #
########################################################################

import argparse
import glob
import sys
import time

import hal
from fdm.r2temp import R2Temp


# The BeBoPr board thermistor input has one side grounded and the other side
# pulled high through a 2.05K resistor to 3.6V.  Following this is a 470R
# resistor, some protection diodes, and a voltage divider cosisting of two
# 10.0K resistors.  The ADC voltage read is the voltage across the lower 10K
# resistor in the 470R + 10K + 10K series chain
def adc2r_bebopr(pin):
    V_adc = pin.rawValue * 1.8 / 4096.0

    V_T  = 0.0  # Voltage across the thermistor (and the 470R + 10K + 10K resistor chain)
    I_PU = 0.0  # Current flowing through the 2.05K pull-up resistor
    R_TD = 0.0  # Resistance of thermistor and the 470R + 10K + 10K divider chain in parallel
    R_T  = 0.0  # Resistance of the thermistor

    V_T = V_adc * 2.0470

    # No dividing by zero or negative voltages despite what the ADC says!
    # Clip to a small positive value
    I_PU = max((3.6 - V_T) / 2050, 0.000001)

    R_TD = V_T / I_PU

    # Acutal resistance can't be negative, but we can get a negative value
    # from the equation below for some real ADC values, so clip to avoid
    # reporting crazy temperature values or dividing by zero
    if R_TD >= 20470:
        R_TD = 20470 - 0.1

    # 1 / Rtotal = 1 / ( 1 / R1 + 1 / R2 )
    # R2  = ( R1 * Rtotal ) / ( R1 - Rtotal )
    R_T = (20470 * R_TD) / (20470 - R_TD)

    # print "V_adc: %f V_T: %f  R_TD: %f  R_T: %f" % (V_adc, V_T, R_TD, R_T)

    return R_T

# The CRAMPS board thermistor input has one side grounded and the other side
# pulled high through a 1.00K resistor to 1.8V.  Following this is a 4.7K
# resistor, some protection diodes, and filtering capacitors.  The ADC voltage
# read is the filtered voltage across the thermistor.
def adc2r_cramps(pin):
    V_adc = pin.rawValue * 1.8 / 4096.0
    V_T  = 0.0  # Voltage across the thermistor
    R_PU = 2000.0 #Pull-up resistence 
    I_PU = 0.0  # Current flowing through the pull-up resistor
    R_T  = 0.0  # Resistance of the thermistor

    V_T = V_adc

    # No dividing by zero or negative voltages despite what the ADC says!
    # Clip to a small positive value
    I_PU = max((1.8 - V_T ) / R_PU, 0.000001) 

    R_T = V_T / I_PU

    return R_T


class Pin:
    def __init__(self):
        self.pin = 0
        self.r2temp = None
        self.halValuePin = 0
        self.halRawPin = 0
        self.filterSamples = []
        self.filterSize = 10
        self.rawValue = 0.0
        self.filename = ""
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


def adc2Temp(pin):
    if(args.cape_board == 'BeBoPr'):
        R = adc2r_bebopr(pin)
    elif (args.cape_board == 'CRAMPS'):
        R = adc2r_cramps(pin)
    else:
        print("Invalid -b cape  name: %s" % args.cape_board)
        print("Valid names are: BeBoPr, CRAMPS")
        sys.exit(1)
    return round(pin.r2temp.r2t(R) * 10.0) / 10.0


def getHalName(pin):
    return "ch-" + '{0:02d}'.format(pin.pin)


def checkAdcInput(pin):
    syspath = '/sys/devices/ocp.*/44e0d000.tscadc/tiadc/iio:device0/'
    tempName = glob.glob(syspath + 'in_voltage' + str(pin.pin) + '_raw')
    pin.filename = tempName[0]
    try:
        if len(pin.filename) > 0:
            f = open(pin.filename, 'r')
            f.close()
            time.sleep(0.001)
        else:
            raise UserWarning('Bad Filename')
    except (UserWarning, IOError):
        print(("Cannot read ADC input: %s" % pin.filename))
        sys.exit(1)


parser = argparse.ArgumentParser(description='HAL component to read ADC values and convert to temperature')
parser.add_argument('-n','--name', help='HAL component name',required=True)
parser.add_argument('-i', '--interval', help='Adc update interval', default=0.05)
parser.add_argument('-c', '--channels', help='Komma separated list of channels and thermistors to use e.g. 01:semitec_103GT_2,02:epcos_B57560G1104', required=True)
parser.add_argument('-f', '--filter_size', help='Size of the low pass filter to use', default=10)
parser.add_argument('-b', '--cape_board', help='Type of cape used', default='BeBoPr')

args = parser.parse_args()

updateInterval = float(args.interval)
filterSize = int(args.filter_size)
error = False
watchdog = True

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
        if ((pin.pin > 5) or (pin.pin < 0)):
            print(("Pin not available"))
            sys.exit(1)
        checkAdcInput(pin)
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
    while (True):
        try:
            for pin in pins:
                f = open(pin.filename, 'r')
                value = float(f.readline())
                pin.addSample(value)
                pin.halRawPin.value = pin.rawValue
                if (pin.r2temp is not None):
                    pin.halValuePin.value = adc2Temp(pin)
            error = False
        except IOError:
            error = True

        halErrorPin.value = error
        halNoErrorPin.value = not error
        watchdog = not watchdog
        halWatchdogPin.value = watchdog
        time.sleep(updateInterval)
except:
    print(("exiting HAL component " + args.name))
    h.exit()

