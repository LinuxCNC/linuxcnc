'''
conv_circle.py

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
from plasmac import circle as CIRCLE

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
    isCenter = self.spbValue.get() == _('CENTER')
    isExternal = self.ctbValue.get() == _('EXTERNAL')
    isOvercut = self.ocEntry['state'] == 'normal' and self.ocbValue.get() == _('ON')
    kerfWidth = self.materials[matNum]['kerf_width']
    error = CIRCLE.preview(self, self.fTmp, self.fNgc, self.fNgcBkp, \
            matNum, matNam, \
            self.preAmble, self.postAmble, \
            self.liValue.get(), self.loValue.get(), \
            isCenter, self.xsValue.get(), self.ysValue.get(), \
            kerfWidth, isExternal, \
            isOvercut, self.ocValue.get(), \
            self.smallHoleDia, self.smallHoleSpeed, \
            self.dValue.get(), self.invalidLeads)
    if error:
        self.dialog_show_ok(_('Circle Error'), error)
    else:
        self.fileOpener(self.fNgc, True, False)
        self.addC['state'] = 'normal'
        self.undoC['state'] = 'normal'
        self.preview_button_pressed(True)

def auto_preview(self, button=False):
    if self.dValue.get():
        preview(self)

def overcut_clicked(self):
    if self.ocbValue.get() == _('OFF'):
        self.ocbValue.set(_('ON'))
        try:
            lolen = float(self.loValue.get())
        except:
            lolen = 0
        try:
            dia = float(self.dValue.get())
        except:
            dia = 0
        if (self.ctbValue.get() == _('EXTERNAL') and lolen) or not dia or dia > self.smallHoleDia:
            return
        else:
            self.ocEntry['state'] = 'normal'
    else:
        self.ocbValue.set(_('OFF'))

def widgets(self):
    if self.comp['development']:
        reload(CIRCLE)
    # start settings
    if not self.settingsExited:
        self.ocButton['state'] = 'disabled'
        self.dValue.set('')
    self.dLabel['text'] = _('DIAMETER')
    #connections
    self.ctButton['command'] = lambda:self.cut_type_clicked(True)
    self.spButton['command'] = lambda:self.start_point_clicked()
    self.ocButton['command'] = lambda:overcut_clicked(self)
    self.previewC['command'] = lambda:preview(self)
    #add to layout
    self.matLabel.grid(column=0, row=0, pady=(4,0), sticky='e')
    self.matCombo.grid(column=1, row=0, pady=(4,0), columnspan=3, sticky='ew')
    self.spLabel.grid(column=0, row=1, pady=(4,0), sticky='e')
    self.spButton.grid(column=1, row=1, pady=(4,0))
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
    self.dLabel.grid(column=0, row=4, pady=(4,0), sticky='e')
    self.dEntry.grid(column=1, row=4, pady=(4,0))
    self.ocLabel.grid(column=0, row=5, pady=(4,0), sticky='e')
    self.ocEntry.grid(column=1, row=5, pady=(4,0))
    self.ocButton.grid(column=2, row=5, pady=(4,0))
    # finish setup
    self.dEntry.focus()
    self.settingsExited = False
