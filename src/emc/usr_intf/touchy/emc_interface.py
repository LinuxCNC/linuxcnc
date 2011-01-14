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


import math

from __main__ import set_active, set_text

class emc_control:
        def __init__(self, emc, listing, error):
                self.emc = emc
                self.emcstat = emc.stat()
                self.emccommand = emc.command()
                self.masked = 0;
                self.sb = 0;
                self.jog_velocity = 100.0/60.0
                self.mdi = 0
                self.listing = listing
                self.error = error
                self.isjogging = [0,0,0,0,0,0,0,0,0]

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

        def home_selected(self, axis):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.home(axis)

        def unhome_selected(self, axis):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.unhome(axis)

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
                for i in range(9):
                        if self.isjogging[i]:
                                self.emccommand.jog(self.emc.JOG_CONTINUOUS, i, self.isjogging[i] * self.jog_velocity)
        
        def continuous_jog(self, axis, direction):
                if self.masked: return
                if direction == 0:
                        self.isjogging[axis] = 0
                        self.emccommand.jog(self.emc.JOG_STOP, axis)
                else:
                        self.isjogging[axis] = direction
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

        def reload_tooltable(self, b):
                if self.masked: return
                self.emccommand.load_tool_table()                

        def opstop_on(self, b):
                if self.masked: return
                self.emccommand.set_optional_stop(1)

        def opstop_off(self, b):
                if self.masked: return
                self.emccommand.set_optional_stop(0)

        def blockdel_on(self, b):
                if self.masked: return
                self.emccommand.set_block_delete(1)

        def blockdel_off(self, b):
                if self.masked: return
                self.emccommand.set_block_delete(0)

        def abort(self):
                self.emccommand.abort()
                set_text(self.error, "")

        def single_block(self, s):
                self.sb = s
                self.emcstat.poll()
                if self.emcstat.queue > 0 or self.emcstat.paused:
                        # program or mdi is running
                        if s:
                                self.emccommand.auto(self.emc.AUTO_PAUSE)
                        else:
                                self.emccommand.auto(self.emc.AUTO_RESUME)

        def cycle_start(self):
                self.emcstat.poll()
                if self.emcstat.paused:
                        if self.sb:
                                self.emccommand.auto(self.emc.AUTO_STEP)
                        else:
                                self.emccommand.auto(self.emc.AUTO_RESUME)
                        return

                if self.emcstat.interp_state == self.emc.INTERP_IDLE:
                        self.emccommand.mode(self.emc.MODE_AUTO)
                        self.emccommand.wait_complete()
                        if self.sb:
                                self.emccommand.auto(self.emc.AUTO_STEP)
                        else:
                                self.emccommand.auto(self.emc.AUTO_RUN, self.listing.get_startline())
                                self.listing.clear_startline()

class emc_status:
        def __init__(self, gtk, emc, listing, relative, absolute, distance,
                     dro_table,
                     error,
                     estops, machines, override_limit, status,
                     floods, mists, spindles, prefs, opstop, blockdel):
                self.gtk = gtk
                self.emc = emc
                self.listing = listing
                self.relative = relative
                self.absolute = absolute
                self.distance = distance
                self.dro_table = dro_table
                self.error = error
                self.estops = estops
                self.machines = machines
                self.override_limit = override_limit
                self.status = status
                self.floods = floods
                self.mists = mists
                self.spindles = spindles
                self.prefs = prefs
                self.opstop = opstop
                self.blockdel = blockdel
                self.resized_dro = 0
                
                self.mm = 0
                self.machine_units_mm=0
                self.unit_convert=[1]*9
                self.actual = 0
                self.emcstat = emc.stat()
                self.emcerror = emc.error_channel()

        def dro_inch(self, b):
                self.mm = 0

        def dro_mm(self, b):
                self.mm = 1

        def set_machine_units(self,u,c):
                self.machine_units_mm = u
                self.unit_convert = c

        def convert_units(self,v,c):
                return map(lambda x,y: x*y, v, c)

        def dro_commanded(self, b):
                self.actual = 0

        def dro_actual(self, b):
                self.actual = 1

        def get_current_tool(self):
                self.emcstat.poll()
                return self.emcstat.tool_in_spindle

        def get_current_system(self):
                self.emcstat.poll()
                g = self.emcstat.gcodes
                for i in g:
                        if i >= 540 and i <= 590:
                                return i/10 - 53
                        elif i >= 590 and i <= 593:
                                return i - 584
                return 1

        def periodic(self):
                self.emcstat.poll()
                am = self.emcstat.axis_mask
                lathe = not (self.emcstat.axis_mask & 2)
                dtg = self.emcstat.dtg

                if not self.resized_dro:
                        height = 9
                        for i in range(9):
                                if i == 1 and lathe:
                                        continue
                                if not (am & (1<<i)):
                                        height -= 1
                                        self.dro_table.remove(self.relative[height])
                                        self.dro_table.remove(self.absolute[height])
                                        self.dro_table.remove(self.distance[height])
                                        
                        self.dro_table.resize(height, 3)
                        self.resized_dro = 1
                                        
                if self.actual:
                        p = self.emcstat.actual_position
                else:
                        p = self.emcstat.position

                x = p[0] - self.emcstat.g5x_offset[0] - self.emcstat.tool_offset[0]
                y = p[1] - self.emcstat.g5x_offset[1] - self.emcstat.tool_offset[1]
                z = p[2] - self.emcstat.g5x_offset[2] - self.emcstat.tool_offset[2]
                a = p[3] - self.emcstat.g5x_offset[3] - self.emcstat.tool_offset[3]
                b = p[4] - self.emcstat.g5x_offset[4] - self.emcstat.tool_offset[4]
                c = p[5] - self.emcstat.g5x_offset[5] - self.emcstat.tool_offset[5]
                u = p[6] - self.emcstat.g5x_offset[6] - self.emcstat.tool_offset[6]
                v = p[7] - self.emcstat.g5x_offset[7] - self.emcstat.tool_offset[7]
                w = p[8] - self.emcstat.g5x_offset[8] - self.emcstat.tool_offset[8]

                if self.emcstat.rotation_xy != 0:
                        t = math.radians(-self.emcstat.rotation_xy)
                        xr = x * math.cos(t) - y * math.sin(t)
                        yr = x * math.sin(t) + y * math.cos(t)
                        x = xr
                        y = yr

                x -= self.emcstat.g92_offset[0] 
                y -= self.emcstat.g92_offset[1] 
                z -= self.emcstat.g92_offset[2] 
                a -= self.emcstat.g92_offset[3] 
                b -= self.emcstat.g92_offset[4] 
                c -= self.emcstat.g92_offset[5] 
                u -= self.emcstat.g92_offset[6] 
                v -= self.emcstat.g92_offset[7] 
                w -= self.emcstat.g92_offset[8] 

                relp = [x, y, z, a, b, c, u, v, w]

                if self.mm != self.machine_units_mm:
                        p = self.convert_units(p,self.unit_convert)
                        relp = self.convert_units(relp,self.unit_convert)
                        dtg = self.convert_units(dtg,self.unit_convert)

                if self.mm:
                        fmt = "%c:% 10.3f"
                else:
                        fmt = "%c:% 9.4f"

                d = 0
                if (am & 1):
                        h = " "
                        if self.emcstat.homed[0]: h = "*"
                        
                        if lathe:
                                set_text(self.relative[d], fmt % ('R', relp[0]))
                                set_text(self.absolute[d], h + fmt % ('R', p[0]))
                                set_text(self.distance[d], fmt % ('R', dtg[0]))
                                d += 1
                                set_text(self.relative[d], fmt % ('D', relp[0] * 2.0))
                                set_text(self.absolute[d], " " + fmt % ('D', p[0] * 2.0))
                                set_text(self.distance[d], fmt % ('D', dtg[0] * 2.0))
                        else:
                                set_text(self.relative[d], fmt % ('X', relp[0]))
                                set_text(self.absolute[d], h + fmt % ('X', p[0]))
                                set_text(self.distance[d], fmt % ('X', dtg[0]))

                        d += 1
                        
                for i in range(1, 9):
                        h = " "
                        if self.emcstat.homed[i]: h = "*"
                        if am & (1<<i):
                                letter = 'XYZABCUVW'[i]
                                set_text(self.relative[d], fmt % (letter, relp[i]))
                                set_text(self.absolute[d], h + fmt % (letter, p[i]))
                                set_text(self.distance[d], fmt % (letter, dtg[i]))
                                d += 1

                estopped = self.emcstat.task_state == self.emc.STATE_ESTOP
                set_active(self.estops['estop'], estopped)
                set_active(self.estops['estop_reset'], not estopped)

                on = self.emcstat.task_state == self.emc.STATE_ON
                set_active(self.machines['on'], on)
                set_active(self.machines['off'], not on)

                ovl = self.emcstat.axis[0]['override_limits']
                set_active(self.override_limit, ovl)

                set_text(self.status['file'], self.emcstat.file)
                set_text(self.status['file_lines'], "%d" % len(self.listing.program))
                set_text(self.status['line'], "%d" % self.emcstat.current_line)
                set_text(self.status['id'], "%d" % self.emcstat.id)
                set_text(self.status['dtg'], "%.4f" % self.emcstat.distance_to_go)
                set_text(self.status['velocity'], "%.4f" % (self.emcstat.current_vel * 60.0))
                set_text(self.status['delay'], "%.2f" % self.emcstat.delay_left)

                flood = self.emcstat.flood
                set_active(self.floods['on'], flood)
                set_active(self.floods['off'], not flood)

                mist = self.emcstat.mist
                set_active(self.mists['on'], mist)
                set_active(self.mists['off'], not mist)

                spin = self.emcstat.spindle_direction
                set_active(self.spindles['forward'], spin == 1)
                set_active(self.spindles['off'], spin == 0)
                set_active(self.spindles['reverse'], spin == -1)

                ol = ""
                for i in range(len(self.emcstat.limit)):
                        if self.emcstat.limit[i]:
                                ol += "%c " % "XYZABCUVW"[i]
                set_text(self.status['onlimit'], ol)

                sd = (_("CCW"), _("Stopped"), _("CW"))
                set_text(self.status['spindledir'], sd[self.emcstat.spindle_direction+1])

                set_text(self.status['spindlespeed'], "%d" % self.emcstat.spindle_speed)
                set_text(self.status['spindlespeed2'], "%d" % self.emcstat.spindle_speed)
                set_text(self.status['loadedtool'], "%d" % self.emcstat.tool_in_spindle)
		if self.emcstat.pocket_prepped == -1:
			set_text(self.status['preppedtool'], _("None"))
		else:
			set_text(self.status['preppedtool'], "%d" % self.emcstat.tool_table[self.emcstat.pocket_prepped].id)

                tt = ""
                for p, t in zip(range(len(self.emcstat.tool_table)), self.emcstat.tool_table):
                        if t.id != -1:
                                tt += "<b>P%02d:</b>T%02d\t" % (p, t.id)
                                if p == 0: tt += '\n'
                set_text(self.status['tooltable'], tt)
                        
                
                set_text(self.status['xyrotation'], "%d" % self.emcstat.rotation_xy)
                set_text(self.status['tlo'], "%.4f" % self.emcstat.tool_offset[2])

                cs = self.emcstat.g5x_index
                if cs<7:
                        cslabel = "G5%d" % (cs+3)
                else:
                        cslabel = "G59.%d" % (cs-6)
                        
                set_text(self.status['label_g5xoffset'], '<b>' + cslabel + '</b>' + ' Offset:');

                g5x = ""
                g92 = ""
                for i in range(len(self.emcstat.g5x_offset)):
                        letter = "XYZABCUVW"[i]
                        if self.emcstat.g5x_offset[i] != 0: g5x += "%s%.4f " % (letter, self.emcstat.g5x_offset[i])
                        if self.emcstat.g92_offset[i] != 0: g92 += "%s%.4f " % (letter, self.emcstat.g92_offset[i])
                                   
                set_text(self.status['g5xoffset'], g5x);
                set_text(self.status['g92offset'], g92);

                active_codes = []
                for i in self.emcstat.gcodes[1:]:
                        if i == -1: continue
                        if i % 10 == 0:
                                active_codes.append("G%d" % (i/10))
                        else:
                                active_codes.append("G%d.%d" % (i/10, i%10))

                for i in self.emcstat.mcodes[1:]:
                        if i == -1: continue
                        active_codes.append("M%d" % i)

                feed_str = "F%.1f" % self.emcstat.settings[1]
                if feed_str.endswith(".0"): feed_str = feed_str[:-2]
                active_codes.append(feed_str)
                active_codes.append("S%.0f" % self.emcstat.settings[2])

                set_text(self.status['activecodes'], " ".join(active_codes))

                set_active(self.prefs['inch'], self.mm == 0)
                set_active(self.prefs['mm'], self.mm == 1)
                set_active(self.prefs['actual'], self.actual == 1)
                set_active(self.prefs['commanded'], self.actual == 0)

                set_active(self.opstop['on'], self.emcstat.optional_stop)
                set_active(self.opstop['off'], not self.emcstat.optional_stop)
                
                set_active(self.blockdel['on'], self.emcstat.block_delete)
                set_active(self.blockdel['off'], not self.emcstat.block_delete)
                

                if self.emcstat.id == 0 and (self.emcstat.interp_state == self.emc.INTERP_PAUSED or self.emcstat.exec_state == self.emc.EXEC_WAITING_FOR_DELAY):
                        self.listing.highlight_line(self.emcstat.current_line)
                elif self.emcstat.id == 0:
                        self.listing.highlight_line(self.emcstat.motion_line)
                else:
                        self.listing.highlight_line(self.emcstat.id or self.emcstat.motion_line)

                e = self.emcerror.poll()
                if e:
                        kind, text = e
                        set_text(self.error, text)

                
