#!/usr/bin/env python
# vim: sts=4 sw=4 et
#    This is a component of LinuxCNC
#    classhandler_persistent.py  Copyright 2010 Michael Haberler
#
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
'''
    example gladevcp handler class with widget and attribute persistence support
'''

from gladevcp.persistence import IniFile,widget_defaults,set_debug,select_widgets

debug = 0

class HandlerClass:

    def on_button_press(self,widget,data=None):
        print "on_button_press"
        self.anint += 1
        # show retained attributes as inifile and current values
        print "attr\tini\tcurrent"
        for k in self.defaults[IniFile.vars].keys():
            print "%s\t%s\t%s" % (k,self.defaults[IniFile.vars][k],getattr(self,k,None))

    def on_destroy(self,obj,data=None):
        '''
        save state on application exit
        '''
        print "on_destroy() - saving state"
        self.ini.save_state(self)

    def on_restore_defaults(self,button,data=None):
        '''
        example callback for 'Reset to defaults' button (unused)
        '''
        print "on_restore_defaults() - setting default state"
        self.ini.create_default_ini()
        self.ini.restore_state(self)


    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts

        # use the module basename for the ini file:
        self.ini_filename = __name__ + '.save'

        # choose widgets and attributes to be retained here:

        # this descriptor will not retain any attrbuts,
        # but the state of all widgets (HAL widgets and Gtk widgets
        # (subject to widget types supported by gladevcp.persistence.accessors())
        #self.defaults = {   IniFile.vars: dict(),
        #                    IniFile.widgets : widget_defaults(<widgetlist>)
        #                }

        # this descriptor will retain the listed attributes, and only
        # HAL widgets state:
        self.defaults = {   IniFile.vars: { 'afloat' : 1.67, 'anint' : 42, 'abool' : True, 'astring': 'sometext' },
                            IniFile.widgets : widget_defaults(select_widgets(self.builder.get_objects(), hal_only=False,output_only = False))
                        }

        self.ini = IniFile(self.ini_filename,self.defaults, self.builder)

        # it is OK to use self.panel.widgets (all widgets) here because only
        # widgets whose state was saved as per descriptor will be restored
        self.ini.restore_state(self)


def get_handlers(halcomp,builder,useropts):

    global debug
    for cmd in useropts:
        exec cmd in globals()

    # get some detail what save/restore etc are doing
    set_debug(debug)

    return [HandlerClass(halcomp,builder,useropts)]
