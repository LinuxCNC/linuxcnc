#!/usr/bin/env python

'''
w_line.py

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
import time
import math
import linuxcnc
import shutil
import hal
from subprocess import Popen,PIPE

class line:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())

    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
            gtk.DIALOG_DESTROY_WITH_PARENT, gtk.MESSAGE_ERROR, 
            gtk.BUTTONS_CLOSE, error)
        md.run()
        md.destroy()

    def load_file(self, fName):
        if self.gui == 'axis':
            Popen('axis-remote {}'.format(fName), stdout = PIPE, shell = True)
        elif self.gui == 'gmoccapy':
            self.c = linuxcnc.command()
            self.c.program_open('blank.ngc')
            self.c.program_open(fName)
        else:
            print('Unknown GUI in .ini file')

    def end_this_shape(self, event):
        if os.path.exists(self.fWizard):
            outWiz = open(self.fWizard, 'a+')
            post = False
            for line in outWiz:
                if '(postamble)' in line:
                    post = True
            if not post:
                outWiz.write('\n(postamble)\n')
                outWiz.write('{}\n'.format(self.postamble))
                outWiz.write('m30\n')
            outWiz.close()
            self.load_file(self.fWizard)
        self.W.destroy()
        return None

    def add_shape_to_file(self, event):
        if os.path.exists(self.fWizard):
            path = os.path.dirname(os.path.abspath(self.fWizard))
            tmp = ('{}/tmp'.format(path))
            shutil.copyfile(self.fWizard, tmp)
            inWiz = open(tmp, 'r')
            outWiz = open(self.fWizard, 'w')
            for line in inWiz:
                if '(postamble)' in line:
                    break
                outWiz.write(line)
            inWiz.close()
            outWiz.close()
            os.remove(tmp)
            inTmp = open(self.fTmp, 'r')
            outWiz = open(self.fWizard, 'a')
            for line in inTmp:
                outWiz.write(line)
        else:
            inTmp = open(self.fTmp, 'r')
            outWiz = open(self.fWizard, 'w')
            outWiz.write('{}\n'.format(self.preamble))
            outWiz.write('f#<_hal[plasmac.cut-feed-rate]>\n')
            for line in inTmp:
                outWiz.write(line)
        inTmp.close()
        outWiz.close()
        self.add.set_sensitive(False)

    def send_preview(self, event):
        self.s.poll()
        xPos = self.s.actual_position[0] - self.s.g5x_offset[0] - self.s.g92_offset[0]
        yPos = self.s.actual_position[1] - self.s.g5x_offset[1] - self.s.g92_offset[1]
        if self.xEEntry.get_text() or self.yEEntry.get_text() or self.lEntry.get_text():
            if self.xSEntry.get_text():
                xS = float(self.xSEntry.get_text())
            else:
                xS = xPos
            if self.ySEntry.get_text():
                yS = float(self.ySEntry.get_text())
            else:
                yS = yPos

            if self.xEEntry.get_text():
                x1 = float(self.xEEntry.get_text())
            else:
                x1 = xPos
            if self.yEEntry.get_text():
                y1 = float(self.yEEntry.get_text())
            else:
                y1 = yPos
            if self.aEntry.get_text():
                angle = math.radians(float(self.aEntry.get_text()))
            else:
                angle = 0
            if self.lEntry.get_text():
                x2 = xS + (float(self.lEntry.get_text()) * math.cos(angle))
                y2 = yS + (float(self.lEntry.get_text()) * math.sin(angle))
            else:
                x2 = y2 = 0
            if x2 <> 0 or y2 <> 0:
                xE = x2
                yE = y2
            else:
                xE = x1
                yE = y1
            self.fTmp = '{}/shape.tmp'.format(self.tmpDir)
            self.fNgc = '{}/shape.ngc'.format(self.tmpDir)
            outTmp = open(self.fTmp, 'w')
            outNgc = open(self.fNgc, 'w')
            if os.path.exists(self.fWizard):
                inWiz = open(self.fWizard, 'r')
                for line in inWiz:
                    if '(postamble)' in line:
                        break
                    outNgc.write(line)
            else:
                outNgc.write('{}\n'.format(self.preamble))
                outNgc.write('f#<_hal[plasmac.cut-feed-rate]>\n')
            outTmp.write('\n(wizard line)\n')
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xE, yE))
            outTmp.write('m5\n')
            outTmp.close()
            outTmp = open(self.fTmp, 'r')
            for line in outTmp:
                outNgc.write(line)
            outTmp.close()
            outNgc.write('\n(postamble)\n')
            outNgc.write('{}\n'.format(self.postamble))
            outNgc.write('m30\n')
            outNgc.close()
            self.load_file(self.fNgc)
            self.add.set_sensitive(True)
            hal.set_p('plasmac_run.preview-tab', '1')
        elif self.aEntry.get_text() and not self.lEntry.get_text():
            self.dialog_error('Length required if Angle entered')
        elif not self.aEntry.get_text() and self.lEntry.get_text():
            self.dialog_error('Angle required if Length entered')
        else:
            self.dialog_error('Minimum requirements are:\n\n'\
                              'X end and/or Y end\n\n'\
                              'or\n\n'\
                              'Angle and Length')

    def do_line(self, fWizard, tmpDir):
        self.tmpDir = tmpDir
        self.fWizard = fWizard
        self.W = gtk.Dialog('Line',
                       None,
                       gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                       buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        self.W.set_default_size(250, 200)
        t = gtk.Table(1, 1, True)
        t.set_row_spacings(6)
        self.W.vbox.add(t)
        xSLabel = gtk.Label('X start')
        xSLabel.set_alignment(0.95, 0.5)
        xSLabel.set_width_chars(10)
        t.attach(xSLabel, 0, 1, 0, 1)
        self.xSEntry = gtk.Entry()
        self.xSEntry.set_width_chars(10)
        t.attach(self.xSEntry, 1, 2, 0, 1)
        ySLabel = gtk.Label('Y start')
        ySLabel.set_alignment(0.95, 0.5)
        ySLabel.set_width_chars(10)
        t.attach(ySLabel, 0, 1, 1, 2)
        self.ySEntry = gtk.Entry()
        self.ySEntry.set_width_chars(10)
        t.attach(self.ySEntry, 1, 2, 1, 2)
        i1Label = gtk.Label('do the next two')
        i1Label.set_alignment(1.0, 1.0)
        t.attach(i1Label, 0, 2, 2, 3)
        xELabel = gtk.Label('X end')
        xELabel.set_alignment(0.95, 0.5)
        xELabel.set_width_chars(10)
        t.attach(xELabel, 0, 1, 3, 4)
        self.xEEntry = gtk.Entry()
        self.xEEntry.set_width_chars(10)
        t.attach(self.xEEntry, 1, 2, 3, 4)
        yELabel = gtk.Label('Y end')
        yELabel.set_alignment(0.95, 0.5)
        yELabel.set_width_chars(10)
        t.attach(yELabel, 0, 1, 4, 5)
        self.yEEntry = gtk.Entry()
        self.yEEntry.set_width_chars(10)
        t.attach(self.yEEntry, 1, 2, 4, 5)
        i2Label = gtk.Label('or the next two')
        i2Label.set_alignment(1.0, 1.0)
        t.attach(i2Label, 0, 2, 5, 6)
        aLabel = gtk.Label('Angle')
        aLabel.set_alignment(0.95, 0.5)
        aLabel.set_width_chars(10)
        t.attach(aLabel, 0, 1, 6, 7)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(10)
        self.aEntry.set_text('0')
        t.attach(self.aEntry, 1, 2, 6, 7)
        lLabel = gtk.Label('Length')
        lLabel.set_alignment(0.95, 0.5)
        lLabel.set_width_chars(10)
        t.attach(lLabel, 0, 1, 7, 8)
        self.lEntry = gtk.Entry()
        self.lEntry.set_width_chars(10)
        t.attach(self.lEntry, 1, 2, 7, 8)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.send_preview)
        t.attach(preview, 0, 1, 9, 10)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        t.attach(self.add, 2, 3, 9, 10)
        end = gtk.Button('Return')
        end.connect('pressed', self.end_this_shape)
        t.attach(end, 4, 5, 9, 10)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/line.png', 
                width=240, 
                height=240)
        image = gtk.Image()
        image.set_from_pixbuf(pixbuf)
        t.attach(image, 2, 5, 0, 8)
        self.xSEntry.grab_focus()
        self.W.show_all()
        if os.path.exists(self.configFile):
            f_in = open(self.configFile, 'r')
            for line in f_in:
                if line.startswith('preamble'):
                    self.preamble = line.strip().split('=')[1]
                elif line.startswith('postamble'):
                    self.postamble = line.strip().split('=')[1]
        response = self.W.run()
