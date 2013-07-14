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

import os

class filechooser:
    def __init__(self, gtk, emc, labels, eventboxes, listing):
        self.labels = labels
        self.eventboxes = eventboxes
        self.numlabels = len(labels)
        self.listing = listing
        self.gtk = gtk
        self.emc = emc
        self.emccommand = emc.command()
        self.fileoffset = 0
        self.dir = os.path.join(os.getenv('HOME'), 'linuxcnc', 'nc_files')
        self.reload(0)

    def populate(self):
        files = self.files[self.fileoffset:]
        for i in range(self.numlabels):
            l = self.labels[i]
            e = self.eventboxes[i]
            if i < len(files):
                l.set_text(files[i])
            else:
                l.set_text('')
            if self.selected == self.fileoffset + i:
                e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse('#fff'))
            else:
                e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse('#ccc'))

    def select(self, eventbox, event):
        n = int(eventbox.get_name()[20:])
        fn = self.labels[n].get_text()
        if len(fn) == 0: return(fn)
        self.selected = self.fileoffset + n
        self.emccommand.mode(self.emc.MODE_MDI)
        fn = os.path.join(self.dir, fn)
        self.emccommand.program_open(fn)
        self.listing.readfile(fn)
        self.populate()
        return(fn)

    def select_and_show(self,fn):
        self.reload(0)
        numfiles = len(self.files)
        fn = os.path.basename(fn)
        self.fileoffset = 0
        found = False
        while True:
            for k in range(self.numlabels):
                n = k + self.fileoffset
                if n >= numfiles: return # notfound
                if self.files[n] == fn:
                    found = True
                    break # from for
            if found: break # from while
            self.fileoffset += self.numlabels

        self.selected = n
        fn = os.path.join(self.dir, fn)
        self.listing.readfile(fn)
        self.populate()

    def up(self, b):
        self.fileoffset -= self.numlabels
        if self.fileoffset < 0:
            self.fileoffset = 0
        self.populate()

    def down(self, b):
        self.fileoffset += self.numlabels
        self.populate()

    def reload(self, b):
        self.files = os.listdir(self.dir)
        self.files = [i for i in self.files if i.endswith('.ngc') and
                      os.path.isfile(os.path.join(self.dir, i))]
        self.files.sort()
        self.selected = -1
        self.populate()
