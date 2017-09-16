#!/usr/bin/env python
# vim: sts=4 sw=4 et
#    This is a component of LinuxCNC
#    complex.py Copyright 2010 Michael Haberler
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.''''''
'''
    gladevcp complex demo example
    Michael Haberler 11/2010

'''

import os,sys
from gladevcp.persistence import IniFile,widget_defaults,set_debug,select_widgets
import hal
import hal_glib
import gtk
import glib

debug = 0

class HandlerClass:
    '''
    class with gladevcp callback handlers
    '''

    def on_button_press(self,halwidget,data=None):
        '''
        a callback method
        parameters are:
            the generating object instance, like a GtkButton instance
            user data passed if any - this is currently unused but
            the convention should be retained just in case
        '''
        print "on_button_press()"
        self.nhits += 1
        halwidget.set_label("hits: %d" % self.nhits)


    def on_toggle_button(self,hal_button,data=None):
        '''
        standard button-pressed callback. Parameter is the widget object instance.
        '''
        print "on_toggle_button() HAL pin value: %s" %(str(hal_button.hal_pin.get()))

    def _on_example_trigger_change(self,hal_pin,data=None):
        '''
        executed when the 'example-trigger' hal pin value changes
        note that this callback will not be exposed as potential callback
        handler through gladevcp since the name begins with an underscore (_) .
        Parameter is the HAL pin instance.
        '''
        print "_on_example_trigger_change() - HAL pin value: %s" % (hal_pin.get())

    def on_led_pin_changed(self,hal_led,data=None):
        '''
        this is an example of a hal-pin-changed signal handler as set in glade.
        The purpose of this callback is to deliver an optional notification to your code beyond
        just reacting to the changed HAL pin.
        the on_led_pin_changed signal is set in the complex.ui hal_led1 signals section
        '''
        print "on_led_pin_changed() - HAL pin value:",hal_led.hal_pin.get()

    def _on_timer_tick(self,userdata=None):
        '''
        the full glib functionality is available if needed.
        here's a timer function which will be called periodically
        returning True restarts the timer
        returning False makes it a one-shot
        '''
        self.lifetime_ticks += 1
        self.builder.get_object('message').hal_pin.set(self.lifetime_ticks)
        return True


    def on_unix_signal(self,signum,stack_frame):
        print "on_unix_signal(): signal %d received, saving state" % (signum)
        self.ini.save_state(self)
        gtk.main_quit()
        self.halcomp.exit()

    def on_destroy(self,obj,data=None):
        '''
        gladevcp_demo.ui has a destroy callback set in the window1 Gobject
        note the widget tree is not safely accessible here any more
        '''
        print "on_destroy() - saving state)"
        self.ini.save_state(self)

    def on_restore_defaults(self,button,data=None):
        self.ini.create_default_ini()
        self.ini.restore_state(self)
        self.builder.get_object('hal_button1').set_label("past hits: %d" % self.nhits)

    def on_save_settings(self,button,data=None):
        print "on_save_settings() - saving state"
        self.ini.save_state(self)

    def _hal_setup(self,halcomp, builder):
        '''
        hal related initialisation
        '''

        # the toplevel window is always named 'window1'. Do not change.
        # widgets may be accessed through builder.get_object() and
        # builder.get_objects()
        self.window1 = self.builder.get_object("window1")
        self.led1 = self.builder.get_object("hal_led1")

        # standard hal pins not associated with any widget
        self.halcomp.newpin("example-out", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("example-in", hal.HAL_S32, hal.HAL_IN)

        # hal pins with change callback. Also unrelated to any HAL widget.
        # When the pin's value changes the callback is executed.
        self.example_trigger = hal_glib.GPin(halcomp.newpin('example-trigger',  hal.HAL_BIT, hal.HAL_IN))
        self.example_trigger.connect('value-changed', self._on_example_trigger_change)

    def __init__(self, halcomp,builder,useropts):
        '''
        Handler classes are instantiated in the following state:
        - the widget tree is created, but not yet realized (no toplevel window.show() executed yet)
        - the halcomp HAL component is set up and the widhget tree's HAL pins have already been added to it
        - it is safe to add more hal pins because halcomp.ready() has not yet been called at this point.

        after all handlers are instantiated in command line and get_handlers() order, callbacks will be
        connected with connect_signals()/signal_autoconnect()

        The builder may be either of libglade or GtkBuilder type depending on the glade file format.
        '''
        self.halcomp = halcomp
        self.builder = builder

        (directory,filename) = os.path.split(__file__)
        (basename,extension) = os.path.splitext(filename)
        self.ini_filename = os.path.join(directory,basename + '.save')

        # the dict driving the ini file persistence feature
        # a signature checks variable names,types,sections
        #
        # to see the mechanism at work, do this:
        # - run the application, change some values, and exit
        # - edit the complex.save file and change a variable name in the widgets or vars section
        #   for example, rename 'a' to 'z'
        #   and remove one of the widgets in the widgets section
        # - then re-run the application
        # during startup, you get a message saying
        # "key 'a' in section 'vars' : missing"
        # "key 'hal_radiobutton1' in section 'widgets' : missing"
        #
        # to see how the protection of ini file versus a changed program works,
        # change the defaults dict below, for instance remove the 'c' : "a string variable
        # then re-run - the signature check at startup should fail, you should get:
        # "signature mismatch in ./complex.save -  resetting to default"
        # and a default ini file is generated

        self.defaults = {  # these will be saved/restored as method attributes
                            IniFile.vars: { 'nhits' : 0, 'lifetime_ticks': 0, 'a': 1.67, 'd': True, 'c' :  "a string"},

                            # we're interested restoring state to output HAL widgets only
                            # NB: this does NOT restore state pf plain gtk objects - set hal_only to False to do this
                            IniFile.widgets: widget_defaults(select_widgets(self.builder.get_objects(), hal_only=True,output_only = True)),
                       }

        self.ini = IniFile(self.ini_filename,self.defaults, self.builder)
        self.ini.restore_state(self)

        # at this point it is ok to refer to restored attributes like self.nhits and self.lifetime_ticks:
        self.builder.get_object('hal_button1').set_label("past hits: %d" % self.nhits)
        self.builder.get_object('message').hal_pin.set(self.lifetime_ticks)

        self._hal_setup(halcomp,builder)

        # demonstrate a slow background timer - granularity is one second
        # for a faster timer, use this:
        # glib.timeout_add(5000,  self._on_timer_tick)
        glib.timeout_add_seconds(1, self._on_timer_tick)


def get_handlers(halcomp,builder,useropts):
    '''
    this function is called by gladevcp at import time (when this module is passed with '-u <modname>.py')

    return a list of object instances whose methods should be connected as callback handlers
    any method whose name does not begin with an underscore ('_') is a  callback candidate

    the 'get_handlers' name is reserved - gladevcp expects it, so do not change
    '''

    # try this at command line:
    #  -U debug=42 -U "print 'debug=%d' % debug"
    global debug
    for cmd in useropts:
        exec cmd in globals()

    set_debug(debug)

    if debug: print "%s.get_handlers() called" % (__name__)

    return [HandlerClass(halcomp,builder,useropts)]
