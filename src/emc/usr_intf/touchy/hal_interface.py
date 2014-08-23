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
    def __init__(self, gui, emc_control, mdi_control, emc):
        self.gui = gui
        self.emc_control = emc_control
        self.emc = emc
        self.emc_stat = self.emc.stat()
        self.mdi_control = mdi_control
        self.c = hal.component("touchy")
        self.c.newpin("status-indicator", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.active", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.x", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.y", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.z", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.a", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.b", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.c", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.u", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.v", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.wheel.w", hal.HAL_BIT, hal.HAL_OUT)
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

        self.c.newpin("jog.continuous.a.positive", hal.HAL_BIT, hal.HAL_IN)
        self.ap = 0
        self.c.newpin("jog.continuous.a.negative", hal.HAL_BIT, hal.HAL_IN)
        self.an = 0
        self.c.newpin("jog.continuous.b.positive", hal.HAL_BIT, hal.HAL_IN)
        self.bp = 0
        self.c.newpin("jog.continuous.b.negative", hal.HAL_BIT, hal.HAL_IN)
        self.bn = 0
        self.c.newpin("jog.continuous.c.positive", hal.HAL_BIT, hal.HAL_IN)
        self.cp = 0
        self.c.newpin("jog.continuous.c.negative", hal.HAL_BIT, hal.HAL_IN)
        self.cn = 0

        self.c.newpin("jog.continuous.u.positive", hal.HAL_BIT, hal.HAL_IN)
        self.up = 0
        self.c.newpin("jog.continuous.u.negative", hal.HAL_BIT, hal.HAL_IN)
        self.un = 0
        self.c.newpin("jog.continuous.v.positive", hal.HAL_BIT, hal.HAL_IN)
        self.vp = 0
        self.c.newpin("jog.continuous.v.negative", hal.HAL_BIT, hal.HAL_IN)
        self.vn = 0
        self.c.newpin("jog.continuous.w.positive", hal.HAL_BIT, hal.HAL_IN)
        self.wp = 0
        self.c.newpin("jog.continuous.w.negative", hal.HAL_BIT, hal.HAL_IN)
        self.wn = 0

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
        self.c["jog.wheel.a"] = n == 3 and self.active
        self.c["jog.wheel.b"] = n == 4 and self.active
        self.c["jog.wheel.c"] = n == 5 and self.active
        self.c["jog.wheel.u"] = n == 6 and self.active
        self.c["jog.wheel.v"] = n == 7 and self.active
        self.c["jog.wheel.w"] = n == 8 and self.active

    def jogincrement(self, inc, incs):
        self.c["jog.wheel.increment"] = incs[inc]

    def jogactive(self, active):
        self.active = active

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

        ap = self.c["jog.continuous.a.positive"]
        if ap ^ self.ap: self.emc_control.continuous_jog(3, ap)
        self.ap = ap

        an = self.c["jog.continuous.a.negative"]
        if an ^ self.an: self.emc_control.continuous_jog(3, -an)
        self.an = an

        bp = self.c["jog.continuous.b.positive"]
        if bp ^ self.bp: self.emc_control.continuous_jog(4, bp)
        self.bp = bp

        bn = self.c["jog.continuous.b.negative"]
        if bn ^ self.bn: self.emc_control.continuous_jog(4, -bn)
        self.bn = bn

        cp = self.c["jog.continuous.c.positive"]
        if cp ^ self.cp: self.emc_control.continuous_jog(5, cp)
        self.cp = cp

        cn = self.c["jog.continuous.c.negative"]
        if cn ^ self.cn: self.emc_control.continuous_jog(5, -cn)
        self.cn = cn

        up = self.c["jog.continuous.u.positive"]
        if up ^ self.up: self.emc_control.continuous_jog(6, up)
        self.up = up

        un = self.c["jog.continuous.u.negative"]
        if un ^ self.un: self.emc_control.continuous_jog(6, -un)
        self.un = un

        vp = self.c["jog.continuous.v.positive"]
        if vp ^ self.vp: self.emc_control.continuous_jog(7, vp)
        self.vp = vp

        vn = self.c["jog.continuous.v.negative"]
        if vn ^ self.vn: self.emc_control.continuous_jog(7, -vn)
        self.vn = vn

        wp = self.c["jog.continuous.w.positive"]
        if wp ^ self.wp: self.emc_control.continuous_jog(8, wp)
        self.wp = wp

        wn = self.c["jog.continuous.w.negative"]
        if wn ^ self.wn: self.emc_control.continuous_jog(8, -wn)
        self.wn = wn

        quillup = self.c["quill-up"]
        if quillup and not self.quillup: 
            self.emc_control.quill_up()
        self.quillup = quillup

        singleblock = self.c["single-block"]
        if singleblock ^ self.singleblock: self.emc_control.single_block(singleblock)
        self.singleblock = singleblock

        cyclestart = self.c["cycle-start"]
        if cyclestart and not self.cyclestart:
            if self.gui.wheel == "jogging": self.gui.wheel = "mv"
            self.gui.jogsettings_activate(0)
            if mdi_mode:
                if not self.singleblock: self.mdi_control.ok(0)
            else:
                self.emc_control.cycle_start()
        self.cyclestart = cyclestart

        abort = self.c["abort"]
        if abort and not self.abort: self.emc_control.abort()
        self.abort = abort

        self.emc_stat.poll()
        self.c["jog.active"] = self.emc_stat.task_mode == self.emc.MODE_MANUAL

        if self.emc_stat.paused:
            # blink
            self.c["status-indicator"] = not self.c["status-indicator"]
        else:
            if self.emc_stat.queue > 0 or self.emc_stat.interp_state != self.emc.INTERP_IDLE:
                # something is running
                self.c["status-indicator"] = 1
            else:
                # nothing is happening
                self.c["status-indicator"] = 0
