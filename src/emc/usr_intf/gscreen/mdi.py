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



# self.mcodes = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 30, 48, 49, 50, 51,
#                52, 53, 60, 61, 62, 63, 64, 65, 66, 67, 68)
# 
# self.gcodes = (0, 10, 20, 30, 40, 50, 51, 52, 53, 70, 80, 100,
#                170, 171, 180, 181, 190, 191, 200, 210, 280, 281,
#                300, 301, 330, 331, 382, 383, 384, 385, 400, 410,
#                411, 420, 421, 430, 431, 490, 530, 540, 550, 560,
#                570, 580, 590, 591, 592, 593, 610, 611, 640, 730,
#                760, 800, 810, 820, 830, 840, 850, 860, 870, 880,
#                890, 900, 901, 910, 911, 920, 921, 922, 923, 930,
#                940, 950, 960, 970, 980, 990)

class mdi:
    def __init__(self, emc):
        self.clear()
        self.emc = emc
        self.emcstat = emc.stat()
        self.emccommand = emc.command()

        self.emcstat.poll()
        am = self.emcstat.axis_mask

        self.axes = []
        self.polar = 0
        axisnames = ['X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W']
        for i in range(9):
           if am & (1<<i):
               self.axes.append(axisnames[i])

        self.gcode = 'M2'

        self.codes = {
            'M3' : [_('Spindle CW'), 'S'],
            'M4' : [_('Spindle CCW'), 'S'],
            'M6' : [_('Tool change'), 'T'],
            'M66' : [_('Input control'), 'P', 'E', 'L', 'Q'],

            # 'A' means 'the axes'
            'G0' : [_('Straight rapid'), 'A'],
            'G00' : [_('Straight rapid'), 'A'],
            'G1' : [_('Straight feed'), 'A', 'F'],
            'G01' : [_('Straight feed'), 'A', 'F'],
            'G2' : [_('Arc CW'), 'A', 'I', 'J', 'K', 'R', 'F'],
            'G02' : [_('Arc CW'), 'A', 'I', 'J', 'K', 'R', 'F'],
            'G3' : [_('Arc CCW'), 'A', 'I', 'J', 'K', 'R', 'F'],
            'G03' : [_('Arc CCW'), 'A', 'I', 'J', 'K', 'R', 'F'],
            'G4' : [_('Dwell'), 'P'],
            'G04' : [_('Dwell'), 'P'],
            'G10' : [_('Setup'), 'L', 'P', 'A', 'Q', 'R'],
            'G33' : [_('Spindle synchronized feed'), 'A', 'K'],
            'G33.1' : [_('Rigid tap'), 'Z', 'K'],
            'G38.2' : [_('Probe'), 'A', 'F'],
            'G38.3' : [_('Probe'), 'A', 'F'],
            'G38.4' : [_('Probe'), 'A', 'F'],
            'G38.5' : [_('Probe'), 'A', 'F'],
            'G41' : [_('Radius compensation left'), 'D'],
            'G42' : [_('Radius compensation right'), 'D'],
            'G41.1' : [_('Radius compensation left, immediate'), 'D', 'L'],
            'G42.1' : [_('Radius compensation right, immediate'), 'D', 'L'],
            'G43' : [_('Tool length offset'), 'H'],
            'G43.1' : [_('Tool length offset immediate'), 'I', 'K'],
            'G53' : [_('Motion in unoffset coordinates'), 'G', 'A', 'F'],
            'G64' : [_('Continuous mode'), 'P'],
            'G76' : [_('Thread'), 'Z', 'P', 'I', 'J', 'K', 'R', 'Q', 'H', 'E', 'L'],
            'G81' : [_('Drill'), 'A', 'R', 'L', 'F'],
            'G82' : [_('Drill with dwell'), 'A', 'R', 'L', 'P', 'F'],
            'G83' : [_('Peck drill'), 'A', 'R', 'L', 'Q', 'F'],
            'G73' : [_('Chip-break drill'), 'A', 'R', 'L', 'Q', 'F'],
            'G85' : [_('Bore'), 'A', 'R', 'L', 'F'],
            'G89' : [_('Bore with dwell'), 'A', 'R', 'L', 'P', 'F'],
            'G92' : [_('Offset all coordinate systems'), 'A'],
            'G96' : [_('CSS Mode'), 'S', 'D'],
            }
        self.ocodes = []

    def add_macros(self, macros):
        for m in macros:
            words = m.split()
            call = "O<%s> call" % words[0]
            args = [''] + [w + ' ' for w in words[1:]]
            self.ocodes.append(call)
            self.codes[call] = args

    def get_description(self, gcode):
        return self.codes[gcode][0]
    
    def get_words(self, gcode):
        self.gcode = gcode
        if gcode[0] == 'M' and gcode.find(".") == -1 and int(gcode[1:]) >= 100 and int(gcode[1:]) <= 199:
            return ['P', 'Q']
        if not self.codes.has_key(gcode):
            return []
        # strip description
        words = self.codes[gcode][1:]
        # replace A with the real axis names
        if 'A' in words:
            i = words.index('A')
            words = words[:i] + self.axes + words[i+1:]
            if self.polar and 'X' in self.axes and 'Y' in self.axes:
                words[self.axes.index('X')] = '@'
                words[self.axes.index('Y')] = '^'
        return words

    def clear(self):
        self.words = {}

    def set_word(self, word, value):
        self.words[word] = value

    def set_polar(self, p):
        self.polar = p;

    def issue(self):
        m = self.gcode
        if m.lower().startswith('o'):
            codes = self.codes[m]
            for code in self.codes[m][1:]:
                v = self.words[code] or "0"
                m = m + " [%s]" % v
        else:
            w = [i for i in self.words if len(self.words.get(i)) > 0]
            if '@' in w:
                m += '@' + self.words.get('@')
                w.remove('@')
            if '^' in w:
                m += '^' + self.words.get('^')
                w.remove('^')
            for i in w:
                if len(self.words.get(i)) > 0:
                    m += i + self.words.get(i)
        self.emcstat.poll()
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi(m)

    def set_tool_touchoff(self,tool,axis,value):
        m = "G10 L10 P%d %s%f"%(tool,axis,value)
        self.emcstat.poll()
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi(m)
        self.emccommand.wait_complete()
        self.emccommand.mdi("g43")

    def set_axis_origin(self,axis,value):
        m = "G10 L20 P0 %s%f"%(axis,value)
        self.emcstat.poll()
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi(m)

    def go_to_position(self,axis,position,feedrate):
        m = "G1 %s %f F%f"%(axis,position,feedrate)
        self.emcstat.poll()
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi(m)

    def set_spindle_speed(self,value):
        m = "s %f"%(value)
        self.emcstat.poll()
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi(m)

    def set_user_system(self,value):
        m = "g %f"%(value)
        self.emcstat.poll()
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi(m)

    def index_tool(self,toolnumber):
        m = "T %f M6"%(toolnumber)
        self.emcstat.poll()
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi("m6 T %f"%(toolnumber))
        self.emccommand.mdi("g43 h%f"%(toolnumber))

    def arbitrary_mdi(self,command):
        if self.emcstat.task_mode != self.emc.MODE_MDI:
            self.emccommand.mode(self.emc.MODE_MDI)
            self.emccommand.wait_complete()
        self.emccommand.mdi(command)

class mdi_control:
    def __init__(self, gtk, emc, labels, eventboxes):
        self.labels = labels
        self.eventboxes = eventboxes
        self.numlabels = len(labels)
        self.numwords = 1
        self.selected = 0
        self.gtk = gtk
        
        self.mdi = mdi(emc)
        
        #for i in range(self.numlabels):
        #    self.not_editing(i)
        #self.editing(self.selected)
        #self.set_text("G")

    def mdi_is_reading(self):
        self.mdi.emcstat.poll()
        if self.mdi.emcstat.interp_state == self.mdi.emc.INTERP_READING:
            return True
        return False

    def set_mdi_mode(self):
        self.mdi.emcstat.poll()
        if self.mdi.emcstat.task_mode != self.mdi.emc.MODE_MDI:
            self.mdi.emccommand.mode(self.mdi.emc.MODE_MDI)
            self.mdi.emccommand.wait_complete()

    def set_axis(self,axis,value):
        premode = self.mdi.emcstat.task_mode
        self.mdi.set_axis_origin(axis,value)
        self.mdi.emccommand.mode(premode)
        self.mdi.emccommand.wait_complete()

    def touchoff(self,tool,axis,value):
        premode = self.mdi.emcstat.task_mode
        self.mdi.set_tool_touchoff(tool,axis,value)
        self.mdi.emccommand.mode(premode)
        self.mdi.emccommand.wait_complete()

    def set_spindle_speed(self,value):
        self.mdi.set_spindle_speed(value)

    def go_to_position(self,axis,position,feedrate):
        self.mdi.go_to_position(axis,position,feedrate)

    def set_user_system(self,system):
        print "set user system to :G",system
        premode = self.mdi.emcstat.task_mode
        self.mdi.set_user_system(system)
        self.mdi.emccommand.mode(premode)
        self.mdi.emccommand.wait_complete()

    def index_tool(self,toolnumber):
        print "set tool number to :T",toolnumber
        premode = self.mdi.emcstat.task_mode
        self.mdi.index_tool(toolnumber)
        #self.mdi.emccommand.mode(premode)
        #self.mdi.emccommand.wait_complete()

    def user_command(self,command):
        premode = self.mdi.emcstat.task_mode
        self.mdi.arbitrary_mdi(command)
        #self.mdi.emccommand.mode(premode)
        #self.mdi.emccommand.wait_complete()

    def not_editing(self, n):
        e = self.eventboxes[n]
        e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse("#ccc"))

    def editing(self, n):
        self.not_editing(self.selected)
        self.selected = n
        e = self.eventboxes[n]
        e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse("#fff"))

    def get_text(self):
        w = self.labels[self.selected]
        return w.get_text()

    def set_text(self, t, n = -1):
        if n == -1: n = self.selected
        w = self.labels[n]
        w.set_text(t)
        if n > 0:
            head = t.rstrip("0123456789.-")
            tail = t[len(head):]
            self.mdi.set_word(head, tail)
        if len(t) < 2:
            w.set_alignment(1.0, 0.5)
        else:
            w.set_alignment(0.0, 0.5)
            
    def clear(self, b):
        t = self.get_text()
        self.set_text(t.rstrip("0123456789.-"))
        
    def back(self, b):
        t = self.get_text()
        if t[-1:] in "0123456789.-":
            self.set_text(t[:-1])

    def fill_out(self):
        if self.selected == 0:
            w = self.mdi.get_words(self.get_text())
            self.numwords = len(w)
            for i in range(1,self.numlabels):
                if i <= len(w):
                    self.set_text(w[i-1], i)
                else:
                    self.set_text("", i)

    def next(self, b):
        self.fill_out();
        if self.numwords > 0:
            self.editing(max(1,(self.selected+1) % (self.numwords+1)))

    def ok(self, b):
        self.fill_out();
        self.mdi.issue()

    def decimal(self, b):
        t = self.get_text()
        if t.find(".") == -1:
            self.set_text(t + ".")

    def minus(self, b):
        t = self.get_text()
        if self.selected > 0:
            head = t.rstrip("0123456789.-")
            tail = t[len(head):]
            if tail.find("-") == -1:
                self.set_text(head + "-" + tail)
            else:
                self.set_text(head + tail[1:])

    def keypad(self, b):
        t = self.get_text()
        num = b.get_name()
        self.set_text(t + num)

    def gp(self, b):
        self.g(b, "G", 1)

    def g(self, b, code="G", polar=0):
        self.mdi.set_polar(polar)
        self.set_text(code, 0)
        for i in range(1, self.numlabels):
            self.set_text("", i)
        self.editing(0)
        self.mdi.clear()

    def m(self, b):
        self.g(b, "M")

    def t(self, b):
        self.g(b, "T")

    def o(self, b):
        old_code = self.labels[0].get_text()
        ocodes = self.mdi.ocodes
        if old_code in ocodes:
            j = (ocodes.index(old_code) + 1) % len(ocodes)
        else:
            j = 0
        self.g(b, ocodes[j])
        self.next(b)

    def select(self, eventbox, event):
        n = int(eventbox.get_name()[12:])
        if self.selected == 0:
            self.fill_out()
        if n <= self.numwords:
            self.editing(n)

    def set_tool(self, tool, g10l11):
        self.g(0)
        self.set_text("G10", 0)
        self.next(0)
        if g10l11:
            self.set_text("L11", 1)
        else:
            self.set_text("L10", 1)
        self.next(0)
        self.set_text("P%d" % tool, 2)
        self.next(0)
        self.next(0)
        self.next(0)

    def set_origin(self, system):
        self.g(0)
        self.set_text("G10", 0)
        self.next(0)
        self.set_text("L20", 1)
        self.next(0)
        self.set_text("P%d" % system, 2)
        self.next(0)
