'''
conv_block.py

Copyright (C) 2020, 2021, 2022  Phillip A Carter
Copyright (C) 2020, 2021, 2022  Gregory D Carl

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

import os, sys
import gettext
from importlib import reload
from plasmac import block as BLOCK

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f"{f.split('/lib')[0]}",'share','locale')
        break
gettext.install('linuxcnc', localedir=localeDir)

def preview(self):
    error = BLOCK.preview(self, self.fNgc, self.fTmp, \
            self.cnValue.get(), self.rnValue.get(), self.coValue.get(), self.roValue.get(), \
            self.xsValue.get(), self.ysValue.get(), self.aValue.get(), self.scValue.get(), \
            self.rtValue.get(), self.convBlock, self.convMirror, self.convFlip, \
            self.convMirrorToggle, self.convFlipToggle, self.g5xIndex, self.convUnits)
    if error:
        self.dialog_show_ok(_('Block Error'), error)
    else:
        self.fileOpener(self.fNgc, True, False)
        self.addC['state'] = 'normal'
        self.undoC['state'] = 'normal'
        self.preview_button_pressed(True)
        self.convBlock[0] = True
        self.convMirrorToggle = False
        self.convFlipToggle = False

def auto_preview(self):
    if not self.cnValue.get() or not self.rnValue.get() or not self.scValue.get(): return
    if int(self.cnValue.get()) > 1 and not self.coValue.get(): return
    if int(self.rnValue.get()) > 1 and not self.roValue.get(): return
    if not float(self.scValue.get()): return
    if self.cnValue.get() or self.rnValue.get():
        preview(self)

def mirror_shape(self):
    self.convMirror = -1 if self.convMirror == 1 else 1
    self.convMirrorToggle = True
    preview(self)

def flip_shape(self):
    self.convFlip = -1 if self.convFlip == 1 else 1
    self.convFlipToggle = True
    preview(self)

def get_parameters(self):
    self.wcs_rotation('get')
    inCode = open(self.fNgc, 'r')
    self.convBlock = [False, False]
#    isConvBlock = False
    self.convUnits = [1, None]
    self.convMirror = 1
    self.convMirrorToggle = False
    self.convFlip = 1
    self.convFlipToggle = False
    for line in inCode:
        line = line.strip().lower()
        # maybe check here for old style rotate, scale, and array
        if line.startswith(';conversational block'):
            self.convBlock = [True, True]
#            isConvBlock = True
        elif 'G21' in line.upper().replace(' ', '') and self.unitsPerMm != 1:
            self.convUnits = [25.4, 'G21']
        elif 'G20' in line.upper().replace(' ', '') and self.unitsPerMm == 1:
            self.convUnits = [0.03937, 'G20']
        elif 'm3' in line:
            break
    inCode.seek(0, 0)
    if self.convBlock[0]:
#    if isConvBlock:
        for line in inCode:
            line = line.strip().lower()
            if line.startswith('#<array_x_offset>'):
                self.coValue.set(f"{float(line.split('=')[1].strip()):0.4f}")
            elif line.startswith('#<array_y_offset>'):
                self.roValue.set(f"{float(line.split('=')[1].strip()):0.4f}")
            elif line.startswith('#<array_columns>'):
                self.cnValue.set(line.split('=')[1].strip())
            elif line.startswith('#<array_rows>'):
                self.rnValue.set(line.split('=')[1].strip())
            elif line.startswith('#<origin_x_offset>'):
                self.xsValue.set(f"{float(line.split('=')[1].strip()):0.4f}")
            elif line.startswith('#<origin_y_offset>'):
                self.ysValue.set(f"{float(line.split('=')[1].strip()):0.4f}")
            elif line.startswith('#<array_angle>'):
                self.aValue.set(line.split('=')[1].strip())
            elif line.startswith('#<blk_scale>'):
                self.scValue.set(line.split('=')[1].strip())
            elif line.startswith('#<shape_angle>'):
                self.rtValue.set(line.split('=')[1].strip())
            elif line.startswith('#<shape_mirror>'):
                self.convMirror = int(line.split('=')[1].strip())
            elif line.startswith('#<shape_flip>'):
                self.convFlip = int(line.split('=')[1].strip())
            elif 'm3' in line:
                break
    inCode.seek(0, 0)

def widgets(self):
    if self.comp['development']:
        reload(BLOCK)
    #starting parameters
#    self.addC['state'] = 'disabled'
    #connections
    self.previewC['command'] = lambda:preview(self)
    self.mirror['command'] = lambda:mirror_shape(self)
    self.flip['command'] = lambda:flip_shape(self)
    #add to layout
    self.ccLabel.grid(column=0, row=0, pady=(4,0), columnspan=4, sticky='ew')
    self.cnLabel.grid(column=0, row=1, pady=(4,0), sticky='e')
    self.cnEntry.grid(column=1, row=1, pady=(4,0))
    self.coLabel.grid(column=2, row=1, pady=(4,0), sticky='e')
    self.coEntry.grid(column=3, row=1, pady=(4,0))
    self.rrLabel.grid(column=0, row=2, pady=(14,0), columnspan=4, sticky='ew')
    self.rnLabel.grid(column=0, row=3, pady=(4,0), sticky='e')
    self.rnEntry.grid(column=1, row=3, pady=(4,0))
    self.roLabel.grid(column=2, row=3, pady=(4,0), sticky='e')
    self.roEntry.grid(column=3, row=3, pady=(4,0))
    self.bsLabel.grid(column=0, row=4, pady=(14,0), columnspan=4, sticky='ew')
    self.scLabel.grid(column=0, row=5, pady=(4,0), sticky='e')
    self.scEntry.grid(column=1, row=5, pady=(4,0))
    self.rtLabel.grid(column=2, row=5, pady=(4,0), sticky='e')
    self.rtEntry.grid(column=3, row=5, pady=(4,0))
    self.mirror.grid(column=1, row=6, pady=(4,0))
    self.flip.grid(column=3, row=6, pady=(4,0))
    self.oLabel.grid(column=0, row=7, pady=(14,0), columnspan=4, sticky='ew')
    self.xsLabel.grid(column=0, row=8, pady=(4,0), sticky='e')
    self.xsEntry.grid(column=1, row=8, pady=(4,0))
    self.ysLabel.grid(column=2, row=8, pady=(4,0), sticky='e')
    self.ysEntry.grid(column=3, row=8, pady=(4,0))
    self.aLabel.grid(column=0, row=9, pady=(4,0), sticky='e')
    self.aEntry.grid(column=1, row=9, pady=(4,0))
    self.cnEntry.focus()
    self.settingsChanged = False
    get_parameters(self)
    if not self.coValue.get() and self.shapeLen['x'] is not None:
        self.coValue.set(round(self.shapeLen['x'], 2))
    if not self.roValue.get() and self.shapeLen['y'] is not None:
        self.roValue.set(round(self.shapeLen['y'], 2))
