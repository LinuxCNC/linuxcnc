#!/usr/bin/env python

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
#        calculating the hight of the popup.

import gtk
import gobject
import pango

class Notification(gtk.Window):
    '''Notification(gtk.Window)
       will show popup windows with messages and icon
    '''

    __gtype_name__ = 'Notification'
    __gproperties__ = {
           'icon_size' : (gobject.TYPE_INT, 'Icon Size', 'Sets the size of the displayed button icon',
                        1, 96, 16, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'message_width' : (gobject.TYPE_INT, 'Message Width', 'Sets the message width in pixel',
                        0, 800, 300, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'x_pos' : (gobject.TYPE_INT, 'Frame X Position', 'Sets the frame X position in pixel',
                        0, 1280, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'y_pos' : (gobject.TYPE_INT, 'Frame Y Position', 'Sets the frame Y position in pixel',
                        0, 1024, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'font' : (gobject.TYPE_STRING, 'Pango Font', 'Display font to use',
                      "sans 10", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'max_messages' : (gobject.TYPE_INT, 'Max Messages', 'Sets the maximum number of messages to show',
                        2, 100, 10, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'top_to_bottom' : (gobject.TYPE_BOOLEAN, 'Show from top to bottom', 'Show the second notification under the first one or on top?',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'use_frames' : (gobject.TYPE_BOOLEAN, 'Use Frames for messages', 'You can separate the messages using frames, but you will need more space',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
                      }
    __gproperties = __gproperties__

    __gsignals__ = {
                'message_deleted': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),
               }


    # build the main gui
    def __init__(self):
        gtk.Window.__init__(self)
        self.connect('destroy', lambda*w:gtk.main_quit())
        self.messages = []
        self.popup = gtk.Window(gtk.WINDOW_POPUP)
        self.vbox = gtk.VBox()
        self.popup.add(self.vbox)
        self.icon_size = 16
        self.message_width = 200
        self.x_pos = 20
        self.y_pos = 20
        self.font = 'sans 10'
        self.max_messages = 10
        self.top_to_bottom = True
        self.use_frames = False
        self.height = 0

    # this will fill the main gui with the frames, containing the messages or errors
    def _show_message(self, message):
        number = message[0]
        text = message[1]
        if message[2]:
            icon_file_name = message[2]
        if self.use_frames:
            frame = gtk.Frame()
            frame.set_label("")
        hbox = gtk.HBox()
        hbox.set_property('spacing', 5)
        if self.use_frames:
            frame.add(hbox)
        labelnumber = gtk.Label(number)
        hbox.pack_start(labelnumber)
        icon = gtk.Image()
        if message[2]:
            icon.set_from_file(icon_file_name)
        else:
            icon.set_from_stock(gtk.STOCK_DIALOG_ERROR, self.icon_size)
        hbox.pack_start(icon)
        label = gtk.Label()
        label.set_line_wrap(True)
        label.set_line_wrap_mode(pango.WRAP_CHAR)
        label.set_size_request(self.message_width, -1)
        font_desc = pango.FontDescription(self.font)
        label.modify_font(font_desc)
        # As messages may contain non pango conform syntax like "vel <= 0" we will have to check that to avoid an error
        pango_ok = True
        try:
            # The GError exception is raised if an error occurs while parsing the markup text.
            pango.parse_markup(text)        
        except:
            pango_ok = False
        if pango_ok:
            label.set_markup(text)
        else:
            label.set_text(text)
        hbox.pack_start(label)
        btn_close = gtk.Button()
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_CANCEL, self.icon_size)
        btn_close.set_image(image)
        btn_close.set_border_width(2)
        btn_close.connect('clicked', self._on_btn_close_clicked, labelnumber.get_text())
        hbox.pack_start(btn_close)
        if self.use_frames:
            widget = frame
        else:
            widget = hbox
        if self.top_to_bottom:
            self.vbox.pack_end(widget)
        else:
            self.vbox.pack_start(widget)
        if self.use_frames:
            frame.show()
        label.show()
        btn_close.show()
        hbox.show()
        icon.show()
# we do not show the labelnumber, but we use it for the handling
#        labelnumber.show()
        self.vbox.show()

    # add a message, the message is a string, it will be line wraped
    # if to long for the frame
    def add_message(self, message, icon_file_name):
        '''Notification.add_message(messagetext, icon_file_name)
        
           messagetext = a string to display
           icon_file_name = a valid absolut path to an icon or None
        '''
        self.popup.hide()
        self.popup.resize(1, 1)
        number_of_messages = len(self.messages)
        if number_of_messages == self.max_messages:
            self.del_first()
            number_of_messages = len(self.messages)
        self.messages.append([number_of_messages, message, icon_file_name])
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
    # if you give a value of "-1" all messages will be deletet
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
            self.add_message(_('Error trying to delet the message with number %s' % messagenumber), None)
            return False
        try:
            del self.messages[int(messagenumber)]
        except:
            return False
        self._refill_messages()
        return True

    # this is the recomendet way to delete a message, by clicking the
    # close button of the coresponding frame
    def _on_btn_close_clicked(self, widget, labelnumber):
        del self.messages[int(labelnumber)]
        self.emit("message_deleted", self.messages)
        self._refill_messages()

    def _refill_messages(self):
        # first we have to hide all messages, otherwise the popup window will mantain
        # all the old messages
        self.popup.hide_all()
        # then we rezise the popup window to a very small size, otherwise the dimensions
        # of the window will be mantained
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
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown notification get_property %s' % property.name)

    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in self.__gproperties.keys():
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
            else:
                raise AttributeError('unknown notification set_property %s' % property.name)
        except:
            print('Attribute error', property, "and", type(value) , value)
            pass

# for testing without glade editor:
def main():

    notification = Notification()
    notification.add_message('Halo World out there', '/usr/share/gmoccapy/images/applet-critical.png')
    notification.add_message('Hallo World ', '/usr/share/gmoccapy/images/std_info.gif')
    notification.show()
    def debug(self, text):
        print "debug", text
    notification.connect("message_deleted", debug)
    gtk.main()

if __name__ == "__main__":
    main()
