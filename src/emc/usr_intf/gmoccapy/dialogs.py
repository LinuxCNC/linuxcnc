#!/usr/bin/env python

'''
    This class is used to handle the dialogs from gmoccapy,
    it is just a coppy of a class from gscreen and has been slighly modified

    Copyright 2014 Norbert Schechner
    nieson@web.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

'''

import gtk
import gladevcp
import pango

# This dialog is for unlocking the system tab
# The unlock code number is defined at the top of the page
def system_dialog(self):
    dialog = gtk.Dialog(_("Enter System Unlock Code"),
               self.widgets.window1,
               gtk.DIALOG_DESTROY_WITH_PARENT,
               (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    label = gtk.Label(_("Enter System Unlock Code"))
    label.modify_font(pango.FontDescription("sans 20"))
    calc = gladevcp.Calculator()
    dialog.vbox.pack_start(label)
    dialog.vbox.add(calc)
    calc.set_value("")
    calc.set_property("font", "sans 20")
    calc.set_editable(True)
    calc.entry.connect("activate", lambda w : dialog.emit("response", gtk.RESPONSE_ACCEPT))
    dialog.parse_geometry("400x400")
    dialog.set_decorated(True)
    dialog.show_all()
    response = dialog.run()
    code = calc.get_value()
    dialog.destroy()
    if response == gtk.RESPONSE_ACCEPT:
        if code == int(self.unlock_code):
            return True
    return False


def entry_dialog(self, data = None, header = _("Enter value") , label = _("Enter the value to set"), integer = False):
    dialog = gtk.Dialog(header,
               self.widgets.window1,
               gtk.DIALOG_DESTROY_WITH_PARENT,
               (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    label = gtk.Label(label)
    label.modify_font(pango.FontDescription("sans 20"))
    calc = gladevcp.Calculator()
    dialog.vbox.pack_start(label)
    dialog.vbox.add(calc)
    if data != None:
        calc.set_value(data)
    else:
        calc.set_value("")
    calc.set_property("font", "sans 20")
    calc.set_editable(True)
    calc.entry.connect("activate", lambda w : dialog.emit("response", gtk.RESPONSE_ACCEPT))
    dialog.parse_geometry("400x400")
    dialog.set_decorated(True)
    dialog.show_all()
    if integer: # The user is only allowed to enter integer values, we hide some button
        calc.num_pad_only(True)
        calc.integer_entry_only(True)
    response = dialog.run()
    value = calc.get_value()
    dialog.destroy()
    if response == gtk.RESPONSE_ACCEPT:
        if value != None:
            return float(value)
        else:
            return "ERROR"
    return "CANCEL"

# display warning dialog
def warning_dialog(self, message, secondary = None, title = _("Operator Message")):
    dialog = gtk.MessageDialog(self.widgets.window1,
        gtk.DIALOG_DESTROY_WITH_PARENT,
        gtk.MESSAGE_INFO, gtk.BUTTONS_OK, message)
    # if there is a secondary message then the first message text is bold
    if secondary:
        dialog.format_secondary_text(secondary)
    dialog.show_all()
    dialog.set_title(title)
    responce = dialog.run()
    dialog.destroy()
    return responce == gtk.RESPONSE_OK

def yesno_dialog(self, message, title = _("Operator Message")):
    dialog = gtk.MessageDialog(self.widgets.window1,
                               gtk.DIALOG_DESTROY_WITH_PARENT,
                               gtk.MESSAGE_QUESTION,
                               gtk.BUTTONS_YES_NO)
    if title:
        dialog.set_title(str(title))
    dialog.set_markup(message)
    responce = dialog.run()
    dialog.destroy()
    return responce == gtk.RESPONSE_YES

def show_user_message(self, message, title = _("Operator Message")):
    dialog = gtk.MessageDialog(self.widgets.window1,
                               gtk.DIALOG_DESTROY_WITH_PARENT,
                               gtk.MESSAGE_INFO,
                               gtk.BUTTONS_OK)
    if title:
        dialog.set_title(str(title))
    dialog.set_markup(message)
    responce = dialog.run()
    dialog.destroy()
    return responce == gtk.RESPONSE_OK

# dialog for run from line
def restart_dialog(self):

    # highlight the gcode down one line lower
    # used for run-at-line restart
    def restart_down(widget, obj, calc):
        obj.widgets.gcode_view.line_down()
        line = int(obj.widgets.gcode_view.get_line_number())
        calc.set_value(line)

    # highlight the gcode down one line higher
    # used for run-at-line restart
    def restart_up(widget, obj, calc):
        obj.widgets.gcode_view.line_up()
        line = int(obj.widgets.gcode_view.get_line_number())
        calc.set_value(line)

    # highlight the gcode of the entered line
    # used for run-at-line restart
    def enter_button(widget, obj, calc):
        line = int(calc.get_value())
        obj.start_line = line
        obj.widgets.gcode_view.set_line_number(line)

    restart_dialog = gtk.Dialog(_("Restart Entry"),
               self.widgets.window1, gtk.DIALOG_DESTROY_WITH_PARENT,
               (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                 gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    label = gtk.Label(_("Restart Entry"))
    label.modify_font(pango.FontDescription("sans 20"))
    restart_dialog.vbox.pack_start(label)
    calc = gladevcp.Calculator()
    restart_dialog.vbox.add(calc)
    calc.set_value("%d" % self.widgets.gcode_view.get_line_number())
    calc.set_property("font", "sans 20")
    calc.set_editable(True)
    calc.num_pad_only(True)
    calc.integer_entry_only(True)
    calc.entry.connect("activate", enter_button, self, calc)
    box = gtk.HButtonBox()
    upbutton = gtk.Button(label = _("Up"))
    box.add(upbutton)
    enterbutton = gtk.Button(label = _("Enter"))
    box.add(enterbutton)
    downbutton = gtk.Button(label = _("Down"))
    box.add(downbutton)
    calc.calc_box.pack_end(box, expand = False, fill = False, padding = 0)
    upbutton.connect("clicked", restart_up, self, calc)
    downbutton.connect("clicked", restart_down, self, calc)
    enterbutton.connect("clicked", enter_button, self, calc)
    restart_dialog.parse_geometry("400x400+0+0")
    restart_dialog.show_all()
    result = restart_dialog.run()
    restart_dialog.destroy()
    if result == gtk.RESPONSE_REJECT:
        line = 0
    else:
        line = int(calc.get_value())
        if line == None:
            line = 0
    self.widgets.gcode_view.set_line_number(line)
    self.start_line = line
    self._add_alarm_entry(_("Ready to Restart program from line %d" % line))
