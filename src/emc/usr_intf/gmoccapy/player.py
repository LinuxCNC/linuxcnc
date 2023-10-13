#!/usr/bin/env python3

'''
    This class is used to handle sound messages from gmoccapy,
    it is just a copy of a class from gscreen and has been slightly modified

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

'''
import gi
from gi.repository import GLib

from qtvcp import logger
LOG = logger.getLogger(__name__)
# Force the log level for this module
#LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL# attempt to setup audio

try:
    gi.require_version('Gst', '1.0')
    from gi.repository import Gst
except:
    LOG.warning("Gst module missing, - is package python3-gst installed?")
    raise Exception("module missing")

# the player class does the work of playing the audio hints
# http://pygstdocs.berlios.de/pygst-tutorial/introduction.html
class Player:

    def __init__(self):
        Gst.init(None)
        # Element playbin automatic plays any file
        self.player = Gst.ElementFactory.make("playbin", "player")
        # Enable message bus to check for errors in the pipeline
        bus = self.player.get_bus()
        bus.add_signal_watch()
        bus.connect("message", self.on_message)
        self.loop = GLib.MainLoop()

    def run(self):
        self.player.set_state(Gst.State.PLAYING)
        self.loop.run()

    def set_sound(self, file):
        # Set the uri to the file
        self.player.set_property("uri", "file://" + file)

    def on_message(self, bus, message):
        t = message.type
        if t == Gst.MessageType.EOS:
            # file ended, stop
            self.player.set_state(Gst.State.NULL)
            self.loop.quit()
        elif t == Gst.MessageType.ERROR:
            # Error occurred, print and stop
            self.player.set_state(Gst.State.NULL)
            err, debug = message.parse_error()
            print ("Error: %s" % err, debug)
            self.loop.quit()
