# -*- python -*-
#    Copyright (C) 2014 Jeff Epler <jepler@unpythonic.net>
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
from gi.repository import Gtk

__all__ = ['MultiFileBuilder']

class MultiFileBuilder:
    def __init__(self):
        self.builders = []
        self.domain = None

    def set_translation_domain(self, domain):
        self.domain = domain

    def connect_signals(self, obj):
        for b in self.builders: b.connect_signals(obj)

    def add_from_file(self, fn):
        builder = Gtk.Builder()
        if self.domain is not None: builder.set_translation_domain(self.domain)

        self.builders.append(builder)
        builder.add_from_file(fn)

    def add_from_string(self, strg):
        builder = Gtk.Builder()
        if self.domain is not None: builder.set_translation_domain(self.domain)

        self.builders.append(builder)
        builder.add_from_string(strg)

    def get_object(self, obj):
        objects = [builder.get_object(obj) for builder in self.builders]
        objects = [o for o in objects if o]
        if not objects: return None
        if len(objects) > 1: raise ValueError, """Use of object with duplicate ID -> '%s'"""% obj
        return objects[0]
  
