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



class emc_control:
        def __init__(self, emc):
                self.emc = emc
                self.emcstat = emc.stat()
                self.emccommand = emc.command()
                self.masked = 0;
                self.sb = 0;
                self.jog_velocity = 100.0/60.0
                self.mdi = 0

        def mask(self):
                # updating toggle button active states dumbly causes spurious events
                self.masked = 1

        def mdi_active(self, m):
                self.mdi = m

        def unmask(self):
                self.masked = 0

        def mist_on(self, b):
                if self.masked: return
                self.emccommand.mist(1)

        def mist_off(self, b):
                if self.masked: return
                self.emccommand.mist(0)

        def flood_on(self, b):
                if self.masked: return
                self.emccommand.flood(1)

        def flood_off(self, b):
                if self.masked: return
                self.emccommand.flood(0)

        def estop(self, b):
                if self.masked: return
                self.emccommand.state(self.emc.STATE_ESTOP)

        def estop_reset(self, b):
                if self.masked: return
                self.emccommand.state(self.emc.STATE_ESTOP_RESET)

        def machine_off(self, b):
                if self.masked: return
                self.emccommand.state(self.emc.STATE_OFF)

        def machine_on(self, b):
                if self.masked: return
                self.emccommand.state(self.emc.STATE_ON)

        def home_all(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.home(-1)

        def unhome_all(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.unhome(-1)

        def home_x(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.home(0)

        def home_y(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.home(1)

        def home_z(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.home(2)

        def unhome_x(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.unhome(0)

        def unhome_y(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.unhome(1)

        def unhome_z(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.unhome(2)

        def jogging(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)

        def override_limits(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.override_limits()

        def spindle_forward(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(1);

        def spindle_off(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(0);

        def spindle_reverse(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(-1);

        def spindle_faster(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(self.emc.SPINDLE_INCREASE)

        def spindle_slower(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(self.emc.SPINDLE_DECREASE)

        def continuous_jog_velocity(self, velocity):
                self.jog_velocity = velocity / 60.0
        
        def continuous_jog(self, axis, direction):
                if self.masked: return
                if direction == 0:
                        self.emccommand.jog(self.emc.JOG_STOP, axis)
                else:
                        self.emccommand.jog(self.emc.JOG_CONTINUOUS, axis, direction * self.jog_velocity)
                
	def quill_up(self):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.wait_complete()
                self.emccommand.jog(self.emc.JOG_CONTINUOUS, 2, 100)

        def feed_override(self, f):
		if self.masked: return
                self.emccommand.feedrate(f/100.0)

        def spindle_override(self, s):
                if self.masked: return
                self.emccommand.spindleoverride(s/100.0)

        def max_velocity(self, m):
                if self.masked: return
                self.emccommand.maxvel(m/60.0)

        def abort(self):
                self.emccommand.abort()

        def single_block(self, s):
                self.sb = s
                self.emcstat.poll()
                if self.emcstat.interp_state != self.emc.INTERP_IDLE:
                        # program is running
                        if s:
                                self.emccommand.auto(self.emc.AUTO_PAUSE)
                        else:
                                self.emccommand.auto(self.emc.AUTO_RESUME)

        def cycle_start(self):
                self.emcstat.poll()
                if self.emcstat.paused:
                        self.emccommand.auto(self.emc.AUTO_STEP)

                if self.emcstat.interp_state == self.emc.INTERP_IDLE:
                        self.emccommand.mode(self.emc.MODE_AUTO)
                        self.emccommand.wait_complete()
                        if self.sb:
                                self.emccommand.auto(self.emc.AUTO_STEP)
                        else:
                                self.emccommand.auto(self.emc.AUTO_RUN, 0)

class emc_status:
        def __init__(self, gtk, emc, listing, dros, error, homes,
                     unhomes, estops, machines, override_limit, status,
                     floods, mists, spindles, prefs):
                self.gtk = gtk
                self.emc = emc
                self.listing = listing
                self.dros = dros
                self.error = error
                self.homes = homes
                self.unhomes = unhomes
                self.estops = estops
                self.machines = machines
                self.override_limit = override_limit
                self.status = status
                self.floods = floods
                self.mists = mists
                self.spindles = spindles
                self.prefs = prefs
                self.mm = 0
                self.actual = 0
                self.emcstat = emc.stat()
                self.emcerror = emc.error_channel()

        def dro_inch(self, b):
                self.mm = 0

        def dro_mm(self, b):
                self.mm = 1

        def dro_commanded(self, b):
                self.actual = 0

        def dro_actual(self, b):
                self.actual = 1

        def get_current_tool(self):
                self.emcstat.poll()
                return self.emcstat.tool_in_spindle

        def periodic(self):
                last_mode = self.emcstat.task_mode
                self.emcstat.poll()

                if self.actual:
                        p = self.emcstat.actual_position
                else:
                        p = self.emcstat.position

                self.dros['xr'].set_text("X:% 9.4f" % (p[0] - self.emcstat.origin[0] - self.emcstat.tool_offset[0]))
                self.dros['yr'].set_text("Y:% 9.4f" % (p[1] - self.emcstat.origin[1]))
                self.dros['zr'].set_text("Z:% 9.4f" % (p[2] - self.emcstat.origin[2] - self.emcstat.tool_offset[2]))
                self.dros['xa'].set_text("X:% 9.4f" % p[0])
                self.dros['ya'].set_text("Y:% 9.4f" % p[1])
                self.dros['za'].set_text("Z:% 9.4f" % p[2])
                self.dros['xd'].set_text("X:% 9.4f" % self.emcstat.dtg[0])
                self.dros['yd'].set_text("Y:% 9.4f" % self.emcstat.dtg[1])
                self.dros['zd'].set_text("Z:% 9.4f" % self.emcstat.dtg[2])

                for j,name in [(0,'x'), (1,'y'), (2,'z')]:
                        self.homes[name].set_active(self.emcstat.homed[j])
                        self.unhomes[name].set_active(not self.emcstat.homed[j])

                estopped = self.emcstat.task_state == self.emc.STATE_ESTOP
                self.estops['estop'].set_active(estopped)
                self.estops['estop_reset'].set_active(not estopped)

                on = self.emcstat.task_state == self.emc.STATE_ON
                self.machines['on'].set_active(on)
                self.machines['off'].set_active(not on)              

                ovl = self.emcstat.axis[0]['override_limits']
                self.override_limit.set_active(ovl)

                self.status['file'].set_text(self.emcstat.file)
                self.status['line'].set_text("%d" % self.emcstat.current_line)
                self.status['id'].set_text("%d" % self.emcstat.id)
                self.status['dtg'].set_text("%f" % self.emcstat.distance_to_go)
                self.status['velocity'].set_text("%f" % self.emcstat.current_vel)
                self.status['delay'].set_text("%f" % self.emcstat.delay_left)

                flood = self.emcstat.flood
                self.floods['on'].set_active(flood)
                self.floods['off'].set_active(not flood)

                mist = self.emcstat.mist
                self.mists['on'].set_active(mist)
                self.mists['off'].set_active(not mist)

                spin = self.emcstat.spindle_direction
                self.spindles['forward'].set_active(spin == 1)
                self.spindles['off'].set_active(spin == 0)
                self.spindles['reverse'].set_active(spin == -1)

                ol = ""
                for i in range(len(self.emcstat.limit)):
                        if self.emcstat.limit[i]:
                                ol += "%c " % "XYZABCUVW"[i]
                self.status['onlimit'].set_text(ol)

                sd = ("CCW", "Stopped", "CW")
                self.status['spindledir'].set_text(sd[self.emcstat.spindle_direction+1])

                self.status['spindlespeed'].set_text("%d" % self.emcstat.spindle_speed)
                self.status['loadedtool'].set_text("%d" % self.emcstat.tool_in_spindle)
                self.status['preppedtool'].set_text("%d" % self.emcstat.tool_prepped)

                self.prefs['inch'].set_active(self.mm == 0)
                self.prefs['mm'].set_active(self.mm == 1)
                self.prefs['actual'].set_active(self.actual == 1)
                self.prefs['commanded'].set_active(self.actual == 0)

                if self.emcstat.id > 0: self.listing.select(self.emcstat.id or self.emcstat.motion_line)

                m = self.emcstat.task_mode
                if m != last_mode:
                        self.error.set_text("")
                last_mode = m

                e = self.emcerror.poll()
                if e:
                        kind, text = e
                        self.error.set_text(text)

                
