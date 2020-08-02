#!/usr/bin/env python

'''
w_settings.py

Copyright (C) 2019, 2020  Phillip A Carter

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import gtk
import sys
import linuxcnc

class settings_wiz:

    def __init__(self):
        self.W = gtk.Window()
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())

    def save_settings(self, widget):
        f_out = open(self.configFile, 'w')
        f_out.write('#plasmac wizards configuration file, format is:\n#name = value\n\n')
        f_out.write('preamble={}\n'.format(self.preEntry.get_text()))
        f_out.write('postamble={}\n'.format(self.postEntry.get_text()))
        f_out.write('origin={}\n'.format(self.centre.get_active()))
        f_out.write('lead-in={}\n'.format(self.liEntry.get_text()))
        f_out.write('lead-out={}\n'.format(self.loEntry.get_text()))
        f_out.write('hole-diameter={}\n'.format(self.dEntry.get_text()))
        f_out.write('hole-speed={}\n'.format(self.sEntry.get_text()))
        f_out.write('window-width={}\n'.format(self.wWEntry.get_text()))
        f_out.write('window-height={}\n'.format(self.wHEntry.get_text()))
        f_out.write('grid-size={}\n'.format(self.gSEntry.get_text()))
        if self.wWEntry.get_text() and self.wHEntry.get_text():
            self.parent.resize_window(int(self.wWEntry.get_text()), int(self.wHEntry.get_text()))
        if self.gSEntry.get_text():
            self.parent.preview.grid_size = float(self.gSEntry.get_text())
            self.parent.preview.load(self.fNgc)

    def load_config(self, widget):
        if os.path.exists(self.configFile):
            f_in = open(self.configFile, 'r')
            for line in f_in:
                if line.startswith('preamble'):
                    self.preEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('postamble'):
                    self.postEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('origin'):
                    if line.strip().split('=')[1] == 'True':
                        self.centre.set_active(1)
                    else:
                        self.bLeft.set_active(1)
                elif line.startswith('lead-in'):
                    self.liEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('lead-out'):
                    self.loEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('hole-diameter'):
                    self.dEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('hole-speed'):
                    self.sEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('window-width'):
                    self.wWEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('window-height'):
                    self.wHEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('grid-size'):
                    self.gSEntry.set_text(line.strip().split('=')[1])

    def entry_changed(self, widget):
        if widget.get_text():
            if widget.get_text()[len(widget.get_text()) - 1] not in '.0123456789':
                widget.set_text(widget.get_text()[:len(widget.get_text()) - 1])

    def settings_show(self, parent, entries, fNgc, fNgcBkp, fTmp, rowS):
        entries.set_row_spacings(rowS)
        self.parent = parent
        for child in entries.get_children():
            entries.remove(child)
        self.fNgc = fNgc
        self.fNgcBkp = fNgcBkp
        self.fTmp = fTmp
        preLabel = gtk.Label('Preamble')
        preLabel.set_alignment(0.95, 0.5)
        preLabel.set_width_chars(8)
        entries.attach(preLabel, 0, 1, 0, 1)
        self.preEntry = gtk.Entry()
        entries.attach(self.preEntry, 1, 5, 0, 1)
        postLabel = gtk.Label('Postamble')
        postLabel.set_alignment(0.95, 0.5)
        postLabel.set_width_chars(8)
        entries.attach(postLabel, 0, 1, 1, 2)
        self.postEntry = gtk.Entry()
        entries.attach(self.postEntry, 1, 5, 1, 2)
        oLabel = gtk.Label('Origin')
        oLabel.set_alignment(0.5, 1.0)
        oLabel.set_width_chars(8)
        entries.attach(oLabel, 0, 5, 2, 3)
        self.centre = gtk.RadioButton(None, 'Centre')
        entries.attach(self.centre, 1, 2, 3, 4)
        self.bLeft = gtk.RadioButton(self.centre, 'Btm Lft')
        entries.attach(self.bLeft, 3, 4, 3, 4)
        shLabel = gtk.Label('Lead Lengths')
        shLabel.set_alignment(0.5, 1.0)
        shLabel.set_width_chars(8)
        entries.attach(shLabel, 0, 5, 4, 5)
        liLabel = gtk.Label('Lead In')
        liLabel.set_alignment(0.95, 0.5)
        liLabel.set_width_chars(8)
        entries.attach(liLabel, 0, 1, 5, 6)
        self.liEntry = gtk.Entry()
        self.liEntry.set_width_chars(8)
        self.liEntry.connect('changed', self.entry_changed)
        entries.attach(self.liEntry, 1, 2, 5, 6)
        loLabel = gtk.Label('Lead Out')
        loLabel.set_alignment(0.05, 0.5)
        loLabel.set_width_chars(8)
        entries.attach(loLabel, 4, 5, 5, 6)
        self.loEntry = gtk.Entry()
        self.loEntry.set_width_chars(8)
        self.loEntry.connect('changed', self.entry_changed)
        entries.attach(self.loEntry, 3, 4, 5, 6)
        shLabel = gtk.Label('Small Holes')
        shLabel.set_alignment(0.5, 1.0)
        shLabel.set_width_chars(8)
        entries.attach(shLabel, 0, 5, 6, 7)
        dLabel = gtk.Label('Diameter')
        dLabel.set_alignment(0.95, 0.5)
        dLabel.set_width_chars(8)
        entries.attach(dLabel, 0, 1, 7, 8)
        self.dEntry = gtk.Entry()
        self.dEntry.set_width_chars(8)
        self.dEntry.connect('changed', self.entry_changed)
        entries.attach(self.dEntry, 1, 2, 7, 8)
        sLabel = gtk.Label('Speed %')
        sLabel.set_alignment(0.05, 0.5)
        sLabel.set_width_chars(8)
        entries.attach(sLabel, 4, 5, 7, 8)
        self.sEntry = gtk.Entry()
        self.sEntry.set_width_chars(8)
        self.sEntry.connect('changed', self.entry_changed)
        entries.attach(self.sEntry, 3, 4, 7, 8)
        wSLabel = gtk.Label('Window Size (min = 890 x 663)')
        wSLabel.set_alignment(0.5, 1.0)
        wSLabel.set_width_chars(8)
        entries.attach(wSLabel, 0, 5, 8, 9)
        wWLabel = gtk.Label('Width')
        wWLabel.set_alignment(0.95, 0.5)
        wWLabel.set_width_chars(8)
        entries.attach(wWLabel, 0, 1, 9, 10)
        self.wWEntry = gtk.Entry()
        self.wWEntry.set_width_chars(8)
        self.wWEntry.connect('changed', self.entry_changed)
        entries.attach(self.wWEntry, 1, 2, 9, 10)
        wHLabel = gtk.Label('Height')
        wHLabel.set_alignment(0.05, 0.5)
        wHLabel.set_width_chars(8)
        entries.attach(wHLabel, 4, 5, 9, 10)
        self.wHEntry = gtk.Entry()
        self.wHEntry.set_width_chars(8)
        self.wHEntry.connect('changed', self.entry_changed)
        entries.attach(self.wHEntry, 3, 4, 9, 10)
        gSLabel = gtk.Label('Grid Size')
        gSLabel.set_alignment(0.95, 0.5)
        gSLabel.set_width_chars(8)
        entries.attach(gSLabel, 0, 1, 11, 12)
        self.gSEntry = gtk.Entry()
        self.gSEntry.set_width_chars(8)
        self.gSEntry.connect('changed', self.entry_changed)
        entries.attach(self.gSEntry, 1, 2, 11, 12)
        save = gtk.Button('Save')
        save.connect('pressed', self.save_settings)
        entries.attach(save, 1, 2, 13, 14)
        cancel = gtk.Button('Reload')
        cancel.connect('pressed', self.load_config)
        entries.attach(cancel, 3, 4, 13, 14)
        self.load_config(None)
        self.parent.W.show_all()
        self.liEntry.grab_focus()
