'''
conv_settings.py

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

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f"{f.split('/lib')[0]}",'share','locale')
        break
gettext.install('linuxcnc', localedir=localeDir)

def save_clicked(self):
    msg = []
    self.preAmble = self.preValue.get()
    self.postAmble = self.pstValue.get()
    self.origin = self.spbValue.get() == 'CENTER'
    error = ''
    valid, self.leadIn = self.conv_is_float(self.liValue.get())
    if not valid:
        msg = _('Invalid LEAD IN entry detected')
        error += f"{msg}\n\n"
    valid, self.leadOut = self.conv_is_float(self.loValue.get())
    if not valid:
        msg = _('Invalid LEAD OUT entry detected')
        error += f"{msg}\n\n"
    valid, self.smallHoleDia = self.conv_is_float(self.shValue.get())
    if not valid:
        msg = _('Invalid DIAMETER entry detected')
        error += f"{msg}\n\n"
    valid, self.smallHoleSpeed = self.conv_is_int(self.hsValue.get())
    if not valid:
        msg = _('Invalid SPEED % entry detected')
        error += f"{msg}\n\n"
    if error:
        self.dialog_show_ok(_('Settings Error'), error)
        return
    self.putPrefs(self.prefs, 'CONVERSATIONAL', 'Preamble', self.preAmble, str)
    self.putPrefs(self.prefs, 'CONVERSATIONAL', 'Postamble', self.postAmble, str)
    self.putPrefs(self.prefs, 'CONVERSATIONAL', 'Origin', self.origin, int)
    self.putPrefs(self.prefs, 'CONVERSATIONAL', 'Leadin', self.leadIn, float)
    self.putPrefs(self.prefs, 'CONVERSATIONAL', 'Leadout', self.leadOut, float)
    self.putPrefs(self.prefs, 'CONVERSATIONAL', 'Hole diameter', self.smallHoleDia, float)
    self.putPrefs(self.prefs, 'CONVERSATIONAL', 'Hole speed', self.smallHoleSpeed, int)
    self.savedSettings['lead_in'] = self.liValue.get()
    self.savedSettings['lead_out'] = self.loValue.get()
    self.savedSettings['start_pos'] = self.spbValue.get()
    self.settingsExited = 1

def reload_clicked(self):
    load(self, self.preAmble, self.leadIn, self.smallHoleDia, self.postAmble)
    show(self)
    if not self.settingsExited:
        self.settingsExited = 2

def exit_clicked(self):
    for child in self.toolFrame.winfo_children():
        if child.winfo_class() == 'Radiobutton':
            child['state'] = 'normal'
    self.restore_buttons()
    if not self.settingsExited:
        self.settingsExited = 3
    self.shape_request(self.oldConvButton)

def load(self, preAmble, leadin, smallHole, postAmble=None):
    postAmble = postAmble if postAmble else preAmble
    self.preAmble = self.getPrefs(self.prefs, 'CONVERSATIONAL', 'Preamble', preAmble, str)
    self.postAmble = self.getPrefs(self.prefs, 'CONVERSATIONAL', 'Postamble', postAmble, str)
    self.origin = self.getPrefs(self.prefs, 'CONVERSATIONAL', 'Origin', 0, int)
    self.leadIn = self.getPrefs(self.prefs, 'CONVERSATIONAL', 'Leadin', leadin, float)
    self.leadOut = self.getPrefs(self.prefs, 'CONVERSATIONAL', 'Leadout', 0, float)
    self.smallHoleDia = self.getPrefs(self.prefs, 'CONVERSATIONAL', 'Hole diameter', smallHole, float)
    self.smallHoleSpeed = self.getPrefs(self.prefs, 'CONVERSATIONAL', 'Hole speed', 60, int)

def show(self):
    self.preValue.set(self.preAmble)
    self.pstValue.set(self.postAmble)
    self.liValue.set(self.leadIn)
    self.loValue.set(self.leadOut)
    self.shValue.set(f"{self.smallHoleDia}")
    self.hsValue.set(f"{self.smallHoleSpeed}")
    if self.origin:
        self.spbValue.set('CENTER')
    else:
        self.spbValue.set('BTM LEFT')

def widgets(self):
    # connections
    self.spButton['command'] = lambda:self.start_point_clicked()
    self.saveS['command'] = lambda:save_clicked(self)
    self.reloadS['command'] = lambda:reload_clicked(self)
    self.exitS['command'] = lambda:exit_clicked(self)
    # remove common widgets
    self.previewC.grid_remove()
    self.addC.grid_remove()
    self.undoC.grid_remove()
    # add to layout
    self.preLabel.grid(column=0, row=0, pady=(4,0), sticky='e')
    self.preEntry.grid(column=1, row=0, pady=(4,0), sticky='ew', columnspan=3)
    self.pstLabel.grid(column=0, row=1, pady=(4,0), sticky='e')
    self.pstEntry.grid(column=1, row=1, pady=(4,0), sticky='ew', columnspan=3)
    self.llLabel.grid(column=0, row=2, pady=(4,0), sticky='ew', columnspan=4)
    self.liLabel.grid(column=0, row=3, pady=(4,0), sticky='e')
    self.liEntry.grid(column=1, row=3, pady=(4,0), sticky='w')
    self.loLabel.grid(column=2, row=3, pady=(4,0), sticky='e')
    self.loEntry.grid(column=3, row=3, pady=(4,0), sticky='w')
    self.shLabel.grid(column=0, row=4, pady=(4,0), sticky='ew', columnspan=4)
    self.dLabel.grid(column=0, row=5, pady=(4,0), sticky='e')
    self.shEntry.grid(column=1, row=5, pady=(4,0), sticky='w')
    self.hsLabel.grid(column=2, row=5, pady=(4,0), sticky='e')
    self.hsEntry.grid(column=3, row=5, pady=(4,0), sticky='w')
    self.pvLabel.grid(column=0, row=6, pady=(4,0), sticky='ew', columnspan=4)
    self.spLabel.grid(column=0, row=7, pady=(4,0), sticky='e')
    self.spButton.grid(column=1, row=7, pady=(4,0), sticky='w')
    self.saveS.grid(column=0, row=12)
    self.reloadS.grid(column=1, row=12, columnspan=2)
    self.exitS.grid(column=3, row=12)
    # finish setup
    self.preEntry.focus()
