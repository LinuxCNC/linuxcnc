#!/usr/bin/env python3

'''
    This widget can be used to show small popup windows showing messages.

    Copyright 2012 Norbert Schechner
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

# TODO : There is still an error if you use top_to_bottom = False
#        the messages are placed correct, until you reached the given max, then
#        the popup will jump one message height down. As far as I found out till now
#        it is caused because the height of the first message is not taken in care
#        calculating the height of the popup.

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Pango

try:
    from gmoccapy import icon_theme_helper
except:
    import icon_theme_helper    # only for testing purpose, otherwise the main method in this file would fail

class Notification(Gtk.Window):
    '''Notification(Gtk.Window)
       will show popup windows with messages and icon
    '''

    __gtype_name__ = 'Notification'
    __gproperties__ = {
           'icon_size' : (GObject.TYPE_INT, 'Icon Size', 'Sets the size of the displayed button icon',
                        1, 96, 16, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'message_width' : (GObject.TYPE_INT, 'Message Width', 'Sets the message width in pixel',
                        0, 800, 300, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'x_pos' : (GObject.TYPE_INT, 'Frame X Position', 'Sets the frame X position in pixel',
                        0, 1280, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'y_pos' : (GObject.TYPE_INT, 'Frame Y Position', 'Sets the frame Y position in pixel',
                        0, 1024, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'font' : (GObject.TYPE_STRING, 'Pango Font', 'Display font to use',
                      "sans 10", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'max_messages' : (GObject.TYPE_INT, 'Max Messages', 'Sets the maximum number of messages to show',
                        2, 100, 10, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'top_to_bottom' : (GObject.TYPE_BOOLEAN, 'Show from top to bottom', 'Show the second notification under the first one or on top?',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'use_frames' : (GObject.TYPE_BOOLEAN, 'Use Frames for messages', 'You can separate the messages using frames, but you will need more space',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
            'icon_theme_path' : (GObject.TYPE_STRING, 'Icon theme lookup path', 'Pathes where to look for icon themes',
                      "", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
            'icon_theme_name' : (GObject.TYPE_STRING, 'Icon theme name', 'Name set in gmoccapy preferences',
                      "classic", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
                      }
    __gproperties = __gproperties__

    __gsignals__ = {
                'message_deleted': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_PYOBJECT,)),
               }


    # build the main gui
    def __init__(self):
        self.messages = []
        self.popup = Gtk.Window(type = Gtk.WindowType.POPUP)
        self.vbox = Gtk.VBox()
        self.popup.add(self.vbox)
        self.icon_size = 24
        self.message_width = 200
        self.x_pos = 20
        self.y_pos = 20
        self.font = 'sans 10'
        self.max_messages = 10
        self.top_to_bottom = True
        self.use_frames = False
        self.height = 0
        self.icon_theme = Gtk.IconTheme()

        # Gtk.Window.__init__() sets all __gproperties__ to their default values.
        # Therefore it calls do_set_property()
        # So the Gtk.IconTheme has to ne defined before. The initialization of the
        # primitive data types above are meaningless (and can be removed?).
        super().__init__()
        self.connect('destroy', lambda*w:Gtk.main_quit())

    # this will fill the main gui with the frames, containing the messages or errors
    def _show_message(self, message):
        number = message[0]
        text = message[1]
        if self.use_frames:
            frame = Gtk.Frame()
            frame.set_label("")
        hbox = Gtk.HBox()
        hbox.set_property('spacing', 5)
        if self.use_frames:
            frame.add(hbox)
        labelnumber = Gtk.Label(label = number)
        hbox.pack_start(labelnumber, False, False, 0)
        icon = Gtk.Image()
        if message[2]:
            icon_name = message[2]
        else:
            icon_name = "dialog_warning"
        default_style = Gtk.Button().get_style_context()
        pixbuf = icon_theme_helper.load_symbolic_from_icon_theme(self.icon_theme, icon_name, self.icon_size, default_style)
        icon.set_from_pixbuf(pixbuf)
        hbox.pack_start(icon, False, False, 3)
        label = Gtk.Label()
        label.set_line_wrap(True)
        label.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR)
        label.set_size_request(self.message_width, -1)
        font_desc = Pango.FontDescription(self.font)
        label.modify_font(font_desc)
        # As messages may contain non Pango conform syntax like "vel <= 0" we will have to check that to avoid an error
        Pango_ok = True
        try:
            # The GError exception is raised if an error occurs while parsing the markup text.
            Pango.parse_markup(text)
        except:
            Pango_ok = False
        if Pango_ok:
            label.set_markup(text)
        else:
            label.set_text(text)
        label.set_xalign(0)
        hbox.pack_start(label, False, False, 0)
        btn_close = Gtk.Button()
        btn_close.set_name("notification_close")
        image = Gtk.Image()
        pixbuf = icon_theme_helper.load_symbolic_from_icon_theme(self.icon_theme ,"window_close", self.icon_size, default_style)
        image.set_from_pixbuf(pixbuf)
        btn_close.set_image(image)
        btn_close.set_border_width(2)
        btn_close.connect('clicked', self._on_btn_close_clicked, labelnumber.get_text())
        btn_close.set_size_request(48, 48)
        btn_box = Gtk.Box.new(Gtk.Orientation.VERTICAL,0)
        btn_box.set_center_widget(btn_close)
        btn_box.show()
        hbox.pack_end(btn_box, False, False, 0)
        if self.use_frames:
            widget = frame
        else:
            widget = hbox
        if self.top_to_bottom:
            self.vbox.pack_end(widget, False, False, 0)
        else:
            self.vbox.pack_start(widget, False, False, 0)
        if self.use_frames:
            frame.show()
        label.show()
        btn_close.show()
        hbox.show()
        icon.show()
# we do not show the labelnumber, but we use it for the handling
        #labelnumber.show()
        self.vbox.show()

    # add a message, the message is a string, it will be line wrapped
    # if to long for the frame
    def add_message(self, message, icon_name=None):
        '''Notification.add_message(messagetext, icon_file_name)

           messagetext = a string to display
           icon_file_name = a valid absolute path to an icon or None
        '''
        self.popup.hide()
        self.popup.resize(1, 1)
        number_of_messages = len(self.messages)
        if number_of_messages == self.max_messages:
            self.del_first()
            number_of_messages = len(self.messages)
        self.messages.append([number_of_messages, message, icon_name])
        self._show_message(self.messages[number_of_messages])
        if not self.top_to_bottom:
            self.height = self.popup.get_size()[1]
        # print(self.y_pos, self.popup.get_size(),self.height)
        self.popup.move(self.x_pos, self.y_pos - self.height)
        self.popup.show()
        # print(self.popup.get_position())

    def del_first(self):
        '''del_first()
           delete the first message
        '''
        if len(self.messages) != 0:
            del self.messages[0]
            self._refill_messages()
            self.emit("message_deleted", self.messages)

    def del_last(self):
        '''del_last()
           delete the last message
        '''
        if len(self.messages) != 0:
            del self.messages[len(self.messages) - 1]
            self._refill_messages()
            self.emit("message_deleted", self.messages)

    # this will delete a message, if the user gives a valid number it will be deleted,
    # but the user must take care to use the correct number
    # if you give a value of "-1" all messages will be deleted
    def del_message(self, messagenumber):
        '''del_message(messagenumber)
           delete the message with the given number

           messagenumber = integer
                           -1 will erase all messages
        '''
        self.emit("message_deleted", self.messages)
        if messagenumber == -1:
            self.messages = []
            self._refill_messages()
            return True
        elif messagenumber > len(self.messages) or messagenumber < 0:
            self.add_message(_('Error trying to delete the message with number {0}'.format(messagenumber), None))
            return False
        try:
            del self.messages[int(messagenumber)]
        except:
            return False
        self._refill_messages()
        return True

    # this is the recomendet way to delete a message, by clicking the
    # close button of the corresponding frame
    def _on_btn_close_clicked(self, widget, labelnumber):
        del self.messages[int(labelnumber)]
        self.emit("message_deleted", self.messages)
        self._refill_messages()

    def _refill_messages(self):
        # first we have to hide all messages, otherwise the popup window will maintain
        # all the old messages
        childs = self.popup.get_children()[0].get_children()
        for child in childs:
            child.hide()
        # then we rezise the popup window to a very small size, otherwise the dimensions
        # of the window will be maintained
        self.popup.resize(1, 1)
        # if it was the last message, than we can hide the popup window
        if len(self.messages) == 0:
            self.popup.hide()
            return
        else: # or we do refill the popup window
            index = 0
            for message in self.messages:
                self.messages[index][0] = index
                self._show_message(message)
                index += 1
        if not self.top_to_bottom:
            self.height = self.popup.get_size()[1]
        self.popup.move(self.x_pos, self.y_pos - self.height)
        self.popup.show()

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown notification get_property %s' % property.name)

    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in list(self.__gproperties.keys()):
                # print("Set property", property, "to", value)
                setattr(self, name, value)
                self.queue_draw()
                if name == 'icon_size':
                    self.icon_size = value
                if name == 'message_width':
                    self.message_width = value
                if name == 'font':
                    self.font = value
                if name == 'x_pos':
                    self.x_pos = value
                if name == 'y_pos':
                    self.y_pos = value
                if name == 'max_messages':
                    self.max_messages = value
                if name == 'top_to_bottom':
                    self.top_to_bottom = value
                if name == 'use_frames':
                    self.use_frames = value
                if name == 'icon_theme_path':
                    self.icon_theme.append_search_path(value)
                if name == 'icon_theme_name':
                    self.icon_theme.set_custom_theme(value)
            else:
                raise AttributeError('unknown notification set_property %s' % property.name)
        except AttributeError as e:
            print('Attribute error in property:', property, "type:", type(value) , "value:", value)
            print("{0} exception occurred {1}".format(type(e).__name__, e.args))
            pass

# for testing without glade editor:
def main():

    notification = Notification()
    notification.icon_theme.append_search_path("../../../../share/gmoccapy/icons/") # relative from this file location
    notification.icon_theme.append_search_path("../share/gmoccapy/icons/")
    notification.icon_theme.append_search_path("/usr/share/gmoccapy/icons/")
    notification.icon_theme.set_custom_theme("classic")
    # notification.icon_theme.set_custom_theme("material")
    # notification.icon_theme.set_custom_theme("material-light")
    notification.add_message('This is a warning', 'dialog_warning')
    notification.add_message('Hallo World this is a long string that have a linebreak ', 'dialog_information')
    notification.add_message('This has a default icon')
    notification.show()
    #def debug(self, text):
    #    print("debug", text)
    #notification.connect("message_deleted", debug)
    Gtk.main()

if __name__ == "__main__":
    main()
