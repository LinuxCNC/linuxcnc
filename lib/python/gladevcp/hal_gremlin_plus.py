#!/usr/bin/env python

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

import sys
import os
import gtk
import gremlin_view
import gobject

#-----------------------------------------------------------------------------
# determine if glade interface designer is running
# in order to prevent connection of most signals
g_is_glade = False
if 'glade' in sys.argv[0] and 'gladevcp' not in sys.argv[0]:
    for d in os.environ['PATH'].split(':'):
        f = os.path.join(d,sys.argv[0])
        if (    os.path.isfile(f)
            and os.access(f, os.X_OK)):
            g_is_glade = True
            break
g_alive = not g_is_glade
#-----------------------------------------------------------------------------
import hal_actions
class HAL_GremlinPlus(gtk.Frame, hal_actions._EMC_ActionBase):
    """HAL_GremlinPlus: gladevcp widget for gremlin_view.GremlinView
       Provides hal_gremlin with some buttons
    """
    __gtype_name__ = 'HAL_GremlinPlus'
    __gproperties__ = {
        'debug' :       (gobject.TYPE_BOOLEAN
                        ,'Debug'
                        ,'Yes or No'
                        ,False
                        ,gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT
                        ),
        'width' :       (gobject.TYPE_INT
                        ,'width'
                        ,'min width pixels'
                        ,-1
                        ,(1<<31)-1
                        ,300
                        ,gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT
                        ),
        'height' :      (gobject.TYPE_INT
                        ,'height'
                        ,'min height pixels'
                        ,-1
                        ,(1<<31)-1
                        ,300
                        ,gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT
                        ),
     'glade_file':      (gobject.TYPE_STRING
                        ,'glade file name'
                        ,'default or full filename'
                        ,'default'
                        ,gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT
                        ),
     'gtk_theme_name':  (gobject.TYPE_STRING
                        ,'GTK+ Theme Name'
                        ,'default | name_of_gtk+_theme'
                        ,'Follow System Theme'
                        ,gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT
                        ),
                      }

    __gproperties = __gproperties__ # self.__gproperties
    def __init__(self):
        super(HAL_GremlinPlus,self).__init__(label=None)
        self.set_label(None)
        # the two attempts above don't prevent glade from making a label

        # put default property values in self.property_dict[]
        self.property_dict = {}
        for name in self.__gproperties.keys():
            gtype = self.__gproperties[name][0]
            if (   gtype == gobject.TYPE_BOOLEAN
                or gtype == gobject.TYPE_STRING):
                ty,lbl,tip,dflt,other = self.__gproperties[name]
            if (   gtype == gobject.TYPE_INT
                or gtype == gobject.TYPE_FLOAT):
                ty,lbl,tip,minv,maxv,dflt,other = self.__gproperties[name]
            self.property_dict[name] = dflt
        gobject.timeout_add(1,self.go_gremlin_view) # defer

    def do_get_property(self,property):
        name = property.name.replace('-', '_')
        if name in self.property_dict.keys():
            return self.property_dict[name]
        else:
            raise AttributeError(_('%s:unknown property %s')
                                 % (g_progname,property.name))

    def do_set_property(self,property,value):
        name = property.name.replace('-','_')
        if name not in self.__gproperties.keys():
            raise(AttributeError
                 ,_('%s:do_set_property: unknown <%s>')
                 % (g_progname,name))
        else:
            self.property_dict[name] = value

    def go_gremlin_view(self):
        self.start_GremlinView(width=self.property_dict['width']
            ,height=self.property_dict['height']
            ,glade_file=self.property_dict['glade_file']
            ,gtk_theme_name= self.property_dict['gtk_theme_name']
            )
        gobject.timeout_add(1,self.remove_unwanted_label)

    def remove_unwanted_label(self):
        # coerce removal of unwanted label
        self.set_label(None)
        return False # one-time-only

    def start_GremlinView(self
                         ,glade_file=None # None: use default ui file
                         ,width=-1
                         ,height=-1
                         ,gtk_theme_name="Follow System Theme"
                         ):
        if glade_file == 'default':
            glade_file = None

        if not g_alive:
            glade_file = None

        self.g = gremlin_view.GremlinView(glade_file=glade_file
               ,parent=self # works with glade running
               ,width=width
               ,height=height
               ,alive=g_alive
               ,gtk_theme_name=gtk_theme_name
               )

# vim: sts=4 sw=4 et
