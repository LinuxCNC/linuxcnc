'''
sim-torch.py

Copyright (C) 2022 Phillip A Carter

sim-torch.py is a simple plasma torch emulator

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

import hal
import math
import random
import time

# create sim-torch component
h = hal.component('sim-torch')
# create the hal pins
h.setprefix('sim-torch')
h.newpin('cut-noise-in', hal.HAL_FLOAT, hal.HAL_IN)
h.newpin('cycles-in', hal.HAL_S32, hal.HAL_IN)
h.newpin('on-delay-in', hal.HAL_S32, hal.HAL_IN)
h.newpin('offset-in', hal.HAL_FLOAT, hal.HAL_IN)
h.newpin('overshoot-in', hal.HAL_S32, hal.HAL_IN)
h.newpin('ramp-noise-in', hal.HAL_FLOAT, hal.HAL_IN)
h.newpin('ramp-up-in', hal.HAL_S32, hal.HAL_IN)
h.newpin('start', hal.HAL_BIT, hal.HAL_IN)
h.newpin('voltage-in', hal.HAL_FLOAT, hal.HAL_IN)
h.newpin('close', hal.HAL_BIT, hal.HAL_IN)
h.newpin('voltage-out', hal.HAL_FLOAT, hal.HAL_OUT)
# setdefaults for input pins
h['cut-noise-in'] = 0.75
h['cycles-in'] = 200     #"the number of cycles that the arc voltage overshoots the cut voltage (cycles)"
h['on-delay-in']   = 10    #"the time from turn on until overshoot begins (cycles)"
h['overshoot-in']  = 50   #"the percentage of the cut voltage that the arc voltage overshoots (percent)"
h['ramp-noise-in'] = 5   #"the amount of noise during overshoot (volts)"
h['ramp-up-in']    = 80     #"percent of 'cycles-in' that the arc voltage ramps up (percent)"
h['voltage-in']    = 100    #"the cut voltage (volts)"
# component is ready
h.ready()

# working variables
angle = 0.0             # current ramp angle
current_cycle = 0       # current cycle
dn_cycles = 0           # overshoot ramp down cycles
init = False            # initialized flag
initial_ramp = 10       # initial ramp up cycles
overshoot_max = 0.0     # maximum overshoot voltage
overshoot_start = 0.0   # overshoot start voltage
up_cycles = 0           # overshoot ramp up cycles

# return a random noise voltage
def random_noise(cycle, volume):
    if cycle % 5 == 0:
        random.seed()
        v = random.randrange(int(volume * -1000), int(volume * 1000))
        return v / 1000
    else:
        return 0

# main loop
try:
    while hal.component_exists('motmod') and not h['close']:
        # validate the inputs
        cut_noise = abs(h['cut-noise-in'])
        cycles = abs(h['cycles-in'])

        if h['on-delay-in'] < 0:
            on_delay = 0
        elif h['on-delay-in'] > 100:
            on_delay = 100
        else:
            on_delay = h['on-delay-in']

        if h['offset-in'] < -10:
            offset = -10
        elif h['offset-in'] > 10:
            offset = 10
        else:
            offset = h['offset-in']

        if h['overshoot-in'] < 0:
            overshoot = 0
        elif h['overshoot-in'] > 100:
            overshoot = 100
        else:
            overshoot = h['overshoot-in']

        ramp_noise = abs(h['ramp-noise-in'])

        if h['ramp-up-in'] < 0:
            ramp_up = 0
        elif h['ramp-up-in'] > 100:
            ramp_up = 100
        else:
            ramp_up = h['ramp-up-in']

        if h['voltage-in'] < 50:
            voltage = 50
        elif h['voltage-in'] > 150:
            voltage = 150
        else:
            voltage = h['voltage-in']

        # setup some variables
        up_cycles = cycles * (ramp_up * 0.01)
        dn_cycles = cycles - up_cycles
        overshoot_max = voltage * (overshoot * 0.01)
        overshoot_start = overshoot_max * 0.4

        if h['start']:
            # start the arc
            if(current_cycle < cycles):
                if current_cycle < on_delay:
                    # initial ramp up to cut voltage
                    h['voltage-out'] = (voltage + overshoot_start) / on_delay * current_cycle
                elif current_cycle <= up_cycles:
                    # ramp up to overshoot voltage
                    angle = math.atan2(overshoot_max - overshoot_start, up_cycles)
                    h['voltage-out'] = voltage + overshoot_start + current_cycle * math.tan(angle) + random_noise(current_cycle, ramp_noise)
                else:
                    # ramp down to cut voltage
                    angle = math.atan2(overshoot_max, dn_cycles)
                    h['voltage-out'] = voltage + overshoot_max - (current_cycle - up_cycles) * math.tan(angle) + random_noise(current_cycle, ramp_noise)
            else:
            # cut voltage voltage
                h['voltage-out'] = voltage + offset + random_noise(current_cycle, cut_noise)
            current_cycle += 1
        else:
            # stop the arc
            current_cycle = 0
            h['voltage-out'] = 0
        time.sleep(.001)
except KeyboardInterrupt:
    pass
except Exception as e:
    print(e)
