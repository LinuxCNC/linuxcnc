#!/usr/bin/env python

'''
    This class is just to handle the widgets of gmoccapy,
    it is just a coppy of a class from gscreen and has been slighly modified

    Copyright 2014 Norbert Schechner
    nieson@web.de
    original Author = Chris Morley

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

# a class for holding the glade widgets rather then searching for them each time
class Widgets:

    def __init__(self, builder):
        self._builder = builder

    def __getattr__(self, attr):
        widget = self._builder.get_object(attr)
        if widget is None:
            raise AttributeError, "No widget %widget" % attr
        return widget

    def __getitem__(self, attr):
        widget = self._builder.get_object(attr)
        if widget is None:
            raise IndexError, "No widget %widget" % attr
        return widget

