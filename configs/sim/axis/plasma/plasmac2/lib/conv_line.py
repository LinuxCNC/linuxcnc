'''
conv_line.py

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
from plasmac import line as LINE

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f"{f.split('/lib')[0]}",'share','locale')
        break
gettext.install('linuxcnc', localedir=localeDir)

def preview(self):
    if self.convLine['xLineEnd'] != self.conv_is_float(self.l1Value.get()) or \
       self.convLine['yLineEnd'] != self.conv_is_float(self.l2Value.get()):
        self.convLine['addSegment'] = 0
    if self.lineCombo.get() == _('LINE POINT ~ POINT'):
        error = LINE.do_line_point_to_point(self, self.l1Value.get(), self.l2Value.get(), \
                                            self.l3Value.get(), self.l4Value.get())
        if not error[0]:
            self.convLine['xLineEnd'] = error[1]
            self.convLine['yLineEnd'] = error[2]
            self.convLine['gcodeLine'] = error[3]
        else:
            error_set(self, f'{error[1]}\n')
            return
    elif self.lineCombo.get() == _('LINE BY ANGLE'):
        error = LINE.do_line_by_angle(self, self.l1Value.get(), self.l2Value.get(), \
                                      self.l3Value.get(), self.l4Value.get())
        if not error[0]:
            self.convLine['xLineEnd'] = error[1]
            self.convLine['yLineEnd'] = error[2]
            self.convLine['gcodeLine'] = error[3]
        else:
            error_set(self, f'{error[1]}\n')
            return
    elif self.lineCombo.get() == _('ARC 3P'):
        error = LINE.do_arc_3_points(self, self.l1Value.get(), self.l2Value.get(), \
                                     self.l3Value.get(), self.l4Value.get(), \
                                     self.l5Value.get(), self.l6Value.get())
        if not error[0]:
            self.convLine['xLineEnd'] = error[1]
            self.convLine['yLineEnd'] = error[2]
            self.convLine['gcodeLine'] = error[3]
        else:
            error_set(self, f'{error[1]}\n')
            return
    elif self.lineCombo.get() == _('ARC 2P +RADIUS'):
        arcType = '3' if 'CCW' in self.g23bValue.get() else '2'
        error = LINE.do_arc_2_points_radius(self, self.l1Value.get(), self.l2Value.get(), self.l3Value.get(), \
                                                  self.l4Value.get(), self.l5Value.get(), arcType)
        if not error[0]:
            self.convLine['xLineEnd'] = error[1]
            self.convLine['yLineEnd'] = error[2]
            self.convLine['gcodeLine'] = error[3]
        else:
            error_set(self, f'{error[1]}\n')
            return
    elif self.lineCombo.get() == _('ARC ANGLE +RADIUS'):
        arcType = '3' if 'CCW' in self.g23bValue.get() else '2'
        error = LINE.do_arc_by_angle_radius(self, self.l1Value.get(), self.l2Value.get(), self.l3Value.get(),
                                                  self.l4Value.get(), self.l5Value.get(), arcType)
        if not error[0]:
            self.convLine['xLineEnd'] = error[1]
            self.convLine['yLineEnd'] = error[2]
            self.convLine['gcodeLine'] = error[3]
        else:
            error_set(self, f'{error[1]}\n')
            return
    if self.convLine['addSegment'] == 1:
        LINE.next_segment(self.fTmp, self.fNgc)
    else:
        valid, self.convLine['xLineStart'] = self.conv_is_float(self.l1Value.get())
        valid, self.convLine['yLineStart'] = self.conv_is_float(self.l2Value.get())
        LINE.first_segment(self.fTmp, self.fNgc, self.fNgcBkp, self.preAmble, \
                           self.lineCombo.get(), self.convLine['xLineStart'], self.convLine['yLineStart'], \
                           int(self.matCombo.get().split(':')[0]), \
                           self.matCombo.get().split(':')[1].strip())
    LINE.last_segment(self.fTmp, self.fNgc, self.convLine['gcodeLine'], self.postAmble)
    self.fileOpener(self.fNgc, True, False)
    self.addC['state'] = 'normal'
    self.undoC['state'] = 'normal'
    self.preview_button_pressed(True)
    if self.convLine['addSegment'] == 1:
        self.convLine['addSegment'] = 2
    self.previewActive = True

def auto_preview(self):
    if self.l1Value.get() and self.l2Value.get() and self.l3Value.get() and self.l4Value.get():
        if self.lineCombo.get() == _('LINE POINT ~ POINT') or \
           self.lineCombo.get() == _('LINE BY ANGLE') or \
           (self.lineCombo.get() == _('ARC 3P') and self.l5Value.get() and self.l6Value.get()) or \
           (self.lineCombo.get() == _('ARC 2P +RADIUS') and self.l5Value.get()) or \
           (self.lineCombo.get() == _('ARC ANGLE +RADIUS') and self.l5Value.get()):
            preview(self)

def arc_type_changed(self):
    text = 'CW - G2' if 'CCW' in self.g23bValue.get() else 'CCW - G3'
    self.g23bValue.set(text)
    auto_preview(self)

def line_type_changed(self, refresh):
#    self.lineCombo.selection_clear()
    if self.lineCombo.get() == _('LINE POINT ~ POINT'):
        if not refresh:
            set_line_point_to_point(self)
    elif self.lineCombo.get() == _('LINE BY ANGLE'):
        if not refresh:
            set_line_by_angle(self)
    elif self.lineCombo.get() == _('ARC 3P'):
        if not refresh:
            set_arc_3_points(self)
    elif self.lineCombo.get() == _('ARC 2P +RADIUS'):
        if not refresh:
            set_arc_2_points_radius(self)
    elif self.lineCombo.get() == _('ARC ANGLE +RADIUS'):
        if not refresh:
            set_arc_by_angle_radius(self)
    self.l3Entry.focus()

def clear_widgets(self):
    set_start_point(self)
    for w in [self.l3Label, self.l4Label, self.l5Label, self.l6Label, ]:
        w['text'] = ''
    for w in [self.l3Entry, self.l4Entry, self.l5Entry, self.l6Entry, ]:
        w.grid_remove()
    self.g23Arc.grid_remove()

def set_start_point(self):
    text = _('START')
    self.l1Label['text'] = _('X START')
    self.l1Value.set(f'{self.convLine["xLineStart"]:0.3f}')
    self.l2Label['text'] = _('Y START')
    self.l2Value.set(f'{self.convLine["yLineStart"]:0.3f}')

def set_line_point_to_point(self):
    clear_widgets(self)
    self.l3Label['text'] = _('X END')
    self.l4Label['text'] = _('Y END')
    for w in [self.l3Value, self.l4Value]:
        w.set('')
    self.l3Entry.grid(column=1, row=4, pady=(24,0))
    self.l4Entry.grid(column=1, row=5, pady=(4,0))

def set_line_by_angle(self):
    clear_widgets(self)
    self.l3Label['text'] = _('LENGTH')
    self.l4Label['text'] = _('ANGLE')
    self.l4Value.set('0.000')
    for w in [self.l3Value, self.l4Value]:
        w.set('')
    self.l3Entry.grid(column=1, row=4, pady=(24,0))
    self.l4Entry.grid(column=1, row=5, pady=(4,0))

def set_arc_3_points(self):
    clear_widgets(self)
    self.l3Label['text'] = _('X NEXT')
    self.l4Label['text'] = _('Y NEXT')
    self.l5Label['text'] = _('X END')
    self.l6Label['text'] = _('Y END')
    for w in [self.l3Value, self.l4Value, self.l5Value, self.l6Value]:
        w.set('')
    self.l3Entry.grid(column=1, row=4, pady=(24,0))
    self.l4Entry.grid(column=1, row=5, pady=(4,0))
    self.l5Entry.grid(column=1, row=6, pady=(24,0))
    self.l6Entry.grid(column=1, row=7, pady=(4,0))

def set_arc_2_points_radius(self):
    clear_widgets(self)
    self.l3Label['text'] = _('X END')
    self.l4Label['text'] = _('Y END')
    self.l5Label['text'] = _('RADIUS')
    self.l5Value.set('0.000')
    self.l6Label['text'] = _('DIRECTION')
    for w in [self.l3Value, self.l4Value, self.l5Value]:
        w.set('')
    self.l3Entry.grid(column=1, row=4, pady=(24,0))
    self.l4Entry.grid(column=1, row=5, pady=(4,0))
    self.l5Entry.grid(column=1, row=6, pady=(24,0))
    self.g23Arc.grid(column=1, row=7, pady=(4,0))

def set_arc_by_angle_radius(self):
    clear_widgets(self)
    self.l3Label['text'] = _('LENGTH')
    self.l4Label['text'] = _('ANGLE')
    self.l4Value.set('0.000')
    self.l5Label['text'] = _('RADIUS')
    self.l6Label['text'] = _('DIRECTION')
    for w in [self.l3Value, self.l4Value, self.l5Value]:
        w.set('')
    self.l3Entry.grid(column=1, row=4, pady=(24,0))
    self.l4Entry.grid(column=1, row=5, pady=(4,0))
    self.l5Entry.grid(column=1, row=6, pady=(24,0))
    self.g23Arc.grid(column=1, row=7, pady=(4,0))

def error_set(self, error):
    self.dialog_show_ok(_('Line Error'), error)

def widgets(self):
    if self.comp['development']:
        reload(LINE)
    # start settings
    self.previewActive = False
    if not self.settingsExited:
        self.dValue.set('')
        self.convLine['xLineStart'] = 0.0
        self.convLine['yLineStart'] = 0.0
        self.convLine['addSegment'] = 0
        self.convLine['gcodeSave'] = ''
    self.dLabel['text'] = _('DIAMETER')
    #connections
    self.previewC['command'] = lambda:preview(self)
    self.g23Arc['command'] = lambda:arc_type_changed(self)
    self.lineCombo['modifycmd'] = lambda:line_type_changed(self, False)
    #add to layout
    self.matLabel.grid(column=0, row=0, pady=(4,0), sticky='e')
    self.matCombo.grid(column=1, row=0, padx=(4,0), pady=(4,0), columnspan=3, sticky='ew')
    self.lnLabel.grid(column=0, row=1, pady=(4,0), sticky='e')
    self.lineCombo.grid(column=1, row=1, padx=(4,0), pady=(4,0), columnspan=2, sticky='ew')
    self.l1Label.grid(column=0, row=2, pady=(4,0), sticky='e')
    self.l1Entry.grid(column=1, row=2, pady=(4,0))
    self.l2Label.grid(column=0, row=3, pady=(4,0), sticky='e')
    self.l2Entry.grid(column=1, row=3, pady=(4,0))
    self.l3Label.grid(column=0, row=4, pady=(24,0), sticky='e')
    self.l3Entry.grid(column=1, row=4, pady=(4,0))
    self.l4Label.grid(column=0, row=5, pady=(4,0), sticky='e')
    self.l4Entry.grid(column=1, row=5, pady=(4,0))
    self.l5Label.grid(column=0, row=6, pady=(24,0), sticky='e')
    self.l5Entry.grid(column=1, row=6, pady=(4,0))
    self.l6Label.grid(column=0, row=7, pady=(4,0), sticky='e')
    self.l6Entry.grid(column=1, row=7, pady=(4,0))
    # finish setup
    if not self.settingsExited:
        line_type_changed(self, False)
    self.settingsExited = False
