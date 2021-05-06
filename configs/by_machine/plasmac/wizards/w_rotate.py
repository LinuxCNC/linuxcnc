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
import math
import linuxcnc
import shutil
import hal
from subprocess import Popen,PIPE
import re

class rotate_wiz:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())
        self.previewed = False

    def rotate_cancel(self, widget):
        shutil.copyfile(self.parent.fNgcBkp, self.parent.fNgc)
        self.parent.preview.load(self.parent.fNgc)
        self.previewed = False

    def rotate_accept(self, widget):
        shutil.copyfile(self.parent.fNgc, self.parent.fNgcBkp)
        self.parent.preview.load(self.parent.fNgc)

    def rotate_preview(self, event):
        if self.aEntry.get_text():
            angle = float(self.aEntry.get_text())
        else:
            angle = 0
        if self.xOEntry.get_text():
            xOffset = float(self.xOEntry.get_text())
        else:
            xOffset = 0
        if self.yOEntry.get_text():
            yOffset = float(self.yOEntry.get_text())
        else:
            yOffset = 0
        outNgc = open(self.parent.fNgc, 'w')
        inCod = open(self.parent.fNgcBkp, 'r')
        for line in inCod:
            if line.strip().lower().startswith('g'):
                rLine = self.rotate_object(angle, xOffset, yOffset, line)
                if rLine is not None:
                    outNgc.write(rLine)
                else:
                    return
            else:
                outNgc.write(line)
        inCod.close()
        outNgc.close()
        self.parent.preview.load(self.parent.fNgc)
        # if self.previewed:
        #     md.destroy()
        self.previewed = True
        self.add.set_sensitive(True)

    def rotate_object(self, angle, xOffset, yOffset, line):
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
            angle = math.radians(angle)
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
                self.parent.dialog_error('ROTATE', 'Cannot decipher G-Code correctly')
                return None
            if 'x' in used:
                newLine += (' x{:.6f}'.format(params['x'] * math.cos(angle) - params['y'] * math.sin(angle) + xOffset))
            if 'y' in used:
                newLine += (' y{:.6f}'.format(params['y'] * math.cos(angle) + params['x'] * math.sin(angle) + yOffset))
            if parts[0] in {'g2', 'g3'}:
                newLine += (' i{:.6f}'.format(params['i'] * math.cos(angle) - params['j'] * math.sin(angle)))
                newLine += (' j{:.6f}'.format(params['j'] * math.cos(angle) + params['i'] * math.sin(angle)))
            return ('{}\n'.format(newLine))

    def rotate_show(self, parent):
        self.parent = parent
        self.parent.entries.set_row_spacings(self.parent.rowSpace)
        for child in self.parent.entries.get_children():
            self.parent.entries.remove(child)
        try:
            shutil.copyfile(inFile, self.parent.fNgcBkp)
        except:
            pass
        aLabel = gtk.Label('Angle')
        aLabel.set_alignment(0.95, 0.5)
        self.parent.entries.attach(aLabel, 1, 2, 1, 2)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(8)
        self.aEntry.set_text('0')
        self.aEntry.connect('activate', self.rotate_preview)
        self.aEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.aEntry, 2, 3, 1, 2)
        xOLabel = gtk.Label('X Offset')
        xOLabel.set_alignment(0.95, 0.5)
        self.parent.entries.attach(xOLabel, 1, 2, 3, 4)
        self.xOEntry = gtk.Entry()
        self.xOEntry.set_width_chars(8)
        self.xOEntry.set_text('0')
        self.xOEntry.connect('activate', self.rotate_preview)
        self.xOEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.xOEntry, 2, 3, 3, 4)
        yOLabel = gtk.Label('Y Offset')
        yOLabel.set_alignment(0.95, 0.5)
        self.parent.entries.attach(yOLabel, 1, 2, 5, 6)
        self.yOEntry = gtk.Entry()
        self.yOEntry.set_width_chars(8)
        self.yOEntry.set_text('0')
        self.yOEntry.connect('activate', self.rotate_preview)
        self.yOEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.yOEntry, 2, 3, 5, 6)
        self.preview = gtk.Button('Preview')
        self.preview.connect('pressed', self.rotate_preview)
        self.parent.entries.attach(self.preview, 0, 1, 12, 13)
        self.add = gtk.Button('Add')
        self.add.connect('pressed', self.rotate_accept)
        self.add.set_sensitive(False)
        self.parent.entries.attach(self.add, 2, 3, 12, 13)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.rotate_cancel)
        self.parent.entries.attach(undo, 4, 5, 12, 13)
        self.lDesc = gtk.Label('Rotating Shape')
        self.lDesc.set_alignment(0.5, 0.5)
        self.lDesc.set_width_chars(8)
        self.parent.entries.attach(self.lDesc, 1, 4, 13, 14)
        self.parent.undo_shape(None, self.add)
        self.parent.W.show_all()
        self.aEntry.grab_focus()
