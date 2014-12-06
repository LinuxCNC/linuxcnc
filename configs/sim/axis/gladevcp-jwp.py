#!/usr/bin/env python

# demo interface to motion jog-while-paused features

import os,sys
from gladevcp.persistence import IniFile,widget_defaults,set_debug,select_widgets
import hal
import hal_glib
import gtk
import glib
import linuxcnc

# see enum pause_state in src/emc/motion/mot_priv.h
states = ("running", "pausing", "paused", "paused/offset","jogging","returning", "pause/step","stepping")

# see src/emc/nml_intf/motion_types.h
motions = ("none", "traverse", "feed", "arc", "toolchange", "probing", "indexrotary")

class HandlerClass:

    def _on_state_changed(self,hal_pin,data=None):
        state = hal_pin.get()
        self.builder.get_object("state").set_text(states[state])

    def _on_motion_type_changed(self,hal_pin,data=None):
        self.builder.get_object("motion_type_label").set_text(motions[hal_pin.get()])

    def on_in_position_changed(self,hal_led,data=None):
        if self.state.get() and hal_led.hal_pin.get():
            self.s.poll()
            posfmt = " ".join(["%-8.4f"] * self.s.axes)
            posn = posfmt % self.s.position[:self.s.axes]
            print "new_position: ", posn, "motion type=",motions[self.motion_type.get()]

    def on_unix_signal(self,signum,stack_frame):
        print "on_unix_signal(): signal %d received, saving state" % (signum)
        self.ini.save_state(self)
        gtk.main_quit()
        self.halcomp.exit()

    def on_destroy(self,obj,data=None):
        print "on_destroy() - saving state"
        self.ini.save_state(self)

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder

        (directory,filename) = os.path.split(__file__)
        (basename,extension) = os.path.splitext(filename)
        self.ini_filename = os.path.join(directory,basename + '.ini')
        self.defaults = {  IniFile.vars: { },
                           IniFile.widgets: widget_defaults(select_widgets(self.builder.get_objects(),
                                                                           hal_only=True,output_only = True)),
                                                                           }
        self.ini = IniFile(self.ini_filename,self.defaults, self.builder)
        self.ini.restore_state(self)

        self.c = linuxcnc.command()
        self.e = linuxcnc.error_channel()
        self.s = linuxcnc.stat()

        self.state = hal_glib.GPin(halcomp.newpin('state', hal.HAL_S32, hal.HAL_IN))
        self.state.connect('value-changed', self._on_state_changed)

        self.motion_type  = hal_glib.GPin(halcomp.newpin('motion_type', hal.HAL_S32, hal.HAL_IN))
        self.motion_type.connect('value-changed', self._on_motion_type_changed)

def get_handlers(halcomp,builder,useropts, compname):
    return [HandlerClass(halcomp,builder,useropts)]
