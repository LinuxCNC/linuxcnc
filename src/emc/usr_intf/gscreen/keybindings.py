# Gscreen is Copyright (c) 20013  Chris Morley
#
# Gscreen is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Gscreen is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# This holds/converts the generic function keyword to the actual function call name
# it returns this name so Gscreen can call the function to actually do something.
# you can add or change these
class Keycalls:
    def __init__(self):
        self.ESTOP = 'on_keycall_ESTOP'
        self.POWER = 'on_keycall_POWER'
        self.ABORT = 'on_keycall_ABORT'
        self.XPOS = 'on_keycall_XPOS'
        self.XNEG = 'on_keycall_XNEG'
        self.YPOS = 'on_keycall_YPOS'
        self.YNEG = 'on_keycall_YNEG'
        self.ZPOS = 'on_keycall_ZPOS'
        self.ZNEG = 'on_keycall_ZNEG'
        self.APOS = 'on_keycall_APOS'
        self.ANEG = 'on_keycall_ANEG'
        self.INCREMENTS = 'on_keycall_INCREMENTS'
        self.TEST = 'on_keycall_INCREMENTS'

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# This holds/converts the actual keypress (keyname = gtk.gdk.keyval_name(event.keyval))
# to a generic function keyword
# you can add or change these.
class Keybinding:
    def __init__(self):
        self.F1 = 'ESTOP'
        self.F2 = 'POWER'
        self.Escape = 'ABORT'
        self.Up = 'YPOS'
        self.Down = 'YNEG'
        self.Right = 'XPOS'
        self.Left = 'XNEG'
        self.Page_Up = 'ZPOS'
        self.Page_Down = 'ZNEG'
        self.bracketleft = 'APOS'
        self.bracketright = 'ANEG'
        self.i = 'INCREMENTS'
        self.I = 'INCREMENTS'

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# These is the public methods for key conversion to function call name.
# get_call and get_binding are for confirmation of a call or binding entry.
# convert() takes a key string (from gtk.gdk.keyval_name(event.keyval)) and converts it to a function call string or returns None
# add_call and add_binding allow adding or changing calls or bindings
# add_conversion() does both at the same time
class Keylookup:
    def __init__(self):
        self.keycall = Keycalls()
        self.keybinding = Keybinding()

    def get_call(self,binding):
        try:
            return self.keycall[binding]
        except:
            print "No key function call"
            return None

    def get_binding(self,key):
        try:
            return self.keybinding[key]
        except:
            print "No key binding"
            return None

    def convert(self,key):
        try:
            b = self.keybinding[key]
            return self.keycall[b]
        except:
            return None

    def add_binding(self,key,binding):
        try:
            self.keybinding[key] = binding
        except:
            print "Binding for key %s could not be added"% key

    def add_call(self,binding,function):
        try:
            self.keycall[binding] = function
        except:
            print "Binding %s could not be added"% binding

    def add_conversion(self,key,binding,function):
        self.add_binding(key,binding)
        self.add_call(binding,function)
