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

import os
import gi
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import GLib
from gi.repository import Pango

from . import hal_actions
import pyngcgui
g_module = os.path.basename(__file__)
#-----------------------------------------------------------------------------
# class to make a gladevcp widget:
# changed to a Box as a Frame caused issues due to the default Alignment widget
# not sure why Gtk3 adds an Alignment considering it is deprecated???
class PyNgcGui(Gtk.Box,hal_actions._EMC_ActionBase):
    """PyNgcGui -- gladevcp widget"""
    __gtype_name__  = 'PyNgcGui'
    __gproperties__ = {
     'use_keyboard' :      (GObject.TYPE_BOOLEAN
                           ,'Use Popup Keyboard'
                           ,'Yes or No'
                           ,False
                           ,GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT
                           ),
     'debug' :             (GObject.TYPE_BOOLEAN
                           ,'Debug'
                           ,'Yes or No'
                           ,False
                           ,GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT
                           ),
     'verbose' :           (GObject.TYPE_BOOLEAN
                           ,'Verbose'
                           ,'Yes or No'
                           ,False
                           ,GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT
                           ),
     'send_function_name': (GObject.TYPE_STRING
                           ,'Send Function'
                           ,'default_send | send_to_axis | dummy_send'
                           ,'default_send'
                           ,GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT
                           ),
     'send_to_dir':        (GObject.TYPE_STRING
                           ,'Send to dir'
                           ,'None|touchy|dirname  None(default:[DISPLAY]PROGRAM_PREFIX'
                           ,''
                           ,GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT
                           ),
     'control_font_name':  (GObject.TYPE_STRING
                           ,'Control Font'
                           ,'example: Sans 10'
                           ,'Sans 10'
                           ,GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT
                           ),
                     }

    __gproperties = __gproperties__ # self.__gproperties
    def __init__(self):
        super(PyNgcGui,self).__init__()
        # put default property values in self.property_dict[]
        self.property_dict = {}
        for name in self.__gproperties.keys():
            gtype = self.__gproperties[name][0]
            if (   gtype == GObject.TYPE_BOOLEAN
                or gtype == GObject.TYPE_STRING):
                ty,lbl,tip,dflt,other = self.__gproperties[name]
            if (   gtype == GObject.TYPE_INT
                or gtype == GObject.TYPE_FLOAT):
                ty,lbl,tip,minv,maxv,dflt,other = self.__gproperties[name]
            self.property_dict[name] = dflt
        GLib.timeout_add(1,self.go_ngcgui) # deferred

    def do_get_property(self,property):
        name = property.name.replace('-', '_')
        if name in self.property_dict.keys():
            return self.property_dict[name]
        else:
            raise AttributeError(_('%s:unknown property %s')
                                 % (g_module,property.name))

    def do_set_property(self,property,value):
        name = property.name.replace('-','_')
        if name not in self.__gproperties.keys():
            raise(AttributeError
                 ,_('%s:pyngcgui:do_set_property: unknown <%s>')
                 % (g_module,name))
        else:
            pyngcgui.vprint('SET P[%s]=%s' % (name,value))
            self.property_dict[name] = value

    def go_ngcgui(self):
        self.start_NgcGui(debug  = self.property_dict['debug']
            ,verbose             = self.property_dict['verbose']
            ,use_keyboard        = self.property_dict['use_keyboard']
            ,send_function_name  = self.property_dict['send_function_name']
            ,send_to_dir         = self.property_dict['send_to_dir']
            ,control_font_name   = self.property_dict['control_font_name']
            )

    def start_NgcGui(self
                    ,debug=False
                    ,verbose=False
                    ,use_keyboard=False
                    ,send_function_name=''
                    ,send_to_dir=''
                    ,control_font_name=None
                    ):

        thenotebook = Gtk.Notebook()
        self.add(thenotebook)

        keyboardfile = None
        if use_keyboard: keyboardfile = 'default'

        send_function = None # None: let NgcGui handle it
        if   send_function_name == '':             send_function = pyngcgui.default_send
        elif send_function_name == 'dummy_send':   send_function = pyngcgui.dummy_send
        elif send_function_name == 'send_to_axis': send_function = pyngcgui.send_to_axis
        elif send_function_name == 'default_send': send_function = pyngcgui.default_send
        else:
            print(_('%s:unknown send_function<%s>')
                  % (g_module,send_function_name))

        if control_font_name is not None:
           control_font = Pango.FontDescription(control_font_name)

        auto_file = None # use default behavior

        if send_to_dir.strip() == "": send_to_dir = None
        if send_to_dir is not None:
            if send_to_dir == 'touchy':
                # allow sent file to show up in touchy auto tab page
                send_to_dir = '~/linuxcnc/nc_files'
            if not os.path.isdir(os.path.expanduser(send_to_dir)):
                raise ValueError(_('%s:Not a directory:\n    %s\n'
                                     % (g_module,send_to_dir)))
            auto_file = os.path.expanduser(
                        os.path.join(send_to_dir,'ngcgui_generated.ngc'))

        self.ngcgui = pyngcgui.NgcGui(w=thenotebook
                            ,debug=debug
                            ,verbose=verbose
                            ,keyboardfile=keyboardfile
                            ,send_function=send_function # prototype: (fname)
                            ,auto_file=auto_file # None for default behavior
                            ,control_font=control_font
                            )
