#!/usr/bin/env python

'''
w_rotate.py

Copyright (C) 2020  Phillip A Carter

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
import linuxcnc
import shutil
import hal
from subprocess import Popen,PIPE
import re
import math

class rotate:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())
        self.fNgc = ''
        self.fCode = ''
        self.previewed = False
        self.data_changed = True
        
    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
                               gtk.DIALOG_DESTROY_WITH_PARENT,
                               gtk.MESSAGE_ERROR, 
                               gtk.BUTTONS_CLOSE,
                               error)
        md.run()
        md.destroy()

    def load_file(self, fName):
        if self.gui == 'axis':
            Popen('axis-remote {}'.format(fName), stdout = PIPE, shell = True)
        elif self.gui == 'gmoccapy':
            self.c = linuxcnc.command()
            self.c.program_open('./wizards/blank.ngc')
            self.c.program_open(fName)
        else:
            print('Unknown GUI in .ini file')

    def cancel(self, widget):
        if os.path.exists(self.fWizard):
            shutil.copyfile(self.fOriginal, self.fWizard)
        if self.previewed:
            if self.gui == 'axis':
                self.load_file(self.fOriginal)
            elif self.gui == 'gmoccapy':
                self.c.program_open(self.fOriginal)
                os.remove(self.outFile)
        self.previewed = False
        self.W.destroy()
        return None

    def accept_rotate(self, widget):
        shutil.copyfile(self.fNgc, self.fWizard)
        self.load_file(self.fWizard)
        self.W.destroy()
        return None

    def preview_rotate(self, event):
        if self.aEntry.get_text():
            angle = float(self.aEntry.get_text())
        else:
            angle = 90
        if self.xOEntry.get_text():
            xOffset = float(self.xOEntry.get_text())
        else:
            xOffset = 0
        if self.yOEntry.get_text():
            yOffset = float(self.yOEntry.get_text())
        else:
            yOffset = 0
        self.fCode = '{}/rotate_code.ngc'.format(self.tmpDir)
        self.s.poll()
        if self.previewed:
            md = gtk.MessageDialog(self.W, 
                                   gtk.DIALOG_DESTROY_WITH_PARENT,
                                   gtk.MESSAGE_INFO, 
                                   gtk.BUTTONS_NONE,
                                   'ROTATE\n\nCalculating.....')
            md.set_keep_above(True)
            md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
            md.set_default_size(200, 100)
            md.show_all()
            self.c.program_open(self.fOriginal)
            watchdog = time.time() + 5
            while(1):
                self.s.poll()
                if os.path.basename(self.s.file) == os.path.basename(self.fOriginal):
                    n = time.time() + 1
                    while(1):
                        if time.time() > n:
                            break
                        while gtk.events_pending():
                            gtk.main_iteration()
                    break
                if time.time() > watchdog:
                    md.destroy()
                    self.dialog_error('Rotate file error')
                    return
        outNgc = open(self.fNgc, 'w')
        inCod = open(self.fOriginal, 'r')
        for line in inCod:
            if line.strip().lower().startswith('g'):
                rLine = self.rotate_object(line)
                if rLine is not None:
                    outNgc.write(rLine)
                else:
                    return
            else:
                outNgc.write(line)
        inCod.close()
        outNgc.close()
        self.load_file(self.fNgc)
        if self.previewed:
            md.destroy()
        hal.set_p('plasmac_run.preview-tab', '1')
        self.previewed = True
        self.data_changed = False
        self.accept.set_sensitive(True)

    def rotate_object(self, line):
        REGCODE = re.compile('([a-z]-?[0-9]+\.?([0-9]+)?)|\(.*\)')
        inLine = line.strip()
        self.comment = ''
        i = inLine.find('(')
        if i >= 0:
          self.comment = inLine[i:]
          inLine = inLine[:i - 1].strip()
        if len(inLine) > 0:
            parts = list([ list(cmd)[0] for cmd in REGCODE.findall(inLine) ])
            if len(parts) == 0 or parts[0] not in ['g0', 'g1', 'g2', 'g3']:
                return line
            angle = math.radians(float(self.aEntry.get_text()))
            params = {'x':0.0, 'y':0.0, 'i':0.0, 'j':0.0,}
            used = ''
            for p in parts:
                for n in 'xyij':
                    if n in p:
                        if n == 'x':
                            params['x'] = float(p.strip(n))
                            used += 'x'
                        elif n == 'y':
                            params['y'] = float(p.strip(n))
                            used += 'y'
                        elif n == 'i':
                            params['i'] = float(p.strip(n))
                            used += 'i'
                        elif n == 'j':
                            params['j'] = float(p.strip(n))
                            used += 'j'
            newLine = ('{}'.format(parts[0]))
            if not 'x' in used and not 'y' in used:
                self.dialog_error('Cannot decipher G-Code correctly')
                return None
            if 'x' in used:
                newLine += (' x{:.6f}'.format(params['x'] * math.cos(angle) - params['y'] * math.sin(angle) + float(self.xOEntry.get_text())))
            if 'y' in used:
                newLine += (' y{:.6f}'.format(params['y'] * math.cos(angle) + params['x'] * math.sin(angle) + float(self.yOEntry.get_text())))
            if parts[0] in {'g2', 'g3'}:
                newLine += (' i{:.6f}'.format(params['i'] * math.cos(angle) - params['j'] * math.sin(angle)))
                newLine += (' j{:.6f}'.format(params['j'] * math.cos(angle) + params['i'] * math.sin(angle)))
            return ('{}\n'.format(newLine))

    def data_change(self, widget):
        self.data_changed = True

    def do_rotate(self, fWizard, inFile, tmpDir):
        self.tmpDir = tmpDir
        self.fWizard = fWizard
        self.fOriginal = '{}/original.ngc'.format(tmpDir)
        self.fNgc = '{}/shape.ngc'.format(self.tmpDir)
        shutil.copyfile(inFile, self.fOriginal)
        self.W = gtk.Dialog('Rotate',
                            None,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        self.W.set_default_size(250, 200)
        t = gtk.Table(1, 1, True)
        t.set_row_spacings(6)
        self.W.vbox.add(t)
        aLabel = gtk.Label('Angle')
        aLabel.set_alignment(0.95, 0.5)
        aLabel.set_width_chars(10)
        t.attach(aLabel, 0, 1, 0, 1)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(10)
        self.aEntry.set_text('90')
        self.aEntry.connect('changed', self.data_change)
        t.attach(self.aEntry, 1, 2, 0, 1)
        xOLabel = gtk.Label('X Offset')
        xOLabel.set_alignment(0.95, 0.5)
        xOLabel.set_justify(gtk.JUSTIFY_RIGHT)
        xOLabel.set_width_chars(10)
        t.attach(xOLabel, 0, 1, 1, 2)
        self.xOEntry = gtk.Entry()
        self.xOEntry.set_width_chars(10)
        self.xOEntry.set_text('0')
        self.xOEntry.connect('changed', self.data_change)
        t.attach(self.xOEntry, 1, 2, 1, 2)
        yOLabel = gtk.Label('Y Offset')
        yOLabel.set_alignment(0.95, 0.5)
        yOLabel.set_width_chars(10)
        t.attach(yOLabel, 0, 1, 2, 3)
        self.yOEntry = gtk.Entry()
        self.yOEntry.set_width_chars(10)
        self.yOEntry.set_text('0')
        self.yOEntry.connect('changed', self.data_change)
        t.attach(self.yOEntry, 1, 2, 2, 3)
        self.preview = gtk.Button('Preview')
        self.preview.connect('pressed', self.preview_rotate)
        t.attach(self.preview, 1, 2, 3, 4)
        self.accept = gtk.Button('Accept')
        self.accept.connect('pressed', self.accept_rotate)
        self.accept.set_sensitive(False)
        t.attach(self.accept, 2, 3, 3, 4)
        cancel = gtk.Button('Cancel')
        cancel.connect('pressed', self.cancel)
        t.attach(cancel, 3, 4, 3, 4)
        self.aEntry.grab_focus()
        self.W.show_all()
        response = self.W.run()
