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


class emc_control:
        def __init__(self, emc):
                self.emc = emc
                self.emcstat = emc.stat()
                self.emccommand = emc.command()
                self.masked = 0;
                self.sb = 0;
                self.jog_velocity = 100.0/60.0
                self.angular_jog_velocity = 3600/60
                self.mdi = 0
                self.isjogging = [0,0,0,0,0,0,0,0,0]
                self.restart_line_number = self.restart_reset_line = 0
                self.emccommand.teleop_enable(1)
                self.emccommand.wait_complete()
                self.emcstat.poll()
                if self.emcstat.kinematics_type != emc.KINEMATICS_IDENTITY:
                    raise SystemExit, "\n*** emc_control: Only KINEMATICS_IDENTITY is supported\n"

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
                self.emccommand.teleop_enable(False)
                self.emccommand.home(-1)

        def unhome_all(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.teleop_enable(False)
                self.emccommand.unhome(-1)

        def home_selected(self, axis):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.teleop_enable(False)
                self.emccommand.home(axis)

        def unhome_selected(self, axis):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.teleop_enable(False)
                self.emccommand.unhome(axis)

        def jogging(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)

        def override_limits(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.override_limits()

        def spindle_forward(self, b, rpm=100):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(1,rpm,0);

        def spindle_off(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(0);

        def spindle_reverse(self, b, rpm=100):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(-1,rpm,0);

        def spindle_faster(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(self.emc.SPINDLE_INCREASE)

        def spindle_slower(self, b):
                if self.masked: return
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.spindle(self.emc.SPINDLE_DECREASE)

        def set_motion_mode(self):
            self.emcstat.poll()
            if self.emcstat.motion_mode != self.emc.TRAJ_MODE_TELEOP:
                self.emccommand.teleop_enable(1)
                self.emccommand.wait_complete()

        def continuous_jog_velocity(self, velocity,angular=None):
                if self.masked: return
                self.set_motion_mode()
                if velocity == None:
                    rate = self.angular_jog_velocity = angular / 60.0
                else:
                    rate = self.jog_velocity = velocity / 60.0
                for i in range(9):
                        if self.isjogging[i]:
                                self.emccommand.jog(self.emc.JOG_CONTINUOUS
                                ,0 ,i ,self.isjogging[i] * rate)
        
        def continuous_jog(self, axis, direction):
                if self.masked: return
                self.set_motion_mode()
                if direction == 0:
                        self.isjogging[axis] = 0
                        self.emccommand.jog(self.emc.JOG_STOP, 0, axis)
                else:
                    if axis in (3,4,5):
                        rate = self.angular_jog_velocity
                    else:
                        rate = self.jog_velocity
                    self.isjogging[axis] = direction
                    self.emccommand.jog(self.emc.JOG_CONTINUOUS
                    ,0 ,axis ,direction * rate)

        def incremental_jog(self, axis, direction, distance):
                if self.masked: return
                self.set_motion_mode()
                self.isjogging[axis] = direction
                if axis in (3,4,5):
                    rate = self.angular_jog_velocity
                else:
                    rate = self.jog_velocity
                self.emccommand.jog(self.emc.JOG_INCREMENT
                ,0 ,axis ,direction * rate, distance)
                self.isjogging[axis] = 0
                
        def quill_up(self):
                if self.masked: return
                self.set_motion_mode()
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.wait_complete()
                self.emccommand.jog(self.emc.JOG_CONTINUOUS, 0, 2, 100)

        def feed_hold(self, data):
                if self.masked: return
                self.emccommand.set_feed_hold(data)

        def feed_override(self, f):
                if self.masked: return
                self.emccommand.feedrate(f)
                
        def rapid_override(self, f):
                if self.masked: return
                self.emccommand.rapidrate(f)

        def spindle_override(self, s):
                if self.masked: return
                self.emccommand.spindleoverride(s)

        def max_velocity(self, m):
                if self.masked: return
                self.emccommand.maxvel(m)

        def reload_tooltable(self, b):
                if self.masked: return
                self.emccommand.load_tool_table()                

        def opstop(self, data):
                if self.masked: return
                self.emccommand.set_optional_stop(data)

        def blockdel(self, data):
                if self.masked: return
                self.emccommand.set_block_delete(data)

        def abort(self):
                self.emccommand.abort()

        def single_block(self, s):
                self.sb = s
                self.emcstat.poll()
                if self.emcstat.queue > 0 or self.emcstat.paused:
                        # program or mdi is running
                        if s:
                                self.emccommand.auto(self.emc.AUTO_PAUSE)
                        else:
                                self.emccommand.auto(self.emc.AUTO_RESUME)

        # make sure linuxcnc is in AUTO mode
        # if Linuxcnc is paused then pushing cycle start will step the program
        # else the program starts from restart_line_number
        # after restarting it resets the restart_line_number to 0.
        # You must explicitily set a different restart line each time
        def cycle_start(self):
                if self.emcstat.task_mode != self.emc.MODE_AUTO:
                    self.emccommand.mode(self.emc.MODE_AUTO)
                    self.emccommand.wait_complete()
                self.emcstat.poll()
                if self.emcstat.paused:
                    self.emccommand.auto(self.emc.AUTO_STEP)
                    return
                if self.emcstat.interp_state == self.emc.INTERP_IDLE:
                        print self.restart_line_number
                        self.emccommand.auto(self.emc.AUTO_RUN, self.restart_line_number)
                self.restart_line_number = self.restart_reset_line

        # This restarts the program at the line specified directly (without cyscle start push)
        def re_start(self,line):
            self.emccommand.mode(self.emc.MODE_AUTO)
            self.emccommand.wait_complete()
            self.emccommand.auto(self.emc.AUTO_RUN, line)
            self.restart_line_number = self.restart_reset_line


        # set the restart line, you can the either restart directly
        # or restart on the cycle start button push
        # see above.
        # reset option allows one to change the default restart after it next restarts
        # eg while a restart dialog is open, always restart at the line it says
        # when the dialog close change the  line and reset both to zero
        def set_restart_line (self,line,reset=0):
            self.restart_line_number = line
            self.restart_reset_line = reset

        def set_manual_mode(self):
            self.emcstat.poll()
            if self.emcstat.task_mode != self.emc.MODE_MANUAL:
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.teleop_enable(1)
                self.emccommand.wait_complete()
            self.set_motion_mode()

        def set_mdi_mode(self):
            self.emcstat.poll()
            if self.emcstat.task_mode != self.emc.MODE_MDI:
                self.emccommand.mode(self.emc.MODE_MDI)
                self.emccommand.wait_complete()

        def set_auto_mode(self):
            self.emcstat.poll()
            if self.emcstat.task_mode != self.emc.MODE_AUTO:
                self.emccommand.mode(self.emc.MODE_AUTO)
                self.emccommand.wait_complete()

        def get_mode(self):
            self.emcstat.poll()
            return self.emcstat.task_mode

class emc_status:
        def __init__(self, data, emc):
            self.data = data
            self.emc = emc
            self.resized_dro = 0
            self.mm = 0
            self.machine_units_mm=0
            self.unit_convert=[1]*9
            self.actual = 1
            self.emcstat = emc.stat()
            self.emcerror = emc.error_channel()

        def get_feedrate(self):
            return self.emcstat.feedrate

        def get_spindlerate(self):
            return self.emcstat.spindle[0]['override']

        def get_maxvelocity(self):
            return self.emcstat.maxvelocity

        def dro_inch(self, b):
                self.mm = self.data.dro_units = 0

        def dro_mm(self, b):
                self.mm = self.data.dro_units = 1

        def set_machine_units(self,u,c):
                self.machine_units_mm = self.data.machine_units = u
                self.unit_convert = c

        # This holds the conversion multiplicand of all 9 axes
        # angular axes are always 1 - They are not converted.
        def convert_units_list(self,v):
                c = self.unit_convert
                return map(lambda x,y: x*y, v, c)

        # This converts the given data units if the current display mode (self.mm)
        # is not the same as the machine's basic units.
        # It converts with the multiplicand of joint 0 
        def convert_units(self,data):
            if self.mm != self.machine_units_mm:
                return self.unit_convert[0] * data
            else:
                return data

        def get_linear_units(self):
            return self.emcstat.linear_units

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
                p = self.convert_units_list(p)
                relp = self.convert_units_list(relp)
                dtg = self.convert_units_list(dtg)
            for letter in self.data.axis_list:
                count = "xyzabcuvws".index(letter)
                self.data["%s_is_homed"% letter] = self.emcstat.homed[count]
                self.data["%s_abs"% letter] = p[count]
                self.data["%s_rel"% letter] = relp[count]
                self.data["%s_dtg"% letter] = dtg[count]
            # active G codes
            temp = []; active_codes = []
            for i in sorted(self.emcstat.gcodes[1:]):
                if i == -1: continue
                if i % 10 == 0:
                        temp.append("%d" % (i/10))
                else:
                        temp.append("%d.%d" % (i/10, i%10))
            ipr = False
            for num,i in enumerate(temp):
                if num == 8:active_codes.append("\n")
                active_codes.append("G"+i)
                if i == '95': ipr = True
            self.data.IPR_mode = ipr
            self.data.active_gcodes = active_codes
            # M codes
            temp = []; active_codes = []
            for i in sorted(self.emcstat.mcodes[1:]):
                if i == -1: continue
                temp.append("%d" % i)
            for i in (temp):
                active_codes.append("M"+i)
            self.data.active_mcodes = active_codes
            feed_str = "%.3f" % self.emcstat.settings[1]
            if feed_str.endswith(".000"): feed_str = feed_str[:-4]
            self.data.active_feed_command = feed_str
            self.data.active_spindle_command = "%.0f" % self.emcstat.settings[2]

            # estop status
            self.data.estopped = self.emcstat.task_state == self.emc.STATE_ESTOP
            # spindle
            self.data.spindle_dir = self.emcstat.spindle[0]['direction']
            self.data.spindle_speed = abs(self.emcstat.spindle[0]['speed'])
            self.data.spindle_override = self.emcstat.spindle[0]['override']
            # other
            self.data.tool_in_spindle = self.emcstat.tool_in_spindle
            self.data.flood = self.emcstat.flood
            self.data.mist = self.emcstat.mist
            self.data.machine_on = self.emcstat.task_state == self.emc.STATE_ON
            self.data.or_limits = self.emcstat.joint[0]['override_limits']
            self.data.feed_hold = self.emcstat.feed_hold_enabled
            self.data.feed_override = self.emcstat.feedrate
            self.data.velocity_override = self.emcstat.max_velocity / self.data._maxvelocity
            self.data.rapid_override = self.emcstat.rapidrate
            self.data.file = self.emcstat.file
            self.data.last_line = self.data.motion_line
            self.data.motion_line = self.emcstat.motion_line
            self.data.line =  self.emcstat.current_line
            self.data.id =  self.emcstat.id
            self.data.dtg = self.emcstat.distance_to_go
            self.data.velocity = self.convert_units(self.emcstat.current_vel) * 60.0
            self.data.delay = self.emcstat.delay_left
            if self.emcstat.pocket_prepped == -1:
                self.data.preppedtool = None
            else:
                self.data.preppedtool = self.emcstat.tool_table[self.emcstat.pocket_prepped].id

