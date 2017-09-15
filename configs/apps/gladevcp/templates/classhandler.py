#!/usr/bin/env python
# vim: sts=4 sw=4 et
#    This is a component of LinuxCNC
#    classhandler.py  Copyright 2010 Michael Haberler
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
    example gladevcp handler class to start your own
    no persistence support
'''

import glib

class HandlerClass:


    def on_button_press(self,widget,data=None):
        print "on_button_press"


    def on_destroy(self,obj,data=None):
        print "on_destroy"


    def _on_timer_tick(self,userdata=None):
        print "timer tick"
        return True


    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts

        # demonstrate a slow background timer - granularity is one second
        # for a faster timer, use this:
        # glib.timeout_add(5000,  self._on_timer_tick)
        glib.timeout_add_seconds(1, self._on_timer_tick)


def get_handlers(halcomp,builder,useropts):

    for cmd in useropts:
        exec cmd in globals()

    return [HandlerClass(halcomp,builder,useropts)]
