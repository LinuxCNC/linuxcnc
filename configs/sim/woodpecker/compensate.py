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
import linuxcnc
import hal
from PyQt5 import QtGui, QtWidgets
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.figure import Figure
from mpl_toolkits.mplot3d import Axes3D
from scipy.interpolate import griddata
from enum import Enum, unique

update = 0.05   # this is how often the z external offset value is updated based on current x & y position 

@unique
class States(Enum):
    START = 1
    IDLE = 2
    LOADMAP = 3
    CREATEPLOT = 4
    RUNNING = 5
    RESET = 6
    STOP = 7


class Compensation():
    def __init__(self):
        super(Compensation, self).__init__()
        self.data = None
        self.x_data = None
        self.y_data = None
        self.z_data = None
        self.method = None
        self.scale = 0.001
        self.file_valid = True
        self.filename = "probe_points.txt"
        if len(sys.argv) < 2:
            print("ERROR: No interpolation method specified. Defaulting to cubic.")
        elif sys.argv[1] in ("nearest", "linear", "cubic"):
            self.method = sys.argv[1]

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
        # interpolate, zi has all the offset values but need to be transposed
        self.z1 = griddata((self.x_data, self.y_data), self.z_data, (self.xi, self.yi), method = self.method)
        self.zi = np.transpose(self.z1)

    def create_plot(self):
        fig = plt.figure(figsize=(6,5))
#        print(plt.rcParams.keys())
        plt.rcParams.update({'text.color': '#e0e0e0', 'axes.labelcolor': '#e0e0e0', 'axes.edgecolor': '#e0e0e0'})
        plt.rcParams.update({'xtick.color': '#e0e0e0', 'ytick.color': '#e0e0e0'})
        ax = fig.add_subplot(111, projection='3d')
        ax.plot_surface(self.xi, self.yi, self.z1, cmap=cm.viridis)
        ax.set_title('Compensation Height Map')
        ax.set_xlabel('X Axis')
        ax.set_ylabel('Y Axis')
        ax.set_zlabel('Z Axis')
        plt.savefig("height_map.png", transparent=True)

    def compensate(self):
        if not self.file_valid: return 0
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
        compensation = int(zo / self.scale)
        return compensation

    def run(self):
        self.h = hal.component("compensate")
        self.h.newpin("enable-in", hal.HAL_BIT, hal.HAL_IN)
        self.h.newpin("map-ready", hal.HAL_BIT, hal.HAL_OUT)
        self.h.newpin("scale", hal.HAL_FLOAT, hal.HAL_OUT)
        self.h.newpin("counts", hal.HAL_S32, hal.HAL_OUT)
        self.h.newpin("clear", hal.HAL_BIT, hal.HAL_IN)
        self.h.newpin("x-pos", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.newpin("y-pos", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.newpin("z-pos", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.newpin("fade-height", hal.HAL_FLOAT, hal.HAL_IN)
        self.h.ready()
        self.stat = linuxcnc.stat()
        
        currentState = States.START
        prevState = States.STOP
        fadeHeight = self.h["fade-height"]

        try:
            while True:
                time.sleep(update)

                # get linuxcnc task_state status for machine on / off transitions
                self.stat.poll()

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
                        prevMapTime = mapTime
                        try:
                            self.loadMap()
                            currentState = States.CREATEPLOT
                        except Exception as err:
                            print("Loadmap error: ", err)
                            self.file_valid = False
                            current_state = States.RUNNING
                    else:
                        currentState = States.RUNNING

                elif currentState == States.CREATEPLOT:
                    if currentState != prevState:
                        prevState = currentState
                    self.create_plot()
                    self.h['map-ready'] = True
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
                    self.h['map-ready'] = False
                    self.file_valid = True
                    # toggle the clear output
                    self.h["clear"] = 1;
                    time.sleep(0.1)
                    self.h["clear"] = 0;
                    currentState = States.IDLE

        except KeyboardInterrupt:
            raise SystemExit

comp = Compensation()
comp.run()
