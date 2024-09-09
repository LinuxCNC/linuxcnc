'''
conv_triangle.py

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
from plasmac import triangle as TRIANGLE

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f"{f.split('/lib')[0]}",'share','locale')
        break
gettext.install('linuxcnc', localedir=localeDir)

def preview(self):
    matNum = int(self.matCombo.get().split(':')[0])
    matNam = self.matCombo.get().split(':')[1].strip()
    isExternal = self.ctbValue.get() == _('EXTERNAL')
    kerfWidth = self.materials[matNum]['kerf_width']
    error = TRIANGLE.preview(self, self.fTmp, self.fNgc, self.fNgcBkp, \
            matNum, matNam, \
            self.preAmble, self.postAmble, \
            self.liValue.get(), self.loValue.get(), \
            self.xsValue.get(), self.ysValue.get(), \
            kerfWidth, isExternal, \
            self.aaValue.get(), self.baValue.get(), self.caValue.get(), \
            self.alValue.get(), self.blValue.get(), self.clValue.get(), \
            self.aValue.get())
    if error:
        self.dialog_show_ok(_('Triangle Error'), error)
    else:
        self.fileOpener(self.fNgc, True, False)
        self.addC['state'] = 'normal'
        self.undoC['state'] = 'normal'
        self.preview_button_pressed(True)

def auto_preview(self):
    if (self.aaValue.get() and self.baValue.get() and self.caValue.get() and \
       (self.alValue.get() or self.blValue.get() or self.clValue.get())) or \
       (self.aaValue.get() and self.blValue.get() and self.clValue.get()) or \
       (self.alValue.get() and self.baValue.get() and self.clValue.get()) or \
       (self.alValue.get() and self.blValue.get() and self.caValue.get()) or \
       (self.alValue.get() and self.blValue.get() and self.clValue.get()):
        preview(self)

def widgets(self):
    if self.comp['development']:
        reload(TRIANGLE)
    # start settings
    if not self.settingsExited:
        self.aaValue.set('')
        self.baValue.set('')
        self.caValue.set('')
        self.alValue.set('')
        self.blValue.set('')
        self.clValue.set('')
        self.aValue.set('')
    # connections
    self.ctButton['command'] = lambda:self.cut_type_clicked()
    self.previewC['command'] = lambda:preview(self)
    # add to layout
    self.matLabel.grid(column=0, row=0, pady=(4,0), sticky='e')
    self.matCombo.grid(column=1, row=0, pady=(4,0), columnspan=3, sticky='ew')
    self.ctLabel.grid(column=2, row=1, pady=(4,0), sticky='e')
    self.ctButton.grid(column=3, row=1, pady=(4,0))
    self.xsLabel.grid(column=0, row=2, pady=(4,0), sticky='e')
    self.xsEntry.grid(column=1, row=2, pady=(4,0))
    self.ysLabel.grid(column=2, row=2, pady=(4,0), sticky='e')
    self.ysEntry.grid(column=3, row=2, pady=(4,0))
    self.liLabel.grid(column=0, row=3, pady=(4,0), sticky='e')
    self.liEntry.grid(column=1, row=3, pady=(4,0))
    self.loLabel.grid(column=2, row=3, pady=(4,0), sticky='e')
    self.loEntry.grid(column=3, row=3, pady=(4,0))
    self.aaLabel.grid(column=0, row=4, pady=(4,0), sticky='e')
    self.aaEntry.grid(column=1, row=4, pady=(4,0))
    self.alLabel.grid(column=2, row=4, pady=(4,0), sticky='e')
    self.alEntry.grid(column=3, row=4, pady=(4,0))
    self.baLabel.grid(column=0, row=5, pady=(4,0), sticky='e')
    self.baEntry.grid(column=1, row=5, pady=(4,0))
    self.blLabel.grid(column=2, row=5, pady=(4,0), sticky='e')
    self.blEntry.grid(column=3, row=5, pady=(4,0))
    self.caLabel.grid(column=0, row=6, pady=(4,0), sticky='e')
    self.caEntry.grid(column=1, row=6, pady=(4,0))
    self.clLabel.grid(column=2, row=6, pady=(4,0), sticky='e')
    self.clEntry.grid(column=3, row=6, pady=(4,0))
    self.aLabel.grid(column=0, row=7, pady=(4,0), sticky='e')
    self.aEntry.grid(column=1, row=7, pady=(4,0))
    # finish setup
    self.aaEntry.focus()
    self.settingsExited = False
