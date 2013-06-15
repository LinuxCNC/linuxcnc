#!/usr/bin/python

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
import bisect
import glob
import sys
import time

import hal

# Fixme: Put thermistor data in an include file
Thermistor = {}
Thermistor["epcos_B57560G1104"] = [
# Temp,		Resistance,	alpha
[ -60.0,	float('inf'),	7.6 ],
[ -55.0,	10011000,	7.4 ],
[ -50.0,	6956000,	7.2 ],
[ -45.0,	4894500,	6.9 ],
[ -40.0,	3485300,	6.7 ],
[ -35.0,	2510200,	6.5 ],
[ -30.0,	1827500,	6.2 ],
[ -25.0,	1344300,	6.0 ],
[ -20.0,	998530,		5.9 ],
[ -15.0,	748670,		5.7 ],
[ -10.0,	566360,		5.5 ],
[  -5.0,	432120,		5.3 ],
[   0.0,	332400,		5.2 ],
[   5.0,	257690,		5.0 ],
[  10.0,	201270,		4.9 ],
[  15.0,	158340,		4.7 ],
[  20.0,	125420,		4.6 ],
[  25.0,	100000,		4.5 ],
[  30.0,	80239,		4.3 ],
[  35.0,	64776,		4.2 ],
[  40.0,	52598,		4.1 ],
[  45.0,	42950,		4.0 ],
[  50.0,	35262,		3.9 ],
[  55.0,	29100,		3.8 ],
[  60.0,	24136,		3.7 ],
[  65.0,	20114,		3.6 ],
[  70.0,	16841,		3.5 ],
[  75.0,	14164,		3.4 ],
[  80.0,	11963,		3.3 ],
[  85.0,	10147,		3.3 ],
[  90.0,	8640.7,		3.2 ],
[  95.0,	7386.7,		3.1 ],
[ 100.0,	6338.3,		3.0 ],
[ 105.0,	5458.4,		3.0 ],
[ 110.0,	4717,		2.9 ],
[ 115.0,	4090.1,		2.8 ],
[ 120.0,	3558.1,		2.8 ],
[ 125.0,	3105,		2.7 ],
[ 130.0,	2717.9,		2.6 ],
[ 135.0,	2386.1,		2.6 ],
[ 140.0,	2100.8,		2.5 ],
[ 145.0,	1854.8,		2.5 ],
[ 150.0,	1641.9,		2.4 ],
[ 155.0,	1457.3,		2.4 ],
[ 160.0,	1296.7,		2.3 ],
[ 165.0,	1156.6,		2.3 ],
[ 170.0,	1034.1,		2.2 ],
[ 175.0,	926.64,		2.2 ],
[ 180.0,	832.24,		2.1 ],
[ 185.0,	749.07,		2.1 ],
[ 190.0,	675.64,		2.0 ],
[ 195.0,	610.64,		2.0 ],
[ 200.0,	552.99,		2.0 ],
[ 205.0,	501.75,		1.9 ],
[ 210.0,	456.11,		1.9 ],
[ 215.0,	415.37,		1.9 ],
[ 220.0,	378.95,		1.8 ],
[ 225.0,	346.31,		1.8 ],
[ 230.0,	317.01,		1.8 ],
[ 235.0,	290.67,		1.7 ],
[ 240.0,	266.93,		1.7 ],
[ 245.0,	245.51,		1.7 ],
[ 250.0,	226.15,		1.6 ],
[ 255.0,	208.62,		1.6 ],
[ 260.0,	192.73,		1.6 ],
[ 265.0,	178.29,		1.5 ],
[ 270.0,	165.16,		1.5 ],
[ 275.0,	153.19,		1.5 ],
[ 280.0,	142.28,		1.5 ],
[ 285.0,	132.31,		1.4 ],
[ 290.0,	123.19,		1.4 ],
[ 295.0,	114.83,		1.4 ],
[ 300.0,	107.16,		1.4 ],
[ 400.0,	0.0,		1.4 ] ]


# Temperature table needs resistance to be ordered low [0] to high [n]
Thermistor["epcos_B57560G1104"].reverse()




# The BeBoPr board thermistor input has one side grounded and the other side
# pulled high through a 2.05K resistor to 3.6V.  Following this is a 470R
# resistor, some protection diodes, and a voltage divider cosisting of two
# 10.0K resistors.  The ADC voltage read is the voltage across the lower 10K
# resistor in the 470R + 10K + 10K series chain
def adc2r(V_adc):
    V_T  = 0.0  # Voltage across the thermistor (and the 470R + 10K + 10K resistor chain)
    I_PU = 0.0  # Current flowing through the 2.05K pull-up resistor
    R_TD = 0.0  # Resistance of thermistor and the 470R + 10K + 10K divider chain in parallel
    R_T  = 0.0  # Resistance of the thermistor

    V_T = V_adc * 2.0470

    # No dividing by zero or negative voltages despite what the ADC says!
    # Clip to a small positive value
    I_PU = max((3.6 - V_T ) / 2050, 0.000001)   

    R_TD = V_T / I_PU

    # Acutal resistance can't be negative, but we can get a negative value
    # from the equation below for some real ADC values, so clip to avoid
    # reporting crazy temperature values or dividing by zero
    if R_TD >= 20470 :
        R_TD = 20470 - 0.1

    # 1 / Rtotal = 1 / ( 1 / R1 + 1 / R2 )
    # R2  = ( R1 * Rtotal ) / ( R1 - Rtotal )
    R_T  = ( 20470 * R_TD ) / ( 20470 - R_TD )

    # print "V_adc: %f V_T: %f  R_TD: %f  R_T: %f" % (V_adc, V_T, R_TD, R_T)
    

    return R_T

# Convert resistance value into temperature, using thermistor table
def r2t(R_T):
    temp_slope = 0.0
    temp       = 0.0

    i = max(bisect.bisect_right(R_Key, R_T) - 1, 0)
    
    temp_slope = (thermistor[0][i] - thermistor[0][i+1]) / (thermistor[1][i] - thermistor[1][i+1])
    temp = thermistor[0][i] + ((R_T - thermistor[1][i]) * temp_slope)
    #print "Temp:", temp, "i.R_T:", i, R_T, "slope:", temp_slope, 
    #print "Deg.left:", Thermistor["epcos_B57560G1104"][i], "Deg.right:", Thermistor["epcos_B57560G1104"][i+1]
    return temp

parser = argparse.ArgumentParser(description='HAL component to read ADC values and convert to temperature')
parser.add_argument('-n','--name', help='HAL component name',required=True)
parser.add_argument('-a','--adc',  help='ADC input to read',required=True)
parser.add_argument('-t','--therm',help='Thermistor table to use', required=True)
args = parser.parse_args()

syspath = '/sys/devices/ocp.*/helper.*/'
FileName = glob.glob (syspath + args.adc)
try:
    if len(FileName) > 0:
        f = open(FileName[0], 'r')
    else:
        raise UserWarning('Bad Filename')
except (UserWarning, IOError) :
    print("Cannot read ADC input: %s" % syspath + args.adc)
    sys.exit(1)

if args.therm in Thermistor:
    # Shuffle array to make three lists of values (Temp, Resistane, Alpha)
    # so we can use bisect to efficiently do table lookups
    thermistor = map(list, zip(*Thermistor[args.therm]))
else:
    print("Unknown thermistor type: %s" % args.therm)
    print 'Try one of:', Thermistor.keys()
    sys.exit(1)

# Pull out the resistance values to use as a key for bisect
R_Key = thermistor[1]

h = hal.component(args.name)
h.newpin("raw", hal.HAL_U32, hal.HAL_OUT)
h.newpin("temp", hal.HAL_FLOAT, hal.HAL_OUT)
h.ready()
try:
    Err = 0.0
    ADC_V = 0.0
    temp = 0.0

    while 1:
    	f = open(FileName[0], 'r')
        ADC_IN = int(f.readline())
        h['raw'] = ADC_IN
        ADC_V = float(ADC_IN) / 1000.0
        temp = r2t(adc2r(ADC_V))
        h['temp'] = temp
        #print temp
        f.close()
        time.sleep(0.050)
except KeyboardInterrupt:
    raise SystemExit

