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

class settings:

    def __init__(self):
        self.W = gtk.Window()
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())

    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
            gtk.DIALOG_DESTROY_WITH_PARENT, gtk.MESSAGE_ERROR, 
            gtk.BUTTONS_CLOSE, error)
        md.run()
        md.destroy()

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
        self.W.destroy()
        return None

    def cancel(self, event):
        self.W.destroy()
        return None

    def do_settings(self):
        self.W = gtk.Dialog('Settings',
                       None,
                       gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                       buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        self.W.set_default_size(250, 200)
        t = gtk.Table(1, 1, True)
        self.W.vbox.add(t)
        preLabel = gtk.Label('Preamble')
        preLabel.set_alignment(0.95, 0.5)
        preLabel.set_width_chars(10)
        t.attach(preLabel, 0, 1, 0, 1)
        self.preEntry = gtk.Entry()
        t.attach(self.preEntry, 1, 5, 0, 1)
        postLabel = gtk.Label('Postamble')
        postLabel.set_alignment(0.95, 0.5)
        postLabel.set_width_chars(10)
        t.attach(postLabel, 0, 1, 1, 2)
        self.postEntry = gtk.Entry()
        t.attach(self.postEntry, 1, 5, 1, 2)
        oLabel = gtk.Label('Origin')
        oLabel.set_alignment(0.5, 1.0)
        oLabel.set_width_chars(10)
        t.attach(oLabel, 0, 5, 2, 3)
        self.centre = gtk.RadioButton(None, 'Centre')
        t.attach(self.centre, 1, 2, 3, 4)
        self.bLeft = gtk.RadioButton(self.centre, 'Bottom Left')
        t.attach(self.bLeft, 3, 4, 3, 4)
        shLabel = gtk.Label('Lead Lengths')
        shLabel.set_alignment(0.5, 1.0)
        shLabel.set_width_chars(10)
        t.attach(shLabel, 0, 5, 4, 5)
        liLabel = gtk.Label('Lead In')
        liLabel.set_alignment(0.95, 0.5)
        liLabel.set_width_chars(10)
        t.attach(liLabel, 0, 1, 5, 6)
        self.liEntry = gtk.Entry()
        self.liEntry.set_width_chars(10)
        t.attach(self.liEntry, 1, 2, 5, 6)
        loLabel = gtk.Label('Lead Out')
        loLabel.set_alignment(0.05, 0.5)
        loLabel.set_width_chars(10)
        t.attach(loLabel, 4, 5, 5, 6)
        self.loEntry = gtk.Entry()
        self.loEntry.set_width_chars(10)
        t.attach(self.loEntry, 3, 4, 5, 6)
        shLabel = gtk.Label('Small Holes')
        shLabel.set_alignment(0.5, 1.0)
        shLabel.set_width_chars(10)
        t.attach(shLabel, 0, 5, 6, 7)
        dLabel = gtk.Label('Diameter')
        dLabel.set_alignment(0.95, 0.5)
        dLabel.set_width_chars(10)
        t.attach(dLabel, 0, 1, 7, 8)
        self.dEntry = gtk.Entry()
        self.dEntry.set_width_chars(10)
        t.attach(self.dEntry, 1, 2, 7, 8)
        sLabel = gtk.Label('Speed %')
        sLabel.set_alignment(0.05, 0.5)
        sLabel.set_width_chars(10)
        t.attach(sLabel, 4, 5, 7, 8)
        self.sEntry = gtk.Entry()
        self.sEntry.set_width_chars(10)
        t.attach(self.sEntry, 3, 4, 7, 8)
        save = gtk.Button('Save')
        save.connect('pressed', self.save_settings)
        t.attach(save, 1, 2, 9, 10)
        cancel = gtk.Button('Cancel')
        cancel.connect('pressed', self.cancel)
        t.attach(cancel, 3, 4, 9, 10)
        self.W.show_all()
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
        response = self.W.run()
