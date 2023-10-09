#!/usr/bin/python3
#    This is a userspace program that interfaces Huanyang GT-series VFDs
#    to the LinuxCNC HAL.

#    Copyright (C) 2022 Jim Sloot <persei802@gmail.com>
#    Created from the original hy_gt_vfd.c by Sebastian Kuzminsky
#    This program is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Lesser General Public
#    License as published by the Free Software Foundation, version 2.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    General Public License for more details.

#    You should have received a copy of the GNU Lesser General Public
#    License along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301-1307 USA.

import sys
import hal, time
import argparse
from qtvcp import logger
from pymodbus.client import ModbusSerialClient

LOG = logger.getLogger(__name__)
LOG.setLevel(logger.DEBUG)

# state machine states
INIT = 1
RUNNING = 2
ERROR = 3

# Any options not specified in the command line will use the default values listed below.
device = "/dev/ttyUSB0"
byte_size = 8
baud_rate = 38400
parity = "N"
stop_bits = 1
slave = 1
max_speed = 24000
min_speed = 7200
period = 0.25 # seconds to sleep before each cycle
motor_is_on = False
baud_values = ["1200", "2400", "4800", "9600", "19200", "38400"]
parity_values = ["E", "O", "N"]
stop_values = ["1", "2"]
byte_values = ["5", "6", "7", "8"]

h = hal.component("hy_gt_vfd")
parser = argparse.ArgumentParser()

# Parse command line options
def parse_args():
    global device, baud_rate, parity, stop_bits, byte_size, slave, max_speed, min_speed
    parser.add_argument("-d", "--device", help="serial device")
    parser.add_argument("-b", "--bits", help="number of bits")
    parser.add_argument("-r", "--rate", help="baudrate")
    parser.add_argument("-p", "--parity", help="parity")
    parser.add_argument("-s", "--stopbits", help="stop bits")
    parser.add_argument("-t", "--slave", help="modbus slave number")
    parser.add_argument("-M", "--maxrpm", help="max motor speed in RPM")
    parser.add_argument("-m", "--minrpm", help="min motor speed in RPM")
    args = parser.parse_args()
    if args.device:
        device = args.device
    if args.bits:
        if args.bits in byte_values:
            byte_size = int(args.bits)
        else:
            print("Invalid byte size - using default of {}".format(byte_size))
            print("Must be one of ", byte_values)
    if args.rate:
        if args.rate in baud_values:
            baud_rate = int(args.rate)
        else:
            print("Invalid baud rate - using default of {}".format(baud_rate))
            print("Must be one of ", baud_values)
    if args.parity:
        if args.parity in parity_values:
            parity = args.parity
        else:
            print("Invalid parity setting - using default of {}".format(parity))
            print("Must be one of ", parity_values)
    if args.stopbits:
        if args.stopbits in stop_values:
            stop_bits = int(args.stopbits)
        else:
            print("Invalid stop bits - using default of {}".format(stop_bits))
            print("Must be one of ", stop_values)
    if args.slave:
        if 1 <= int(args.slave) <= 127:
            slave = int(args.slave)
        else:
            print("Slave address must be between 1 and 127")
    if args.maxrpm:
        if float(args.maxrpm) > min_speed:
            max_speed = float(args.maxrpm)
        else:
            print("Max RPM must be greater than Min RPM")
    if args.minrpm:
        if float(args.minrpm) < max_speed:
            min_speed = float(args.minrpm)
        else:
            print("Min RPM must be less than Max RPM")

# Initialize the serial port
def init_serial():
    global vfd
    params = {'port': device,
              'baudrate': baud_rate,
              'parity': parity,
              'stopbits': stop_bits,
              'bytesize': byte_size,
              'timeout': 1}
    vfd = ModbusSerialClient(method='rtu', **params)
    if vfd.connect(): return True
    return False
 
# Create HAL pins
def init_pins():
    h.newpin('speed-cmd', hal.HAL_FLOAT, hal.HAL_IN)
    h.newpin('speed-fb', hal.HAL_FLOAT, hal.HAL_OUT)
    h.newpin('speed-rpm', hal.HAL_FLOAT, hal.HAL_OUT)
    h.newpin('spindle-on', hal.HAL_BIT, hal.HAL_IN)
    h.newpin('output-current', hal.HAL_FLOAT, hal.HAL_OUT)
    h.newpin('output-voltage', hal.HAL_FLOAT, hal.HAL_OUT)
#    h.newpin('output-power', hal.HAL_FLOAT, hal.HAL_OUT)
    h.newpin('fault-info', hal.HAL_U32, hal.HAL_OUT)
    h.newpin('modbus-errors', hal.HAL_U32, hal.HAL_OUT)
    h.newpin('vfd-ok', hal.HAL_BIT, hal.HAL_OUT)
    h['modbus-errors'] = 0
    h['vfd-ok'] = False
    h.ready()

# Turn spindle motor on or off
def set_motor_on(state):
    req = vfd.write_register(0x1000, state, slave = slave)

# Set spindle speed as percentage of maximum speed
def set_motor_speed():
    global last_speed
    speed = h['speed-cmd']
    if speed == last_speed: return
    last_speed = speed
    if speed > max_speed:
        speed_cmd = 10000
    elif speed < min_speed:
        speed_cmd = int((min_speed / max_speed) * 10000)
    else:
        speed_cmd = int((speed / max_speed) * 10000)
    req = vfd.write_register(0x2000, speed_cmd, slave = slave)
    
# read VFD registers
def get_vfd_data():
    global currentState
    read_ok = True
    try:
        time.sleep(delay)
        data = vfd.read_holding_registers(address = 0x3003, count = 3, slave = slave)
        if not data.isError():
            h['output-voltage'] = data.registers[0]
            h['output-current'] = data.registers[1]
            h['speed-rpm'] = data.registers[2]
            h['speed-fb'] = data.registers[2] / 60
        else:
            h['modbus-errors'] += 1
            read_ok = False
        time.sleep(delay)
        fault = vfd.read_holding_registers(address = 0x5000, count = 1, slave = slave)
        if not fault.isError():
            h['fault-info'] = fault.registers[0]
        else:
            h['modbus-errors'] += 1
            read_ok = False
    except Exception as e:
        LOG.debug(f"Exception: {e}")
        currentState = ERROR
    return read_ok

## start
last_speed = 0
currentState = INIT
prevState = None
delay = 80 / baud_rate
parse_args()
init_pins()

try:
    while True:
        time.sleep(period)
        if currentState == INIT:
            if currentState != prevState:
                LOG.info("State : VFD INIT")
                prevState = currentState
            if init_serial():
                LOG.info(f"Connected to {device}")
                LOG.debug(f"VFD object : {vfd}")
                h['vfd-ok'] = True
                h['modbus-errors'] = 0
                currentState = RUNNING
            else:
                LOG.debug(f"Could not connect to {device}")
                time.sleep(1)

        elif currentState == RUNNING:
            if currentState != prevState:
                LOG.info("State : VFD RUNNING")
                prevState = currentState
            if get_vfd_data():
                if h['spindle-on'] is True:
                    set_motor_speed()
                    if motor_is_on is False:
                        motor_is_on = True
                        set_motor_on(1)
                elif motor_is_on is True:
                    motor_is_on = False
                    set_motor_on(5)
            else:
                LOG.debug("ERROR reading VFD registers")

        elif currentState == ERROR:
            LOG.info("State : VFD ERROR")
            h['vfd-ok'] = False
            del vfd
            prevState = currentState
            currentState = INIT

except KeyboardInterrupt:
    raise SystemExit

