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

import gtk
import gtk.glade
import gnome.ui

import xml.dom.minidom

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
    distdir = "/etc/emc2/sample-configs/common"


(ESTOP, PROBE, PPR, PHA, PHB,
HOME_X, HOME_Y, HOME_Z, HOME_A,
MIN_HOME_X, MIN_HOME_Y, MIN_HOME_Z, MIN_HOME_A,
MAX_HOME_X, MAX_HOME_Y, MAX_HOME_Z, MAX_HOME_A,
BOTH_HOME_X, BOTH_HOME_Y, BOTH_HOME_Z, BOTH_HOME_A,
MIN_X, MIN_Y, MIN_Z, MIN_A,
MAX_X, MAX_Y, MAX_Z, MAX_A,
BOTH_X, BOTH_Y, BOTH_Z, BOTH_A,
ALL_LIMIT, ALL_HOME, UNUSED_INPUT) = range(36)

(XSTEP, XDIR, YSTEP, YDIR,
ZSTEP, ZDIR, ASTEP, ADIR, CW, CCW, PWM,
MIST, FLOOD, ESTOP, AMP, PUMP, UNUSED_OUTPUT) = range(17)

hal_output_names = ("xstep", "xdir", "ystep", "ydir",
"zstep", "zdir", "astep", "adir",
"spindle-cw", "spindle-ccw", "spindle-pwm",
"coolant-mist", "coolant-flood", "estop-out", "xenable",
"charge-pump")

hal_input_names = ("estop-ext", "probe-in",
"spindle-index", "spindle-phase-a", "spindle-phase-b",
"home-x", "home-y", "home-z", "home-a",
"min-home-x", "min-home-y", "min-home-z", "min-home-a",
"max-home-x", "max-home-y", "max-home-z", "max-home-a",
"both-home-x", "both-home-y", "both-home-z", "both-home-a",
"min-x", "min-y", "min-z", "min-a",
"max-x", "max-y", "max-z", "max-a",
"both-x", "both-y", "both-z", "both-a",
"all-limit", "all-home")

human_output_names = (_("X Step"), _("X Direction"), _("Y Step"), _("Y Direction"),
_("Z Step"), _("Z Direction"), _("A Step"), _("A Direction"),
_("Spindle CW"), _("Spindle CCW"), _("Spindle PWM"),
_("Coolant Mist"), _("Coolant Flood"), _("ESTOP Out"), _("Amplifier Enable"),
_("Charge Pump"), _("Unused"))

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
_("All limits"), _("All home"), _("Unused"))

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
	self.drivertype = 5 # Other
	self.steptime = self.stepspace = 5000
	self.dirhold = self.dirsetup = 20000
        self.latency = 15000
        self.period = 25000

        self.ioaddr = "0x378"

	self.manualtoolchange = 1
	self.customhal = 1
	self.pyvcp = 0

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

	self.pin10 = BOTH_HOME_X
	self.pin11 = BOTH_HOME_Y
	self.pin12 = BOTH_HOME_Z
	self.pin13 = BOTH_HOME_A
	self.pin15 = PROBE

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
	    if m != m1:
		warnings.append(_("File %r was modified since it was written by stepconf") % f)
	if not warnings: return

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
	print >>file, "NML_FILE = emc.nml"
	print >>file, "DEBUG = 0"

	print >>file
	print >>file, "[DISPLAY]"
	print >>file, "DISPLAY = axis"
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
            print >>file, "PYVCP = panel.xml"

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
	print >>file, "SHMEM_KEY = 111"
	print >>file, "COMM_TIMEOUT = 1.0"
	print >>file, "COMM_WAIT = 0.010"
	print >>file, "BASE_PERIOD = %d" % base_period
	print >>file, "SERVO_PERIOD = 1000000"

	print >>file
	print >>file, "[HAL]"
	print >>file, "HALFILE = %s.hal" % self.machinename
	if self.customhal:
	    print >>file, "HALFILE = custom.hal"
	    print >>file, "POSTGUI_HALFILE = custom_postgui.hal"

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

	self.write_one_axis(file, 0, "x", "LINEAR")
	if self.axes != 2:
	    self.write_one_axis(file, 1, "y", "LINEAR")
	self.write_one_axis(file, 2, "z", "LINEAR")
	if self.axes == 1:
	    self.write_one_axis(file, 3, "a", "ANGULAR")

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
        return pps

    def minperiod(self):
        return self.latency + self.steptime + self.stepspace + 5000
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

    def write_one_axis(self, file, num, letter, type):
	order = "1203"
	def get(s): return self[letter + s]
	print >>file
	print >>file, "[AXIS_%d]" % num
	print >>file, "TYPE = %s" % type
	print >>file, "HOME = %s" % get("homepos")
	print >>file, "MAX_VELOCITY = %s" % get("maxvel")
	print >>file, "MAX_ACCELERATION = %s" % get("maxacc")
	print >>file, "STEPGEN_MAXACCEL = %s" % (1.05 * get("maxacc"))
	print >>file, "SCALE = %s" % get("scale")
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
	thisaxishome = set((ALL_HOME, HOME_X + num, MIN_HOME_X + num,
			    MAX_HOME_X + num, BOTH_HOME_X + num))
	ignore = set((MIN_HOME_X + num, MAX_HOME_X + num, BOTH_HOME_X + num))
	homes = bool(inputs & thisaxishome)
    
	if homes:
	    print >>file, "HOME_OFFSET = %f" % get("homesw")
	    print >>file, "SEARCH_VEL = %f" % get("homevel")
	    latchvel = get("homevel") / abs(get("homevel"))
	    if get("latchdir"): latchvel = -latchvel
	    latchvel = latchvel * 1000 / get("scale")
	    print >>file, "LATCH_VEL = %f" % latchvel
	    if inputs & ignore:
		print >>file, "HOME_IGNORE_LIMITS = YES"
	    print >>file, "HOME_SEQUENCE = %s" % order[num]
	else:
	    print >>file, "HOME_OFFSET = %s" % get("homepos")

    def home_sig(self, axnum):
	inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
	thisaxishome = set((ALL_HOME, HOME_X + axnum, MIN_HOME_X + axnum,
			    MAX_HOME_X + axnum, BOTH_HOME_X + axnum))
	for i in inputs:
	    if i in thisaxishome: return hal_input_names[i]

    def min_lim_sig(self, axnum):
   	inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
	thisaxishome = set((ALL_LIMIT, MIN_X + axnum, MIN_HOME_X + axnum,
			    BOTH_HOME_X + axnum))
	for i in inputs:
	    if i in thisaxishome: return hal_input_names[i]

    def max_lim_sig(self, axnum):
   	inputs = set((self.pin10,self.pin11,self.pin12,self.pin13,self.pin15))
	thisaxishome = set((ALL_LIMIT, MAX_X + axnum, MAX_HOME_X + axnum,
			    BOTH_HOME_X + axnum))
	for i in inputs:
	    if i in thisaxishome: return hal_input_names[i]
 
    def connect_axis(self, file, num, let):
	axnum = "xyza".index(let)
        lat = self.latency
	print >>file
	print >>file, "setp stepgen.%d.position-scale [AXIS_%d]SCALE" % (num, axnum)
	print >>file, "setp stepgen.%d.steplen 1" % num
	print >>file, "setp stepgen.%d.stepspace 0" % num
	print >>file, "setp stepgen.%d.dirhold %d" % (num, self.dirhold + lat)
	print >>file, "setp stepgen.%d.dirsetup %d" % (num, self.dirsetup + lat)
	print >>file, "setp stepgen.%d.maxaccel [AXIS_%d]STEPGEN_MAXACCEL" % (num, axnum)
	print >>file, "net %spos-cmd axis.%d.motor-pos-cmd => stepgen.%d.position-cmd" % (let, axnum, num)
	print >>file, "net %spos-fb stepgen.%d.position-fb => axis.%d.motor-pos-fb" % (let, num, axnum)
	print >>file, "net %sstep <= stepgen.%d.step" % (let, num)
	print >>file, "net %sdir <= stepgen.%d.dir" % (let, num)
	print >>file, "net %senable axis.%d.amp-enable-out => stepgen.%d.enable" % (let, axnum, num)
	homesig = self.home_sig(axnum)
	if homesig:
	    print >>file, "net %s => axis.%d.home-sw-in" % (homesig, num)
	min_limsig = self.min_lim_sig(axnum)
	if min_limsig:
	    print >>file, "net %s => axis.%d.neg-lim-sw-in" % (min_limsig, num)
	max_limsig = self.max_lim_sig(axnum)
	if max_limsig:
	    print >>file, "net %s => axis.%d.pos-lim-sw-in" % (max_limsig, num)

    def connect_input(self, file, num):
	p = self['pin%d' % num]
	i = self['pin%dinv' % num]
	if p == UNUSED_INPUT: return

	if i:
	    print >>file, "net %s <= parport.0.pin-%02d-in-not" \
		% (hal_input_names[p], num)
	else:
	    print >>file, "net %s <= parport.0.pin-%02d-in" \
		% (hal_input_names[p], num)

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
	print >>file, "net %s => parport.0.pin-%02d-out" % (hal_output_names[p], num)
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
	print >>file, "loadrt [EMCMOT]EMCMOT base_period_nsec=[EMCMOT]BASE_PERIOD servo_period_nsec=[EMCMOT]SERVO_PERIOD traj_period_nsec=[EMCMOT]SERVO_PERIOD key=[EMCMOT]SHMEM_KEY num_joints=[TRAJ]AXES"
	print >>file, "loadrt probe_parport"
	print >>file, "loadrt hal_parport cfg=%s" % self.ioaddr
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

	if pump:
	    print >>file, "loadrt charge_pump"
	    print >>file, "net estop-out charge-pump.enable <= iocontrol.0.user-enable-out"

	if pwm:
	    print >>file, "loadrt pwmgen output_type=0"

	print >>file
	print >>file, "addf parport.0.read base-thread"
	print >>file, "addf stepgen.make-pulses base-thread"
	if encoder: print >>file, "addf encoder.update-counters base-thread"
	if pump: print >>file, "addf charge-pump base-thread"
	if pwm: print >>file, "addf pwmgen.make-pulses base-thread"
	print >>file, "addf parport.0.write base-thread"
	print >>file, "addf parport.0.reset base-thread"

	print >>file
	print >>file, "addf stepgen.capture-position servo-thread"
	if encoder: print >>file, "addf encoder.capture-position servo-thread"
	print >>file, "addf motion-command-handler servo-thread"
	print >>file, "addf motion-controller servo-thread"
	print >>file, "addf stepgen.update-freq servo-thread"
	if pwm: print >>file, "addf pwmgen.update servo-thread"

	if pwm:
	    x1 = self.spindlepwm1
	    x2 = self.spindlepwm2
	    y1 = self.spindlespeed1
	    y2 = self.spindlespeed2
	    scale = (y2-y1) / (x2-x1)
	    offset = y1 - x1 * scale
	    print >>file
	    print >>file, "net spindle-cmd <= motion.spindle-speed-in => pwmgen.0.value"
	    print >>file, "net spindle-enable <= motion.spindle-on => pwmgen.0.enable"
	    print >>file, "net spindle-pwm <= pwmgen.0.pwm"
	    print >>file, "setp pwmgen.0.pwm-freq %s" % self.spindlecarrier	
	    print >>file, "setp pwmgen.0.scale %s" % scale
	    print >>file, "setp pwmgen.0.offset %s" % offset

        if CW in outputs:
            print >>file, "net spindle-cw <= motion.spindle-forward"
        if CCW in outputs:
            print >>file, "net spindle-ccw <= motion.spindle-reverse"

	if encoder:
	    print >>file
	    if PHB not in inputs:
		print >>file, "setp encoder.0.position-scale %f"\
		     % (1. / 2 / int(self.spindlecpr))
		print >>file, "setp encoder.0.counter-mode 1"
	    else:
		print >>file, "setp encoder.0.position-scale %f" \
		    % (1. / 4 / int(self.spindlecpr))
	    print >>file, "net spindle-position encoder.0.position => motion.spindle-revs"
	    print >>file, "net spindle-velocity encoder.0.velocity => motion.spindle-speed-in"
	    print >>file, "net spindle-index-enable encoder.0.index-enable <=> motion.spindle-index-enable"

	if probe:
	    print >>file
	    print >>file, "net probe-in => motion.probe-input"

	print >>file
	for o in (1,2,3,4,5,6,7,8,9,14,16,17): self.connect_output(file, o)

	print >>file
	for i in (10,11,12,13,15): self.connect_input(file, i)

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
	if ESTOP in inputs:
	    print >>file, "net estop-in => iocontrol.0.emc-enable-in"
	else:
	    print >>file, "net estop-out => iocontrol.0.emc-enable-in"

	print >>file
	if self.manualtoolchange:
	    print >>file, "loadusr -W hal_manualtoolchange"
	    print >>file, "net tool-change iocontrol.0.tool-change => hal_manualtoolchange.change"
	    print >>file, "net tool-changed iocontrol.0.tool-changed <= hal_manualtoolchange.changed"
	    print >>file, "net tool-number iocontrol.0.tool-prep-number => hal_manualtoolchange.number"

	else:
	    print >>file, "net tool-change-loopback iocontrol.0.tool-change => icontrol.0.tool-changed"
	print >>file, "net tool-prepare-loopback iocontrol.0.tool-prepare => iocontrol.0.tool-prepared"

        if self.pyvcp:
	    vcp = os.path.join(base, "panel.xml")
            if not os.path.exists(vcp):
                f1 = open(vcp, "w")

                print >>f1, "<?xml version='1.0' encoding='UTF-8'?>"

                print >>f1, "<!-- "
                print >>f1, _("Include your PyVCP panel here.\nThe contents of this file will not be overwritten when you run stepconf again.")
                print >>f1, "-->"
                print >>f1, "<pyvcp>"
                print >>f1, "</pyvcp>"
        if self.pyvcp or self.customhal:
	    custom = os.path.join(base, "custom_postgui.hal")
	    if not os.path.exists(custom):
		f1 = open(custom, "w")
		print >>f1, _("# Include your customized HAL commands here")
                print >>f1, _("""\
# The commands in this file are run after the AXIS GUI (including PyVCP panel)
# starts
# This file will not be overwritten when you run stepconf again""")
	if self.customhal:
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
	makedirs(base)

	self.md5sums = []
	self.write_readme(base)
	self.write_inifile(base)
	self.write_halfile(base)
	self.copy(base, "emc.nml")
	self.copy(base, "tool.tbl")
	self.copy(base, "emc.var")

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
	self.widgets.druidpagestart1.show()
        self.widgets.druidpagestart1.set_watermark(self.watermark)
	self.widgets.complete.show()
        self.widgets.complete.set_watermark(self.watermark)
	
	self.xml.signal_autoconnect(self)

	self.in_pport_prepare = False
	self.axis_under_test = False
        self.jogminus = self.jogplus = 0

	self.data = Data()

    def gtk_main_quit(self, *args):
	gtk.main_quit()

    def on_window1_delete_event(self, *args):
        dialog = gtk.MessageDialog(self.widgets.window1,
	    gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
	    gtk.MESSAGE_QUESTION, gtk.BUTTONS_YES_NO,
		 _("Quit Stepconf and discard changes?"))
	dialog.show_all()
	result = dialog.run()
	dialog.destroy()
	if result == gtk.RESPONSE_YES:
	    gtk.main_quit()
	    return False
	else:
	    return True
    on_druid1_cancel = on_window1_delete_event

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

    def on_basicinfo_prepare(self, *args):
	self.widgets.machinename.set_text(self.data.machinename)
	self.widgets.axes.set_active(self.data.axes)
	self.widgets.units.set_active(self.data.units)
	self.widgets.latency.set_value(self.data.latency)
	self.widgets.steptime.set_value(self.data.steptime)
	self.widgets.stepspace.set_value(self.data.stepspace)
	self.widgets.dirsetup.set_value(self.data.dirsetup)
	self.widgets.dirhold.set_value(self.data.dirhold)
	self.widgets.drivertype.set_active(self.data.drivertype)
	self.widgets.manualtoolchange.set_active(self.data.manualtoolchange)
	self.widgets.ioaddr.set_text(self.data.ioaddr)
	self.widgets.machinename.grab_focus()

    def on_basicinfo_next(self, *args):
	self.data.machinename = self.widgets.machinename.get_text()
	self.data.axes = self.widgets.axes.get_active()
	self.data.units = self.widgets.units.get_active()
	self.data.drivertype = self.widgets.drivertype.get_active()
	self.data.steptime = self.widgets.steptime.get_value()
	self.data.stepspace = self.widgets.stepspace.get_value()
	self.data.dirsetup = self.widgets.dirsetup.get_value()
	self.data.dirhold = self.widgets.dirhold.get_value()
	self.data.latency = self.widgets.latency.get_value()
	self.data.manualtoolchange = self.widgets.manualtoolchange.get_active()
	self.data.ioaddr = self.widgets.ioaddr.get_text()

    def on_machinename_changed(self, *args):
	self.widgets.confdir.set_text(
	    "~/emc2/configs/%s" % self.widgets.machinename.get_text())

    def on_drivertype_changed(self, *args):
	drive_characteristics = [
	    [4000, 500, 20000, 1000],     # Gecko
	    [500,  4000, 4000, 1000],     # L297   XXX active low
	    [1000, 2000, 1000, 1000],     # PMDX-150
	    [1000, 6000, 24000, 20000],   # Sherline  XXX find proper values
	    [1000, 2000, 200, 200],       # Xylotex
	]
	v = self.widgets.drivertype.get_active()
	if v < len(drive_characteristics):
	    d = drive_characteristics[v]
	    self.widgets.steptime.set_value(d[0])
	    self.widgets.stepspace.set_value(d[1])
	    self.widgets.dirhold.set_value(d[2])
	    self.widgets.dirsetup.set_value(d[3])

	    self.widgets.steptime.set_sensitive(0)
	    self.widgets.stepspace.set_sensitive(0)
	    self.widgets.dirhold.set_sensitive(0)
	    self.widgets.dirsetup.set_sensitive(0)
	else:
	    self.widgets.steptime.set_sensitive(1)
	    self.widgets.stepspace.set_sensitive(1)
	    self.widgets.dirhold.set_sensitive(1)
	    self.widgets.dirsetup.set_sensitive(1)

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
	ex = exclusive.get(v, ())
	for pin1 in (10,11,12,13,15):
	    if pin1 == pin: continue
	    p = 'pin%d' % pin1
	    v1 = self.widgets[p].get_active()
	    if v1 in ex or v1 == v:
		self.widgets[p].set_active(UNUSED_INPUT)

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
	for pin in (10,11,12,13,15):
	    p = 'pin%d' % pin
	    model = self.widgets[p].get_model()
	    model.clear()
	    for name in human_input_names: model.append((name,))
	for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
	    p = 'pin%d' % pin
	    self.widgets[p].set_active(self.data[p])
	    p = 'pin%dinv' % pin
	    self.widgets[p].set_active(self.data[p])
	self.widgets.customhal.set_active(self.data.customhal)
	self.widgets.pyvcp.set_active(self.data.pyvcp)
	self.widgets.pin1.grab_focus()
	self.in_pport_prepare = False
 
    def on_pport_next(self, *args):
	for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
	    p = 'pin%d' % pin
	    self.data[p] = self.widgets[p].get_active()
	    p = 'pin%dinv' % pin
	    self.data[p] = self.widgets[p].get_active()
	self.data.customhal = self.widgets.customhal.get_active()
	self.data.pyvcp = self.widgets.pyvcp.get_active()
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
	elif d.units:
	    w[axis + "screwunits"].set_text(_("mm / rev"))
	    w[axis + "velunits"].set_text(_("mm / s"))
	    w[axis + "accunits"].set_text(_("mm / s²"))
	    w[axis + "accdistunits"].set_text(_("mm"))
	else:
	    w[axis + "screwunits"].set_text(_("rev / in"))
	    w[axis + "velunits"].set_text(_("in / s"))
	    w[axis + "accunits"].set_text(_("in / s²"))
	    w[axis + "accdistunits"].set_text(_("in"))

	n = "xyza".index(axis)

	inputs = set((d.pin10, d.pin11, d.pin12, d.pin13, d.pin15))
	thisaxishome = set((ALL_HOME, HOME_X + n, MIN_HOME_X + n,
			    MAX_HOME_X + n, BOTH_HOME_X + n))
	homes = bool(inputs & thisaxishome)
	w[axis + "homesw"].set_sensitive(homes)
	w[axis + "homevel"].set_sensitive(homes)
	w[axis + "latchdir"].set_sensitive(homes)

	w[axis + "steprev"].grab_focus()

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

    def on_complete_back(self, *args):
	if self.has_spindle_speed_control():
	    self.widgets.druid1.set_page(self.widgets.spindle)
	elif self.data.axes != 1:
	    self.widgets.druid1.set_page(self.widgets.zaxis)
	else:
	    self.widgets.druid1.set_page(self.widgets.aaxis)
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
		self.widgets.druid1.set_page(self.widgets.complete)
	    return True
    def on_aaxis_next(self, *args):
	self.axis_done('a')
	if self.has_spindle_speed_control():
	    self.widgets.druid1.set_page(self.widgets.spindle)
	else:
	    self.widgets.druid1.set_page(self.widgets.complete)
	return True

    def has_spindle_speed_control(self):
	d = self.data
	return PWM in (d.pin1, d.pin2, d.pin3, d.pin4, d.pin5, d.pin6, d.pin7,
	    d.pin8, d.pin9, d.pin14, d.pin16) or \
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
	    scale = self.data[axis + "scale"] = (pitch * get("steprev")
		* get("microstep") * (get("pulleynum") / get("pulleyden")))
            w[axis + "scale"].set_text("%.1f" % scale)
            self.widgets.druid1.set_buttons_sensitive(1,1,1,1)
            w[axis + "axistest"].set_sensitive(1)
	except ValueError: # Some entries not numbers or not valid
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
	gtk.main_quit()

    def on_calculate_ideal_period(self, *args):
        steptime = self.widgets.steptime.get_value()
        stepspace = self.widgets.stepspace.get_value()
        latency = self.widgets.latency.get_value()
        minperiod = int(latency + steptime + stepspace + 5000)
        maxhz = int(1e9 / minperiod)

        self.widgets.baseperiod.set_text("%s ns" % minperiod)
        self.widgets.maxsteprate.set_text("%s Hz" % maxhz)

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

    def test_axis(self, axis):
	data = self.data
	widgets = self.widgets

	vel = float(widgets[axis + "maxvel"].get_text())
	acc = float(widgets[axis + "maxacc"].get_text())

        scale = data[axis + "scale"]
        maxvel = 1.5 * vel
        period = int(1e9 / maxvel / scale)

        steptime = self.widgets.steptime.get_value()
        stepspace = self.widgets.stepspace.get_value()
        latency = self.widgets.latency.get_value()
        minperiod = int(latency + steptime + stepspace + 5000)

        if period < minperiod:
            period = minperiod
            maxvel = 1e9 / minperiod / scale

	self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")

	axnum = "xyza".index(axis)
	step = XSTEP + 2 * axnum
	dir = XDIR + 2 * axnum
	halrun.write("""
	    loadrt steptest
	    loadrt stepgen step_type=0
	    loadrt hal_parport cfg=%(ioaddr)s
	    loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n

	    addf stepgen.make-pulses fast
	    addf parport.0.write fast
	    addf parport.0.reset fast

	    addf stepgen.capture-position slow
	    addf steptest.0 slow
	    addf stepgen.update-freq slow

	    net step stepgen.0.step => parport.0.pin-%(steppin)02d-out
	    net dir stepgen.0.dir => parport.0.pin-%(dirpin)02d-out
	    net cmd steptest.0.position-cmd => stepgen.0.position-cmd
	    net fb stepgen.0.position-fb => steptest.0.position-fb

	    setp parport.0.pin-%(steppin)02d-out-reset 1
	    setp parport.0.reset-time %(resettime)d
	    setp stepgen.0.stepspace 0
	    setp stepgen.0.steplen 1
	    setp stepgen.0.dirhold %(dirhold)d
	    setp stepgen.0.dirsetup %(dirsetup)d
	    setp stepgen.0.position-scale %(scale)d
	    setp steptest.0.epsilon %(onestep)f

	    setp stepgen.0.enable 1
	""" % {
	    'period': period,
	    'ioaddr': data.ioaddr,
	    'steppin': data.find_output(step),
	    'dirpin': data.find_output(dir),
	    'dirhold': data.dirhold + data.latency,
	    'dirsetup': data.dirsetup + data.latency,
            'onestep': 1. / data[axis + "scale"],
	    'scale': data[axis + "scale"],
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
	    widgets.testacc.set_range(0, 360000)
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
	    widgets.testacc.set_range(0, 100000)
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
	    widgets.testacc.set_range(0, 3600)
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

	halrun.write("start"); halrun.flush()
	widgets.dialog1.show_all()
	result = widgets.dialog1.run()
	widgets.dialog1.hide()
	
	if amp:
	    halrun.write("""setp parport.0.pin-%02d-out 0""" % amp)
	if estop:
	    halrun.write("""setp parport.0.pin-%02d-out 0""" % estop)

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
