# Touchy is Copyright (c) 2009  Chris Radek <chris@timeguy.com>
#
# Touchy is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Touchy is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.



import hal

class hal_interface:
    def __init__(self, gui, emc_control, mdi_control):
        self.gui = gui
        self.emc_control = emc_control
        self.mdi_control = mdi_control
        self.c = hal.component("touchy")
        self.c.newpin("jog.active", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.x", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.y", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.z", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.increment", hal.HAL_FLOAT, hal.HAL_OUT)
        self.c.newpin("jog.continuous.x.positive", hal.HAL_BIT, hal.HAL_IN)
        self.xp = 0
        self.c.newpin("jog.continuous.x.negative", hal.HAL_BIT, hal.HAL_IN)
        self.xn = 0
        self.c.newpin("jog.continuous.y.positive", hal.HAL_BIT, hal.HAL_IN)
        self.yp = 0
        self.c.newpin("jog.continuous.y.negative", hal.HAL_BIT, hal.HAL_IN)
        self.yn = 0
        self.c.newpin("jog.continuous.z.positive", hal.HAL_BIT, hal.HAL_IN)
        self.zp = 0
        self.c.newpin("jog.continuous.z.negative", hal.HAL_BIT, hal.HAL_IN)
        self.zn = 0
        self.c.newpin("quill-up", hal.HAL_BIT, hal.HAL_IN)
        self.quillup = 0
        self.c.newpin("cycle-start", hal.HAL_BIT, hal.HAL_IN)
        self.cyclestart = 0
        self.c.newpin("abort", hal.HAL_BIT, hal.HAL_IN)
        self.abort = 0
        self.c.newpin("single-block", hal.HAL_BIT, hal.HAL_IN)
        self.singleblock = 0
        self.c.newpin("wheel-counts", hal.HAL_S32, hal.HAL_IN)
        self.counts = 0
        self.jog_velocity = 1
        self.c.ready()
        self.active = 0
        self.jogaxis(0)

    def wheel(self):
        counts = self.c["wheel-counts"]/4
        ret = counts - self.counts
        self.counts = counts
        return ret

    def jogaxis(self, n):
        self.c["jog.wheel.x"] = n == 0 and self.active
        self.c["jog.wheel.y"] = n == 1 and self.active
        self.c["jog.wheel.z"] = n == 2 and self.active

    def jogincrement(self, inc):
        incs = [0.01, 0.001, 0.0001]
        self.c["jog.wheel.increment"] = incs[inc]

    def jogactive(self, active):
        self.active = active
        self.c["jog.active"] = active;

    def periodic(self, mdi_mode):
        # edge detection
        xp = self.c["jog.continuous.x.positive"]
        if xp ^ self.xp: self.emc_control.continuous_jog(0, xp)
        self.xp = xp

        xn = self.c["jog.continuous.x.negative"]
        if xn ^ self.xn: self.emc_control.continuous_jog(0, -xn)
        self.xn = xn

        yp = self.c["jog.continuous.y.positive"]
        if yp ^ self.yp: self.emc_control.continuous_jog(1, yp)
        self.yp = yp

        yn = self.c["jog.continuous.y.negative"]
        if yn ^ self.yn: self.emc_control.continuous_jog(1, -yn)
        self.yn = yn

        zp = self.c["jog.continuous.z.positive"]
        if zp ^ self.zp: self.emc_control.continuous_jog(2, zp)
        self.zp = zp

        zn = self.c["jog.continuous.z.negative"]
        if zn ^ self.zn: self.emc_control.continuous_jog(2, -zn)
        self.zn = zn

        quillup = self.c["quill-up"]
        if quillup and not self.quillup: 
            self.emc_control.quill_up()
        self.quillup = quillup

        cyclestart = self.c["cycle-start"]
        if cyclestart and not self.cyclestart:
            if self.gui.wheel == "jogging": self.gui.wheel = "mv"
            self.gui.jogsettings_activate(0)
            if mdi_mode:
                self.mdi_control.ok(0)
            else:
                self.emc_control.cycle_start()
        self.cyclestart = cyclestart

        abort = self.c["abort"]
        if abort and not self.abort: self.emc_control.abort()
        self.abort = abort

        singleblock = self.c["single-block"]
        if singleblock ^ self.singleblock: self.emc_control.single_block(singleblock)
        self.singleblock = singleblock

        
