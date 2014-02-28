#!/usr/bin/env python

'''
    This class is used to handle sound messages from gmoccapy,
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
import gobject
import gst

# the player class does the work of playing the audio hints
# http://pygstdocs.berlios.de/pygst-tutorial/introduction.html
class Player:

    def __init__(self):
        import gst
        # Element playbin automatic plays any file
        self.player = gst.element_factory_make("playbin", "player")
        # Enable message bus to check for errors in the pipeline
        bus = self.player.get_bus()
        bus.add_signal_watch()
        bus.connect("message", self.on_message)
        self.loop = gobject.MainLoop()

    def run(self):
        self.player.set_state(gst.STATE_PLAYING)
        self.loop.run()

    def set_sound(self, file):
        # Set the uri to the file
        self.player.set_property("uri", "file://" + file)

    def on_message(self, bus, message):
        t = message.type
        if t == gst.MESSAGE_EOS:
            # file ended, stop
            self.player.set_state(gst.STATE_NULL)
            self.loop.quit()
        elif t == gst.MESSAGE_ERROR:
            # Error ocurred, print and stop
            self.player.set_state(gst.STATE_NULL)
            err, debug = message.parse_error()
            print ("Error: %s" % err, debug)
            self.loop.quit()
