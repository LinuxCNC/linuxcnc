#!/usr/bin/python3
"""Copyright (C) 2020 Scott Alford, scottalford75@gmail.com
Modified for use with QtDragon by Jim Sloot(persei802@gmail.com)
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU 2 General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""

import sys
import os
import time
import numpy as np
try:
    from scipy.interpolate import griddata
except Exception as e:
    print(e,'Is python3-scipy installed?')
from enum import Enum, IntEnum, unique
import linuxcnc
import hal

update = 0.05   # this is how often the z external offset value is updated based on current x & y position 

@unique
class States(Enum):
    START = 1
    IDLE = 2
    LOADMAP = 3
    RUNNING = 4
    RESET = 5
    STOP = 6

@unique
class Methods(IntEnum):
    CUBIC = 0
    LINEAR = 1
    NEAREST = 2

class Compensation:
    def __init__(self):
        self.scale = 0.001
        self.filename = "probe_points.txt"

    def loadMap(self):
        # data coordinates and values
        self.data = np.loadtxt(self.filename, dtype = float, delimiter = " ", usecols = (0, 1, 2))
        self.x_data = np.around(self.data[:, 0], 1)
        self.y_data = np.around(self.data[:, 1], 1)
        self.z_data = self.data[:, 2]

        # get the x and y, min and max values from the data
        self.xMin = int(np.min(self.x_data))
        self.xMax = int(np.max(self.x_data))
        self.yMin = int(np.min(self.y_data))
        self.yMax = int(np.max(self.y_data))

        # target grid to interpolate to, 1 grid per mm
        self.xSteps = (self.xMax-self.xMin) + 1
        self.ySteps = (self.yMax-self.yMin) + 1
        self.x = np.linspace(self.xMin, self.xMax, self.xSteps)
        self.y = np.linspace(self.yMin, self.yMax, self.ySteps)
        self.xi, self.yi = np.meshgrid(self.x, self.y)

        if self.h.method == int(Methods.CUBIC):
            method = 'cubic'
        elif self.h.method == int(Methods.LINEAR):
            method = 'linear'
        elif self.h.method == int(Methods.NEAREST):
            method = 'nearest'
        else:
            method = 'cubic'
            print("ERROR: z_level_compensation: HAL pin interpolation \
method {} not recognised (outside 0-2). Defaulting to cubic(0).".format(self.h.method))
        # interpolate, zi has all the offset values but need to be transposed
        self.zi = griddata((self.x_data, self.y_data), self.z_data, (self.xi, self.yi), method = method)
        self.zi = np.transpose(self.zi)

    def compensate(self):
        x = self.h['x-pos'] - self.stat.g5x_offset[0]
        y = self.h['y-pos'] - self.stat.g5x_offset[1]
        # get nearest integer position
        self.xpos = int(round(x))
        self.ypos = int(round(y))
        
        # clamp the range
        self.xpos = self.xMin if self.xpos < self.xMin else self.xMax if self.xpos > self.xMax else self.xpos
        self.ypos = self.yMin if self.ypos < self.yMin else self.yMax if self.ypos > self.yMax else self.ypos
        
        # location in the offset map array
        self.Xn = self.xpos - self.xMin
        self.Yn = self.ypos - self.yMin
        
        # get the nearest compensation offset and convert to counts (s32) with a scale (float) 
        # Requested offset == counts * scale
        zo = self.zi[self.Xn, self.Yn]
        try:
            compensation = int(zo / self.scale)
        except:
            compensation = 0.0
        return compensation

    def run(self):
        self.h = hal.component("z_level_compensation")
        self.h.newpin("enable-in", hal.HAL_BIT, hal.HAL_IN)
        self.h.newpin("scale", hal.HAL_FLOAT, hal.HAL_OUT)
        self.h.newpin("counts", hal.HAL_S32, hal.HAL_OUT)
        self.h.newpin("clear", hal.HAL_BIT, hal.HAL_IN)
        self.h.newpin("x-pos", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.newpin("y-pos", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.newpin("z-pos", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.newpin("fade-height", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.newpin("method", hal.HAL_S32, hal.HAL_IN)
        self.h.ready()
        self.stat = linuxcnc.stat()
        
        currentState = States.START
        prevState = States.STOP
        fadeHeight = self.h["fade-height"]

        try:
            while True:
                time.sleep(update)
                
                # get linuxcnc task_state status for machine on / off transitions
                try:
                    self.stat.poll()
                except:
                    continue
                
                if currentState == States.START:
                    if currentState != prevState:
                        prevState = currentState

                    # do start-up tasks
                    prevMapTime = 0
                    self.h["counts"] = 0
                    currentState = States.IDLE
                
                elif currentState == States.IDLE:
                    if currentState != prevState:
                        prevState = currentState
                        
                    # stay in IDLE state until compensation is enabled
                    if self.h["enable-in"]:
                        currentState = States.LOADMAP
            
                elif currentState == States.LOADMAP:
                    if currentState != prevState:
                        prevState = currentState
                    mapTime = os.path.getmtime(self.filename)
                    if mapTime != prevMapTime:
                        self.loadMap()
                        prevMapTime = mapTime
                    currentState = States.RUNNING
                
                elif currentState == States.RUNNING:
                    if currentState != prevState:
                        prevState = currentState
                    if self.h["enable-in"]:
                        zPos = self.h["z-pos"]
                        if fadeHeight == 0:
                            compScale = 1
                        elif zPos < fadeHeight:
                            compScale = (fadeHeight - zPos)/fadeHeight
                            if compScale > 1:
                                compScale = 1
                        else:
                            compScale = 0
                        if self.stat.task_state == linuxcnc.STATE_ON:
                            # get the compensation if machine power is on, else set to 0
                            # otherwise we lose compensation eoffset if machine power is cycled 
                            # when compensation is enabled
                            compensation = self.compensate()
                            self.h["counts"] = compensation * compScale
                            self.h["scale"] = self.scale
                        else:
                            self.h["counts"] = 0
                    else:
                        currentState = States.RESET
                        
                elif currentState == States.RESET:
                    if currentState != prevState:
                        prevState = currentState

                    # reset the eoffsets counts register so we don't accumulate
                    self.h["counts"] = 0

                    # toggle the clear output
                    self.h["clear"] = 1;
                    time.sleep(0.1)
                    self.h["clear"] = 0;
                    currentState = States.IDLE

        except KeyboardInterrupt:
            raise SystemExit

comp = Compensation()
comp.run()
