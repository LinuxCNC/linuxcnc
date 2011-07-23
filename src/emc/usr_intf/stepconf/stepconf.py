#!/usr/bin/python2.4
# -*- encoding: utf-8 -*-
#    This is stepconf, a graphical configuration editor for emc2
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
import commands

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
                _("Stepconf encountered an error.  The following "
                "information may be useful in troubleshooting:\n\n")
                + "".join(lines))
    m.show()
    m.run()
    m.destroy()
sys.excepthook = excepthook

BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
import gettext
gettext.install("emc2", localedir=LOCALEDIR, unicode=True)
gtk.glade.bindtextdomain("emc2", LOCALEDIR)
gtk.glade.textdomain("emc2")

# internalname / displayed name / steptime/ step space / direction hold / direction setup
drivertypes = [
    ["gecko201", _("Gecko 201"), 500, 4000, 20000, 1000],
    ["gecko202", _("Gecko 202"), 500, 4500, 20000, 1000],
    ["gecko203v", _("Gecko 203v"), 1000, 2000, 200 , 200],
    ["gecko210", _("Gecko 210"),  500, 4000, 20000, 1000],
    ["gecko212", _("Gecko 212"),  500, 4000, 20000, 1000],
    ["gecko320", _("Gecko 320"),  3500, 500, 200, 200],
    ["gecko540", _("Gecko 540"),  1000, 2000, 200, 200],
    ["l297", _("L297"), 500,  4000, 4000, 1000],
    ["pmdx150", _("PMDX-150"), 1000, 2000, 1000, 1000],
    ["sherline", _("Sherline"), 22000, 22000, 100000, 100000],
    ["xylotex", _("Xylotex 8S-3"), 2000, 1000, 200, 200],
    ["oem750", _("Parker-Compumotor oem750"), 1000, 1000, 1000, 200000],
    ["jvlsmd41", _("JVL-SMD41 or 42"), 500, 500, 2500, 2500],
    ["hobbycnc", _("Hobbycnc Pro Chopper"), 2000, 2000, 2000, 2000],
    ["keling", _("Keling 4030"), 5000, 5000, 20000, 20000],
]

def iceil(x):
    if isinstance(x, (int, long)): return x
    if isinstance(x, basestring): x = float(x)
    return int(math.ceil(x))

datadir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "emc")
wizard = os.path.join(datadir, "emc2-wizard.gif")
if not os.path.isfile(wizard):
    wizard = os.path.join("/etc/emc2/emc2-wizard.gif")
if not os.path.isfile(wizard):
    emc2icon = os.path.join("/usr/share/emc/emc2-wizard.gif")
if not os.path.isfile(wizard):
    wizdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
    wizard = os.path.join(wizdir, "emc2-wizard.gif")

icondir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
emc2icon = os.path.join(icondir, "emc2icon.png")
if not os.path.isfile(emc2icon):
    emc2icon = os.path.join("/etc/emc2/emc2-wizard.gif")
if not os.path.isfile(emc2icon):
    emc2icon = os.path.join("/usr/share/emc/emc2icon.png")

distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "configs", "common")
if not os.path.isdir(distdir):
    distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "doc", "emc2", "sample-configs", "common")
if not os.path.isdir(distdir):
    distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "emc2", "sample-configs", "common")
if not os.path.isdir(distdir):
    distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "doc", "emc2", "examples", "sample-configs", "common")
if not os.path.isdir(distdir):
    distdir = "/usr/share/doc/emc2/examples/sample-configs/common"

(XSTEP, XDIR, YSTEP, YDIR,
ZSTEP, ZDIR, ASTEP, ADIR,
ON, CW, CCW, PWM, BRAKE,
MIST, FLOOD, ESTOP, AMP,
PUMP, DOUT0, DOUT1, DOUT2, DOUT3,
UNUSED_OUTPUT) = hal_output_names = [
"xstep", "xdir", "ystep", "ydir",
"zstep", "zdir", "astep", "adir",
"spindle-on", "spindle-cw", "spindle-ccw", "spindle-pwm", "spindle-brake",
"coolant-mist", "coolant-flood", "estop-out", "xenable",
"charge-pump", "dout-00", "dout-01", "dout-02", "dout-03",
"unused-output"]

(ESTOP_IN, PROBE, PPR, PHA, PHB,
HOME_X, HOME_Y, HOME_Z, HOME_A,
MIN_HOME_X, MIN_HOME_Y, MIN_HOME_Z, MIN_HOME_A,
MAX_HOME_X, MAX_HOME_Y, MAX_HOME_Z, MAX_HOME_A,
BOTH_HOME_X, BOTH_HOME_Y, BOTH_HOME_Z, BOTH_HOME_A,
MIN_X, MIN_Y, MIN_Z, MIN_A,
MAX_X, MAX_Y, MAX_Z, MAX_A,
BOTH_X, BOTH_Y, BOTH_Z, BOTH_A,
ALL_LIMIT, ALL_HOME, DIN0, DIN1, DIN2, DIN3,
UNUSED_INPUT) = hal_input_names = [
"estop-ext", "probe-in", "spindle-index", "spindle-phase-a", "spindle-phase-b",
"home-x", "home-y", "home-z", "home-a",
"min-home-x", "min-home-y", "min-home-z", "min-home-a",
"max-home-x", "max-home-y", "max-home-z", "max-home-a",
"both-home-x", "both-home-y", "both-home-z", "both-home-a",
"min-x", "min-y", "min-z", "min-a",
"max-x", "max-y", "max-z", "max-a",
"both-x", "both-y", "both-z", "both-a",
"all-limit", "all-home", "din-00", "din-01", "din-02", "din-03",
"unused-input"]

human_output_names = (_("X Step"), _("X Direction"), _("Y Step"), _("Y Direction"),
_("Z Step"), _("Z Direction"), _("A Step"), _("A Direction"),
_("Spindle ON"),_("Spindle CW"), _("Spindle CCW"), _("Spindle PWM"), _("Spindle Brake"),
_("Coolant Mist"), _("Coolant Flood"), _("ESTOP Out"), _("Amplifier Enable"),
_("Charge Pump"),
_("Digital out 0"), _("Digital out 1"), _("Digital out 2"), _("Digital out 3"),
_("Unused"))

human_input_names = (_("ESTOP In"), _("Probe In"),
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
_("Digital in 0"), _("Digital in 1"), _("Digital in 2"), _("Digital in 3"),
_("Unused"))

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
        self.machinename = _("my-mill")
        self.axes = 0 # XYZ
        self.units = 0 # inch
        self.drivertype = "other"
        self.steptime = 5000
        self.stepspace = 5000
        self.dirhold = 20000 
        self.dirsetup = 20000
        self.latency = 15000
        self.period = 25000

        self.ioaddr = "0x378"
        self.ioaddr2 = _("Enter Address")
        self.pp2_direction = 0 # input
        self.ioaddr3 = _("Enter Address")
        self.pp3_direction = 0 # input
        self.number_pports = 1

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

        self.pin1inv = 0
        self.pin2inv = 0
        self.pin3inv = 0
        self.pin4inv = 0
        self.pin5inv = 0
        self.pin6inv = 0
        self.pin7inv = 0
        self.pin8inv = 0
        self.pin9inv = 0
        self.pin10inv = 0
        self.pin11inv = 0
        self.pin12inv = 0
        self.pin13inv = 0
        self.pin14inv = 0
        self.pin15inv = 0
        self.pin16inv = 0
        self.pin17inv = 0

        self.pin1 = ESTOP
        self.pin2 = XSTEP
        self.pin3 = XDIR
        self.pin4 = YSTEP
        self.pin5 = YDIR
        self.pin6 = ZSTEP
        self.pin7 = ZDIR
        self.pin8 = ASTEP
        self.pin9 = ADIR
        self.pin14 = CW
        self.pin16 = PWM
        self.pin17 = AMP

        self.pin10 = UNUSED_INPUT
        self.pin11 = UNUSED_INPUT
        self.pin12 = UNUSED_INPUT
        self.pin13 = UNUSED_INPUT
        self.pin15 = UNUSED_INPUT

        self.xsteprev = 200
        self.xmicrostep = 2
        self.xpulleynum = 1
        self.xpulleyden = 1
        self.xleadscrew = 20
        self.xmaxvel = 1
        self.xmaxacc = 30

        self.xhomepos = 0
        self.xminlim =  0
        self.xmaxlim =  8
        self.xhomesw =  0
        self.xhomevel = .05
        self.xlatchdir = 0
        self.xscale = 0

        self.ysteprev = 200
        self.ymicrostep = 2
        self.ypulleynum = 1
        self.ypulleyden = 1
        self.yleadscrew = 20
        self.ymaxvel = 1
        self.ymaxacc = 30

        self.yhomepos = 0
        self.yminlim =  0
        self.ymaxlim =  8
        self.yhomesw =  0
        self.yhomevel = .05
        self.ylatchdir = 0
        self.yscale = 0


        self.zsteprev = 200
        self.zmicrostep = 2
        self.zpulleynum = 1
        self.zpulleyden = 1
        self.zleadscrew = 20
        self.zmaxvel = 1
        self.zmaxacc = 30

        self.zhomepos = 0
        self.zminlim = -4
        self.zmaxlim =  0
        self.zhomesw = 0
        self.zhomevel = .05
        self.zlatchdir = 0
        self.zscale = 0


        self.asteprev = 200
        self.amicrostep = 2
        self.apulleynum = 1
        self.apulleyden = 1
        self.aleadscrew = 8
        self.amaxvel = 360
        self.amaxacc = 1200

        self.ahomepos = 0
        self.aminlim = -9999
        self.amaxlim =  9999
        self.ahomesw =  0
        self.ahomevel = .05
        self.alatchdir = 0
        self.ascale = 0

        self.spindlecarrier = 100
        self.spindlecpr = 100
        self.spindlespeed1 = 100
        self.spindlespeed2 = 800
        self.spindlepwm1 = .2
        self.spindlepwm2 = .8

        self.digitsin = 15
        self.digitsout = 15
        self.s32in = 10
        self.s32out = 10
        self.floatsin = 10
        self.floatsout = 10
        self.halui = 0
        self.createsymlink = 1
        self.createshortcut = 1

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

        warnings = []
        for f, m in self.md5sums:
            m1 = md5sum(f)
            if m1 and m != m1:
                warnings.append(_("File %r was modified since it was written by stepconf") % f)
        if warnings:
            warnings.append("")
            warnings.append(_("Saving this configuration file will discard configuration changes made outside stepconf."))
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

        legacy_hal_output_names = ["xstep", "xdir", "ystep", "ydir",
        "zstep", "zdir", "astep", "adir",
        "spindle-on", "spindle-cw", "spindle-ccw", "spindle-pwm",
        "coolant-mist", "coolant-flood", "estop-out", "xenable",
        "charge-pump", "unused-output"]

        legacy_hal_input_names = ["estop-ext", "probe-in",
        "spindle-index", "spindle-phase-a", "spindle-phase-b",
        "home-x", "home-y", "home-z", "home-a",
        "min-home-x", "min-home-y", "min-home-z", "min-home-a",
        "max-home-x", "max-home-y", "max-home-z", "max-home-a",
        "both-home-x", "both-home-y", "both-home-z", "both-home-a",
        "min-x", "min-y", "min-z", "min-a",
        "max-x", "max-y", "max-z", "max-a",
        "both-x", "both-y", "both-z", "both-a",
        "all-limit", "all-home", "unused-input"]

        for p in (10,11,12,13,15):
            pin = "pin%d" % p
            p = self[pin]
            if isinstance(p, int):
                self[pin] = legacy_hal_input_names[p]

        for p in (1,2,3,4,5,6,7,8,9,14,16,17):
            pin = "pin%d" % p
            p = self[pin]
            if isinstance(p, int):
                self[pin] = legacy_hal_output_names[p]

        legacy_driver_type = [  # Must exactly match texts in drivertypes
            "gecko201",
            "l297",
            "pmdx150",
            "sherline",
            "xylotex",
            "oem750",
        ]

        if isinstance(self.drivertype, int):
            if self.drivertype < len(legacy_driver_type):
                self.drivertype = legacy_driver_type[self.drivertype]
            else:
                self.drivertype = "other"

    def add_md5sum(self, filename, mode="r"):
        self.md5sums.append((filename, md5sum(filename)))

    def write_inifile(self, base):
        filename = os.path.join(base, self.machinename + ".ini")
        file = open(filename, "w")
        print >>file, _("# Generated by stepconf at %s") % time.asctime()
        print >>file, _("# If you make changes to this file, they will be")
        print >>file, _("# overwritten when you run stepconf again")

        print >>file
        print >>file, "[EMC]"
        print >>file, "MACHINE = %s" % self.machinename
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
        print >>file, "[FILTER]"
        print >>file, "PROGRAM_EXTENSION = .png,.gif,.jpg Greyscale Depth Image"
        print >>file, "PROGRAM_EXTENSION = .py Python Script"
        print >>file, "png = image-to-gcode"
        print >>file, "gif = image-to-gcode"
        print >>file, "jpg = image-to-gcode"
        print >>file, "py = python"        

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
           print >>file, _("# add halui MDI commands here (max 64) ")

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
        if self.doublestep():
            base_period = 1e9 / pps
        else:
            base_period = .5e9 / pps
        if base_period > 100000: base_period = 100000
        if base_period < self.minperiod(): base_period = self.minperiod()
        return int(base_period)

    def ideal_maxvel(self, scale):
        if self.doublestep():
            return abs(.95 * 1e9 / self.ideal_period() / scale)
        else:
            return abs(.95 * .5 * 1e9 / self.ideal_period() / scale)

    def write_one_axis(self, file, num, letter, type, all_homes):
        order = "1203"
        def get(s): return self[letter + s]
        scale = get("scale")
        vel = min(get("maxvel"), self.ideal_maxvel(scale))
        print >>file
        print >>file, "[AXIS_%d]" % num
        print >>file, "TYPE = %s" % type
        print >>file, "HOME = %s" % get("homepos")
        print >>file, "MAX_VELOCITY = %s" % vel
        print >>file, "MAX_ACCELERATION = %s" % get("maxacc")
        print >>file, "STEPGEN_MAXACCEL = %s" % (1.25 * get("maxacc"))
        print >>file, "SCALE = %s" % scale
        if num == 3:
            print >>file, "FERROR = 1"
            print >>file, "MIN_FERROR = .25"
        elif self.units:
            print >>file, "FERROR = 1"
            print >>file, "MIN_FERROR = .25"
        else:
            print >>file, "FERROR = 0.05"
            print >>file, "MIN_FERROR = 0.01"

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

        inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
        thisaxishome = set((ALL_HOME, "home-" + letter, "min-home-" + letter,
                            "max-home-" + letter, "both-home-" + letter))
        ignore = set(("min-home-" + letter, "max-home-" + letter,
                            "both-home-" + letter))
        homes = bool(inputs & thisaxishome)
    
        if homes:
            print >>file, "HOME_OFFSET = %f" % get("homesw")
            print >>file, "HOME_SEARCH_VEL = %f" % get("homevel")
            latchvel = get("homevel") / abs(get("homevel"))
            if get("latchdir"): latchvel = -latchvel
            # set latch velocity to one step every two servo periods
            # to ensure that we can capture the position to within one step
            latchvel = latchvel * 500 / get("scale")
            # don't do the latch move faster than the search move
            if abs(latchvel) > abs(get("homevel")):
                latchvel = latchvel * (abs(get("homevel"))/abs(latchvel))
            print >>file, "HOME_LATCH_VEL = %f" % latchvel
            if inputs & ignore:
                print >>file, "HOME_IGNORE_LIMITS = YES"
            if all_homes:
                print >>file, "HOME_SEQUENCE = %s" % order[num]
        else:
            print >>file, "HOME_OFFSET = %s" % get("homepos")

    def home_sig(self, axis):
        inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
        thisaxishome = set((ALL_HOME, "home-" + axis, "min-home-" + axis,
                            "max-home-" + axis, "both-home-" + axis))
        for i in inputs:
            if i in thisaxishome: return i

    def min_lim_sig(self, axis):
           inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
           thisaxisminlimits = set((ALL_LIMIT, "min-" + axis, "min-home-" + axis,
                               "both-" + axis, "both-home-" + axis))
           for i in inputs:
               if i in thisaxisminlimits: return i

    def max_lim_sig(self, axis):
           inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
           thisaxismaxlimits = set((ALL_LIMIT, "max-" + axis, "max-home-" + axis,
                               "both-" + axis, "both-home-" + axis))
           for i in inputs:
               if i in thisaxismaxlimits: return i
 
    def connect_axis(self, file, num, let):
        axnum = "xyza".index(let)
        lat = self.latency
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

    def connect_input(self, file, num):
        p = self['pin%d' % num]
        i = self['pin%dinv' % num]
        if p == UNUSED_INPUT: return

        if i:
            print >>file, "net %s <= parport.0.pin-%02d-in-not" \
                % (p, num)
        else:
            print >>file, "net %s <= parport.0.pin-%02d-in" \
                % (p, num)

    def find_input(self, input):
        inputs = set((10, 11, 12, 13, 15))
        for i in inputs:
            pin = getattr(self, "pin%d" % i)
            inv = getattr(self, "pin%dinv" % i)
            if pin == input: return i
        return None

    def find_output(self, output):
        outputs = set((1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 16, 17))
        for i in outputs:
            pin = getattr(self, "pin%d" % i)
            inv = getattr(self, "pin%dinv" % i)
            if pin == output: return i
        return None

    def connect_output(self, file, num):
        p = self['pin%d' % num]
        i = self['pin%dinv' % num]
        if p == UNUSED_OUTPUT: return
        if i: print >>file, "setp parport.0.pin-%02d-out-invert 1" % num
        print >>file, "net %s => parport.0.pin-%02d-out" % (p, num)
        if self.doublestep():
            if p in (XSTEP, YSTEP, ZSTEP, ASTEP):
                print >>file, "setp parport.0.pin-%02d-out-reset 1" % num

    def write_halfile(self, base):
        inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
        outputs = set((self.pin1, self.pin2, self.pin3, self.pin4, self.pin5,
            self.pin6, self.pin7, self.pin8, self.pin9, self.pin14, self.pin16,
            self.pin17))

        filename = os.path.join(base, self.machinename + ".hal")
        file = open(filename, "w")
        print >>file, _("# Generated by stepconf at %s") % time.asctime()
        print >>file, _("# If you make changes to this file, they will be")
        print >>file, _("# overwritten when you run stepconf again")

        print >>file, "loadrt trivkins"
        print >>file, "loadrt [EMCMOT]EMCMOT base_period_nsec=[EMCMOT]BASE_PERIOD servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[TRAJ]AXES"
        print >>file, "loadrt probe_parport"
        port3name=port2name=port2dir=port3dir=""
        if self.number_pports>2:
             port3name = self.ioaddr3
             if self.pp3_direction:
                port3dir =" in"
             else: 
                port3dir =" out"
        if self.number_pports>1:
             port2name = self.ioaddr2
             if self.pp2_direction:
                port2dir =" in"
             else: 
                port2dir =" out"
        print >>file, "loadrt hal_parport cfg=\"%s out %s%s %s%s\"" % (self.ioaddr, port2name, port2dir, port3name, port3dir)
        if self.doublestep():
            print >>file, "setp parport.0.reset-time %d" % self.steptime
        encoder = PHA in inputs
        counter = PHB not in inputs
        probe = PROBE in inputs
        pwm = PWM in outputs
        pump = PUMP in outputs

        if self.axes == 2:
            print >>file, "loadrt stepgen step_type=0,0"
        elif self.axes == 1:
            print >>file, "loadrt stepgen step_type=0,0,0,0"
        else:
            print >>file, "loadrt stepgen step_type=0,0,0"

        if encoder:
            print >>file, "loadrt encoder num_chan=1"
        if self.pyvcphaltype == 1 and self.pyvcpconnect == 1:
            print >>file, "loadrt abs count=1"
            if encoder:
               print >>file, "loadrt scale count=1"
               print >>file, "loadrt lowpass count=1"
        if pump:
            print >>file, "loadrt charge_pump"
            print >>file, "net estop-out charge-pump.enable iocontrol.0.user-enable-out"
            print >>file, "net charge-pump <= charge-pump.out"

        if pwm:
            print >>file, "loadrt pwmgen output_type=0"

        if self.classicladder:
            print >>file, "loadrt classicladder_rt numPhysInputs=%d numPhysOutputs=%d numS32in=%d numS32out=%d numFloatIn=%d numFloatOut=%d" %(self.digitsin , self.digitsout , self.s32in, self.s32out, self.floatsin, self.floatsout)

        print >>file
        print >>file, "addf parport.0.read base-thread"
        if self.number_pports > 1:
            print >>file, "addf parport.1.read base-thread"
        if self.number_pports > 2:
            print >>file, "addf parport.2.read base-thread"

        print >>file, "addf stepgen.make-pulses base-thread"
        if encoder: print >>file, "addf encoder.update-counters base-thread"
        if pump: print >>file, "addf charge-pump base-thread"
        if pwm: print >>file, "addf pwmgen.make-pulses base-thread"
        print >>file, "addf parport.0.write base-thread"
        if self.doublestep():
            print >>file, "addf parport.0.reset base-thread"
        if self.number_pports > 1:
            print >>file, "addf parport.1.write base-thread"
        if self.number_pports > 2:
            print >>file, "addf parport.2.write base-thread"
        print >>file
        print >>file, "addf stepgen.capture-position servo-thread"
        if encoder: print >>file, "addf encoder.capture-position servo-thread"
        print >>file, "addf motion-command-handler servo-thread"
        print >>file, "addf motion-controller servo-thread"
        if self.classicladder:
            print >>file,"addf classicladder.0.refresh servo-thread"
        print >>file, "addf stepgen.update-freq servo-thread"
        if pwm: print >>file, "addf pwmgen.update servo-thread"
        if self.pyvcphaltype == 1 and self.pyvcpconnect == 1:
            print >>file, "addf abs.0 servo-thread"
            if encoder:
               print >>file, "addf scale.0 servo-thread"
               print >>file, "addf lowpass.0 servo-thread"
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
            if dout in outputs:
                print >>file, "net %s <= motion.digital-out-%02d" % (dout, i)

        for i in range(4):
            din = "din-%02d" % i
            if din in inputs:
                print >>file, "net %s => motion.digital-in-%02d" % (din, i)

        print >>file
        for o in (1,2,3,4,5,6,7,8,9,14,16,17): self.connect_output(file, o)      
        print >>file
            
        print >>file
        for i in (10,11,12,13,15): self.connect_input(file, i)
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
            print >>file, _("# **** Setup for external estop ladder program -START ****")
            print >>file
            print >>file, "net estop-out => classicladder.0.in-00"
            print >>file, "net estop-ext => classicladder.0.in-01"
            print >>file, "net estop-strobe classicladder.0.in-02 <= iocontrol.0.user-request-enable"
            print >>file, "net estop-outcl classicladder.0.out-00 => iocontrol.0.emc-enable-in"
            print >>file
            print >>file, _("# **** Setup for external estop ladder program -END ****")
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
                print >>file, _("# Load Classicladder with modbus master included (GUI must run for Modbus)")
                print >>file, "loadusr classicladder --modmaster custom.clp"
            else:
                print >>file, _("# Load Classicladder without GUI (can reload LADDER GUI in AXIS GUI")
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
                  print >>f1, _("# **** Setup of spindle speed display using pyvcp -START ****")
                  if encoder:
                      print >>f1, _("# **** Use ACTUAL spindle velocity from spindle encoder")
                      print >>f1, _("# **** spindle-velocity bounces around so we filter it with lowpass")
                      print >>f1, _("# **** spindle-velocity is signed so we use absolute component to remove sign") 
                      print >>f1, _("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
                      print >>f1
                      print >>f1, ("setp scale.0.gain 60")
                      print >>f1, ("setp lowpass.0.gain 0.01")
                      print >>f1, ("net spindle-velocity => lowpass.0.in")
                      print >>f1, ("net spindle-rps-filtered <= lowpass.0.out")
                      print >>f1, ("net spindle-rps-filtered => abs.0.in")
                      print >>f1, ("net absolute-spindle-vel <= abs.0.out => scale.0.in")
                      print >>f1, ("net scaled-spindle-vel <= scale.0.out => pyvcp.spindle-speed")
                  else:
                      print >>f1, _("# **** Use COMMANDED spindle velocity from EMC because no spindle encoder was specified")
                      print >>f1, _("# **** COMANDED velocity is signed so we use absolute component (abs.0) to remove sign")
                      print >>f1
                      print >>f1, ("net spindle-cmd => abs.0.in")
                      print >>f1, ("net absolute-spindle-vel <= abs.0.out => pyvcp.spindle-speed")                     

        if self.customhal or self.classicladder or self.halui:
            custom = os.path.join(base, "custom.hal")
            if not os.path.exists(custom):
                f1 = open(custom, "w")
                print >>f1, _("# Include your customized HAL commands here")
                print >>f1, _("# This file will not be overwritten when you run stepconf again") 
        file.close()
        self.add_md5sum(filename)

    def write_readme(self, base):
        filename = os.path.join(base, "README")
        file = open(filename, "w")
        print >>file, _("Generated by stepconf at %s") % time.asctime()
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
        self.copy(base, "tool.tbl")

        filename = "%s.stepconf" % base

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

        # see http://freedesktop.org/wiki/Software/xdg-user-dirs
        desktop = commands.getoutput("""
            test -f ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs && . ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs
            echo ${XDG_DESKTOP_DIR:-$HOME/Desktop}""")
        if self.createsymlink:
            shortcut = os.path.join(desktop, self.machinename)
            if os.path.exists(desktop) and not os.path.exists(shortcut):
                os.symlink(base,shortcut)

        if self.createshortcut and os.path.exists(desktop):
            if os.path.exists(BASE + "/scripts/emc"):
                scriptspath = (BASE + "/scripts/emc")
            else:
                scriptspath ="emc"

            filename = os.path.join(desktop, "%s.desktop" % self.machinename)
            file = open(filename, "w")
            print >>file,"[Desktop Entry]"
            print >>file,"Version=1.0"
            print >>file,"Terminal=false"
            print >>file,"Name=" + _("launch %s") % self.machinename
            print >>file,"Exec=%s %s/%s.ini" \
                         % ( scriptspath, base, self.machinename )
            print >>file,"Type=Application"
            print >>file,"Comment=" + _("Desktop Launcher for EMC config made by Stepconf")
            print >>file,"Icon=%s"% emc2icon
            file.close()
            # Ubuntu 10.04 require launcher to have execute permissions
            os.chmod(filename,0775)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class App:
    fname = 'stepconf.glade'  # XXX search path

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

    def __init__(self):
        gnome.init("stepconf", "0.6") 
        glade = xml.dom.minidom.parse(os.path.join(datadir, self.fname))
        self.make_axispage(glade, 'y')
        self.make_axispage(glade, 'z')
        self.make_axispage(glade, 'a')
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

        for i in drivertypes:
            self.widgets.drivertype.append_text(i[1])
        self.widgets.drivertype.append_text(_("Other"))
        self.data = Data()
   
        tempfile = os.path.join(distdir, "configurable_options/ladder/TEMP.clp")
        if os.path.exists(tempfile):
           os.remove(tempfile)

    def gtk_main_quit(self, *args):
        gtk.main_quit()

    def on_window1_delete_event(self, *args):
        if self.warning_dialog (_("Quit Stepconf and discard changes?"),False):
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
            filter.add_pattern("*.stepconf")
            filter.set_name(_("EMC2 'stepconf' configuration files"))
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

    def drivertype_fromid(self):
        for d in drivertypes:
            if d[0] == self.data.drivertype: return d[1]

    def drivertype_toid(self, what=None):
        if not isinstance(what, int): what = self.drivertype_toindex(what)
        if what < len(drivertypes): return drivertypes[what][0]
        return "other"

    def drivertype_toindex(self, what=None):
        if what is None: what = self.data.drivertype
        for i, d in enumerate(drivertypes):
            if d[0] == what: return i
        return len(drivertypes)

    def drivertype_fromindex(self):
        i = self.widgets.drivertype.get_active()
        if i < len(drivertypes): return drivertypes[i][1]
        return _("Other")

    def on_basicinfo_prepare(self, *args):
        self.widgets.drivetime_expander.set_expanded(True)
        self.widgets.parport_expander.set_expanded(True)
        self.widgets.machinename.set_text(self.data.machinename)
        self.widgets.axes.set_active(self.data.axes)
        self.widgets.units.set_active(self.data.units)
        self.widgets.latency.set_value(self.data.latency)
        self.widgets.steptime.set_value(self.data.steptime)
        self.widgets.stepspace.set_value(self.data.stepspace)
        self.widgets.dirsetup.set_value(self.data.dirsetup)
        self.widgets.dirhold.set_value(self.data.dirhold)
        self.widgets.drivertype.set_active(self.drivertype_toindex())
        self.widgets.manualtoolchange.set_active(self.data.manualtoolchange)
        self.widgets.ioaddr.set_text(self.data.ioaddr)
        self.widgets.machinename.grab_focus()
        self.widgets.ioaddr2.set_text(self.data.ioaddr2) 
        self.widgets.ioaddr3.set_text(self.data.ioaddr3)
        self.widgets.pp2_direction.set_active(self.data.pp2_direction)
        self.widgets.pp3_direction.set_active(self.data.pp3_direction)
        if self.data.number_pports>2:
             self.widgets.pp2_checkbutton.set_active(1)
             self.widgets.pp3_checkbutton.set_active(1)
        elif self.data.number_pports>1:
             self.widgets.pp2_checkbutton.set_active(1)
        
        

    def on_basicinfo_next(self, *args):
        self.widgets.drivetime_expander.set_expanded(False)
        self.widgets.parport_expander.set_expanded(False)
        machinename = self.widgets.machinename.get_text()
        self.data.machinename = machinename.replace(" ","_")
        self.data.axes = self.widgets.axes.get_active()
        self.data.units = self.widgets.units.get_active()
        self.data.drivertype = self.drivertype_toid(self.widgets.drivertype.get_active())
        self.data.steptime = self.widgets.steptime.get_value()
        self.data.stepspace = self.widgets.stepspace.get_value()
        self.data.dirsetup = self.widgets.dirsetup.get_value()
        self.data.dirhold = self.widgets.dirhold.get_value()
        self.data.latency = self.widgets.latency.get_value()
        self.data.manualtoolchange = self.widgets.manualtoolchange.get_active()
        self.data.ioaddr = self.widgets.ioaddr.get_text()
        self.data.ioaddr2 = self.widgets.ioaddr2.get_text()
        self.data.ioaddr3 = self.widgets.ioaddr3.get_text()
        self.data.pp2_direction = self.widgets.pp2_direction.get_active()
        self.data.pp3_direction = self.widgets.pp3_direction.get_active()
        if self.widgets.pp3_checkbutton.get_active() and self.widgets.pp2_checkbutton.get_active():
            self.data.number_pports = 3
        elif self.widgets.pp2_checkbutton.get_active():
            self.data.number_pports = 2
        else:
            self.data.number_pports = 1

    def on_machinename_changed(self, *args):
        temp = self.widgets.machinename.get_text()
        self.widgets.confdir.set_text("~/emc2/configs/%s" % temp.replace(" ","_"))

    def on_drivertype_changed(self, *args):
        v = self.widgets.drivertype.get_active()
        if v < len(drivertypes):
            d = drivertypes[v]
            self.widgets.steptime.set_value(d[2])
            self.widgets.stepspace.set_value(d[3])
            self.widgets.dirhold.set_value(d[4])
            self.widgets.dirsetup.set_value(d[5])

            self.widgets.steptime.set_sensitive(0)
            self.widgets.stepspace.set_sensitive(0)
            self.widgets.dirhold.set_sensitive(0)
            self.widgets.dirsetup.set_sensitive(0)
        else:
            self.widgets.steptime.set_sensitive(1)
            self.widgets.stepspace.set_sensitive(1)
            self.widgets.dirhold.set_sensitive(1)
            self.widgets.dirsetup.set_sensitive(1)
        self.on_calculate_ideal_period()

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

        p = 'pin%d' % pin
        v = self.widgets[p].get_active()
        ex = exclusive.get(hal_input_names[v], ())

        for pin1 in (10,11,12,13,15):
            if pin1 == pin: continue
            p = 'pin%d' % pin1
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
           
    def on_pport_prepare(self, *args):
        self.in_pport_prepare = True
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pin%d' % pin
            model = self.widgets[p].get_model()
            model.clear()        
            for name in human_output_names: model.append((name,))
            self.widgets[p].set_wrap_width(3)
            self.widgets[p].set_active(hal_output_names.index(self.data[p]))
            p = 'pin%dinv' % pin
            self.widgets[p].set_active(self.data[p])
        for pin in (10,11,12,13,15):
            p = 'pin%d' % pin
            model = self.widgets[p].get_model()
            model.clear()
            for name in human_input_names: model.append((name,))
            self.widgets[p].set_wrap_width(3)
            self.widgets[p].set_active(hal_input_names.index(self.data[p]))
            p = 'pin%dinv' % pin
            self.widgets[p].set_active(self.data[p])
        self.widgets.pin1.grab_focus()
        self.in_pport_prepare = False

    def on_pp2_checkbutton_toggled(self, *args): 
        i = self.widgets.pp2_checkbutton.get_active()   
        self.widgets.pp2_direction.set_sensitive(i)
        self.widgets.ioaddr2.set_sensitive(i)
        if i == 0:
           self.widgets.pp3_checkbutton.set_active(i)
           self.widgets.ioaddr3.set_sensitive(i)

    def on_pp3_checkbutton_toggled(self, *args): 
        i = self.widgets.pp3_checkbutton.get_active() 
        if self.widgets.pp2_checkbutton.get_active() ==0:
          i=0  
          self.widgets.pp3_checkbutton.set_active(0)
        self.widgets.pp3_direction.set_sensitive(i)
        self.widgets.ioaddr3.set_sensitive(i)
        
 
    def on_pport_next(self, *args):
        for pin in (10,11,12,13,15):
            p = 'pin%d' % pin
            self.data[p] = hal_input_names[self.widgets[p].get_active()]
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pin%d' % pin
            self.data[p] = hal_output_names[self.widgets[p].get_active()]
        for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
            p = 'pin%dinv' % pin
            self.data[p] = self.widgets[p].get_active()
        self.data.pp2_direction = self.widgets.pp2_direction.get_active()
        self.data.pp3_direction = self.widgets.pp3_direction.get_active()
    on_pport_back = on_pport_next

    def on_sherlinedefault_clicked(self, *args):
        self.widgets.pin2.set_active(1)
        self.widgets.pin3.set_active(0)
        self.widgets.pin4.set_active(3)
        self.widgets.pin5.set_active(2)
        self.widgets.pin6.set_active(5)
        self.widgets.pin7.set_active(4)
        self.widgets.pin8.set_active(7)
        self.widgets.pin9.set_active(6)

    def on_xylotexdefault_clicked(self, *args):
        self.widgets.pin2.set_active(0)
        self.widgets.pin3.set_active(1)
        self.widgets.pin4.set_active(2)
        self.widgets.pin5.set_active(3)
        self.widgets.pin6.set_active(4)
        self.widgets.pin7.set_active(5)
        self.widgets.pin8.set_active(6)
        self.widgets.pin9.set_active(7)

    def axis_prepare(self, axis):
        d = self.data
        w = self.widgets
        def set_text(n): w[axis + n].set_text("%s" % d[axis + n])
        def set_active(n): w[axis + n].set_active(d[axis + n])
        set_text("steprev")
        set_text("microstep")
        set_text("pulleynum")
        set_text("pulleyden")
        set_text("leadscrew")
        set_text("maxvel")
        set_text("maxacc")
        set_text("homepos")
        set_text("minlim")
        set_text("maxlim")
        set_text("homesw")
        set_text("homevel")
        set_active("latchdir")

        if axis == "a":
            w[axis + "screwunits"].set_text(_("degree / rev"))
            w[axis + "velunits"].set_text(_("deg / s"))
            w[axis + "accunits"].set_text(_("deg / s²"))
            w[axis + "accdistunits"].set_text(_("deg"))
            w[axis + "scaleunits"].set_text(_("Steps / deg"))
        elif d.units:
            w[axis + "screwunits"].set_text(_("mm / rev"))
            w[axis + "velunits"].set_text(_("mm / s"))
            w[axis + "accunits"].set_text(_("mm / s²"))
            w[axis + "accdistunits"].set_text(_("mm"))
            w[axis + "scaleunits"].set_text(_("Steps / mm"))
        else:
            w[axis + "screwunits"].set_text(_("rev / in"))
            w[axis + "velunits"].set_text(_("in / s"))
            w[axis + "accunits"].set_text(_("in / s²"))
            w[axis + "accdistunits"].set_text(_("in"))
            w[axis + "scaleunits"].set_text(_("Steps / in"))

        inputs = set((d.pin10, d.pin11, d.pin12, d.pin13, d.pin15))
        thisaxishome = set((ALL_HOME, "home-" + axis, "min-home-" + axis,
                            "max-home-" + axis, "both-home-" + axis))
        homes = bool(inputs & thisaxishome)
        w[axis + "homesw"].set_sensitive(homes)
        w[axis + "homevel"].set_sensitive(homes)
        w[axis + "latchdir"].set_sensitive(homes)

        w[axis + "steprev"].grab_focus()
        gobject.idle_add(lambda: self.update_pps(axis))

    def axis_done(self, axis):
        d = self.data
        w = self.widgets
        def get_text(n): d[axis + n] = float(w[axis + n].get_text())
        def get_active(n): d[axis + n] = w[axis + n].get_active()
        get_text("steprev")
        get_text("microstep")
        get_text("pulleynum")
        get_text("pulleyden")
        get_text("leadscrew")
        get_text("maxvel")
        get_text("maxacc")
        get_text("homepos")
        get_text("minlim")
        get_text("maxlim")
        get_text("homesw")
        get_text("homevel")
        get_active("latchdir")
        
    def on_xaxis_prepare(self, *args): self.axis_prepare('x')
    def on_yaxis_prepare(self, *args): self.axis_prepare('y')
    def on_zaxis_prepare(self, *args): self.axis_prepare('z')
    def on_aaxis_prepare(self, *args): self.axis_prepare('a')

    def on_xaxis_back(self, *args): self.axis_done('x')
    def on_yaxis_back(self, *args): self.axis_done('y')
    def on_zaxis_back(self, *args):
        self.axis_done('z')
        if self.data.axes == 2:
            self.widgets.druid1.set_page(self.widgets.xaxis)
            return True
    def on_aaxis_back(self, *args): self.axis_done('a')

    def on_xaxistest_clicked(self, *args): self.test_axis('x')
    def on_yaxistest_clicked(self, *args): self.test_axis('y')
    def on_zaxistest_clicked(self, *args): self.test_axis('z')
    def on_aaxistest_clicked(self, *args): self.test_axis('a')

    def on_spindle_prepare(self, *args):
        self.widgets['spindlecarrier'].set_text("%s" % self.data.spindlecarrier)
        self.widgets['spindlespeed1'].set_text("%s" % self.data.spindlespeed1)
        self.widgets['spindlespeed2'].set_text("%s" % self.data.spindlespeed2)
        self.widgets['spindlepwm1'].set_text("%s" % self.data.spindlepwm1)
        self.widgets['spindlepwm2'].set_text("%s" % self.data.spindlepwm2)
        self.widgets['spindlecpr'].set_text("%s" % self.data.spindlecpr)

        data = self.data
        if PHA in (data.pin10, data.pin11, data.pin12, data.pin13, data.pin15):
            self.widgets.spindlecpr.set_sensitive(1)
        else:
            self.widgets.spindlecpr.set_sensitive(0)

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
                self.widgets.radiobutton8.set_active(True)
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
                self.widgets.radiobutton4.set_active(True)

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
           if self.widgets.radiobutton1.get_active() == True:
              if self.data.tempexists:
                   self.data.laddername='TEMP.clp'
              else:
                   self.data.laddername= 'blank.clp'
                   self.data.ladderhaltype = 0
           if self.widgets.radiobutton2.get_active() == True:
              self.data.laddername = 'estop.clp'
              inputs = set((self.data.pin10,self.data.pin11,self.data.pin12,self.data.pin13,self.data.pin15))
              if ESTOP_IN not in inputs:
                 self.warning_dialog(_("You need to designate an E-stop input pin in the Parallel Port Setup page for this program."),True)
                 self.widgets.druid1.set_page(self.widgets.advanced)
                 return True
              self.data.ladderhaltype = 1
           if self.widgets.radiobutton3.get_active() == True:
                 self.data.laddername = 'serialmodbus.clp'
                 self.data.modbus = 1
                 self.widgets.modbus.set_active(self.data.modbus) 
                 self.data.ladderhaltype = 0          
           if self.widgets.radiobutton4.get_active() == True:
              self.data.laddername='custom.clp'
           else:
               if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custom.clp" % self.data.machinename)):
                  if not self.warning_dialog(_("OK to replace existing custom ladder program?\nExisting Custom.clp will be renamed custom_backup.clp.\nAny existing file named -custom_backup.clp- will be lost. "),False):
                     self.widgets.druid1.set_page(self.widgets.advanced)
                     return True 
           if self.widgets.radiobutton1.get_active() == False:
              if os.path.exists(os.path.join(distdir, "configurable_options/ladder/TEMP.clp")):
                 if not self.warning_dialog(_("You edited a ladder program and have selected a different program to copy to your configuration file.\nThe edited program will be lost.\n\nAre you sure?  "),False):
                   self.widgets.druid1.set_page(self.widgets.advanced)
                   return True       
        if self.data.pyvcp == True:
           if self.widgets.radiobutton5.get_active() == True:
              self.data.pyvcpname = "blank.xml"
              self.pyvcphaltype = 0
           if self.widgets.radiobutton6.get_active() == True:
              self.data.pyvcpname = "spindle.xml"
              self.data.pyvcphaltype = 1
           if self.widgets.radiobutton8.get_active() == True:
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
        self.widgets.radiobutton1.set_sensitive(i)
        self.widgets.radiobutton2.set_sensitive(i)
        self.widgets.radiobutton3.set_sensitive(i)
        if  self.widgets.createconfig.get_active():
            self.widgets.radiobutton4.set_sensitive(False)
        else:
            self.widgets.radiobutton4.set_sensitive(i)
        self.widgets.loadladder.set_sensitive(i)
        self.widgets.label_digin.set_sensitive(i)
        self.widgets.label_digout.set_sensitive(i)
        self.widgets.label_s32in.set_sensitive(i)
        self.widgets.label_s32out.set_sensitive(i)
        self.widgets.label_floatin.set_sensitive(i)
        self.widgets.label_floatout.set_sensitive(i)
        self.widgets.ladderconnect.set_sensitive(i)
        self.widgets.clpins_expander.set_sensitive(i)

    def on_pyvcp_toggled(self,*args):
        i= self.widgets.pyvcp.get_active()
        self.widgets.radiobutton5.set_sensitive(i)
        self.widgets.radiobutton6.set_sensitive(i)
        if  self.widgets.createconfig.get_active():
            self.widgets.radiobutton8.set_sensitive(False)
        else:
            self.widgets.radiobutton8.set_sensitive(i)
        self.widgets.displaypanel.set_sensitive(i)
        self.widgets.pyvcpconnect.set_sensitive(i)

    def on_displaypanel_clicked(self,*args):
        print"clicked"
        self.testpanel(self)

    def on_complete_back(self, *args):
        self.widgets.druid1.set_page(self.widgets.advanced)
        return True

    def on_xaxis_next(self, *args):
        self.axis_done('x')
        if self.data.axes == 2:
            self.widgets.druid1.set_page(self.widgets.zaxis)
            return True

    def on_yaxis_next(self, *args): self.axis_done('y')

    def on_zaxis_next(self, *args):
        self.axis_done('z')
        if self.data.axes != 1:
            if self.has_spindle_speed_control():
                self.widgets.druid1.set_page(self.widgets.spindle)
            else:
                self.widgets.druid1.set_page(self.widgets.advanced)
            return True

    def on_aaxis_next(self, *args):
        self.axis_done('a')
        if self.has_spindle_speed_control():
            self.widgets.druid1.set_page(self.widgets.spindle)
        else:
            self.widgets.druid1.set_page(self.widgets.advanced)
        return True

    def has_spindle_speed_control(self):
        d = self.data
        return PWM in (d.pin1, d.pin2, d.pin3, d.pin4, d.pin5, d.pin6, d.pin7,
            d.pin8, d.pin9, d.pin14, d.pin16, d.pin17) or \
                PPR in (d.pin10, d.pin11, d.pin12, d.pin13, d.pin15) or \
                PHA in (d.pin10, d.pin11, d.pin12, d.pin13, d.pin15) \

    def update_pps(self, axis):
        w = self.widgets
        d = self.data
        def get(n): return float(w[axis + n].get_text())

        try:
            pitch = get("leadscrew")
            if d.units == 1 or axis == 'a': pitch = 1./pitch
            pps = (pitch * get("steprev") * get("microstep") *
                (get("pulleynum") / get("pulleyden")) * get("maxvel"))
            if pps == 0: raise ValueError
            pps = abs(pps)
            acctime = get("maxvel") / get("maxacc")
            accdist = acctime * .5 * get("maxvel")
            w[axis + "acctime"].set_text("%.4f" % acctime)
            w[axis + "accdist"].set_text("%.4f" % accdist)
            w[axis + "hz"].set_text("%.1f" % pps)
            scale = self.data[axis + "scale"] = (1.0 * pitch * get("steprev")
                * get("microstep") * (get("pulleynum") / get("pulleyden")))
            w[axis + "scale"].set_text("%.1f" % scale)
            self.widgets.druid1.set_buttons_sensitive(1,1,1,1)
            w[axis + "axistest"].set_sensitive(1)
        except (ValueError, ZeroDivisionError): # Some entries not numbers or not valid
            w[axis + "acctime"].set_text("")
            w[axis + "accdist"].set_text("")
            w[axis + "hz"].set_text("")
            w[axis + "scale"].set_text("")
            self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
            w[axis + "axistest"].set_sensitive(0)

    def on_xsteprev_changed(self, *args): self.update_pps('x')
    def on_ysteprev_changed(self, *args): self.update_pps('y')
    def on_zsteprev_changed(self, *args): self.update_pps('z')
    def on_asteprev_changed(self, *args): self.update_pps('a')

    def on_xmicrostep_changed(self, *args): self.update_pps('x')
    def on_ymicrostep_changed(self, *args): self.update_pps('y')
    def on_zmicrostep_changed(self, *args): self.update_pps('z')
    def on_amicrostep_changed(self, *args): self.update_pps('a')

    def on_xpulleynum_changed(self, *args): self.update_pps('x')
    def on_ypulleynum_changed(self, *args): self.update_pps('y')
    def on_zpulleynum_changed(self, *args): self.update_pps('z')
    def on_apulleynum_changed(self, *args): self.update_pps('a')

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

        if self.data.pyvcp and not self.widgets.radiobutton8.get_active() == True:                
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

    def update_axis_params(self, *args):
        axis = self.axis_under_test
        if axis is None: return
        halrun = self.halrun
        halrun.write("""
            setp stepgen.0.maxaccel %(accel)f
            setp stepgen.0.maxvel %(vel)f
            setp steptest.0.jog-minus %(jogminus)s
            setp steptest.0.jog-plus %(jogplus)s
            setp steptest.0.run %(run)s
            setp steptest.0.amplitude %(amplitude)f
            setp steptest.0.maxvel %(vel)f
            setp steptest.0.dir %(dir)s
        """ % {
            'jogminus': self.jogminus,
            'jogplus': self.jogplus,
            'run': self.widgets.run.get_active(),
            'amplitude': self.widgets.testamplitude.get_value(),
            'accel': self.widgets.testacc.get_value(),
            'vel': self.widgets.testvel.get_value(),
            'dir': self.widgets.testdir.get_active(),
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
    
    def testpanel(self,w):
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        if self.widgets.radiobutton5.get_active() == True:
           return True
        if self.widgets.radiobutton6.get_active() == True:
           panel = "spindle.xml"
        if self.widgets.radiobutton8.get_active() == True:
           panel = "custompanel.xml"
           panelname = os.path.expanduser("~/emc2/configs/%s" % self.data.machinename)
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )    
        halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
        if self.widgets.radiobutton6.get_active() == True:
                halrun.write("setp displaytest.spindle-speed 1000\n")
        halrun.write("waitusr displaytest\n"); halrun.flush()
        halrun.close()   

    def load_ladder(self,w):         
        newfilename = os.path.join(distdir, "configurable_options/ladder/TEMP.clp")    
        self.data.modbus = self.widgets.modbus.get_active()
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")
        halrun.write(""" 
              loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n
              loadrt classicladder_rt numPhysInputs=%(din)d numPhysOutputs=%(dout)d numS32in=%(sin)d numS32out=%(sout)d\
                     numFloatIn=%(fin)d numFloatOut=%(fout)d\n
              addf classicladder.0.refresh slow\n
              start\n
                      """ % {
                      'period': 50000,
                      'din': self.widgets.digitsin.get_value(),
                      'dout': self.widgets.digitsout.get_value(),
                      'sin': self.widgets.s32in.get_value(),
                      'sout': self.widgets.s32out.get_value(), 
                      'fin':self.widgets.floatsin.get_value(),
                      'fout':self.widgets.floatsout.get_value(),
                 })
        if self.widgets.radiobutton1.get_active() == True:
            if self.data.tempexists:
               self.data.laddername='TEMP.clp'
            else:
               self.data.laddername= 'blank.clp'
        if self.widgets.radiobutton2.get_active() == True:
            self.data.laddername= 'estop.clp'
        if self.widgets.radiobutton3.get_active() == True:
            self.data.laddername = 'serialmodbus.clp'
            self.data.modbus = True
            self.widgets.modbus.set_active(self.data.modbus)
        if self.widgets.radiobutton4.get_active() == True:
            self.data.laddername='custom.clp'
            originalfile = filename = os.path.expanduser("~/emc2/configs/%s/custom.clp" % self.data.machinename)
        else:
            filename = os.path.join(distdir, "configurable_options/ladder/"+ self.data.laddername)        
        if self.data.modbus == True: 
            halrun.write("loadusr -w classicladder --modmaster --newpath=%(newfilename)s %(filename)s\
                \n" %          { 'newfilename':newfilename ,'filename':filename })
        else:
            halrun.write("loadusr -w classicladder --newpath=%(newfilename)s %(filename)s\n" % { 'newfilename':newfilename ,'filename':filename })
        halrun.flush()
        halrun.close()
        if os.path.exists(newfilename):
            self.data.tempexists = True
            self.widgets.newladder.set_text('Edited ladder program')
            self.widgets.radiobutton1.set_active(True)
        else:
            self.data.tempexists = 0
        

    def test_axis(self, axis):
        data = self.data
        widgets = self.widgets

        vel = float(widgets[axis + "maxvel"].get_text())
        acc = float(widgets[axis + "maxacc"].get_text())

        scale = data[axis + "scale"]
        maxvel = 1.5 * vel
        if data.doublestep():
                period = int(1e9 / maxvel / scale)
        else:
                period = int(.5e9 / maxvel / scale)

        steptime = self.widgets.steptime.get_value()
        stepspace = self.widgets.stepspace.get_value()
        latency = self.widgets.latency.get_value()
        minperiod = self.data.minperiod()

        if period < minperiod:
            period = minperiod
            if data.doublestep():
                maxvel = 1e9 / minperiod / abs(scale)
            else:
                maxvel = 1e9 / minperiod / abs(scale)
        if period > 100000:
            period = 100000

        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")

        axnum = "xyza".index(axis)
        step = axis + "step"
        dir = axis + "dir"
        halrun.write("""
            loadrt steptest
            loadrt stepgen step_type=0
            loadrt probe_parport
            loadrt hal_parport cfg=%(ioaddr)s
            loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n

            addf stepgen.make-pulses fast
            addf parport.0.write fast

            addf stepgen.capture-position slow
            addf steptest.0 slow
            addf stepgen.update-freq slow

            net step stepgen.0.step => parport.0.pin-%(steppin)02d-out
            net dir stepgen.0.dir => parport.0.pin-%(dirpin)02d-out
            net cmd steptest.0.position-cmd => stepgen.0.position-cmd
            net fb stepgen.0.position-fb => steptest.0.position-fb

            setp parport.0.pin-%(steppin)02d-out-reset 1
            setp stepgen.0.steplen 1
            setp stepgen.0.dirhold %(dirhold)d
            setp stepgen.0.dirsetup %(dirsetup)d
            setp stepgen.0.position-scale %(scale)f
            setp steptest.0.epsilon %(onestep)f

            setp stepgen.0.enable 1
        """ % {
            'period': period,
            'ioaddr': data.ioaddr,
            'steppin': data.find_output(step),
            'dirpin': data.find_output(dir),
            'dirhold': data.dirhold + data.latency,
            'dirsetup': data.dirsetup + data.latency,
            'onestep': abs(1. / data[axis + "scale"]),
            'scale': data[axis + "scale"],
        })

        if data.doublestep():
            halrun.write("""
                setp parport.0.reset-time %(resettime)d
                setp stepgen.0.stepspace 0
                addf parport.0.reset fast
            """ % {
                'resettime': data['steptime']
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
            inv = getattr(data, "pin%dinv" % pin)
            if inv:
                halrun.write("setp parport.0.pin-%(pin)02d-out-invert 1\n"
                    % {'pin': pin}) 

        widgets.dialog1.set_title(_("%s Axis Test") % axis.upper())

        if axis == "a":
            widgets.testvelunit.set_text(_("deg / s"))
            widgets.testaccunit.set_text(_("deg / s²"))
            widgets.testampunit.set_text(_("deg"))
            widgets.testvel.set_increments(1,5)
            widgets.testacc.set_increments(1,5)
            widgets.testamplitude.set_increments(1,5)
            widgets.testvel.set_range(0, maxvel)
            widgets.testacc.set_range(1, 360000)
            widgets.testamplitude.set_range(0, 1440)
            widgets.testvel.set_digits(1)
            widgets.testacc.set_digits(1)
            widgets.testamplitude.set_digits(1)
            widgets.testamplitude.set_value(10)
        elif data.units:
            widgets.testvelunit.set_text(_("mm / s"))
            widgets.testaccunit.set_text(_("mm / s²"))
            widgets.testampunit.set_text(_("mm"))
            widgets.testvel.set_increments(1,5)
            widgets.testacc.set_increments(1,5)
            widgets.testamplitude.set_increments(1,5)
            widgets.testvel.set_range(0, maxvel)
            widgets.testacc.set_range(1, 100000)
            widgets.testamplitude.set_range(0, 1000)
            widgets.testvel.set_digits(2)
            widgets.testacc.set_digits(2)
            widgets.testamplitude.set_digits(2)
            widgets.testamplitude.set_value(.5)
        else:
            widgets.testvelunit.set_text(_("in / s"))
            widgets.testaccunit.set_text(_("in / s²"))
            widgets.testampunit.set_text(_("in"))
            widgets.testvel.set_increments(.1,5)
            widgets.testacc.set_increments(1,5)
            widgets.testamplitude.set_increments(.1,5)
            widgets.testvel.set_range(0, maxvel)
            widgets.testacc.set_range(1, 3600)
            widgets.testamplitude.set_range(0, 36)
            widgets.testvel.set_digits(1)
            widgets.testacc.set_digits(1)
            widgets.testamplitude.set_digits(1)
            widgets.testamplitude.set_value(15)

        self.jogplus = self.jogminus = 0
        self.widgets.testdir.set_active(0)
        self.widgets.run.set_active(0)
        self.widgets.testacc.set_value(acc)
        self.widgets.testvel.set_value(vel)
        self.axis_under_test = axis
        self.update_axis_params()

        halrun.write("start\n"); halrun.flush()
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
            widgets[axis+"maxacc"].set_text("%s" % widgets.testacc.get_value())
            widgets[axis+"maxvel"].set_text("%s" % widgets.testvel.get_value())
        self.axis_under_test = None

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
