#!/usr/bin/env python3

#------------------------------------------------------------------------------
# Copyright: 2013
# Author:    Dewey Garrett <dgarrett@panix.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#------------------------------------------------------------------------------

"""
# popup keyboard for use with touchscreen applications
# used by pyngcgui.py
# based on work of John Thornton's Buglump

Optional __init__() args:
  glade_file         (default = 'popupkeyboard.ui')
  dialog_name        (default = 'dialog')     Main window
  num_entry_name     (default = 'num_entry')  Entry for display
  coord_buttons_name (default = 'coords')     Box for coord buttons
  use_coord_buttons  (default = True)         Enable coord buttons

Required objects for glade_file:
  Gtk.Window (Main window)
  Gtk.Entry  (Entry for display)

Optional objects for galde_file:
  Gtk.*Box:  (Box for coord buttons)

All buttons use a single handler named 'on_click'.
The PopupKeyboard class recognizes buttons by their LABEL.

Required button LABELS (case insensitive):
  0,1,2,3,4,5,6,7,8,9,. (numbers and decimal point)
  +/-                   (toggle sign)
  clear                 (clear)
  bs                    (back space)
  save                  (save)
  cancel                (cancel)
Optional button LABELS (case insensitive):
  x,y,z,a,b,c,u,v,w,d   (9 axes plus d for diameter)

"""

import linuxcnc
import sys
import os
import gi
gi.require_version('Pango', '1.0')
from gi.repository import Pango
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject
g_ui_dir = linuxcnc.SHARE + "/linuxcnc"

class PopupKeyboard:
    def __init__(self
                ,glade_file=None
                ,dialog_name='dialog'
                ,num_entry_name='num_entry'
                ,coord_buttons_name='coords'
                ,use_coord_buttons=True
                ,theme_name=None
                ):

        if (glade_file is None):
            glade_file = os.path.join(g_ui_dir,'popupkeyboard.ui')

        fontname ='sans 12 bold'
        self.use_coord_buttons = use_coord_buttons

        self.builder = Gtk.Builder()
        self.builder.add_from_file(glade_file)
        self.builder.connect_signals(self)

        self.dialog    = self.builder.get_object(dialog_name)
        self.set_theme(theme_name)
        self.num_entry = self.builder.get_object(num_entry_name)
        try:
            self.coord_buttons = self.builder.get_object(coord_buttons_name)
        except:
            self.coord_buttons = None

        self.result = None
        self.location = None

        self.top = self.dialog.get_toplevel()
        self.top.set_title(glade_file)
        self.top.set_keep_above(True)

        if (not self.use_coord_buttons):
            if self.coord_buttons:
                self.coord_buttons.hide()

        # prevent closing of dialog by window manager, escape key , etc
        # http://faq.pygtk.org/index.py?file=faq10.013.htp&req=show
        self.top.connect("response",    self.on_response) #reqd
        self.top.connect("delete-event",self.on_delete)   #reqd
        self.top.connect("close",       self.on_close)    #probably not reqd

        # find buttons with labels XYZABCUVW or D
        # and show iff corresponding axis is in axis_mask
        self.label_to_btn = {}
        for btn in self.builder.get_objects():
            if type(btn) is not Gtk.Button:
                continue
            self.label_to_btn[btn.get_label().upper()] = btn

            #not working gtk3:
            #if isinstance(btn.child, Gtk.Label):
            #    lbl = btn.child
            #    lbl.modify_font(Pango.FontDescription(fontname))

        if use_coord_buttons: self.support_coord_buttons()

        # making it insensitive clears the initial selection region
        #deprecated in gtk3:
        #self.num_entry.set_state(Gtk.StateType.INSENSITIVE)
        #self.num_entry.modify_text(Gtk.StateType.INSENSITIVE
        #                          ,Gtk.gdk.color_parse('black'))
        #self.num_entry.modify_font(Pango.FontDescription(fontname))

    def support_coord_buttons(self):
        try:
            self.stat = linuxcnc.stat()
            self.stat.poll()
            has_x = False
            for axno in range(0,9):
                axname = 'XYZABCUVW'[axno]
                if axname in self.label_to_btn:
                    b = self.label_to_btn[axname]
                    if bool(self.stat.axis_mask & (1 << axno)):
                        b.show()
                        if axno == 0: has_x = True
                    else:
                        b.hide()
            bdiam = None
            if 'D' in self.label_to_btn:
                bdiam = self.label_to_btn['D']
            if bdiam and has_x:
                bdiam.show()
            elif bdiam:
                bdiam.hide()
        except linuxcnc.error as msg:
            self.stat = None
            if self.coord_buttons is not None:
                self.coord_buttons.hide()
                print("linuxcnc must be running to use coordinate keys")
            # continue without buttons for testing when linuxnc not running
        except Exception as err:
            print('Exception:',Exception)
            print(sys.exc_info())
            sys.exit(1)


    def set_theme(self,tname=None):
        if tname is None:
            return
        screen   = self.dialog.get_screen()
        #wip: gtk3 notworking
        #settings = Gtk.settings_get_for_screen(screen)
        #settings.set_string_property('gtk-theme-name',tname,"")
        #theme    = settings.get_property('gtk-theme-name')

    # prevent closing of dialog by window manager, escape key , etc
    def on_response(self,widget,response):
        if response < 0:
            GObject.signal_stop_emission_by_name(widget,'response')
        return True

    def on_delete(self,widget,event): return True

    def on_close(self,widget,event): return True

    def run(self,initial_value='',title=None):
        if title is not None:
            self.top.set_title(title)
        self.num_entry.set_text(str(initial_value))
        if self.location:
            #Gtk3:deprecated:
            #self.dialog.parse_geometry('+%d+%d'
            #                          % (self.location[0],self.location[1]))
            pass
        self.num_entry.set_position(0)
        self.dialog.run()
        if self.result is None:
            return False #user canceled
        else:
            return True

    def on_click(self, widget, data=None):
        l = widget.get_label()
        e = self.num_entry
        self.label_to_method(l)(e,l)

    def label_to_method(self,l):
        ll = l.lower()
        if ll.find('clear') >= 0:  return self.do_clear
        if ll.find('save')  >= 0:  return self.do_save
        if ll.find('cancel')>= 0:  return self.do_cancel
        if ll.find('+/-')   >= 0:  return self.do_sign
        if ll.find('bs')    >= 0:  return self.do_backspace
        if ll in  ('.0123456789'): return self.do_number
        if ll in  ('xyzabcuvwd'):  return self.do_axis_letter
        return self.do_unknown_label

    def do_unknown_label(self,e,l):
        print('PopupKeyboard:do_unknown_label: <%s>' % l)

    def do_number(self,e,l):
        # not needed if INSENSITIVE:
        # if e.get_selection_bounds(): e.delete_selection()
        e.set_text(e.get_text() + l)

    def do_backspace(self,e,l):
        e.set_text(e.get_text()[:-1])

    def do_sign(self,e,l):
        current = e.get_text()
        if current == '':
            current = '-'
        elif current[0] == '-':
            current = current[1:]
        else:
            current = '-' + current
        e.set_text(current)

    def do_save(self,e,l):
        self.result = e.get_text()
        e.set_text('')
        self.location = self.dialog.get_position()
        self.dialog.hide() # tells dialog.run() to finish

    def do_cancel(self,e,l):
        self.result = None # None means canceled
        self.dialog.hide() # tells dialog.run() to finish

    def do_clear(self,e,l):
        e.set_text('')

    def do_axis_letter(self,e,l):
        if self.stat:
            self.stat.poll()
            e.set_text("%.6g" % self.coord_value(l))
        else:
            print("linuxcnc must be running to use <%s> key" % l)

    def get_result(self):
        return(self.result)

    def coord_value(self,char):
        # offset calc copied from emc_interface.py
        # char = 'x' | 'y' | ...
        # 'd' is for diameter
        s = self.stat
        s.poll()
        p = s.position # tuple=(xvalue, yvalue, ...
        return {
                'x': p[0] - s.g5x_offset[0] - s.tool_offset[0],
                'y': p[1] - s.g5x_offset[1] - s.tool_offset[1],
                'z': p[2] - s.g5x_offset[2] - s.tool_offset[2],
                'a': p[3] - s.g5x_offset[3] - s.tool_offset[3],
                'b': p[4] - s.g5x_offset[4] - s.tool_offset[4],
                'c': p[5] - s.g5x_offset[5] - s.tool_offset[5],
                'u': p[6] - s.g5x_offset[6] - s.tool_offset[6],
                'v': p[7] - s.g5x_offset[7] - s.tool_offset[7],
                'w': p[8] - s.g5x_offset[8] - s.tool_offset[8],
                'd':(p[0] - s.g5x_offset[0] - s.tool_offset[0])* 2,#2*R
               }[char.lower()]


if __name__ == "__main__":
    m = PopupKeyboard()
    print("\nClear and Save to end test\n")
    ct = 100
    while True:
        m.run(initial_value=''
             ,title=str(ct)
             )
        result = m.get_result()
        print('result = <%s>' % result)
        if result=='':
            sys.exit(0)
        ct += 1
    Gtk.main()
# vim: sts=4 sw=4 et
