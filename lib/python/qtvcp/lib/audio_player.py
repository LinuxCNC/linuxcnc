#!/usr/bin/env python3
# qtVcp library audio player
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# This library is used to play sounds launched from STATUS messages
# it has uses OS builtin sounds or can play arbritray files
#######################################################################

# Set up logging
from qtvcp import logger

LOG = logger.getLogger(__name__)

import os
import subprocess

# try to add ability for audio feedback to user.
PY_LIB_GOOD = GST_LIB_GOOD = True
try:
    import gi
    gi.require_version('Gst', '1.0')
    from gi.repository import Gst
    Gst.init(None)
except Exception as e:
    print(e)
    LOG.warning('audio alerts - Is python3-gst1.0 installed?')
    PY_LIB_GOOD = False
    p = subprocess.Popen('gst-launch-1.0', shell=True,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, close_fds=True)
    output, error = p.communicate()
    if p.returncode > 1:
        LOG.warning('no audio alerts available - Is gstreamer-1.0-tools installed?')
        GST_LIB_GOOD = False

from qtvcp.core import Status
from qtvcp.widgets.widget_baseclass import _HalWidgetBase

# Instaniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
STATUS = Status()
ESPEAK = False
try:
    from espeak import espeak
    import queue
    esQueue = queue.Queue()
    espeak.set_voice("m3")
    espeak.set_parameter(espeak.Parameter.Rate,160)
    espeak.set_parameter(espeak.Parameter.Pitch,1)
    ESPEAK = True
except:
    LOG.warning('audio alerts - Is python3-espeak installed? (sudo apt install python3-espeak)')
    try:
        subprocess.check_output('''espeak --help''', shell=True)
        ESPEAK = True
    except:
        LOG.warning('audio alerts - Is espeak installed? (sudo apt install espeak)')
        LOG.warning('Text to speech output not available. ')


# the player class does the work of playing the audio hints
# http://pygstdocs.berlios.de/pygst-tutorial/introduction.html
class Player:
    def __init__(self):
        self.set_sounds()
        if PY_LIB_GOOD:
            self.player = Gst.ElementFactory.make("playbin", "player")
            self.player.set_property("uri", "file://" + self.error)
            # Enable message bus to check for errors in the pipeline
            bus = self.player.get_bus()
            bus.add_signal_watch()
            bus.connect("message", self.on_message)
        try:
            espeak.set_SynthCallback(self.speak_finished)
        except Exception as e:
            pass

    # set sounds based on distribution
    def set_sounds(self):
        try:
            import distro
            if 'mint' in distro.id().lower():
                self.error = '/usr/share/sounds/LinuxMint/stereo/dialog-error.ogg'
                self.ready = '/usr/share/sounds/LinuxMint/stereo/dialog-information.ogg'
                self.done = '/usr/share/sounds/LinuxMint/stereo/system-ready.ogg'
                self.attention = '/usr/share/sounds/LinuxMint/stereo/dialog-question.wav'
                self.ring = '/usr/share/sounds/LinuxMint/stereo/phone-incoming-call.ogg'
                self.login = '/usr/share/sounds/LinuxMint/stereo/desktop-login.ogg'
                self.logout = '/usr/share/sounds/LinuxMint/stereo/desktop-logout.ogg'
                self.bell = '/usr/share/sounds/freedesktop/stereo/bell.oga'
                if not os.path.exists(self.error):
                    log.error('Audio player - Mint sound File not found {}'.format(self.error))
                return
        except:
            pass
        self.error = '/usr/share/sounds/freedesktop/stereo/dialog-error.oga'
        self.ready = '/usr/share/sounds/freedesktop/stereo/message.oga'
        self.done = '/usr/share/sounds/freedesktop/stereo/complete.oga'
        self.attention = '/usr/share/sounds/freedesktop/stereo/suspend-error.oga'
        self.ring = '/usr/share/sounds/freedesktop/stereo/phone-incoming-call.oga'
        self.login = '/usr/share/sounds/freedesktop/stereo/service-login.oga'
        self.logout = '/usr/share/sounds/freedesktop/stereo/service-logout.oga'
        self.bell = '/usr/share/sounds/freedesktop/stereo/bell.oga'
        if not os.path.exists(self.error):
            LOG.error('Audio player - Default sound File not found {}'.format(self.error))

    # play sounds on these messages from GStat
    # play-sound allows an arbrtrary absolute file name
    # play-alert uses internal builtins
    # use the function name les 'play_'
    def _register_messages(self):
        if PY_LIB_GOOD:
            STATUS.connect('play-sound', self.jump)
        elif GST_LIB_GOOD:
            STATUS.connect('play-sound', self.os_jump)

    # jump to a builtin alert sound
    # This uses the system to play the sound because gst is not available
    # this can still fail if gstreamer/tools are not available 
    def os_jump(self, w, f):
        if 'beep' in f.lower():
            self[f.lower()]()
            return
        elif 'speak' in f.lower():
            self.os_speak(f)
            return
        path = os.path.expanduser(f)
        if not os.path.exists(path):
            path = self[f.lower()]
            if not os.path.exists(path):
                LOG.error('Audio player using system - file not found {}'.format(path))
                return
        try:
            cmd = '''gst-launch-1.0 playbin uri='file://%s' ''' % path
            self.p = subprocess.Popen(cmd, shell=True,
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, close_fds=True)
        except:
            LOG.error('Audio player using system - file not found {}'.format(f))

    # jump to a builtin alert sound
    # we use this so we can trap errors easily
    def jump(self, w, f):
        if 'beep' in f.lower():
            self[f.lower()]()
            return
        elif 'speak' in f.lower():
            self.os_speak(f)
            return
        if self.player.get_state(Gst.State.NULL) == Gst.State.PLAYING:
            self.player.set_state(Gst.State.NULL)
        sfile = os.path.expanduser(f)
        if os.path.exists(sfile):
            self.player.set_property("uri", "file://" + sfile)
            self.player.set_state(Gst.State.PLAYING)
        else:
            try:
                self['play_%s' % f.lower()]()
            except:
                LOG.error('Audio player - Canned alert not found {}: {}'.format(f, self[f.lower()]))

    # this gets messages back from GStreamer to control playback
    def on_message(self, bus, message):
        t = message.type
        if t == Gst.MessageType.EOS:
            # file ended, stop
            self.player.set_state(Gst.State.NULL)
        elif t == Gst.MessageType.ERROR:
            # Error occurred, print and stop
            self.player.set_state(Gst.State.NULL)
            err, debug = message.parse_error()
            LOG.error('Audio player - Error {}'.format(err, debug))
            # print "Error: %s" % err, debug

    # play builtin alert sounds
    def play_error(self):
        self.player.set_property("uri", "file://" + self.error)
        self.player.set_state(Gst.State.PLAYING)

    def play_ready(self):
        self.player.set_property("uri", "file://" + self.ready)
        self.player.set_state(Gst.State.PLAYING)

    def play_attention(self):
        self.player.set_property("uri", "file://" + self.attention)
        self.player.set_state(Gst.State.PLAYING)

    def play_ring(self):
        self.player.set_property("uri", "file://" + self.ring)
        self.player.set_state(Gst.State.PLAYING)

    def play_done(self):
        self.player.set_property("uri", "file://" + self.done)
        self.player.set_state(Gst.State.PLAYING)

    def play_login(self):
        self.player.set_property("uri", "file://" + self.logout)
        self.player.set_state(Gst.State.PLAYING)

    def play_logout(self):
        self.player.set_property("uri", "file://" + self.login)
        self.player.set_state(Gst.State.PLAYING)

    def play_bell(self):
        self.player.set_property("uri", "file://" + self.bell)
        self.player.set_state(Gst.State.PLAYING)

    ################
    # beep functions
    ################
    def beep_ring(self):
        os.system('''for n in 1 2 3 ; do
            for f in 1 2 1 2 1 2 1 2 1 2 ; do
            beep -f ${f}000 -l 20
            done
            done''')

    def beep_start(self):
        os.system('''beep -f165.4064 -l100 -n -f130.813 -l100 -n \
            -f261.626 -l100 -n -f523.251 -l100 -n -f1046.50 -l100 \
            -n -f2093.00 -l100 -n -f4186.01 -l10''')

    def beep(self):
        os.system("beep -f 555 ")

    ###################
    # Espeak functions
    ###################
    def os_speak(self, f):
        cmd = f.lower().lstrip('speak')
        if ESPEAK:
            if '_kill_' in cmd:
                if espeak.is_playing():
                    self.speak_cancel()
                return
            try:
                # uses a queue so doesn't speak over it's self.
                esQueue.put(cmd)
                if not espeak.is_playing():
                    espeak.synth(esQueue.get())
            except Exception as e:
                #print ('oops',e)
                # fallback call the system espeak - no queue used
                os.system('''espeak -s 160 -v m3 -p 1 "%s" &''' % cmd)

    # when sentences ends, start the next one, until there are none. 
    def speak_finished(self, *args):
        if args[0] == espeak.event_MSG_TERMINATED:
            if not esQueue.empty():
                espeak.synth(esQueue.get())

    def speak_cancel(self):
        espeak.cancel()

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


if __name__ == "__main__":
    import gi
    from gi.repository import GObject as gobject
    from gi.repository import GLib
    try:
        test = Player()
        test.play_error()
        print('done')
        G = GLib.MainLoop()
        G.run()

    except Exception as e:
        print(e)
