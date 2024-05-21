'''
pmx485.py

Copyright (C) 2019 - 2024 Phillip A Carter
Copyright (C) 2020 - 2024 Gregory D Carl

pmx485.py makes use of some code from hpmx.py by Pedro Grijalva Mireles

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import sys
import hal
import serial
import time

address      = '01'
regRead      = '04'
regWrite     = '06'
rCurrent     = '2094'
rCurrentMax  = '209A'
rCurrentMin  = '2099'
rFault       = '2098'
rMode        = '2093'
rPressure    = '2096'
rPressureMax = '209D'
rPressureMin = '209C'
rArcTimeLow  = '209E'
rArcTimeHigh = '209F'
validRead    = '0402'
started      = False
errorCount   = 0

# create pmx485 component
try:
    pmx485 = hal.component('pmx485')
    pmx485.newpin('mode_set', hal.HAL_FLOAT, hal.HAL_IN)      #set cutting mode
    pmx485.newpin('current_set', hal.HAL_FLOAT, hal.HAL_IN)   #set cutting current
    pmx485.newpin('pressure_set', hal.HAL_FLOAT, hal.HAL_IN)  #set gas pressure
    pmx485.newpin('enable', hal.HAL_BIT, hal.HAL_IN)          #enabler
    pmx485.newpin('mode', hal.HAL_FLOAT, hal.HAL_OUT)         #cut mode feedback
    pmx485.newpin('current', hal.HAL_FLOAT, hal.HAL_OUT)      #cutting current feedback
    pmx485.newpin('pressure', hal.HAL_FLOAT, hal.HAL_OUT)     #gas pressure feedback
    pmx485.newpin('fault', hal.HAL_FLOAT, hal.HAL_OUT)        #fault code
    pmx485.newpin('status', hal.HAL_BIT, hal.HAL_OUT)         #connection status out
    pmx485.newpin('current_min', hal.HAL_FLOAT, hal.HAL_OUT)  #minimum allowed current
    pmx485.newpin('current_max', hal.HAL_FLOAT, hal.HAL_OUT)  #maximum allowed current
    pmx485.newpin('pressure_min', hal.HAL_FLOAT, hal.HAL_OUT) #minimum allowed gas pressure
    pmx485.newpin('pressure_max', hal.HAL_FLOAT, hal.HAL_OUT) #maximum allowed gas pressure
    pmx485.newpin('arcTime', hal.HAL_FLOAT, hal.HAL_OUT)      #arc on time feedback
    pmx485.ready()
except:
    print('\nERROR: pmx485 component could not be initialized\n')
    raise(SystemExit)

enabled = pmx485.enable

# connection setup
comPort = sys.argv[1]
try:
    comms = serial.Serial(comPort,
                          baudrate = 19200,
                          bytesize = 8,
                          parity = 'E',
                          stopbits = 1,
                          timeout = 0.1
                         )
except:
    print(f'\nERROR: Could not open {comPort} for Powermax communications\n')
    raise SystemExit

# get the checksum
def get_lrc(data):
    try:
        lrc = 0
        for i in range(0, len(data), 2):
            a, b = data[i:i+2]
            try:
                lrc = (lrc + int(a + b, 16)) & 255
            except:
                return '00'
        lrc = (f'{(((lrc ^ 255) + 1) & 255):02X}').upper()
        return lrc
    except:
        return 0

# write data to register
def write_register(reg, value):
    try:
        data = f'{address}{regWrite}{reg}{value}'
        if len(data) == 12:
            lrc = get_lrc(data)
            packet = f':{data}{lrc}\r\n'
            reply = ''
            comms.write(packet.encode())
            reply = comms.readline().decode()
            if reply:
                if reply == packet:
                    return 1
        return 0
    except:
        return 0

# read data from register
def read_register(reg):
    try:
        data = f'{address}{regRead}{reg}0001'
        if len(data) == 12:
            lrc = get_lrc(data)
            packet = f':{data}{lrc}\r\n'
            reply = ''
            comms.write(packet.encode())
            reply = comms.readline().decode()
            if reply:
                if len(reply) == 15 and reply[:7] == f':{address}{validRead}':
                    lrc = get_lrc(reply[1:11])
                    if lrc == reply[11:13]:
                        return reply[7:11]
        return 0
    except:
        return 0

# set machine to local mode
def close_machine():
    write_register(rMode, f'{0:04X}')
    write_register(rCurrent, f'{0:04X}')
    write_register(rPressure, f'{0:04X}')

# set machine to remote mode
def open_machine():
    # set mode
    mode = write_register(rMode, f'{int(pmx485.mode_set):04X}')
    # set current
    current = write_register(rCurrent, f'{int(pmx485.current_set * 64.0):04X}')
    # set pressure
    pressure = write_register(rPressure, f'{int(pmx485.pressure_set * 128.0):04X}')
    if mode and current and pressure:
        return True
    else:
        return False

# get settings limits
def get_limits():
    # get minimum current setting
    cMin = read_register(rCurrentMin)
    if cMin:
        pmx485.current_min = round(int(cMin, 16) / 64.0, 1)
    # get maximum current setting
    cMax = read_register(rCurrentMax)
    if cMax:
        pmx485.current_max = round(int(cMax, 16) / 64.0, 1)
    # get minimum pressure setting
    pMin = read_register(rPressureMin)
    if pMin:
        pmx485.pressure_min = round(int(pMin, 16) / 128.0, 1)
    # get maximum pressure setting
    pMax = read_register(rPressureMax)
    if pMax:
        pmx485.pressure_max = round(int(pMax, 16) / 128.0, 1)
    if cMin and cMax and pMin and pMax:
        return True
    else:
        return False

# main loop
while hal.component_exists('motmod'):
    try:
        # only run not enabled code once, saves memory usage
        if enabled != pmx485.enable:
            enabled = pmx485.enable
            if not enabled:
                close_machine()
                comms.close()
                pmx485.status = False
                started = False
        if enabled:
            if not started:
                if not comms.isOpen():
                    comms.open()
                if open_machine():
                    started = True
                if started and get_limits():
                    started = True
                else:
                    started = False
            else:
                # set mode
                if pmx485.mode_set != pmx485.mode:
                    mode = write_register(rMode, f'{int(pmx485.mode_set):04X}')
                    if mode:
                        pmx485.mode = pmx485.mode_set
                        get_limits()
                # get mode
                else:
                    mode = read_register(rMode)
                    if mode:
                        pmx485.mode = int(mode, 16)
                # set current
                if pmx485.current_set != round(pmx485.current, 1):
                    current = write_register(rCurrent, f'{int(pmx485.current_set * 64):04X}')
                    if current:
                        pmx485.current = pmx485.current_set
                # get current
                else:
                    current = read_register(rCurrent)
                    if current:
                        pmx485.current = round(int(current, 16) / 64.0, 1)
                # set pressure
                if pmx485.pressure_set != round(pmx485.pressure, 1):
                    pressure = write_register(rPressure, f'{int(pmx485.pressure_set * 128):04X}')
                    if pressure:
                        pmx485.pressure = pmx485.pressure_set
                # get pressure
                else:
                    pressure = read_register(rPressure)
                    if pressure:
                        pmx485.pressure = round(int(pressure, 16) / 128.0, 1)
                # get fault code
                fault = read_register(rFault)
                if fault:
                    pmx485.fault = int(fault, 16)
                # get arc on time
                arcTimeLow = read_register(rArcTimeLow)
                arcTimeHigh = read_register(rArcTimeHigh)
                if arcTimeLow and arcTimeHigh:
                    pmx485.arcTime = int((arcTimeHigh + arcTimeLow), 16)
                # set status
                if mode and current and pressure and fault and arcTimeLow and arcTimeHigh:
                    pmx485.status = True
                    errorCount = 0
                else:
                    errorCount +=1
                    if errorCount > 3:
                        errorCount = 0
                        enabled = False
                        started = False
                        pmx485.status = False
                        close_machine()
                        comms.close()
                        time.sleep(0.1)
        else:
            time.sleep(0.1)
    except:
        if enabled:
            print('Unknown error in pmx485 communications')
        if started:
            if not comms.isOpen():
                comms.open()
            close_machine()
            comms.close()
