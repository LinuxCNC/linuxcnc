#!/usr/bin/env python3

'''
    This class is used to handle the dialogs from gmoccapy,
    it is just a copy of a class from gscreen and has been slightly modified

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

'''
import gi
gi.require_version("Gtk","3.0")
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Pango
import gladevcp

class Dialogs(GObject.GObject):

    __gtype_name__ = 'Dialogs'

    __gsignals__ = {
                'play_sound': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
               }

    def __init__(self):
        GObject.GObject.__init__(self)

    # This dialog is for unlocking the system tab
    # The unlock code number is defined at the top of the page
    def system_dialog(self, caller):
        dialog = Gtk.Dialog(_("Enter System Unlock Code"),
                   caller.widgets.window1,
                   Gtk.DialogFlags.DESTROY_WITH_PARENT,
                   (Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                    Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT))
        label = Gtk.Label(_("Enter System Unlock Code"))
        label.modify_font(Pango.FontDescription("sans 20"))
        calc = gladevcp.Calculator()
        dialog.vbox.pack_start(label, False, False, 0)
        dialog.vbox.add(calc)
        calc.set_value("")
        calc.set_property("font", "sans 20")
        calc.set_editable(True)
        calc.entry.connect("activate", lambda w : dialog.emit("response", Gtk.ResponseType.ACCEPT))
        dialog.parse_geometry("400x400")
        dialog.set_decorated(True)
        dialog.show_all()
        self.emit("play_sound", "alert")
        response = dialog.run()
        code = calc.get_value()
        dialog.destroy()
        if response == Gtk.ResponseType.ACCEPT:
            if code == int(caller.unlock_code):
                return True
        return False

    def entry_dialog(self, caller, data = None, header = _("Enter value") , label = _("Enter the value to set"), integer = False):
        dialog = Gtk.Dialog(header,
                   caller.widgets.window1,
                   Gtk.DialogFlags.DESTROY_WITH_PARENT,
                  (Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                   Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT))
        label = Gtk.Label(label)
        label.modify_font(Pango.FontDescription("sans 20"))
        calc = gladevcp.Calculator()
        # add label to control the height of the action_area
        label_h = Gtk.Label("\n\n\n")
        dialog.action_area.pack_start(label_h, True, True, 0)
        dialog.action_area.reorder_child(label_h, 0)
        dialog.vbox.pack_start(label, True, True, 0)
        dialog.vbox.add(calc)
        if data != None:
            calc.set_value(data)
        else:
            calc.set_value("")
        calc.set_property("font", "sans 20")
        calc.set_editable(True)
        calc.entry.connect("activate", lambda w : dialog.emit("response", Gtk.ResponseType.ACCEPT))
        dialog.parse_geometry("460x400")
        dialog.set_decorated(True)
        self.emit("play_sound", "alert")
        if integer: # The user is only allowed to enter integer values, we hide some button
            calc.num_pad_only(True)
            calc.integer_entry_only(True)
        dialog.show_all()
        response = dialog.run()
        value = calc.get_value()
        dialog.destroy()
        if response == Gtk.ResponseType.ACCEPT:
            if value != None:
                if integer:
                    return int(value)
                else:
                    return float(value)
            else:
                return "ERROR"
        return "CANCEL"

    # display warning dialog
    def warning_dialog(self, caller, message, secondary = None, title = _("Operator Message"), sound = True):
        dialog = Gtk.MessageDialog(caller.widgets.window1,
                                   Gtk.DialogFlags.DESTROY_WITH_PARENT,
                                   Gtk.MessageType.INFO, Gtk.ButtonsType.OK, message)
        # if there is a secondary message then the first message text is bold
        if secondary:
            dialog.format_secondary_text(secondary)
        dialog.show_all()
        if sound:
            self.emit("play_sound", "alert")
        dialog.set_title(title)
        responce = dialog.run()
        dialog.destroy()
        return responce == Gtk.ResponseType.OK

    def yesno_dialog(self, caller, message, title = _("Operator Message")):
        dialog = Gtk.MessageDialog(caller.widgets.window1,
                                   Gtk.DialogFlags.DESTROY_WITH_PARENT,
                                   Gtk.MessageType.QUESTION,
                                   Gtk.ButtonsType.YES_NO)
        if title:
            dialog.set_title(str(title))
        dialog.set_markup(message)
        dialog.show_all()
        self.emit("play_sound", "alert")
        responce = dialog.run()
        dialog.destroy()
        return responce == Gtk.ResponseType.YES

    def show_user_message(self, caller, message, title = _("Operator Message")):
        dialog = Gtk.MessageDialog(caller.widgets.window1,
                                   Gtk.DialogFlags.DESTROY_WITH_PARENT,
                                   Gtk.MessageType.INFO,
                                   Gtk.ButtonsType.OK)
        if title:
            dialog.set_title(str(title))
        dialog.set_markup(message)
        dialog.show_all()
        self.emit("play_sound", "alert")
        responce = dialog.run()
        dialog.destroy()
        return responce == Gtk.ResponseType.OK

    # dialog for run from line
    def restart_dialog(self, caller):

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

        restart_dialog = Gtk.Dialog(_("Restart Entry"),
                   caller.widgets.window1, Gtk.DialogFlags.DESTROY_WITH_PARENT,
                   (Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                     Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT))
        label = Gtk.Label(_("Restart Entry"))
        label.modify_font(Pango.FontDescription("sans 20"))
        restart_dialog.vbox.pack_start(label, False, False, 0)
        calc = gladevcp.Calculator()
        restart_dialog.vbox.add(calc)
        calc.set_value("%d" % caller.widgets.gcode_view.get_line_number())
        calc.set_property("font", "sans 20")
        calc.set_editable(True)
        calc.num_pad_only(True)
        calc.integer_entry_only(True)
        calc.entry.connect("activate", enter_button, caller, calc)
        box = Gtk.HButtonBox()
        upbutton = Gtk.Button(label = _("Up"))
        box.add(upbutton)
        enterbutton = Gtk.Button(label = _("Enter"))
        box.add(enterbutton)
        downbutton = Gtk.Button(label = _("Down"))
        box.add(downbutton)
        calc.calc_box.pack_end(box, expand = False, fill = False, padding = 0)
        upbutton.connect("clicked", restart_up, caller, calc)
        downbutton.connect("clicked", restart_down, caller, calc)
        enterbutton.connect("clicked", enter_button, caller, calc)
        restart_dialog.parse_geometry("400x400+0+0")
        restart_dialog.show_all()
        self.emit("play_sound", "alert")
        result = restart_dialog.run()
        restart_dialog.destroy()
        if result == Gtk.ResponseType.REJECT:
            line = 0
        else:
            line = int(calc.get_value())
            if line == None:
                line = 0
        caller.widgets.gcode_view.set_line_number(line)
        caller.start_line = line
