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



class listing:
    def __init__(self, gtk, emc, labels, eventboxes):
        self.labels = labels
        self.eventboxes = eventboxes
        self.numlabels = len(labels)
        self.gtk = gtk
        self.emc = emc
        self.lineoffset = 0
        self.selected = -1
        self.start_line = -1
        self.filename = ""
        self.program = []
        self.lines = 0
        self.populate()

    def populate(self):
        program = self.program[self.lineoffset:self.lineoffset + self.numlabels]
        for i in range(self.numlabels):
            l = self.labels[i]
            e = self.eventboxes[i]
            if i < len(program):
                l.set_text(program[i].rstrip())
            else:
                l.set_text('')
            
            if self.start_line == self.lineoffset + i:
                e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse('#66f'))
            elif self.selected == self.lineoffset + i:
                e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse('#fff'))
            else:
                e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse('#ccc'))

    def show_line(self, n):
        if len(self.program) <= self.numlabels:
            self.lineoffset = 0
        else:
            self.lineoffset = min(max(0, n - self.numlabels/2),self.lines - self.numlabels)
        self.populate()

    def highlight_line(self, n):
        n -= 1                          # program[] is zero-based, emc line numbers are one-based
        if self.selected == n: return
        self.selected = n
        self.show_line(n)

    def up(self, b):
        self.lineoffset -= self.numlabels
        if self.lineoffset < 0:
            self.lineoffset = 0
        self.populate()

    def down(self, b):
        self.lineoffset += self.numlabels
        self.populate()

    def readfile(self, fn):
        self.filename = fn
        f = file(fn, 'r')
        self.program = f.readlines()
        f.close()
        self.lines = len(self.program)
        self.lineoffset = 0
        self.selected = -1
        self.populate()

    def reload(self, b):
        pass

    def previous(self, b,count=1):
	for i in range(count):
	    while True:
		if self.start_line <= 0:
		    break
		self.start_line -= 1
		if (self.program[self.start_line][0] == 'N' or
		    self.program[self.start_line][0] == 'n' ):
		    break
        self.show_line(self.start_line)

    def next(self,b,count=1):
        if count < 0: return self.previous(b, -count)
	for i in range(count):
	    while True:
		if self.start_line >= len(self.program)-1:
		    break
		self.start_line += 1
		if (self.program[self.start_line][0] == 'N' or
		    self.program[self.start_line][0] == 'n' ):
		    break
        self.show_line(self.start_line)

    def clear_startline(self):
        self.start_line = -1
        self.populate()

    def get_startline(self):
        return self.start_line + 1
