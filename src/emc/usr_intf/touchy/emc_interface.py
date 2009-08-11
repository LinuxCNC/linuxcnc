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
                self.emccommand = emc.command()
                self.masked = 0;

        def mask(self):
                # updating toggle button active states dumbly causes spurious events
                self.masked = 1

        def unmask(self):
                self.masked = 0

        def mist_on(self, b):
                if self.masked: return
                self.emccommand.mist(1)

        def mist_off(self, b):
                if self.masked: return
                self.emccommand.mist(0)

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

class emc_status:
        def __init__(self, gtk, emc, dros, error, homes,
                     unhomes, estops, machines, override_limit, status):
                self.gtk = gtk
                self.dros = dros
                self.error = error
                self.homes = homes
                self.unhomes = unhomes
                self.estops = estops
                self.machines = machines
                self.override_limit = override_limit
                self.status = status
                self.emc = emc
                self.emcstat = emc.stat()
                self.emcerror = emc.error_channel()
                print dir(self.emcstat)
                print self.emcstat.limit

        def periodic(self):
                last_mode = self.emcstat.task_mode
                self.emcstat.poll()
                # XXX tlo?
                self.dros['xr'].set_text("X:% 9.4f" % (self.emcstat.actual_position[0] - self.emcstat.origin[0] - self.emcstat.tool_offset[0]))
                self.dros['yr'].set_text("Y:% 9.4f" % (self.emcstat.actual_position[1] - self.emcstat.origin[1]))
                self.dros['zr'].set_text("Z:% 9.4f" % (self.emcstat.actual_position[2] - self.emcstat.origin[2] - self.emcstat.tool_offset[2]))
                self.dros['xa'].set_text("X:% 9.4f" % self.emcstat.actual_position[0])
                self.dros['ya'].set_text("Y:% 9.4f" % self.emcstat.actual_position[1])
                self.dros['za'].set_text("Z:% 9.4f" % self.emcstat.actual_position[2])
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
                self.status['dtg'].set_text("%f" % self.emcstat.distance_to_go)
                self.status['velocity'].set_text("%f" % self.emcstat.current_vel)
                self.status['delay'].set_text("%f" % self.emcstat.delay_left)

                ol = ""
                for i in range(len(self.emcstat.limit)):
                        if self.emcstat.limit[i]:
                                ol += "%c " % "XYZABCUVW"[i]
                self.status['onlimit'].set_text(ol)

                sd = ("CCW", "Stopped", "CW")
                self.status['spindledir'].set_text(sd[self.emcstat.spindle_direction+1])

                self.status['spindlespeed'].set_text("%d" % self.emcstat.spindle_speed)
                self.status['loadedtool'].set_text("%d" % self.emcstat.tool_in_spindle)

                m = self.emcstat.task_mode
                if m != last_mode:
                        self.error.set_text("")
                last_mode = m

                e = self.emcerror.poll()
                if e:
                        kind, text = e
                        self.error.set_text(text)

                
