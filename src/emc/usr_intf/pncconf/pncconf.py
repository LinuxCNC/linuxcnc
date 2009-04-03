#!/usr/bin/python2.4
# -*- encoding: utf-8 -*-
#    This is pncconf, a graphical configuration editor for EMC2
#    Chris Morley
#    This is based from stepconf, a graphical configuration editor for emc2
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
import sys
import os
import pwd
import errno
import time
import md5
import pickle
import shutil
import math
import getopt
import textwrap

import gobject
import gtk
import gtk.glade
import gnome.ui

import xml.dom.minidom

import traceback

# otherwise, on hardy the user is shown spurious "[application] closed
# unexpectedly" messages but denied the ability to actually "report [the]
# problem"
def excepthook(exc_type, exc_obj, exc_tb):
    try:
        w = app.widgets.window1
    except NameError:
        w = None
    lines = traceback.format_exception(exc_type, exc_obj, exc_tb)
    m = gtk.MessageDialog(w,
                gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_ERROR, gtk.BUTTONS_OK,
                _("PNCconf encountered an error.  The following "
                "information may be useful in troubleshooting:\n\n")
                + "".join(lines))
    m.show()
    m.run()
    m.destroy()
sys.excepthook = excepthook

BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
import gettext;
#def _(x): return x
gettext.install("axis", localedir=LOCALEDIR, unicode=True)
gtk.glade.bindtextdomain("axis", LOCALEDIR)
gtk.glade.textdomain("axis")

def iceil(x):
    if isinstance(x, (int, long)): return x
    if isinstance(x, basestring): x = float(x)
    return int(math.ceil(x))

datadir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "emc")

wizard = os.path.join(datadir, "emc2-wizard.gif")
if not os.path.isfile(wizard):
    wizard = os.path.join("/etc/emc2/emc2-wizard.gif")
if not os.path.isfile(wizard):
    wizdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
    wizard = os.path.join(wizdir, "emc2-wizard.gif")

distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "configs", "common")
if not os.path.isdir(distdir):
    distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "emc2", "sample-configs", "common")
if not os.path.isdir(distdir):
    distdir = "/etc/emc2/sample-configs/common"

(UNUSED_OUTPUT,
ON, CW, CCW, PWM, BRAKE,
MIST, FLOOD, ESTOP, AMP,
PUMP, DOUT0, DOUT1, DOUT2, DOUT3) = hal_output_names = [
"unused-output", 
"spindle-on", "spindle-cw", "spindle-ccw", "spindle-pwm", "spindle-brake",
"coolant-mist", "coolant-flood", "estop-out", "xenable",
"charge-pump", "dout-00", "dout-01", "dout-02", "dout-03"
]

(UNUSED_INPUT, ESTOP_IN, PROBE, PPR, PHA, PHB,
HOME_X, HOME_Y, HOME_Z, HOME_A,
MIN_HOME_X, MIN_HOME_Y, MIN_HOME_Z, MIN_HOME_A,
MAX_HOME_X, MAX_HOME_Y, MAX_HOME_Z, MAX_HOME_A,
BOTH_HOME_X, BOTH_HOME_Y, BOTH_HOME_Z, BOTH_HOME_A,
MIN_X, MIN_Y, MIN_Z, MIN_A,
MAX_X, MAX_Y, MAX_Z, MAX_A,
BOTH_X, BOTH_Y, BOTH_Z, BOTH_A,
ALL_LIMIT, ALL_HOME, DIN0, DIN1, DIN2, 
DIN3) = hal_input_names = [
"unused-input", "estop-ext", "probe-in", "spindle-index", "spindle-phase-a", "spindle-phase-b",
"home-x", "home-y", "home-z", "home-a",
"min-home-x", "min-home-y", "min-home-z", "min-home-a",
"max-home-x", "max-home-y", "max-home-z", "max-home-a",
"both-home-x", "both-home-y", "both-home-z", "both-home-a",
"min-x", "min-y", "min-z", "min-a",
"max-x", "max-y", "max-z", "max-a",
"both-x", "both-y", "both-z", "both-a",
"all-limit", "all-home", "din-00", "din-01", "din-02", "din-03"]

human_output_names = [ _("Unused Output"),
_("Spindle ON"),_("Spindle CW"), _("Spindle CCW"), _("Spindle PWM"), _("Spindle Brake"),
_("Coolant Mist"), _("Coolant Flood"), _("ESTOP Out"), _("Amplifier Enable"),
_("Charge Pump"),
_("Digital out 0"), _("Digital out 1"), _("Digital out 2"), _("Digital out 3")]

human_input_names = [ _("Unused Input"), _("ESTOP In"), _("Probe In"),
_("Spindle Index"), _("Spindle Phase A"), _("Spindle Phase B"),
_("Home X"), _("Home Y"), _("Home Z"), _("Home A"),
_("Minimum Limit + Home X"), _("Minimum Limit + Home Y"),
_("Minimum Limit + Home Z"), _("Minimum Limit + Home A"),
_("Maximum Limit + Home X"), _("Maximum Limit + Home Y"),
_("Maximum Limit + Home Z"), _("Maximum Limit + Home A"),
_("Both Limit + Home X"), _("Both Limit + Home Y"),
_("Both Limit + Home Z"), _("Both Limit + Home A"),
_("Minimum Limit X"), _("Minimum Limit Y"),
_("Minimum Limit Z"), _("Minimum Limit A"),
_("Maximum Limit X"), _("Maximum Limit Y"),
_("Maximum Limit Z"), _("Maximum Limit A"),
_("Both Limit X"), _("Both Limit Y"),
_("Both Limit Z"), _("Both Limit A"),
_("All limits"), _("All home"),
_("Digital in 0"), _("Digital in 1"), _("Digital in 2"), _("Digital in 3")]

human_names_shared_home = [_("Minimum Limit + Home X"), _("Minimum Limit + Home Y"),
_("Minimum Limit + Home Z"), _("Minimum Limit + Home A"),
_("Maximum Limit + Home X"), _("Maximum Limit + Home Y"),
_("Maximum Limit + Home Z"), _("Maximum Limit + Home A"),
_("Both Limit + Home X"), _("Both Limit + Home Y"),
_("Both Limit + Home Z"), _("Both Limit + Home A")]

human_names_limit_only = [ _("Minimum Limit X"), _("Minimum Limit Y"),
_("Minimum Limit Z"), _("Minimum Limit A"),
_("Maximum Limit X"), _("Maximum Limit Y"),
_("Maximum Limit Z"), _("Maximum Limit A"),
_("Both Limit X"), _("Both Limit Y"),
_("Both Limit Z"), _("Both Limit A"),_("All limits"),("All home")]

(UNUSED_OUTPUT, X_PWM, Y_PWM, Z_PWM, A_PWM, SPINDLE_PWM) = hal_servo_output_names = [
"unused-output","xpwm", "ypwm", "zpwm", "apwm", "spindlepwm"]

human_servo_output_names =[ _("Unused PWM Gen"), _("X Hardware PWM Gen"), _("Y Hardware PWM Gen"),
 _("Z Hardware PWM Gen"), _("A Hardware PWM Gen"), _("Spindle Hrdware PWM Gen")]

(UNUSED_INPUT, X_ENCODER_A, X_ENCODER_B, X_ENCODER_I, Y_ENCODER_A, Z_ENCODER_A, A_ENCODER_A, SPINDLE_ENCODER_A, 
X_MPG, Y_MPG, Z_MPG, SELECT_MPG)  = hal_servo_input_names = [
 "unused-input", "x-encoder-a", "x-encoder-b", "x-encoder-i", "y-encoder-a","z-encoder-a", "a-encoder-a", "spindle-encoder-a",
"X-mpg-a", "y-mpg-a", "z-mpg-a", "select-mpg-a"]

human_servo_input_names = [ _("Unused Encoder-A"), _("X Hardware Encoder-A"), _("X Hardware Encoder-B"), _("X Hardware Encoder-I"), _("Y Hardware Encoder"), _("Z Hardware Encoder"),
_("A Hardware Encoder"), _("Spindle Hrdware Encoder"),_("X Hand Wheel"),_("Y Hand wheel"), _("Z Hand Wheel"),
_("Selectable Axis Hand Wheel")]

(UNUSED_STEPGEN, X_STEPGEN, Y_STEPGEN, Z_STEPGEN, A_STEPGEN) = hal_stepper_names = ["unused", "x-stepgen", "y-stepgen", 
"z-stepgen", "a-stepgen"]

human_stepper_names = [_("Unused StepGen"), _("X Hardware StepGen"), _("Y Hardware StepGen"), _("Z Hardware StepGen"), _("A Hardware StepGen")]

pintype_names = [_("GPIO Input"),_("GPIO Output"),_("GPIO O Drain"),_("HDW Encoder"),_("HDW PWM Gen"),_("HDW Step Gen")]

def md5sum(filename):
    try:
        f = open(filename, "rb")
    except IOError:
        return None
    else:
        return md5.new(f.read()).hexdigest()

class Widgets:
    def __init__(self, xml):
        self._xml = xml
    def __getattr__(self, attr):
        r = self._xml.get_widget(attr)
        if r is None: raise AttributeError, "No widget %r" % attr
        return r
    def __getitem__(self, attr):
        r = self._xml.get_widget(attr)
        if r is None: raise IndexError, "No widget %r" % attr
        return r

class Data:
    def __init__(self):
        pw = pwd.getpwuid(os.getuid())
        self.machinename = _("my_EMC_machine")
        self.frontend = 1 # AXIS
        self.axes = 0 # XYZ
        self.units = 0 # inch
        self.drivertype = "other"
        self.steptime = 5000
        self.stepspace = 5000
        self.dirhold = 20000 
        self.dirsetup = 20000
        self.latency = 15000
        self.period = 25000

        self.mesa5i20 = 1
        self.firstpp_direction = 1 # output
        self.ioaddr = "0x378"
        self.ioaddr2 = _("Enter Address")
        self.Secondpp_direction = 0 # input
        self.ioaddr3 = _("Enter Address")
        self.Thirdpp_direction = 0 # input
        self.number_pports = 0
        self.limitsnone = False
        self.limitswitch = False
        self.limitshared = True
        self.homenone = False
        self.homeswitch = False
        self.homeindex = False
        self.homeboth = True

        self.manualtoolchange = 1
        self.customhal = 1 # include custom hal file
        self.pyvcp = 0 # not included
        self.pyvcpname = "custom.xml"
        self.pyvcphaltype = 0 # no HAL connections specified
        self.pyvcpconnect = 1 # HAL connections allowed

        self.classicladder = 0 # not included
        self.tempexists = 0 # not present
        self.laddername = "custom.clp"
        self.modbus = 0
        self.ladderhaltype = 0 # no HAL connections specified
        self.ladderconnect = 1 # HAL connections allowed

        self.servoinputsignames = []
        self.servooutputsignames = []
        self.halinputsignames = []
        self.haloutputsignames = []
        self.halsteppersignames = []

        self.firstpppin1invout = 0
        self.firstpppin2invout = 0
        self.firstpppin3invout = 0
        self.firstpppin4invout = 0
        self.firstpppin5invout = 0
        self.firstpppin6invout = 0
        self.firstpppin7invout = 0
        self.firstpppin8invout = 0
  
        self.firstpppin1invin = 0
        self.firstpppin2invin = 0
        self.firstpppin3invin = 0
        self.firstpppin4invin = 0
        self.firstpppin5invin = 0
        self.firstpppin6invin = 0
        self.firstpppin7invin = 0
        self.firstpppin8invin = 0

        self.firstpppin9invout = 0
        self.firstpppin10invin = 0
        self.firstpppin11invin = 0
        self.firstpppin12invin = 0
        self.firstpppin13invin = 0
        self.firstpppin14invout = 0
        self.firstpppin15invin = 0
        self.firstpppin16invout = 0
        self.firstpppin17invout = 0

        self.firstpppin1out = UNUSED_OUTPUT
        self.firstpppin2out = UNUSED_OUTPUT
        self.firstpppin3out = UNUSED_OUTPUT
        self.firstpppin4out = UNUSED_OUTPUT
        self.firstpppin5out = UNUSED_OUTPUT
        self.firstpppin6out = UNUSED_OUTPUT
        self.firstpppin7out = UNUSED_OUTPUT
        self.firstpppin8out = UNUSED_OUTPUT
        self.firstpppin9out = UNUSED_OUTPUT
        self.firstpppin14out = UNUSED_OUTPUT
        self.firstpppin16out = UNUSED_OUTPUT
        self.firstpppin17out = UNUSED_OUTPUT
        
        self.firstpppin1in = UNUSED_INPUT
        self.firstpppin2in = UNUSED_INPUT
        self.firstpppin3in = UNUSED_INPUT
        self.firstpppin4in = UNUSED_INPUT
        self.firstpppin5in = UNUSED_INPUT
        self.firstpppin6in = UNUSED_INPUT
        self.firstpppin7in = UNUSED_INPUT
        self.firstpppin8in = UNUSED_INPUT

        self.firstpppin10in = UNUSED_INPUT
        self.firstpppin11in = UNUSED_INPUT
        self.firstpppin12in = UNUSED_INPUT
        self.firstpppin13in = UNUSED_INPUT
        self.firstpppin15in = UNUSED_INPUT

        self.Secondpppin1invout = 0
        self.Secondpppin2invout = 0
        self.Secondpppin3invout = 0
        self.Secondpppin4invout = 0
        self.Secondpppin5invout = 0
        self.Secondpppin6invout = 0
        self.Secondpppin7invout = 0
        self.Secondpppin8invout = 0
  
        self.Secondpppin1invin = 0
        self.Secondpppin2invin = 0
        self.Secondpppin3invin = 0
        self.Secondpppin4invin = 0
        self.Secondpppin5invin = 0
        self.Secondpppin6invin = 0
        self.Secondpppin7invin = 0
        self.Secondpppin8invin = 0

        self.Secondpppin9invout = 0
        self.Secondpppin10invin = 0
        self.Secondpppin11invin = 0
        self.Secondpppin12invin = 0
        self.Secondpppin13invin = 0
        self.Secondpppin14invout = 0
        self.Secondpppin15invin = 0
        self.Secondpppin16invout = 0
        self.Secondpppin17invout = 0

        self.Secondpppin1out = UNUSED_OUTPUT
        self.Secondpppin2out = UNUSED_OUTPUT
        self.Secondpppin3out = UNUSED_OUTPUT
        self.Secondpppin4out = UNUSED_OUTPUT
        self.Secondpppin5out = UNUSED_OUTPUT
        self.Secondpppin6out = UNUSED_OUTPUT
        self.Secondpppin7out = UNUSED_OUTPUT
        self.Secondpppin8out = UNUSED_OUTPUT
        self.Secondpppin9out = UNUSED_OUTPUT
        self.Secondpppin14out = UNUSED_OUTPUT
        self.Secondpppin16out = UNUSED_OUTPUT
        self.Secondpppin17out = UNUSED_OUTPUT
        
        self.Secondpppin1in = UNUSED_INPUT
        self.Secondpppin2in = UNUSED_INPUT
        self.Secondpppin3in = UNUSED_INPUT
        self.Secondpppin4in = UNUSED_INPUT
        self.Secondpppin5in = UNUSED_INPUT
        self.Secondpppin6in = UNUSED_INPUT
        self.Secondpppin7in = UNUSED_INPUT
        self.Secondpppin8in = UNUSED_INPUT

        self.Secondpppin10in = UNUSED_INPUT
        self.Secondpppin11in = UNUSED_INPUT
        self.Secondpppin12in = UNUSED_INPUT
        self.Secondpppin13in = UNUSED_INPUT
        self.Secondpppin15in = UNUSED_INPUT
        
        self.Thirdpppin1invout = 0
        self.Thirdpppin2invout = 0
        self.Thirdpppin3invout = 0
        self.Thirdpppin4invout = 0
        self.Thirdpppin5invout = 0
        self.Thirdpppin6invout = 0
        self.Thirdpppin7invout = 0
        self.Thirdpppin8invout = 0
  
        self.Thirdpppin1invin = 0
        self.Thirdpppin2invin = 0
        self.Thirdpppin3invin = 0
        self.Thirdpppin4invin = 0
        self.Thirdpppin5invin = 0
        self.Thirdpppin6invin = 0
        self.Thirdpppin7invin = 0
        self.Thirdpppin8invin = 0

        self.Thirdpppin9invout = 0
        self.Thirdpppin10invin = 0
        self.Thirdpppin11invin = 0
        self.Thirdpppin12invin = 0
        self.Thirdpppin13invin = 0
        self.Thirdpppin14invout = 0
        self.Thirdpppin15invin = 0
        self.Thirdpppin16invout = 0
        self.Thirdpppin17invout = 0

        self.Thirdpppin1out = UNUSED_OUTPUT
        self.Thirdpppin2out = UNUSED_OUTPUT
        self.Thirdpppin3out = UNUSED_OUTPUT
        self.Thirdpppin4out = UNUSED_OUTPUT
        self.Thirdpppin5out = UNUSED_OUTPUT
        self.Thirdpppin6out = UNUSED_OUTPUT
        self.Thirdpppin7out = UNUSED_OUTPUT
        self.Thirdpppin8out = UNUSED_OUTPUT
        self.Thirdpppin9out = UNUSED_OUTPUT
        self.Thirdpppin14out = UNUSED_OUTPUT
        self.Thirdpppin16out = UNUSED_OUTPUT
        self.Thirdpppin17out = UNUSED_OUTPUT
        
        self.Thirdpppin1in = UNUSED_INPUT
        self.Thirdpppin2in = UNUSED_INPUT
        self.Thirdpppin3in = UNUSED_INPUT
        self.Thirdpppin4in = UNUSED_INPUT
        self.Thirdpppin5in = UNUSED_INPUT
        self.Thirdpppin6in = UNUSED_INPUT
        self.Thirdpppin7in = UNUSED_INPUT
        self.Thirdpppin8in = UNUSED_INPUT

        self.Thirdpppin10in = UNUSED_INPUT
        self.Thirdpppin11in = UNUSED_INPUT
        self.Thirdpppin12in = UNUSED_INPUT
        self.Thirdpppin13in = UNUSED_INPUT
        self.Thirdpppin15in = UNUSED_INPUT

        self.m5i20c2pin0 = X_ENCODER_A
        self.m5i20c2pin1 = Y_ENCODER_A
        self.m5i20c2pin2 = X_ENCODER_B
        self.m5i20c2pin3 = Y_ENCODER_A
        self.m5i20c2pin4 = X_ENCODER_I
        self.m5i20c2pin5 = UNUSED_INPUT
        self.m5i20c2pin6 = X_PWM
        self.m5i20c2pin7 = UNUSED_OUTPUT
        self.m5i20c2pin8 = UNUSED_OUTPUT
        self.m5i20c2pin9 = UNUSED_OUTPUT
        self.m5i20c2pin10 = UNUSED_OUTPUT
        self.m5i20c2pin11 = UNUSED_OUTPUT
        self.m5i20c2pin12 = UNUSED_INPUT
        self.m5i20c2pin13 = UNUSED_INPUT
        self.m5i20c2pin14 = UNUSED_INPUT
        self.m5i20c2pin15 = UNUSED_INPUT    
        self.m5i20c2pin16 = UNUSED_INPUT
        self.m5i20c2pin17 = UNUSED_INPUT
        self.m5i20c2pin18 = UNUSED_OUTPUT
        self.m5i20c2pin19 = UNUSED_OUTPUT
        self.m5i20c2pin20 = UNUSED_OUTPUT
        self.m5i20c2pin21 = UNUSED_OUTPUT
        self.m5i20c2pin22 = UNUSED_OUTPUT
        self.m5i20c2pin23 = UNUSED_STEPGEN

        self.m5i20c2pin0inv = False
        self.m5i20c2pin1inv = False
        self.m5i20c2pin2inv = False
        self.m5i20c2pin3inv = False
        self.m5i20c2pin4inv = False
        self.m5i20c2pin5inv = False
        self.m5i20c2pin6inv = False
        self.m5i20c2pin7inv = False
        self.m5i20c2pin8inv = False
        self.m5i20c2pin9inv = False
        self.m5i20c2pin10inv = False
        self.m5i20c2pin11inv = False
        self.m5i20c2pin12inv = False
        self.m5i20c2pin13inv = False
        self.m5i20c2pin14inv = False
        self.m5i20c2pin15inv = False
        self.m5i20c2pin16inv = False
        self.m5i20c2pin17inv = False
        self.m5i20c2pin18inv = False
        self.m5i20c2pin19inv = False
        self.m5i20c2pin20inv = False
        self.m5i20c2pin21inv = False
        self.m5i20c2pin22inv = False
        self.m5i20c2pin23inv = False

        self.m5i20c2pin0type = 3
        self.m5i20c2pin1type = 3
        self.m5i20c2pin2type = 3
        self.m5i20c2pin3type = 3
        self.m5i20c2pin4type = 3
        self.m5i20c2pin5type = 3
        self.m5i20c2pin6type = 4
        self.m5i20c2pin7type = 4
        self.m5i20c2pin8type = 4
        self.m5i20c2pin9type = 4
        self.m5i20c2pin10type = 4
        self.m5i20c2pin11type = 4
        self.m5i20c2pin12type = 3
        self.m5i20c2pin13type = 3
        self.m5i20c2pin14type = 3
        self.m5i20c2pin15type = 3
        self.m5i20c2pin16type = 3
        self.m5i20c2pin17type = 3
        self.m5i20c2pin18type = 4
        self.m5i20c2pin19type = 4
        self.m5i20c2pin20type = 4
        self.m5i20c2pin21type = 4
        self.m5i20c2pin22type = 4
        self.m5i20c2pin23type = 5

        self.m5i20c3pin0 = UNUSED_INPUT
        self.m5i20c3pin1 = UNUSED_INPUT
        self.m5i20c3pin2 = UNUSED_INPUT
        self.m5i20c3pin3 = UNUSED_INPUT
        self.m5i20c3pin4 = UNUSED_INPUT
        self.m5i20c3pin5 = UNUSED_INPUT
        self.m5i20c3pin6 = UNUSED_INPUT
        self.m5i20c3pin7 = UNUSED_INPUT
        self.m5i20c3pin8 = UNUSED_INPUT
        self.m5i20c3pin9 = UNUSED_INPUT
        self.m5i20c3pin10 = UNUSED_INPUT
        self.m5i20c3pin11 = UNUSED_INPUT
        self.m5i20c3pin12 = UNUSED_INPUT
        self.m5i20c3pin13 = UNUSED_INPUT
        self.m5i20c3pin14 = UNUSED_INPUT
        self.m5i20c3pin15 = UNUSED_INPUT
    
        self.m5i20c3pin16 = UNUSED_OUTPUT
        self.m5i20c3pin17 = UNUSED_OUTPUT
        self.m5i20c3pin18 = UNUSED_OUTPUT
        self.m5i20c3pin19 = UNUSED_OUTPUT
        self.m5i20c3pin20 = UNUSED_OUTPUT
        self.m5i20c3pin21 = UNUSED_OUTPUT
        self.m5i20c3pin22 = UNUSED_OUTPUT
        self.m5i20c3pin23 = UNUSED_OUTPUT

        self.m5i20c3pin0inv = False
        self.m5i20c3pin1inv = False
        self.m5i20c3pin2inv = False
        self.m5i20c3pin3inv = False
        self.m5i20c3pin4inv = False
        self.m5i20c3pin5inv = False
        self.m5i20c3pin6inv = False
        self.m5i20c3pin7inv = False
        self.m5i20c3pin8inv = False
        self.m5i20c3pin9inv = False
        self.m5i20c3pin10inv = False
        self.m5i20c3pin11inv = False
        self.m5i20c3pin12inv = False
        self.m5i20c3pin13inv = False
        self.m5i20c3pin14inv = False
        self.m5i20c3pin15inv = False
        self.m5i20c3pin16inv = False
        self.m5i20c3pin17inv = False
        self.m5i20c3pin18inv = False
        self.m5i20c3pin19inv = False
        self.m5i20c3pin20inv = False
        self.m5i20c3pin21inv = False
        self.m5i20c3pin22inv = False
        self.m5i20c3pin23inv = False

        self.m5i20c3pin0type = 0
        self.m5i20c3pin1type = 0
        self.m5i20c3pin2type = 0
        self.m5i20c3pin3type = 0
        self.m5i20c3pin4type = 0
        self.m5i20c3pin5type = 0
        self.m5i20c3pin6type = 0
        self.m5i20c3pin7type = 0
        self.m5i20c3pin8type = 0
        self.m5i20c3pin9type = 0
        self.m5i20c3pin10type = 0
        self.m5i20c3pin11type = 0
        self.m5i20c3pin12type = 0
        self.m5i20c3pin13type = 0
        self.m5i20c3pin14type = 0
        self.m5i20c3pin15type = 0
        self.m5i20c3pin16type = 1
        self.m5i20c3pin17type = 1
        self.m5i20c3pin18type = 1
        self.m5i20c3pin19type = 1
        self.m5i20c3pin20type = 1
        self.m5i20c3pin21type = 1
        self.m5i20c3pin22type = 1
        self.m5i20c3pin23type = 1

        self.m5i20c4pin0 = UNUSED_INPUT
        self.m5i20c4pin1 = UNUSED_INPUT
        self.m5i20c4pin2 = UNUSED_INPUT
        self.m5i20c4pin3 = UNUSED_INPUT
        self.m5i20c4pin4 = UNUSED_INPUT
        self.m5i20c4pin5 = UNUSED_INPUT
        self.m5i20c4pin6 = UNUSED_INPUT
        self.m5i20c4pin7 = UNUSED_INPUT
        self.m5i20c4pin8 = UNUSED_INPUT
        self.m5i20c4pin9 = UNUSED_INPUT
        self.m5i20c4pin10 = UNUSED_INPUT
        self.m5i20c4pin11 = UNUSED_INPUT
        self.m5i20c4pin12 = UNUSED_INPUT
        self.m5i20c4pin13 = UNUSED_INPUT
        self.m5i20c4pin14 = UNUSED_INPUT
        self.m5i20c4pin15 = UNUSED_INPUT
    
        self.m5i20c4pin16 = UNUSED_OUTPUT
        self.m5i20c4pin17 = UNUSED_OUTPUT
        self.m5i20c4pin18 = UNUSED_OUTPUT
        self.m5i20c4pin19 = UNUSED_OUTPUT
        self.m5i20c4pin20 = UNUSED_OUTPUT
        self.m5i20c4pin21 = UNUSED_OUTPUT
        self.m5i20c4pin22 = UNUSED_OUTPUT
        self.m5i20c4pin23 = UNUSED_OUTPUT
    
        self.m5i20c4pin0inv = False
        self.m5i20c4pin1inv = False
        self.m5i20c4pin2inv = False
        self.m5i20c4pin3inv = False
        self.m5i20c4pin4inv = False
        self.m5i20c4pin5inv = False
        self.m5i20c4pin6inv = False
        self.m5i20c4pin7inv = False
        self.m5i20c4pin8inv = False
        self.m5i20c4pin9inv = False
        self.m5i20c4pin10inv = False
        self.m5i20c4pin11inv = False
        self.m5i20c4pin12inv = False
        self.m5i20c4pin13inv = False
        self.m5i20c4pin14inv = False
        self.m5i20c4pin15inv = False
        self.m5i20c4pin16inv = False
        self.m5i20c4pin17inv = False
        self.m5i20c4pin18inv = False
        self.m5i20c4pin19inv = False
        self.m5i20c4pin20inv = False
        self.m5i20c4pin21inv = False
        self.m5i20c4pin22inv = False
        self.m5i20c4pin23inv = False

        self.m5i20c4pin0type = 0
        self.m5i20c4pin1type = 0
        self.m5i20c4pin2type = 0
        self.m5i20c4pin3type = 0
        self.m5i20c4pin4type = 0
        self.m5i20c4pin5type = 0
        self.m5i20c4pin6type = 0
        self.m5i20c4pin7type = 0
        self.m5i20c4pin8type = 0
        self.m5i20c4pin9type = 0
        self.m5i20c4pin10type = 0
        self.m5i20c4pin11type = 0
        self.m5i20c4pin12type = 0
        self.m5i20c4pin13type = 0
        self.m5i20c4pin14type = 0
        self.m5i20c4pin15type = 0
        self.m5i20c4pin16type = 1
        self.m5i20c4pin17type = 1
        self.m5i20c4pin18type = 1
        self.m5i20c4pin19type = 1
        self.m5i20c4pin20type = 1
        self.m5i20c4pin21type = 1
        self.m5i20c4pin22type = 1
        self.m5i20c4pin23type = 1

        self.xsteprev = 200
        self.xmicrostep = 2
        self.xpulleynum = 1
        self.xpulleyden = 1
        self.xleadscrew = 5
        self.xusecomp = 0
        self.xcompfilename = "xcompensation"
        self.xcomptype = 0
        self.xusebacklash = 0
        self.xbacklash = 0
        self.xmaxvel = .0167
        self.xmaxacc = 2
        self.xinvertmotor = 1
        self.xinvertencoder = 0
        self.xoutputscale = 1
        self.xoutputoffset = 0
        self.xmaxoutput = 10
        self.xP = 1.0
        self.xI = 0
        self.xD = 0
        self.xFF0 = 0
        self.xFF1 = 0
        self.xFF2 = 0
        self.xminferror = .0005
        self.xmaxferror = .005
        self.xhomepos = 0
        self.xminlim =  0
        self.xmaxlim =  8
        self.xhomesw =  0
        self.xhomevel = .05
        self.xhomelatchvel = .025
        self.xhomefinalvel = 0
        self.xlatchdir = 0
        self.xusehomeindex = 1
        self.xhomesequence = 1203
        self.xencodercounts =4000
        self.xscale = 0

        self.ysteprev = 200
        self.ymicrostep = 2
        self.ypulleynum = 1
        self.ypulleyden = 1
        self.yleadscrew = 5
        self.yusecomp = 0
        self.ycompfilename = "ycompensation"
        self.ycomptype = 0
        self.yusebacklash = 0
        self.ybacklash = 0
        self.ymaxvel = .0167
        self.ymaxacc = 2
        self.yinvertmotor = 0
        self.yinvertencoder = 0
        self.youtputscale = 1
        self.youtputoffset = 0
        self.ymaxoutput = 10
        self.yP = 1
        self.yI = 0
        self.yD = 0
        self.yFF0 = 0
        self.yFF1 = 0
        self.yFF2 = 0
        self.yminferror = 0.125
        self.ymaxferror = 0.250
        self.yhomepos = 0
        self.yminlim =  0
        self.ymaxlim =  8
        self.yhomesw =  0
        self.yhomevel = .05
        self.yhomelatchvel = .025
        self.yhomefinalvel = 0
        self.ylatchdir = 0
        self.yusehomeindex = 0
        self.yencodercounts =4000
        self.yscale = 0
        
        self.zsteprev = 200
        self.zmicrostep = 2
        self.zpulleynum = 1
        self.zpulleyden = 1
        self.zleadscrew = 5
        self.zusecomp = 0
        self.zcompfilename = "zcompensation"
        self.zcomptype = 0
        self.zusebacklash = 0
        self.zbacklash = 0
        self.zmaxvel = .0167
        self.zmaxacc = 2
        self.zinvertmotor = 0
        self.zinvertencoder = 0
        self.zoutputscale = 1
        self.zoutputoffset = 0
        self.zmaxoutput = 10
        self.zP = 1
        self.zI = 0
        self.zD = 0
        self.zFF0 = 0
        self.zFF1 = 0
        self.zFF2 = 0
        self.zminferror = 0.0005
        self.zmaxferror = 0.005
        self.zhomepos = 0
        self.zminlim = -4
        self.zmaxlim =  0
        self.zhomesw = 0
        self.zhomevel = .05
        self.zhomelatchvel = .025
        self.zhomefinalvel = 0
        self.zlatchdir = 0
        self.zusehomeindex = 0
        self.zencodercounts =1000
        self.zscale = 0


        self.asteprev = 200
        self.amicrostep = 2
        self.apulleynum = 1
        self.apulleyden = 1
        self.aleadscrew = 8
        self.ausecomp = 0
        self.acompfilename = "acompensation"
        self.acomptype = 0
        self.ausebacklash = 0
        self.abacklash = 0
        self.amaxvel = 6
        self.amaxacc = 1
        self.ainvertmotor = 0
        self.ainvertencoder = 0
        self.aoutputscale = 1
        self.aoutputoffset = 0
        self.amaxoutput = 10
        self.aP = 1
        self.aI = 0
        self.aD = 0
        self.aFF0 = 0
        self.aFF1 = 0
        self.aFF2 = 0
        self.aminferror = 0.0005
        self.amaxferror = 0.005
        self.ahomepos = 0
        self.aminlim = -9999
        self.amaxlim =  9999
        self.ahomesw =  0
        self.ahomevel = .05
        self.ahomelatchvel = .025
        self.ahomefinalvel = 0
        self.alatchdir = 0
        self.ausehomeindex = 0
        self.aencodercounts = 1000
        self.ascale = 0

        self.spindlecarrier = 100
        self.spindlecpr = 100
        self.spindlespeed1 = 100
        self.spindlespeed2 = 800
        self.spindlepwm1 = .2
        self.spindlepwm2 = .8
        self.spindlefeedback = 0
        self.spindlecontrol = 0
        self.spindleoutputscale = 1
        self.spindleoutputoffset = 0
        self.spindlemaxoutput = 10
        self.spindlescale = 0

        self.limitstype = 0
        self.homingtype = 0

        self.digitsin = 15
        self.digitsout = 15
        self.s32in = 10
        self.s32out = 10
        self.floatsin = 10
        self.floatsout = 10
        self.halui = 0
        self.createsymlink = 1
        self.createshortcut = 0

    def load(self, filename, app=None, force=False):
        def str2bool(s):
            return s == 'True'

        converters = {'string': str, 'float': float, 'int': int, 'bool': str2bool, 'eval': eval}

        d = xml.dom.minidom.parse(open(filename, "r"))
        for n in d.getElementsByTagName("property"):
            name = n.getAttribute("name")
            conv = converters[n.getAttribute('type')]
            text = n.getAttribute('value')
            setattr(self, name, conv(text))
        for i in  self.servoinputsignames:
            hal_servo_input_names.append(i)
            human_servo_input_names.append(i)
        for i in  self.servooutputsignames:
            hal_servo_output_names.append(i)
            human_servo_output_names.append(i)
        for i in  self.halinputsignames:
            hal_input_names.append(i)
            human_input_names.append(i)
        for i in  self.haloutputsignames:
            hal_output_names.append(i)
            human_output_names.append(i)
        for i in  self.halsteppersignames:
            hal_stepper_names.append(i)
            human_stepper_names.append(i)


        warnings = []
        for f, m in self.md5sums:
            m1 = md5sum(f)
            if m1 and m != m1:
                warnings.append(_("File %r was modified since it was written by PNCconf") % f)
        if not warnings: return

        warnings.append("")
        warnings.append(_("Saving this configuration file will discard configuration changes made outside PNCconf."))
        if app:
            dialog = gtk.MessageDialog(app.widgets.window1,
                gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_WARNING, gtk.BUTTONS_OK,
                     "\n".join(warnings))
            dialog.show_all()
            dialog.run()
            dialog.destroy()
        else:
            for para in warnings:
                for line in textwrap.wrap(para, 78): print line
                print
            print
            if force: return
            response = raw_input(_("Continue? "))
            if response[0] not in _("yY"): raise SystemExit, 1

        


    def add_md5sum(self, filename, mode="r"):
        self.md5sums.append((filename, md5sum(filename)))

    def write_inifile(self, base):
        filename = os.path.join(base, self.machinename + ".ini")
        file = open(filename, "w")
        print >>file, _("# Generated by PNCconf at %s") % time.asctime()
        print >>file, _("# If you make changes to this file, they will be")
        print >>file, _("# overwritten when you run PNCconf again")

        print >>file
        print >>file, "[EMC]"
        print >>file, "MACHINE = %s" % self.machinename
        print >>file, "NML_FILE = emc.nml"
        print >>file, "DEBUG = 0"

        print >>file
        print >>file, "[DISPLAY]"
        print >>file, "DISPLAY = axis"
        print >>file, "EDITOR = gedit"
        print >>file, "POSITION_OFFSET = RELATIVE"
        print >>file, "POSITION_FEEDBACK = ACTUAL"
        print >>file, "MAX_FEED_OVERRIDE = 1.2"
        print >>file, "INTRO_GRAPHIC = emc2.gif"
        print >>file, "INTRO_TIME = 5"
        print >>file, "PROGRAM_PREFIX = %s" % \
                                    os.path.expanduser("~/emc2/nc_files")
        if self.units:
            print >>file, "INCREMENTS = 5mm 1mm .5mm .1mm .05mm .01mm .005mm"
        else:
            print >>file, "INCREMENTS = .1in .05in .01in .005in .001in .0005in .0001in"
        if self.pyvcp:
            print >>file, "PYVCP = custompanel.xml"

        if self.axes == 2:
            print >>file, "LATHE = 1"

        print >>file
        print >>file, "[TASK]"
        print >>file, "TASK = milltask"
        print >>file, "CYCLE_TIME = 0.010"

        print >>file
        print >>file, "[RS274NGC]"
        print >>file, "PARAMETER_FILE = emc.var"

        base_period = self.ideal_period()

        print >>file
        print >>file, "[EMCMOT]"
        print >>file, "EMCMOT = motmod"
        print >>file, "COMM_TIMEOUT = 1.0"
        print >>file, "COMM_WAIT = 0.010"
        print >>file, "BASE_PERIOD = %d" % base_period
        print >>file, "SERVO_PERIOD = 1000000"

        print >>file
        print >>file, "[HAL]"
        if self.halui:
            print >>file,"HALUI = halui"          
        print >>file, "HALFILE = %s.hal" % self.machinename
        if self.customhal:
            print >>file, "HALFILE = custom.hal"
            print >>file, "POSTGUI_HALFILE = custom_postgui.hal"

        if self.halui:
           print >>file
           print >>file, "[HALUI]"
           if self.pyvcphaltype == 2 and self.pyvcpconnect:
               print >>file,"MDI_COMMAND = G0 G53 Z0"
               print >>file,"MDI_COMMAND = G28"
               print >>file,"MDI_COMMAND = G92 X0"
               print >>file,"MDI_COMMAND = G92 Y0"
               print >>file,"MDI_COMMAND = G92 Z0"
               print >>file,"MDI_COMMAND = G92.1"

        print >>file
        print >>file, "[TRAJ]"
        if self.axes == 1:
            print >>file, "AXES = 4"
            print >>file, "COORDINATES = X Y Z A"
            print >>file, "MAX_ANGULAR_VELOCITY = %.2f" % self.amaxvel
            defvel = min(60, self.amaxvel/10.)
            print >>file, "DEFAULT_ANGULAR_VELOCITY = %.2f" % defvel
        elif self.axes == 0:
            print >>file, "AXES = 3"
            print >>file, "COORDINATES = X Y Z"
        else:
            print >>file, "AXES = 3"
            print >>file, "COORDINATES = X Z"
        if self.units:
            print >>file, "LINEAR_UNITS = mm"
        else:
            print >>file, "LINEAR_UNITS = inch"
        print >>file, "ANGULAR_UNITS = degree"
        print >>file, "CYCLE_TIME = 0.010"
        maxvel = max(self.xmaxvel, self.ymaxvel, self.zmaxvel)        
        hypotvel = (self.xmaxvel**2 + self.ymaxvel**2 + self.zmaxvel**2) **.5
        defvel = min(maxvel, max(.1, maxvel/10.))
        print >>file, "DEFAULT_VELOCITY = %.2f" % defvel
        print >>file, "MAX_LINEAR_VELOCITY = %.2f" % maxvel

        print >>file
        print >>file, "[EMCIO]"
        print >>file, "EMCIO = io"
        print >>file, "CYCLE_TIME = 0.100"
        print >>file, "TOOL_TABLE = tool.tbl"

        all_homes = self.home_sig("x") and self.home_sig("z")
        if self.axes != 2: all_homes = all_homes and self.home_sig("y")
        if self.axes == 4: all_homes = all_homes and self.home_sig("a")

        self.write_one_axis(file, 0, "x", "LINEAR", all_homes)
        if self.axes != 2:
            self.write_one_axis(file, 1, "y", "LINEAR", all_homes)
        self.write_one_axis(file, 2, "z", "LINEAR", all_homes)
        if self.axes == 1:
            self.write_one_axis(file, 3, "a", "ANGULAR", all_homes)

        file.close()
        self.add_md5sum(filename)

    def hz(self, axname):
        steprev = getattr(self, axname+"steprev")
        microstep = getattr(self, axname+"microstep")
        pulleynum = getattr(self, axname+"pulleynum")
        pulleyden = getattr(self, axname+"pulleyden")
        leadscrew = getattr(self, axname+"leadscrew")
        maxvel = getattr(self, axname+"maxvel")
        if self.units or axname == 'a': leadscrew = 1./leadscrew
        pps = leadscrew * steprev * microstep * (pulleynum/pulleyden) * maxvel
        return abs(pps)

    def doublestep(self, steptime=None):
        if steptime is None: steptime = self.steptime
        return steptime <= 5000

    def minperiod(self, steptime=None, stepspace=None, latency=None):
        if steptime is None: steptime = self.steptime
        if stepspace is None: stepspace = self.stepspace
        if latency is None: latency = self.latency
        if self.doublestep(steptime):
            return max(latency + steptime + stepspace + 5000, 4*steptime)
        else:
            return latency + max(steptime, stepspace)

    def maxhz(self):
        return 1e9 / self.minperiod()

    def ideal_period(self):
        xhz = self.hz('x')
        yhz = self.hz('y')
        zhz = self.hz('z')
        ahz = self.hz('a')
        if self.axes == 1:
            pps = max(xhz, yhz, zhz, ahz)
        elif self.axes == 0:
            pps = max(xhz, yhz, zhz)
        else:
            pps = max(xhz, zhz)
        base_period = 1e9 / pps
        if base_period > 100000: base_period = 100000
        if base_period < self.minperiod(): base_period = self.minperiod()
        return int(base_period)

    def write_one_axis(self, file, num, letter, type, all_homes):
        order = "1203"
        def get(s): return self[letter + s]
        print >>file
        print >>file, "[AXIS_%d]" % num
        print >>file, "TYPE = %s" % type
        print >>file, "HOME = %s" % get("homepos")
        print >>file, "MAX_VELOCITY = %s" % get("maxvel")
        print >>file, "MAX_ACCELERATION = %s" % get("maxacc")
        if self[letter + "usecomp"]:
            print >>file, "COMP_FILE = %s" % get("compfilename")
            print >>file, "COMP_FILE_TYPE = %s" % get("comptype")
        if self[letter + "usebacklash"]:
            print >>file, "BACKLASH = %s" % get("backlash")
        #print >>file, "STEPGEN_MAXACCEL = %s" % (1.25 * get("maxacc"))
        print >>file, "FERROR = %s"% get("maxferror")
        print >>file, "MIN_FERROR = %s" % get("minferror")
        print >>file, "P = %s" % get("P")
        print >>file, "I = %s" % get("I") 
        print >>file, "D = %s" % get("D")
        print >>file, "FF0 = %s" % get("FF0")
        print >>file, "FF1 = %s" % get("FF1")
        print >>file, "FF2 = %s" % get("FF2")
        print >>file, "OUTPUT_SCALE = %s" % get("outputscale")
        print >>file, "OUTPUT_OFFSET = %s" % get("outputoffset")
        print >>file, "MAX_OUTPUT = %s" % get("maxoutput")
        print >>file, "INPUT_SCALE = %s" % get("scale")
        # emc2 doesn't like having home right on an end of travel,
        # so extend the travel limit by up to .01in or .1mm
        minlim = get("minlim")
        maxlim = get("maxlim")
        home = get("homepos")
        if self.units: extend = .001
        else: extend = .01
        minlim = min(minlim, home - extend)
        maxlim = max(maxlim, home + extend)
        print >>file, "MIN_LIMIT = %s" % minlim
        print >>file, "MAX_LIMIT = %s" % maxlim

        thisaxishome = set(("all-home", "home-" + letter, "min-home-" + letter, "max-home-" + letter, "both-home-" + letter))
        ignore = set(("min-home-" + letter, "max-home-" + letter, "both-home-" + letter))
        homes = False
        for i in thisaxishome:
            test = self.findsignal(i)
            if not test == "false": homes = True
           
        print "debug",homes
        if homes:
            print >>file, "HOME_OFFSET = %f" % get("homesw")
            print >>file, "HOME_SEARCH_VEL = %f" % get("homevel")
            latchvel = abs(get("homelatchvel"))
            if get("latchdir"): latchvel = -latchvel            
            print >>file, "HOME_LATCH_VEL = %f" % latchvel
            print >>file, "HOME_FINAL_VEL = %f" % get("homefinalvel")
            if get("usehomeindex"):useindex = "YES"
            else: useindex = "NO"   
            print >>file, "HOME_USE_INDEX = %s" % useindex
            for i in ignore:
                test = self.findsignal(i)
                if not test == "false":
                    print >>file, "HOME_IGNORE_LIMITS = YES"
                    break
            if all_homes:
                print >>file, "HOME_SEQUENCE = %s" % order[num]
        else:
            print >>file, "HOME_OFFSET = %s" % get("homepos")

    def home_sig(self, axis):
        thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis, "max-home-" + axis, "both-home-" + axis))
        for i in thisaxishome:
            test = self.findsignal(i)
            if not test == "false": return i

    def min_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "min-" + axis,"min-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               test = self.findsignal(i)
               if not test == "false": return i

    def max_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "max-" + axis, "max-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               test = self.findsignal(i)
               if not test == "false": return i
 
    def connect_axis(self, file, num, let):
        axnum = "xyza".index(let)
        lat = self.latency
        print >>file
        print >>file, "setp pid.%d.maxoutput [AXIS_%d]MAX_VELOCITY"% (num, axnum)
        print >>file, "setp pid.%d.Pgain [AXIS_%d]P" % (num, axnum)
        print >>file, "setp pid.%d.Igain [AXIS_%d]I"% (num, axnum)
        print >>file, "setp pid.%d.Dgain [AXIS_%d]D" % (num, axnum)
        print >>file, "setp pid.%d.FF0 [AXIS_%d]FF0" % (num, axnum)
        print >>file, "setp pid.%d.FF1 [AXIS_%d]FF1" % (num, axnum)
        print >>file, "setp pid.%d.FF2 [AXIS_%d]FF2" % (num, axnum)
        print >>file, "setp pid.%d.deadband [AXIS_%d]DEADBAND" % (num, axnum)
        print >>file
        print >>file, "setp stepgen.%d.position-scale [AXIS_%d]SCALE" % (num, axnum)
        print >>file, "setp stepgen.%d.steplen 1" % num
        if self.doublestep():
            print >>file, "setp stepgen.%d.stepspace 0" % num
        else:
            print >>file, "setp stepgen.%d.stepspace 1" % num
        print >>file, "setp stepgen.%d.dirhold %d" % (num, self.dirhold + lat)
        print >>file, "setp stepgen.%d.dirsetup %d" % (num, self.dirsetup + lat)
        print >>file, "setp stepgen.%d.maxaccel [AXIS_%d]STEPGEN_MAXACCEL" % (num, axnum)
        print >>file, "net %spos-cmd axis.%d.motor-pos-cmd => stepgen.%d.position-cmd" % (let, axnum, num)
        print >>file, "net %spos-fb stepgen.%d.position-fb => axis.%d.motor-pos-fb" % (let, num, axnum)
        print >>file, "net %sstep <= stepgen.%d.step" % (let, num)
        print >>file, "net %sdir <= stepgen.%d.dir" % (let, num)
        print >>file, "net %senable axis.%d.amp-enable-out => stepgen.%d.enable" % (let, axnum, num)
        homesig = self.home_sig(let)
        if homesig:
            print >>file, "net %s => axis.%d.home-sw-in" % (homesig, axnum)
        min_limsig = self.min_lim_sig(let)
        if min_limsig:
            print >>file, "net %s => axis.%d.neg-lim-sw-in" % (min_limsig, axnum)
        max_limsig = self.max_lim_sig(let)
        if max_limsig:
            print >>file, "net %s => axis.%d.pos-lim-sw-in" % (max_limsig, axnum)

    def connect_input(self, file):
        for q in (1,2,3,4,5,6,7,8,10,11,12,13,15):
            p = self['firstpppin%din' % q]
            i = self['firstpppin%dinvin' % q]
            if p == UNUSED_INPUT: continue

            if i: print >>file, "net %s <= parport.0.pin-%02d-in-not" % (p, q)
            else: print >>file, "net %s <= parport.0.pin-%02d-in" % (p, q)
        print >>file
        for q in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15):
            p = self['m5i20c3pin%d' % q]
            i = self['m5i20c3pin%dinv' % q]
            if p == "unused-input": continue
            if i: print >>file, "net %s <= m5i20.0.in-%02d-not"  % (p, q)
            else: print >>file, "net %s <= m5i20.0.in-%02d" % (p, q)
        print >>file
        for q in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15):
            p = self['m5i20c4pin%d' % q]
            i = self['m5i20c3pin%dinv' % q]
            if p == "unused-input": continue
            if i: print >>file, "net %s <= m5i20.0.in-%02d-not"  % (p, (q+16))
            else: print >>file, "net %s <= m5i20.0.in-%02d" % (p, (q+16))

    def find_input(self, input):
        inputs = set((10, 11, 12, 13, 15))
        for i in inputs:
            pin = getattr(self, "firstpppin%din" % i)
            inv = getattr(self, "firstpppin%dinvin" % i)
            if pin == input: return i
        return None

    def find_output(self, output):
        outputs = set((1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 16, 17))
        for i in outputs:
            pin = getattr(self, "firstpppin%dout" % i)
            inv = getattr(self, "firstpppin%dinvout" % i)
            if pin == output: return i
        return None

    def connect_output(self, file):
        for q in (1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 16, 17):
            p = self['firstpppin%dout' % q]
            i = self['firstpppin%dinvout' % q]
            if p == UNUSED_OUTPUT: continue
            if i: print >>file, "setp parport.0.pin-%02d-out-invert true" % q
            print >>file, "net %s => parport.0.pin-%02d-out" % (p, q)
        print >>file
        for q in (16,17,18,19,20,21,22,23):
            p = self['m5i20c3pin%d' % q]
            i = self['m5i20c3pin%dinv' % q]
            print p
            if p == "unused-output": continue
            if i: print >>file, "setp m5i20.0.out-%02d-invert true" % (q-16)
            print >>file, "net %s => m5i20.0.out-%02d" % (p, q-16)
        print >>file
        for q in (16,17,18,19,20,21,22,23):
            p = self['m5i20c4pin%d' % q]
            i = self['m5i20c3pin%dinv' % q]
            if p == "unused-output": continue
            if i: print >>file, "setp m5i20.0.out-%02d-invert true" % (q-8)
            print >>file, "net %s => m5i20.0.out-%02d" % (p, q-8)


    def write_halfile(self, base):
        inputs = set((self.firstpppin10in,self.firstpppin11in,self.firstpppin12in,self.firstpppin13in,self.firstpppin15in))
        outputs = set((self.firstpppin1out, self.firstpppin2out, self.firstpppin3out, self.firstpppin4out, self.firstpppin5out,
            self.firstpppin6out, self.firstpppin7out, self.firstpppin8out, self.firstpppin9out, self.firstpppin14out, self.firstpppin16out,
            self.firstpppin17out))

        filename = os.path.join(base, self.machinename + ".hal")
        file = open(filename, "w")
        print >>file, _("# Generated by PNCconf at %s") % time.asctime()
        print >>file, _("# If you make changes to this file, they will be")
        print >>file, _("# overwritten when you run PNCconf again")

        print >>file, "loadrt trivkins"
        print >>file, "loadrt [EMCMOT]EMCMOT base_period_nsec=[EMCMOT]BASE_PERIOD servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[TRAJ]AXES"
        if self.mesa5i20>0:
            print >>file, "loadrt hal_m5i20"
        if self.number_pports>0:
            print >>file, "loadrt probe_parport"
            port3name = port2name = port1name = port3dir = port2dir = port1dir = ""
            if self.number_pports>2:
                 port3name = " " + self.ioaddr3
                 if self.Thirdpp_direction:
                    port3dir =" out"
                 else: 
                    port3dir =" in"
            if self.number_pports>1:
                 port2name = " " + self.ioaddr2
                 if self.Secondpp_direction:
                    port2dir =" out"
                 else: 
                    port2dir =" in"
            port1name = self.ioaddr
            if self.firstpp_direction:
               port1dir =" out"
            else: 
               port1dir =" in"
            print >>file, "loadrt hal_parport cfg=\"%s%s%s%s%s%s\"" % (port1name, port1dir, port2name, port2dir, port3name, port3dir)
            if self.doublestep():
                print >>file, "setp parport.0.reset-time %d" % self.steptime

        encodertest = self.findsignal("spindle-phase-a")
        print "pinnumber= ", encodertest
        if not encodertest == "false":
            encoder = True
            print "encoder ", encoder
        else:
            encoder = False

        counter = PHB not in inputs
        probe = PROBE in inputs
        pwm = "spindle-pwm" in outputs
        pump = PUMP in outputs

        if self.axes == 2:
            #print >>file, "loadrt stepgen step_type=0,0"
            print >>file, "loadrt pid num_chan=2"
        elif self.axes == 1:
            #print >>file, "loadrt stepgen step_type=0,0,0,0"
            print >>file, "loadrt pid num_chan=4"
        else:
            #print >>file, "loadrt stepgen step_type=0,0,0"
            print >>file, "loadrt pid num_chan=3"

        if encoder:
            print >>file, "loadrt encoder num_chan=1"
        if self.pyvcphaltype == 1 and self.pyvcpconnect == 1:
            print >>file, "loadrt abs count=1"
            if encoder:
               print >>file, "loadrt scale count=1"

        if pump:
            print >>file, "loadrt charge_pump"
            print >>file, "net estop-out charge-pump.enable iocontrol.0.user-enable-out"
            print >>file, "net charge-pump <= charge-pump.out"

        if pwm:
            print >>file, "loadrt pwmgen output_type=0"

        if self.classicladder:
            print >>file, "loadrt classicladder_rt numPhysInputs=%d numPhysOutputs=%d numS32in=%d numS32out=%d numFloatIn=%d numFloatOut=%d" %(self.digitsin , self.digitsout , self.s32in, self.s32out, self.floatsin, self.floatsout)

        print >>file
        if self.number_pports > 0:
            print >>file, "addf parport.0.read base-thread"
        if self.number_pports > 1:
            print >>file, "addf parport.1.read base-thread"
        if self.number_pports > 2:
            print >>file, "addf parport.2.read base-thread"

        #print >>file, "addf stepgen.make-pulses base-thread"
        if encoder: print >>file, "addf encoder.update-counters base-thread"
        if pump: print >>file, "addf charge-pump base-thread"
        if pwm: print >>file, "addf pwmgen.make-pulses base-thread"
        if self.number_pports > 0:
            print >>file, "addf parport.0.write base-thread"
            if self.doublestep():
                print >>file, "addf parport.0.reset base-thread"
        if self.number_pports > 1:
            print >>file, "addf parport.1.write base-thread"
        if self.number_pports > 2:
            print >>file, "addf parport.2.write base-thread"
        if self.mesa5i20>0:
            print >>file, "addf m5i20.0.encoder-read servo-thread" 
            print >>file, "addf m5i20.0.digital-in-read servo-thread"
        #print >>file, "addf stepgen.capture-position servo-thread"
        if encoder: print >>file, "addf encoder.capture-position servo-thread"
        print >>file, "addf motion-command-handler servo-thread"
        print >>file, "addf motion-controller servo-thread"
        if self.axes == 2:
            print >>file, "addf pid.0.do-pid-calcs servo-thread"
            print >>file, "addf pid.1.do-pid-calcs servo-thread"
        elif self.axes == 1:
            print >>file, "addf pid.0.do-pid-calcs servo-thread"
            print >>file, "addf pid.1.do-pid-calcs servo-thread"
            print >>file, "addf pid.2.do-pid-calcs servo-thread"
            print >>file, "addf pid.3.do-pid-calcs servo-thread"
        else:
            print >>file, "addf pid.0.do-pid-calcs servo-thread"
            print >>file, "addf pid.1.do-pid-calcs servo-thread"
            print >>file, "addf pid.2.do-pid-calcs servo-thread"
        if self.classicladder:
            print >>file,"addf classicladder.0.refresh servo-thread"
        #print >>file, "addf stepgen.update-freq servo-thread"
        if pwm: print >>file, "addf pwmgen.update servo-thread"
        if self.pyvcphaltype == 1 and self.pyvcpconnect == 1:
            print >>file, "addf abs.0 servo-thread"
            if encoder:
               print >>file, "addf scale.0 servo-thread"
        if self.mesa5i20>0:
            print >>file, "addf m5i20.0.dac-write servo-thread" 
            print >>file, "addf m5i20.0.digital-out-write servo-thread"
        print >>file
        if pwm:
            x1 = self.spindlepwm1
            x2 = self.spindlepwm2
            y1 = self.spindlespeed1
            y2 = self.spindlespeed2
            scale = (y2-y1) / (x2-x1)
            offset = x1 - y1 / scale
            print >>file
            print >>file, "net spindle-cmd <= motion.spindle-speed-out => pwmgen.0.value"
            print >>file, "net spindle-enable <= motion.spindle-on => pwmgen.0.enable"
            print >>file, "net spindle-pwm <= pwmgen.0.pwm"
            print >>file, "setp pwmgen.0.pwm-freq %s" % self.spindlecarrier        
            print >>file, "setp pwmgen.0.scale %s" % scale
            print >>file, "setp pwmgen.0.offset %s" % offset
            print >>file, "setp pwmgen.0.dither-pwm true"
        else: 
            print >>file, "net spindle-cmd <= motion.spindle-speed-out"

        if ON in outputs:
            print >>file, "net spindle-on <= motion.spindle-on"
        if CW in outputs:
            print >>file, "net spindle-cw <= motion.spindle-forward"
        if CCW in outputs:
            print >>file, "net spindle-ccw <= motion.spindle-reverse"
        if BRAKE in outputs:
            print >>file, "net spindle-brake <= motion.spindle-brake"

        if MIST in outputs:
            print >>file, "net coolant-mist <= iocontrol.0.coolant-mist"

        if FLOOD in outputs:
            print >>file, "net coolant-flood <= iocontrol.0.coolant-flood"

        if encoder:
            print >>file
            if PHB not in inputs:
                print >>file, "setp encoder.0.position-scale %f"\
                     % self.spindlecpr
                print >>file, "setp encoder.0.counter-mode 1"
            else:
                print >>file, "setp encoder.0.position-scale %f" \
                    % ( 4.0 * int(self.spindlecpr))
            print >>file, "net spindle-position encoder.0.position => motion.spindle-revs"
            print >>file, "net spindle-velocity encoder.0.velocity => motion.spindle-speed-in"
            print >>file, "net spindle-index-enable encoder.0.index-enable <=> motion.spindle-index-enable"
            print >>file, "net spindle-phase-a encoder.0.phase-A"
            print >>file, "net spindle-phase-b encoder.0.phase-B"
            print >>file, "net spindle-index encoder.0.phase-Z"


        if probe:
            print >>file
            print >>file, "net probe-in => motion.probe-input"

        for i in range(4):
            dout = "dout-%02d" % i
            print dout, dout in outputs
            if dout in outputs:
                print >>file, "net %s <= motion.digital-out-%02d" % (dout, i)

        for i in range(4):
            din = "din-%02d" % i
            print din, din in inputs
            if din in inputs:
                print >>file, "net %s <= motion.digital-in-%02d" % (din, i)

        print >>file
        self.connect_output(file)              
        print >>file
        self.connect_input(file)
        print >>file

        if self.axes == 2:
            self.connect_axis(file, 0, 'x')
            self.connect_axis(file, 1, 'z')
        elif self.axes == 0:
            self.connect_axis(file, 0, 'x')
            self.connect_axis(file, 1, 'y')
            self.connect_axis(file, 2, 'z')
        elif self.axes == 1:
            self.connect_axis(file, 0, 'x')
            self.connect_axis(file, 1, 'y')
            self.connect_axis(file, 2, 'z')
            self.connect_axis(file, 3, 'a')

        print >>file
        print >>file, "net estop-out <= iocontrol.0.user-enable-out"
        if  self.classicladder and self.ladderhaltype == 1 and self.ladderconnect: # external estop program
            print >>file 
            print >>file, "# **** Setup for external estop ladder program -START ****"
            print >>file
            print >>file, "net estop-out => classicladder.0.in-00"
            print >>file, "net estop-ext => classicladder.0.in-01"
            print >>file, "net estop-strobe classicladder.0.in-02 <= iocontrol.0.user-request-enable"
            print >>file, "net estop-outcl classicladder.0.out-00 => iocontrol.0.emc-enable-in"
            print >>file
            print >>file, "# **** Setup for external estop ladder program -END ****"
        elif ESTOP_IN in inputs:
            print >>file, "net estop-ext => iocontrol.0.emc-enable-in"
        else:
            print >>file, "net estop-out => iocontrol.0.emc-enable-in"

        print >>file
        if self.manualtoolchange:
            print >>file, "loadusr -W hal_manualtoolchange"
            print >>file, "net tool-change iocontrol.0.tool-change => hal_manualtoolchange.change"
            print >>file, "net tool-changed iocontrol.0.tool-changed <= hal_manualtoolchange.changed"
            print >>file, "net tool-number iocontrol.0.tool-prep-number => hal_manualtoolchange.number"

        else:
            print >>file, "net tool-number <= iocontrol.0.tool-prep-number"
            print >>file, "net tool-change-loopback iocontrol.0.tool-change => iocontrol.0.tool-changed"
        print >>file, "net tool-prepare-loopback iocontrol.0.tool-prepare => iocontrol.0.tool-prepared"
        if self.classicladder:
            print >>file
            if self.modbus:
                print >>file, "# Load Classicladder with modbus master included (GUI must run for Modbus)"
                print >>file, "loadusr classicladder --modmaster custom.clp"
            else:
                print >>file, "# Load Classicladder without GUI (can reload LADDER GUI in AXIS GUI"
                print >>file, "loadusr classicladder --nogui custom.clp"
        if self.pyvcp:
            vcp = os.path.join(base, "custompanel.xml")
            if not os.path.exists(vcp):
                f1 = open(vcp, "w")

                print >>f1, "<?xml version='1.0' encoding='UTF-8'?>"

                print >>f1, "<!-- "
                print >>f1, _("Include your PyVCP panel here.\n")
                print >>f1, "-->"
                print >>f1, "<pyvcp>"
                print >>f1, "</pyvcp>"
        if self.pyvcp or self.customhal:
            custom = os.path.join(base, "custom_postgui.hal")
            if os.path.exists(custom): 
                shutil.copy( custom,os.path.join(base,"postgui_backup.hal") ) 
            f1 = open(custom, "w")
            print >>f1, _("# Include your customized HAL commands here")
            print >>f1, _("""\
# The commands in this file are run after the AXIS GUI (including PyVCP panel) starts""") 
            print >>f1
            if self.pyvcphaltype == 1 and self.pyvcpconnect: # spindle speed/tool # display
                  print >>f1, ("# **** Setup of spindle speed and tool number display using pyvcp -START ****")
                  if encoder:
                      print >>f1, ("# **** Use ACTUAL spindle velocity from spindle encoder")
                      print >>f1, ("# **** spindle-velocity is signed so we use absolute compoent to remove sign") 
                      print >>f1, ("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
                      print >>f1
                      print >>f1, ("setp scale.0.gain .01667")
                      print >>f1, ("net spindle-velocity => abs.0.in")
                      print >>f1, ("net absolute-spindle-vel <= abs.0.out => scale.0.in")
                      print >>f1, ("net scaled-spindle-vel <= scale.0.out => pyvcp.spindle-speed")
                  else:
                      print >>f1, ("# **** Use COMMANDED spindle velocity from EMC because no spindle encoder was specified")
                      print >>f1, ("# **** COMANDED velocity is signed so we use absolute component (abs.0) to remove sign")
                      print >>f1
                      print >>f1, ("net spindle-cmd => abs.0.in")
                      print >>f1, ("net absolute-spindle-vel <= abs.0.out => pyvcp.spindle-speed")                     
                  print >>f1, ("net tool-number => pyvcp.toolnumber")
                  print >>f1
                  print >>f1, ("# **** Setup of spindle speed and tool number display using pyvcp -END ****")
            if self.pyvcphaltype == 2 and self.pyvcpconnect: # Hal_UI example
                      print >>f1, ("# **** Setup of pyvcp buttons and MDI commands using HAL_UI and pyvcp - START ****")
                      print >>f1
                      print >>f1, ("net jog-X+ <= pyvcp.jog-x+ => halui.jog.0.plus")
                      print >>f1, ("net jog-X- <= pyvcp.jog-x- => halui.jog.0.minus")
                      print >>f1, ("net jog-Y+ <= pyvcp.jog-y+ => halui.jog.1.plus")
                      print >>f1, ("net jog-Y- <= pyvcp.jog-y- => halui.jog.1.minus")
                      print >>f1, ("net jog-Z+ <= pyvcp.jog-z+ => halui.jog.2.plus")
                      print >>f1, ("net jog-Z- <= pyvcp.jog-z- => halui.jog.2.minus")
                      print >>f1, ("net jog-speed <= pyvcp.jog-speed => halui.jog-speed")
                      print >>f1, ("net optional-stp-on <= pyvcp.ostop-on => halui.program.optional-stop.on")
                      print >>f1, ("net optional-stp-off <= pyvcp.ostop-off => halui.program.optional-stop.off")
                      print >>f1, ("net optional-stp-is-on <= pyvcp.ostop-is-on => halui.program.optional-stop.is-on")
                      print >>f1, ("net program-pause <= pyvcp.pause => halui.program.pause")
                      print >>f1, ("net program-resume <= pyvcp.resume => halui.program.resume")
                      print >>f1, ("net program-single-step <= pyvcp.step => halui.program.step")
                      print >>f1
                      print >>f1, ("# **** The following mdi-comands are specified in the machine named INI file under [HALUI] heading")
                      print >>f1, ("# **** command 00 - rapid to Z 0 ( G0 Z0 )")
                      print >>f1, ("# **** command 01 - rapid to reference point ( G 28 )")
                      print >>f1, ("# **** command 02 - zero X axis in G54 cordinate system")
                      print >>f1, ("# **** command 03 - zero Y axis in G54 cordinate system")
                      print >>f1, ("# **** command 04 - zero Z axis in G54 cordinate system")
                      print >>f1
                      print >>f1, ("net MDI-Z-up <= pyvcp.MDI-z_up => halui.mdi-command-00")
                      print >>f1, ("net MDI-reference-pos <= pyvcp.MDI-reference => halui.mdi-command-01")
                      print >>f1, ("net MDI-zero_X <= pyvcp.MDI-zerox => halui.mdi-command-02")
                      print >>f1, ("net MDI-zero_Y <= pyvcp.MDI-zeroy => halui.mdi-command-03")
                      print >>f1, ("net MDI-zero_Z <= pyvcp.MDI-zeroz => halui.mdi-command-04")
                      print >>f1, ("net MDI-clear-offset <= pyvcp.MDI-clear-offset => halui.mdi-command-05")
                      print >>f1
                      print >>f1, ("# **** Setup of pyvcp buttons and MDI commands using HAL_UI and pyvcp - END ****")

        if self.customhal or self.classicladder or self.halui:
            custom = os.path.join(base, "custom.hal")
            if not os.path.exists(custom):
                f1 = open(custom, "w")
                print >>f1, _("# Include your customized HAL commands here")
                print >>f1, _("# This file will not be overwritten when you run PNCconf again") 
        file.close()
        self.add_md5sum(filename)

    def write_readme(self, base):
        filename = os.path.join(base, "README")
        file = open(filename, "w")
        print >>file, _("Generated by PNCconf at %s") % time.asctime()
        print >>file
        print >>file,("Mesa 5i20 connector 2 ")
        for x in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
            temp = self["m5i20c2pin%d" % x]
            print >>file,("pin# %(pinnum)d is connected to signal:'%(data)s'" %{ 'pinnum':x, 'data':temp}) 
        print >>file
        print >>file,("Mesa 5i20 connector 3 ")
        for x in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
            temp = self["m5i20c3pin%d" % x]
            print >>file,("pin# %(pinnum)d is connected to signal:'%(data)s'" %{ 'pinnum':x, 'data':temp}) 
        print >>file
        print >>file,("Mesa 5i20 connector 4 ")
        for x in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
            temp = self["m5i20c4pin%d" % x]
            print >>file,("pin# %(pinnum)d is connected to signal:'%(data)s'" %{ 'pinnum':x, 'data':temp}) 
        file.close()
        self.add_md5sum(filename)

    def copy(self, base, filename):
        dest = os.path.join(base, filename)
        if not os.path.exists(dest):
            shutil.copy(os.path.join(distdir, filename), dest)

    def save(self):
        base = os.path.expanduser("~/emc2/configs/%s" % self.machinename)
        ncfiles = os.path.expanduser("~/emc2/nc_files")
        if not os.path.exists(ncfiles):
            makedirs(ncfiles)
            examples = os.path.join(BASE, "share", "emc", "ncfiles")
            if not os.path.exists(examples):
                examples = os.path.join(BASE, "nc_files")
            if os.path.exists(examples):
                os.symlink(examples, os.path.join(ncfiles, "examples"))
        
        makedirs(base)

        self.md5sums = []
        self.write_readme(base)
        self.write_inifile(base)
        self.write_halfile(base)
        self.copy(base, "emc.nml")
        self.copy(base, "tool.tbl")
        self.copy(base, "emc.var")

        filename = "%s.pncconf" % base

        d = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, "stepconf", None)
        e = d.documentElement

        for k, v in sorted(self.__dict__.iteritems()):
            if k.startswith("_"): continue
            n = d.createElement('property')
            e.appendChild(n)

            if isinstance(v, float): n.setAttribute('type', 'float')
            elif isinstance(v, bool): n.setAttribute('type', 'bool')
            elif isinstance(v, int): n.setAttribute('type', 'int')
            elif isinstance(v, list): n.setAttribute('type', 'eval')
            else: n.setAttribute('type', 'string')

            n.setAttribute('name', k)
            n.setAttribute('value', str(v))
        
        d.writexml(open(filename, "wb"), addindent="  ", newl="\n")
        print("%s" % base)

        if self.createsymlink:
            if not os.path.exists(os.path.expanduser("~/Desktop/%s" % self.machinename)):
                os.symlink(base,os.path.expanduser("~/Desktop/%s" % self.machinename))

        if self.createshortcut:
            if os.path.exists(BASE + "/scripts/emc"):
                scriptspath = (BASE + "/scripts/emc")
            else:
                scriptspath ="emc"
            print"%s" % BASE
            print"%s" % scriptspath
            filename = os.path.expanduser("~/Desktop/%s.desktop" % self.machinename)
            file = open(filename, "w")
            print >>file,"[Desktop Entry]"
            print >>file,"Version=1.0"
            print >>file,"Terminal=false"
            print >>file,"Name=" + _("launch %s") % self.machinename
            print >>file,"Exec=%s %s/%s.ini" \
                         % ( scriptspath, base, self.machinename )
            print >>file,"Type=Application"
            print >>file,"Comment=" + _("Desktop Launcher for EMC config made by PNCconf")
            print >>file,"Icon=/etc/emc2/emc2icon.png"
            file.close()

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

    def findsignal(self, sig):
        ppinput=dict([(self["firstpppin%din" %s],"firstpppin%din" %s) for s in (1,2,3,4,5,6,7,8,10,11,12,13,15)])
        ppoutput=dict([(self["firstpppin%dout" %s],"firstpppin%dout" %s) for s in (1,2,3,4,5,6,7,8,9,14,16,17)])
        mesa2=dict([(self["m5i20c2pin%d" %s],"m5i20c2pin%d" %s) for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23)])
        mesa3=dict([(self["m5i20c3pin%d" %s],"m5i20c3pin%d" %s) for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23)])
        mesa4=dict([(self["m5i20c4pin%d" %s],"m5i20c4pin%d" %s) for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23)])
        try:
            return ppinput[sig]
        except :
            try:
                return ppoutput[sig]
            except :
                try:
                    return mesa2[sig]
                except :
                    try:
                        return mesa3[sig]
                    except :
                        try:
                            return mesa4[sig]
                        except :
                            return "false"

class App:
    fname = 'pncconf.glade'  # XXX search path

    def _getwidget(self, doc, id):
        for i in doc.getElementsByTagName('widget'):
            if i.getAttribute('id') == id: return i

    def make_axispage(self, doc, axisname):
        axispage = self._getwidget(doc, 'xaxis').parentNode.cloneNode(True)
        nextpage = self._getwidget(doc, 'spindle').parentNode
        widget = self._getwidget(axispage, "xaxis")
        for node in widget.childNodes:
            if (node.nodeType == xml.dom.Node.ELEMENT_NODE
                    and node.tagName == "property"
                    and node.getAttribute('name') == "title"):
                node.childNodes[0].data = _("%s Axis Configuration") % axisname.upper()
        for node in axispage.getElementsByTagName("widget"):
            id = node.getAttribute('id')
            if id.startswith("x"):
                node.setAttribute('id', axisname + id[1:])
            else:
                node.setAttribute('id', axisname + id)
        for node in axispage.getElementsByTagName("signal"):
            handler = node.getAttribute('handler')
            node.setAttribute('handler', handler.replace("on_x", "on_" + axisname))
        for node in axispage.getElementsByTagName("property"):
            name = node.getAttribute('name')
            if name == "mnemonic_widget":
                node.childNodes[0].data = axisname + node.childNodes[0].data[1:]
        nextpage.parentNode.insertBefore(axispage, nextpage)

    def make_axismotorpage(self, doc, axisname):
        axispage = self._getwidget(doc, 'xaxismotor').parentNode.cloneNode(True)
        nextpage = self._getwidget(doc, 'xaxis').parentNode
        widget = self._getwidget(axispage, "xaxismotor")
        for node in widget.childNodes:
            if (node.nodeType == xml.dom.Node.ELEMENT_NODE
                    and node.tagName == "property"
                    and node.getAttribute('name') == "title"):
                node.childNodes[0].data = _("%s Axis Motor/Encoder Configuration") % axisname.upper()
        for node in axispage.getElementsByTagName("widget"):
            id = node.getAttribute('id')
            if id.startswith("x"):
                node.setAttribute('id', axisname + id[1:])
            else:
                node.setAttribute('id', axisname + id)
        for node in axispage.getElementsByTagName("signal"):
            handler = node.getAttribute('handler')
            node.setAttribute('handler', handler.replace("on_x", "on_" + axisname))
        for node in axispage.getElementsByTagName("property"):
            name = node.getAttribute('name')
            if name == "mnemonic_widget":
                node.childNodes[0].data = axisname + node.childNodes[0].data[1:]
        nextpage.parentNode.insertBefore(axispage, nextpage)

    def make_pportpage(self, doc, axisname):
        axispage = self._getwidget(doc, 'firstpport').parentNode.cloneNode(True)
        nextpage = self._getwidget(doc, 'xaxismotor').parentNode
        widget = self._getwidget(axispage, "firstpport")
        for node in widget.childNodes:
            if (node.nodeType == xml.dom.Node.ELEMENT_NODE
                    and node.tagName == "property"
                    and node.getAttribute('name') == "title"):
                node.childNodes[0].data = _("%s Parallel Port Setup") % axisname
        for node in axispage.getElementsByTagName("widget"):
            id = node.getAttribute('id')
            if id.startswith("first"):
                node.setAttribute('id', axisname + id[5:])
            else:
                node.setAttribute('id', axisname + id)
        for node in axispage.getElementsByTagName("signal"):
            handler = node.getAttribute('handler')
            node.setAttribute('handler', handler.replace("on_first", "on_" + axisname))
        for node in axispage.getElementsByTagName("property"):
            name = node.getAttribute('name')
            if name == "mnemonic_widget":
                node.childNodes[0].data = axisname + node.childNodes[0].data[1:]
        nextpage.parentNode.insertBefore(axispage, nextpage)

    def __init__(self):
        gnome.init("pncconf", "0.6") 
        glade = xml.dom.minidom.parse(os.path.join(datadir, self.fname))
        self.make_axispage(glade, 'y')
        self.make_axispage(glade, 'z')
        self.make_axispage(glade, 'a')
        self.make_axismotorpage(glade, 'y')
        self.make_axismotorpage(glade, 'z')
        self.make_axismotorpage(glade, 'a')
        self.make_pportpage(glade, 'Second')
        self.make_pportpage(glade, 'Third')
        doc = glade.toxml().encode("utf-8")

        self.xml = gtk.glade.xml_new_from_buffer(doc, len(doc), domain="axis")
        self.widgets = Widgets(self.xml)

        self.watermark = gtk.gdk.pixbuf_new_from_file(wizard)
        self.widgets.dialog1.hide()
        self.widgets.druidpagestart1.set_watermark(self.watermark)
        self.widgets.complete.set_watermark(self.watermark)
        self.widgets.druidpagestart1.show()
        self.widgets.complete.show()
        
        self.xml.signal_autoconnect(self)

        self.in_pport_prepare = False
        self.axis_under_test = False
        self.jogminus = self.jogplus = 0

        
        self.data = Data()
   
        tempfile = os.path.join(distdir, "configurable_options/ladder/TEMP.clp")
        if os.path.exists(tempfile):
           os.remove(tempfile)

    def gtk_main_quit(self, *args):
        gtk.main_quit()

    def on_window1_delete_event(self, *args):
        if self.warning_dialog (_("Quit PNCconfig and discard changes?"),False):
            gtk.main_quit()
            return False
        else:
            return True
    on_druid1_cancel = on_window1_delete_event
    
    def warning_dialog(self,message,is_ok_type):
        if is_ok_type:
           dialog = gtk.MessageDialog(app.widgets.window1,
                gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_WARNING, gtk.BUTTONS_OK,message)
           dialog.show_all()
           result = dialog.run()
           dialog.destroy()
           return True
        else:   
            dialog = gtk.MessageDialog(self.widgets.window1,
               gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
               gtk.MESSAGE_QUESTION, gtk.BUTTONS_YES_NO,message)
            dialog.show_all()
            result = dialog.run()
            dialog.destroy()
            if result == gtk.RESPONSE_YES:
                return True
            else:
                return False

    def on_page_newormodify_prepare(self, *args):
        self.widgets.createsymlink.set_active(self.data.createsymlink)
        self.widgets.createshortcut.set_active(self.data.createshortcut)

    def on_page_newormodify_next(self, *args):
        if not self.widgets.createconfig.get_active():
            filter = gtk.FileFilter()
            filter.add_pattern("*.pncconf")
            filter.set_name(_("EMC2 'PNCconf' configuration files"))
            dialog = gtk.FileChooserDialog(_("Modify Existing Configuration"),
                self.widgets.window1, gtk.FILE_CHOOSER_ACTION_OPEN,
                (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                 gtk.STOCK_OPEN, gtk.RESPONSE_OK))
            dialog.set_default_response(gtk.RESPONSE_OK)
            dialog.add_filter(filter) 
            dialog.add_shortcut_folder(os.path.expanduser("~/emc2/configs"))
            dialog.set_current_folder(os.path.expanduser("~/emc2/configs"))
            dialog.show_all()
            result = dialog.run()
            if result == gtk.RESPONSE_OK:
                filename = dialog.get_filename()
                dialog.destroy()
                self.data.load(filename, self)
            else:
                dialog.destroy()
                return True
        self.data.createsymlink = self.widgets.createsymlink.get_active()
        self.data.createshortcut = self.widgets.createshortcut.get_active()

    def on_basicinfo_prepare(self, *args):
        self.widgets.machinename.set_text(self.data.machinename)
        self.widgets.axes.set_active(self.data.axes)
        self.widgets.units.set_active(self.data.units)
        self.widgets.latency.set_value(self.data.latency)
        self.widgets.machinename.grab_focus()
        self.widgets.mesa5i20_checkbutton.set_active(self.data.mesa5i20)
        self.widgets.ioaddr.set_text(self.data.ioaddr)
        self.widgets.ioaddr2.set_text(self.data.ioaddr2) 
        self.widgets.ioaddr3.set_text(self.data.ioaddr3)
        self.widgets.firstpp_direction.set_active(self.data.firstpp_direction)
        self.widgets.Secondpp_direction.set_active(self.data.Secondpp_direction)
        self.widgets.Thirdpp_direction.set_active(self.data.Thirdpp_direction)
        if self.data.number_pports>0:
             self.widgets.pp1_checkbutton.set_active(1)
        else :
             self.widgets.pp1_checkbutton.set_active(0)
        if self.data.number_pports>1:
             self.widgets.pp2_checkbutton.set_active(1)
        if self.data.number_pports>2:
             self.widgets.pp3_checkbutton.set_active(1)
        if self.data.limitsnone :
             self.widgets.limittype_none.set_active(1)
        if self.data.limitswitch :
             self.widgets.limittype_switch.set_active(1)
        if self.data.limitshared :
             self.widgets.limittype_shared.set_active(1)
        if self.data.homenone :
             self.widgets.home_none.set_active(1)
        if self.data.homeindex :
             self.widgets.home_index.set_active(1)
        if self.data.homeswitch :
             self.widgets.home_switch.set_active(1)
        if self.data.homeboth :
             self.widgets.home_both.set_active(1)
    
    def on_mesa5i20_checkbutton_toggled(self, *args): 
        i = self.widgets.mesa5i20_checkbutton.get_active()   
        self.widgets.nbr5i20.set_sensitive(i)
        
    def on_pp1_checkbutton_toggled(self, *args): 
        i = self.widgets.pp1_checkbutton.get_active()   
        self.widgets.firstpp_direction.set_sensitive(i)
        self.widgets.ioaddr.set_sensitive(i)
        if i == 0:
           self.widgets.pp2_checkbutton.set_active(i)
           self.widgets.ioaddr2.set_sensitive(i)
           self.widgets.pp3_checkbutton.set_active(i)
           self.widgets.ioaddr3.set_sensitive(i)

    def on_pp2_checkbutton_toggled(self, *args): 
        i = self.widgets.pp2_checkbutton.get_active()  
        if self.widgets.pp1_checkbutton.get_active() == 0:
          i = 0  
          self.widgets.pp2_checkbutton.set_active(0)
        self.widgets.Secondpp_direction.set_sensitive(i)
        self.widgets.ioaddr2.set_sensitive(i)
        if i == 0:
           self.widgets.pp3_checkbutton.set_active(i)
           self.widgets.ioaddr3.set_sensitive(i)

    def on_pp3_checkbutton_toggled(self, *args): 
        i = self.widgets.pp3_checkbutton.get_active() 
        if self.widgets.pp2_checkbutton.get_active() == 0:
          i = 0  
          self.widgets.pp3_checkbutton.set_active(0)
        self.widgets.Thirdpp_direction.set_sensitive(i)
        self.widgets.ioaddr3.set_sensitive(i)      

    def on_basicinfo_next(self, *args):
        self.data.machinename = self.widgets.machinename.get_text()
        self.data.axes = self.widgets.axes.get_active()
        self.data.units = self.widgets.units.get_active()
        self.data.latency = self.widgets.latency.get_value()
        self.data.manualtoolchange = self.widgets.manualtoolchange.get_active()
        self.data.ioaddr = self.widgets.ioaddr.get_text()
        self.data.ioaddr2 = self.widgets.ioaddr2.get_text()
        self.data.ioaddr3 = self.widgets.ioaddr3.get_text()
        self.data.mesa5i20 = self.widgets.mesa5i20_checkbutton.get_active()
        if self.widgets.pp3_checkbutton.get_active() and self.widgets.pp2_checkbutton.get_active():
            self.data.number_pports = 3
        elif self.widgets.pp2_checkbutton.get_active() and self.widgets.pp1_checkbutton.get_active():
            self.data.number_pports = 2
        elif self.widgets.pp1_checkbutton.get_active():
            self.data.number_pports = 1
        else :
            self.data.number_pports = 0
        if self.data.number_pports == 0 and self.data.mesa5i20 == 0 :
           self.warning_dialog(_("You need to designate a parport and/or mesa 5i20 I/O device before continuing."),True)
           self.widgets.druid1.set_page(self.widgets.basicinfo)
           return True 
        self.data.firstpp_direction = self.widgets.firstpp_direction.get_active()
        self.data.Secondpp_direction = self.widgets.Secondpp_direction.get_active()
        self.data.Thirdpp_direction = self.widgets.Thirdpp_direction.get_active()
        self.data.limitshared = self.widgets.limittype_shared.get_active()
        self.data.limitsnone = self.widgets.limittype_none.get_active()
        self.data.limitswitch = self.widgets.limittype_switch.get_active()
        self.data.limitshared = self.widgets.limittype_shared.get_active()
        self.data.homenone = self.widgets.home_none.get_active()

    def on_machinename_changed(self, *args):
        self.widgets.confdir.set_text(
            "~/emc2/configs/%s" % self.widgets.machinename.get_text())

    def on_GUI_config_prepare(self, *args):
        self.widgets.manualtoolchange.set_active(self.data.manualtoolchange)
        if self.data.frontend == 1: i= 1
        else: i = 0
        self.widgets.GUIAXIS.set_active(i)
        self.widgets.GUITkEmc.set_active(not i)

    def on_GUI_config_next(self, *args):
        if self.widgets.GUIAXIS.get_active():
           self.data.frontend = 1
        else : 
           self.data.frontend = 2
        if not self.data.mesa5i20:
           self.widgets.druid1.set_page(self.widgets.firstpport)
           return True

    def do_exclusive_inputs(self, pin):
        if self.in_pport_prepare: return
        exclusive = {
            HOME_X: (MAX_HOME_X, MIN_HOME_X, BOTH_HOME_X, ALL_HOME),
            HOME_Y: (MAX_HOME_Y, MIN_HOME_Y, BOTH_HOME_Y, ALL_HOME),
            HOME_Z: (MAX_HOME_Z, MIN_HOME_Z, BOTH_HOME_Z, ALL_HOME),
            HOME_A: (MAX_HOME_A, MIN_HOME_A, BOTH_HOME_A, ALL_HOME),

            MAX_HOME_X: (HOME_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X, ALL_LIMIT, ALL_HOME),
            MAX_HOME_Y: (HOME_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y, ALL_LIMIT, ALL_HOME),
            MAX_HOME_Z: (HOME_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z, ALL_LIMIT, ALL_HOME),
            MAX_HOME_A: (HOME_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A, ALL_LIMIT, ALL_HOME),

            MIN_HOME_X: (HOME_X, MAX_HOME_X, BOTH_HOME_X, ALL_LIMIT, ALL_HOME),
            MIN_HOME_Y: (HOME_Y, MAX_HOME_Y, BOTH_HOME_Y, ALL_LIMIT, ALL_HOME),
            MIN_HOME_Z: (HOME_Z, MAX_HOME_Z, BOTH_HOME_Z, ALL_LIMIT, ALL_HOME),
            MIN_HOME_A: (HOME_A, MAX_HOME_A, BOTH_HOME_A, ALL_LIMIT, ALL_HOME),

            BOTH_HOME_X: (HOME_X, MAX_HOME_X, MIN_HOME_X, ALL_LIMIT, ALL_HOME),
            BOTH_HOME_Y: (HOME_Y, MAX_HOME_Y, MIN_HOME_Y, ALL_LIMIT, ALL_HOME),
            BOTH_HOME_Z: (HOME_Z, MAX_HOME_Z, MIN_HOME_Z, ALL_LIMIT, ALL_HOME),
            BOTH_HOME_A: (HOME_A, MAX_HOME_A, MIN_HOME_A, ALL_LIMIT, ALL_HOME),

            MIN_X: (BOTH_X, BOTH_HOME_X, MIN_HOME_X, ALL_LIMIT),
            MIN_Y: (BOTH_Y, BOTH_HOME_Y, MIN_HOME_Y, ALL_LIMIT),
            MIN_Z: (BOTH_Z, BOTH_HOME_Z, MIN_HOME_Z, ALL_LIMIT),
            MIN_A: (BOTH_A, BOTH_HOME_A, MIN_HOME_A, ALL_LIMIT),

            MAX_X: (BOTH_X, BOTH_HOME_X, MIN_HOME_X, ALL_LIMIT),
            MAX_Y: (BOTH_Y, BOTH_HOME_Y, MIN_HOME_Y, ALL_LIMIT),
            MAX_Z: (BOTH_Z, BOTH_HOME_Z, MIN_HOME_Z, ALL_LIMIT),
            MAX_A: (BOTH_A, BOTH_HOME_A, MIN_HOME_A, ALL_LIMIT),

            BOTH_X: (MIN_X, MAX_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X, ALL_LIMIT),
            BOTH_Y: (MIN_Y, MAX_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y, ALL_LIMIT),
            BOTH_Z: (MIN_Z, MAX_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z, ALL_LIMIT),
            BOTH_A: (MIN_A, MAX_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A, ALL_LIMIT),

            ALL_LIMIT: (
                MIN_X, MAX_X, BOTH_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X,
                MIN_Y, MAX_Y, BOTH_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y,
                MIN_Z, MAX_Z, BOTH_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z,
                MIN_A, MAX_A, BOTH_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A),
            ALL_HOME: (
                HOME_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X,
                HOME_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y,
                HOME_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z,
                HOME_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A),
        } 

        p = 'firstpppin%din' % pin
        v = self.widgets[p].get_active()
        ex = exclusive.get(hal_input_names[v], ())

        for pin1 in (10,11,12,13,15):
            if pin1 == pin: continue
            p = 'firstpppin%din' % pin1
            v1 = hal_input_names[self.widgets[p].get_active()]
            if v1 in ex or v1 == v:
                self.widgets[p].set_active(hal_input_names.index(UNUSED_INPUT))

    def on_pin10_changed(self, *args):
        self.do_exclusive_inputs(10)
    def on_pin11_changed(self, *args):
        self.do_exclusive_inputs(11)
    def on_pin12_changed(self, *args):
        self.do_exclusive_inputs(12)
    def on_pin13_changed(self, *args):
        self.do_exclusive_inputs(13)
    def on_pin15_changed(self, *args):
        self.do_exclusive_inputs(15)

    def on_mesa5i20_prepare(self, *args):
        self.in_mesa_prepare = True
        for connector in (2,3,4):
            for pin in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                pinv = 'm5i20c%(con)dpin%(num)dinv' % {'con':connector ,'num': pin}
                ptype = 'm5i20c%(con)dpin%(num)dtype' % {'con':connector ,'num': pin}
                self.widgets[pinv].set_active(self.data[pinv])
                model = self.widgets[p].get_model()
                model.clear()
                print p
                if self.data[ptype] == 0:                    
                    for name in human_input_names:
                        print name ,self.data[p]
                        if self.data.limitshared or self.data.limitsnone:
                            if name in human_names_limit_only: continue 
                        if self.data.limitswitch or self.data.limitsnone:
                            if name in human_names_shared_home: continue                          
                        if self.data.homenone or self.data.limitshared:
                            if name in (_("Home X"), _("Home Y"), _("Home Z"), _("Home A"),_("All home")): continue
                        model.append((name,))
                    self.widgets[p].set_active(hal_input_names.index(self.data[p]))
                if self.data[ptype] in (1,2):                
                    for name in human_output_names: model.append((name,))
                    self.widgets[p].set_active(hal_output_names.index(self.data[p]))   
                if self.data[ptype] == 3:              
                    for name in human_servo_input_names: model.append((name,))
                    self.widgets[p].set_active(hal_servo_input_names.index(self.data[p]))
                    self.widgets[pinv].set_sensitive(0)
                if self.data[ptype] == 4:
                    for name in human_servo_output_names: model.append((name,))
                    self.widgets[p].set_active(hal_servo_output_names.index(self.data[p])) 
                    self.widgets[pinv].set_sensitive(0)
                if self.data[ptype] == 5:              
                    for name in human_stepper_names: model.append((name,))
                    self.widgets[p].set_active(hal_stepper_names.index(self.data[p]))
                    self.widgets[pinv].set_sensitive(0)
                model = self.widgets[ptype].get_model()
                model.clear()
                i = self.data[ptype]                      
                if i in (0,1,2):
                    for j in (0,1,2):
                        temp = pintype_names[j]
                        model.append((temp,))
                    self.widgets[ptype].set_active(i)
                else:
                    temp = pintype_names[i]
                    model.append((temp,))
                    self.widgets[ptype].set_active(0)
        self.in_mesa_prepare = False

    def on_mesa5i20_next(self, *args):
        for connector in (3,4):
            for pin in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15):
                foundit = 0
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                self.get_input_signals_from_gui(p)
                p = 'm5i20c%(con)dpin%(num)dinv' % {'con':connector ,'num': pin}
                self.data[p] = self.widgets[p].get_active()
            for pin in (16,17,18,19,20,21,22,23):
                foundit = 0
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                selection = self.widgets[p].get_active_text()
                for i in human_output_names:
                   if selection == i : foundit = 1
                if not foundit:
                  model = self.widgets[p].get_model()
                  model.append((selection,))
                  g = human_output_names
                  g.append ((selection))
                  hal_output_names.append ((selection))
                  self.data.haloutputsignames.append ((selection))
                self.data[p] = hal_output_names[self.widgets[p].get_active()]
                p = 'm5i20c%(con)dpin%(num)dinv' % {'con':connector ,'num': pin}
                self.data[p] = self.widgets[p].get_active()     
        if self.data.number_pports == 0 :       
           self.widgets.druid1.set_page(self.widgets.xaxismotor)
           return True 

    def get_input_signals_from_gui(self,p):
        foundit = 0
        selection = self.widgets[p].get_active_text()
        for index , i in enumerate(human_input_names):
            if selection == i : 
                foundit = 1
                break               
        if not foundit:
             model = self.widgets[p].get_model()
             model.append((selection,))
             g = human_input_names
             g.append ((selection))
             hal_input_names.append ((selection))
             self.data.halinputsignames.append ((selection))
        self.data[p] = hal_input_names[index]
            

    def on_m5i20panel_clicked(self, *args):self.m5i20test(self)
    
    def on_m5i20pintype_changed(self, *args):
         if self.in_mesa_prepare == True: return
         #connector = 3
         #pin = 0
         for connector in (2,3,4):
            for pin in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23): 
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                ptype = 'm5i20c%(con)dpin%(num)dtype' % {'con':connector ,'num': pin}    
                old = self.data[ptype]
                new = self.widgets[ptype].get_active()  
                print p, "old", old, "new", new       
                if old == 0 and new in (1,2):
                    print "switch to output"
                    model = self.widgets[p].get_model()
                    model.clear()
                    for name in human_output_names: model.append((name,))
                    self.widgets[p].set_active(0)
                    self.data[p] = UNUSED_OUTPUT
                    self.data[ptype] = new
                if old in (1,2) and new == 0:
                    print "switch to input"
                    model = self.widgets[p].get_model()
                    model.clear()
                    for name in human_input_names:
                        if self.data.limitshared or self.data.limitsnone:
                            if name in human_names_limit_only: continue 
                        if self.data.limitswitch or self.data.limitsnone:
                            if name in human_names_shared_home: continue                          
                        if self.data.homenone or self.data.limitshared:
                            if name in (_("Home X"), _("Home Y"), _("Home Z"), _("Home A"),_("All home")): continue
                        model.append((name,))
                    self.widgets[p].set_active(0)
                    self.data[p] = UNUSED_INPUT
                    print p, old, new
                    self.data[ptype] = new
       
    def on_firstpport_prepare(self, *args):
        self.in_pport_prepare = True
        self.prepare_parport("first")
        c = self.data.firstpp_direction
        if c:
                self.widgets.firstpport.set_title(_("First Parallel Port set for OUTPUT"))
        else:
                self.widgets.firstpport.set_title(_("First Parallel Port set for INPUT"))   

    def on_firstpport_next(self, *args):
        self.next_parport("first")
        #self.findsignal("all-home")          
        #on_pport_back = on_pport_next
        if self.data.number_pports<2:
                self.widgets.druid1.set_page(self.widgets.xaxismotor)
                return True

    def on_firstpport_back(self, *args):
         if not self.data.mesa5i20 :
                self.widgets.druid1.set_page(self.widgets.GUIconfig)
                return True

    def on_Secondpport_prepare(self, *args):
         self.prepare_parport("Second")
         c = self.data.Secondpp_direction
         if c:
                self.widgets.Secondpport.set_title(_("Second Parallel Port set for OUTPUT"))
         else:
                self.widgets.Secondpport.set_title(_("Second Parallel Port set for INPUT"))

    def on_Secondpport_next(self, *args):
        self.next_parport("Second")
        if self.data.number_pports<3:
                self.widgets.druid1.set_page(self.widgets.xaxismotor)
                return True

    def on_Thirdpport_prepare(self, *args):
         self.prepare_parport("Third")
         c = self.data.Thirdpp_direction
         if c:
                self.widgets.Thirdpport.set_title(_("Third Parallel Port set for OUTPUT"))
         else:
                self.widgets.Thirdpport.set_title(_("Third Parallel Port set for INPUT"))
  
    def on_Thirdpport_next(self, *args):
        self.next_parport("Third")

    def prepare_parport(self,portname):
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = '%spppin%dout' % (portname,pin)
            model = self.widgets[p].get_model()
            model.clear()
            for name in human_output_names: model.append((name,))
            self.widgets[p].set_active(hal_output_names.index(self.data[p]))
            p = '%spppin%dinvout' % (portname, pin)
            self.widgets[p].set_active(self.data[p])
        for pin in (1,2,3,4,5,6,7,8,10,11,12,13,15):
            p = '%spppin%din' % (portname, pin)
            model = self.widgets[p].get_model()
            model.clear()
            for name in human_input_names:
                    if self.data.limitshared or self.data.limitsnone:
                        if name in human_names_limit_only: continue 
                    if self.data.limitswitch or self.data.limitsnone:
                        if name in human_names_shared_home: continue                          
                    if self.data.homenone or self.data.limitshared:
                        if name in (_("Home X"), _("Home Y"), _("Home Z"), _("Home A"),_("All home")): continue         
                    model.append((name,))
            self.widgets[p].set_active(hal_input_names.index(self.data[p]))
            p = '%spppin%dinvin' % (portname, pin)
            self.widgets[p].set_active(self.data[p])
        self.widgets[portname+"pppin1out"].grab_focus()
        self.in_pport_prepare = False
        c = self.data[portname+"pp_direction"]
        for pin in (1,2,3,4,5,6,7,8):
            p = '%spppin%dout' % (portname, pin)
            self.widgets[p].set_sensitive(c)
            if not c :self.widgets[p].set_active(hal_output_names.index("unused-output"))
            p = '%spppin%dinvout' % (portname, pin)
            self.widgets[p].set_sensitive(c)
            p = '%spppin%din' % (portname, pin)
            self.widgets[p].set_sensitive(not c)
            if not c :self.widgets[p].set_active(hal_input_names.index("unused-input"))
            p = '%spppin%dinvin' % (portname, pin)
            self.widgets[p].set_sensitive(not c)

    def next_parport(self,portname):
        for pin in (1,2,3,4,5,6,7,8,10,11,12,13,15):           
            p = '%spppin%din' % (portname, pin)
            self.get_input_signals_from_gui(p)
            p = '%spppin%dinvin' % (portname, pin)
            self.data[p] = self.widgets[p].get_active()
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):           
            foundit = 0
            p = '%spppin%dout' % (portname, pin)
            selection = self.widgets[p].get_active_text()
            for i in human_output_names:
               if selection == i : foundit = 1
            if not foundit:
                model = self.widgets[p].get_model()
                model.append((selection,))
                g = human_output_names
                g.append ((selection))
                hal_output_names.append ((selection))
                self.data.haloutputsignames.append ((selection))
            self.data[p] = hal_output_names[self.widgets[p].get_active()]
        self.data[p] = hal_output_names[self.widgets[p].get_active()]
        p = '%spppin%dinvout' % (portname, pin)
        self.data[p] = self.widgets[p].get_active() 
        
    def on_xaxismotor_prepare(self, *args):
        self.axis_prepare('x')
    def on_xaxismotor_next(self, *args):     
        self.axis_done('x')
        self.widgets.druid1.set_page(self.widgets.xaxis)
        return True
    def on_xaxismotor_back(self, *args):
        if self.data.number_pports==1:
                self.widgets.druid1.set_page(self.widgets.firstpport)
                return True
        elif self.data.number_pports==2:
                self.widgets.druid1.set_page(self.widgets.Secondpport)
                return True
        elif self.data.number_pports==3:
                self.widgets.druid1.set_page(self.widgets.Thirdpport)
                return True
        elif self.data.mesa5i20 :
                self.widgets.druid1.set_page(self.widgets.mesa5i20)
                return True    

    def on_yaxismotor_prepare(self, *args):
        self.axis_prepare('y')
    def on_yaxismotor_next(self, *args):
        self.axis_done('y')
        self.widgets.druid1.set_page(self.widgets.yaxis)
        return True
    def on_yaxismotor_back(self, *args):        
        self.widgets.druid1.set_page(self.widgets.xaxis)
        return True
    
    def on_zaxismotor_prepare(self, *args):
        self.axis_prepare('z')
    def on_zaxismotor_next(self, *args):
        self.axis_done('z')
        self.widgets.druid1.set_page(self.widgets.zaxis)
        return True
    def on_zaxismotor_back(self, *args):    
        if self.data.axes == 2:
            self.widgets.druid1.set_page(self.widgets.xaxis)
            return True    
        else:
            self.widgets.druid1.set_page(self.widgets.yaxis)
            return True

    def on_aaxismotor_prepare(self, *args):
        self.axis_prepare('a')
    def on_aaxismotor_next(self, *args):
        self.axis_done('a')
        self.widgets.druid1.set_page(self.widgets.aaxis)
        return True
    def on_aaxismotor_back(self, *args):        
        self.widgets.druid1.set_page(self.widgets.zaxis)
        return True

    def axis_prepare(self, axis):
        d = self.data
        w = self.widgets
        def set_text(n): w[axis + n].set_text("%s" % d[axis + n])
        def set_value(n): w[axis + n].set_value(d[axis + n])
        def set_active(n): w[axis + n].set_active(d[axis + n])
        
        #set_text("steprev")
        #set_text("microstep")
        set_value("P")
        set_value("I")
        set_value("D")
        set_value("FF0")
        set_value("FF1")
        set_value("FF2")
        set_text("maxferror")
        set_text("minferror")
        set_text("outputscale")
        set_text("outputoffset")
        set_active("invertmotor")
        set_active("invertencoder")  
        set_text("maxoutput")
        set_text("pulleynum")
        set_text("pulleyden")
        set_text("leadscrew")
        set_text("compfilename")
        set_active("comptype")
        set_value("backlash")
        set_active("usecomp")
        w[axis+"encoderlines"].set_text("%d" % (d[axis+"encodercounts"]/4))
        set_text("encodercounts")
        w[axis+"maxvel"].set_text("%d" % (d[axis+"maxvel"]*60))
        set_text("maxacc")
        set_text("homepos")
        set_text("minlim")
        set_text("maxlim")
        set_text("homesw")
        w[axis+"homevel"].set_text("%d" % (d[axis+"homevel"]*60))
        w[axis+"homelatchvel"].set_text("%d" % (d[axis+"homelatchvel"]*60))
        w[axis+"homefinalvel"].set_text("%d" % (d[axis+"homefinalvel"]*60))
        set_active("latchdir")
        set_active("usehomeindex")

        if axis == "a":
            w[axis + "screwunits"].set_text(_("degrees / rev"))
            w[axis + "velunits"].set_text(_("degrees / minute"))
            w[axis + "accunits"].set_text(_("degrees / second²"))
            w[axis + "homevelunits"].set_text(_("degrees / minute"))
            w[axis + "homelatchvelunits"].set_text(_("degrees / minute"))
            w[axis + "homefinalvelunits"].set_text(_("degrees / minute"))
            w[axis + "accdistunits"].set_text(_("degrees"))
            w[axis + "testdistanceunits"].set_text(_("degrees"))
            w[axis + "resolutionunits1"].set_text(_("degrees / encoder pulse"))
            w[axis + "resolutionunits2"].set_text(_("degreess / encoder pulse"))
            w[axis + "scaleunits"].set_text(_("Encoder pulses / degree"))
            w[axis + "minfollowunits"].set_text(_("degrees"))
            w[axis + "maxfollowunits"].set_text(_("degrees"))

        elif d.units:
            w[axis + "screwunits"].set_text(_("Pitch (mm / rev)"))
            w[axis + "velunits"].set_text(_("mm / minute"))
            w[axis + "accunits"].set_text(_("mm / second²"))
            w[axis + "homevelunits"].set_text(_("mm / minute"))
            w[axis + "homelatchvelunits"].set_text(_("mm / minute"))
            w[axis + "homefinalvelunits"].set_text(_("mm / minute"))
            w[axis + "accdistunits"].set_text(_("mm"))
            w[axis + "resolutionunits1"].set_text(_("mm / encoder pulse"))
            w[axis + "resolutionunits2"].set_text(_("mm / encoder pulse"))
            w[axis + "scaleunits"].set_text(_("Encoder pulses / mm"))
            w[axis + "testdistanceunits"].set_text(_("mm"))
            w[axis + "minfollowunits"].set_text(_("mm"))
            w[axis + "maxfollowunits"].set_text(_("mm"))
           
        else:
            w[axis + "screwunits"].set_text(_("TPI (rev / inch)"))
            w[axis + "velunits"].set_text(_("inches / minute"))
            w[axis + "accunits"].set_text(_("inches / second²"))
            w[axis + "homevelunits"].set_text(_("inches / minute"))
            w[axis + "homelatchvelunits"].set_text(_("inches / minute"))
            w[axis + "homefinalvelunits"].set_text(_("inches / minute"))
            w[axis + "accdistunits"].set_text(_("inches"))
            w[axis + "resolutionunits1"].set_text(_("inches / encoder pulse"))
            w[axis + "resolutionunits2"].set_text(_("inches / encoder pulse"))
            w[axis + "scaleunits"].set_text(_("Encoder pulses / inch"))
            w[axis + "testdistanceunits"].set_text(_("inches"))
            w[axis + "minfollowunits"].set_text(_("inches"))
            w[axis + "maxfollowunits"].set_text(_("inches"))
           
        thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis,"max-home-" + axis, "both-home-" + axis))
        homes = False
        for i in thisaxishome:
            test = self.data.findsignal(i)
            if not test == "false": homes = True
        w[axis + "homesw"].set_sensitive(homes)
        w[axis + "homevel"].set_sensitive(homes)
        w[axis + "latchdir"].set_sensitive(homes)
        w[axis + "usehomeindex"].set_sensitive(homes)
        w[axis + "homefinalvel"].set_sensitive(homes)
        w[axis + "homelatchvel"].set_sensitive(homes)

        i = d[axis + "usecomp"]
        w[axis + "comptype"].set_sensitive(i)
        w[axis + "compfilename"].set_sensitive(i)
        i = d[axis + "usebacklash"]
        w[axis + "backlash"].set_sensitive(i)
      #  w[axis + "steprev"].grab_focus()
        gobject.idle_add(lambda: self.update_pps(axis))

    def on_xusecomp_toggled(self, *args): self.comp_toggle('x')
    def on_yusecomp_toggled(self, *args): self.comp_toggle('y')
    def on_zusecomp_toggled(self, *args): self.comp_toggle('z')
    def on_ausecomp_toggled(self, *args): self.comp_toggle('a')
    def on_xusebacklash_toggled(self, *args): self.backlash_toggle('x')
    def on_yusebacklash_toggled(self, *args): self.backlash_toggle('y')
    def on_zusebacklash_toggled(self, *args): self.backlash_toggle('z')
    def on_ausebacklash_toggled(self, *args): self.backlash_toggle('a')

    def comp_toggle(self, axis):
        i = self.widgets[axis + "usecomp"].get_active()   
        self.widgets[axis + "compfilename"].set_sensitive(i)
        self.widgets[axis + "comptype"].set_sensitive(i)
        if i:
            self.widgets[axis + "backlash"].set_sensitive(0)
            self.widgets[axis + "usebacklash"].set_active(0)

    def backlash_toggle(self, axis):
        i = self.widgets[axis + "usebacklash"].get_active()   
        self.widgets[axis + "backlash"].set_sensitive(i)
        if i:
            self.widgets[axis + "compfilename"].set_sensitive(0)
            self.widgets[axis + "comptype"].set_sensitive(0)
            self.widgets[axis + "usecomp"].set_active(0)


    def axis_done(self, axis):
        d = self.data
        w = self.widgets
        def get_text(n): d[axis + n] = float(w[axis + n].get_text())
        def get_active(n): d[axis + n] = w[axis + n].get_active()
        #get_text("steprev")
        #get_text("microstep")
        get_text("P")
        get_text("I")
        get_text("D")
        get_text("FF0")
        get_text("FF1")
        get_text("FF2")
        get_text("maxferror")
        get_text("minferror")
        get_text("outputscale")
        get_text("outputoffset")
        get_text("maxoutput")
        get_text("encodercounts")
        get_active("invertmotor")
        get_active("invertencoder") 
        get_text("pulleynum")
        get_text("pulleyden")
        get_text("leadscrew")
        d[axis + "compfilename"] = w[axis + "compfilename"].get_text()
        get_active("comptype")
        d[axis + "backlash"]= w[axis + "backlash"].get_value()
        get_active("usecomp")
        get_active("usebacklash")
        d[axis + "maxvel"] = (float(w[axis + "maxvel"].get_text())/60)
        get_text("maxacc")
        get_text("homepos")
        get_text("minlim")
        get_text("maxlim")
        get_text("homesw")
        d[axis + "homevel"] = (float(w[axis + "homevel"].get_text())/60)
        d[axis + "homelatchvel"] = (float(w[axis + "homelatchvel"].get_text())/60)
        d[axis + "homefinalvel"] = (float(w[axis + "homefinalvel"].get_text())/60)
        get_active("latchdir")
        get_active("usehomeindex")

    def update_pps(self, axis):
        #return # temp change
        w = self.widgets
        d = self.data
        def get(n): return float(w[axis + n].get_text())

        try:
            pitch = get("leadscrew")
            if d.units == 1 or axis =='a' : pitch = 1./pitch
            pps = (pitch * 
                (get("pulleynum") / get("pulleyden")) * (get("maxvel")/60))
            if pps == 0: raise ValueError
            pps = abs(pps)
            acctime = (get("maxvel")/60) / get("maxacc")
            accdist = acctime * .5 * (get("maxvel")/60)
            w[axis + "acctime"].set_text("%.4f" % acctime)
            w[axis + "accdist"].set_text("%.4f" % accdist)
           # w[axis + "hz"].set_text("%.1f" % pps)
            w[axis+"encodercounts"].set_text( "%d" % ( 4 * float(w[axis+"encoderlines"].get_text())))
            scale = self.data[axis + "scale"] = ( ( pitch) * get("encodercounts") 
                * (get("pulleynum") / get("pulleyden")))
            w[axis + "scale"].set_text("%.1f" % scale)
            w[axis + "chartresolution"].set_text("%.7f" % (1.0 / scale))
            self.widgets.druid1.set_buttons_sensitive(1,1,1,1)
            w[axis + "axistest"].set_sensitive(1)
        except (ValueError, ZeroDivisionError): # Some entries not numbers or not valid
            w[axis + "acctime"].set_text("")
            w[axis + "accdist"].set_text("")
            #w[axis + "hz"].set_text("")
            w[axis + "scale"].set_text("")
            self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
            w[axis + "axistest"].set_sensitive(0)

    def on_xpulleynum_changed(self, *args): self.update_pps('x')
    def on_ypulleynum_changed(self, *args): self.update_pps('y')
    def on_zpulleynum_changed(self, *args): self.update_pps('z')
    def on_apulleynum_changed(self, *args): self.update_pps('a')

    def on_xencoderlines_changed(self, *args):self.update_pps('x')
    def on_yencoderlines_changed(self, *args):self.update_pps('y')
    def on_zencoderlines_changed(self, *args):self.update_pps('z')
    def on_aencoderlines_changed(self, *args):self.update_pps('a')
 
    def on_xpulleyden_changed(self, *args): self.update_pps('x')
    def on_ypulleyden_changed(self, *args): self.update_pps('y')
    def on_zpulleyden_changed(self, *args): self.update_pps('z')
    def on_apulleyden_changed(self, *args): self.update_pps('a')

    def on_xleadscrew_changed(self, *args): self.update_pps('x')
    def on_yleadscrew_changed(self, *args): self.update_pps('y')
    def on_zleadscrew_changed(self, *args): self.update_pps('z')
    def on_aleadscrew_changed(self, *args): self.update_pps('a')

    def on_xmaxvel_changed(self, *args): self.update_pps('x')
    def on_ymaxvel_changed(self, *args): self.update_pps('y')
    def on_zmaxvel_changed(self, *args): self.update_pps('z')
    def on_amaxvel_changed(self, *args): self.update_pps('a')

    def on_xmaxacc_changed(self, *args): self.update_pps('x')
    def on_ymaxacc_changed(self, *args): self.update_pps('y')
    def on_zmaxacc_changed(self, *args): self.update_pps('z')
    def on_amaxacc_changed(self, *args): self.update_pps('a')
        
    def on_xaxis_prepare(self, *args): self.axis_prepare('x')
    def on_yaxis_prepare(self, *args): self.axis_prepare('y')
    def on_zaxis_prepare(self, *args): self.axis_prepare('z')
    def on_aaxis_prepare(self, *args): self.axis_prepare('a')
   
    def on_xaxis_next(self, *args):
        self.axis_done('x')
        if self.data.axes == 2:
            self.widgets.druid1.set_page(self.widgets.zaxismotor)
            return True
        else:
            self.widgets.druid1.set_page(self.widgets.yaxismotor)
            return True
    def on_yaxis_next(self, *args):
        self.axis_done('y')
        self.widgets.druid1.set_page(self.widgets.zaxismotor)
        return True  
    def on_xaxis_back(self, *args):
        self.axis_done('x')
        self.widgets.druid1.set_page(self.widgets.xaxismotor)
        return True
    def on_yaxis_back(self, *args): 
        self.axis_done('y')
        self.widgets.druid1.set_page(self.widgets.yaxismotor)
        return True
    def on_zaxis_next(self, *args):
        if self.data.axes != 1 :
            if self.has_spindle_speed_control():
                self.widgets.druid1.set_page(self.widgets.spindle)
                return True
            else:
                self.widgets.druid1.set_page(self.widgets.advanced)
                return True
        else:
            self.widgets.druid1.set_page(self.widgets.aaxismotor)
            return True
    def on_zaxis_back(self, *args):
        self.axis_done('z')     
        self.widgets.druid1.set_page(self.widgets.zaxismotor)
        return True
    def on_aaxis_next(self, *args):
        self.axis_done('a')
        if self.has_spindle_speed_control():
            self.widgets.druid1.set_page(self.widgets.spindle)
        else:
            self.widgets.druid1.set_page(self.widgets.advanced)
        return True
    def on_aaxis_back(self, *args):
        self.axis_done('a')
        self.widgets.druid1.set_page(self.widgets.aaxismotor)
        return True

    def on_xaxistest_clicked(self, *args): self.test_axis('x')
    def on_yaxistest_clicked(self, *args): self.test_axis('y')
    def on_zaxistest_clicked(self, *args): self.test_axis('z')
    def on_aaxistest_clicked(self, *args): self.test_axis('a')

    def on_xaxistune_clicked(self, *args): self.tune_axis('x')
    def on_yaxistune_clicked(self, *args): self.tune_axis('y')
    def on_zaxistune_clicked(self, *args): self.tune_axis('z')
    def on_aaxistune_clicked(self, *args): self.tune_axis('a')

    def on_spindle_prepare(self, *args):
        self.widgets['spindlecarrier'].set_text("%s" % self.data.spindlecarrier)
        self.widgets['spindlespeed1'].set_text("%s" % self.data.spindlespeed1)
        self.widgets['spindlespeed2'].set_text("%s" % self.data.spindlespeed2)
        self.widgets['spindlepwm1'].set_text("%s" % self.data.spindlepwm1)
        self.widgets['spindlepwm2'].set_text("%s" % self.data.spindlepwm2)
        self.widgets['spindlecpr'].set_text("%s" % self.data.spindlecpr)

        d = self.data
        has_spindle_pha = self.data.findsignal("spindle-phase-a")
        if has_spindle_pha == "false":
            self.widgets.spindlecpr.set_sensitive(0)
        else: self.widgets.spindlecpr.set_sensitive(1)        

    def on_spindle_next(self, *args):
        self.data.spindlecarrier = float(self.widgets.spindlecarrier.get_text())
        self.data.spindlespeed1 = float(self.widgets.spindlespeed1.get_text())
        self.data.spindlespeed2 = float(self.widgets.spindlespeed2.get_text())
        self.data.spindlepwm1 = float(self.widgets.spindlepwm1.get_text())
        self.data.spindlepwm2 = float(self.widgets.spindlepwm2.get_text())
        self.data.spindlecpr = float(self.widgets.spindlecpr.get_text())
        
    def on_spindle_back(self, *args):
        self.on_spindle_next()
        if self.data.axes != 1:
            self.widgets.druid1.set_page(self.widgets.zaxis)
        else:
            self.widgets.druid1.set_page(self.widgets.aaxis)
        return True

    def on_advanced_prepare(self, *args):       
        self.widgets.pyvcp.set_active(self.data.pyvcp)
        self.on_pyvcp_toggled()
        if  not self.widgets.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custompanel.xml" % self.data.machinename)):
                self.widgets.pyvcpexist.set_active(True)
        self.widgets.classicladder.set_active(self.data.classicladder)
        self.widgets.modbus.set_active(self.data.modbus)
        self.widgets.digitsin.set_value(self.data.digitsin)
        self.widgets.digitsout.set_value(self.data.digitsout)
        self.widgets.s32in.set_value(self.data.s32in)
        self.widgets.s32out.set_value(self.data.s32out)
        self.widgets.floatsin.set_value(self.data.floatsin)
        self.widgets.floatsout.set_value(self.data.floatsout)
        self.widgets.halui.set_active(self.data.halui)
        self.widgets.ladderconnect.set_active(self.data.ladderconnect)
        self.widgets.pyvcpconnect.set_active(self.data.pyvcpconnect)
        self.on_classicladder_toggled()
        if  not self.widgets.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custom.clp" % self.data.machinename)):
                self.widgets.ladderexist.set_active(True)

    def on_advanced_next(self, *args):
        self.data.pyvcp = self.widgets.pyvcp.get_active()
        self.data.classicladder = self.widgets.classicladder.get_active()
        self.data.modbus = self.widgets.modbus.get_active()
        self.data.digitsin = self.widgets.digitsin.get_value()
        self.data.digitsout = self.widgets.digitsout.get_value()
        self.data.s32in = self.widgets.s32in.get_value()
        self.data.s32out = self.widgets.s32out.get_value()
        self.data.floatsin = self.widgets.floatsin.get_value()
        self.data.floatsout = self.widgets.floatsout.get_value()
        self.data.halui = self.widgets.halui.get_active()    
        self.data.pyvcpconnect = self.widgets.pyvcpconnect.get_active()  
        self.data.ladderconnect = self.widgets.ladderconnect.get_active()          
        if self.data.classicladder:
           if self.widgets.ladderblank.get_active() == True:
              if self.data.tempexists:
                   self.data.laddername='TEMP.clp'
              else:
                   self.data.laddername= 'blank.clp'
                   self.data.ladderhaltype = 0
           if self.widgets.ladder1.get_active() == True:
              self.data.laddername = 'estop.clp'
              has_estop = self.data.findsignal("estop-ext")
              if has_estop == "false":
                 self.warning_dialog(_("You need to designate an E-stop input pin for this ladder program."),True)
                 self.widgets.druid1.set_page(self.widgets.advanced)
                 return True
              self.data.ladderhaltype = 1
           if self.widgets.ladder2.get_active() == True:
                 self.data.laddername = 'serialmodbus.clp'
                 self.data.modbus = 1
                 self.widgets.modbus.set_active(self.data.modbus) 
                 self.data.ladderhaltype = 0          
           if self.widgets.ladderexist.get_active() == True:
              self.data.laddername='custom.clp'
           else:
               if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custom.clp" % self.data.machinename)):
                  if not self.warning_dialog(_("OK to replace existing custom ladder program?\nExisting Custom.clp will be renamed custom_backup.clp.\nAny existing file named -custom_backup.clp- will be lost. "),False):
                     self.widgets.druid1.set_page(self.widgets.advanced)
                     return True 
           if self.widgets.ladderexist.get_active() == False:
              if os.path.exists(os.path.join(distdir, "configurable_options/ladder/TEMP.clp")):
                 if not self.warning_dialog(_("You edited a ladder program and have selected a different program to copy to your configuration file.\nThe edited program will be lost.\n\nAre you sure?  "),False):
                   self.widgets.druid1.set_page(self.widgets.advanced)
                   return True       
        if self.data.pyvcp == True:
           if self.widgets.pyvcpblank.get_active() == True:
              self.data.pyvcpname = "blank.xml"
              self.pyvcphaltype = 0
           if self.widgets.pyvcp1.get_active() == True:
              self.data.pyvcpname = "spindle.xml"
              self.data.pyvcphaltype = 1
           if self.widgets.pyvcp2.get_active() == True:
              self.data.pyvcpname = "xyzjog.xml"
              self.data.pyvcphaltype = 2
              self.data.halui = True 
              self.widgets.halui.set_active(True)   
           if self.widgets.pyvcpexist.get_active() == True:
              self.data.pyvcpname = "custompanel.xml"
           else:
              if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custompanel.xml" % self.data.machinename)):
                 if not self.warning_dialog(_("OK to replace existing custom pyvcp panel and custom_postgui.hal file ?\nExisting custompanel.xml and custom_postgui.hal will be renamed custompanel_backup.xml and postgui_backup.hal.\nAny existing file named custompanel_backup.xml and custom_postgui.hal will be lost. "),False):
                   return True

    def on_advanced_back(self, *args):
        if self.has_spindle_speed_control():
            self.widgets.druid1.set_page(self.widgets.spindle)
        elif self.data.axes != 1:
            self.widgets.druid1.set_page(self.widgets.zaxis)
        else:
            self.widgets.druid1.set_page(self.widgets.aaxis)
        return True

    def on_loadladder_clicked(self, *args):self.load_ladder(self)

    def on_classicladder_toggled(self, *args):

        i= self.widgets.classicladder.get_active()
        self.widgets.digitsin.set_sensitive(i)
        self.widgets.digitsout.set_sensitive(i)
        self.widgets.s32in.set_sensitive(i)
        self.widgets.s32out.set_sensitive(i)
        self.widgets.floatsin.set_sensitive(i)
        self.widgets.floatsout.set_sensitive(i)
        self.widgets.modbus.set_sensitive(i)
        self.widgets.ladderblank.set_sensitive(i)
        self.widgets.ladder1.set_sensitive(i)
        self.widgets.ladder2.set_sensitive(i)
        if  self.widgets.createconfig.get_active():
            self.widgets.ladderexist.set_sensitive(False)
        else:
            self.widgets.ladderexist.set_sensitive(i)
        self.widgets.loadladder.set_sensitive(i)
        self.widgets.label_digin.set_sensitive(i)
        self.widgets.label_digout.set_sensitive(i)
        self.widgets.label_s32in.set_sensitive(i)
        self.widgets.label_s32out.set_sensitive(i)
        self.widgets.label_floatin.set_sensitive(i)
        self.widgets.label_floatout.set_sensitive(i)
        self.widgets.ladderconnect.set_sensitive(i)
        

    def on_pyvcp_toggled(self,*args):
        i= self.widgets.pyvcp.get_active()
        self.widgets.pyvcpblank.set_sensitive(i)
        self.widgets.pyvcp1.set_sensitive(i)
        self.widgets.pyvcp2.set_sensitive(i)
        if  self.widgets.createconfig.get_active():
            self.widgets.pyvcpexist.set_sensitive(False)
        else:
            self.widgets.pyvcpexist.set_sensitive(i)
        self.widgets.displaypanel.set_sensitive(i)
        self.widgets.pyvcpconnect.set_sensitive(i)

    def on_displaypanel_clicked(self,*args):
        self.testpanel(self)

    def on_complete_back(self, *args):
        self.widgets.druid1.set_page(self.widgets.advanced)
        return True

    def has_spindle_speed_control(self):
        d = self.data
        has_spindle_pwm = self.data.findsignal("spindle-pwm")
        if has_spindle_pwm == "false":
            return False
        else: return True
   
    def on_complete_finish(self, *args):
        self.data.save()        
        if self.data.classicladder: 
           if not self.data.laddername == "custom.clp":
                filename = os.path.join(distdir, "configurable_options/ladder/%s" % self.data.laddername)
                original = os.path.expanduser("~/emc2/configs/%s/custom.clp" % self.data.machinename)
                if os.path.exists(filename):     
                  if os.path.exists(original):
                     print "custom file already exists"
                     shutil.copy( original,os.path.expanduser("~/emc2/configs/%s/custom_backup.clp" % self.data.machinename) ) 
                     print "made backup of existing custom"
                  shutil.copy( filename,original)
                  print "copied ladder program to usr directory"
                  print"%s" % filename
                else:
                     print "Master or temp ladder files missing from configurable_options dir"

        if self.data.pyvcp and not self.widgets.pyvcpexist.get_active() == True:                
           panelname = os.path.join(distdir, "configurable_options/pyvcp/%s" % self.data.pyvcpname)
           originalname = os.path.expanduser("~/emc2/configs/%s/custompanel.xml" % self.data.machinename)
           if os.path.exists(panelname):     
                  if os.path.exists(originalname):
                     print "custom PYVCP file already exists"
                     shutil.copy( originalname,os.path.expanduser("~/emc2/configs/%s/custompanel_backup.xml" % self.data.machinename) ) 
                     print "made backup of existing custom"
                  shutil.copy( panelname,originalname)
                  print "copied PYVCP program to usr directory"
                  print"%s" % panelname
           else:
                  print "Master PYVCP files missing from configurable_options dir"
        gtk.main_quit()

    def on_calculate_ideal_period(self, *args):
        steptime = self.widgets.steptime.get_value()
        stepspace = self.widgets.stepspace.get_value()
        latency = self.widgets.latency.get_value()
        minperiod = self.data.minperiod(steptime, stepspace, latency)
        maxhz = int(1e9 / minperiod)
        if not self.data.doublestep(steptime): maxhz /= 2
        self.widgets.baseperiod.set_text("%d ns" % minperiod)
        self.widgets.maxsteprate.set_text("%d Hz" % maxhz)

    def on_latency_test_clicked(self, w):
        self.latency_pid = os.spawnvp(os.P_NOWAIT,
                                "latency-test", ["latency-test"])
        self.widgets['window1'].set_sensitive(0)
        gobject.timeout_add(15, self.latency_running_callback)

    def latency_running_callback(self):
        pid, status = os.waitpid(self.latency_pid, os.WNOHANG)
        if pid:
            self.widgets['window1'].set_sensitive(1)
            return False
        return True

    def m5i20test(self,w):
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        self.terminal = terminal = os.popen("gnome-terminal --title=joystick_search -x less /proc/bus/input/devices", "w" )  
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )    
        halrun.write("loadusr -Wn m5i20test pyvcp -c m5i20test %(panel)s\n" %{'panel':"m5i20panel.xml",})
        halrun.write("loadusr halmeter\n")
        halrun.write("start\n")
        halrun.write("waitusr m5i20test\n"); halrun.flush()
        halrun.close()
        terminal.close()
    
    def testpanel(self,w):
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        if self.widgets.pyvcpblank.get_active() == True:
           return True
        if self.widgets.pyvcp1.get_active() == True:
           panel = "spindle.xml"
        if self.widgets.pyvcp2.get_active() == True:
           panel = "xyzjog.xml"
        if self.widgets.pyvcpexist.get_active() == True:
           panel = "custompanel.xml"
           panelname = os.path.expanduser("~/emc2/configs/%s" % self.data.machinename)
        print "panel-%s" % panel
        print"dir-%s" % panelname
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )    
        halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
        if self.widgets.pyvcp1.get_active() == True:
                halrun.write("setp displaytest.spindle-speed 1000\n")
                halrun.write("setp displaytest.toolnumber 4\n")
        halrun.write("waitusr displaytest\n"); halrun.flush()
        halrun.close()

    def load_ladder(self,w):   
        newfilename = os.path.join(distdir, "configurable_options/ladder/TEMP.clp")    
        self.data.modbus = self.widgets.modbus.get_active()
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")
        halrun.write(""" 
              loadrt classicladder_rt numPhysInputs=%(din)d numPhysOutputs=%(dout)d numS32in=%(sin)d numS32out=%(sout)d numFloatIn=%(fin)d numFloatOut=%(fout)d\n""" % {
                      'din': self.widgets.digitsin.get_value(),
                      'dout': self.widgets.digitsout.get_value(),
                      'sin': self.widgets.s32in.get_value(),
                      'sout': self.widgets.s32out.get_value(), 
                      'fin':self.widgets.floatsin.get_value(),
                      'fout':self.widgets.floatsout.get_value(),
                 })
        if self.widgets.ladderexist.get_active() == True:
            if self.data.tempexists:
               self.data.laddername='TEMP.clp'
            else:
               self.data.laddername= 'blank.clp'
        if self.widgets.ladder1.get_active() == True:
            self.data.laddername= 'estop.clp'
        if self.widgets.ladder2.get_active() == True:
            self.data.laddername = 'serialmodbus.clp'
            self.data.modbus = True
            self.widgets.modbus.set_active(self.data.modbus)
        if self.widgets.ladderexist.get_active() == True:
            self.data.laddername='custom.clp'
            originalfile = filename = os.path.expanduser("~/emc2/configs/%s/custom.clp" % self.data.machinename)
        else:
            filename = os.path.join(distdir, "configurable_options/ladder/"+ self.data.laddername)        
        if self.data.modbus == True: 
            halrun.write("loadusr -w classicladder --modmaster --newpath=%(newfilename)s %(filename)s\n" %          { 'newfilename':newfilename ,'filename':filename })
        else:
            halrun.write("loadusr -w classicladder --newpath=%(newfilename)s %(filename)s\n" % { 'newfilename':newfilename ,'filename':filename })
        halrun.write("start\n"); halrun.flush()
        halrun.close()
        if os.path.exists(newfilename):
            self.data.tempexists = True
            self.widgets.newladder.set_text('Edited ladder program')
            self.widgets.ladderexist.set_active(True)
        else:
            self.data.tempexists = 0
        
    def tune_axis(self, axis):
        d = self.data
        w = self.widgets
        axnum = "xyza".index(axis)
        w.notebook2.set_current_page(axnum)

        if axis == "a":
            w[axis + "testdistunits"].set_text(_("degrees"))
            w[axis + "testvelunits"].set_text(_("degrees / minute"))
            w[axis + "testaccunits"].set_text(_("degrees / second²"))
        elif d.units:
            w[axis + "testdistunits"].set_text(_("mm"))
            w[axis + "testvelunits"].set_text(_("mm / minute"))
            w[axis + "testaccunits"].set_text(_("mm / second²"))
        else:
            w[axis + "testdistunits"].set_text(_("inches"))
            w[axis + "testvelunits"].set_text(_("inches / minute"))
            w[axis + "testaccunits"].set_text(_("inches / second²"))
        w[axis+"testvel"].set_value(float(w[axis+"maxvel"].get_text()))
        w[axis+"testacc"].set_value(float(w[axis+"maxacc"].get_text()))
        w[axis+"testcurrentP"].set_value(w[axis+"P"].get_value())
        w[axis+"testorigP"].set_text("%s" % w[axis+"P"].get_value())
        w[axis+"testcurrentI"].set_value(w[axis+"I"].get_value())
        w[axis+"testorigI"].set_text("%s" % w[axis+"I"].get_value())
        w[axis+"testcurrentD"].set_value(w[axis+"D"].get_value())
        w[axis+"testorigD"].set_text("%s" % w[axis+"D"].get_value())
        w[axis+"testcurrentFF0"].set_value(w[axis+"FF0"].get_value())
        w[axis+"testorigFF0"].set_text("%s" % w[axis+"FF0"].get_value())
        w[axis+"testcurrentFF1"].set_value(w[axis+"FF1"].get_value())
        w[axis+"testorigFF1"].set_text("%s" % w[axis+"FF1"].get_value())
        w[axis+"testcurrentFF2"].set_value(w[axis+"FF2"].get_value())
        w[axis+"testorigFF2"].set_text("%s" % w[axis+"FF2"].get_value())
        w.dialog2.set_title(_("%s Axis Tune") % axis.upper())
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")
        halrun.write("""
        loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n
        loadusr halscope
        """ % {
            'period': 30000,
         })
        halrun.write("start\n"); halrun.flush()
        w.dialog2.show_all()
        self.widgets['window1'].set_sensitive(0)
        result = w.dialog2.run()
        w.dialog2.hide()
        if result == gtk.RESPONSE_OK:
            w[axis+"maxvel"].set_text("%s" % w[axis+"testvel"].get_value())
            w[axis+"maxacc"].set_text("%s" % w[axis+"testacc"].get_value())
            w[axis+"P"].set_value( float(w[axis+"testcurrentP"].get_text()))
            w[axis+"I"].set_value( float(w[axis+"testcurrentI"].get_text()))
            w[axis+"D"].set_value( float(w[axis+"testcurrentD"].get_text()))
            w[axis+"FF0"].set_value( float(w[axis+"testcurrentFF0"].get_text()))
            w[axis+"FF1"].set_value( float(w[axis+"testcurrentFF1"].get_text()))
            w[axis+"FF2"].set_value( float(w[axis+"testcurrentFF2"].get_text()))
        halrun.close()  
        self.widgets['window1'].set_sensitive(1)

    def test_axis(self, axis):
        data = self.data
        widgets = self.widgets

        fastdac = float(widgets["fastdac"].get_text())
        slowdac = float(widgets["slowdac"].get_text())
        dacspeed = widgets.Dac_speed_fast.get_active()
        widgets.xtestinvertmotor.set_active(widgets[axis+"invertmotor"].get_active())
        widgets.xtestinvertencoder.set_active(widgets[axis+"invertencoder"].get_active())
        
        
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")

        axnum = "xyza".index(axis)
        step = axis + "step"
        dir = axis + "dir"
        halrun.write("""            
            loadrt probe_parport
            loadrt hal_parport cfg=%(ioaddr)s
            loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n
            addf parport.0.write fast
            
        """ % {
            'period': 30000,
            'ioaddr': data.ioaddr,           
        })
       
        amp = data.find_output(AMP)
        if amp:
            halrun.write("setp parport.0.pin-%(enablepin)02d-out 1\n"
                % {'enablepin': amp})

        estop = data.find_output(ESTOP)
        if estop:
            halrun.write("setp parport.0.pin-%(estoppin)02d-out 1\n"
                % {'estoppin': estop})

        for pin in 1,2,3,4,5,6,7,8,9,14,16,17:
            inv = getattr(data, "firstpppin%dinvout" % pin)
            if inv:
                halrun.write("setp parport.0.pin-%(pin)02d-out-invert 1\n"
                    % {'pin': pin}) 

        widgets.dialog1.set_title(_("%s Axis Test") % axis.upper())
        self.jogplus = self.jogminus = 0
        self.axis_under_test = axis
        self.update_axis_params()

        halrun.write("start\n"); halrun.flush()
        self.widgets['window1'].set_sensitive(0)
        widgets.dialog1.show_all()
        result = widgets.dialog1.run()
        widgets.dialog1.hide()
        
        if amp:
            halrun.write("""setp parport.0.pin-%02d-out 0\n""" % amp)
        if estop:
            halrun.write("""setp parport.0.pin-%02d-out 0\n""" % estop)

        time.sleep(.001)

        halrun.close()        

        if result == gtk.RESPONSE_OK:
            #widgets[axis+"maxacc"].set_text("%s" % widgets.testacc.get_value())
            widgets[axis+"invertmotor"].set_active(widgets.xtestinvertmotor.get_active())
            widgets[axis+"invertencoder"].set_active(widgets.xtestinvertencoder.get_active())
            #widgets[axis+"maxvel"].set_text("%s" % widgets.testvel.get_value())
        self.axis_under_test = None
        self.widgets['window1'].set_sensitive(1)
    
    def update_axis_params(self, *args):
        axis = self.axis_under_test
        if axis is None: return
        halrun = self.halrun
        halrun.write("""
            setp steptest.0.jog-minus %(jogminus)s
            setp steptest.0.jog-plus %(jogplus)s
        """ % {
            'jogminus': self.jogminus,
            'jogplus': self.jogplus,           
        })
        halrun.flush()

    def on_jogminus_pressed(self, w):
        self.jogminus = 1
        self.update_axis_params()
    def on_jogminus_released(self, w):
        self.jogminus = 0
        self.update_axis_params()

    def on_jogplus_pressed(self, w):
        self.jogplus = 1
        self.update_axis_params()
    def on_jogplus_released(self, w):
        self.jogplus = 0
        self.update_axis_params()
    def run(self, filename=None):
        if filename is not None:
            self.data.load(filename, self)
            self.widgets.druid1.set_page(self.widgets.basicinfo)
        gtk.main()
   
    

def makedirs(d):
    try:
        os.makedirs(d)
    except os.error, detail:
        if detail.errno != errno.EEXIST: raise
makedirs(os.path.expanduser("~/emc2/configs"))

opts, args = getopt.getopt(sys.argv[1:], "fr")
mode = 0
force = 0
for k, v in opts:
    if k == "-r": mode = 1
    if k == "-f": force = 1

if mode:
    filename = args[0]
    data = Data()
    data.load(filename, None, force)
    data.save()
elif args:
    app = App()
    app.run(args[0])
else:
    app = App()
    app.run()
