#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    stepconf 1.1 revamped by Chris Morley 2014
#    replaced Gnome Druid as that is not available in future linux distributions
#    and because of GTK/GLADE bugs, the GLADE file could only be edited with Ubuntu 8.04
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

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GLib
from gi.repository import Gdk

import signal
import sys
import os
from optparse import Option, OptionParser
import hal
import xml.dom.minidom
import hashlib
import math
import errno
import textwrap
import hal
import shutil
import time
from multifilebuilder import MultiFileBuilder

try:
    from defusedexpat import pyexpat as expat
except ImportError:
    from xml.parsers import expat

import subprocess

import traceback
# otherwise, on hardy the user is shown spurious "[application] closed
# unexpectedly" messages but denied the ability to actually "report [the]
# problem"
def excepthook(exc_type, exc_obj, exc_tb):
    try:
        w = app.w.window1
    except NameError:
        w = None
    lines = traceback.format_exception(exc_type, exc_obj, exc_tb)
    msg=_("Stepconf encountered an error.  The following "
           "information may be useful in troubleshooting:\n\n")
    m = Gtk.MessageDialog(
        parent=w,
        modal=True,
        destroy_with_parent=True,
        message_type=Gtk.MessageType.ERROR,
        buttons=Gtk.ButtonsType.OK,
        text=msg + "".join(lines))
    m.show()
    m.run()
    m.destroy()
sys.excepthook = excepthook

BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))

# translations,locale
import locale, gettext
LOCALEDIR = os.path.join(BASE, "share", "locale")
domain = "linuxcnc"
gettext.install(domain, localedir=LOCALEDIR)

locale.setlocale(locale.LC_ALL, '')
locale.bindtextdomain(domain, LOCALEDIR)
gettext.bindtextdomain(domain, LOCALEDIR)

datadir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "linuxcnc","stepconf")
main_datadir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "linuxcnc")
wizard = os.path.join(datadir, "linuxcnc-wizard.gif")
if not os.path.isfile(wizard):
    wizard = os.path.join(main_datadir, "linuxcnc-wizard.gif")
if not os.path.isfile(wizard):
    print("cannot find linuxcnc-wizard.gif, looked in %s and %s" % (datadir, main_datadir))
    sys.exit(1)

icondir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
linuxcncicon = os.path.join(icondir, "linuxcncicon.png")
if not os.path.isfile(linuxcncicon):
    linuxcncicon = os.path.join("/etc/linuxcnc/linuxcnc-wizard.gif")
if not os.path.isfile(linuxcncicon):
    linuxcncicon = os.path.join("/usr/share/linuxcnc/linuxcncicon.png")

distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "configs", "common")
if not os.path.isdir(distdir):
    distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "doc", "linuxcnc", "sample-configs", "common")
if not os.path.isdir(distdir):
    distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "linuxcnc", "sample-configs", "common")
if not os.path.isdir(distdir):
    distdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "doc", "linuxcnc", "examples", "sample-configs", "common")
if not os.path.isdir(distdir):
    distdir = "/usr/share/doc/linuxcnc/examples/sample-configs/common"


from stepconf import pages
from stepconf import build_INI
from stepconf import build_HAL

debug = False

def makedirs(d):
    try:
        os.makedirs(d)
    except os.error as detail:
        if detail.errno != errno.EEXIST: raise
makedirs(os.path.expanduser("~/linuxcnc/configs"))

def md5sum(filename):
    try:
        f = open(filename, "rb")
    except IOError:
        return None
    else:
        return hashlib.md5(f.read()).hexdigest()

class Private_Data:
    def __init__(self):
        self.in_pport_prepare = True
        self.distdir = distdir
        self.available_page =[['intro', 'Stepconf', True],['start', _('Start'), True],
                                ['base',_('Base Information'),True],
                                ['pport1', _('Parallel Port 1'),True],['pport2', _('Parallel Port 2'),True],
                                ['options',_('Options'), True],['halui_page', _('HALUI'), True],
                                ['ubuttons',_('User Buttons'), False],
                                ['thcad',_('QtPlasmaC THCAD'), False],
                                ['axisx', _('Axis X'), True],
                                ['axisy', _('Axis Y'), True],['axisz', _('Axis Z'), True],
                                ['axisu', _('Axis U'), True],['axisv', _('Axis V'), True],
                                ['axisa', _('Axis A'), True],
                                ['spindle',_('Spindle'), True],['finished',_('Almost Done'),True]
                             ]
        # internalname / displayed name / steptime/ step space / direction hold / direction setup
        self.alldrivertypes = [
                            ["gecko201", _("Gecko 201"), 500, 4000, 20000, 1000],
                            ["gecko202", _("Gecko 202"), 500, 4500, 20000, 1000],
                            ["gecko203v", _("Gecko 203v"), 1000, 2000, 200 , 200],
                            ["gecko210", _("Gecko 210"),  500, 4000, 20000, 1000],
                            ["gecko212", _("Gecko 212"),  500, 4000, 20000, 1000],
                            ["gecko320", _("Gecko 320"),  3500, 500, 200, 200],
                            ["gecko540", _("Gecko 540"),  5000, 5000, 10000, 10000],
                            ["l297", _("L297"), 500,  4000, 4000, 1000],
                            ["pmdx150", _("PMDX-150"), 1000, 2000, 1000, 1000],
                            ["sherline", _("Sherline"), 22000, 22000, 100000, 100000],
                            ["xylotex", _("Xylotex 8S-3"), 2000, 1000, 200, 200],
                            ["oem750", _("Parker-Compumotor oem750"), 1000, 1000, 1000, 200000],
                            ["jvlsmd41", _("JVL-SMD41 or 42"), 500, 500, 2500, 2500],
                            ["hobbycnc", _("Hobbycnc Pro Chopper"), 2000, 2000, 2000, 2000],
                            ["keling", _("Keling 4030"), 5000, 5000, 20000, 20000],
                            ["dm860tv10", _("DM860T v1.0"), 5000, 5000, 5000, 5000],
                            ["dm860tv30", _("DM860T v3.0"), 5000, 5000, 5000, 5000],
                            ]

        (   self.XSTEP, self.XDIR, self.YSTEP, self.YDIR,
            self.ZSTEP, self.ZDIR, self.ASTEP, self.ADIR,
            self.USTEP, self.UDIR, self.VSTEP, self.VDIR,
            self.X2STEP, self.X2DIR, self.Y2STEP, self.Y2DIR,
            self.ON, self.CW, self.CCW, self.PWM, self.BRAKE,
            self.MIST, self.FLOOD, self.ESTOP, self.AMP,
            self.PUMP, self.DOUT0, self.DOUT1, self.DOUT2, self.DOUT3,
            self.PLASMAC_TORCH,self.OHMIC_ENABLE,self.SCRIBE_ARM,self.SCRIBE_ON,self.PLASMAC_LASER,
            self.UNUSED_OUTPUT,
        ) = self.hal_output_names = [
            "xstep", "xdir", "ystep", "ydir",
            "zstep", "zdir", "astep", "adir",
            "ustep", "udir", "vstep", "vdir",
            "x2step", "x2dir", "y2step", "y2dir",
            "spindle-on", "spindle-cw", "spindle-ccw", "spindle-pwm", "spindle-brake",
            "coolant-mist", "coolant-flood", "estop-out", "xenable",
            "charge-pump", "dout-00", "dout-01", "dout-02", "dout-03",
            "plasmac:torch-on", "plasmac:ohmic-enable", "plasmac:scribe-arm", "plasmac:scribe-on", "plasmac:laser-on",
            "unused-output"]

        (   self.ESTOP_IN, self.PROBE, self.PPR, self.PHA, self.PHB,
            self.HOME_X, self.HOME_Y, self.HOME_Z, self.HOME_A, self.HOME_U, self.HOME_V, self.HOME_TX, self.HOME_TY,
            self.MIN_HOME_X, self.MIN_HOME_Y, self.MIN_HOME_Z, self.MIN_HOME_A, self.MIN_HOME_U, self.MIN_HOME_V, self.MIN_HOME_TX, self.MIN_HOME_TY,
            self.MAX_HOME_X, self.MAX_HOME_Y, self.MAX_HOME_Z, self.MAX_HOME_A, self.MAX_HOME_U, self.MAX_HOME_V, self.MAX_HOME_TX, self.MAX_HOME_TY,
            self.BOTH_HOME_X, self.BOTH_HOME_Y, self.BOTH_HOME_Z, self.BOTH_HOME_A, self.BOTH_HOME_U, self.BOTH_HOME_V, self.BOTH_HOME_TX, self.BOTH_HOME_TY,
            self.MIN_X, self.MIN_Y, self.MIN_Z, self.MIN_A, self.MIN_U, self.MIN_V, self.MIN_TX, self.MIN_TY,
            self.MAX_X, self.MAX_Y, self.MAX_Z, self.MAX_A,self.MAX_U, self.MAX_V, self.MAX_TX, self.MAX_TY,
            self.BOTH_X, self.BOTH_Y, self.BOTH_Z, self.BOTH_A,self.BOTH_U, self.BOTH_V, self.BOTH_TX, self.BOTH_TY,
            self.ALL_LIMIT, self.ALL_HOME, self.ALL_LIMIT_HOME, self.DIN0, self.DIN1, self.DIN2, self.DIN3,
            self.ARC_VOLTS,self.ARC_OK,self.FLOAT_SWITCH,self.BREAKAWAY,
            self.OHMIC_CONTACT,self.MOVE_UP,self.MOVE_DOWN,
            self.UNUSED_INPUT,
        ) = self.hal_input_names = [
            "estop-ext", "probe-in", "spindle-index", "spindle-phase-a", "spindle-phase-b",
            "home-x", "home-y", "home-z", "home-a","home-u", "home-v", "home-x2", "home-y2",
            "min-home-x", "min-home-y", "min-home-z", "min-home-a","min-home-u", "min-home-v", "min-home-x2", "min-home-y2",
            "max-home-x", "max-home-y", "max-home-z", "max-home-a","max-home-u", "max-home-v", "max-home-x2", "max-home-y2",
            "both-home-x", "both-home-y", "both-home-z", "both-home-a", "both-home-u", "both-home-v", "both-home-x2", "both-home-y2",
            "min-x", "min-y", "min-z", "min-a","min-u", "min-v", "min-x2", "min-y2",
            "max-x", "max-y", "max-z", "max-a", "max-u", "max-v", "max-x2", "max-y2",
            "both-x", "both-y", "both-z", "both-a", "both-u", "both-v", "both-x1", "both-y2",
            "all-limit", "all-home", "all-limit-home", "din-00", "din-01", "din-02", "din-03",
            "plasmac:arc-voltage-raw", "plasmac:arc-ok-in", "plasmac:float-switch", "plasmac:breakaway",
            "plasmac:ohmic-sense-in", "plasmac:move-up", "plasmac:move-down",
            "unused-input"]

        self.human_output_names = (_("X Step"), _("X Direction"), _("Y Step"), _("Y Direction"),
            _("Z Step"), _("Z Direction"), _("A Step"), _("A Direction"),
            _("U Step"), _("U Direction"), _("V Step"), _("V Direction"),
            _("Tandem X Step"), _("Tandem X Direction"), _("Tandem Y Step"), _("Tandem Y Direction"),
            _("Spindle ON"),_("Spindle CW"), _("Spindle CCW"), _("Spindle PWM"), _("Spindle Brake"),
            _("Coolant Mist"), _("Coolant Flood"), _("ESTOP Out"), _("Amplifier Enable"),
            _("Charge Pump"),
            _("Digital out 0"), _("Digital out 1"), _("Digital out 2"), _("Digital out 3"),
            _("Plasma Torch On"),_("Plasma Ohmic Enable"), _("Plasma Scribe Arm"), _("Plasma Scribe On"),_("Plasma Laser On"),
            _("Unused"))

        self.human_input_names = (_("ESTOP In"), _("Probe In"),
            _("Spindle Index"), _("Spindle Phase A"), _("Spindle Phase B"),
            _("Home X"), _("Home Y"), _("Home Z"), _("Home A"), _("Home U"), _("Home V"),
            _("Home Tandem X"), _("Home Tandem Y"),
            _("Minimum Limit + Home X"), _("Minimum Limit + Home Y"),
            _("Minimum Limit + Home Tandem X"), _("Minimum Limit + Home Tandem Y"),
            _("Minimum Limit + Home Z"), _("Minimum Limit + Home A"),
            _("Minimum Limit + Home U"), _("Minimum Limit + Home V"),
            _("Maximum Limit + Home X"), _("Maximum Limit + Home Y"),
            _("Maximum Limit + Home Tandem X"), _("Maximum Limit + Home Tandem Y"),
            _("Maximum Limit + Home Z"), _("Maximum Limit + Home A"),
            _("Maximum Limit + Home U"), _("Maximum Limit + Home V"),
            _("Both Limit + Home X"), _("Both Limit + Home Y"),
            _("Both Limit + Home Z"), _("Both Limit + Home A"),
            _("Both Limit + Home U"), _("Both Limit + Home V"),
            _("Both Limit + Home Tandem X"), _("Both Limit + Home Tandem Y"),
            _("Minimum Limit X"), _("Minimum Limit Y"),
            _("Minimum Limit Tandem X"), _("Minimum Limit Tandem Y"),
            _("Minimum Limit Z"), _("Minimum Limit A"),
            _("Minimum Limit U"), _("Minimum Limit V"),
            _("Maximum Limit X"), _("Maximum Limit Y"),
            _("Maximum Limit Tandem X"), _("Maximum Limit Tandem Y"),
            _("Maximum Limit Z"), _("Maximum Limit A"),
            _("Maximum Limit U"), _("Maximum Limit V"),
            _("Both Limit X"), _("Both Limit Y"),
            _("Both Limit Z"), _("Both Limit A"),
            _("Both Limit U"), _("Both Limit V"),
            _("Both Limit Tandem X"), _("Both Limit Tandem Y"),
            _("All limits"), _("All home"), _("All limits + homes"),
            _("Digital in 0"), _("Digital in 1"), _("Digital in 2"), _("Digital in 3"),
            _("Plasma Arc Voltage"), _("Plasma Arc OK"), _("Plasma Float Switch"), _("Plasma Breakaway"),
            _("Plasma Ohmic Contact"), _("Plasma Move Up"), _("Plasma Move Down"),
            _("Unused"))

        self.MESS_START = _('Start')
        self.MESS_FWD = _('Forward')
        self.MESS_DONE = _('Done')
        self.MESS_CL_REWRITE =_("OK to replace existing custom ladder program?\nExisting Custom.clp will be renamed custom_backup.clp.\nAny existing file named -custom_backup.clp- will be lost. ")
        self.MESS_CL_EDITED = _("You edited a ladder program and have selected a different program to copy to your configuration file.\nThe edited program will be lost.\n\nAre you sure?  ")
        self.MESS_NO_ESTOP = _("You need to designate an E-stop input pin in the Parallel Port Setup page for this program.")
        self.MESS_PYVCP_REWRITE =_("OK to replace existing custom pyvcp panel file ?\nExisting custompanel.xml will be renamed custompanel_backup.xml.\nAny existing file named custompanel_backup.xml will be lost. ")
        self.MESS_ABORT = _("Quit Stepconf and discard changes?")
        self.MESS_QUIT = _("The configuration has been built and saved.\nDo you want to quit?")
        self.MESS_NO_REALTIME = _("You are using a simulated-realtime version of LinuxCNC, so testing / tuning of hardware is unavailable.")
        self.MESS_KERNEL_WRONG = _("You are using a realtime version of LinuxCNC but didn't load a realtime kernel so testing / tuning of hardware is\
                 unavailable.\nThis is possibly because you updated the OS and it doesn't automatically load the RTAI kernel anymore.\n"+
            "You are using the {} kernel.\nYou need to use kernel:".format(os.uname()[2]))

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class Data:
    def __init__(self,SIG):
        #pw = pwd.getpwuid(os.getuid())
        self.createsymlink = True
        self.createshortcut = True
        self.sim_hardware = True
        self.sim_hardware = False
        self._lastconfigname= ""
        self._chooselastconfig = True
        self._preference_version = 1.0

        self.machinename = _("my-mill")
        self.axes = 0 # XYZ
        self.units = 0 # inch
        self.drivertype = "Other"
        self.steptime = 5000
        self.stepspace = 5000
        self.dirhold = 20000 
        self.dirsetup = 20000
        self.latency = 15000
        self.period = 25000

        self.ioaddr = "0"
        self.ioaddr2 = "1"
        self.pp2_direction = 0 # output
        self.ioaddr3 = "2"
        self.pp3_direction = 0 # output
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

        self.select_axis = True
        self.select_gmoccapy = False
        self.select_qtdragon = False
        self.select_qtplasmac = False

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

        self.pin1 = SIG.ESTOP
        self.pin2 = SIG.XSTEP
        self.pin3 = SIG.XDIR
        self.pin4 = SIG.YSTEP
        self.pin5 = SIG.YDIR
        self.pin6 = SIG.ZSTEP
        self.pin7 = SIG.ZDIR
        self.pin8 = SIG.ASTEP
        self.pin9 = SIG.ADIR
        self.pin14 = SIG.CW
        self.pin16 = SIG.PWM
        self.pin17 = SIG.AMP

        self.pin10 = SIG.UNUSED_INPUT
        self.pin11 = SIG.UNUSED_INPUT
        self.pin12 = SIG.UNUSED_INPUT
        self.pin13 = SIG.UNUSED_INPUT
        self.pin15 = SIG.UNUSED_INPUT

        #   port 2
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            self[p] = SIG.UNUSED_OUTPUT
            p = 'pp2_pin%dinv' % pin
            self[p] = 0
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            self[p] = SIG.UNUSED_INPUT
            p = 'pp2_pin%d_in_inv' % pin
            self[p] = 0

        for i in ('x','y','z','u','v'):
             self[i+'steprev'] = 200
             self[i+'microstep'] = 2
             self[i+'pulleynum'] = 1
             self[i+'pulleyden'] = 1
             self[i+'leadscrew'] = 20
             self[i+'maxvel'] = 0
             self[i+'maxacc'] = 0

             self[i+'homepos'] = 0
             self[i+'minlim'] =  0
             self[i+'maxlim'] =  0
             self[i+'homesw'] =  0
             self[i+'homevel'] = 0
             self[i+'latchdir'] = 0
             self[i+'scale'] = 0

        # set xyzuv axes defaults depending on units true = imperial
        self.set_axis_unit_defaults(True)

        self.asteprev = 200
        self.amicrostep = 2
        self.apulleynum = 1
        self.apulleyden = 1
        self.aleadscrew = 360
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
        self.spindlefiltergain = .01
        self.spindlenearscale = 1.5
        self.usespindleatspeed = False

        self.digitsin = 15
        self.digitsout = 15
        self.s32in = 10
        self.s32out = 10
        self.floatsin = 10
        self.floatsout = 10
        self.halui = 0
        self.halui_list = []
        self.createsymlink = 1
        self.createshortcut = 1

        # QtPlasmaC
        self.qtplasmacmode = 0
        self.qtplasmacscreen = 0
        self.qtplasmacestop = 0
        self.qtplasmacdro = 0
        self.qtplasmacerror = 0
        self.qtplasmacstart = 0
        self.qtplasmacpause = 0
        self.qtplasmacstop = 0
        self.qtplasmacpmx = ""
        self.qtplasmacbase = BASE
        self.qtplasmac_bnames = ["OHMIC\\TEST","PROBE\\TEST","SINGLE\\CUT","NORMAL\\CUT","TORCH\\PULSE","FRAMING", "USER\\MANUAL",
                                 "","","","","","","","","","","","",""]
        self.qtplasmac_bcodes = ["ohmic-test","probe-test 10","single-cut","cut-type","torch-pulse 0.5","framing", "user-manual",
                                 "","","","","","","","","","","","",""]
        self.thcadenc = 0
        self.voltsmodel = "10"
        self.voltsfjumper = "64"
        self.voltszerof = 100.0
        self.voltsfullf = 999.0
        self.voltsrdiv = 20
        self.ohmiccontact = 0

        # tandem joints
        self.tandemjoints = []
        self.axislist = []

    # change the XYZ axis defaults to metric or imperial
    # This only sets data that makes sense to change eg gear ratio don't change
    def set_axis_unit_defaults(self, units=True):
        if units: # imperial
            for i in ('x','y','z','u','v'):
                self[i+'maxvel'] = 1
                self[i+'maxacc'] = 30
                self[i+'homevel'] = .05
                self[i+'leadscrew'] = 20
                if not i == 'z':
                    self[i+'minlim'] = 0
                    self[i+'maxlim'] = 8
                else:
                    self.zminlim = -4
                    self.zmaxlim = 0
        else: # metric
            for i in ('x','y','z','u','v'):
                self[i+'maxvel'] = 25
                self[i+'maxacc'] = 750
                self[i+'homevel'] = 1.5
                self[i+'leadscrew'] = 5
                if not i =='z':
                    self[i+'minlim'] = 0
                    self[i+'maxlim'] = 200
                else:
                    self.zminlim = -100
                    self.zmaxlim = 0

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
        uhz = self.hz('u')
        vhz = self.hz('v')
        ahz = self.hz('a')
        if self.axes == 1:
            pps = max(xhz, yhz, zhz, ahz)
        elif self.axes == 0:
            pps = max(xhz, yhz, zhz)
        elif self.axes == 2:
            pps = max(xhz, zhz)
        elif self.axes == 3:
            pps = max(xhz, yhz, uhz, vhz)
        elif self.axes == 4:
            pps = max(xhz, yhz)
        else:
            print('error in ideal period calculation - number of axes unrecognized')
            return
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

    def load_preferences(self):
        # set preferences if they exist
        link = short = advanced = show_pages = False
        filename = os.path.expanduser("~/.stepconf-preferences")
        if os.path.exists(filename):
            version = 0.0
            d = xml.dom.minidom.parse(open(filename, "rt"))
            for n in d.getElementsByTagName("property"):
                name = n.getAttribute("name")
                text = n.getAttribute('value')
                if name == "version":
                    version = eval(text)
                if name == "always_shortcut":
                    short = eval(text)
                if name == "always_link":
                    link = eval(text)
                if name == "sim_hardware":
                    sim_hardware = eval(text)
                if name == "machinename":
                    self._lastconfigname = text
                if name == "chooselastconfig":
                    self._chooselastconfig = eval(text)
            # these are set from the hidden preference file
            self.createsymlink = link
            self.createshortcut = short
            self.sim_hardware = sim_hardware

    # write stepconf's hidden preference file
    def save_preferences(self):
        filename = os.path.expanduser("~/.stepconf-preferences")
        print(filename)
        d2 = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, "int-pncconf", None)
        e2 = d2.documentElement

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'float')
        n2.setAttribute('name', "version")
        n2.setAttribute('value', str("%f"%self._preference_version))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "always_shortcut")
        n2.setAttribute('value', str("%s"% self.createshortcut))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "always_link")
        n2.setAttribute('value', str("%s"% self.createsymlink))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "sim_hardware")
        n2.setAttribute('value', str("%s"% self.sim_hardware))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "chooselastconfig")
        n2.setAttribute('value', str("%s"% self._chooselastconfig))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'string')
        n2.setAttribute('name', "machinename")
        n2.setAttribute('value', str("%s"%self.machinename))

        d2.writexml(open(filename, "wt"), addindent="  ", newl="\n")

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
                dialog = Gtk.MessageDialog(
                    parent=app.w.window1,
                    modal=True,
                    destroy_with_parent=True,
                    message_type=Gtk.MessageType.WARNING,
                    buttons=Gtk.ButtonsType.OK,
                    text=warnings)
                dialog.show_all()
                dialog.run()
                dialog.destroy()
            else:
                for para in warnings:
                    for line in textwrap.wrap(para, 78): print(line)
                    print()
                print()
                if force: return
                response = input(_("Continue? "))
                if response[0] not in _("yY"): raise SystemExit(1)

        for p in (10,11,12,13,15):
            pin = "pin%d" % p
            p = self[pin]
        for p in (1,2,3,4,5,6,7,8,9,14,16,17):
            pin = "pin%d" % p
            p = self[pin]

    def save(self,basedir):
        base = basedir
        self.md5sums = []

        if self.classicladder: 
           if not self.laddername == "custom.clp":
                filename = os.path.join(distdir, "configurable_options/ladder/%s" % self.laddername)
                original = os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.machinename)
                if os.path.exists(filename):     
                  if os.path.exists(original):
                     print("custom file already exists")
                     shutil.copy( original,os.path.expanduser("~/linuxcnc/configs/%s/custom_backup.clp" % self.machinename) ) 
                     print("made backup of existing custom")
                  shutil.copy( filename,original)
                  print("copied ladder program to usr directory")
                  print("%s" % filename)
                else:
                     print("Master or temp ladder files missing from configurable_options dir")

        if self.pyvcp and not self.pyvcpname == "custompanel.xml":                
           panelname = os.path.join(distdir, "configurable_options/pyvcp/%s" % self.pyvcpname)
           originalname = os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.machinename)
           if os.path.exists(panelname):     
                  if os.path.exists(originalname):
                     print("custom PYVCP file already exists")
                     shutil.copy( originalname,os.path.expanduser("~/linuxcnc/configs/%s/custompanel_backup.xml" % self.machinename) ) 
                     print("made backup of existing custom")
                  shutil.copy( panelname,originalname)
                  print("copied PYVCP program to usr directory")
                  print("%s" % panelname)
           else:
                  print("Master PYVCP files missing from configurable_options dir")

        filename = "%s.stepconf" % base

        d = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, "stepconf", None)
        e = d.documentElement

        for k, v in sorted(self.__dict__.items()):
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

        d.writexml(open(filename, "wt"), addindent="  ", newl="\n")
        print("%s" % base)

        # see http://freedesktop.org/wiki/Software/xdg-user-dirs
        desktop = subprocess.getoutput("""
            test -f ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs && . ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs
            echo ${XDG_DESKTOP_DIR:-$HOME/Desktop}""")
        if self.createsymlink:
            shortcut = os.path.join(desktop, self.machinename)
            if os.path.exists(desktop) and not os.path.exists(shortcut):
                os.symlink(base,shortcut)

        if self.createshortcut and os.path.exists(desktop):
            if os.path.exists(BASE + "/scripts/linuxcnc"):
                scriptspath = (BASE + "/scripts/linuxcnc")
            else:
                scriptspath ="linuxcnc"

            filename = os.path.join(desktop, "%s.desktop" % self.machinename)
            file = open(filename, "w")
            print("[Desktop Entry]", file=file)
            print("Version=1.0", file=file)
            print("Terminal=false", file=file)
            print("Name=" + _("launch %s") % self.machinename, file=file)
            print("Exec=%s %s/%s.ini" \
                         % ( scriptspath, base, self.machinename ), file=file)
            print("Type=Application", file=file)
            print("Comment=" + _("Desktop Launcher for LinuxCNC config made by Stepconf"), file=file)
            print("Icon=%s"% linuxcncicon, file=file)
            file.close()
            # Ubuntu 10.04 require launcher to have execute permissions
            os.chmod(filename,0o775)

    def add_md5sum(self, filename, mode="r"):
        self.md5sums.append((filename, md5sum(filename)))

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# a class for holding the glade widgets rather then searching for them each time
class Widgets:
    def __init__(self, xml):
        self._xml = xml
    def __getattr__(self, attr):
        r = self._xml.get_object(attr)
        if r is None: raise AttributeError("No widget %r" % attr)
        return r
    def __getitem__(self, attr):
        r = self._xml.get_object(attr)
        if r is None: raise IndexError("No widget %r" % attr)
        return r

class StepconfApp:
    def __init__(self, dbgstate):
        global debug
        debug = self.debug = dbgstate
        global dbg
        dbg = self.dbg
        self.recursive_block = False
        self.axis_under_test = None
        # Private data holds the array of pages to load, signals, and messages
        self._p = Private_Data()
        self.d = Data(self._p)
        # Try find parport
        self.d.lparport = self.find_parport()
        # build the glade files
        self.builder = MultiFileBuilder()
        self.builder.set_translation_domain(domain)
        self.builder.add_from_file(os.path.join(datadir,'main_page.glade'))
        window = self.builder.get_object("window1")
        notebook1 = self.builder.get_object("notebook1")
        for x,y,z in (self._p.available_page):
            if x == 'intro':continue
            dbg("loading glade page REFERENCE:%s TITLE:%s STATE:%s"% (x,y,z))
            self.builder.add_from_file(os.path.join(datadir, '%s.glade'%x))
            page = self.builder.get_object(x)
            label = Gtk.Label(label=x)
            notebook1.append_page(child=page, tab_label=label)
        notebook1.set_show_tabs(False)

        self.w = Widgets(self.builder)
        self.p = pages.Pages(self)
        self.INI = build_INI.INI(self)
        self.HAL = build_HAL.HAL(self)
        self.builder.set_translation_domain(domain) # for locale translations
        self.builder.connect_signals( self.p ) # register callbacks from Pages class
        #wiz_pic = Gdk.pixbuf_new_from_file(wizard)
        image = Gtk.Image()
        image.set_from_file(wizard)
        wiz_pic = image.get_pixbuf()
        self.w.wizard_image.set_from_pixbuf(wiz_pic)
        try:
            self.d.load_preferences()
        except expat.ExpatError as ee:
            message = _("Loading configuration error:\n\n{}").format(str(ee))
            dialog = Gtk.MessageDialog(
                parent=window,
                modal=True,
                message_type=Gtk.MessageType.WARNING,
                buttons=Gtk.ButtonsType.OK,
                text=message)
            dialog.show_all()
            dialog.run()
            dialog.destroy()

        self.p.initialize()
        window.show()
        #self.w.xencoderscale.realize()
        window.set_position(Gtk.WindowPosition.CENTER)
        window.hide()
        window.unrealize()
        window.show()

    def build_base(self):
        base = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
        ncfiles = os.path.expanduser("~/linuxcnc/nc_files")
        if not os.path.exists(ncfiles):
            makedirs(ncfiles)
            examples = os.path.join(BASE, "share", "linuxcnc", "ncfiles")
            if not os.path.exists(examples):
                examples = os.path.join(BASE, "nc_files")
            if os.path.exists(examples):
                os.symlink(examples, os.path.join(ncfiles, "examples"))
        makedirs(base)
        return base

    def copy(self, base, filename):
        dest = os.path.join(base, filename)
        if not os.path.exists(dest):
            shutil.copy(os.path.join(distdir, filename), dest)

    def buid_config(self):
        base = self.build_base()
        self.d.save(base)
        self.d.save_preferences()
        #self.write_readme(base)
        self.INI.write_inifile(base)
        self.HAL.write_halfile(base)
        # qtplasmac specific
        if self.d.select_qtplasmac:
            # different tool table for qtplasmac
            filename = os.path.join(base, "tool.tbl")
            file = open(filename, "w")
            print("T0 P1 X0 Y0 ;torch", file=file)
            print("T1 P2 X0 Y0 ;scribe", file=file)
            file.close()
        self.copy(base, "tool.tbl")
        if self.warning_dialog(self._p.MESS_QUIT,False):
            Gtk.main_quit()

#*******************
# GUI Helper functions
#*******************

    # print debug strings
    def dbg(self,str):
        global debug
        if not debug: return
        print("DEBUG: %s"%str)

    # Check for realtime-capable LinuxCNC.
    # Returns True if the running version of LinuxCNC is realtime-capable
    # (or if debug is enabled), returns False otherwise.
    def check_for_rt(self):
        is_realtime_capable = False
        try:
            if hal.is_sim:
                self.warning_dialog(self._p.MESS_NO_REALTIME,True)
            elif hal.is_rt:
                if hal.is_kernelspace:
                    actual_kernel = os.uname()[2]
                    if hal.kernel_version == actual_kernel:
                        is_realtime_capable = True
                    else:
                        self.warning_dialog(self._p.MESS_KERNEL_WRONG + '%s'%hal.kernel_version,True)
                else:
                    is_realtime_capable = True
        except:
            print('STEPCONF WARNING: check-for-realtime function failed - continuing anyways.')
            print(sys.exc_info())
            return True

        if is_realtime_capable or debug:
            return True
        else:
            return False

    # pop up dialog
    def warning_dialog(self,message,is_ok_type):
        if is_ok_type:
           dialog = Gtk.MessageDialog(
                parent=app.w.window1,
                modal=True,
                destroy_with_parent=True,
                message_type=Gtk.MessageType.WARNING,
                buttons=Gtk.ButtonsType.OK,
                text=message)
           dialog.show_all()
           result = dialog.run()
           dialog.destroy()
           return True
        else:   
            dialog = Gtk.MessageDialog(
                parent=app.w.window1,
                modal=True,
                destroy_with_parent=True,
                message_type=Gtk.MessageType.QUESTION,
                buttons=Gtk.ButtonsType.YES_NO,
                text=message)
            dialog.show_all()
            result = dialog.run()
            dialog.destroy()
            if result == Gtk.ResponseType.YES:
                return True
            else:
                return False

# Driver functions
    def drivertype_fromid(self):
        for d in self._p.alldrivertypes:
            if d[0] == self.d.drivertype: return d[1]

    def drivertype_toid(self, what=None):
        if not isinstance(what, int): what = self.drivertype_toindex(what)
        if what < len(self._p.alldrivertypes): return self._p.alldrivertypes[what][0]
        return "other"

    def drivertype_toindex(self, what=None):
        if what is None: what = self.d.drivertype
        for i, d in enumerate(self._p.alldrivertypes):
            if d[0] == what: return i
        return len(self._p.alldrivertypes)

    def drivertype_fromindex(self):
        i = self.w.drivertype.get_active()
        if i < len(self._p.alldrivertypes): return self._p.alldrivertypes[i][1]
        return _("Other")

    def calculate_ideal_period(self):
        steptime = self.w.steptime.get_value()
        stepspace = self.w.stepspace.get_value()
        latency = self.w.latency.get_value()
        minperiod = self.d.minperiod(steptime, stepspace, latency)
        maxhz = int(1e9 / minperiod)
        if not self.d.doublestep(steptime): maxhz /= 2
        self.w.baseperiod.set_text("%d ns" % minperiod)
        self.w.maxsteprate.set_text("%d Hz" % maxhz)

    def update_drivertype_info(self):
        v = self.w.drivertype.get_active()
        if v < len(self._p.alldrivertypes):
            d = self._p.alldrivertypes[v]
            self.w.steptime.set_value(d[2])
            self.w.stepspace.set_value(d[3])
            self.w.dirhold.set_value(d[4])
            self.w.dirsetup.set_value(d[5])

            self.w.steptime.set_sensitive(0)
            self.w.stepspace.set_sensitive(0)
            self.w.dirhold.set_sensitive(0)
            self.w.dirsetup.set_sensitive(0)
        else:
            self.w.steptime.set_sensitive(1)
            self.w.stepspace.set_sensitive(1)
            self.w.dirhold.set_sensitive(1)
            self.w.dirsetup.set_sensitive(1)
        self.calculate_ideal_period()
        
    # parport io preset
    def find_parport(self):
        # Try to find parallel port
        lparport=[]
        # Try /proc/sys/dev/parport/parport#/base-addr
        parport_path="/proc/sys/dev/parport"
        if(os.path.isdir(parport_path)):
            parport_list=os.listdir(parport_path)
            for current_parport in parport_list:
                if(current_parport == "default"):
                    continue
                # find port number
                find_string="parport"
                if(current_parport.find(find_string) == 0):
                    try:
                        port_number=current_parport.split(find_string)[1]
                        lparport.append(port_number)
                    except:
                        continue
                # find base-addr file
                # Not used, but I want to be sure there is a real parport
                baseaddr=os.path.join(parport_path, current_parport, "base-addr")
                if(os.path.exists(baseaddr) == True):
                    try:
                        in_file = open(baseaddr,"r")
                    except:
                        print ("Unable to open %s" % baseaddr )
                        continue
                    # read base-addr file
                    try:
                        for line in in_file:
                            # get init_address
                            lline=line.split()
                            dec_address=lline[0].strip()
                            init_address=hex(int(dec_address))
                    except:
                        print ("Error read %s" % baseaddr)
                        in_file.close()
                        continue
                    in_file.close()
        if lparport == []:
            print ("No parport found")
            return([])
        return(lparport)

    # preset out pins
    def preset_sherline_outputs(self):
        self.w.pin2.set_active(1)
        self.w.pin3.set_active(0)
        self.w.pin4.set_active(3)
        self.w.pin5.set_active(2)
        self.w.pin6.set_active(5)
        self.w.pin7.set_active(4)
        self.w.pin8.set_active(7)
        self.w.pin9.set_active(6)

    def preset_xylotex_outputs(self):
        self.w.pin2.set_active(0)
        self.w.pin3.set_active(1)
        self.w.pin4.set_active(2)
        self.w.pin5.set_active(3)
        self.w.pin6.set_active(4)
        self.w.pin7.set_active(5)
        self.w.pin8.set_active(6)
        self.w.pin9.set_active(7)

    def preset_tb6560_3axes_outputs(self):
        SIG = self._p
        def index(signal):
            return self._p.hal_output_names.index(signal)
        # x axis
        self.w.pin1.set_active(index(SIG.XSTEP))
        self.w.pin16.set_active(index(SIG.XDIR))
        self.w.pin4.set_active(index(SIG.AMP))
        # Y axis
        self.w.pin14.set_active(index(SIG.YSTEP))
        self.w.pin7.set_active(index(SIG.YDIR))
        self.w.pin17.set_active(index(SIG.AMP))
        # Z axis
        self.w.pin3.set_active(index(SIG.ZSTEP))
        self.w.pin6.set_active(index(SIG.ZDIR))
        self.w.pin5.set_active(index(SIG.AMP))
        # spindle
        self.w.pin2.set_active(index(SIG.ON))

    def preset_tb6560_4axes_outputs(self):
        SIG = self._p
        def index(signal):
            return self._p.hal_output_names.index(signal)
        # x axis
        self.w.pin2.set_active(index(SIG.XSTEP))
        self.w.pin3.set_active(index(SIG.XDIR))
        self.w.pin1.set_active(index(SIG.AMP))
        # Y axis
        self.w.pin4.set_active(index(SIG.YSTEP))
        self.w.pin5.set_active(index(SIG.YDIR))
        # Z axis
        self.w.pin6.set_active(index(SIG.ZSTEP))
        self.w.pin7.set_active(index(SIG.ZDIR))
        # A axis
        self.w.pin8.set_active(index(SIG.ASTEP))
        self.w.pin9.set_active(index(SIG.ADIR))

    # check for spindle output signals
    def has_spindle_speed_control(self):
        d = self.d
        SIG = self._p

        # Check pp1 for output signals
        pp1_check = SIG.PWM in (d.pin1, d.pin2, d.pin3, d.pin4, d.pin5, d.pin6,
            d.pin7, d.pin8, d.pin9, d.pin14, d.pin16, d.pin17)
        if pp1_check is True: return True

        # now check port 2, which can be set to 'in' or 'out' mode: so can have
        # other pins number to check then pp1
        # output pins:
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            if d[p] == SIG.PWM: return True

        # if we get to here - there are no spindle control signals
        return False

    def has_spindle_encoder(self):
        SIG = self._p
        d = self.d

        # pp1 input pins
        if SIG.PPR in (d.pin10, d.pin11, d.pin12, d.pin13, d.pin15): return True
        if SIG.PHA in (d.pin10, d.pin11, d.pin12, d.pin13, d.pin15): return True

        # pp2 input pins
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            if d[p] in (SIG.PPR, SIG.PHA): return True

        # if we get to here - there are no spindle encoder signals
        return False

    # for Axis page calculation updates
    def update_pps(self, axis):
        def get(n):
            return float(self.w[axis + n].get_text())
        self.axis_sanity_test(axis)
        try:
            pitch = get("leadscrew")
            step = get("steprev")
            micro = get("microstep")
            pullnum = get("pulleynum")
            pulldem = get("pulleyden")
            if self.d.units == 1 or axis == 'a': pitch = 1./pitch
            pps = (pitch * step * micro * (pullnum / pulldem) * get("maxvel"))
            if pps == 0: raise ValueError
            pps = abs(pps)
            acctime = get("maxvel") / get("maxacc")
            accdist = acctime * .5 * get("maxvel")
            self.w[axis + "acctime"].set_text("%.4f" % acctime)
            self.w[axis + "accdist"].set_text("%.4f" % accdist)
            self.w[axis + "hz"].set_text("%.1f" % pps)
            scale = self.d[axis + "scale"] = (1.0 * pitch * step
                * micro * (pullnum / pulldem))
            self.w[axis + "scale"].set_text("%.1f" % scale)
            temp = "Axis Scale: %d × %d × (%.1f ÷ %.1f) × %.3f ="% (
                int(step),int(micro),(pullnum),(pulldem),pitch)
            self.w[axis + "scaledescr"].set_text(temp)
            self.p.set_buttons_sensitive(1,1)
            self.w[axis + "axistest"].set_sensitive(1)
        except (ValueError, ZeroDivisionError): # Some entries not numbers or not valid
            self.w[axis + "acctime"].set_text("")
            self.w[axis + "accdist"].set_text("")
            self.w[axis + "hz"].set_text("")
            self.w[axis + "scale"].set_text("")
            self.p.set_buttons_sensitive(0,0)
            self.w[axis + "axistest"].set_sensitive(0)

    def axis_sanity_test(self, axis):
        # I hate the inner function
        def get(n):
            return float(self.w[axis + n].get_text())

        # List of field with background color can change
        datalist = ('steprev','microstep','pulleynum','pulleyden','leadscrew',
                    'maxvel','maxacc')
        mystyle =""
        for i in datalist:
            # Damn! this is a bug. GTKBuilder sets the widget name to be the builder ID.
            widget_name = Gtk.Buildable.get_name(self.w[axis+i])
            try:
                a=get(i)
                if a <= 0:raise ValueError
            except:
                mystyle = mystyle + '#' + widget_name + ' { background-image: linear-gradient(90deg,yellow,red);}' + os.linesep
            else:
                mystyle = mystyle + '#' + widget_name + ' { background-image: linear-gradient(@bg_color,@bg_color); }' + os.linesep

        # Really I have not found a better way to change the background color
        # I hate the person who removed the get_background_color function in GTK3...
        provider = Gtk.CssProvider()
        provider.load_from_data(bytes(mystyle.encode()))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    # pport functions
    # disallow some signal combinations
    def do_exclusive_inputs(self, pin,port):
        # If initializing the Pport pages we don't want the signal calls to register here.
        # if we are working in here we don't want signal calls because of changes made in here
        # GTK supports signal blocking but then you can't assign signal names in GLADE -slaps head
        if self._p.in_pport_prepare or self.recursive_block: return
        self.recursive_block = True
        SIG = self._p
        exclusive = {
            SIG.HOME_X: (SIG.MAX_HOME_X, SIG.MIN_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_Y: (SIG.MAX_HOME_Y, SIG.MIN_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_Z: (SIG.MAX_HOME_Z, SIG.MIN_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_A: (SIG.MAX_HOME_A, SIG.MIN_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_TX: (SIG.MAX_HOME_TX, SIG.MIN_HOME_TX, SIG.BOTH_HOME_TX, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_TY: (SIG.MAX_HOME_TY, SIG.MIN_HOME_TY, SIG.BOTH_HOME_TY, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.MAX_HOME_X: (SIG.HOME_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_Y: (SIG.HOME_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_Z: (SIG.HOME_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_A: (SIG.HOME_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_TX: (SIG.HOME_TX, SIG.MIN_HOME_TX, SIG.MAX_HOME_TX, SIG.BOTH_HOME_TX, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_TY: (SIG.HOME_TY, SIG.MIN_HOME_TY, SIG.MAX_HOME_TY, SIG.BOTH_HOME_TY, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.MIN_HOME_X:  (SIG.HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_Y:  (SIG.HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_Z:  (SIG.HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_A:  (SIG.HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_TX:  (SIG.HOME_TX, SIG.MAX_HOME_TX, SIG.BOTH_HOME_TX, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_TY:  (SIG.HOME_TY, SIG.MAX_HOME_TY, SIG.BOTH_HOME_TY, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.BOTH_HOME_X:  (SIG.HOME_X, SIG.MAX_HOME_X, SIG.MIN_HOME_X, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_Y:  (SIG.HOME_Y, SIG.MAX_HOME_Y, SIG.MIN_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_Z:  (SIG.HOME_Z, SIG.MAX_HOME_Z, SIG.MIN_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_A:  (SIG.HOME_A, SIG.MAX_HOME_A, SIG.MIN_HOME_A, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_TX:  (SIG.HOME_TX, SIG.MAX_HOME_TX, SIG.MIN_HOME_TX, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_TY:  (SIG.HOME_TY, SIG.MAX_HOME_TY, SIG.MIN_HOME_TY, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.MIN_X: (SIG.BOTH_X, SIG.BOTH_HOME_X, SIG.MIN_HOME_X, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_Y: (SIG.BOTH_Y, SIG.BOTH_HOME_Y, SIG.MIN_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_Z: (SIG.BOTH_Z, SIG.BOTH_HOME_Z, SIG.MIN_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_A: (SIG.BOTH_A, SIG.BOTH_HOME_A, SIG.MIN_HOME_A, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_TX: (SIG.BOTH_TX, SIG.BOTH_HOME_TX, SIG.MIN_HOME_TX, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_TY: (SIG.BOTH_TY, SIG.BOTH_HOME_TY, SIG.MIN_HOME_TY, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),

            SIG.MAX_X: (SIG.BOTH_X, SIG.BOTH_HOME_X, SIG.MIN_HOME_X, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_Y: (SIG.BOTH_Y, SIG.BOTH_HOME_Y, SIG.MIN_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_Z: (SIG.BOTH_Z, SIG.BOTH_HOME_Z, SIG.MIN_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_A: (SIG.BOTH_A, SIG.BOTH_HOME_A, SIG.MIN_HOME_A, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_TX: (SIG.BOTH_TX, SIG.BOTH_HOME_TX, SIG.MIN_HOME_TX, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_TY: (SIG.BOTH_TY, SIG.BOTH_HOME_TY, SIG.MIN_HOME_TY, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),

            SIG.BOTH_X: (SIG.MIN_X, SIG.MAX_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_Y: (SIG.MIN_Y, SIG.MAX_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_Z: (SIG.MIN_Z, SIG.MAX_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_A: (SIG.MIN_A, SIG.MAX_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_TX: (SIG.MIN_TX, SIG.MAX_TX, SIG.MIN_HOME_TX, SIG.MAX_HOME_TX, SIG.BOTH_HOME_TX, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_TY: (SIG.MIN_TY, SIG.MAX_TY, SIG.MIN_HOME_TY, SIG.MAX_HOME_TY, SIG.BOTH_HOME_TY, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),

            SIG.ALL_LIMIT: (
                SIG.MIN_X, SIG.MAX_X, SIG.BOTH_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.MIN_Y, SIG.MAX_Y, SIG.BOTH_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.MIN_Z, SIG.MAX_Z, SIG.BOTH_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.MIN_A, SIG.MAX_A, SIG.BOTH_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.MIN_TX, SIG.MAX_TX, SIG.BOTH_TX, SIG.MIN_HOME_TX, SIG.MAX_HOME_TX, SIG.BOTH_HOME_TX,
                SIG.MIN_TY, SIG.MAX_TY, SIG.BOTH_TY, SIG.MIN_HOME_TY, SIG.MAX_HOME_TY, SIG.BOTH_HOME_TY,
                SIG.ALL_LIMIT_HOME),
            SIG.ALL_HOME: (
                SIG.HOME_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.HOME_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.HOME_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.HOME_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.HOME_TX, SIG.MIN_HOME_TX, SIG.MAX_HOME_TX, SIG.BOTH_HOME_TX,
                SIG.HOME_TY, SIG.MIN_HOME_TY, SIG.MAX_HOME_TY, SIG.BOTH_HOME_TY,
                SIG.ALL_LIMIT_HOME),
            SIG.ALL_LIMIT_HOME: (
                SIG.HOME_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.HOME_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.HOME_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.HOME_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.HOME_TX, SIG.MIN_HOME_TX, SIG.MAX_HOME_TX, SIG.BOTH_HOME_TX,
                SIG.HOME_TY, SIG.MIN_HOME_TY, SIG.MAX_HOME_TY, SIG.BOTH_HOME_TY,
                SIG.MIN_X, SIG.MAX_X, SIG.BOTH_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.MIN_Y, SIG.MAX_Y, SIG.BOTH_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.MIN_Z, SIG.MAX_Z, SIG.BOTH_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.MIN_A, SIG.MAX_A, SIG.BOTH_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.MIN_TX, SIG.MAX_TX, SIG.BOTH_TX, SIG.MIN_HOME_TX, SIG.MAX_HOME_TX, SIG.BOTH_HOME_TX,
                SIG.MIN_TY, SIG.MAX_TY, SIG.BOTH_TY, SIG.MIN_HOME_TY, SIG.MAX_HOME_TY, SIG.BOTH_HOME_TY,
                SIG.ALL_LIMIT, SIG.ALL_HOME),
        }
        v = pin.get_active()
        name = self._p.hal_input_names[v]
        ex = exclusive.get(name, ())
        # search pport1 for the illegal signals and change them to unused.
        dbg( 'looking for %s in pport1'%name)
        for pin1 in (10,11,12,13,15):
            p = 'pin%d' % pin1
            if self.w[p] == pin: continue
            v1 = self._p.hal_input_names[self.w[p].get_active()]
            if v1 in ex or v1 == name:
                dbg( 'found %s, at %s'%(name,p))
                self.w[p].set_active(self._p.hal_input_names.index(SIG.UNUSED_INPUT))
                if not port ==1: # if on the other page must change the data model too
                    dbg( 'found on other pport page')
                    self.d[p] = SIG.UNUSED_INPUT
        # search pport2 for the illegal signals and change them to unused.
        dbg( 'looking for %s in pport2'%name)
        for pin1 in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p2 = 'pp2_pin%d_in' % pin1
            if self.w[p2] == pin: continue
            v2 = self._p.hal_input_names[self.w[p2].get_active()]
            if v2 in ex or v2 == name:
                dbg( 'found %s, at %s'%(name,p2))
                self.w[p2].set_active(self._p.hal_input_names.index(SIG.UNUSED_INPUT))
                if not port ==2:# if on the other page must change the data model too
                    dbg( 'found on other pport page')
                    self.d[p2] = SIG.UNUSED_INPUT
        self.recursive_block = False
#**************
# Latency test
#**************
    def run_latency_test(self):
        self.latency_pid = os.spawnvp(os.P_NOWAIT,
                                "latency-test", ["latency-test"])
        self.w['window1'].set_sensitive(0)
        GLib.timeout_add(15, self.latency_running_callback)

    def latency_running_callback(self):
        pid, status = os.waitpid(self.latency_pid, os.WNOHANG)
        if pid:
            self.w['window1'].set_sensitive(1)
            return False
        return True

#***************
# PYVCP TEST
#***************
    def testpanel(self,w):
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        if self.w.radiobutton5.get_active() == True:
            print('no sample requested')
            return True
        if self.w.radiobutton6.get_active() == True:
            panel = "spindle.xml"
        if self.w.radiobutton8.get_active() == True:
            panel = "custompanel.xml"
            panelname = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
        halrun = os.popen("cd %(panelname)s\nhalrun -Is > /dev/null"% {'panelname':panelname,}, "w" )    
        halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
        if self.w.radiobutton6.get_active() == True:
            halrun.write("setp displaytest.spindle-speed 1000\n")
        halrun.write("waitusr displaytest\n")
        halrun.flush()
        halrun.close()   

#**************
# LADDER TEST
#**************
    def load_ladder(self,w):         
        newfilename = os.path.join(distdir, "configurable_options/ladder/TEMP.clp")    
        self.d.modbus = self.w.modbus.get_active()
        self.halrun = halrun = os.popen("halrun -Is", "w")
        halrun.write(""" 
              loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n
              loadrt classicladder_rt numPhysInputs=%(din)d numPhysOutputs=%(dout)d numS32in=%(sin)d numS32out=%(sout)d\
                     numFloatIn=%(fin)d numFloatOut=%(fout)d\n
              addf classicladder.0.refresh slow\n
              start\n
                      """ % {
                      'period': 50000,
                      'din': self.w.digitsin.get_value(),
                      'dout': self.w.digitsout.get_value(),
                      'sin': self.w.s32in.get_value(),
                      'sout': self.w.s32out.get_value(), 
                      'fin':self.w.floatsin.get_value(),
                      'fout':self.w.floatsout.get_value(),
                 })
        if self.w.radiobutton1.get_active() == True:
            if self.d.tempexists:
               self.d.laddername='TEMP.clp'
            else:
               self.d.laddername= 'blank.clp'
        if self.w.radiobutton2.get_active() == True:
            self.d.laddername= 'estop.clp'
        if self.w.radiobutton3.get_active() == True:
            self.d.laddername = 'serialmodbus.clp'
            self.d.modbus = True
            self.w.modbus.set_active(self.d.modbus)
        if self.w.radiobutton4.get_active() == True:
            self.d.laddername='custom.clp'
            originalfile = filename = os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)
        else:
            filename = os.path.join(distdir, "configurable_options/ladder/"+ self.d.laddername)        
        if self.d.modbus == True: 
            halrun.write("loadusr -w classicladder --modmaster --newpath=%(newfilename)s %(filename)s\
                \n" %          { 'newfilename':newfilename ,'filename':filename })
        else:
            halrun.write("loadusr -w classicladder --newpath=%(newfilename)s %(filename)s\n" % { 'newfilename':newfilename ,'filename':filename })
        halrun.flush()
        halrun.close()
        if os.path.exists(newfilename):
            self.d.tempexists = True
            self.w.newladder.set_text('Edited ladder program')
            self.w.radiobutton1.set_active(True)
        else:
            self.d.tempexists = 0

#**********
# Axis Test
#***********
    def test_axis(self, axis):
        if not self.check_for_rt(): return
        SIG = self._p

        vel = float(self.w[axis + "maxvel"].get_text())
        acc = float(self.w[axis + "maxacc"].get_text())

        scale = self.d[axis + "scale"]
        maxvel = 1.5 * vel
        if self.d.doublestep():
                period = int(1e9 / maxvel / scale)
        else:
                period = int(.5e9 / maxvel / scale)

        steptime = self.w.steptime.get_value()
        stepspace = self.w.stepspace.get_value()
        latency = self.w.latency.get_value()
        minperiod = self.d.minperiod()

        if period < minperiod:
            period = minperiod
            if self.d.doublestep():
                maxvel = 1e9 / minperiod / abs(scale)
            else:
                maxvel = 1e9 / minperiod / abs(scale)
        if period > 100000:
            period = 100000

        self.halrun = halrun = os.popen("halrun -Is", "w")
        if debug:
            halrun.write("echo\n")
        axnum = "xyza".index(axis)
        step = axis + "step"
        dir = axis + "dir"

        halrun.write("""
            loadrt steptest
            loadrt stepgen step_type=0
            """)

        port3name=port2name=port2dir=port3dir=""
        if self.d.number_pports>2:
             port3name = ' '+self.d.ioaddr3
             if self.d.pp3_direction: # Input option
                port3dir =" in"
             else: 
                port3dir =" out"
        if self.d.number_pports>1:
             port2name = ' '+self.d.ioaddr2
             if self.d.pp2_direction: # Input option
                port2dir =" in"
             else: 
                port2dir =" out"
        halrun.write( "loadrt hal_parport cfg=\"%s out%s%s%s%s\"\n" % (self.d.ioaddr, port2name, port2dir, port3name, port3dir))
        halrun.write("""
            loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow
            addf stepgen.make-pulses fast
            addf parport.0.write fast
            """%{'period': period})

        if self.d.number_pports>1:
            halrun.write( "addf parport.0.write fast\n")
        if self.d.number_pports>2:
            halrun.write( "addf parport.0.write fast\n")
        temp = self.find_output(axis +'step')
        step_pin = temp[0][0]
        temp = self.find_output(axis +'dir')
        dir_pin = temp[0][0] 
        halrun.write("""
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
            'steppin': step_pin,
            'dirpin': dir_pin,
            'dirhold': self.d.dirhold + self.d.latency,
            'dirsetup': self.d.dirsetup + self.d.latency,
            'onestep': abs(1. / self.d[axis + "scale"]),
            'scale': self.d[axis + "scale"],
        })

        if self.doublestep():
            halrun.write("""
                setp parport.0.reset-time %(resettime)d
                setp stepgen.0.stepspace 0
                addf parport.0.reset fast
            """ % {
                'resettime': self.d['steptime']
            })
        amp_signals = self.find_output(SIG.AMP)
        for pin in amp_signals:
            amp,amp_port = pin
            halrun.write("setp parport.%(portnum)d.pin-%(enablepin)02d-out 1\n"
                % {'enablepin': amp,'portnum': amp_port})

        estop_signals = self.find_output(SIG.ESTOP)
        for pin in estop_signals:
            estop,e_port = pin
            halrun.write("setp parport.%(portnum)d.pin-%(estoppin)02d-out 1\n"
                % {'estoppin': estop,'portnum': e_port})

        for pin in 1,2,3,4,5,6,7,8,9,14,16,17:
            inv = getattr(self.d, "pin%dinv" % pin)
            if inv:
                halrun.write("setp parport.0.pin-%(pin)02d-out-invert 1\n"
                    % {'pin': pin})
        if self.d.number_pports > 1:
            if self.d.pp2_direction:# Input option
                out_list =(1,14,16,17)
            else:
                out_list =(1,2,3,4,5,6,7,8,9,14,16,17)
            for pin in (out_list):
                inv = getattr(self.d, "pp2_pin%dinv" % pin)
                if inv:
                    halrun.write("setp parport.1.pin-%(pin)02d-out-invert 1\n"
                    % {'pin': pin})
        if debug:
            halrun.write("loadusr halmeter sig cmd -g 275 415\n")

        self.w.dialog1.set_title(_("%s Axis Test") % axis.upper())

        if axis == "a":
            self.w.testvelunit.set_text(_("deg / s"))
            self.w.testaccunit.set_text(_(u"deg / s²"))
            self.w.testampunit.set_text(_("deg"))
            self.w.testvel.set_increments(1,5)
            self.w.testacc.set_increments(1,5)
            self.w.testamplitude.set_increments(1,5)
            self.w.testvel.set_range(0, maxvel)
            self.w.testacc.set_range(1, 360000)
            self.w.testamplitude.set_range(0, 1440)
            self.w.testvel.set_digits(1)
            self.w.testacc.set_digits(1)
            self.w.testamplitude.set_digits(1)
            self.w.testamplitude.set_value(10)
        elif self.d.units:
            self.w.testvelunit.set_text(_("mm / s"))
            self.w.testaccunit.set_text(_(u"mm / s²"))
            self.w.testampunit.set_text(_("mm"))
            self.w.testvel.set_increments(1,5)
            self.w.testacc.set_increments(1,5)
            self.w.testamplitude.set_increments(1,5)
            self.w.testvel.set_range(0, maxvel)
            self.w.testacc.set_range(1, 100000)
            self.w.testamplitude.set_range(0, 1000)
            self.w.testvel.set_digits(2)
            self.w.testacc.set_digits(2)
            self.w.testamplitude.set_digits(2)
            self.w.testamplitude.set_value(15)
        else:
            self.w.testvelunit.set_text(_("in / s"))
            self.w.testaccunit.set_text(_(u"in / s²"))
            self.w.testampunit.set_text(_("in"))
            self.w.testvel.set_increments(.1,5)
            self.w.testacc.set_increments(1,5)
            self.w.testamplitude.set_increments(.1,5)
            self.w.testvel.set_range(0, maxvel)
            self.w.testacc.set_range(1, 3600)
            self.w.testamplitude.set_range(0, 36)
            self.w.testvel.set_digits(1)
            self.w.testacc.set_digits(1)
            self.w.testamplitude.set_digits(1)
            self.w.testamplitude.set_value(.5)

        self.jogplus = self.jogminus = 0
        self.w.testdir.set_active(0)
        self.w.run.set_active(0)
        self.w.testacc.set_value(acc)
        self.w.testvel.set_value(vel)
        self.axis_under_test = axis
        self.update_axis_test()

        halrun.write("start\n"); halrun.flush()
        self.w.dialog1.show_all()
        result = self.w.dialog1.run()
        self.w.dialog1.hide()
        
        if amp_signals:
            for pin in amp_signals:
                amp,amp_port = pin
                halrun.write("setp parport.%(portnum)d.pin-%(enablepin)02d-out 0\n"
                % {'enablepin': amp,'portnum': amp_port})
        if estop_signals:
            for pin in estop_signals:
                estop,e_port = pin
                halrun.write("setp parport.%(portnum)d.pin-%(estoppin)02d-out 0\n"
                % {'estoppin': estop,'portnum': e_port})

        time.sleep(.001)
        halrun.close()

    def update_axis_test(self, *args):
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
            'run': self.w.run.get_active(),
            'amplitude': self.w.testamplitude.get_value(),
            'accel': self.w.testacc.get_value(),
            'vel': self.w.testvel.get_value(),
            'dir': self.w.testdir.get_active(),
        })
        halrun.flush()

#**********************************
# Common helper functions
#**********************************

    def build_input_set(self):
        input_set =(self.d.pin10,self.d.pin11,self.d.pin12,self.d.pin13,self.d.pin15)
        if self.d.number_pports > 1:
            if self.d.pp2_direction:# Input option
                in_list =(2,3,4,5,6,7,8,9,10,11,12,13,15)
            else:
                in_list =(10,11,12,13,15)
            for pin in (in_list):
                p = 'pp2_pin%d_in' % pin
                input_set +=(self.d[p],)
        return set(input_set)

    def build_output_set(self):
        output_set =(self.d.pin1, self.d.pin2, self.d.pin3, self.d.pin4, self.d.pin5,
            self.d.pin6, self.d.pin7, self.d.pin8, self.d.pin9, self.d.pin14, self.d.pin16,
            self.d.pin17)
        if self.d.number_pports > 1:
            if self.d.pp2_direction:# Input option
                out_list =(1,14,16,17)
            else:
                out_list =(1,2,3,4,5,6,7,8,9,14,16,17)
            for pin in (out_list):
                p = 'pp2_pin%d' % pin
                output_set += (self.d[p],)
        return set(output_set)

    def find_input(self, input):
        inputs = set((10, 11, 12, 13, 15))
        for i in inputs:
            pin = getattr(self.d, "pin%d" % i)
            inv = getattr(self.d, "pin%dinv" % i)
            if pin == input: return i
        return None

    def find_output(self, output):
        found_list = []
        out_list = set((1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 16, 17))
        port = 0
        for i in out_list:
            pin = self.d["pin%d" % i]
            inv = self.d["pin%dinv" % i]
            if pin == output: found_list.append((i,port))
        if self.d.number_pports > 1:
            port = 1
            if self.d.pp2_direction:# Input option
                out_list =(1,14,16,17)
            else:
                out_list =(1,2,3,4,5,6,7,8,9,14,16,17)
            for i in (out_list):
                pin = self.d['pp2_pin%d' % i]
                if pin == output: found_list.append((i,port))
        return found_list

    def doublestep(self, steptime=None):
        if steptime is None: steptime = self.d.steptime
        return steptime <= 5000

    def home_sig(self, axis):
        SIG = self._p
        inputs = self.build_input_set()
        thisaxishome = set((SIG.ALL_HOME, SIG.ALL_LIMIT_HOME, "home-" + axis, "min-home-" + axis,
                            "max-home-" + axis, "both-home-" + axis))
        for i in inputs:
            if i in thisaxishome: return i

# Boiler code
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# starting with 'stepconf -d' gives debug messages
if __name__ == "__main__":
    def dummy():
        pass
    usage = "usage: Stepconf -[options]"
    parser = OptionParser(usage=usage)
    parser.add_option("-d", action="store_true", dest="debug",help="Print debug info and ignore realtime/kernel tests")
    (options, args) = parser.parse_args()
    if options.debug:
        app = StepconfApp(dbgstate=True)
    else:
        app = StepconfApp(False)
    signal.signal(signal.SIGINT, lambda *args: dummy())
    Gtk.main()

