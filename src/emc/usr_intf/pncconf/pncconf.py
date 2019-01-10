#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#    This is pncconf, a graphical configuration editor for LinuxCNC
#    Chris Morley copyright 2009
#    This is based from stepconf, a graphical configuration editor for linuxcnc
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

import sys
import os
# this is for importing modules from lib/python/pncconf
BIN = os.path.dirname(__file__)
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
libdir = os.path.join(BASE, "lib", "python","pncconf")
sys.path.insert(0, libdir)
import pwd
import errno
import time
import hashlib
import pickle
import shutil
import math
from optparse import Option, OptionParser
import textwrap
import locale
import copy
import commands
import fnmatch
import subprocess
import gobject
import gtk
import gtk.glade

import xml.dom.minidom
import xml.etree.ElementTree
import xml.etree.ElementPath
import traceback
from multifilebuilder import MultiFileBuilder
from touchy import preferences
from pncconf import pages
from pncconf import build_INI
from pncconf import build_HAL
from pncconf import tests
from pncconf import private_data
import cairo
import hal
#import mesatest

def get_value(w):
    try:
        return w.get_value()
    except AttributeError:
        pass
    oldlocale = locale.getlocale(locale.LC_NUMERIC)
    try:
        locale.setlocale(locale.LC_NUMERIC, "")
        return locale.atof(w.get_text())
    finally:
        locale.setlocale(locale.LC_NUMERIC, oldlocale)

def makedirs(d):
    try:
        os.makedirs(d)
    except os.error, detail:
        if detail.errno != errno.EEXIST: raise
makedirs(os.path.expanduser("~/linuxcnc/configs"))

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
domain = "linuxcnc"
gettext.install(domain, localedir=LOCALEDIR, unicode=True)
locale.setlocale(locale.LC_ALL, '')
locale.bindtextdomain(domain, LOCALEDIR)
gettext.bindtextdomain(domain, LOCALEDIR)

def iceil(x):
    if isinstance(x, (int, long)): return x
    if isinstance(x, basestring): x = float(x)
    return int(math.ceil(x))

prefs = preferences.preferences()
_DEBUGSTRING = ["NONE"]
debugstate = False

def md5sum(filename):
    try:
        f = open(filename, "rb")
    except IOError:
        return None
    else:
        return hashlib.md5(f.read()).hexdigest()    

# a class for holding the glade widgets rather then searching for them each time
class Widgets:
    def __init__(self, xml):
        self._xml = xml
    def __getattr__(self, attr):
        r = self._xml.get_object(attr)
        if r is None: raise AttributeError, "No widget %r" % attr
        return r
    def __getitem__(self, attr):
        r = self._xml.get_object(attr)
        if r is None: raise IndexError, "No widget %r" % attr
        return r



class Data:
    def __init__(self,_PD):
        pw = pwd.getpwuid(os.getuid())
        # custom signal name lists
        self.halencoderinputsignames = []
        self.halmuxencodersignames = []
        self.halresolversignames = []
        self.hal8i20signames = []
        self.halpwmoutputsignames = []
        self.haltppwmoutputsignames = []
        self.halinputsignames = []
        self.haloutputsignames = []
        self.halsteppersignames = []
        self.halpotsignames = []
        self.halanaloginsignames = []

        # internal flags
        self._mesa0_arrayloaded = False
        self._mesa0_configured = False
        self._mesa1_configured = False
        self._mesa1_arrayloaded = False
        self._components_is_prepared = False

        # internal combobox arrays
        self._notusedliststore = None
        self._gpioliststore = None
        self._stepperliststore = None
        self._encoderliststore = None
        self._muxencoderliststore = None
        self._resolverliststore = None
        self._8i20liststore = None
        self._pwmliststore = None
        self._tppwmliststore = None
        self._sserialliststore = None
        self._potliststore = None
        self._analoginliststore = None

        self._gpioosignaltree = None
        self._gpioisignaltree = None
        self._steppersignaltree = None
        self._encodersignaltree = None
        self._muxencodersignaltree = None
        self._resolversignaltree = None
        self._8i20signaltree = None
        self._pwmcontrolsignaltree = None
        self._pwmrelatedsignaltree = None
        self._tppwmsignaltree = None
        self._sserialsignaltree = None
        self._potsignaltree = None
        self._analoginsignaltree = None

        # pncconf default options
        self.createsymlink = 1
        self.createshortcut = 0  
        self._lastconfigname= ""
        self._chooselastconfig = True
        self._preference_version = 1.0
        self._pncconf_version = 3.0 # This is the actual version of this program
        self.pncconf_loaded_version = 3.0 # This will version number for new configs or be overwritten by a loaded file to the files version.
        self._re_editmode = False
        self._customfirmwarefilename = "~/Desktop/custom_firmware/firmware.py"
        self.advanced_option = False
        self._substitution_list=[]

        # basic machine data
        self.help = "help-welcome.txt"
        self.machinename = _("my_LinuxCNC_machine")
        self.frontend = _PD._AXIS 
        self.axes = 0 # XYZ
        self.available_axes = []
        self.baseperiod = 50000
        self.servoperiod = 1000000
        self.units = _PD._IMPERIAL # inch
        self.limitsnone = True
        self.limitswitch = False
        self.limitshared = False
        self.homenone = True
        self.homeswitch = False
        self.homeindex = False
        self.homeboth = False
        self.limitstype = 0
        self.homingtype = 0
        self.usbdevicename = "none"
        self.joystickjog = False
        self.joystickjograpidrate0 = 0.1
        self.joystickjograpidrate1 = 1.0
        self.joystickjograpidrate2 = 10.0
        self.joystickjograpidrate3 = 100.0
        self.joycmdrapida = ""
        self.joycmdrapidb = ""
        self.joycmdxpos = ""
        self.joycmdxneg = ""
        self.joycmdypos = ""
        self.joycmdyneg = ""
        self.joycmdzpos = ""
        self.joycmdzneg = ""
        self.joycmdapos = ""
        self.joycmdaneg = ""
        self.joycmdrapid = ""
        self.joycmdanalogx = ""
        self.joycmdanalogy = ""
        self.joycmdanalogz = ""
        self.joycmdanaloga = ""
        self.externaljog = False
        self.singlejogbuttons = False
        self.multijogbuttons = False
        self.jograpidrate = 1.0

        self.externalmpg = False
        self.guimpg = True    
        self.multimpg = False
        self.sharedmpg = False
        self.incrselect = False
        self.mpgincrvalue0 = 0     # all incr-select low
        self.mpgincrvalue1 = .0001 # incr-select-a  high
        self.mpgincrvalue2 = .0005 # b
        self.mpgincrvalue3 = .001  # ab
        self.mpgincrvalue4 = .005  # c
        self.mpgincrvalue5 = .01   # ac
        self.mpgincrvalue6 = .05   # bc
        self.mpgincrvalue7 = .1    # abc
        self.mpgincrvalue8 = .125 # d
        self.mpgincrvalue9 = .125  # ad
        self.mpgincrvalue10 = .125  # bd
        self.mpgincrvalue11 = .125  # abd
        self.mpgincrvalue12 = .125 # cd
        self.mpgincrvalue13 = .125 # acd
        self.mpgincrvalue14 = .125 # bcd
        self.mpgincrvalue15 = .125 # abcd
        self.mpgdebounce = True
        self.mpgdebouncetime = .2
        self.mpggraycode = False
        self.mpgignorefalse = False

        self.externalfo = False
        self.fo_usempg = False
        self.fo_useswitch = False
        self.foincrvalue0 = 0  # all incr-select low
        self.foincrvalue1 = 5  # incr-select-a  high
        self.foincrvalue2 = 10  # b
        self.foincrvalue3 = 25  # ab
        self.foincrvalue4 = 50 # c
        self.foincrvalue5 = 75 # ac
        self.foincrvalue6 = 90 # bc
        self.foincrvalue7 = 100 # abc
        self.foincrvalue8 = 110  # d
        self.foincrvalue9 = 125  # ad
        self.foincrvalue10 = 140  # bd
        self.foincrvalue11 = 150  # abd
        self.foincrvalue12 = 165 # cd
        self.foincrvalue13 = 180 # acd
        self.foincrvalue14 = 190 # bcd
        self.foincrvalue15 = 200 # abcd
        self.fodebounce = True
        self.fodebouncetime = .2
        self.fograycode = False
        self.foignorefalse = False

        self.externalso = False
        self.so_usempg = False
        self.so_useswitch = False
        self.soincrvalue0 = 0  # all incr-select low
        self.soincrvalue1 = 5  # incr-select-a  high
        self.soincrvalue2 = 10  # b
        self.soincrvalue3 = 25  # ab
        self.soincrvalue4 = 50 # c
        self.soincrvalue5 = 75 # ac
        self.soincrvalue6 = 90 # bc
        self.soincrvalue7 = 100 # abc
        self.soincrvalue8 = 110  # d
        self.soincrvalue9 = 125  # ad
        self.soincrvalue10 = 140  # bd
        self.soincrvalue11 = 150  # abd
        self.soincrvalue12 = 165 # cd
        self.soincrvalue13 = 180 # acd
        self.soincrvalue14 = 190 # bcd
        self.soincrvalue15 = 200 # abcd
        self.sodebounce = True
        self.sodebouncetime = .2
        self.sograycode = False
        self.soignorefalse = False

        self.externalmvo = False
        self.mvo_usempg = False
        self.mvo_useswitch = False
        self.mvoincrvalue0 = 0  # all incr-select low
        self.mvoincrvalue1 = 5  # incr-select-a  high
        self.mvoincrvalue2 = 10  # b
        self.mvoincrvalue3 = 25  # ab
        self.mvoincrvalue4 = 50 # c
        self.mvoincrvalue5 = 75 # ac
        self.mvoincrvalue6 = 90 # bc
        self.mvoincrvalue7 = 100 # abc
        self.mvoincrvalue8 = 100  # d
        self.mvoincrvalue9 = 100  # ad
        self.mvoincrvalue10 = 100  # bd
        self.mvoincrvalue11 = 100  # abd
        self.mvoincrvalue12 = 100 # cd
        self.mvoincrvalue13 = 100 # acd
        self.mvoincrvalue14 = 100 # bcd
        self.mvoincrvalue15 = 100 # abcd
        self.mvodebounce = True
        self.mvodebouncetime = .2
        self.mvograycode = False
        self.mvoignorefalse = False

        self.gearselect = False
        self.gsincrvalue0 = 0  # all incr-select low
        self.gsincrvalue1 = 0  # incr-select-a  high
        self.gsincrvalue2 = 0  # b
        self.gsincrvalue3 = 0  # ab
        self.gsincrvalue4 = 0 # c
        self.gsincrvalue5 = 0 # ac
        self.gsincrvalue6 = 0 # bc
        self.gsincrvalue7 = 0 # abc
        self.gsincrvalue8 = 0  # d
        self.gsincrvalue9 = 0  # ad
        self.gsincrvalue10 = 0  # bd
        self.gsincrvalue11 = 0  # abd
        self.gsincrvalue12 = 0 # cd
        self.gsincrvalue13 = 0 # acd
        self.gsincrvalue14 = 0 # bcd
        self.gsincrvalue15 = 0 # abcd
        self.gsdebounce = True
        self.gsdebouncetime = .2
        self.gsgraycode = False
        self.gsignorefalse = False

        self.serial_vfd = False

        # GUI frontend defaults
        self.position_offset = 1 # relative
        self.position_feedback = 1 # actual
        self.max_feed_override = 2.0 # percentage
        self.min_spindle_override = .5
        self.max_spindle_override = 1.0
        # These are for AXIS gui only
        # linear jog defaults are set with: set_axis_unit_defaults() 
        self.default_angular_velocity = 12
        self.min_angular_velocity = 3
        self.max_angular_velocity = 180
        self.increments_metric = "5mm 1mm .5mm .1mm .05mm .01mm .005mm"
        self.increments_imperial= ".1in .05in .01in .005in .001in .0005in .0001in"
        self.editor = "gedit"
        self.geometry = "xyz"
        self.axisforcemax = False
        self.axissize = [False,0,0]
        self.axisposition = [False,0,0]

        # Gmoccapy
        self.gmcpytheme = "Follow System Theme"

        # Touchy only
        self.touchysize = [False,0,0]
        self.touchyposition = [False,0,0]
        self.touchytheme = "Follow System Theme"
        self.touchyforcemax = False
        self.touchyabscolor = "default"
        self.touchyrelcolor = "default"
        self.touchydtgcolor = "default"
        self.touchyerrcolor = "default"

        # LinuxCNC assorted defaults and options
        self.toolchangeprompt = True
        self.multimpg = False
        self.require_homing = True
        self.individual_homing = False
        self.restore_joint_position = False
        self.random_toolchanger = False
        self.raise_z_on_toolchange = False
        self.allow_spindle_on_toolchange = False
        self.customhal = False
        self.usebldc = False

        # These components can be used by pncconf so we must keep track of them.
        # in case the user needs some for a custom HAL file
        self.userneededpid = 0
        self.userneededabs = 0
        self.userneededscale = 0
        self.userneededmux16 = 0
        self.userneededlowpass = 0
        self.userneededbldc = 0

        # pyvcp data
        self.pyvcp = 0 # not included
        self.pyvcpname = "custom.xml"
        self.pyvcpexist = False
        self.pyvcp1 = False
        self.pyvcpblank = True
        self.pyvcphaltype = 0 # no HAL connections specified
        self.pyvcpconnect = 1 # HAL connections allowed
        self.pyvcpwidth = 200
        self.pyvcpheight = 200
        self.pyvcpxpos = 0
        self.pyvcpypos = 0
        self.pyvcpposition = False
        self.pyvcpsize = False

        # gladevcp data
        self.gladevcp = False # not included
        self.gladesample = True
        self.gladeexists = False
        self.spindlespeedbar = True
        self.spindleatspeed = True
        self.maxspeeddisplay = 1000
        self.zerox = False
        self.zeroy = False
        self.zeroz = False
        self.zeroa = False
        self.autotouchz = False
        self.gladevcphaluicmds = 0
        self.centerembededgvcp = True
        self.sideembededgvcp = False
        self.standalonegvcp = False
        self.gladevcpposition = False
        self.gladevcpsize = False
        self.gladevcpforcemax = False
        self.gladevcpwidth = 200
        self.gladevcpheight = 200
        self.gladevcpxpos = 0
        self.gladevcpypos = 0
        self.gladevcptheme = "Follow System Theme"

        # classicladder data
        self.classicladder = 0 # not included
        self.digitsin = 15 # default number of pins
        self.digitsout = 15
        self.s32in = 10
        self.s32out = 10
        self.floatsin = 10
        self.floatsout = 10
        self.bitmem = 50
        self.wordmem = 50
        self.tempexists = 0 # not present ( a blank CL program edited through pncconf)
        self.laddername = "custom.clp"
        self.modbus = 0 # not included
        self.ladderhaltype = 0 # no HAL connections specified
        self.ladderconnect = 1 # HAL connections allowed
        self.ladderexist = False
        self.cl_haluicmds = 0
        self.laddertouchz = False

        # stepper timing data
        self.drivertype = "other"
        self.steptime = 5000
        self.stepspace = 5000
        self.dirhold = 20000 
        self.dirsetup = 20000
        self.latency = 15000
        self.period = 25000

        # For parallel port 
        self.pp1_direction = 1 # output
        self.ioaddr1 = "0"
        self.ioaddr2 = "1"
        self.pp2_direction = 0 # input
        self.ioaddr3 = "2"
        self.pp3_direction = 0 # input
        self.number_pports = 0

        for connector in("pp1","pp2","pp3"):
            # initialize parport input / inv pins
            for i in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                pinname ="%s_Ipin%d"% (connector,i)
                self[pinname] = _PD.UNUSED_INPUT
                pinname ="%s_Ipin%d_inv"% (connector,i)
                self[pinname] = False
            # initialize parport output / inv pins
            for i in (1,2,3,4,5,6,7,8,9,14,16,17):
                pinname ="%s_Opin%d"% (connector,i)
                self[pinname] = _PD.UNUSED_OUTPUT
                pinname ="%s_Opin%d_inv"% (connector,i)
                self[pinname] = False

        self.number_mesa = 1 # number of cards
        # for first mesa card
        self.mesa0_currentfirmwaredata = None
        self.mesa0_boardtitle = "5i25-Internal Data"        
        self.mesa0_firmware = _PD.MESA_INTERNAL_FIRMWAREDATA[0][2]  
        self.mesa0_parportaddrs = "0x378"
        self.mesa0_card_addrs = "192.168.1.121"
        self.mesa0_isawatchdog = 1
        self.mesa0_pwm_frequency = 20000
        self.mesa0_pdm_frequency = 6000000
        self.mesa0_3pwm_frequency = 20000
        self.mesa0_watchdog_timeout = 5000000
        self.mesa0_numof_encodergens = 1
        self.mesa0_numof_resolvers = 0
        self.mesa0_numof_pwmgens = 0
        self.mesa0_numof_tppwmgens = 0
        self.mesa0_numof_stepgens = 5
        self.mesa0_numof_sserialports = 1
        self.mesa0_numof_sserialchannels = 2

        # second mesa card
        self.mesa1_currentfirmwaredata = _PD.MESA_INTERNAL_FIRMWAREDATA[1]
        self.mesa1_boardtitle = "5i25-Internal Data"
        self.mesa1_firmware = _PD.MESA_INTERNAL_FIRMWAREDATA[0][2]
        self.mesa1_parportaddrs = "0x378"
        self.mesa1_card_addrs = "192.168.1.121"
        self.mesa1_isawatchdog = 1
        self.mesa1_pwm_frequency = 20000
        self.mesa1_pdm_frequency = 6000000
        self.mesa1_3pwm_frequency = 20000
        self.mesa1_watchdog_timeout = 5000000
        self.mesa1_numof_encodergens = 1
        self.mesa1_numof_resolvers = 0
        self.mesa1_numof_pwmgens = 0
        self.mesa1_numof_tppwmgens = 0
        self.mesa1_numof_stepgens = 5
        self.mesa1_numof_sserialports = 1
        self.mesa1_numof_sserialchannels = 2

        for boardnum in(0,1):
            connector = 2
            pinname ="mesa%dc%dpin"% (boardnum,connector)
            self[pinname+"0"] = _PD.UNUSED_ENCODER
            self[pinname+"0type"] = _PD.ENCB
            self[pinname+"1"] = _PD.UNUSED_ENCODER
            self[pinname+"1type"] = _PD.ENCA
            self[pinname+"2"] = _PD.UNUSED_ENCODER
            self[pinname+"2type"] = _PD.ENCB
            self[pinname+"3"] = _PD.UNUSED_ENCODER
            self[pinname+"3type"] = _PD.ENCA
            self[pinname+"4"] = _PD.UNUSED_ENCODER
            self[pinname+"4type"] = _PD.ENCI
            self[pinname+"5"] = _PD.UNUSED_ENCODER
            self[pinname+"5type"] = _PD.ENCI
            self[pinname+"6"] = _PD.UNUSED_PWM
            self[pinname+"6type"] = _PD.PWMP
            self[pinname+"7"] = _PD.UNUSED_PWM
            self[pinname+"7type"] = _PD.PWMP
            self[pinname+"8"] = _PD.UNUSED_PWM
            self[pinname+"8type"] = _PD.PWMD
            self[pinname+"9"] = _PD.UNUSED_PWM
            self[pinname+"9type"] = _PD.PWMD
            self[pinname+"10"] = _PD.UNUSED_PWM
            self[pinname+"10type"] = _PD.PWME
            self[pinname+"11"] = _PD.UNUSED_PWM
            self[pinname+"11type"] = _PD.PWME
            self[pinname+"12"] = _PD.UNUSED_ENCODER
            self[pinname+"12type"] = _PD.ENCB
            self[pinname+"13"] = _PD.UNUSED_ENCODER
            self[pinname+"13type"] = _PD.ENCA
            self[pinname+"14"] = _PD.UNUSED_ENCODER
            self[pinname+"14type"] = _PD.ENCB
            self[pinname+"15"] = _PD.UNUSED_ENCODER
            self[pinname+"15type"] = _PD.ENCA
            self[pinname+"16"] = _PD.UNUSED_ENCODER
            self[pinname+"16type"] = _PD.ENCI
            self[pinname+"17"] = _PD.UNUSED_ENCODER
            self[pinname+"17type"] = _PD.ENCI
            self[pinname+"18"] = _PD.UNUSED_PWM
            self[pinname+"18type"] = _PD.PWMP
            self[pinname+"19"] = _PD.UNUSED_PWM
            self[pinname+"19type"] = _PD.PWMP
            self[pinname+"20"] = _PD.UNUSED_PWM
            self[pinname+"20type"] = _PD.PWMD
            self[pinname+"21"] = _PD.UNUSED_PWM
            self[pinname+"21type"] = _PD.PWMD
            self[pinname+"22"] = _PD.UNUSED_PWM
            self[pinname+"22type"] = _PD.PWME
            self[pinname+"23"] = _PD.UNUSED_PWM
            self[pinname+"23type"] = _PD.PWME
        for boardnum in(0,1):
            for connector in(1,3,4,5,6,7,8,9):
                # This initializes GPIO input pins
                for i in range(0,16):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self[pinname] = _PD.UNUSED_INPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self[pinname] = _PD.GPIOI
                # This initializes GPIO output pins
                for i in range(16,24):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self[pinname] = _PD.UNUSED_OUTPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self[pinname] = _PD.GPIOO
            for connector in(1,2,3,4,5,6,7,8,9):
                # This initializes the mesa inverse pins
                for i in range(0,24):
                    pinname ="mesa%dc%dpin%dinv"% (boardnum,connector,i)
                    self[pinname] = False
            port = 0 #TODO up to 4 ports
            for channel in range(0,_PD._NUM_CHANNELS):
                self["mesa%dsserial%d_%dsubboard"% (boardnum, port,channel)] = "none"
                # This initializes pins
                for i in range(0,_PD._SSCOMBOLEN):
                    pinname ="mesa%dsserial%d_%dpin%d"% (boardnum, port,channel,i)
                    if i < 24:
                        self[pinname] = _PD.UNUSED_INPUT
                    else:
                        self[pinname] = _PD.UNUSED_OUTPUT
                    pinname ="mesa%dsserial%d_%dpin%dtype"% (boardnum, port,channel,i)
                    if i < 24:
                        self[pinname] = _PD.GPIOI
                    else:
                        self[pinname] = _PD.GPIOO
                    pinname ="mesa%dsserial%d_%dpin%dinv"% (boardnum, port,channel,i)
                    self[pinname] = False

        # halui data
        self.halui = 0 # not included
        # Command list
        for i in range(0,15):
                pinname ="halui_cmd%s"% i
                self[pinname] = ""

        #HAL component command list
        self.loadcompservo = []
        self.addcompservo = []
        self.loadcompbase = []
        self.addcompbase = []

        self.xhomesequence = 1
        self.yhomesequence = 2
        self.zhomesequence = 0
        self.ahomesequence = 3

        # common axis data
        for temp in("x","y","z","a","s"):

            self[temp+"drivertype"]= "custom"
            self[temp+"steprev"]= 200
            self[temp+"microstep"]= 5
            self[temp+"motor_pulleydriver"]= 1
            self[temp+"motor_pulleydriven"]= 1
            self[temp+"cbmicrosteps"]= False
            self[temp+"cbmotor_pulley"]= False
            if not temp == 's':
                self[temp+"motor_wormdriver"]= 1
                self[temp+"motor_wormdriven"]= 1
                self[temp+"encoder_pulleydriver"]= 1
                self[temp+"encoder_pulleydriven"]= 1
                self[temp+"encoder_wormdriver"]= 1
                self[temp+"encoder_wormdriven"]= 1
                self[temp+"motor_leadscrew"]= 5
                self[temp+"encoder_leadscrew"]= 5
                self[temp+"motor_leadscrew_tpi"]= 5
                self[temp+"encoder_leadscrew_tpi"]= 5
                self[temp+"cbencoder_pitch"]= False
                self[temp+"cbencoder_tpi"]= False
                self[temp+"cbencoder_worm"]= False
                self[temp+"cbencoder_pulley"]= False
                self[temp+"cbmotor_pitch"]= False
                self[temp+"cbmotor_tpi"]= False
                self[temp+"cbmotor_worm"]= False
                self[temp+"outputscale"]= 10
                self[temp+"outputminlimit"]= -10
                self[temp+"outputmaxlimit"]= 10
            else:
                self[temp+"motor_gear1driver"]= 1
                self[temp+"motor_gear1driven"]= 1
                self[temp+"motor_gear2driver"]= 1
                self[temp+"motor_gear2driven"]= 1
                self[temp+"motor_max"] = 2000
                self[temp+"cbmotor_gear1"]= False
                self[temp+"cbmotor_gear2"]= False
                self[temp+"negative_rot"]= False
                self[temp+"rbvoltage_5"]= False
                self[temp+"outputscale2"]= 10
                self[temp+"outputmaxvoltage"]= 10
            self[temp+"encodercounts"]= 4000
            self[temp+"usecomp"]= 0
            self[temp+"compfilename"]= temp+"compensation"
            self[temp+"comptype"]= 0
            self[temp+"usebacklash"]= 0
            self[temp+"backlash"]= 0
            self[temp+"invertmotor"]= 0
            self[temp+"invertencoder"]= 0
            self[temp+"3pwmscale"]= 1
            self[temp+"3pwmdeadtime"]= 500
            self[temp+"maxoutput"]= 0
            self[temp+"P"]= None
            self[temp+"I"]= 0
            self[temp+"D"]= 0
            self[temp+"FF0"]= 0
            self[temp+"FF1"]= 1
            self[temp+"FF2"]= 0
            self[temp+"bias"]= 0
            self[temp+"deadband"]= 0
            self[temp+"steptime"]= 1000
            self[temp+"stepspace"]= 1000
            self[temp+"dirhold"]= 1000
            self[temp+"dirsetup"]= 1000
            self[temp+"homepos"]= 0
            self[temp+"homesw"]=  0
            self[temp+"homefinalvel"]= 0
            self[temp+"latchdir"]= 0
            self[temp+"searchdir"]= 0
            self[temp+"usehomeindex"]= 0
            self[temp+"stepscale"]= 0
            self[temp+"encoderscale"]= 0
            self[temp+"bldc_option"]= False
            self[temp+"bldc_config"]= ""
            self[temp+"bldc_no_feedback"]= False
            self[temp+"bldc_absolute_feedback"]= False
            self[temp+"bldc_incremental_feedback"]= True
            self[temp+"bldc_use_hall"]= True
            self[temp+"bldc_use_encoder"]= False
            self[temp+"bldc_fanuc_alignment"]= False
            self[temp+"bldc_use_index"]= False
            self[temp+"bldc_digital_output"]= False
            self[temp+"bldc_six_outputs"]= False
            self[temp+"bldc_force_trapz"]= False
            self[temp+"bldc_emulated_feedback"]= False
            self[temp+"bldc_output_hall"]= False
            self[temp+"bldc_output_fanuc"]= False
            self[temp+"bldc_scale"]= 512
            self[temp+"bldc_poles"]= 4
            self[temp+"bldc_lead_angle"]= 90
            self[temp+"bldc_inital_value"]= 0.2
            self[temp+"bldc_encoder_offset"]= 0
            self[temp+"bldc_reverse"]= False
            self[temp+"bldc_drive_offset"]= 0.0
            self[temp+"bldc_pattern_out"]= 25
            self[temp+"bldc_pattern_in"]= 25
            self[temp+"8i20maxcurrent"] = 5.0

        # set xyz axes defaults depending on units true = imperial
        self.set_axis_unit_defaults(True)

        # rotary tables need bigger limits
        self.aminlim = -9999
        self.amaxlim =  9999
        self.amaxvel = 360
        self.amaxacc = 1200
        self.ahomevel = 0
        self.ahomesearchvel= 2
        self.ahomelatchvel= .5
        self.aminferror= .05
        self.amaxferror= .5
        # adjust spindle settings
        self.soutputscale = 2000
        self.smaxoutput = 2000
        self.sP = 0
        self.sFF1= 0
        self.sFF0= 1
        self.smaxvel = 2000
        self.smaxacc = 300
        self.snearrange = 200.0
        self.snearscale = 1.20
        self.sfiltergain = 1.0
        self.suseatspeed = False
        self.suseoutputrange2 = False
        self.scaleselect = False
        self.susenegativevoltage = True
        self.susenearrange = False
        self.ssingleinputencoder = False
        self.mitsub_vfd = False
        self.mitsub_vfd_port = '/dev/ttyS0'
        self.mitsub_vfd_id = (('spindle_vfd',1))
        self.mitsub_vfd_baud = 9600
        self.mitsub_vfd_command_scale = 1
        self.mitsub_vfd_feedback_scale = 1
        self.gs2_vfd = True
        self.gs2_vfd_port = '/dev/ttyS0'
        self.gs2_vfd_slave = 1
        self.gs2_vfd_baud = 9600
        self.gs2_vfd_accel = 10
        self.gs2_vfd_deaccel = 0

    # change the XYZ axis defaults to metric or imperial
    # This only sets data that makes sense to change eg gear ratio don't change
    def set_axis_unit_defaults(self, units=True):
        if units: # imperial
            # set GUI defaults
            self.max_linear_velocity = 1 # 60 inches per min
            self.default_linear_velocity = .25 # 15 inches per min
            self.min_linear_velocity = .01667

            # axes defaults
            for i in ('x','y','z'):
                self[i+'maxvel'] = 1
                self[i+'maxacc'] = 30
                self[i+'homevel'] = .05
                self[i+"homesearchvel"]= .05
                self[i+"homelatchvel"]= .025
                self[i+"minferror"]= .05
                self[i+"maxferror"]= .5
                if not i == 'z':
                    self[i+'minlim'] = 0
                    self[i+'maxlim'] = 8
                else:
                    self.zminlim = -4
                    self.zmaxlim = 0
        else: # metric
            # set gui defaults
            self.max_linear_velocity = 25 # 1500 mm per min
            self.default_linear_velocity = 6 # 380 mm per min
            self.min_linear_velocity = .5

            # axes defaults
            for i in ('x','y','z'):
                self[i+'maxvel'] = 25
                self[i+'maxacc'] = 750
                self[i+'homevel'] = 1.5
                self[i+"homesearchvel"]= 1
                self[i+"homelatchvel"]= .5
                self[i+"minferror"]= 1
                self[i+"maxferror"]= 10
                if not i =='z':
                    self[i+'minlim'] = 0
                    self[i+'maxlim'] = 200
                else:
                    self.zminlim = -100
                    self.zmaxlim = 0

    def load(self, filename, app=None, force=False):
        self.pncconf_loaded_version = 0.0
        def str2bool(s):
            return s == 'True'

        converters = {'string': str, 'float': float, 'int': int, 'bool': str2bool, 'eval': eval}

        d = xml.dom.minidom.parse(open(filename, "r"))
        for n in d.getElementsByTagName("property"):
            name = n.getAttribute("name")
            conv = converters[n.getAttribute('type')]
            text = n.getAttribute('value')
            setattr(self, name, conv(text))
        
        # this loads custom signal names created by the user
        # adds endings to the custom signal name when put in
        # hal signal name arrays
        #encoders
        temp =[]; i = False
        for i in  self.halencoderinputsignames:
            temp.append((i,i+"-a"))
            for j in(["-a","-b","-i","-m"]):
                _PD.hal_encoder_input_names.append(i+j)
        end = len(_PD.human_encoder_input_names)-1
        if i:  _PD.human_encoder_input_names[end][1]= temp
        #resolvers
        temp = []; i = False
        for i in  self.halresolversignames:
            temp.append(i)
            _PD.hal_resolver_input_names.append(i)
        end = len(_PD.human_resolver_input_names)-1
        if i: _PD.human_resolver_input_names[end][1]= temp
        # 8I20 amplifier card
        temp = []; i = False
        for i in  self.hal8i20signames:
            temp.append(i)
            _PD.hal_8i20_input_names.append(i)
        end = len(_PD.human_8i20_input_names)-1
        if i: _PD.human_8i20_input_names[end][1]= temp
        # potentiometer
        temp = []; i = False
        for i in  self.halpotsignames:
            temp.append(i)
            for j in(["-output","-enable"]):
                _PD.hal_pot_output_names.append(i+j)
        end = len(_PD.human_pot_output_names)-1
        if i: _PD.human_pot_output_names[end][1]= temp
        # analog input
        temp = []; i = False
        for i in  self.halanaloginsignames:
            temp.append(i)
            _PD.hal_analog_input_names.append(i)
        end = len(_PD.human_analog_input_names)-1
        if i: _PD.human_analog_input_names[end][1]= temp
        #pwm
        temp =[]; i = False
        for i in  self.halpwmoutputsignames:
            temp.append((i,i+"-pulse"))
            for j in(["-pulse","-dir","-enable"]):
                _PD.hal_pwm_output_names.append(i+j)
        end = len(_PD.human_pwm_output_names)-1
        if i: _PD.human_pwm_output_names[end][1]= temp
        # tppwm
        temp =[]; i = False
        for i in  self.haltppwmoutputsignames:
            temp.append(i)
            for j in(["-a","-b","-c","-anot","-bnot","-cnot","-fault","-enable"]):
                _PD.hal_tppwm_output_names.append(i+j)
        end = len(_PD.human_tppwm_output_names)-1
        if i:  _PD.human_tppwm_output_names[end][1]= temp
        # GPIO Input
        temp = []; i = False
        for i in  self.halinputsignames:
            temp.append((i, i))
            _PD.hal_input_names.append(i)
        end = len(_PD.human_input_names)-1
        if i: _PD.human_input_names[end][1]= temp
        # GPIO Output
        temp = []; i = False
        for i in  self.haloutputsignames:
            temp.append(i)
            _PD.hal_output_names.append(i)
        end = len(_PD.human_output_names)-1
        if i: _PD.human_output_names[end][1]= temp
        # steppers
        temp = []; i = False
        for i in  self.halsteppersignames:
            temp.append((i,i+"-step"))
            for j in(["-step","-dir","-c","-d","-e","-f"]):
                _PD.hal_stepper_names.append(i+j)
        end = len(_PD.human_stepper_names)-1
        if i: _PD.human_stepper_names[end][1]= temp

        warnings = []
        warnings2 = []
        if self.pncconf_loaded_version < self._pncconf_version:
            warnings.append(_("This configuration was saved with an earlier version of pncconf which may be incompatible.\n\
If it doesn't plainly cause an error, you still may want to save it with another name and check it. Safer to start from scratch.\n\
If you have a REALLY large config that you wish to convert to this newer version of PNConf - ask on the LinuxCNC forum - it may be possible..") )
        for f, m in self.md5sums:
            m1 = md5sum(f)
            if m1 and m != m1:
                warnings2.append(_("File %r was modified since it was written by PNCconf") % f)
        if not warnings and not warnings2: return
        if warnings2:
            warnings.append("")
            warnings.append(_("Saving this configuration file will discard configuration changes made outside PNCconf."))
        if warnings:
            warnings = warnings + warnings2
        self.pncconf_loaded_version = self._pncconf_version
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

    def save(self,basedir):
        base = basedir
        ncfiles = os.path.expanduser("~/linuxcnc/nc_files")
        if not os.path.exists(ncfiles):
            makedirs(ncfiles)
            examples = os.path.join(BASE, "share", "linuxcnc", "ncfiles")
            if not os.path.exists(examples):
                examples = os.path.join(BASE, "nc_files")
            if os.path.exists(examples):
                os.symlink(examples, os.path.join(ncfiles, "examples"))
        
        makedirs(base)
        makedirs(base+"/backups")

        self.md5sums = []

        filename = os.path.join(base, "tool.tbl")
        file = open(filename, "w")
        print >>file, "T0 P0 ;"
        print >>file, "T1 P1 ;"
        print >>file, "T2 P2 ;"
        print >>file, "T3 P3 ;"
        file.close()

        filename = "%s.pncconf" % base

        d = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, "pncconf", None)
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
        print "%s" % base

        # write pncconf hidden preference file
        filename = os.path.expanduser("~/.pncconf-preferences")
        print filename
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
        n2.setAttribute('name', "show_advanced_pages")
        n2.setAttribute('value', str("%s"% self.advanced_option))

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

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'eval')
        n2.setAttribute('name', "MESABLACKLIST")
        n2.setAttribute('value', str(_PD.MESABLACKLIST))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'string')
        n2.setAttribute('name', "customfirmwarefilename")
        n2.setAttribute('value', str("%s"% self._customfirmwarefilename))

        d2.writexml(open(filename, "wb"), addindent="  ", newl="\n")

        # write to Touchy preference file directly
        if self.frontend == _PD._TOUCHY:
            #print "Setting TOUCHY preferences"
            templist = {"touchyabscolor":"abs_textcolor","touchyrelcolor":"rel_textcolor",
                        "touchydtgcolor":"dtg_textcolor","touchyerrcolor":"err_textcolor"}
            for key,value in templist.iteritems():
                prefs.putpref(value, self[key], str)
            if self.touchyposition[0] or self.touchysize[0]:
                    pos = size = ""
                    if self.touchyposition[0]:
                        pos = "+%d+%d"% (self.touchyposition[1],self.touchyposition[2])
                    if self.touchysize[0]:
                        size = "%dx%d"% (self.touchysize[1],self.touchysize[2])
                    geo = "%s%s"%(size,pos)
            else: geo = "default"
            prefs.putpref('window_geometry',geo, str)
            prefs.putpref('gtk_theme',self.touchytheme, str)
            prefs.putpref('window_force_max', self.touchyforcemax, bool)

        # write AXIS rc file for geometry
        if self.frontend == _PD._AXIS and (self.axisposition[0] or self.axissize[0] or self.axisforcemax):
            filename = os.path.expanduser("~/.axisrc")
            if App.warning_dialog(app,"Ok to replace AXIS's .axisrc file?\n\
 If you haven't added custom commands to this hidden file, outside of pncconf, then this should be fine.\n\
Choosing no will mean AXIS options such as size/position and force maximum might not be as requested \n",False):
                f1 = open(filename, "w")
                if self.axisposition[0] or self.axissize[0]:
                    #print "Setting AXIS geometry option"
                    pos = size = ""
                    if self.axisposition[0]:
                        pos = "+%d+%d"% (self.axisposition[1],self.axisposition[2])
                    if self.axissize[0]:
                        size = "%dx%d"% (self.axissize[1],self.axissize[2])
                    geo = "%s%s"%(size,pos)
                    print >>f1,"""root_window.tk.call("wm","geometry",".","%s")"""%(geo)
                if self.axisforcemax:
                    #print "Setting AXIS forcemax option"
                    print >>f1,"""# Find the largest size possible and set AXIS to it"""
                    print >>f1,"""maxgeo=root_window.tk.call("wm","maxsize",".")"""
                    print >>f1,"""try:"""
                    print >>f1,"""   fullsize=maxgeo.split(' ')[0] + 'x' + maxgeo.split(' ')[1]"""
                    print >>f1,"""except:"""
                    print >>f1,"""   fullsize=str(maxgeo[0]) + 'x' + str(maxgeo[1])"""
                    print >>f1,"""root_window.tk.call("wm","geometry",".",fullsize)"""
                    print >>f1,"""# Uncomment for fullscreen"""
                    print >>f1,"""#root_window.attributes('-fullscreen', True)"""

        # make system link and shortcut to pncconf files
        # see http://freedesktop.org/wiki/Software/xdg-user-dirs
        desktop = commands.getoutput("""
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
            print >>file,"[Desktop Entry]"
            print >>file,"Version=1.0"
            print >>file,"Terminal=false"
            print >>file,"Name=" + _("launch %s") % self.machinename
            print >>file,"Exec=%s %s/%s.ini" \
                         % ( scriptspath, base, self.machinename )
            print >>file,"Type=Application"
            print >>file,"Comment=" + _("Desktop Launcher for LinuxCNC config made by PNCconf")
            print >>file,"Icon=%s"% _PD.LINUXCNCICON
            file.close()
            # Ubuntu 10.04 require launcher to have execute permissions
            os.chmod(filename,0775)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

    # This method returns I/O pin designation (name and number) of a given HAL signalname.
    # It does not check to see if the signalname is in the list more then once.
    # if parports are not used then signals are not searched.
    def findsignal(self, sig):
        if self.number_pports:
            ppinput = {}
            ppoutput = {}
            for i in (1,2,3):
                for s in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                    key = self["pp%d_Ipin%d" %(i,s)]
                    ppinput[key] = "pp%d_Ipin%d" %(i,s) 
                for s in (1,2,3,4,5,6,7,8,9,14,16,17):
                    key = self["pp%d_Opin%d" %(i,s)]
                    ppoutput[key] = "pp%d_Opin%d" %(i,s) 
        mesa = {}
        for boardnum in range(0,int(self.number_mesa)):
            for concount,connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_PD._NUMOFCNCTRS]) :
                for s in range(0,24):
                    key =   self["mesa%dc%dpin%d"% (boardnum,connector,s)]
                    mesa[key] = "mesa%dc%dpin%d" %(boardnum,connector,s)
            if self["mesa%d_numof_sserialports"% boardnum]:
                sserial = {}
                port = 0
                for channel in range (0,self["mesa%d_currentfirmwaredata"% boardnum][_PD._MAXSSERIALCHANNELS]):
                        if channel ==_PD._NUM_CHANNELS: break # TODO may not be all channels available
                        for pin in range (0,_PD._SSCOMBOLEN):       
                            key = self['mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, pin)]
                            sserial[key] = 'mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, pin)
        try:
            return mesa[sig]
        except:
            try:
                return sserial[sig]
            except:
                pass
        if self.number_pports:
            try:
                return ppinput[sig]
            except:
                try:
                    return ppoutput[sig]
                except:
                    return None
        else: return None

    # search all the current firmware array for related pins
    # if not the same component number as the pin that changed or
    # if not in the relate component type keep searching
    # if is the right component type and number, check the relatedsearch array for a match
    # if its a match add it to a list of pins (pinlist) that need to be updated
    def list_related_pins(self, relatedsearch, boardnum, connector, channel, pin, style):
        #print relatedsearch, boardnum, connector, channel, pin, style
        pinlist =[]
        if not channel == None:
            subfirmname = self["mesa%dsserial%d_%dsubboard"% (boardnum, connector, channel)]
            for subnum,temp in enumerate(_PD.MESA_DAUGHTERDATA):
                if _PD.MESA_DAUGHTERDATA[subnum][_PD._SUBFIRMNAME] == subfirmname: break
            subboardname = _PD.MESA_DAUGHTERDATA[subnum][_PD._SUBBOARDNAME]
            currentptype,currentcompnum = _PD.MESA_DAUGHTERDATA[subnum][_PD._SUBSTARTOFDATA+pin]
            for t_pin in range (0,_PD._SSCOMBOLEN):
                comptype,compnum = _PD.MESA_DAUGHTERDATA[subnum][_PD._SUBSTARTOFDATA+t_pin]
                if compnum != currentcompnum: continue
                if comptype not in (relatedsearch): continue
                if style == 0:
                    tochange = ['mesa%dsserial%d_%dpin%d'% (boardnum,connector,channel,t_pin),boardnum,connector,channel,t_pin]
                if style == 1:
                    tochange = ['mesa%dsserial%d_%dpin%dtype'% (boardnum,connector,channel,t_pin),boardnum,connector,channel,t_pin]
                if style == 2:
                    tochange = ['mesa%dsserial%d_%dpin%dinv'% (boardnum,connector,channel,t_pin),boardnum,connector,channel,t_pin]
                pinlist.append(tochange)

        else:
            for concount,i in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_PD._NUMOFCNCTRS]):
                if i == connector:
                    currentptype,currentcompnum = self["mesa%d_currentfirmwaredata"% (boardnum)][_PD._STARTOFDATA+pin+(concount*24)]
                    for t_concount,t_connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_PD._NUMOFCNCTRS]):
                        for t_pin in range (0,24):
                            comptype,compnum = self["mesa%d_currentfirmwaredata"% (boardnum)][_PD._STARTOFDATA+t_pin+(t_concount*24)]
                            if compnum != currentcompnum: continue
                            if comptype not in (relatedsearch): continue
                            if style == 0:
                                tochange = ['mesa%dc%dpin%d'% (boardnum,t_connector,t_pin),boardnum,t_connector,None,t_pin]
                            if style == 1:
                                tochange = ['mesa%dc%dpin%dtype'% (boardnum,t_connector,t_pin),boardnum,t_connector,None,t_pin]
                            if style == 2:
                                tochange = ['mesa%dc%dpin%dinv'% (boardnum,t_connector,t_pin),boardnum,t_connector,None,t_pin]
                            pinlist.append(tochange)
        return pinlist

    # This method takes a signalname data pin (eg mesa0c3pin1)
    # and converts it to a HAL pin names (eg hm2_5i20.0.gpio.01)
    # component number conversion is for adjustment of position of pins related to the
    # 'controlling pin' eg encoder-a (controlling pin) encoder-b encoder -I
    # (a,b,i are related pins for encoder component)
    # gpionumber is a flag to return a gpio piname instead of the component pinname
    # this is used when we want to invert the pins of a component output (such as a stepper)
    # because you actually must invert the GPIO that would be in that position
    # prefixonly flag is used when we want the pin name without the component name.
    # used with sserial when we want the sserial port and channel so we can add out own name (eg enable pins)
    def make_pinname(self, pin, gpionumber = False, prefixonly = False):
        test = str(pin)  
        halboardnum = 0
        if test == "None": return None
        elif 'mesa' in test:
            type_name = { _PD.GPIOI:"gpio", _PD.GPIOO:"gpio", _PD.GPIOD:"gpio",
                _PD.ENCA:"encoder", _PD.ENCB:"encoder",_PD.ENCI:"encoder",_PD.ENCM:"encoder",
                _PD.RES0:"resolver",_PD.RES1:"resolver",_PD.RES2:"resolver",_PD.RES3:"resolver",_PD.RES4:"resolver",_PD.RES5:"resolver",
                _PD.MXE0:"encoder", _PD.MXE1:"encoder",
                _PD.PWMP:"pwmgen",_PD.PWMD:"pwmgen", _PD.PWME:"pwmgen", _PD.PDMP:"pwmgen", _PD.PDMD:"pwmgen", _PD.PDME:"pwmgen",
                _PD.UDMU:"pwmgen",_PD.UDMD:"pwmgen", _PD.UDME:"pwmgen",_PD.STEPA:"stepgen", _PD.STEPB:"stepgen",
                _PD.TPPWMA:"tppwmgen",_PD.TPPWMB:"tppwmgen",_PD.TPPWMC:"tppwmgen",
                _PD.TPPWMAN:"tppwmgen",_PD.TPPWMBN:"tppwmgen",_PD.TPPWMCN:"tppwmgen",
                _PD.TPPWME:"tppwmgen",_PD.TPPWMF:"tppwmgen",_PD.AMP8I20:"8i20",_PD.POTO:"spinout",
                _PD.POTE:"spinena",_PD.POTD:"spindir",_PD.ANALOGIN:"analog","Error":"None" }
            boardnum = int(test[4:5])
            boardname = self["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME]
            ptype = self[pin+"type"]
            if boardnum == 1 and self.mesa1_currentfirmwaredata[_PD._BOARDNAME] == self.mesa0_currentfirmwaredata[_PD._BOARDNAME]:
                halboardnum = 1
            if 'serial' in test:
                # sample pin name = mesa0sserial0_0pin24
                pinnum = int(test[18:])
                portnum = int(test[12:13])
                channel = int(test[14:15])
                subfirmname = self["mesa%dsserial%d_%dsubboard"% (boardnum, portnum, channel)]
                for subnum,temp in enumerate(_PD.MESA_DAUGHTERDATA):
                    #print "pinname search -",_PD.MESA_DAUGHTERDATA[subnum][_PD._SUBFIRMNAME],subfirmname
                    if _PD.MESA_DAUGHTERDATA[subnum][_PD._SUBFIRMNAME] == subfirmname: break
                #print "pinname -found subboard name:",_PD.MESA_DAUGHTERDATA[subnum][_PD._SUBFIRMNAME],subfirmname,subnum,"channel:",channel
                subboardname = _PD.MESA_DAUGHTERDATA[subnum][_PD._SUBBOARDNAME]
                firmptype,compnum = _PD.MESA_DAUGHTERDATA[subnum][_PD._SUBSTARTOFDATA+pinnum]
                # we iter over this dic because of locale translation problems when using
                # comptype = type_name[ptype]
                comptype = "ERROR FINDING COMPONENT TYPE"
                for key,value in type_name.iteritems():
                    if key == ptype: comptype = value
                if value == "Error":
                    print "**** ERROR PNCCONF: pintype error in make_pinname: (sserial) ptype = ",ptype
                    return None
                # if gpionumber flag is true - convert to gpio pin name
                if gpionumber or ptype in(_PD.GPIOI,_PD.GPIOO,_PD.GPIOD):
                    if "7i77" in (subboardname) or "7i76" in(subboardname)or "7i84" in(subboardname):
                        if ptype in(_PD.GPIOO,_PD.GPIOD):
                            comptype = "output"
                            if pinnum >15 and pinnum <24:
                                pinnum = pinnum-16
                            elif pinnum >39:
                                pinnum = pinnum -32
                        elif ptype == _PD.GPIOI:
                            comptype = "input"
                            if pinnum >23 and pinnum < 40:
                                pinnum = pinnum-8
                        return "hm2_%s.%d.%s.%d.%d."% (boardname,halboardnum,subboardname,portnum,channel) + comptype+"-%02d"% (pinnum)
                    elif "7i69" in (subboardname) or "7i73" in (subboardname) or "7i64" in(subboardname):
                        if ptype in(_PD.GPIOO,_PD.GPIOD):
                            comptype = "output"
                            pinnum -= 24
                        elif ptype == _PD.GPIOI:
                            comptype = "input"
                        return "hm2_%s.%d.%s.%d.%d."% (boardname,halboardnum,subboardname,portnum,channel) + comptype+"-%02d"% (pinnum)
                    elif "7i70" in (subboardname) or "7i71" in (subboardname):
                        if ptype in(_PD.GPIOO,_PD.GPIOD):
                            comptype = "output"
                        elif ptype == _PD.GPIOI:
                            comptype = "input"
                        return "hm2_%s.%d.%s.%d.%d."% (boardname,halboardnum,subboardname,portnum,channel) + comptype+"-%02d"% (pinnum)
                    else:
                        print "**** ERROR PNCCONF: subboard name ",subboardname," in make_pinname: (sserial) ptype = ",ptype,pin
                        return None
                elif ptype in (_PD.AMP8I20,_PD.POTO,_PD.POTE,_PD.POTD) or prefixonly:
                    return "hm2_%s.%d.%s.%d.%d."% (boardname,halboardnum,subboardname,portnum,channel)
                elif ptype in(_PD.PWMP,_PD.PDMP,_PD.UDMU):
                    comptype = "analogout"
                    return "hm2_%s.%d.%s.%d.%d."% (boardname,halboardnum,subboardname,portnum,channel) + comptype+"%d"% (compnum)
                elif ptype == (_PD.ANALOGIN):
                    if "7i64" in(subboardname):
                        comptype = "analog"
                    else:
                        comptype = "analogin"
                    return "hm2_%s.%d.%s.%d.%d."% (boardname,halboardnum,subboardname,portnum,channel) + comptype+"%d"% (compnum)
                elif ptype == (_PD.ENCA):
                    comptype = "enc"
                    return "hm2_%s.%d.%s.%d.%d."% (boardname,halboardnum,subboardname,portnum,channel) + comptype+"%d"% (compnum)
                else:
                    print "**** ERROR PNCCONF: pintype error in make_pinname: (sserial) ptype = ",ptype,pin
                    return None
            else:
                # sample pin name = mesa0c3pin1
                pinnum = int(test[10:])
                connum = int(test[6:7])
                # we iter over this dic because of locale translation problems when using
                # comptype = type_name[ptype]
                comptype = "ERROR FINDING COMPONENT TYPE"
                # we need concount (connector designations are not in numerical order, pin names are) and comnum from this
                for concount,i in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_PD._NUMOFCNCTRS]):
                        if i == connum:
                            dummy,compnum = self["mesa%d_currentfirmwaredata"% (boardnum)][_PD._STARTOFDATA+pinnum+(concount*24)]
                            break
                for key,value in type_name.iteritems():
                    if key == ptype: comptype = value
                if value == "Error":
                    print "**** ERROR PNCCONF: pintype error in make_pinname: (mesa) ptype = ",ptype
                    return None
                # if gpionumber flag is true - convert to gpio pin name
                if gpionumber or ptype in(_PD.GPIOI,_PD.GPIOO,_PD.GPIOD):
                    comptype = "gpio"
                    if '5i25' in boardname or '7i76e' in boardname or '7i92' in boardname:
                        compnum = int(pinnum)+(concount*17)
                    else:
                        compnum = int(pinnum)+(concount*24)
                    return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%03d"% (compnum)          
                elif ptype in (_PD.ENCA,_PD.ENCB,_PD.ENCI,_PD.ENCM,_PD.PWMP,_PD.PWMD,_PD.PWME,_PD.PDMP,_PD.PDMD,_PD.PDME,_PD.UDMU,_PD.UDMD,_PD.UDME,
                    _PD.STEPA,_PD.STEPB,_PD.STEPC,_PD.STEPD,_PD.STEPE,_PD.STEPF,
                    _PD.TPPWMA,_PD.TPPWMB,_PD.TPPWMC,_PD.TPPWMAN,_PD.TPPWMBN,_PD.TPPWMCN,_PD.TPPWME,_PD.TPPWMF):
                    return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%02d"% (compnum)
                elif ptype in (_PD.RES0,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5):
                    temp = (_PD.RES0,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5)
                    for num,dummy in enumerate(temp):
                        if ptype == dummy:break
                    return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%02d"% (compnum*6+num)
                elif ptype in (_PD.MXE0,_PD.MXE1):
                    num = 0
                    if ptype == _PD.MXE1: num = 1
                    return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%02d"% ((compnum * 2 + num))

        elif 'pp' in test:
            print test
            ending = "-out"
            test = str(pin) 
            print  self[pin]
            pintype = str(test[4:5])
            print pintype
            pinnum = int(test[8:])
            print pinnum
            connum = int(test[2:3])-1
            print connum
            if pintype == 'I': ending = "-in"
            return "parport."+str(connum)+".pin-%02d"%(pinnum)+ending
        else:
            print "pintype error in make_pinname: pinname = ",test
            return None

class App:
    def __init__(self, dbgstate=0):
        print dbgstate
        global debug
        global dbg
        global _PD
        self.debugstate = dbgstate
        dbg = self.dbg
        if self.debugstate:
           print 'PNCconf debug',dbgstate
           global _DEBUGSTRING
           _DEBUGSTRING = [dbgstate]
        self.recursive_block = False
        self.firmware_block = False
        # Private data holds the array of pages to load, signals, and messages
        _PD = self._p = private_data.Private_Data(self,BIN,BASE)
        self.d = Data(self._p)
        
        self.splash_screen()
        #self.pbar.set_fraction(.2)
        #while gtk.events_pending():
        #    gtk.main_iteration()

        bar_size = 0
        # build the glade files
        self.builder = MultiFileBuilder()
        self.builder.set_translation_domain(domain)
        self.builder.add_from_file(os.path.join(self._p.DATADIR,'main_page.glade'))
        self.builder.add_from_file(os.path.join(self._p.DATADIR,'dialogs.glade'))
        self.builder.add_from_file(os.path.join(self._p.DATADIR,'help.glade'))
        window = self.builder.get_object("window1")
        notebook1 = self.builder.get_object("notebook1")
        for name,y,z,a in (self._p.available_page):
            if name == 'intro': continue
            dbg("loading glade page REFERENCE:%s TITLE:%s INIT STATE: %s STATE:%s"% (name,y,z,a),mtype="glade")
            if not z:
                self.add_placeholder_page(name)
                page = self.builder.get_object('label_%s'%name)
                notebook1.append_page(page)
                continue
            self.builder.add_from_file(os.path.join(self._p.DATADIR, '%s.glade'%name))
            page = self.builder.get_object(name)
            notebook1.append_page(page)
            self.pbar.set_fraction(bar_size)
            while gtk.events_pending():
                gtk.main_iteration()
            bar_size += .0555
        if not 'dev' in dbgstate:
            notebook1.set_show_tabs(False)

        self.widgets = Widgets(self.builder)
        self.TESTS = tests.TESTS(self)
        self.p = pages.Pages(self)
        self.INI = build_INI.INI(self)
        self.HAL = build_HAL.HAL(self)
        self.builder.set_translation_domain(domain) # for locale translations
        self.builder.connect_signals( self.p ) # register callbacks from Pages class
        wiz_pic = gtk.gdk.pixbuf_new_from_file(self._p.WIZARD)
        self.widgets.wizard_image.set_from_pixbuf(wiz_pic)

        self.window.hide()
        axisdiagram = os.path.join(self._p.HELPDIR,"axisdiagram1.png")
        self.widgets.helppic0.set_from_file(axisdiagram)
        axisdiagram = os.path.join(self._p.HELPDIR,"lathe_diagram.png")
        self.widgets.helppic1.set_from_file(axisdiagram)
        self.map_7i76 = gtk.gdk.pixbuf_new_from_file(os.path.join(self._p.HELPDIR,"7i76_map.png"))
        self.widgets.map_7i76_image.set_from_pixbuf(self.map_7i76)
        self.map_7i77 = gtk.gdk.pixbuf_new_from_file(os.path.join(self._p.HELPDIR,"7i77_map.png"))
        self.widgets.map_7i77_image.set_from_pixbuf(self.map_7i77)
        #self.widgets.openloopdialog.hide()

        self.p.initialize()
        window.show()
        self.axis_under_test = False
        self.jogminus = self.jogplus = 0

        # set preferences if they exist
        link = short = advanced = show_pages = False
        filename = os.path.expanduser("~/.pncconf-preferences")
        if os.path.exists(filename):
            match =  open(filename).read()
            textbuffer = self.widgets.textoutput.get_buffer()
            try :
                textbuffer.set_text("%s\n\n"% filename)
                textbuffer.insert_at_cursor(match)
            except:
                pass
            version = 0.0
            d = xml.dom.minidom.parse(open(filename, "r"))
            for n in d.getElementsByTagName("property"):
                name = n.getAttribute("name")
                text = n.getAttribute('value')
                if name == "version":
                    version = eval(text)
                if name == "always_shortcut":
                    short = eval(text)
                if name == "always_link":
                    link = eval(text)
                if name == "show_advanced_pages":
                    show_pages = eval(text)
                if name == "machinename":
                    self.d._lastconfigname = text
                if name == "chooselastconfig":
                    self.d._chooselastconfig = eval(text)
                if name == "MESABLACKLIST":
                    if version == self.d._preference_version:
                        self._p.MESABLACKLIST = eval(text)
                if name == "EXTRA_MESA_FIRMWAREDATA":
                    self.d._customfirmwarefilename = text
                    rcfile = os.path.expanduser(self.d._customfirmwarefilename)
                    print rcfile
                    if os.path.exists(rcfile):
                        try:
                            execfile(rcfile)
                        except:
                            print _("**** PNCCONF ERROR:    custom firmware loading error")
                            self._p.EXTRA_MESA_FIRMWAREDATA = []
                    if not self._p.EXTRA_MESA_FIRMWAREDATA == []:
                        print _("**** PNCCONF INFO:    Found extra firmware in file")
        # these are set from the hidden preference file
        self.widgets.createsymlink.set_active(link)
        self.widgets.createshortcut.set_active(short)
        self.widgets.advancedconfig.set_active(show_pages)

        tempfile = os.path.join(self._p.DISTDIR, "configurable_options/ladder/TEMP.clp")
        if os.path.exists(tempfile):
           os.remove(tempfile)

    def add_placeholder_page(self,name):
                string = '''
                    <?xml version="1.0"?>
                    <interface>
                      <requires lib="gtk+" version="2.16"/>
                      <!-- interface-naming-policy project-wide -->
                      <object class="GtkLabel" id="label_%s">
                        <property name="visible">True</property>
                        <property name="label" translatable="yes">%s</property>
                      </object>
                    </interface>
                         '''%(name,name)
                self.builder.add_from_string(string)

# build functions
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
            shutil.copy(os.path.join(self._p.DISTDIR, filename), dest)

    def buid_config(self):
        base = self.build_base()
        self.d.save(base)
        #self.write_readme(base)
        self.INI.write_inifile(base)
        self.HAL.write_halfile(base)
        self.copy(base, "tool.tbl")
        if self.warning_dialog(self._p.MESS_QUIT,False):
            gtk.main_quit()

# helper functions

    def splash_screen(self):
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        self.window.set_type_hint(gtk.gdk.WINDOW_TYPE_HINT_SPLASHSCREEN)     
        self.window.set_title(_("Pncconf setup"))
        self.window.set_border_width(10)

        vbox = gtk.VBox(False, 5)
        vbox.set_border_width(10)
        self.window.add(vbox)
        vbox.show()
        align = gtk.Alignment(0.5, 0.5, 0, 0)
        vbox.pack_start(align, False, False, 5)
        align.show()

        self.pbar = gtk.ProgressBar()
        self.pbar.set_text(_("Pncconf is setting up"))
        self.pbar.set_fraction(.1)

        align.add(self.pbar)
        self.pbar.show()
        self.window.show()
        while gtk.events_pending():
            gtk.main_iteration()

    def dbg(self,message,mtype='all'):
        for hint in _DEBUGSTRING:
            if "all" in hint or mtype in hint:
                print(message)
                if "step" in _DEBUGSTRING:
                    c = raw_input(_("\n**** Debug Pause! ****"))
                return

    def query_dialog(self,title, message):
        def responseToDialog(entry, dialog, response):
            dialog.response(response)
        label = gtk.Label(message)
        #label.modify_font(pango.FontDescription("sans 20"))
        entry = gtk.Entry()
        dialog = gtk.MessageDialog(self.widgets.window1,
                gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_WARNING, gtk.BUTTONS_OK_CANCEL, title)

        dialog.vbox.pack_start(label)
        dialog.vbox.add(entry)
        #allow the user to press enter to do ok
        entry.connect("activate", responseToDialog, dialog, gtk.RESPONSE_OK)
        dialog.show_all()
        result = dialog.run()

        text = entry.get_text()
        dialog.destroy()
        if result ==  gtk.RESPONSE_OK:
            return text
        else:
            return None

    def warning_dialog(self,message,is_ok_type):
        if is_ok_type:
           dialog = gtk.MessageDialog(self.widgets.window1,
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

    def show_help(self):
        helpfilename = os.path.join(self._p.HELPDIR, "%s"% self.d.help)
        textbuffer = self.widgets.helpview.get_buffer()
        try :
            infile = open(helpfilename, "r")
            if infile:
                string = infile.read()
                infile.close()
                textbuffer.set_text(string)
        except:
            text = _("Specific Help page is unavailable\n")
            self.warning_dialog(text,True)
        self.widgets.help_window.set_title(_("Help Pages") )
        self.widgets.helpnotebook.set_current_page(0)
        self.widgets.help_window.show_all()
        self.widgets.help_window.present()

    def print_page(self,print_dialog, context, n, imagename):
        ctx = context.get_cairo_context()
        gdkcr = gtk.gdk.CairoContext(ctx)
        gdkcr.set_source_pixbuf(self[imagename], 0,0)
        gdkcr.paint ()

    def print_image(self,image_name):
        print 'print image'
        print_dialog = gtk.PrintOperation()
        print_dialog.set_n_pages(1)
        settings = gtk.PrintSettings()
        settings.set_orientation(gtk.PAGE_ORIENTATION_LANDSCAPE)
        print_dialog.set_print_settings(settings)
        print_dialog.connect("draw-page", self.print_page, image_name)
        res = print_dialog.run(gtk.PRINT_OPERATION_ACTION_PRINT_DIALOG, self.widgets.help_window)
        if res == gtk.PRINT_OPERATION_RESULT_APPLY:
            settings = print_dialog.get_print_settings()

    # check for realtime kernel
    def check_for_rt(self):
        actual_kernel = os.uname()[2]
        if hal.is_sim :
            self.warning_dialog(self._p.MESS_NO_REALTIME,True)
            if self.debugstate:
                return True
            else:
                return False
        elif hal.is_kernelspace and hal.kernel_version != actual_kernel:
            self.warning_dialog(self._p.MESS_KERNEL_WRONG + '%s'%hal.kernel_version,True)
            if self.debugstate:
                return True
            else:
                return False
        else:
            return True

    def add_external_folder_boardnames(self):
        if os.path.exists(self._p.FIRMDIR):
            self._p.MESA_BOARDNAMES = []
            for root, dirs, files in os.walk(self._p.FIRMDIR):
                folder = root.lstrip(self._p.FIRMDIR)
                if folder in self._p.MESABLACKLIST:continue
                if folder == "":continue
                dbg("****folder added :%s"%folder,mtype='firmware')
                self._p.MESA_BOARDNAMES.append(folder)
        else:
            #TODO what if there are no external firmware is this enough?
            self.warning_dialog(_("You have no hostmot2 firmware downloaded in folder:\n%s\n\
PNCconf will use internal firmware data"%self._p.FIRMDIR),True)

        self._p.MESA_BOARDNAMES.append('5i25-Internal Data')
        self._p.MESA_BOARDNAMES.append('7i76e-Internal Data')
        self._p.MESA_BOARDNAMES.append('7i92-Internal Data')
        self._p.MESA_BOARDNAMES.append('7i80HD-Internal Data')
        if self.d.advanced_option:
            self._p.MESA_BOARDNAMES.append('Discovery Option')
        # add any extra firmware boardnames from .pncconf-preference file 
        if not self._p.EXTRA_MESA_FIRMWAREDATA == []:
            for search, item in enumerate(self._p.EXTRA_MESA_FIRMWAREDATA):
                d = self._p.EXTRA_MESA_FIRMWAREDATA[search]
                if not d[_PD._BOARDTITLE] in self._p.MESA_BOARDNAMES:
                    self._p.MESA_BOARDNAMES.append(d[_PD._BOARDTITLE])
        model = self.widgets.mesa_boardname_store
        model.clear()
        for search,item in enumerate(self._p.MESA_BOARDNAMES):
            #print search,item
            model.append((item,))

    def fill_pintype_model(self):
        # notused
        self.d._notusedliststore = gtk.ListStore(str,int)
        self.d._notusedliststore.append([_PD.pintype_notused[0],0])
        # gpio
        self.d._gpioliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_gpio):
            self.d._gpioliststore.append([text,0])
        # stepper
        self.d._stepperliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_stepper):
            self.d._stepperliststore.append([text,number])
        # encoder
        self.d._encoderliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_encoder):
            self.d._encoderliststore.append([text,number])
        # mux encoder
        self.d._muxencoderliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_muxencoder):
            self.d._muxencoderliststore.append([text,number])
        # resolver
        self.d._resolverliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_resolver):
            self.d._resolverliststore.append([text,number])
        # 8i20 AMP
        self.d._8i20liststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_8i20):
            self.d._8i20liststore.append([text,number])
        # potentiometer output
        self.d._potliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_potentiometer):
            self.d._potliststore.append([text,number])
        # analog input
        self.d._analoginliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_analog_in):
            self.d._analoginliststore.append([text,number])
        # pwm
        self.d._pwmrelatedliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_pwm):
            self.d._pwmrelatedliststore.append([text,number])
        self.d._pwmcontrolliststore = gtk.ListStore(str,int)
        self.d._pwmcontrolliststore.append([_PD.pintype_pwm[0],0])
        self.d._pwmcontrolliststore.append([_PD.pintype_pdm[0],0])
        self.d._pwmcontrolliststore.append([_PD.pintype_udm[0],0])
        # pdm
        self.d._pdmrelatedliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_pdm):
            self.d._pdmrelatedliststore.append([text,number])
        self.d._pdmcontrolliststore = gtk.ListStore(str,int)
        self.d._pdmcontrolliststore.append([_PD.pintype_pwm[0],0])
        self.d._pdmcontrolliststore.append([_PD.pintype_pdm[0],0])
        self.d._pdmcontrolliststore.append([_PD.pintype_udm[0],0])
        # udm
        self.d._udmrelatedliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_udm):
            self.d._udmrelatedliststore.append([text,number])
        self.d._udmcontrolliststore = gtk.ListStore(str,int)
        self.d._udmcontrolliststore.append([_PD.pintype_pwm[0],0])
        self.d._udmcontrolliststore.append([_PD.pintype_pdm[0],0])
        self.d._udmcontrolliststore.append([_PD.pintype_udm[0],0])
        #tppwm
        self.d._tppwmliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_tp_pwm):
            self.d._tppwmliststore.append([text,number])
        #sserial
        self.d._sserialliststore = gtk.ListStore(str,int)
        for number,text in enumerate(_PD.pintype_sserial):
            self.d._sserialliststore.append([text,number])

    # comboboxes with 3 levels
    def fill_combobox_models2(self):
        templist = [ ["_gpioisignaltree",_PD.human_input_names,1,'hal_input_names'],
                    ["_steppersignaltree",_PD.human_stepper_names,1,'hal_stepper_names'],
                    ["_encodersignaltree",_PD.human_encoder_input_names,1,'hal_encoder_input_names'],
                    ["_muxencodersignaltree",_PD.human_encoder_input_names,1,'hal_encoder_input_names'],
                    ["_pwmsignaltree",_PD.human_pwm_output_names,1,'hal_pwm_output_names'],]
        for item in templist:
            #print "\ntype",item[0]
            count = 0
            end = len(item[1])-1
            # treestore(parentname,parentnum,signalname,signaltreename,signal index number)
            self.d[item[0]]= gtk.TreeStore(str,int,str,str,int)
            for i,parent in enumerate(item[1]):
                ############################
                # if there are no children:
                ############################
                if not isinstance(parent[1], list):
                    signame = parent[1]
                    index = _PD[item[3]].index(parent[1])
                    #print 'no children:', signame, index
                    # add parent and get reference for child
                    # This entry is selectable it has a signal attached to it
                    piter = self.d[item[0]].append(None, [parent[0], index,signame,item[3],0])
                    #print parent,parentnum,count,signame,item[3],i,signame,count
                else:
                    # If list is empty it's a custome signal - with no signals yet
                  if len(parent[1]) == 0:
                    piter = self.d[item[0]].append(None, [parent[0], 0,'none',item[3],0])
                  else:
                    #print "parsing child",parent[1]
                    # add parent title
                    ##########################
                    # if there are children:
                    # add an entry to first list that cannot be selected 
                    # (well it always gives the unused signal - 0)
                    # because we need users to select from the next column
                    ##########################
                    piter = self.d[item[0]].append(None, [parent[0],0,signame,item[3],0])
                    for j,child in enumerate(parent[1]):
                        #############################
                        # If grandchildren
                        #############################
                        if isinstance(child[1], list):
                            ##########################
                            # if there are children:
                            # add an entry to second list that cannot be selected 
                            # (well it always gives the unused signal - 0)
                            # because we need users to select from the next column
                            ##########################
                            citer = self.d[item[0]].append(piter, [child[0], 0,signame,item[3],0])
                            #print 'add to CHILD list',child[0]
                            #print 'String:',child[1]
                            
                            for k,grandchild in enumerate(child[1]):
                                #print 'raw grand:  ', grandchild
                                #############################
                                # If GREAT children
                                #############################
                                #print grandchild[0],grandchild[1]
                                if isinstance(grandchild[1], list):
                                    #print 'ERROR combo boxes can not have GREAT children yet add'
                                    #print 'skipping'
                                    continue
                                else:
                                    #############################
                                    # If No GREAT children
                                    ############################

                                    humanName = grandchild[0]
                                    sigName = grandchild[1]
                                    index = _PD[item[3]].index(grandchild[1])
                                    halNameArray = item[3]
                                    #print 'adding to grandchild to childlist:  ', humanName,index,sigName,halNameArray,index
                                    self.d[item[0]].append(citer, [humanName, index,sigName,halNameArray,index])
                        ####################
                        # No grandchildren
                        ####################
                        else:
                            #print' add to child - no grandchild',child
                            humanName = child[0]
                            sigName = child[1]
                            index = _PD[item[3]].index(child[1])
                            halNameArray = item[3]
                            #print child[0],index,sigName,item[3],index
                            self.d[item[0]].append(piter, [humanName, index,sigName,halNameArray,index])
                            count +=item[2]

    # combobox with 2 levels
    def fill_combobox_models(self):
        templist = [ ["_gpioosignaltree",_PD.human_output_names,1,'hal_output_names'],
                     
                     
                     ["_resolversignaltree",_PD.human_resolver_input_names,1,'hal_resolver_input_names'],
                     ["_tppwmsignaltree",_PD.human_tppwm_output_names,8,'hal_tppwm_output_names'],
                     ["_8i20signaltree",_PD.human_8i20_input_names,1,'hal_8i20_input_names'],
                     ["_potsignaltree",_PD.human_pot_output_names,2,'hal_pot_output_names'],
                     ["_analoginsignaltree",_PD.human_analog_input_names,1,'hal_analog_input_names'],
                     ["_sserialsignaltree",_PD.human_sserial_names,3,'hal_sserial_names']
                   ]
        for item in templist:
            #print "\ntype",item[0]
            count = 0
            end = len(item[1])-1
            # treestore(parentname,parentnum,signalname,signaltreename,signal index number)
            self.d[item[0]]= gtk.TreeStore(str,int,str,str,int)
            for i,parent in enumerate(item[1]):
                ############################
                # if there are no children:
                ############################
                if len(parent[1]) == 0:
                    # if combobox has a 'custom' signal choice then the index must be 0
                    if i == end and not item[0] =="_sserialsignaltree":parentnum = 0 
                    else:parentnum = count
                    #print "length of human names:",len(parent[1])
                    # this adds the index number (parentnum) of the signal
                    try:
                        signame=_PD[item[3]][count]
                    except:
                        signame = 'none'
                    # add parent and get reference for child
                    piter = self.d[item[0]].append(None, [parent[0], parentnum,signame,item[3],count])
                    #print parent,parentnum,count,signame,item[3],i,signame,count
                    if count == 0: count = 1
                    else: count +=item[2]
                ##########################
                # if there are children:
                ##########################
                else:
                    #print "parsing child",signame
                    # add parent title
                    piter = self.d[item[0]].append(None, [parent[0],0,signame,item[3],count])
                    for j,child in enumerate(parent[1]):
                        #print len(child[1]), child[0]
                        #if item[0] =='_gpioisignaltree':
                            #print item[0], child[0],len(child[1])
                        #############################
                        # If grandchildren
                        #############################
                        if len(child[1]) > 1:
                            # add child and get reference
                            citer = self.d[item[0]].append(piter, [child[0], 0,signame,item[3],count])
                            #if item[0] =='_gpioisignaltree':
                                #print 'add to CHILD list',child[0]
                                #print 'Strig:',child[1]
                            for k,grandchild in enumerate(child[1]):
                                #print 'raw grand:  ', grandchild
                                #############################
                                # If greatchildren
                                #############################
                                #print grandchild[0],grandchild[1]
                                if len(grandchild) > 1:
                                    #print 'add to grandchild child list',grandchild[0]
                                    index = _PD[item[3]].index(grandchild[1])
                                    self.d[item[0]].append(citer, [grandchild[0],index,grandchild[1],item[3],index])
                                    continue
                                else:
                                    #############################
                                    # If No greatchildren
                                    #############################
                                    try:
                                        signame=_PD[item[3]][count]
                                    except:
                                        signame = 'none'
                                    #print 'adding to grandchild to childlist:  ', grandchild,signame,item[3],count
                                    # add grandchild
                                    self.d[item[0]].append(piter, [child,0,signame,item[3],count])
                                    #count +=item[2]

                        ####################
                        # No grandchildren
                        ####################
                        else:
                            #print' add to child - no grandchild',child
                            signame=_PD[item[3]][count]
                            #print i,count,parent[0],child,signame,item[3], _PD[item[3]].index(signame),count
                            self.d[item[0]].append(piter, [child, count,signame,item[3],count])
                            count +=item[2]
                            
        self.fill_combobox_models2()
        self.d._notusedsignaltree = gtk.TreeStore(str,int,str,str,int)
        self.d._notusedsignaltree.append(None, [_PD.human_notused_names[0][0],0,'unused-unused','_notusedsignaltree',0])
        # make a filter for sserial encoder as they can't be used for AXES
        self.d._encodersignalfilter = self.d._encodersignaltree.filter_new()
        self.d._enc_filter_list = ['Axis Encoder']
        self.d._encodersignalfilter.set_visible_func(self.visible_cb, self.d._enc_filter_list)
        # build filters for the 'controlling' sserial combbox
        # We need to limit selections often
        for channel in range(0,_PD._NUM_CHANNELS):
            self.d['_sserial%d_filter_list'%channel] =[]
            self.d['_sserial%d_signalfilter'%channel] = self.d._sserialsignaltree.filter_new()
            self.d['_sserial%d_signalfilter'%channel].set_visible_func(self.filter_cb,self.d['_sserial%d_filter_list'%channel])
            self.set_filter('_sserial%d'%channel,'ALL')

    # Filter out any matching names in a list
    def visible_cb(self, model, iter, data ):
        #print model.get_value(iter, 0) ,data
        return not model.get_value(iter, 0) in data

    # filter out anything not in one of the lists, the list depending on a keyword 
    def set_filter(self,sserial,data):
        keyword = data.upper()
        if keyword == '7I77':
            f_list = ['Unused','7i77']
        elif keyword == '7I76':
            f_list = ['Unused','7i76']
        else:
            f_list = ['Unused','7i73','7i69','8i20','7i64','7i71','7i70','7i84']
        del self.d['%s_filter_list'%sserial][:]
        for i in(f_list):
            self.d['%s_filter_list'%sserial].append(i)
        #print '\n',filterlist,self.d[filterlist]
        self.d['%s_signalfilter'%sserial].refilter()

    # Filter callback
    def filter_cb(self, model, iter, data ):
        #print model.get_value(iter, 0) ,data
        for i in data:
            if i in model.get_value(iter, 0):
                return True
        return False

    def load_config(self):
        filter = gtk.FileFilter()
        filter.add_pattern("*.pncconf")
        filter.set_name(_("LinuxCNC 'PNCconf' configuration files"))
        dialog = gtk.FileChooserDialog(_("Modify Existing Configuration"),
            self.widgets.window1, gtk.FILE_CHOOSER_ACTION_OPEN,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
             gtk.STOCK_OPEN, gtk.RESPONSE_OK))
        dialog.set_default_response(gtk.RESPONSE_OK)
        dialog.add_filter(filter) 
        if not self.d._lastconfigname == "" and self.d._chooselastconfig:
            dialog.set_filename(os.path.expanduser("~/linuxcnc/configs/%s.pncconf"% self.d._lastconfigname))
        dialog.add_shortcut_folder(os.path.expanduser("~/linuxcnc/configs"))
        dialog.set_current_folder(os.path.expanduser("~/linuxcnc/configs"))
        dialog.show_all()
        result = dialog.run()
        if result == gtk.RESPONSE_OK:
            filename = dialog.get_filename()
            dialog.destroy()
            self.d.load(filename, self)
            self.d._mesa0_configured = False
            self.d._mesa1_configured = False
            try:
                # check that the firmware is current enough by checking the length of a sub element and that the other is an integer.
                for boardnum in(0,1):
                    i=j=None
                    i = len(self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS])
                    j = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._HIFREQ]+100 # throws an error if not an integer.
                    if not i > 1:
                        print i,j,boardnum
                        raise UserWarning
            except :
                print i,j,boardnum
                self.warning_dialog(_("It seems data in this file is from too old of a version of PNCConf to continue.\n."),True)
                return True
        else:
            dialog.destroy()
            return True

    def mesa_firmware_search(self,boardtitle,*args):
        #TODO if no firm packages set up for internal data?
        #TODO don't do this if the firmware is already loaded
        self.pbar.set_text("Loading external firmware")
        self.pbar.set_fraction(0)
        self.window.show()
        while gtk.events_pending():
            gtk.main_iteration()
        firmlist = []
        for root, dirs, files in os.walk(self._p.FIRMDIR):
            folder = root.lstrip(self._p.FIRMDIR)
            #dbg('Firmware folder:%s'% folder)
            if folder in self._p.MESABLACKLIST:continue
            if not folder == boardtitle:continue
            for n,name in enumerate(files):
                if name in self._p.MESABLACKLIST:continue
                if ".xml" in name:
                    dbg('%s'% name)
                    temp = name.rstrip(".xml")
                    firmlist.append(temp)
        dbg("\nXML list:%s"%firmlist,mtype="firmname")
        for n,currentfirm in enumerate(firmlist):
            self.pbar.set_fraction(n*1.0/len(firmlist))
            while gtk.events_pending():
                gtk.main_iteration()
            # XMLs don't tell us the driver type so set to None (parse will guess)
            firmdata = self.parse_xml(None,boardtitle, currentfirm,os.path.join(
                                self._p.FIRMDIR,boardtitle,currentfirm+".xml"))
            self._p.MESA_FIRMWAREDATA.append(firmdata)
        self.window.hide()

    def parse_xml(self, driver, boardtitle, firmname, xml_path):
            def search(elementlist):
                for i in elementlist:
                    temp = root.find(i)
                    if temp is not None:
                        return temp.text
                return temp

            root = xml.etree.ElementTree.parse(xml_path)
            watchdog = encoder = resolver = pwmgen = led = muxedqcount = 0
            stepgen = tppwmgen = sserialports = sserialchannels = 0
            numencoderpins = numpwmpins = 3; numstepperpins = 2; numttpwmpins = 0; numresolverpins = 10

            text = search(('boardname','BOARDNAME'))
            if text == None:
                print 'Missing info: boardname'
                return
            boardname = text.lower()
            #dbg("\nBoard and firmwarename:  %s %s\n"%( boardname, firmname), "firmraw")

            text  = search(("IOPORTS","ioports")) ; #print numcnctrs
            if text == None:
                print 'Missing info: ioports'
                return
            numcnctrs = int(text)
            text = search(("PORTWIDTH","portwidth"))
            if text == None:
                print 'Missing info: portwidth'
                return
            portwidth = int(text)
            maxgpio  = numcnctrs * portwidth ; #print maxgpio
            placeholders = 24-portwidth
            text = search(("CLOCKLOW","clocklow")) ; #print lowfreq
            if text == None:
                print 'Missing info: clocklow'
                return
            lowfreq = int(text)/1000000
            text = search(("CLOCKHIGH","clockhigh")); #print hifreq
            if text == None:
                print 'Missing info: clockhigh'
                return
            hifreq = int(text)/1000000
            modules = root.findall(".//modules")[0]
            if driver == None:
                if "7i43" in boardname:
                    driver = "hm2_7i43"
                elif "7i90" in boardname:
                    driver = "hm2_7i90"
                elif '7i76e' in boardname or '7i92' in boardname or '7i80' in boardname:
                    driver = 'hm2_eth'
                else:
                    driver = 'hm2_pci'
            for i,j in enumerate(modules):
                k = modules[i].find("tagname").text
                print k
                if k in ("Watchdog","WatchDog","WATCHDOG"): 
                    l = modules[i].find("numinstances").text;#print l,k
                    watchdog = int(l)
                elif k in ("Encoder","QCOUNT"): 
                    l = modules[i].find("numinstances").text;#print l,k
                    encoder = int(l)
                elif k in ("ResolverMod","RESOLVERMOD"):
                    l = modules[i].find("numinstances").text;#print l,k
                    resolver = int(l)
                elif k in ("PWMGen","PWMGEN","PWM"):
                    l = modules[i].find("numinstances").text;#print l,k
                    pwmgen = int(l)
                elif k == "LED": 
                    l = modules[i].find("numinstances").text;#print l,k
                    led = int(l)
                elif k in ("MuxedQCount","MUXEDQCOUNT"): 
                    l = modules[i].find("numinstances").text;#print l,k
                    muxedqcount = int(l)
                elif k in ("StepGen","STEPGEN"): 
                    l = modules[i].find("numinstances").text;#print l,k
                    stepgen = int(l)
                elif k in ("TPPWM","TPPWM"): 
                    l = modules[i].find("numinstances").text;#print l,k
                    tppwmgen = int(l)
                elif k in ("SSerial","SSERIAL"):
                    l = modules[i].find("numinstances").text;#print l,k
                    sserialports = int(l)
                elif k in ("None","NONE"): 
                    l = modules[i].find("numinstances").text;#print l,k
                elif k in ("IOPort","AddrX","MuxedQCountSel"):
                    continue
                else:
                    print "**** WARNING: Pncconf parsing firmware: tagname (%s) not reconized"% k

            discov_sserial = []
            ssname = root.findall("SSERIALDEVICES/SSERIALFUNCTION")
            for i in (ssname):
                port = i.find("PORT").text
                dev = i.find("DEVICE").text
                chan = i.find("CHANNEL").text
                discov_sserial.append((int(port),int(chan),dev))
            print 'discovered sserial:', discov_sserial

            pins = root.findall(".//pins")[0]
            temppinlist = []
            tempconlist = []
            pinconvertenc = {"PHASE A":_PD.ENCA,"PHASE B":_PD.ENCB,"INDEX":_PD.ENCI,"INDEXMASK":_PD.ENCM,
                "QUAD-A":_PD.ENCA,"QUAD-B":_PD.ENCB,"QUAD-IDX":_PD.ENCI,
                "MUXED PHASE A":_PD.MXE0,"MUXED PHASE B":_PD.MXE1,"MUXED INDEX":_PD.MXEI,
                "MUXED INDEX MASK":_PD.MXEM,"MUXED ENCODER SELECT 0":_PD.MXES,"MUXED ENCODER SELEC":_PD.MXES,
                "MUXQ-A":_PD.MXE0,"MUXQ-B":_PD.MXE1,"MUXQ-IDX":_PD.MXEI,"MUXSEL0":_PD.MXES}
            pinconvertresolver = {"RESOLVER POWER ENABLE":_PD.RESU,"RESOLVER SPIDI 0":_PD.RES0,
                 "RESOLVER SPIDI 1":_PD.RES1,"RESOLVER ADC CHANNEL 2":_PD.RES2,"RESOLVER ADC CHANNEL 1":_PD.RES3,
                 "RESOLVER ADC CHANNEL 0":_PD.RES4,"RESOLVER SPI CLK":_PD.RES5,"RESOLVER SPI CHIP SELECT":_PD.RESU,
                 "RESOLVER PDMM":_PD.RESU,"RESOLVER PDMP":_PD.RESU}
            pinconvertstep = {"STEP":_PD.STEPA,"DIR":_PD.STEPB,"STEP/TABLE1":_PD.STEPA,"DIR/TABLE2":_PD.STEPB}
                #"StepTable 2":STEPC,"StepTable 3":STEPD,"StepTable 4":STEPE,"StepTable 5":STEPF
            pinconvertppwm = {"PWM/UP":_PD.PWMP,"DIR/DOWN":_PD.PWMD,"ENABLE":_PD.PWME,
                    "PWM":_PD.PWMP,"DIR":_PD.PWMD,"/ENABLE":_PD.PWME}
            pinconverttppwm = {"PWM A":_PD.TPPWMA,"PWM B":_PD.TPPWMB,"PWM C":_PD.TPPWMC,
                "PWM /A":_PD.TPPWMAN,"PWM /B":_PD.TPPWMBN,"PWM /C":_PD.TPPWMCN,
                "FAULT":_PD.TPPWMF,"ENABLE":_PD.TPPWME}
            pinconvertsserial = {"RXDATA1":_PD.RXDATA0,"TXDATA1":_PD.TXDATA0,"TXE1":_PD.TXEN0,"TXEN1":_PD.TXEN0,
                                "RXDATA2":_PD.RXDATA1,"TXDATA2":_PD.TXDATA1,"TXE2":_PD.TXEN1,"TXEN2":_PD.TXEN1,
                                "RXDATA3":_PD.RXDATA2,"TXDATA3":_PD.TXDATA2,"TXE3":_PD.TXEN2,"TXEN3":_PD.TXEN2,
                                "RXDATA4":_PD.RXDATA3,"TXDATA4":_PD.TXDATA3,"TXE4":_PD.TXEN3,"TXEN4":_PD.TXEN3,
                                "RXDATA5":_PD.RXDATA4,"TXDATA5":_PD.TXDATA4,"TXE5":_PD.TXEN4,"TXEN4":_PD.TXEN4,
                                "RXDATA6":_PD.RXDATA5,"TXDATA6":_PD.TXDATA5,"TXE6":_PD.TXEN5,"TXEN6":_PD.TXEN5,
                                "RXDATA7":_PD.RXDATA6,"TXDATA7":_PD.TXDATA6,"TXE7":_PD.TXEN6,"TXEN7":_PD.TXEN6,
                                "RXDATA8":_PD.RXDATA7,"TXDATA8":_PD.TXDATA7,"TXE8":_PD.TXEN7,"TXEN8":_PD.TXEN7}
            pinconvertnone = {"NOT USED":_PD.GPIOI}

            count = 0
            for i,j in enumerate(pins):
                instance_num = 9999
                is_gpio = False
                temppinunit = []
                temp = pins[i].find("connector").text
                tempcon = int(temp.strip("P"))
                tempfunc = pins[i].find("secondaryfunctionname").text
                tempfunc = tempfunc.upper() # normalise capitalization: Peters XMLs are different from linuxcncs

                if "(IN)" in tempfunc:
                    tempfunc = tempfunc.rstrip(" (IN)")
                elif "(OUT" in tempfunc:
                    tempfunc = tempfunc.rstrip(" (OUT)")
                convertedname = "Not Converted"
                # this converts the XML file componennt names to pncconf's names

                try:
                    secmodname = pins[i].find("secondarymodulename")
                    modulename = secmodname.text.upper()
                    dbg("secondary modulename:  %s, %s."%( tempfunc,modulename), "firmraw")
                    if modulename in ("ENCODER","QCOUNT","MUXEDQCOUNT","MUXEDQCOUNTSEL"):
                        convertedname = pinconvertenc[tempfunc]
                    elif modulename in ("ResolverMod","RESOLVERMOD"):
                        convertedname = pinconvertresolver[tempfunc]
                    elif modulename in ("PWMGen","PWMGEN","PWM"):
                        convertedname = pinconvertppwm[tempfunc]
                    elif modulename in ("StepGen","STEPGEN"):
                        convertedname = pinconvertstep[tempfunc]
                    elif modulename in ("TPPWM","TPPWM"):
                        convertedname = pinconverttppwm[tempfunc]
                    elif modulename in ("SSerial","SSERIAL"):
                        temp = pins[i].find("foundsserialdevice")
                        if temp is not None:
                            founddevice = temp.text.upper()
                        else:
                            founddevice = None
                        print tempfunc,founddevice
                        # this auto selects the sserial 7i76 mode 0 card for sserial 0 and 2
                        # as the 5i25/7i76 uses some of the sserial channels for it's pins.
                        if boardname in ("5i25","7i92"):
                            if "7i77_7i76" in firmname:
                                if tempfunc == "TXDATA1": convertedname = _PD.SS7I77M0
                                elif tempfunc == "TXDATA2": convertedname = _PD.SS7I77M1
                                elif tempfunc == "TXDATA4": convertedname = _PD.SS7I76M3
                                else: convertedname = pinconvertsserial[tempfunc]
                                #print "XML ",firmname, tempfunc,convertedname
                            elif "7i76x2" in firmname or "7i76x1" in firmname:
                                if tempfunc == "TXDATA1": convertedname = _PD.SS7I76M0
                                elif tempfunc == "TXDATA3": convertedname = _PD.SS7I76M2
                                else: convertedname = pinconvertsserial[tempfunc]
                                #print "XML ",firmname, tempfunc,convertedname
                            elif "7i77x2" in firmname or "7i77x1" in firmname:
                                if tempfunc == "TXDATA1": convertedname = _PD.SS7I77M0
                                elif tempfunc == "TXDATA2": convertedname = _PD.SS7I77M1
                                elif tempfunc == "TXDATA4": convertedname = _PD.SS7I77M3
                                elif tempfunc == "TXDATA5": convertedname = _PD.SS7I77M4
                                else: convertedname = pinconvertsserial[tempfunc]
                                #print "XML ",firmname, tempfunc,convertedname
                            elif founddevice == "7I77-0": convertedname = _PD.SS7I77M0
                            elif founddevice == "7I77-1": convertedname = _PD.SS7I77M1
                            elif founddevice == "7I77-3": convertedname = _PD.SS7I77M3
                            elif founddevice == "7I77-4": convertedname = _PD.SS7I77M4
                            elif founddevice == "7I76-0": convertedname = _PD.SS7I76M0
                            elif founddevice == "7I76-2": convertedname = _PD.SS7I76M2
                            elif founddevice == "7I76-3": convertedname = _PD.SS7I76M3
                            else: convertedname = pinconvertsserial[tempfunc]
                        else:
                            convertedname = pinconvertsserial[tempfunc]
                    elif modulename in ("None","NONE"):
                        is_gpio = True
                        #convertedname = pinconvertnone[tempfunc]
                    else: is_gpio = True
                except:
                    is_gpio = True
                    exc_type, exc_value, exc_traceback = sys.exc_info()
                    formatted_lines = traceback.format_exc().splitlines()
                    print
                    print "****pncconf verbose XML parse debugging:",formatted_lines[0]
                    traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
                    print formatted_lines[-1]

                if is_gpio:
                    # must be GPIO pins if there is no secondary mudule name
                    # or if pinconvert fails eg. StepTable instance default to GPIO 
                    temppinunit.append(_PD.GPIOI)
                    temppinunit.append(0) # 0 signals to pncconf that GPIO can changed to be input or output
                else:
                    instance_num = int(pins[i].find("secondaryinstance").text)
                    # this is a workaround for the 7i77_7i776 firmware. it uses a mux encoder for the 7i76 but only uses half of it
                    # this is because of a limitation of hostmot2 - it can't have mux encoders and regular encoders
                    # so in pncconf we look for this and change it to a regular encoder.
                    if boardname == "5i25" and firmname == "7i77_7i76":
                        if modulename in ("MuxedQCount","MUXEDQCOUNT") and  instance_num == 3:
                            instance_num = 6
                            encoder =-1
                            if convertedname == _PD.MXE0: convertedname = _PD.ENCA
                            elif convertedname == _PD.MXE1: convertedname = _PD.ENCB
                            elif convertedname == _PD.MXEI: convertedname = _PD.ENCI
                    temppinunit.append(convertedname)
                    if tempfunc in("MUXED ENCODER SELECT 0","MUXEDQCOUNTSEL") and  instance_num == 6:
                        instance_num = 3
                    temppinunit.append(instance_num)
                    tempmod = pins[i].find("secondarymodulename").text
                    tempfunc = tempfunc.upper()# normalize capitalization
                    #dbg("secondary modulename, function:  %s, %s."%( tempmod,tempfunc), "firmraw")
                    if tempmod in("Encoder","MuxedQCount") and tempfunc in ("MUXED INDEX MASK (IN)","INDEXMASK (IN)"):
                        numencoderpins = 4
                    if tempmod in("SSerial","SSERIAL") and tempfunc in ("TXDATA1","TXDATA2","TXDATA3",
                            "TXDATA4","TXDATA5","TXDATA6","TXDATA7","TXDATA8"):
                        sserialchannels +=1
                #dbg("temp: %s, converted name: %s. num %d"%( tempfunc,convertedname,instance_num), "firmraw")
                if not tempcon in tempconlist:
                    tempconlist.append(tempcon)
                temppinlist.append(temppinunit)
                # add NONE place holders for boards with less then 24 pins per connector.
                if not placeholders == 0 and i == (portwidth + count-1):
                    #print "loop %d"% i
                    count =+ portwidth
                    #print "count %d" % count
                    for k in range(0,placeholders):
                        #print "%d fill here with %d parts"% (k,placeholders)
                        temppinlist.append((_PD.NUSED,0))
            if not sserialchannels == 0:
                sserialchannels +=1
            temp = [boardtitle,boardname,firmname,boardtitle,driver,encoder + muxedqcount,
                    numencoderpins,resolver,numresolverpins,pwmgen,numpwmpins,
                    tppwmgen,numttpwmpins,stepgen,numstepperpins,
                    sserialports,sserialchannels,discov_sserial,0,0,0,0,0,0,0,watchdog,maxgpio,
                    lowfreq,hifreq,tempconlist]
            for i in temppinlist:
                temp.append(i)
            if "5i25" in boardname :
                dbg("5i25 firmware:\n%s\n"%( temp), mtype="5i25")
            print 'firm added:\n',temp
            return temp

    def discover_mesacards(self):
        name = self.query_dialog('Discovery Search','Specify device name')
        if name is None: return
        print 'try to discover board:',name
        if not name:
            name = '5i25'
        info = self.call_mesaflash(name)
        print 'INFO:',info,'<-'
        if info is None: return None
        lines = info.splitlines()
        try:
            if 'ERROR' in lines[0]:
                raise ValueError('Mesaflash Error')
        except ValueError as err:
            text = err.args
            self.warning_dialog(text[0],True)
            return
        except:
            self.warning_dialog('Unspecified Error with Mesaflash',True)
            return
        if 'No' in lines[0] and 'board found' in lines[0] :
            text = _("No board was found\n")
            self.warning_dialog(text,True)
            print 'OOPS no board found!'
            return None
        return info

    def call_mesaflash(self, devicename):
        print 'DEVICE NAME SPECIFIED',devicename
        # 7i43 needs it's firmware loaded before it can be 'discovered'
        if '7i43' in devicename.lower():
            devicename = '7i43 --epp'

            halrun = os.popen("halrun -Is > /dev/null", "w")
            halrun.write("echo\n")
            load,read,write = self.hostmot2_command_string()
            # do I/O load commands
            for i in load:
                halrun.write('%s\n'%i)
            halrun.flush()
            time.sleep(.001)
            halrun.close()
        elif '7i90' in devicename.lower():
            devicename = '7i90 --epp'

        cmd ="""gksudo "sh -c 'mesaflash --device %s';'mesaflash --device %s --sserial';'mesaflash --device %s --readhmid' " """%(devicename,devicename,devicename)
        #cmd ="""  mesaflash --device %s;mesaflash --device %s --sserial;mesaflash --device %s --readhmid  """%(devicename,devicename,devicename)

        discover = subprocess.Popen([cmd], shell=True,stdin=subprocess.PIPE,stdout=subprocess.PIPE )
        output = discover.communicate()[0]
        if output == '': return None
        textbuffer = self.widgets.textoutput.get_buffer()
        try :         
            textbuffer.set_text(output)
            self.widgets.helpnotebook.set_current_page(2)
            self.widgets.help_window.show_all()
        except:
            text = _("Discovery is  unavailable\n")
            self.warning_dialog(text,True)
        print 'cmd=',cmd
        return output

    def parse_discovery(self,info,boardnum=0):
        DRIVER = BOARDNAME = ''
        WATCHDOG = NUMCONS = NUMCONPINS = ENCODERS = MUXENCODERS = 0
        RESOLVERS = NUMSSCHANNELS = SSERIALPORTS = 0
        PWMGENS = LEDS = STEPGENS = TPPWMGEN = 0
        NUMENCODERPINS = NUMPWMPINS = 3; NUMSTEPPERPINS = 2
        NUMTPPWMPINS = 0;NUMRESOLVERPINS = 10

        DOC = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, 'hostmot2', None)
        ELEMENT = DOC.documentElement

        def add_element(ELEMENT,name):
            n1 = DOC.createElement(name)
            ELEMENT.appendChild(n1)
            return n1

        def add_text(root,title,value):
            n = DOC.createElement(title)
            root.appendChild(n)
            nodeText = DOC.createTextNode( value )
            n.appendChild(nodeText)
            return n

        info = info.upper()
        lines = info.splitlines()
        sserial=[]
        ssflag = pinsflag = True
        dev7i77flag = dev7i76flag = False
        for l_num,i in enumerate(lines):
            i = i.lstrip()
            temp2 = i.split(" ")
            #print i,temp2
            if 'ETH' in i:
                DRIVER = 'hm2_eth'
            if 'PCI' in i:
                DRIVER = 'hm2_pci'
            if 'BOARDNAME' in i:
                BOARDNAME = temp2[2].strip('MESA').lower()
                add_text(ELEMENT,'BOARDNAME',BOARDNAME)
            if 'DEVICE AT' in i:
                if ssflag:
                    n1 = add_element(ELEMENT,'SSERIALDEVICES')
                    ssflag = False
                for num,i in enumerate(temp2):
                    if i =="CHANNEL":
                        sserial.append((temp2[num+1].strip(':'),temp2[num+2]))
                        n2 = add_element(n1,'SSERIALFUNCTION')
                        add_text(n2,'PORT','0')
                        add_text(n2,'CHANNEL',temp2[num+1].strip(':'))
                        add_text(n2,'DEVICE',temp2[num+2])
                        if '7I77' in(temp2[num+2]):
                            dev7i77flag = True
                        elif '7I76' in(temp2[num+2]):
                            dev7i76flag = True
            if 'SSLBP CHANNELS:' in i:
                NUMSSCHANNELS = temp2[2]
            if 'CLOCK LOW FREQUENCY: ' in i:
                add_text(ELEMENT,'CLOCKLOW',str(int(float(temp2[3])*1000000)))
            if 'CLOCK HIGH FREQUENCY:' in i:
                add_text(ELEMENT,'CLOCKHIGH',str(int(float(temp2[3])*1000000)))
            if 'NUMBER OF IO PORTS:' in i:
                NUMCONS = temp2[4]
                add_text(ELEMENT,'IOPORTS',NUMCONS)
            if 'WIDTH OF ONE I/O PORT:' in i:
                NUMCONPINS = temp2[5]
                add_text(ELEMENT,'PORTWIDTH',NUMCONPINS)

            if 'MODULES IN CONFIGURATION:' in i:
                mod_ele = add_element(ELEMENT,'modules')
                modflag = True
            if 'MODULE: WATCHDOG' in i:
                tline = lines[l_num+1].split(" ")
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','WATCHDOG')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'MODULE: QCOUNT' in i:
                tline = lines[l_num+1].split(" ")
                ENCODERS = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','QCOUNT')
                add_text(new,'numinstances',tline[4].lstrip())

            if 'MODULE: MUXEDQCOUNTSEL' in i:
                continue
            if 'MODULE: MUXEDQCOUNT' in i:
                tline = lines[l_num+1].split(" ")
                MUXENCODERS = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','MUXEDQCOUNT')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'MODULE: SSERIAL' in i:
                tline = lines[l_num+1].split(" ")
                SSERIALPORTS = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','SSERIAL')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'MODULE: RESOLVERMOD' in i:
                tline = lines[l_num+1].split(" ")
                RESOLVER = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','RESOLVERMOD')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'MODULE: PWM' in i:
                tline = lines[l_num+1].split(" ")
                PWMGENS = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','PWMGEN')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'MODULE: TPPWM' in i:
                tline = lines[l_num+1].split(" ")
                TPPWMGENS = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','TPPWMGEN')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'MODULE: STEPGEN' in i:
                tline = lines[l_num+1].split(" ")
                STEPGENS = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','STEPGEN')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'MODULE: LED' in i:
                tline = lines[l_num+1].split(" ")
                LEDS = tline[4].lstrip()
                new = add_element(mod_ele,'module')
                add_text(new,'tagname','LED')
                add_text(new,'numinstances',tline[4].lstrip())
            if 'IO CONNECTIONS FOR' in i:
                if pinsflag:
                    n1 = add_element(ELEMENT,'pins')
                    pinsflag = False
                CON = temp2[3]
                print CON
                for num in range(l_num+3,l_num+3+int(NUMCONPINS)):
                    CHAN = PINFNCTN = ''
                    pin_line = ' '.join(lines[num].split()).split()
                    PINNO = pin_line[0]
                    IO = pin_line[1]
                    SECFNCTN = pin_line[3]
                    n2 = add_element(n1,'pin')
                    add_text(n2,'index',IO)
                    add_text(n2,'connector',CON)
                    add_text(n2,'pinno',PINNO)
                    add_text(n2,'secondarymodulename',SECFNCTN)
                    if not SECFNCTN == 'NONE':
                        CHAN = pin_line[4]
                        PINFNCTN = pin_line[5]
                        if PINFNCTN in("TXDATA1","TXDATA2","TXDATA3",
                            "TXDATA4","TXDATA5","TXDATA6","TXDATA7","TXDATA8"):
                            num = int(PINFNCTN[6])-1
                            print num
                            for idnum,dev in sserial:
                                print idnum,dev,num
                                if int(idnum) == num:
                                    NEW_FNCTN = '%s-%d'% (dev,num)
                                    add_text(n2,'foundsserialdevice',NEW_FNCTN)
                        add_text(n2,'secondaryfunctionname',PINFNCTN)
                        add_text(n2,'secondaryinstance',CHAN)
                    else:
                        add_text(n2,'secondaryfunctionname','NOT USED')
                    
                    print '    I/O ',IO, ' function ',SECFNCTN,' CHANNEL:',CHAN,'PINFUNCTION:',PINFNCTN

        print 'Sserial CARDS FOUND:',sserial
        print NUMCONS,NUMCONPINS,ENCODERS,MUXENCODERS,SSERIALPORTS,NUMSSCHANNELS
        print RESOLVERS,PWMGENS,LEDS
        firmname = "~/mesa%d_discovered.xml"%boardnum
        filename = os.path.expanduser(firmname)
        DOC.writexml(open(filename, "wb"), addindent="  ", newl="\n")
        return DRIVER, BOARDNAME, firmname, filename

    # update all the firmware/boardname arrays and comboboxes
    def discovery_selection_update(self, info, bdnum):
        driver, boardname, firmname, path = self.parse_discovery(info,boardnum=bdnum)
        boardname = 'Discovered:%s'% boardname
        firmdata = self.parse_xml( driver,boardname,firmname,path)
        self._p.MESA_FIRMWAREDATA.append(firmdata)
        self._p.MESA_INTERNAL_FIRMWAREDATA.append(firmdata)
        self._p.MESA_BOARDNAMES.append(boardname)
        # add firmname to combo box if it's not there
        model = self.widgets["mesa%s_firmware"%bdnum].get_model()
        flag = True
        for search,item in enumerate(model):
            if model[search][0]  == firmname:
                flag = False
                break
        if flag:
            model.append((firmname,))
            search = 0
            model = self.widgets["mesa%s_firmware"%bdnum].get_model()
            for search,item in enumerate(model):
                if model[search][0]  == firmname:
                    self.widgets["mesa%s_firmware"%bdnum].set_active(search)
                    break
        # add boardtitle
        model = self.widgets["mesa%s_boardtitle"%bdnum].get_model()
        flag2 = True
        for search,item in enumerate(model):
            if model[search][0]  == boardname:
                flag2 = False
                break
        if flag2:
            model.append((boardname,))
            search = 0
            model = self.widgets["mesa%s_boardtitle"%bdnum].get_model()
            for search,item in enumerate(model):
                #print model[search][0], boardname
                if model[search][0]  == boardname:
                    self.widgets["mesa%s_boardtitle"%bdnum].set_active(search)
                    break
        # update if there was a change
        if flag or flag2:
            self.on_mesa_component_value_changed(None,0)

    def add_device_rule(self):
        text = []
        sourcefile = "/tmp/"
        if os.path.exists("/etc/udev/rules.d/50-LINUXCNC-general.rules"):
            text.append( "General rule already exists\n")
        else:
            text.append("adding a general rule first\nso your device will be found\n")
            filename = os.path.join(sourcefile, "LINUXCNCtempGeneral.rules")
            file = open(filename, "w")
            print >>file, ("# This is a rule for LinuxCNC's hal_input\n")
            print >>file, ("""SUBSYSTEM="input", MODE="0660", GROUP="plugdev" """) 
            file.close()
            p=os.popen("gksudo cp  %sLINUXCNCtempGeneral.rules /etc/udev/rules.d/50-LINUXCNC-general.rules"% sourcefile )
            time.sleep(.1)
            p.flush()
            p.close()
            os.remove('%sLINUXCNCtempGeneral.rules'% sourcefile)
        text.append(("disconect USB device please\n"))
        if not self.warning_dialog("\n".join(text),False):return

        os.popen('less /proc/bus/input/devices >> %sLINUXCNCnojoytemp.txt'% sourcefile)
        text = ["Plug in USB device please"]
        if not self.warning_dialog("\n".join(text),False):return
        time.sleep(1)

        os.popen('less /proc/bus/input/devices >> %sLINUXCNCjoytemp.txt'% sourcefile).read()
        diff = os.popen (" less /proc/bus/input/devices  | diff   %sLINUXCNCnojoytemp.txt %sLINUXCNCjoytemp.txt "%(sourcefile, sourcefile) ).read()
        self.widgets.help_window.set_title(_("USB device Info Search"))

        os.remove('%sLINUXCNCnojoytemp.txt'% sourcefile)
        os.remove('%sLINUXCNCjoytemp.txt'% sourcefile)
        if diff =="":
            text = ["No new USB device found"]
            if not self.warning_dialog("\n".join(text),True):return
        else:
            textbuffer = self.widgets.textoutput.get_buffer()
            try :         
                textbuffer.set_text(diff)
                self.widgets.helpnotebook.set_current_page(2)
                self.widgets.help_window.show_all()
            except:
                text = _("USB device  page is unavailable\n")
                self.warning_dialog(text,True)
            linelist = diff.split("\n")
            for i in linelist:
                if "Name" in i:
                    temp = i.split("\"")
                    name = temp[1]
                    temp = name.split(" ")
                    self.widgets.usbdevicename.set_text(temp[0])
            infolist = diff.split()
            for i in infolist:
                if "Vendor" in i:
                    temp = i.split("=")
                    vendor = temp[1]           
                if "Product" in i:
                    temp = i.split("=")
                    product = temp[1]
        
            text =[ "Vendor = %s\n product = %s\n name = %s\nadding specific rule"%(vendor,product,name)]
            if not self.warning_dialog("\n".join(text),False):return
            tempname = sourcefile+"LINUXCNCtempspecific.rules"
            file = open(tempname, "w")
            print >>file, ("# This is a rule for LINUXCNC's hal_input\n")
            print >>file, ("# For devicename=%s\n"% name)
            print >>file, ("""SYSFS{idProduct}=="%s", SYSFS{idVendor}=="%s", MODE="0660", GROUP="plugdev" """%(product,vendor)) 
            file.close()
            # remove illegal filename characters
            for i in ("(",")"):
                temp = name.replace(i,"")
                name = temp
            newname = "50-LINUXCNC-%s.rules"% name.replace(" ","_")
            os.popen("gksudo cp  %s /etc/udev/rules.d/%s"% (tempname,newname) )
            time.sleep(1)
            os.remove('%sLINUXCNCtempspecific.rules'% sourcefile)
            text = ["Please unplug and plug in your device again"]
            if not self.warning_dialog("\n".join(text),True):return

    def test_joystick(self):
        halrun = subprocess.Popen("halrun -I  ", shell=True,stdin=subprocess.PIPE,stdout=subprocess.PIPE )   
        #print "requested devicename = ",self.widgets.usbdevicename.get_text()
        halrun.stdin.write("loadusr hal_input -W -KRAL +%s\n"% self.widgets.usbdevicename.get_text())
        halrun.stdin.write("loadusr halmeter -g 0 500\n")
        time.sleep(1.5)
        halrun.stdin.write("show pin\n")
        self.warning_dialog("Close me When done.\n",True)
        halrun.stdin.write("exit\n")
        output = halrun.communicate()[0]
        temp2 = output.split(" ")
        temp=[]
        for i in temp2:
            if i =="": continue
            temp.append(i)
        buttonlist=""
        for index,i in enumerate(temp):
            if "bit" in i and "OUT" in temp[index+1]:
                buttonlist = buttonlist + "  Digital:    %s"% ( temp[index+3] )
            if "float" in i and "OUT" in temp[index+1]:
                buttonlist = buttonlist + "                                                            Analog:     %s"% ( temp[index+3] )
        if buttonlist =="": return
        textbuffer = self.widgets.textoutput.get_buffer()
        try :         
            textbuffer.set_text(buttonlist)
            self.widgets.helpnotebook.set_current_page(2)
            self.widgets.help_window.show_all()
        except:
            text = _("Pin names are unavailable\n")
            self.warning_dialog(text,True)

    def search_for_device_rule(self):
        flag = False
        textbuffer = self.widgets.textoutput.get_buffer()
        textbuffer.set_text("Searching for device rules in folder:    /etc/udev/rules.d\n\n")
        for entry in os.listdir("/etc/udev/rules.d"):
            if fnmatch.fnmatch( entry,"50-LINUXCNC-*"):
                temp = open("/etc/udev/rules.d/" + entry, "r").read()
                templist = temp.split("\n")
                for i in templist:
                    if "devicename=" in i:
                        flag = True
                        temp = i.split("=")
                        name = temp[1]
                        try:
                            textbuffer.insert_at_cursor( "File name:    %s\n"% entry)
                            textbuffer.insert_at_cursor( "Device name:    %s\n\n"% name)
                            self.widgets.helpnotebook.set_current_page(2)
                            self.widgets.help_window.show_all()
                        except:
                            self.show_try_errors()
                            text = _("Device names are unavailable\n")
                            self.warning_dialog(text,True)
        if flag == False:
            text = _("No Pncconf made device rules were found\n")
            textbuffer.insert_at_cursor(text)
            self.warning_dialog(text,True)

    def read_touchy_preferences(self):
        # This reads the Touchy preference file directly
        tempdict = {"touchyabscolor":"abs_textcolor","touchyrelcolor":"rel_textcolor",
                    "touchydtgcolor":"dtg_textcolor","touchyerrcolor":"err_textcolor"}
        for key,value in tempdict.iteritems():
            data = prefs.getpref(value, 'default', str)
            if data == "default":
                self.widgets[key].set_active(False)
            else:
                self.widgets[key].set_active(True)
                self.widgets[key+"button"].set_color(gtk.gdk.color_parse(data))
        self.widgets.touchyforcemax.set_active(bool(prefs.getpref('window_force_max')))

    def get_installed_themes(self):
            data1 = self.d.gladevcptheme
            data2 = prefs.getpref('gtk_theme', 'Follow System Theme', str)
            data3 = self.d.gmcpytheme
            model = self.widgets.themestore
            model.clear()
            model.append((_("Follow System Theme"),))
            model2 = self.widgets.glade_themestore
            model2.clear()
            model2.append((_("Follow System Theme"),))
            temp1 = temp2 = temp3 = 0
            names = os.listdir(_PD.THEMEDIR)
            names.sort()
            for search,dirs in enumerate(names):
                model.append((dirs,))
                model2.append((dirs,))
                if dirs  == data1:
                    temp1 = search+1
                if dirs  == data2:
                    temp2 = search+1
                if dirs  == data3:
                    temp3 = search+1
            self.widgets.gladevcptheme.set_active(temp1)
            self.widgets.touchytheme.set_active(temp2)
            self.widgets.gmcpy_theme.set_active(temp3)

    def gladevcp_sanity_check(self):
                if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/gvcp-panel.ui" % self.d.machinename)):
                    if not self.warning_dialog(_("OK to replace existing glade panel ?\
\nIt will be renamed and added to 'backups' folder.\n Clicking 'existing custom program' will avoid this warning, but \
if you change related options later -such as spindle feedback- the HAL connection will not update"),False):
                        return True

    def pyvcp_sanity_check(self):
              if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/pyvcp-panel.xml" % self.d.machinename)):
                 if not self.warning_dialog(_("OK to replace existing custom pyvcp panel?\
\nExisting pyvcp-panel.xml will be renamed and added to 'backups' folder\n\
Clicking 'existing custom program' will aviod this warning. "),False):
                    return True

    # disallow some signal combinations
    def do_exclusive_inputs(self, widget,portnum,pinname):
        # If initializing the Pport pages we don't want the signal calls to register here.
        # if we are working in here we don't want signal calls because of changes made in here
        # GTK supports signal blocking but then you can't assign signal block name references in GLADE -slaps head
        if self._p.prepare_block or self.recursive_block: return
        if 'mesa' in pinname:
            ptype = '%stype'%pinname
            if not self.widgets[ptype].get_active_text() == _PD.pintype_gpio[0]: return
        self.recursive_block = True
        SIG = self._p
        exclusive = {
            SIG.HOME_X: (SIG.MAX_HOME_X, SIG.MIN_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_Y: (SIG.MAX_HOME_Y, SIG.MIN_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_Z: (SIG.MAX_HOME_Z, SIG.MIN_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.HOME_A: (SIG.MAX_HOME_A, SIG.MIN_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.MAX_HOME_X: (SIG.HOME_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_Y: (SIG.HOME_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_Z: (SIG.HOME_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MAX_HOME_A: (SIG.HOME_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.MIN_HOME_X:  (SIG.HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_Y:  (SIG.HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_Z:  (SIG.HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.MIN_HOME_A:  (SIG.HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.BOTH_HOME_X:  (SIG.HOME_X, SIG.MAX_HOME_X, SIG.MIN_HOME_X, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_Y:  (SIG.HOME_Y, SIG.MAX_HOME_Y, SIG.MIN_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_Z:  (SIG.HOME_Z, SIG.MAX_HOME_Z, SIG.MIN_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_HOME_A:  (SIG.HOME_A, SIG.MAX_HOME_A, SIG.MIN_HOME_A, SIG.ALL_LIMIT, SIG.ALL_HOME, SIG.ALL_LIMIT_HOME),

            SIG.MIN_X: (SIG.BOTH_X, SIG.BOTH_HOME_X, SIG.MIN_HOME_X, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_Y: (SIG.BOTH_Y, SIG.BOTH_HOME_Y, SIG.MIN_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_Z: (SIG.BOTH_Z, SIG.BOTH_HOME_Z, SIG.MIN_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MIN_A: (SIG.BOTH_A, SIG.BOTH_HOME_A, SIG.MIN_HOME_A, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),

            SIG.MAX_X: (SIG.BOTH_X, SIG.BOTH_HOME_X, SIG.MIN_HOME_X, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_Y: (SIG.BOTH_Y, SIG.BOTH_HOME_Y, SIG.MIN_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_Z: (SIG.BOTH_Z, SIG.BOTH_HOME_Z, SIG.MIN_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.MAX_A: (SIG.BOTH_A, SIG.BOTH_HOME_A, SIG.MIN_HOME_A, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),

            SIG.BOTH_X: (SIG.MIN_X, SIG.MAX_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_Y: (SIG.MIN_Y, SIG.MAX_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_Z: (SIG.MIN_Z, SIG.MAX_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),
            SIG.BOTH_A: (SIG.MIN_A, SIG.MAX_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A, SIG.ALL_LIMIT, SIG.ALL_LIMIT_HOME),

            SIG.ALL_LIMIT: (
                SIG.MIN_X, SIG.MAX_X, SIG.BOTH_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.MIN_Y, SIG.MAX_Y, SIG.BOTH_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.MIN_Z, SIG.MAX_Z, SIG.BOTH_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.MIN_A, SIG.MAX_A, SIG.BOTH_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.ALL_LIMIT_HOME),
            SIG.ALL_HOME: (
                SIG.HOME_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.HOME_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.HOME_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.HOME_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.ALL_LIMIT_HOME),
            SIG.ALL_LIMIT_HOME: (
                SIG.HOME_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.HOME_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.HOME_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.HOME_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.MIN_X, SIG.MAX_X, SIG.BOTH_X, SIG.MIN_HOME_X, SIG.MAX_HOME_X, SIG.BOTH_HOME_X,
                SIG.MIN_Y, SIG.MAX_Y, SIG.BOTH_Y, SIG.MIN_HOME_Y, SIG.MAX_HOME_Y, SIG.BOTH_HOME_Y,
                SIG.MIN_Z, SIG.MAX_Z, SIG.BOTH_Z, SIG.MIN_HOME_Z, SIG.MAX_HOME_Z, SIG.BOTH_HOME_Z,
                SIG.MIN_A, SIG.MAX_A, SIG.BOTH_A, SIG.MIN_HOME_A, SIG.MAX_HOME_A, SIG.BOTH_HOME_A,
                SIG.ALL_LIMIT, SIG.ALL_HOME),
        }
        model = self.widgets[pinname].get_model()
        piter = self.widgets[pinname].get_active_iter()
        try:
            dummy, index,signame,sig_group = model.get(piter, 0,1,2,3)
        except:
            self.recursive_block = False
            return
        dbg('exclusive: current:%s %d %s %s'%(pinname,index,signame,sig_group),mtype='excl')
        ex = exclusive.get(signame, ())
        if self.d.number_mesa > 0:
            dbg( 'looking for %s in mesa'%signame,mtype='excl')
            # check mesa main board - only if the tab is shown and the ptype is GOIOI
            for boardnum in range(0,int(self.d.number_mesa)):
                for concount,connector in enumerate(self.d["mesa%d_currentfirmwaredata"% (boardnum)][_PD._NUMOFCNCTRS]) :
                    try:
                        if not self.widgets['mesa%dcon%dtable'%(boardnum,connector)].get_visible():continue
                    except:
                        break
                        break
                    for s in range(0,24):
                        p = "mesa%dc%dpin%d"% (boardnum,connector,s)
                        ptype = "mesa%dc%dpin%dtype"% (boardnum,connector,s)
                        #print p,self.widgets[ptype].get_active_text(),_PD.pintype_gpio[0]
                        try:
                            if not self.widgets[ptype].get_active_text() == _PD.pintype_gpio[0]: continue
                            if self.widgets[p] == widget:continue
                        except:
                            break
                            break
                            break
                        model = self.widgets[p].get_model()
                        piter = self.widgets[p].get_active_iter()
                        dummy, index,v1,sig_group = model.get(piter, 0,1,2,3)
                        #print 'check mesa signals',v1
                        if v1 in ex or v1 == signame:
                            dbg( 'found %s, at %s'%(signame,p),mtype='excl')
                            self.widgets[p].set_active(self._p.hal_input_names.index(SIG.UNUSED_INPUT))
                            self.d[p] = SIG.UNUSED_INPUT
            port = 0
            dbg( 'looking for %s in mesa sserial'%signame,mtype='excl')
            for channel in range (0,self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._MAXSSERIALCHANNELS]):
                if channel == _PD._NUM_CHANNELS: break # TODO may not have all channels worth of glade widgets
                if not self.widgets['mesa%dsserial%d_%d'%(boardnum,port,channel)].get_visible():continue
                #print "sserial data transfering"
                for s in range (0,_PD._SSCOMBOLEN):
                    p = 'mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, s)
                    ptype = 'mesa%dsserial%d_%dpin%dtype' % (boardnum, port, channel, s)
                    try:
                        if not self.widgets[ptype].get_active_text() == _PD.pintype_gpio[0]: continue
                        if self.widgets[p] == widget:continue
                    except:
                        break
                        break
                    model = self.widgets[p].get_model()
                    piter = self.widgets[p].get_active_iter()
                    dummy, index,v1,sig_group = model.get(piter, 0,1,2,3)
                    #print 'check mesa signals',v1
                    if v1 in ex or v1 == signame:
                        dbg( 'found %s, at %s'%(signame,p),mtype='excl')
                        self.widgets[p].set_active(self._p.hal_input_names.index(SIG.UNUSED_INPUT))
                        self.d[p] = SIG.UNUSED_INPUT

        if self.d.number_pports >0:
            # search pport1 for the illegal signals and change them to unused.
            dbg( 'looking for %s in pport1'%signame,mtype='excl')
            for pin1 in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                p = 'pp1_Ipin%d' % pin1
                # pport2 may not be loaded yet
                try:
                    if self.widgets[p] == widget:continue
                except:
                    self.recursive_block = False
                    return
                model = self.widgets[p].get_model()
                piter = self.widgets[p].get_active_iter()
                dummy, index,v1,sig_group = model.get(piter, 0,1,2,3)
                #print 'check pport1 signals',v1
                if v1 in ex or v1 == signame:
                    dbg( 'found %s, at %s'%(signame,p),mtype='excl')
                    self.widgets[p].set_active(self._p.hal_input_names.index(SIG.UNUSED_INPUT))
                    self.d[p] = SIG.UNUSED_INPUT
        if self.d.number_pports >1:
            # search pport2 for the illegal signals and change them to unused.
            dbg( 'looking for %s in pport2'%signame,mtype='excl')
            for pin1 in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                p2 = 'pp2_Ipin%d' % pin1
                # pport2 may not be loaded yet
                try:
                    if self.widgets[p2] == widget: continue
                except:
                    self.recursive_block = False
                    return
                model = self.widgets[p].get_model()
                piter = self.widgets[p].get_active_iter()
                dummy, index,v2,sig_group = model.get(piter, 0,1,2,3)
                #print 'check pport2 signals',v1
                if v2 in ex or v2 == signame:
                    dbg( 'found %s, at %s'%(signame,p2),mtype='excl')
                    self.widgets[p2].set_active(self._p.hal_input_names.index(SIG.UNUSED_INPUT))
                    self.d[p2] = SIG.UNUSED_INPUT
        self.recursive_block = False

        # MESA SIGNALS
        # connect signals with pin designation data to mesa signal comboboxes and pintype comboboxes
        # record the signal ID numbers so we can block the signals later in the mesa routines
        # have to do it here manually (instead of autoconnect) because glade doesn't handle added
        # user info (board/connector/pin number designations) and doesn't record the signal ID numbers
        # none of this is done if mesa is not checked off in pncconf
        # TODO we should check to see if signals are already present as each time user goes though this page
        # the signals get added again causing multple calls to the functions.
    def init_mesa_signals(self,boardnum):
        cb = "mesa%d_discovery"% (boardnum)
        i = "_mesa%dsignalhandler_discovery"% (boardnum)
        self.d[i] = int(self.widgets[cb].connect("clicked", self.p['on_mesa%d_discovery_clicked'%boardnum]))
        cb = "mesa%d_comp_update"% (boardnum)
        i = "_mesa%dsignalhandler_comp_update"% (boardnum)
        self.d[i] = int(self.widgets[cb].connect("clicked", self.on_mesa_component_value_changed,boardnum))
        cb = "mesa%d_boardtitle"% (boardnum)
        i = "_mesa%dsignalhandler_boardname_change"% (boardnum)
        self.d[i] = int(self.widgets[cb].connect("changed", self.on_mesa_boardname_changed,boardnum))
        cb = "mesa%d_firmware"% (boardnum)
        i = "_mesa%dsignalhandler_firmware_change"% (boardnum)
        self.d[i] = int(self.widgets[cb].connect("changed", self.on_mesa_firmware_changed,boardnum))
        for connector in (1,2,3,4,5,6,7,8,9):
            for pin in range(0,24):
                cb = "mesa%dc%ipin%i"% (boardnum,connector,pin)
                i = "_mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)
                self.d[i] = int(self.widgets[cb].connect("changed",
                    self.on_general_pin_changed,"mesa",boardnum,connector,None,pin,False))
                i = "_mesa%dactivatehandlerc%ipin%i"% (boardnum,connector,pin)
                self.d[i] = int(self.widgets[cb].child.connect("activate",
                    self.on_general_pin_changed,"mesa",boardnum,connector,None,pin,True))
                self.widgets[cb].connect('changed', self.do_exclusive_inputs,boardnum,cb)
                cb = "mesa%dc%ipin%itype"% (boardnum,connector,pin)
                i = "_mesa%dptypesignalhandlerc%ipin%i"% (boardnum,connector,pin)
                self.d[i] = int(self.widgets[cb].connect("changed", self.on_mesa_pintype_changed,boardnum,connector,None,pin))
        # SmartSerial signals
        port = 0 #TODO we only support one serial port
        for channel in range (0,self._p._NUM_CHANNELS):
            for pin in range (0,self._p._SSCOMBOLEN):
                cb = "mesa%dsserial%i_%ipin%i"% (boardnum,port,channel,pin)
                i = "_mesa%dsignalhandlersserial%i_%ipin%i"% (boardnum,port,channel,pin)
                self.d[i] = int(self.widgets[cb].connect("changed",
                    self.on_general_pin_changed,"sserial",boardnum,port,channel,pin,False))
                i = "_mesa%dactivatehandlersserial%i_%ipin%i"% (boardnum,port,channel,pin)
                self.d[i] = int(self.widgets[cb].child.connect("activate",
                    self.on_general_pin_changed,"sserial",boardnum,port,channel,pin,True))
                self.widgets[cb].connect('changed', self.do_exclusive_inputs,boardnum,cb)
                cb = "mesa%dsserial%i_%ipin%itype"% (boardnum,port,channel,pin)
                i = "_mesa%dptypesignalhandlersserial%i_%ipin%i"% (boardnum,port,channel,pin)
                self.d[i] = int(self.widgets[cb].connect("changed", self.on_mesa_pintype_changed,boardnum,port,channel,pin))
        self.widgets["mesa%d_7i29_sanity_check"%boardnum].connect('clicked', self.daughter_board_sanity_check)
        self.widgets["mesa%d_7i30_sanity_check"%boardnum].connect('clicked', self.daughter_board_sanity_check)
        self.widgets["mesa%d_7i33_sanity_check"%boardnum].connect('clicked', self.daughter_board_sanity_check)
        self.widgets["mesa%d_7i40_sanity_check"%boardnum].connect('clicked', self.daughter_board_sanity_check)
        self.widgets["mesa%d_7i48_sanity_check"%boardnum].connect('clicked', self.daughter_board_sanity_check)

    def init_mesa_options(self,boardnum):
        #print 'init mesa%d options'%boardnum
        i = self.widgets['mesa%d_boardtitle'%boardnum].get_active_text()
        # check for installed firmware
        #print i,self.d['mesa%d_boardtitle'%boardnum]
        if 1==1:#if not self.d['_mesa%d_arrayloaded'%boardnum]:
            #print boardnum,self._p.FIRMDIR,i
            # add any extra firmware data from .pncconf-preference file
            #if not customself._p.MESA_FIRMWAREDATA == []:
            #    for i,j in enumerate(customself._p.MESA_FIRMWAREDATA):
            #        self._p.MESA_FIRMWAREDATA.append(customself._p.MESA_FIRMWAREDATA[i])
            # ok set up mesa info
            dbg('Looking for firmware data %s'%self.d["mesa%d_firmware"% boardnum])
            found = False
            search = 0
            model = self.widgets["mesa%d_firmware"% boardnum].get_model()
            for search,item in enumerate(model):
                dbg('%d,%s'%(search,model[search][0]))
                if model[search][0]  == self.d["mesa%d_firmware"% boardnum]:
                    self.widgets["mesa%d_firmware"% boardnum].set_active(search)
                    found = True
                    dbg('found firmware # %d'% search)
                    break
            if not found:
                dbg('firmware not found')
                cur_firm = self.d['mesa%d_currentfirmwaredata'% boardnum][_PD._FIRMWARE]
                dbg('looking for: %s'% cur_firm )
                #self.widgets["mesa%d_firmware"% boardnum].set_active(0)
                self._p.MESA_FIRMWAREDATA.append(self.d['mesa%d_currentfirmwaredata'% boardnum])
                model.append((cur_firm,))
                self.init_mesa_options(boardnum)
                return
            else:      
                self.widgets["mesa%d_pwm_frequency"% boardnum].set_value(self.d["mesa%d_pwm_frequency"% boardnum])
                self.widgets["mesa%d_pdm_frequency"% boardnum].set_value(self.d["mesa%d_pdm_frequency"% boardnum])
                self.widgets["mesa%d_3pwm_frequency"% boardnum].set_value(self.d["mesa%d_3pwm_frequency"% boardnum])
                self.widgets["mesa%d_watchdog_timeout"% boardnum].set_value(self.d["mesa%d_watchdog_timeout"% boardnum])
                self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(self.d["mesa%d_numof_encodergens"% boardnum])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(self.d["mesa%d_numof_pwmgens"% boardnum])
                self.widgets["mesa%d_numof_tppwmgens"% boardnum].set_value(self.d["mesa%d_numof_tppwmgens"% boardnum])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(self.d["mesa%d_numof_stepgens"% boardnum])
                self.widgets["mesa%d_numof_sserialports"% boardnum].set_value(self.d["mesa%d_numof_sserialports"% boardnum])
                self.widgets["mesa%d_numof_sserialchannels"% boardnum].set_value(self.d["mesa%d_numof_sserialchannels"% boardnum])
        if not self.widgets.createconfig.get_active() and not self.d['_mesa%d_configured'%boardnum]:
            bt = self.d['mesa%d_boardtitle'%boardnum]
            firm = self.d['mesa%d_firmware'%boardnum]
            pgens = self.d['mesa%d_numof_pwmgens'%boardnum]
            tpgens = self.d['mesa%d_numof_tppwmgens'%boardnum]
            stepgens = self.d['mesa%d_numof_stepgens'%boardnum]
            enc = self.d['mesa%d_numof_encodergens'%boardnum]
            ssports = self.d['mesa%d_numof_sserialports'%boardnum]
            sschannels = self.d['mesa%d_numof_sserialchannels'%boardnum]
            self.set_mesa_options(boardnum,bt,firm,pgens,tpgens,stepgens,enc,ssports,sschannels)
        elif not self.d._mesa0_configured:
            self.widgets['mesa%dcon2table'%boardnum].hide()
            self.widgets['mesa%dcon3table'%boardnum].hide()   
            self.widgets['mesa%dcon4table'%boardnum].hide()
            self.widgets['mesa%dcon5table'%boardnum].hide()
 
    def on_mesa_boardname_changed(self, widget,boardnum):
        #print "**** INFO boardname %d changed"% boardnum
        model = self.widgets["mesa%d_boardtitle"% boardnum].get_model()
        title = self.widgets["mesa%d_boardtitle"% boardnum].get_active_text()
        if title:
            if 'Discovery Option' in title:
                self.widgets["mesa%d_discovery"% boardnum].show()
            else:
                self.widgets["mesa%d_discovery"% boardnum].hide()
        for i in(1,2,3,4,5,6,7,8,9):
            self.widgets['mesa%dcon%dtable'%(boardnum,i)].hide()
        for i in(0,1,2,3,4,5):
            self.widgets["mesa%dsserial0_%d"%(boardnum,i)].hide()
        if title == None: return
        #print 'title',title
        self.fill_firmware(boardnum)

    def fill_firmware(self,boardnum):
        #print 'fill firmware'
        self.firmware_block = True
        title = self.widgets["mesa%d_boardtitle"% boardnum].get_active_text()
        #print title
        self._p.MESA_FIRMWAREDATA = []
        if os.path.exists(os.path.join(self._p.FIRMDIR,title)):
            self.mesa_firmware_search(title)
            self.d['_mesa%d_arrayloaded'%boardnum] = True
        for i in self._p.MESA_INTERNAL_FIRMWAREDATA:
            self._p.MESA_FIRMWAREDATA.append(i)
        model = self.widgets["mesa%d_firmware"% boardnum].get_model()
        model.clear()
        temp=[]
        for search, item in enumerate(self._p.MESA_FIRMWAREDATA):
            d = self._p.MESA_FIRMWAREDATA[search]
            if not d[self._p._BOARDTITLE] == title:continue
            temp.append(d[self._p._FIRMWARE])
        temp.sort()
        for i in temp:
            #print i
            model.append((i,))
        self.widgets["mesa%d_firmware"% boardnum].set_active(0)
        self.firmware_block = False
        self.on_mesa_firmware_changed(None,boardnum)
        #print "firmware-",self.widgets["mesa%d_firmware"% boardnum].get_active_text(),self.widgets["mesa%d_firmware"% boardnum].get_active()
        #print "boardname-" + d[_PD._BOARDNAME]

    def on_mesa_firmware_changed(self, widget,boardnum):
        if self.firmware_block:
            return
        print "**** INFO firmware %d changed"% boardnum
        model = self.widgets["mesa%d_boardtitle"% boardnum].get_model()
        active = self.widgets["mesa%d_boardtitle"% boardnum].get_active()
        if active < 0:
          title = None
        else: title = model[active][0]
        firmware = self.widgets["mesa%d_firmware"% boardnum].get_active_text()
        for search, item in enumerate(self._p.MESA_FIRMWAREDATA):
            d = self._p.MESA_FIRMWAREDATA[search]
            #print firmware,d[_PD._FIRMWARE],title,d[_PD._BOARDTITLE]
            if not d[_PD._BOARDTITLE] == title:continue
            if d[_PD._FIRMWARE] == firmware:
                self.widgets["mesa%d_numof_encodergens"%boardnum].set_range(0,d[_PD._MAXENC])
                self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(d[_PD._MAXENC])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_range(0,d[_PD._MAXPWM])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(d[_PD._MAXPWM])
                if d[_PD._MAXTPPWM]:
                    self.widgets["mesa%d_numof_tppwmgens"% boardnum].show()
                    self.widgets["mesa%d_numof_tpp_label"% boardnum].show()
                    self.widgets["mesa%d_3pwm_freq_label"% boardnum].show()
                    self.widgets["mesa%d_3pwm_freq_units"% boardnum].show()
                    self.widgets["mesa%d_3pwm_frequency"% boardnum].show()
                else:
                    self.widgets["mesa%d_numof_tppwmgens"% boardnum].hide()
                    self.widgets["mesa%d_numof_tpp_label"% boardnum].hide()
                    self.widgets["mesa%d_3pwm_freq_label"% boardnum].hide()
                    self.widgets["mesa%d_3pwm_freq_units"% boardnum].hide()
                    self.widgets["mesa%d_3pwm_frequency"% boardnum].hide()
                self.widgets["mesa%d_numof_tppwmgens"% boardnum].set_range(0,d[_PD._MAXTPPWM])
                self.widgets["mesa%d_numof_tppwmgens"% boardnum].set_value(d[_PD._MAXTPPWM])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_range(0,d[_PD._MAXSTEP])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(d[_PD._MAXSTEP])
                self.d["mesa%d_numof_resolvers"% boardnum] = (d[_PD._MAXRES]) # TODO fix this hack should be selectable
                if d[_PD._MAXRES]:
                    self.widgets["mesa%d_numof_resolvers"% boardnum].show()
                    self.widgets["mesa%d_numof_resolvers"% boardnum].set_value(d[_PD._MAXRES]*6)
                    self.widgets["mesa%d_numof_resolvers"% boardnum].set_sensitive(False)
                    self.widgets["mesa%d_numof_resolvers_label"% boardnum].show()
                    self.widgets["mesa%d_pwm_frequency"% boardnum].set_value(24000)
                else:
                    self.widgets["mesa%d_numof_resolvers"% boardnum].hide()
                    self.widgets["mesa%d_numof_resolvers_label"% boardnum].hide()
                    self.widgets["mesa%d_numof_resolvers"% boardnum].set_value(0)
                if d[_PD._MAXSSERIALPORTS]:
                    self.widgets["mesa%d_numof_sserialports"% boardnum].show()
                    self.widgets["mesa%d_numof_sserialports_label"% boardnum].show()
                    self.widgets["mesa%d_numof_sserialchannels"% boardnum].show()
                    self.widgets["mesa%d_numof_sserialchannels_label"% boardnum].show()
                else:
                    self.widgets["mesa%d_numof_sserialports"% boardnum].hide()
                    self.widgets["mesa%d_numof_sserialports_label"% boardnum].hide()
                    self.widgets["mesa%d_numof_sserialchannels"% boardnum].hide()
                    self.widgets["mesa%d_numof_sserialchannels_label"% boardnum].hide()
                self.widgets["mesa%d_numof_sserialports"% boardnum].set_range(0,d[_PD._MAXSSERIALPORTS])
                self.widgets["mesa%d_numof_sserialports"% boardnum].set_value(d[_PD._MAXSSERIALPORTS])
                self.widgets["mesa%d_numof_sserialchannels"% boardnum].set_range(1,d[_PD._MAXSSERIALCHANNELS])
                self.widgets["mesa%d_numof_sserialchannels"% boardnum].set_value(d[_PD._MAXSSERIALCHANNELS])
                self.widgets["mesa%d_totalpins"% boardnum].set_text("%s"% d[_PD._MAXGPIO])
                self.widgets["mesa%d_3pwm_frequency"% boardnum].set_sensitive(d[_PD._MAXTPPWM])
                if d[_PD._MAXRES]:
                    self.widgets["mesa%d_pwm_frequency"% boardnum].set_sensitive(False)
                else:
                    self.widgets["mesa%d_pwm_frequency"% boardnum].set_sensitive(d[_PD._MAXPWM])
                self.widgets["mesa%d_pdm_frequency"% boardnum].set_sensitive(d[_PD._MAXPWM])

                if 'eth' in d[_PD._HALDRIVER] or "7i43" in title or '7i90' in title:
                    self.widgets["mesa%d_card_addrs_hbox"% boardnum].show()
                    if '7i43' in title or '7i90' in title:
                        self.widgets["mesa%d_parportaddrs"% boardnum].show()
                        self.widgets["mesa%d_card_addrs"% boardnum].hide()
                    else:
                        self.widgets["mesa%d_parportaddrs"% boardnum].hide()
                        self.widgets["mesa%d_card_addrs"% boardnum].show()
                    self.widgets["mesa%d_parporttext"% boardnum].show()
                else:
                    self.widgets["mesa%d_card_addrs_hbox"% boardnum].hide()
                    self.widgets["mesa%d_parporttext"% boardnum].hide()
                break
  
    # This method converts data from the GUI page to signal names for pncconf's mesa data variables
    # It starts by checking pin type to set up the proper lists to search
    # then depending on the pin type widget data is converted to signal names.
    # if the signal name is not in the list add it to Human_names, signal_names
    # and disc-saved signalname lists
    # if encoder, pwm, or stepper pins the related pin are also set properly
    # it does this by searching the current firmware array and finding what the
    # other related pins numbers are then changing them to the appropriate signalname.    
    def mesa_data_transfer(self,boardnum):
        for concount,connector in enumerate(self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS]) :
            for pin in range(0,24):
                p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                pinv = 'mesa%dc%dpin%dinv' % (boardnum,connector,pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                self.data_transfer(boardnum,connector,None,pin,p,pinv,ptype)
                self.d["mesa%d_pwm_frequency"% boardnum] = self.widgets["mesa%d_pwm_frequency"% boardnum].get_value()
        self.d["mesa%d_pdm_frequency"% boardnum] = self.widgets["mesa%d_pdm_frequency"% boardnum].get_value()
        self.d["mesa%d_3pwm_frequency"% boardnum] = self.widgets["mesa%d_3pwm_frequency"% boardnum].get_value()
        self.d["mesa%d_watchdog_timeout"% boardnum] = self.widgets["mesa%d_watchdog_timeout"% boardnum].get_value()
        port = 0
        for channel in range (0,self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._MAXSSERIALCHANNELS]):
                if channel == _PD._NUM_CHANNELS: break # TODO may not have all channels worth of glade widgets
                subboardname = self.d["mesa%dsserial%d_%dsubboard"% (boardnum, port, channel)]
                #print "data transfer-channel ",channel," subboard name",subboardname
                if subboardname == "none": 
                    #print "no subboard for %s"% subboardname
                    continue
                #print "sserial data transfering"
                for pin in range (0,_PD._SSCOMBOLEN):
                    p = 'mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, pin)
                    pinv = 'mesa%dsserial%d_%dpin%dinv' % (boardnum, port, channel, pin)
                    ptype = 'mesa%dsserial%d_%dpin%dtype' % (boardnum, port, channel, pin)
                    self.data_transfer(boardnum,port,channel,pin,p,pinv,ptype)
                    #print "sserial data transfer",p

    def data_transfer(self,boardnum,connector,channel,pin,p,pinv,ptype):
                foundit = False
                piter = self.widgets[p].get_active_iter()
                ptiter = self.widgets[ptype].get_active_iter()
                pintype = self.widgets[ptype].get_active_text()
                selection = self.widgets[p].get_active_text()
                signaltree = self.widgets[p].get_model()
                #if "serial" in p:
                #    print "**** INFO mesa-data-transfer:",p," selection: ",selection,"  pintype: ",pintype
                #    print "**** INFO mesa-data-transfer:",ptiter,piter
                # type NOTUSED
                if pintype == _PD.NUSED:
                    self.d[p] = _PD.UNUSED_UNUSED
                    self.d[ptype] = _PD.NUSED
                    self.d[pinv] = False
                    return
                # type GPIO input
                if pintype == _PD.GPIOI:
                    ptypetree = self.d._gpioliststore
                    signaltocheck = _PD.hal_input_names
                # type gpio output and open drain
                elif pintype in (_PD.GPIOO,_PD.GPIOD):
                    ptypetree = self.d._gpioliststore
                    signaltocheck = _PD.hal_output_names
                #type encoder
                elif pintype in (_PD.ENCA,_PD.ENCB,_PD.ENCI,_PD.ENCM):
                    ptypetree = self.d._encoderliststore
                    signaltocheck = _PD.hal_encoder_input_names
                # resolvers
                elif pintype in (_PD.RES0,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5,_PD.RESU):
                    ptypetree = self.d._resolverliststore
                    signaltocheck = _PD.hal_resolver_input_names
                # 8i20 amplifier card
                elif pintype == _PD.AMP8I20:
                    ptypetree = self.d._8i20liststore
                    signaltocheck = _PD.hal_8i20_input_names
                # potentiometer output
                elif pintype in (_PD.POTO,_PD.POTE):
                    ptypetree = self.d._potliststore
                    signaltocheck = _PD.hal_pot_output_names
                # analog in 
                elif pintype == (_PD.ANALOGIN):
                    ptypetree = self.d._analoginliststore
                    signaltocheck = _PD.hal_analog_input_names
                #type mux encoder
                elif pintype in (_PD.MXE0, _PD.MXE1, _PD.MXEI, _PD.MXEM, _PD.MXES):
                    ptypetree = self.d._muxencoderliststore
                    signaltocheck = _PD.hal_encoder_input_names
                # type PWM gen
                elif pintype in( _PD.PDMP,_PD.PDMD,_PD.PDME):
                    if pintype == _PD.PDMP:
                        ptypetree = self.d._pdmcontrolliststore
                    else:
                        ptypetree = self.d._pdmrelatedliststore
                    signaltocheck = _PD.hal_pwm_output_names
                # PDM
                elif pintype in( _PD.PWMP,_PD.PWMD,_PD.PWME):
                    if pintype == _PD.PWMP:
                        ptypetree = self.d._pwmcontrolliststore
                    else:
                        ptypetree = self.d._pwmrelatedliststore
                    signaltocheck = _PD.hal_pwm_output_names
                # Up/Down mode
                elif pintype in( _PD.UDMU,_PD.UDMD,_PD.UDME):
                    if pintype == _PD.UDMU:
                        ptypetree = self.d._udmcontrolliststore
                    else:
                        ptypetree = self.d._udmrelatedliststore
                    signaltocheck = _PD.hal_pwm_output_names
                # type tp pwm
                elif pintype in (_PD.TPPWMA,_PD.TPPWMB,_PD.TPPWMC,_PD.TPPWMAN,_PD.TPPWMBN,_PD.TPPWMCN,_PD.TPPWME,_PD.TPPWMF): 
                    ptypetree = self.d._tppwmliststore 
                    signaltocheck = _PD.hal_tppwm_output_names
                # type step gen
                elif pintype in (_PD.STEPA,_PD.STEPB):
                    ptypetree = self.d._stepperliststore
                    signaltocheck = _PD.hal_stepper_names
                # type sserial
                elif pintype in (_PD.RXDATA0,_PD.TXDATA0,_PD.TXEN0,_PD.RXDATA1,_PD.TXDATA1,_PD.TXEN1,_PD.RXDATA2,
                                    _PD.TXDATA2,_PD.TXEN2,_PD.RXDATA3,_PD.TXDATA3,_PD.TXEN3,
                                    _PD.RXDATA4,_PD.TXDATA4,_PD.TXEN4,_PD.RXDATA5,_PD.TXDATA5,_PD.TXEN5,_PD.RXDATA6,_PD.TXDATA6,
                                    _PD.TXEN6,_PD.RXDATA7,_PD.TXDATA7,_PD.TXEN7,
                                    _PD.SS7I76M0,_PD.SS7I76M2,_PD.SS7I76M3,_PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4):
                    ptypetree = self.d._sserialliststore
                    signaltocheck = _PD.hal_sserial_names
                # this suppresses errors because of unused and uninitialized sserial instances
                elif pintype == None and "sserial" in ptype: return
                else :
                    print "**** ERROR mesa-data-transfer: error unknown pin type:",pintype,"of ",ptype
                    return
                
                # **Start widget to data Convertion**                    
                # for encoder pins
                if piter == None:
                        #print "callin pin changed !!!"
                        name ="mesa"
                        if "sserial" in p: name = "sserial"
                        self.on_general_pin_changed(None,name,boardnum,connector,channel,pin,True)  
                        selection = self.widgets[p].get_active_text()
                        piter = self.widgets[p].get_active_iter()
                        if piter == None:
                            print "****ERROR PNCCONF: no custom name available"
                            return
                        #print "found signame -> ",selection," "
                # ok we have a piter with a signal type now- lets convert it to a signalname
                #if not "serial" in p:
                #    self.debug_iter(piter,p,"signal")
                dummy, index = signaltree.get(piter,0,1)
                #if not "serial" in p:
                #    print "signaltree: ",dummy
                #    self.debug_iter(ptiter,ptype,"ptype")
                widgetptype, index2 = ptypetree.get(ptiter,0,1)
                #if not "serial" in p:
                #    print "ptypetree: ",widgetptype
                if pintype in (_PD.GPIOI,_PD.GPIOO,_PD.GPIOD,_PD.MXE0,_PD.MXE1,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5,_PD.RESU,_PD.SS7I76M0,
                                _PD.SS7I76M2,_PD.SS7I76M3,_PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4) or (index == 0):
                    index2 = 0
                elif pintype in ( _PD.TXDATA0,_PD.RXDATA0,_PD.TXEN0,_PD.TXDATA1,_PD.RXDATA1,_PD.TXEN1,_PD.TXDATA2,_PD.RXDATA2,
                                   _PD.TXEN2,_PD.TXDATA3,_PD.RXDATA3,_PD.TXEN3,_PD.TXDATA4,_PD.RXDATA4,_PD.TXEN4,
                                  _PD.TXDATA5,_PD.RXDATA5,_PD.TXEN5,_PD.TXDATA6,_PD.RXDATA6,_PD.TXEN6,_PD.TXDATA7,_PD.RXDATA7,_PD.TXEN7 ):
                    index2 = 0
                #print index,index2,signaltocheck[index+index2]
                self.d[p] = signaltocheck[index+index2]
                self.d[ptype] = widgetptype
                self.d[pinv] = self.widgets[pinv].get_active()
                #if "serial" in p:
                #    print "*** INFO PNCCONF mesa pin:",p,"signalname:",self.d[p],"pin type:",widgetptype

    def on_mesa_pintype_changed(self, widget,boardnum,connector,channel,pin):
                #print "mesa pintype changed:",boardnum,connector,channel,pin
                if not channel == None:
                    port = connector
                    p = 'mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, pin)
                    ptype = 'mesa%dsserial%d_%dpin%dtype' % (boardnum, port, channel, pin)
                    blocksignal = "_mesa%dsignalhandlersserial%i_%ipin%i" % (boardnum, port, channel, pin)
                    ptypeblocksignal = "_mesa%dptypesignalhandlersserial%i_%ipin%i"% (boardnum, port, channel, pin)
                else:
                    p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                    ptype = 'mesa%dc%dpin%dtype' %  (boardnum,connector,pin)
                    blocksignal = "_mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)
                    ptypeblocksignal  = "_mesa%dptypesignalhandlerc%ipin%i" % (boardnum, connector,pin)
                modelcheck = self.widgets[p].get_model()
                modelptcheck = self.widgets[ptype].get_model()
                new = self.widgets[ptype].get_active_text()
                #print "pintypechanged",p
                # switch GPIO input to GPIO output
                # here we switch the available signal names in the combobox
                # we block signals so pinchanged method is not called
                if modelcheck == self.d._gpioisignaltree and new in (_PD.GPIOO,_PD.GPIOD):
                    #print "switch GPIO input ",p," to output",new
                    self.widgets[p].handler_block(self.d[blocksignal])
                    self.widgets[p].set_model(self.d._gpioosignaltree)
                    self.widgets[p].set_active(0)
                    self.widgets[p].handler_unblock(self.d[blocksignal])
                # switch GPIO output to input
                elif modelcheck == self.d._gpioosignaltree:
                    if new == _PD.GPIOI:
                        #print "switch GPIO output ",p,"to input"
                        self.widgets[p].handler_block(self.d[blocksignal])
                        self.widgets[p].set_model(self.d._gpioisignaltree)
                        self.widgets[p].set_active(0)
                        self.widgets[p].handler_unblock(self.d[blocksignal])
                # switch between pulse width, pulse density or up/down mode analog modes
                # here we search the firmware for related pins (eg PWMP,PWMD,PWME ) and change them too.
                # we block signals so we don't call this routine again.
                elif modelptcheck in (self.d._pwmcontrolliststore, self.d._pdmcontrolliststore, self.d._udmcontrolliststore):
                    relatedpins = [_PD.PWMP,_PD.PWMD,_PD.PWME]
                    if new == _PD.PWMP:
                        display = 0
                        relatedliststore = self.d._pwmrelatedliststore
                        controlliststore = self.d._pwmcontrolliststore
                    elif new == _PD.PDMP:
                        display = 1
                        relatedliststore = self.d._pdmrelatedliststore
                        controlliststore = self.d._pdmcontrolliststore
                    elif new == _PD.UDMU:
                        display = 2
                        relatedliststore = self.d._udmrelatedliststore
                        controlliststore = self.d._udmcontrolliststore
                    else:print "**** WARNING PNCCONF: pintype error-PWM type not found";return
                    self.widgets[ptype].handler_block(self.d[ptypeblocksignal])
                    self.widgets[ptype].set_model(controlliststore)
                    self.widgets[ptype].set_active(display)
                    self.widgets[ptype].handler_unblock(self.d[ptypeblocksignal])
                    pinlist = self.d.list_related_pins(relatedpins, boardnum, connector, channel, pin, 1)
                    for i in (pinlist):
                        relatedptype = i[0]
                        if relatedptype == ptype :continue
                        if not channel == None:
                            ptypeblocksignal = "_mesa%dptypesignalhandlersserial%i_%ipin%i"% (i[1], i[2],i[3],i[4])
                        else:
                            ptypeblocksignal  = "_mesa%dptypesignalhandlerc%ipin%i" % (i[1], i[2],i[4])
                        self.widgets[relatedptype].handler_block(self.d[ptypeblocksignal])
                        j = self.widgets[relatedptype].get_active()
                        self.widgets[relatedptype].set_model(relatedliststore)
                        self.widgets[relatedptype].set_active(j)
                        self.widgets[relatedptype].handler_unblock(self.d[ptypeblocksignal])
                else: print "**** WARNING PNCCONF: pintype error in pintypechanged method new ",new,"    pinnumber ",p

    def on_mesa_component_value_changed(self, widget,boardnum):
        self.in_mesa_prepare = True
        self.d["mesa%d_pwm_frequency"% boardnum] = self.widgets["mesa%d_pwm_frequency"% boardnum].get_value()
        self.d["mesa%d_pdm_frequency"% boardnum] = self.widgets["mesa%d_pdm_frequency"% boardnum].get_value()
        self.d["mesa%d_watchdog_timeout"% boardnum] = self.widgets["mesa%d_watchdog_timeout"% boardnum].get_value()
        numofpwmgens = self.d["mesa%d_numof_pwmgens"% boardnum] = int(self.widgets["mesa%d_numof_pwmgens"% boardnum].get_value())
        numoftppwmgens = self.d["mesa%d_numof_tppwmgens"% boardnum] = int(self.widgets["mesa%d_numof_tppwmgens"% boardnum].get_value())
        numofstepgens = self.d["mesa%d_numof_stepgens"% boardnum] = int(self.widgets["mesa%d_numof_stepgens"% boardnum].get_value())
        numofencoders = self.d["mesa%d_numof_encodergens"% boardnum] = int(self.widgets["mesa%d_numof_encodergens"% boardnum].get_value())
        numofsserialports = self.d["mesa%d_numof_sserialports"% boardnum] = int(self.widgets["mesa%d_numof_sserialports"% boardnum].get_value())
        numofsserialchannels = self.d["mesa%d_numof_sserialchannels"% boardnum] = \
        int(self.widgets["mesa%d_numof_sserialchannels"% boardnum].get_value())
        title = self.d["mesa%d_boardtitle"% boardnum] = self.widgets["mesa%d_boardtitle"% boardnum].get_active_text()
        firmware = self.d["mesa%d_firmware"% boardnum] = self.widgets["mesa%d_firmware"% boardnum].get_active_text()
        self.set_mesa_options(boardnum,title,firmware,numofpwmgens,numoftppwmgens,numofstepgens,numofencoders,numofsserialports,numofsserialchannels)
        return True

    # This method sets up the mesa GUI page and is used when changing component values / firmware or boards from config page.
    # it changes the component comboboxes according to the firmware max and user requested amounts
    # it adds signal names to the signal name combo boxes according to component type and in the
    # case of GPIO options selected on the basic page such as limit/homing types.
    # it will grey out I/O tabs according to the selected board type. 
    # it uses GTK signal blocking to block on_general_pin_change and on_mesa_pintype_changed methods.
    # Since this method is for initialization, there is no need to check for changes and this speeds up
    # the update.  
    # 'self._p.MESA_FIRMWAREDATA' holds all the firmware d.
    # 'self.d.mesaX_currentfirmwaredata' hold the current selected firmware data (X is 0 or 1)

    def set_mesa_options(self,boardnum,title,firmware,numofpwmgens,numoftppwmgens,numofstepgens,numofencoders,numofsserialports,numofsserialchannels):
        _PD.prepare_block = True
        self.p.set_buttons_sensitive(0,0)
        self.pbar.set_text("Setting up Mesa tabs")
        self.pbar.set_fraction(0)
        self.window.show()
        while gtk.events_pending():
            gtk.main_iteration()
        for search, item in enumerate(self._p.MESA_FIRMWAREDATA):
            d = self._p.MESA_FIRMWAREDATA[search]
            if not d[_PD._BOARDTITLE] == title:continue
            if d[_PD._FIRMWARE] == firmware:
                self.d["mesa%d_currentfirmwaredata"% boardnum] = self._p.MESA_FIRMWAREDATA[search]
                break
        dbg('current firmware:\n%r'%self._p.MESA_FIRMWAREDATA[search],mtype='curfirm')

        self.widgets["mesa%dcon2table"% boardnum].hide()
        self.widgets["mesa%dcon3table"% boardnum].hide()
        self.widgets["mesa%dcon4table"% boardnum].hide()
        self.widgets["mesa%dcon5table"% boardnum].hide()
        self.widgets["mesa%dcon6table"% boardnum].hide()
        self.widgets["mesa%dcon7table"% boardnum].hide()
        self.widgets["mesa%dcon8table"% boardnum].hide()
        self.widgets["mesa%dcon9table"% boardnum].hide()
        self.widgets["mesa%dsserial0_0"% boardnum].hide()
        self.widgets["mesa%dsserial0_1"% boardnum].hide()
        self.widgets["mesa%dsserial0_2"% boardnum].hide()
        self.widgets["mesa%dsserial0_3"% boardnum].hide()
        self.widgets["mesa%dsserial0_4"% boardnum].hide()
        currentboard = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME]
        for i in self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS]:
            self.widgets["mesa%dcon%dtable"% (boardnum,i)].show()

#        self.widgets["mesa%d"%boardnum].set_title("Mesa%d Configuration-Board: %s firmware: %s"% (boardnum,self.d["mesa%d_boardtitle"%boardnum],
#            self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._FIRMWARE]))

        temp = "/usr/share/doc/hostmot2-firmware-%s/%s.PIN"% (self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._DIRECTORY],
            self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._FIRMWARE] )
        filename = os.path.expanduser(temp)
        if os.path.exists(filename):
            match =  open(filename).read()
            textbuffer = self.widgets.textoutput.get_buffer()
            try :
                textbuffer.set_text("%s\n\n"% filename)
                textbuffer.insert_at_cursor(match)
            except:
                pass

        for concount,connector in enumerate(self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS]) :
            for pin in range (0,24):
                self.pbar.set_fraction((pin+1)/24.0)
                while gtk.events_pending():
                    gtk.main_iteration()
                firmptype,compnum = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._STARTOFDATA+pin+(concount*24)]       
                p = 'mesa%dc%dpin%d' % (boardnum, connector, pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum, connector , pin)
                #print "**** INFO set-mesa-options DATA:",self.d[p],p,self.d[ptype]
                #print "**** INFO set-mesa-options FIRM:",firmptype
                #print "**** INFO set-mesa-options WIDGET:",self.widgets[p].get_active_text(),self.widgets[ptype].get_active_text()
                complabel = 'mesa%dc%dpin%dnum' % (boardnum, connector , pin)
                pinv = 'mesa%dc%dpin%dinv' % (boardnum, connector , pin)
                blocksignal = "_mesa%dsignalhandlerc%ipin%i" % (boardnum, connector, pin)    
                ptypeblocksignal  = "_mesa%dptypesignalhandlerc%ipin%i" % (boardnum, connector,pin)  
                actblocksignal = "_mesa%dactivatehandlerc%ipin%i"  % (boardnum, connector, pin) 
                # kill all widget signals:
                self.widgets[ptype].handler_block(self.d[ptypeblocksignal])
                self.widgets[p].handler_block(self.d[blocksignal]) 
                self.widgets[p].child.handler_block(self.d[actblocksignal])
                self.firmware_to_widgets(boardnum,firmptype,p,ptype,pinv,complabel,compnum,concount,pin,numofencoders,
                                        numofpwmgens,numoftppwmgens,numofstepgens,None,numofsserialports,numofsserialchannels,False)

        self.d["mesa%d_numof_stepgens"% boardnum] = numofstepgens
        self.d["mesa%d_numof_pwmgens"% boardnum] = numofpwmgens
        self.d["mesa%d_numof_encodergens"% boardnum] = numofencoders
        self.d["mesa%d_numof_sserialports"% boardnum] = numofsserialports
        self.d["mesa%d_numof_sserialchannels"% boardnum] = numofsserialchannels     
        self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(numofstepgens)
        self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(numofencoders)      
        self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(numofpwmgens)
        self.in_mesa_prepare = False   
        self.d["_mesa%d_configured"% boardnum] = True
        # unblock all the widget signals again
        for concount,connector in enumerate(self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS]) :
            for pin in range (0,24):
                p = 'mesa%dc%dpin%d' % (boardnum, connector, pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum, connector , pin)
                blocksignal = "_mesa%dsignalhandlerc%ipin%i" % (boardnum, connector, pin)    
                ptypeblocksignal  = "_mesa%dptypesignalhandlerc%ipin%i" % (boardnum, connector,pin)  
                actblocksignal = "_mesa%dactivatehandlerc%ipin%i"  % (boardnum, connector, pin) 
                self.widgets[ptype].handler_unblock(self.d[ptypeblocksignal])
                self.widgets[p].handler_unblock(self.d[blocksignal]) 
                self.widgets[p].child.handler_unblock(self.d[actblocksignal])
        self.mesa_mainboard_data_to_widgets(boardnum)
        self.window.hide()
        self.p.set_buttons_sensitive(1,1)
        _PD.prepare_block = False

    def set_sserial_options(self,boardnum,port,channel):
        numofsserialports = self.d["mesa%d_numof_sserialports"% boardnum]
        numofsserialchannels = self.d["mesa%d_numof_sserialchannels"% boardnum]
        subboardname = self.d["mesa%dsserial%d_%dsubboard"% (boardnum, port, channel)]
        if subboardname == "none":return
        self.pbar.set_text("Setting up Mesa Smart Serial tabs")
        self.pbar.set_fraction(0)
        self.window.show()
        while gtk.events_pending():
            gtk.main_iteration()
        for subnum,temp in enumerate(self._p.MESA_DAUGHTERDATA):
            #print self._p.MESA_DAUGHTERDATA[subnum][self._p._SUBFIRMNAME],subboardname
            if self._p.MESA_DAUGHTERDATA[subnum][self._p._SUBFIRMNAME] == subboardname: break
        #print "found subboard name:",self._p.MESA_DAUGHTERDATA[subnum][self._p._SUBFIRMNAME],subboardname,subnum,"channel:",channel
        for pin in range (0,self._p._SSCOMBOLEN):
            self.pbar.set_fraction((pin+1)/60.0)
            while gtk.events_pending():
                gtk.main_iteration()      
            p = 'mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, pin)
            ptype = 'mesa%dsserial%d_%dpin%dtype' % (boardnum, port, channel, pin)
            pinv = 'mesa%dsserial%d_%dpin%dinv' % (boardnum, port, channel, pin)
            complabel = 'mesa%dsserial%d_%dpin%dnum' % (boardnum, port, channel, pin)
            blocksignal = "_mesa%dsignalhandlersserial%i_%ipin%i" % (boardnum, port, channel, pin)
            ptypeblocksignal  = "_mesa%dptypesignalhandlersserial%i_%ipin%i" % (boardnum, port, channel, pin)
            actblocksignal = "_mesa%dactivatehandlersserial%i_%ipin%i"  % (boardnum, port, channel, pin)
            firmptype,compnum = self._p.MESA_DAUGHTERDATA[subnum][self._p._SUBSTARTOFDATA+pin]
            #print "sserial set options",p
            # kill all widget signals:
            self.widgets[ptype].handler_block(self.d[ptypeblocksignal])
            self.widgets[p].handler_block(self.d[blocksignal])
            self.widgets[p].child.handler_block(self.d[actblocksignal])
            concount = 0
            numofencoders = 10
            numofpwmgens = 12
            numoftppwmgens = 0
            numofstepgens = 0
            self.firmware_to_widgets(boardnum,firmptype,p,ptype,pinv,complabel,compnum,concount,pin,numofencoders,
                                    numofpwmgens,numoftppwmgens,numofstepgens,subboardname,numofsserialports,numofsserialchannels,True)
        # all this to unblock signals
        for pin in range (0,self._p._SSCOMBOLEN):
            firmptype,compnum = self._p.MESA_DAUGHTERDATA[0][self._p._SUBSTARTOFDATA+pin]       
            p = 'mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, pin)
            ptype = 'mesa%dsserial%d_%dpin%dtype' % (boardnum, port, channel, pin)
            pinv = 'mesa%dsserial%d_%dpin%dinv' % (boardnum, port, channel, pin)
            complabel = 'mesa%dsserial%d_%dpin%dnum' % (boardnum, port, channel, pin)
            blocksignal = "_mesa%dsignalhandlersserial%i_%ipin%i" % (boardnum, port, channel, pin)    
            ptypeblocksignal  = "_mesa%dptypesignalhandlersserial%i_%ipin%i" % (boardnum, port, channel, pin)  
            actblocksignal = "_mesa%dactivatehandlersserial%i_%ipin%i"  % (boardnum, port, channel, pin) 
            # unblock all widget signals:
            self.widgets[ptype].handler_unblock(self.d[ptypeblocksignal])
            self.widgets[p].handler_unblock(self.d[blocksignal]) 
            self.widgets[p].child.handler_unblock(self.d[actblocksignal])
        # now that the widgets are set up as per firmware, change them as per the loaded data and add signals
        for pin in range (0,self._p._SSCOMBOLEN):
            firmptype,compnum = self._p.MESA_DAUGHTERDATA[subnum][self._p._SUBSTARTOFDATA+pin]       
            p = 'mesa%dsserial%d_%dpin%d' % (boardnum, port, channel, pin)
            #print "INFO: data to widget smartserial- ",p, firmptype
            ptype = 'mesa%dsserial%d_%dpin%dtype' % (boardnum, port, channel, pin)
            pinv = 'mesa%dsserial%d_%dpin%dinv' % (boardnum, port, channel, pin)
            self.data_to_widgets(boardnum,firmptype,compnum,p,ptype,pinv)
             #print "sserial data-widget",p
        self.widgets["mesa%d_numof_sserialports"% boardnum].set_value(numofsserialports)
        self.widgets["mesa%d_numof_sserialchannels"% boardnum].set_value(numofsserialchannels)
        self.window.hide()

    def firmware_to_widgets(self,boardnum,firmptype,p,ptype,pinv,complabel,compnum,concount,pin,numofencoders,numofpwmgens,numoftppwmgens,
                            numofstepgens,subboardname,numofsserialports,numofsserialchannels,sserialflag):
                currentboard = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME]
                # *** convert widget[ptype] to component specified in firmwaredata  *** 

                # if the board has less then 24 pins hide the extra comboboxes
                if firmptype == _PD.NUSED:
                    self.widgets[p].hide()
                    self.widgets[ptype].hide()
                    self.widgets[pinv].hide()
                    self.widgets[complabel].hide()
                    self.widgets[ptype].set_model(self.d._notusedliststore)
                    self.widgets[ptype].set_active(0)
                    self.widgets[p].set_model(self.d._notusedsignaltree)
                    self.widgets[p].set_active(0)
                    return
                else:
                    self.widgets[p].show()
                    self.widgets[ptype].show()
                    self.widgets[pinv].show()
                    self.widgets[complabel].show()
                    self.widgets[p].child.set_editable(True)

                # ---SETUP GUI FOR ENCODER FAMILY COMPONENT--- 
                # check that we are not converting more encoders that user requested
                # if we are then we trick this routine into thinking the firware asked for GPIO:
                # we can do that by changing the variable 'firmptype' to ask for GPIO
                if firmptype in ( _PD.ENCA,_PD.ENCB,_PD.ENCI,_PD.ENCM ): 
                    if numofencoders >= (compnum+1):
                        # if the combobox is not already displaying the right component:
                        # then we need to set up the comboboxes for this pin, otherwise skip it
                        if self.widgets[ptype].get_model():
                            widgetptype = self.widgets[ptype].get_active_text()
                        else: widgetptype = None
                        if not widgetptype == firmptype or not self.d["_mesa%d_configured"%boardnum]:  
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0)
                            self.widgets[ptype].set_model(self.d._encoderliststore)
                            # serial encoders are not for AXES - filter AXES selections out
                            if sserialflag:
                                self.widgets[p].set_model(self.d._encodersignalfilter)
                            else:
                                self.widgets[p].set_model(self.d._encodersignaltree)
                            # we only add every 4th human name so the user can only select
                            # the encoder's 'A' signal name. If its the other signals
                            # we can add them all because pncconf controls what the user sees
                            if firmptype == _PD.ENCA:
                                self.widgets[complabel].set_text("%d:"%compnum)
                                self.widgets[p].set_active(0)
                                self.widgets[p].set_sensitive(1)
                                self.widgets[ptype].set_sensitive(0)
                                self.widgets[ptype].set_active(0)
                            # pncconf control what the user sees with these ones:
                            elif firmptype in(_PD.ENCB,_PD.ENCI,_PD.ENCM):
                                self.widgets[complabel].set_text("")
                                self.widgets[p].set_active(0)   
                                self.widgets[p].set_sensitive(0)
                                self.widgets[ptype].set_sensitive(0)
                                for i,j in enumerate((_PD.ENCB,_PD.ENCI,_PD.ENCM)):
                                    if firmptype == j:break 
                                self.widgets[ptype].set_active(i+1)
                    else:   
                        # user requested this encoder component to be GPIO instead
                        # We cheat a little and tell the rest of the method that the firmware says
                        # it should be GPIO and compnum is changed to signify that the GPIO can be changed
                        # from input to output
                        # Right now only mainboard GPIO can be changed
                        # sserial I/O can not
                        firmptype = _PD.GPIOI
                        compnum = 0

                # --- mux encoder ---
                elif firmptype in (_PD.MXE0,_PD.MXE1,_PD.MXEI,_PD.MXEM,_PD.MXES):
                    #print "**** INFO: MUX ENCODER:",firmptype,compnum,numofencoders
                    if numofencoders >= (compnum*2+1) or (firmptype == _PD.MXES and numofencoders >= compnum*2+1) or \
                        (firmptype == _PD.MXEM and numofencoders >= compnum +1):
                        # if the combobox is not already displaying the right component:
                        # then we need to set up the comboboxes for this pin, otherwise skip it
                        self.widgets[pinv].set_sensitive(0)
                        self.widgets[pinv].set_active(0)
                        pmodel = self.widgets[p].set_model(self.d._muxencodersignaltree)
                        ptmodel = self.widgets[ptype].set_model(self.d._muxencoderliststore)
                        self.widgets[ptype].set_active(_PD.pintype_muxencoder.index(firmptype))
                        self.widgets[ptype].set_sensitive(0)
                        self.widgets[p].set_active(0)
                        if firmptype in(_PD.MXE0,_PD.MXE1):
                            temp = 0
                            if firmptype == _PD.MXE1: temp = 1
                            self.widgets[complabel].set_text("%d:"%(compnum *2 + temp))
                            self.widgets[p].set_sensitive(1)
                            self.widgets[ptype].show()
                            self.widgets[p].show()
                        elif firmptype == _PD.MXEM:
                            self.widgets[complabel].set_text("%d:"%compnum)
                            self.widgets[p].set_sensitive(0)
                            self.widgets[ptype].show()
                            self.widgets[p].hide()
                        else:
                            self.widgets[complabel].set_text("")
                            self.widgets[p].set_sensitive(0)
                            self.widgets[ptype].hide()
                            self.widgets[p].hide()
                    else:
                        firmptype = _PD.GPIOI
                        compnum = 0

                # ---SETUP GUI FOR RESOLVER FAMILY COMPONENTS---
                elif firmptype in (_PD.RES0,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5,_PD.RESU):
                    if 0 == 0:
                        self.widgets[pinv].set_sensitive(0)
                        self.widgets[pinv].set_active(0)
                        self.widgets[p].set_model(self.d._resolversignaltree)
                        self.widgets[ptype].set_model(self.d._resolverliststore)
                        self.widgets[ptype].set_sensitive(0)
                        self.widgets[ptype].set_active(0)
                        if firmptype == _PD.RESU:
                            self.widgets[complabel].set_text("")
                            self.widgets[p].hide()
                            self.widgets[p].set_sensitive(0)
                            self.widgets[p].set_active(0) 
                            self.widgets[ptype].set_active(6)
                        else:
                            temp = (_PD.RES0,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5)
                            self.widgets[p].show()
                            for num,i in enumerate(temp):
                                if firmptype == i:break
                            self.widgets[complabel].set_text("%d:"% (compnum*6+num))
                            self.widgets[p].set_sensitive(1)
                            self.widgets[p].set_active(0)
                            self.widgets[ptype].set_active(num)

                # ---SETUP 8i20 amp---
                elif firmptype == _PD.AMP8I20:
                        self.widgets[ptype].set_model(self.d._8i20liststore)
                        self.widgets[p].set_model(self.d._8i20signaltree)
                        self.widgets[complabel].set_text("%d:"%compnum)
                        self.widgets[p].set_active(0)
                        self.widgets[p].set_sensitive(1)
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[pinv].set_active(0)
                        self.widgets[ptype].set_sensitive(0)
                        self.widgets[ptype].set_active(0)

                # --- SETUP potentiometer output
                elif firmptype in (_PD.POTO,_PD.POTE):
                        self.widgets[ptype].set_model(self.d._potliststore)
                        self.widgets[p].set_model(self.d._potsignaltree)
                        self.widgets[complabel].set_text("%d:"%compnum)
                        self.widgets[p].set_active(0)
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[pinv].set_active(0)
                        self.widgets[ptype].set_sensitive(0)
                        if firmptype == _PD.POTO:
                            self.widgets[ptype].set_active(0)
                            self.widgets[p].set_sensitive(1)
                        else:
                            self.widgets[ptype].set_active(1)
                            self.widgets[p].set_sensitive(0)

                # --- SETUP analog input
                elif firmptype == (_PD.ANALOGIN):
                        self.widgets[ptype].set_model(self.d._analoginliststore)
                        self.widgets[p].set_model(self.d._analoginsignaltree)
                        self.widgets[complabel].set_text("%d:"%compnum)
                        self.widgets[p].set_active(0)
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[pinv].set_active(0)
                        self.widgets[ptype].set_sensitive(0)
                        self.widgets[ptype].set_active(0)
                        self.widgets[p].set_sensitive(1)

                # ---SETUP GUI FOR PWM FAMILY COMPONENT---
                # the user has a choice of pulse width or pulse density modulation
                elif firmptype in ( _PD.PWMP,_PD.PWMD,_PD.PWME,_PD.PDMP,_PD.PDMD,_PD.PDME ):
                    if numofpwmgens >= (compnum+1):
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[pinv].set_active(0)
                        self.widgets[p].set_model(self.d._pwmsignaltree)         
                        # only add the -pulse signal names for the user to see
                        if firmptype in(_PD.PWMP,_PD.PDMP):
                            self.widgets[complabel].set_text("%d:"%compnum)
                            #print "firmptype = controlling"
                            self.widgets[ptype].set_model(self.d._pwmcontrolliststore)
                            self.widgets[ptype].set_sensitive(not sserialflag) # sserial pwm cannot be changed
                            self.widgets[p].set_sensitive(1)
                            self.widgets[p].set_active(0)
                            self.widgets[ptype].set_active(0)
                        # add them all here      
                        elif firmptype in (_PD.PWMD,_PD.PWME,_PD.PDMD,_PD.PDME):
                            self.widgets[complabel].set_text("")
                            #print "firmptype = related"
                            if firmptype in (_PD.PWMD,_PD.PWME):
                                self.widgets[ptype].set_model(self.d._pwmrelatedliststore)
                            else:
                                self.widgets[ptype].set_model(self.d._pdmrelatedliststore)
                            self.widgets[p].set_sensitive(0)
                            self.widgets[p].set_active(0) 
                            self.widgets[ptype].set_sensitive(0)
                            temp = 1
                            if firmptype in (_PD.PWME,_PD.PDME):
                                self.widgets[pinv].set_sensitive(0)
                                temp = 2
                            self.widgets[ptype].set_active(temp)
                    else:
                        firmptype = _PD.GPIOI
                        compnum = 0
                # ---SETUP GUI FOR TP PWM FAMILY COMPONENT---   
                elif firmptype in ( _PD.TPPWMA,_PD.TPPWMB,_PD.TPPWMC,_PD.TPPWMAN,_PD.TPPWMBN,_PD.TPPWMCN,_PD.TPPWME,_PD.TPPWMF ):
                    if numoftppwmgens >= (compnum+1):
                        if not self.widgets[ptype].get_active_text() == firmptype or not self.d["_mesa%d_configured"%boardnum]:
                            self.widgets[p].set_model(self.d._tppwmsignaltree)
                            self.widgets[ptype].set_model(self.d._tppwmliststore)
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0)
                            self.widgets[ptype].set_sensitive(0)
                            self.widgets[ptype].set_active(_PD.pintype_tp_pwm.index(firmptype))
                            self.widgets[p].set_active(0)
                            # only add the -a signal names for the user to change
                            if firmptype == _PD.TPPWMA:
                                self.widgets[complabel].set_text("%d:"%compnum)
                                self.widgets[p].set_sensitive(1)
                            # the rest the user can't change      
                            else:
                                self.widgets[complabel].set_text("")
                                self.widgets[p].set_sensitive(0)
                    else:
                        firmptype = _PD.GPIOI
                        compnum = 0
                # ---SETUP SMART SERIAL COMPONENTS---
                # smart serial has port numbers (0-3) and channels (0-7).
                # so the component number check is different from other components it checks the port number and channel number
                elif firmptype in (_PD.TXDATA0,_PD.RXDATA0,_PD.TXEN0,_PD.TXDATA1,_PD.RXDATA1,_PD.TXEN1,
                                    _PD.TXDATA2,_PD.RXDATA2,_PD.TXEN2,_PD.TXDATA3,_PD.RXDATA3,_PD.TXEN3,
                                    _PD.TXDATA4,_PD.RXDATA4,_PD.TXEN4,_PD.TXDATA5,_PD.RXDATA5,_PD.TXEN5,
                                    _PD.TXDATA6,_PD.RXDATA6,_PD.TXEN6,_PD.TXDATA7,_PD.RXDATA7,_PD.TXEN7,
                                    _PD.SS7I76M0,_PD.SS7I76M2,_PD.SS7I76M3,_PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4):
                    channelnum = 1
                    if firmptype in (_PD.TXDATA1,_PD.RXDATA1,_PD.TXEN1,_PD.SS7I77M1): channelnum = 2
                    if firmptype in (_PD.TXDATA2,_PD.RXDATA2,_PD.TXEN2,_PD.SS7I76M2): channelnum = 3
                    if firmptype in (_PD.TXDATA3,_PD.RXDATA3,_PD.TXEN3,_PD.SS7I76M3,_PD.SS7I77M3): channelnum = 4
                    if firmptype in (_PD.TXDATA4,_PD.RXDATA4,_PD.TXEN4,_PD.SS7I77M4): channelnum = 5
                    if firmptype in (_PD.TXDATA5,_PD.RXDATA5,_PD.TXEN5): channelnum = 6
                    if firmptype in (_PD.TXDATA6,_PD.RXDATA6,_PD.TXEN6): channelnum = 7
                    if firmptype in (_PD.TXDATA7,_PD.RXDATA7,_PD.TXEN7): channelnum = 8
                    # control combobox is the one the user can select from others are unsensitized
                    CONTROL = False
                    if firmptype in (_PD.TXDATA0,_PD.TXDATA1,_PD.TXDATA2,_PD.TXDATA3,_PD.TXDATA4,_PD.TXDATA5,
                                        _PD.TXDATA6,_PD.TXDATA7,_PD.SS7I76M0,_PD.SS7I76M2,_PD.SS7I76M3,
                                        _PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4):
                        CONTROL = True
                    #print "**** INFO: SMART SERIAL ENCODER:",firmptype," compnum = ",compnum," channel = ",channelnum
                    #print "sserial channel:%d"% numofsserialchannels
                    if numofsserialports >= (compnum + 1) and numofsserialchannels >= (channelnum):
                        # if the combobox is not already displaying the right component:
                        # then we need to set up the comboboxes for this pin, otherwise skip it
                        #if compnum < _PD._NUM_CHANNELS: # TODO not all channels available 
                        #    self.widgets["mesa%dsserialtab%d"% (boardnum,compnum)].show()
                        
                        self.widgets[pinv].set_sensitive(0)
                        self.widgets[pinv].set_active(0)
                        # Filter the selection that the user can choose.
                        # eg only show two modes for 7i77 and 7i76 or
                        # don't give those selections on regular sserial channels
                        if CONTROL:
                            self.widgets[p].set_model(self.d['_sserial%d_signalfilter'%(channelnum-1)])
                            if firmptype in (_PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4):
                                self.set_filter('_sserial%d'% (channelnum-1),'7I77')
                            elif firmptype in (_PD.SS7I76M0,_PD.SS7I76M2,_PD.SS7I76M3):
                                self.set_filter('_sserial%d'% (channelnum-1),'7I76')
                            else:
                                self.set_filter('_sserial%d'% (channelnum-1),'ALL')
                        else:
                            self.widgets[p].set_model(self.d._sserialsignaltree)
                        self.widgets[ptype].set_model(self.d._sserialliststore)
                        self.widgets[ptype].set_active(_PD.pintype_sserial.index(firmptype))
                        self.widgets[ptype].set_sensitive(0)
                        self.widgets[p].set_active(0)
                        self.widgets[p].child.set_editable(False) # sserial cannot have custom names
                        # controlling combbox
                        if CONTROL:
                            self.widgets[complabel].set_text("%d:"% (channelnum -1))
                            if channelnum <= _PD._NUM_CHANNELS:#TODO not all channels available
                                self.widgets[p].set_sensitive(1)
                            else:
                                self.widgets[p].set_sensitive(0)
                            # This is a bit of a hack to make 7i77 and 7i76 firmware automatically choose
                            # the apropriate sserial component and allow the user to select different modes
                            # if the sserial ptype is 7i76 or 7i77 then the data must be set to 7i76/7i77 signal
                            # as that sserial instance can only be for the 7i76/7i77 I/O points
                            # 7i76:
                            if firmptype in (_PD.SS7I76M0,_PD.SS7I76M2,_PD.SS7I76M3):
                                if not self.d[p] in (_PD.I7I76_M0_T,_PD.I7I76_M2_T):
                                    self.d[p] = _PD.I7I76_M0_T
                                self.d[ptype] = firmptype
                                self.widgets[p].set_sensitive(self.d.advanced_option)
                            # 7i77:
                            elif firmptype in (_PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4):
                                if not self.d[p] in (_PD.I7I77_M3_T,_PD.I7I77_M0_T):
                                    self.d[p] = _PD.I7I77_M0_T
                                if not firmptype in( _PD.SS7I77M1,_PD.SS7I77M4):
                                    self.widgets[p].set_sensitive(self.d.advanced_option)
                                else:
                                    self.widgets[p].set_sensitive(0)
                                self.d[ptype] = firmptype
                            else:
                                print 'found a sserial channel'
                                ssdevice = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._SSDEVICES]
                                for port,channel,device in (ssdevice):
                                    print port,channel,device,channelnum
                                    if port == 0 and channel+1 == channelnum:
                                        print 'configure for: %s device'% device
                                        if '7I64' in device:
                                            if not '7i64' in self.d[p]:
                                                self.d[p] = _PD.I7I64_T
                                        elif '7I73' in device:
                                            if not '7i73' in self.d[p]:
                                                self.d[p] = _PD.I7I73_M0_T
                        else:
                            self.widgets[complabel].set_text("")
                            self.widgets[p].set_sensitive(0)
                    else:
                        firmptype = _PD.GPIOI
                        compnum = 0
                # ---SETUP FOR STEPPER FAMILY COMPONENT---
                elif firmptype in (_PD.STEPA,_PD.STEPB):
                    if numofstepgens >= (compnum+1):
                        self.widgets[ptype].set_model(self.d._stepperliststore)
                        self.widgets[p].set_model(self.d._steppersignaltree)
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[pinv].set_active(0)
                        self.widgets[ptype].set_sensitive(0)
                        self.widgets[ptype].set_active( _PD.pintype_stepper.index(firmptype) )
                        self.widgets[p].set_active(0)
                        #self.widgets[p].set_active(0)
                        if firmptype == _PD.STEPA:
                            self.widgets[complabel].set_text("%d:"%compnum)
                            self.widgets[p].set_sensitive(1)
                        elif firmptype == _PD.STEPB:
                            self.widgets[complabel].set_text("")
                            self.widgets[p].set_sensitive(0)
                    else:
                        firmptype = _PD.GPIOI
                        compnum = 0
                # ---SETUP FOR GPIO FAMILY COMPONENT---
                # first check to see if firmware says it should be in GPIO family
                # (note this can be because firmware says it should be some other 
                # type but the user wants to deselect it so as to use it as GPIO
                # this is done in the firmptype checks before this check. 
                # They will change firmptype variable to GPIOI)       
                # check if firmptype is in GPIO family
                # check if widget is already configured
                # we now set everything in a known state.
                if firmptype in (_PD.GPIOI,_PD.GPIOO,_PD.GPIOD):
                    if self.widgets[ptype].get_model():
                        widgettext = self.widgets[ptype].get_active_text()
                    else:
                        widgettext = None
                    if sserialflag:
                        if "7i77" in subboardname or "7i76" in subboardname or "7i84" in subboardname:
                            if pin <16:
                                self.widgets[complabel].set_text("%02d:"%(pin)) # sserial input
                            elif (pin >23 and pin < 40):
                                self.widgets[complabel].set_text("%02d:"%(pin-8)) # sserial input
                            elif pin >15 and pin < 24:
                                self.widgets[complabel].set_text("%02d:"%(pin-16)) #sserial output
                            elif pin >39:
                                self.widgets[complabel].set_text("%02d:"%(pin-32)) #sserial output
                        elif "7i70" in subboardname or "7i71" in subboardname:
                            self.widgets[complabel].set_text("%02d:"%(pin))
                        else:
                            if pin <24 :
                                self.widgets[complabel].set_text("%02d:"%(concount*24+pin)) # sserial input
                            else:
                                self.widgets[complabel].set_text("%02d:"%(concount*24+pin-24)) #sserial output
                    elif '5i25' in currentboard or '7i76e' in currentboard or '7i92' in currentboard:
                         self.widgets[complabel].set_text("%03d:"%(concount*17+pin))# 5i25 mainboard GPIO
                    else:
                         self.widgets[complabel].set_text("%03d:"%(concount*24+pin))# mainboard GPIO
                    if compnum == 100 and widgettext == firmptype:
                        return
                    elif not compnum == 100 and (widgettext in (_PD.GPIOI,_PD.GPIOO,_PD.GPIOD)):
                        return
                    else:
                        #self.widgets[ptype].show()
                        #self.widgets[p].show()
                        self.widgets[p].set_sensitive(1)
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[ptype].set_sensitive(not compnum == 100) # compnum = 100 means GPIO cannot be changed by user
                        self.widgets[ptype].set_model(self.d._gpioliststore)
                        if firmptype == _PD.GPIOI:
                            # set pin treestore to gpioi signals
                            if not self.widgets[p].get_model() == self.d._gpioisignaltree:
                                self.widgets[p].set_model(self.d._gpioisignaltree)
                                # set ptype gpioi
                                self.widgets[ptype].set_active(0)
                                # set p unused signal
                                self.widgets[p].set_active(0)
                                # set pinv unset
                                self.widgets[pinv].set_active(False)
                        else:
                            if not self.widgets[p].get_model() == self.d._gpioosignaltree:
                                self.widgets[p].set_model(self.d._gpioosignaltree)
                                # set ptype gpioo
                                self.widgets[ptype].set_active(1)
                                # set p unused signal
                                self.widgets[p].set_active(0)
                                # set pinv unset
                                self.widgets[pinv].set_active(False)



    def find_sig_name_iter(self,model, signal_name):
        for i, k in enumerate(model):
            itr = model.get_iter(i)
            title = model.get_value(itr,2)
            #print 'first:',title
            # check first set
            if title == signal_name :return itr
            cld_itr = model.iter_children(itr)
            if cld_itr != None:
                while cld_itr != None:
                    gcld_itr = model.iter_children(cld_itr)
                    if gcld_itr != None:
                        while gcld_itr != None:
                            title = model.get_value(gcld_itr,2)
                            #print title
                            # check third set
                            if title == signal_name :return gcld_itr
                            gcld_itr = model.iter_next(gcld_itr)  
                    title = model.get_value(cld_itr,2)
                    #print title
                    # check second set
                    if title == signal_name :return cld_itr
                    cld_itr = model.iter_next(cld_itr)
        # return first entry if no signal name is found
        return model.get_iter_first()



    def mesa_mainboard_data_to_widgets(self,boardnum):
        for concount,connector in enumerate(self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS]) :
            for pin in range (0,24):
                firmptype,compnum = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._STARTOFDATA+pin+(concount*24)]       
                p = 'mesa%dc%dpin%d' % (boardnum, connector, pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum, connector , pin)
                pinv = 'mesa%dc%dpin%dinv' % (boardnum, connector , pin)
                self.data_to_widgets(boardnum,firmptype,compnum,p,ptype,pinv)

    # by now the widgets should be right according to the firmware (and user deselected components)
    # now we apply the data - setting signalnames and possible changing the pintype choice (eg pwm to pdm)
    # We need to only set the 'controlling' signalname the pinchanged method will be called
    # immediately and set the 'related' pins (if there are related pins)
    def data_to_widgets(self,boardnum,firmptype,compnum,p,ptype,pinv):
                debug = False
                datap = self.d[p]
                dataptype = self.d[ptype]
                datapinv = self.d[pinv]
                widgetp = self.widgets[p].get_active_text()
                widgetptype = self.widgets[ptype].get_active_text()
                #print "**** INFO set-data-options DATA:",p,datap,dataptype
                #print "**** INFO set-data-options WIDGET:",p,widgetp,widgetptype
                # ignore related pins
                if widgetptype in (_PD.ENCB,_PD.ENCI,_PD.ENCM,
                                    _PD.MXEI,_PD.MXEM,_PD.MXES,
                                    _PD.RESU,
                                    _PD.STEPB,_PD.STEPC,_PD.STEPD,_PD.STEPE,_PD.STEPF,
                                    _PD.PDMD,_PD.PDME,_PD.PWMD,_PD.PWME,_PD.UDMD,_PD.UDME,
                                    _PD.TPPWMB,_PD.TPPWMC,_PD.TPPWMAN,_PD.TPPWMBN,_PD.TPPWMCN,_PD.TPPWME,_PD.TPPWMF,
                                    _PD.NUSED,_PD.POTD,_PD.POTE,
                                    _PD.RXDATA0,_PD.TXEN0,_PD.RXDATA1,_PD.TXEN1,_PD.RXDATA2,_PD.TXEN2,_PD.RXDATA3,_PD.TXEN3,
                                    _PD.RXDATA4,_PD.TXEN4,_PD.RXDATA5,_PD.TXEN5,_PD.RXDATA6,_PD.TXEN6,_PD.RXDATA7,_PD.TXEN7
                                    ):
                    self.widgets[pinv].set_active(datapinv)
                    return




                # TODO fix this for cmboboxes withgrandchildren
                # we are searching through human names - why not just search the model?

                # type GPIO
                # if compnum  = 100  then it means that the GPIO type can not
                # be changed from what the firmware designates it as.
                if widgetptype in (_PD.GPIOI,_PD.GPIOO,_PD.GPIOD):
                        #print "data ptype index:",_PD.pintype_gpio.index(dataptype)
                        #self.debug_iter(0,p,"data to widget")
                        #self.debug_iter(0,ptype,"data to widget")
                        # signal names for GPIO INPUT
                        #print "compnum = ",compnum
                        if compnum == 100: dataptype = widgetptype 
                        self.widgets[pinv].set_active(self.d[pinv])
                        try:
                            self.widgets[ptype].set_active( _PD.pintype_gpio.index(dataptype) )
                        except:
                            self.widgets[ptype].set_active( _PD.pintype_gpio.index(widgetptype) )
                        # if GPIOI or dataptype not in GPIO family force it GPIOI
                        if dataptype == _PD.GPIOI or dataptype not in(_PD.GPIOO,_PD.GPIOI,_PD.GPIOD):
                            human = _PD.human_input_names
                            signal = _PD.hal_input_names
                            tree = self.d._gpioisignaltree
                        # signal names for GPIO OUTPUT and OPEN DRAIN OUTPUT
                        elif dataptype in (_PD.GPIOO,_PD.GPIOD):
                            human = _PD.human_output_names
                            signal = _PD.hal_output_names
                            tree = self.d._gpioosignaltree
                        self.widgets[p].set_model(tree)
                        itr = self.find_sig_name_iter(tree, datap)
                        self.widgets[p].set_active_iter(itr)

                # type encoder / mux encoder
                # we find the data's signal index
                # then we search through the combobox's actual model's 4th array index
                # this contains the comboxbox's signal's index number
                # when they match then that is the row to show in the combobox
                # this is different because the sserial combobox's model
                # can be filtered and that screws with the relationship of
                # signalname array vrs model row
                elif widgetptype == _PD.ENCA or widgetptype in(_PD.MXE0,_PD.MXE1):
                    #print "ENC ->dataptype:",self.d[ptype]," dataptype:",self.d[p],signalindex
                    pinmodel = self.widgets[p].get_model()
                    itr = self.find_sig_name_iter(pinmodel, datap)
                    self.widgets[p].set_active_iter(itr)

                # type resolver
                elif widgetptype in(_PD.RES0,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5,_PD.RESU):
                    try:
                        signalindex = _PD.hal_resolver_input_names.index(datap)
                    except:
                        if debug: print "**** INFO: PNCCONF warning no resolver signal named: %s\n     found for pin %s"% (datap ,p)
                        signalindex = 0
                    #print "dataptype:",self.d[ptype]," dataptype:",self.d[p],signalindex
                    count = 0
                    temp = (0) # set unused resolver
                    if signalindex > 0:
                        for row,parent in enumerate(_PD.human_resolver_input_names):
                            if row == 0: continue
                            if len(parent[1]) == 0:
                                    count +=1
                                    #print row,count,"parent-",parent[0]
                                    if count == signalindex:
                                        #print "match",row
                                        temp = (row)
                                        break
                                    continue
                            for column,child in enumerate(parent[1]):
                                count +=1
                                #print row,column,count,parent[0],child
                                if count == signalindex:
                                    #print "match",row
                                    temp = (row,column)
                                    break
                            if count >= signalindex:break
                    #print "temp",temp
                    treeiter = self.d._resolversignaltree.get_iter(temp)
                    self.widgets[p].set_active_iter(treeiter)

                # Type 8i20 AMP
                elif widgetptype == _PD.AMP8I20:
                    try:
                        signalindex = _PD.hal_8i20_input_names.index(datap)
                    except:
                        if debug: print "**** INFO: PNCCONF warning no 8i20 signal named: %s\n     found for pin %s"% (datap ,p)
                        signalindex = 0
                    #print "dataptype:",self.d[ptype]," dataptype:",self.d[p],signalindex
                    count = 0
                    temp = (0) # set unused 8i20 amp
                    if signalindex > 0:
                        for row,parent in enumerate(_PD.human_8i20_input_names):
                            if row == 0: continue
                            if len(parent[1]) == 0:
                                    count +=1
                                    #print row,count,"parent-",parent[0]
                                    if count == signalindex:
                                        #print "match",row
                                        temp = (row)
                                        break
                                    continue
                            for column,child in enumerate(parent[1]):
                                count +=1
                                #print row,column,count,parent[0],child
                                if count == signalindex:
                                    #print "match",row
                                    temp = (row,column)
                                    break
                            if count >= signalindex:break
                    #print "temp",temp
                    treeiter = self.d._8i20signaltree.get_iter(temp)
                    self.widgets[p].set_active_iter(treeiter)

                # Type potentiometer (7i76"s spindle control)
                elif widgetptype in (_PD.POTO,_PD.POTE):
                    self.widgets[pinv].set_active(self.d[pinv])
                    try:
                        signalindex = _PD.hal_pot_output_names.index(datap)
                    except:
                        if debug: print "**** INFO: PNCCONF warning no potentiometer signal named: %s\n     found for pin %s"% (datap ,p)
                        signalindex = 0
                    #print "dataptype:",self.d[ptype]," dataptype:",self.d[p],signalindex
                    count = -1
                    temp = (0) # set unused potentiometer
                    if signalindex > 0:
                        for row,parent in enumerate(_PD.human_pot_output_names):
                            if row == 0: continue
                            if len(parent[1]) == 0:
                                    count +=2
                                    #print row,count,"parent-",parent[0]
                                    if count == signalindex:
                                        #print "match",row
                                        temp = (row)
                                        break
                                    continue
                            for column,child in enumerate(parent[1]):
                                count +=2
                                #print row,column,count,parent[0],child
                                if count == signalindex:
                                    #print "match",row
                                    temp = (row,column)
                                    break
                            if count >= signalindex:break
                    #print "temp",temp
                    treeiter = self.d._potsignaltree.get_iter(temp)
                    self.widgets[p].set_active_iter(treeiter)

                # Type analog in
                elif widgetptype == _PD.ANALOGIN:
                    try:
                        signalindex = _PD.hal_analog_input_names.index(datap)
                    except:
                        if debug: print "**** INFO: PNCCONF warning no analog in signal named: %s\n     found for pin %s"% (datap ,p)
                        signalindex = 0
                    #print "dataptype:",self.d[ptype]," dataptype:",self.d[p],signalindex
                    count = 0
                    temp = (0) # set unused 8i20 amp
                    if signalindex > 0:
                        for row,parent in enumerate(_PD.human_analog_input_names):
                            if row == 0: continue
                            if len(parent[1]) == 0:
                                    count +=1
                                    #print row,count,"parent-",parent[0]
                                    if count == signalindex:
                                        #print "match",row
                                        temp = (row)
                                        break
                                    continue
                            for column,child in enumerate(parent[1]):
                                count +=1
                                #print row,column,count,parent[0],child
                                if count == signalindex:
                                    #print "match",row
                                    temp = (row,column)
                                    break
                            if count >= signalindex:break
                    #print "temp",temp
                    treeiter = self.d._analoginsignaltree.get_iter(temp)
                    self.widgets[p].set_active_iter(treeiter)

                # type PWM gen
                elif widgetptype in (_PD.PDMP,_PD.PWMP,_PD.UDMU):
                    self.widgets[pinv].set_active(datapinv)
                    if self.widgets["mesa%d_numof_resolvers"% boardnum].get_value(): dataptype = _PD.UDMU # hack resolver board needs UDMU
                    if dataptype == _PD.PDMP:
                        #print "pdm"
                        self.widgets[ptype].set_model(self.d._pdmcontrolliststore)
                        self.widgets[ptype].set_active(1)
                    elif dataptype == _PD.PWMP:
                        #print "pwm",self.d._pwmcontrolliststore
                        self.widgets[ptype].set_model(self.d._pwmcontrolliststore)
                        self.widgets[ptype].set_active(0)
                    elif dataptype == _PD.UDMU:
                        #print "udm",self.d._udmcontrolliststore
                        self.widgets[ptype].set_model(self.d._udmcontrolliststore)
                        self.widgets[ptype].set_active(2)
                    itr = self.find_sig_name_iter(self.d._pwmsignaltree, datap)
                    self.widgets[p].set_active_iter(itr)
 
                # type tp 3 pwm for direct brushless motor control 
                elif widgetptype == _PD.TPPWMA:
                    #print "3 pwm"
                    count = -7
                    try:
                        signalindex = _PD.hal_tppwm_output_names.index(datap)
                    except:
                        if debug: print "**** INFO: PNCCONF warning no THREE PWM signal named: %s\n     found for pin %s"% (datap ,p)
                        signalindex = 0
                    #print "3 PWw ,dataptype:",self.d[ptype]," dataptype:",self.d[p],signalindex
                    temp = (0) # set unused stepper
                    if signalindex > 0:
                       for row,parent in enumerate(_PD.human_tppwm_output_names):
                          if row == 0:continue
                          if len(parent[1]) == 0:
                             count += 8
                             #print row,count,parent[0]
                             if count == signalindex:
                                #print "match",row
                                temp = (row)
                                break
                             continue
                       for column,child in enumerate(parent[1]):
                           count +=8
                           #print row,column,count,parent[0],child
                           if count == signalindex:
                               #print "match",row
                               temp = (row,column)
                               break
                           if count >= signalindex:break
                    treeiter = self.d._tppwmsignaltree.get_iter(temp)
                    self.widgets[p].set_active_iter(treeiter)

                # type step gen
                elif widgetptype == _PD.STEPA:
                    #print "stepper", dataptype
                    self.widgets[ptype].set_active(0)
                    self.widgets[p].set_active(0)
                    self.widgets[pinv].set_active(datapinv)
                    itr = self.find_sig_name_iter(self.d._steppersignaltree, datap)
                    self.widgets[p].set_active_iter(itr)

                # type smartserial
                # we do things differently here
                # we find the data's signal index
                # then we search through the combobox's model's 4th array index
                # this contains the comboxbox's signal's index number
                # when they match then that is the row to show in the combobox
                # this is different because the sserial combobox's model
                # can be filtered and that screws with the relationship of
                # signalname array vrs model row
                elif widgetptype in( _PD.TXDATA0,_PD.SS7I76M0,_PD.SS7I77M0,_PD.SS7I77M3,_PD.TXDATA1,
                                    _PD.TXDATA2,_PD.TXDATA3,_PD.TXDATA4,_PD.TXDATA5,_PD.TXDATA6,_PD.TXDATA7,
                                    _PD.SS7I76M2,_PD.SS7I76M3,_PD.SS7I77M1,_PD.SS7I77M4):
                    #print "SMART SERIAL", dataptype,widgetptype
                    self.widgets[pinv].set_active(datapinv)
                    try:
                        signalindex = _PD.hal_sserial_names.index(self.d[p])
                    except:
                        if debug: print "**** INFO: PNCCONF warning no SMART SERIAL signal named: %s\n     found for pin %s"% (datap ,p)
                        signalindex = 0
                    
                    pinmodel = self.widgets[p].get_model()
                    for row,parent in enumerate(pinmodel):
                            #print row,parent[0],parent[2],parent[3],parent[4]
                            if parent[4] == signalindex:
                                #print 'FOUND',parent[2],parent[4]
                                treeiter = pinmodel.get_iter(row)
                                self.widgets[p].set_active_iter(treeiter)
                else:
                    print "**** WARNING: PNCCONF data to widget: ptype not recognized/match:",dataptype,widgetptype

    # This is for when a user picks a signal name or creates a custom signal (by pressing enter)
    # if searches for the 'related pins' of a component so it can update them too
    # it also handles adding and updating custom signal names
    # it is used for mesa boards and parport boards according to boardtype
    def on_general_pin_changed(self, widget, boardtype, boardnum, connector, channel, pin, custom):
                self.p.set_buttons_sensitive(0,0)
                if boardtype == "sserial":
                    p = 'mesa%dsserial%d_%dpin%d' % (boardnum,connector,channel,pin)
                    ptype = 'mesa%dsserial%d_%dpin%dtype' % (boardnum,connector,channel,pin)
                    widgetptype = self.widgets[ptype].get_active_text()
                    #print "pinchanged-",p
                elif boardtype == "mesa":
                    p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                    ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                    widgetptype = self.widgets[ptype].get_active_text()
                elif boardtype == "parport":
                    p = '%s_%s%d' % (boardnum,connector, pin)
                    #print p
                    if "I" in p: widgetptype = _PD.GPIOI
                    else: widgetptype = _PD.GPIOO
                pinchanged =  self.widgets[p].get_active_text()
                piter = self.widgets[p].get_active_iter()
                signaltree = self.widgets[p].get_model()
                try:
                    basetree = signaltree.get_model()
                except:
                    basetree = signaltree
                #print "generalpin changed",p
                #print "*** INFO ",boardtype,"-pin-changed: pin:",p,"custom:",custom
                #print "*** INFO ",boardtype,"-pin-changed: ptype:",widgetptype,"pinchaanged:",pinchanged
                if piter == None and not custom:
                    #print "*** INFO ",boardtype,"-pin-changed: no iter and not custom"
                    self.p.set_buttons_sensitive(1,1)
                    return
                if widgetptype in (_PD.ENCB,_PD.ENCI,_PD.ENCM,
                                    _PD.MXEI,_PD.MXEM,_PD.MXES,
                                    _PD.RESU,
                                    _PD.STEPB,_PD.STEPC,_PD.STEPD,_PD.STEPE,_PD.STEPF,
                                    _PD.PDMD,_PD.PDME,_PD.PWMD,_PD.PWME,_PD.UDMD,_PD.UDME,
                                    _PD.TPPWMB,_PD.TPPWMC,_PD.TPPWMAN,_PD.TPPWMBN,_PD.TPPWMCN,_PD.TPPWME,_PD.TPPWMF,
                                    _PD.RXDATA0,_PD.TXEN0,_PD.RXDATA1,_PD.TXEN1,_PD.RXDATA2,_PD.TXEN2,_PD.RXDATA3,_PD.TXEN3,
                                    _PD.POTE,_PD.POTD
                                    ):return
                # for GPIO output
                if widgetptype in (_PD.GPIOO,_PD.GPIOD):
                    #print"ptype GPIOO\n"
                    halsignallist = 'hal_output_names'
                    humansignallist = _PD.human_output_names
                    addsignalto = self.d.haloutputsignames
                    relatedsearch = ["dummy"]
                    relatedending = [""]
                    customindex = len(humansignallist)-1
                # for GPIO input
                elif widgetptype == _PD.GPIOI:
                    #print"ptype GPIOI\n"
                    halsignallist =  'hal_input_names'
                    humansignallist = _PD.human_input_names
                    addsignalto = self.d.halinputsignames
                    relatedsearch = ["dummy"]
                    relatedending = [""]
                    customindex = len(humansignallist)-1
                # for stepgen pins
                elif widgetptype == _PD.STEPA:
                    #print"ptype step\n"
                    halsignallist = 'hal_stepper_names'
                    humansignallist = _PD.human_stepper_names
                    addsignalto = self.d.halsteppersignames
                    relatedsearch = [_PD.STEPA,_PD.STEPB,_PD.STEPC,_PD.STEPD,_PD.STEPE,_PD.STEPF]
                    relatedending = ["-step","-dir","-c","-d","-e","-f"]
                    customindex = len(humansignallist)-1
                # for encoder pins
                elif widgetptype == _PD.ENCA: 
                    #print"\nptype encoder"
                    halsignallist = 'hal_encoder_input_names'
                    humansignallist = _PD.human_encoder_input_names
                    addsignalto = self.d.halencoderinputsignames
                    relatedsearch = [_PD.ENCA,_PD.ENCB,_PD.ENCI,_PD.ENCM]
                    relatedending = ["-a","-b","-i","-m"]
                    customindex = len(humansignallist)-1
                # for mux encoder pins
                elif widgetptype in(_PD.MXE0,_PD.MXE1): 
                    #print"\nptype encoder"
                    halsignallist = 'hal_encoder_input_names'
                    humansignallist = _PD.human_encoder_input_names
                    addsignalto = self.d.halencoderinputsignames
                    relatedsearch = ["dummy","dummy","dummy","dummy",]
                    relatedending = ["-a","-b","-i","-m"]
                    customindex = len(humansignallist)-1
                # resolvers 
                elif widgetptype in (_PD.RES0,_PD.RES1,_PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5):
                    halsignallist = 'hal_resolver_input_names'
                    humansignallist = _PD.human_resolver_input_names
                    addsignalto = self.d.halresolversignames
                    relatedsearch = ["dummy"]
                    relatedending = [""]
                    customindex = len(humansignallist)-1
                # 8i20 amplifier 
                elif widgetptype == _PD.AMP8I20:
                    halsignallist = 'hal_8i20_input_names'
                    humansignallist = _PD.human_8i20_input_names
                    addsignalto = self.d.hal8i20signames
                    relatedsearch = ["dummy"]
                    relatedending = [""]
                    customindex = len(humansignallist)-1
                # potentiometer output
                elif widgetptype == _PD.POTO:
                    halsignallist = 'hal_pot_output_names'
                    humansignallist = _PD.human_pot_output_names
                    addsignalto = self.d.halpotsignames
                    relatedsearch = [_PD.POTO,_PD.POTE]
                    relatedending = ["-output","-enable"]
                    customindex = 2
                # analog input
                elif widgetptype == _PD.ANALOGIN:
                    halsignallist = 'hal_analog_input_names'
                    humansignallist = _PD.human_analog_input_names
                    addsignalto = self.d.halanaloginsignames
                    relatedsearch = ["dummy"]
                    relatedending = [""]
                    customindex = len(humansignallist)-1
                # for PWM,PDM,UDM pins
                elif widgetptype in(_PD.PWMP,_PD.PDMP,_PD.UDMU): 
                    #print"ptype pwmp\n"
                    halsignallist = 'hal_pwm_output_names'
                    humansignallist = _PD.human_pwm_output_names
                    addsignalto = self.d.halpwmoutputsignames
                    relatedsearch = [_PD.PWMP,_PD.PWMD,_PD.PWME]
                    relatedending = ["-pulse","-dir","-enable"]
                    customindex = len(humansignallist)-1
                elif widgetptype == _PD.TPPWMA: 
                    #print"ptype pdmp\n"
                    halsignallist = 'hal_tppwm_output_names'
                    humansignallist = _PD.human_tppwm_output_names
                    addsignalto = self.d.haltppwmoutputsignames
                    relatedsearch = [_PD.TPPWMA,_PD.TPPWMB,_PD.TPPWMC,_PD.TPPWMAN,_PD.TPPWMBN,_PD.TPPWMCN,_PD.TPPWME,_PD.TPPWMF]
                    relatedending = ["-a","-b","c","-anot","-bnot","cnot","-enable","-fault"]
                    customindex = len(humansignallist)-1
                elif widgetptype in (_PD.TXDATA0,_PD.TXDATA1,_PD.TXDATA2,_PD.TXDATA3,_PD.TXDATA4,_PD.TXDATA5,_PD.SS7I76M0,_PD.SS7I76M3,
                                     _PD.SS7I76M2,_PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4):
                    portnum = 0 #TODO support more ports
                    for count,temp in enumerate(self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS]) :
                        if connector == temp:
                            firmptype,portnum = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._STARTOFDATA+pin+(count*24)]
                            if widgetptype in (_PD.TXDATA0,_PD.SS7I76M0,_PD.SS7I77M0): channelnum = 0
                            elif widgetptype in (_PD.TXDATA1,_PD.SS7I77M1): channelnum = 1
                            elif widgetptype in (_PD.TXDATA2,_PD.SS7I76M2): channelnum = 2
                            elif widgetptype in (_PD.TXDATA3,_PD.SS7I77M3,_PD.SS7I76M3): channelnum = 3
                            elif widgetptype in (_PD.TXDATA4,_PD.SS7I77M4): channelnum = 4
                            elif widgetptype in (_PD.TXDATA5): channelnum = 5
                            BASE = "mesa%dsserial0_%d"% (boardnum,channelnum)
                            if self.widgets[p].get_active_text() == _("Unused Channel"):
                                self.widgets[BASE].hide()
                                self.d[BASE+"subboard"] = "none"
                                return
                            else:
                                self.widgets[BASE].show()
                                # TODO we should search for these names rather then use hard coded logic
                                # so as to make adding cards easier
                                temp = self.widgets[p].get_active_text()
                                table = BASE+"table2"
                                self.widgets[table].show()
                                table = BASE+"table3"
                                self.widgets[table].show()
                                if "7i76" in temp:
                                    if 'Mode 2' in temp:
                                        ssfirmname = "7i76-m2"
                                    else:
                                        ssfirmname = "7i76-m0"
                                    self.d[BASE+"subboard"] = ssfirmname
                                    self.widgets[BASE+'_tablabel'].set_text("7I76 I/O\n (SS# %d)"% channelnum)
                                elif "7i64" in temp:
                                    self.d[BASE+"subboard"] = "7i64"
                                    self.widgets[BASE+'_tablabel'].set_text("7I64 I/O\n (SS# %d)"% channelnum)
                                elif "7i69" in temp:
                                    self.d[BASE+"subboard"] = "7i69"
                                    self.widgets[table].hide()
                                    self.widgets[BASE+'_tablabel'].set_text("7I69 I/O\n (SS# %d)"% channelnum)
                                elif "7i70" in temp:
                                    self.d[BASE+"subboard"] = "7i70"
                                    self.widgets[table].hide()
                                    self.widgets[BASE+'_tablabel'].set_text("7I70 I/O\n (SS# %d)"% channelnum)
                                elif "7i71" in temp:
                                    self.d[BASE+"subboard"] = "7i71"
                                    self.widgets[table].hide()
                                    self.widgets[BASE+'_tablabel'].set_text("7I71 I/O\n (SS# %d)"% channelnum)
                                elif "7i73" in temp:
                                    self.d[BASE+"subboard"] = "7i73-m1"
                                    self.widgets[BASE+'_tablabel'].set_text("7I73 I/O\n (SS# %d)"% channelnum)
                                elif "7i77" in temp:
                                    print 'ssname',temp,'sschannel#',channelnum
                                    if 'Mode 3' in temp:
                                        ssfirmname = "7i77-m3"
                                    else:
                                        ssfirmname = "7i77-m0"
                                    self.d[BASE+"subboard"] = ssfirmname
                                    if channelnum in(0,3):
                                        self.widgets[BASE+'_tablabel'].set_text("7I77 I/O\n (SS# %d)"% channelnum)
                                        self.widgets[table].hide()
                                    elif channelnum in(1,4):
                                        self.widgets[BASE+'_tablabel'].set_text("7I77 PWM\n (SS# %d)"% channelnum)
                                        table = BASE+"table2"
                                        self.widgets[table].hide()
                                        table = BASE+"table1"
                                        self.widgets[table].hide()
                                elif "7i84" in temp:
                                    print 'ssname',temp,'sschannel#',channelnum
                                    if 'Mode 3' in temp:
                                        ssfirmname = "7i84-m3"
                                    else:
                                        ssfirmname = "7i84-m0"
                                    self.d[BASE+"subboard"] = ssfirmname
                                    self.widgets[table].hide()
                                    self.widgets[BASE+'_tablabel'].set_text("7I84 I/O\n (SS# %d)"%channelnum)
                                elif "8i20" in temp:
                                    self.d[BASE+"subboard"] = "8i20"
                                    self.widgets[table].hide()
                                    table = BASE+"table2"
                                    self.widgets[table].hide()
                                    self.widgets[BASE+'_tablabel'].set_text("8I20\n (SS# %d)"% channelnum)
                                else:
                                    self.d[BASE+"subboard"] = "none"
                                    self.widgets[table].hide()
                                    table = BASE+"table2"
                                    self.widgets[table].hide()
                                    table = BASE+"table1"
                                    self.widgets[table].hide()
                                    return
                                # set sserial tab names to corresond to connector numbers so users have a clue
                                # first we have to find the daughter board in pncconf's internal list
                                # TODO here we search the list- this should be done for the table names see above todo
                                subfirmname = self.d[BASE+"subboard"]
                                for subnum,temp in enumerate(self._p.MESA_DAUGHTERDATA):
                                    if self._p.MESA_DAUGHTERDATA[subnum][self._p._SUBFIRMNAME] == subfirmname: break
                                subconlist = self._p.MESA_DAUGHTERDATA[subnum][self._p._SUBCONLIST]
                                # now search the connector list and write it to the tab names
                                for tabnum in range(0,3):
                                    conname = subconlist[tabnum]
                                    tab = BASE+"tab%d"% tabnum
                                    self.widgets[tab].set_text(conname)

                                #print p,temp," set at",self.d[BASE+"subboard"]
                                self.set_sserial_options(boardnum,portnum,channelnum)
                                self.p.set_buttons_sensitive(1,1)
                                return
                    self.p.set_buttons_sensitive(1,1)
                    return
                else:
                    print"**** INFO: pncconf on_general_pin_changed:  pintype not found:%s\n"% widgetptype
                    self.p.set_buttons_sensitive(1,1)
                    return   
                # *** change the related pin's signal names ***
                     
                # see if the piter is none - if it is a custom names has been entered
                # else find the signal name index number if the index is zero set the piter to unused signal
                # this is a work around for thye combo box allowing the parent to be shown and selected in the
                # child column haven\t figured out how to stop that #TODO
                # either way we have to search the current firmware array for the pin numbers of the related
                # pins so we can change them to the related signal name 
                # all signal names have related signal (eg encoders have A and B phase and index and index mask)
                # except 'unused' signal it is a special case as there is no related signal names with it.
                if piter == None or custom:
                    #print "*** INFO ",boardtype,"-pin-changed: PITER:",piter," length:",len(signaltree)
                    if pinchanged in (addsignalto):return
                    for i in (humansignallist):
                        if pinchanged == i[0]:return
                        if pinchanged in i[1]:return
                    length = len(signaltree)
                    index = len(_PD[halsignallist]) - len(relatedsearch)
                    customiter = signaltree.get_iter((length-1,))
                    childiter = signaltree.iter_nth_child(customiter, 0)
                    n = 0
                    while childiter:
                        dummy, index = signaltree.get(childiter, 0, 1)
                        n+=1
                        childiter = signaltree.iter_nth_child(customiter, n)
                    index += len(relatedsearch)
                    
                else:
                    dummy, index = signaltree.get(piter, 0, 1)
                    if index == 0:
                        piter = signaltree.get_iter_first()
                #print "*** INFO ",boardtype,"-pin-changed: index",index
                # This finds the pin type and component number of the pin that has changed
                pinlist = []
                # this components have no related pins - fake the list
                if widgetptype in(_PD.GPIOI,_PD.GPIOO,_PD.GPIOD,_PD.MXE0,_PD.MXE1,_PD.RES0,_PD.RES1,
                                    _PD.RES2,_PD.RES3,_PD.RES4,_PD.RES5,_PD.AMP8I20,_PD.ANALOGIN):
                    pinlist = [["%s"%p,boardnum,connector,channel,pin]]
                else:
                    pinlist = self.d.list_related_pins(relatedsearch, boardnum, connector, channel, pin, 0)
                #print pinlist
                # Now we have a list of pins that need to be updated
                # first check if the name is a custom name if it is
                #   add the legalized custom name to ;
                #   addsignalto -> for recording custom names for next time loaded
                #   signalsto check -> for making signal names (we add different endings for different signalnames
                #   signaltree -> for display in the gui - itis automatically added to all comboboxes that uses this treesort
                # then go through the pinlist:
                # block signals
                # display the proper text depending if custom or not
                # then unblock signals
                if custom:
                    legal_name = pinchanged.replace(" ","_")
                    addsignalto.append ((legal_name))
                    print "add: "+legal_name+" to human list",humansignallist[customindex][1]
                    humansignallist[customindex][1].append ((legal_name))
                    endoftree = len(basetree)-1
                    customiter = basetree.get_iter((endoftree,))
                    newiter = basetree.append(customiter, [legal_name,index,legal_name,halsignallist,index])
                    #print 'new signal:',legal_name,index,legal_name,halsignallist,endoftree,index
                    for offset,i in enumerate(relatedsearch):
                        with_endings = legal_name + relatedending[offset]
                        #print "new signal:",with_endings
                        _PD[halsignallist].append ((with_endings))
                for data in(pinlist):
                    if boardtype == "mesa":
                        blocksignal1 = "_mesa%dsignalhandlerc%ipin%i" % (data[1], data[2], data[4])
                        blocksignal2 = "_mesa%dactivatehandlerc%ipin%i"  % (data[1], data[2], data[4])
                    if boardtype == "sserial":
                        blocksignal1 = "_mesa%dsignalhandlersserial%i_%ipin%i" % (data[1], data[2], data[3], data[4])
                        blocksignal2 = "_mesa%dactivatehandlersserial%i_%ipin%i"  % (data[1], data[2], data[3],data[4])
                    elif boardtype =="parport":
                        blocksignal1 = "_%s_%s%dsignalhandler" % (data[1], data[2], data[4])
                        blocksignal2 = "_%s_%s%dactivatehandler"  % (data[1], data[2], data[4])
                    self.widgets[data[0]].handler_block(self.d[blocksignal1])
                    self.widgets[data[0]].child.handler_block(self.d[blocksignal2])
                    if custom:
                        if basetree == signaltree:
                            temp = newiter
                        else:
                            temp = signaltree.convert_child_iter_to_iter(newiter)
                        self.widgets[data[0]].set_active_iter(temp)
                    else:
                        self.widgets[data[0]].set_active_iter(piter)

                    self.widgets[data[0]].child.handler_unblock(self.d[blocksignal2])
                    self.widgets[data[0]].handler_unblock(self.d[blocksignal1])
                #self.debug_iter(0,p,"pin changed")
                #if boardtype == "mesa": self.debug_iter(0,ptype,"pin changed")
                self.p.set_buttons_sensitive(1,1)


    def pport_push_data(self,port,direction,pin,pinv,signaltree,signaltocheck):
            p = '%s_%s%d' % (port, direction, pin)
            piter = self.widgets[p].get_active_iter()
            selection = self.widgets[p].get_active_text()
            # **Start widget to data Convertion**                    
            if piter == None:# means new custom signal name and user never pushed enter
                    #print "callin pin changed !!!"
                    self.on_general_pin_changed( None,"parport", port, direction, None, pin, True)
                    selection = self.widgets[p].get_active_text()
                    piter = self.widgets[p].get_active_iter()
                    #print "found signame -> ",selection," "
            # ok we have a piter with a signal type now- lets convert it to a signalname
            #print "**** INFO parport-data-transfer piter:",piter
            #self.debug_iter(piter,p,"signal")
            dummy, index = signaltree.get(piter,0,1)
            #print "signaltree: ",dummy
            return p, signaltocheck[index], self.widgets[pinv].get_active()

    def set_pport_combo(self,pinname):
            #print pinname
            # signal names for GPIO INPUT
            datap = self.d[pinname]
            if '_Ipin' in pinname:
                human = self._p.human_input_names
                signal = self._p.hal_input_names
                tree = self.d._gpioisignaltree
                # signal names for GPIO OUTPUT and OPEN DRAIN OUTPUT
            elif 'Opin'in pinname:
                human = self._p.human_output_names
                signal =self._p.hal_output_names
                tree = self.d._gpioosignaltree
            #self.w[pinname].set_model(tree)
            # an error probably means the signal name cannot be found
            # set it as unused rather then error

            itr = self.find_sig_name_iter(tree, datap)
            self.widgets[pinname].set_active_iter(itr)
            return
            try:
                signalindex = signal.index(datap)
            except:
                signalindex = 0
                print "**** INFO: PNCCONF warning no GPIO signal named: %s\n       found for pin %s"% (datap , p)
            #print "gpio temp ptype:",pinname,datap,signalindex
            count = 0
            temp = (0) # set unused gpio if no match
            if signalindex > 0:
                for row,parent in enumerate(human):
                    #print row,parent
                    if len(parent[1]) == 0:continue
                    for column,child in enumerate(parent[1]):
                        count +=1
                        #print row,column,count,parent[0],child
                        if count == signalindex:
                            #print "match",row,column
                            break
                    if count >= signalindex:break
                temp = (row,column)
            treeiter = tree.get_iter(temp)
            self.widgets[pinname].set_active_iter(treeiter)
        
    def signal_sanity_check(self, *args):
        warnings = []
        do_warning = False
        do_error = False
        for i in self.d.available_axes:
            tppwm = pwm = amp_8i20 = False
            step = self.d.findsignal(i+"-stepgen-step")
            step2 = self.d.findsignal(i+"2-stepgen-step")
            enc = self.d.findsignal(i+"-encoder-a")
            resolver = self.d.findsignal(i+"-resolver")
            if self.d.findsignal("%s-8i20"% i): amp_8i20 = pwm =True
            if self.d.findsignal(i+"-pwm-pulse"): pwm = True
            if self.d.findsignal(i+"-tppwm-a"): tppwm = pwm = True
            #print "signal sanity check: axis",i,"\n    pwm = ",pwm,"\n    3pwm =",tppwm,"\n    encoder =",enc,"\n    step=",step
            if i == 's':
                if step and pwm:
                    warnings.append(_("You can not have both steppers and pwm signals for spindle control\n") )
                    do_error = True
                continue
            if not step and not pwm:
                warnings.append(_("You forgot to designate a stepper or pwm signal for axis %s\n")% i)
                do_error = True
            if pwm and not (enc or resolver):
                warnings.append(_("You forgot to designate an encoder /resolver signal for axis %s servo\n")% i)
                do_error = True
            if enc and not pwm and not step: 
                warnings.append(_("You forgot to designate a pwm signal or stepper signal for axis %s\n")% i)
                do_error = True
            if step and pwm: 
                warnings.append(_("You can not have both steppers and pwm signals for axis %s\n")% i)
                do_error = True
            if step2 and not step: 
                warnings.append(_("If using a tandem axis stepper, you must select a master stepgen for axis %s\n")% i)
                do_error = True
        if self.d.frontend == _PD._TOUCHY:# TOUCHY GUI
            abort = self.d.findsignal("abort")
            cycle = self.d.findsignal("cycle-start")
            single = self.d.findsignal("single-step")
            mpg = self.d.findsignal("select-mpg-a")
            if not cycle: 
                warnings.append(_("Touchy require an external cycle start signal\n"))
                do_warning = True
            if not abort: 
                warnings.append(_("Touchy require an external abort signal\n"))
                do_warning = True
            if not single: 
                warnings.append(_("Touchy require an external single-step signal\n"))
                do_warning = True
            if not mpg: 
                warnings.append(_("Touchy require an external multi handwheel MPG encoder signal on the mesa page\n"))
                do_warning = True
            if not self.d.externalmpg:
                warnings.append(_("Touchy require 'external mpg jogging' to be selected on the external control page\n"))
                do_warning = True
            if self.d.multimpg:
                warnings.append(_("Touchy require the external mpg to be in 'shared mpg' mode on the external controls page\n"))
                do_warning = True
            if self.d.incrselect:
                warnings.append(_("Touchy require selectable increments to be unchecked on the external controls page\n"))
                do_warning = True

        if do_warning or do_error:
            self.warning_dialog("\n".join(warnings),True)
        if do_error: return True
        return False

    def daughter_board_sanity_check(self,widget):
        warnings = []
        do_warning = False
        for boardnum in range(0,int(self.d.number_mesa)):
            if widget == self.widgets["mesa%d_7i29_sanity_check"%boardnum]:
                warnings.append(_("The 7i29 daughter board requires PWM type generators and a PWM base frequency of 20 khz\n"))
                do_warning = True
            if widget == self.widgets["mesa%d_7i30_sanity_check"%boardnum]:
                warnings.append(_("The 7i30 daughter board requires PWM type generators and a PWM base frequency of 20 khz\n"))
                do_warning = True
            if widget == self.widgets["mesa%d_7i33_sanity_check"%boardnum]:
                warnings.append(_("The 7i33 daughter board requires PDM type generators and a PDM base frequency of 6 Mhz\n"))
                do_warning = True
            if widget == self.widgets["mesa%d_7i40_sanity_check"%boardnum]:
                warnings.append(_("The 7i40 daughter board requires PWM type generators and a PWM base frequency of 50 khz\n"))
                do_warning = True
            if widget == self.widgets["mesa%d_7i48_sanity_check"%boardnum]:
                warnings.append(_("The 7i48 daughter board requires UDM type generators and a PWM base frequency of 24 khz\n"))
                do_warning = True

        if do_warning:
            self.warning_dialog("\n".join(warnings),True)

    def axis_prepare(self, axis):
        d = self.d
        w = self.widgets
        def set_text_from_text(n): w[axis + n].set_text("%s" % d[axis + n])
        def set_text(n): w[axis + n].set_text(locale.format("%.4f", (d[axis + n])))
        def set_value(n): w[axis + n].set_value(d[axis + n])
        def set_active(n): w[axis + n].set_active(d[axis + n])
        stepdriven = encoder = pwmgen = resolver = tppwm = digital_at_speed = amp_8i20 = False
        spindlepot = sserial_scaling = False
        vfd_spindle = self.d.serial_vfd and (self.d.mitsub_vfd or self.d.gs2_vfd) 
        if self.d.findsignal("%s-8i20"% axis):amp_8i20 = True
        if self.d.findsignal("spindle-at-speed"): digital_at_speed = True
        if self.d.findsignal(axis+"-stepgen-step"): stepdriven = True
        if self.d.findsignal(axis+"-encoder-a"): encoder = True
        if self.d.findsignal(axis+"-resolver"): encoder = resolver = True
        temp = self.d.findsignal(axis+"-pwm-pulse")
        if temp:
            pwmgen = True
            pinname = self.d.make_pinname(temp)
            if "analog" in pinname: sserial_scaling = True
        if self.d.findsignal(axis+"-tppwm-a"): pwmgen = tppwm = True
        if self.d.findsignal(axis+"-pot-output"): spindlepot = sserial_scaling = True

        model = w[axis+"drivertype"].get_model()
        model.clear()
        for i in _PD.alldrivertypes:
            model.append((i[1],))
        model.append((_("Custom"),))   
        w["steprev"].set_text("%s" % d[axis+"steprev"])
        w["microstep"].set_text("%s" % d[axis +"microstep"])
        # P setting needs to default to different values based on
        # stepper vrs servo configs. But we still want to allow user setting it.
        # If the value is None then we should set a default value, if not then
        # that means it's been set to something already...hopefully right.
        # TODO this should be smarter - after going thru a config once it
        # always uses the value set here - if it is set to a default value
        # if should keep checking that the value is still right.
        # but thats a bigger change then we want now.
        if not d[axis + "P"] == None:
            set_value("P")
        elif stepdriven == True:
            w[axis + "P"].set_value(1/(d.servoperiod/1000000000))
        else:
            w[axis + "P"].set_value(50)
        set_value("I")
        set_value("D")
        set_value("FF0")
        set_value("FF1")
        set_value("FF2")
        set_value("bias")
        set_value("deadband")
        set_value("steptime")
        set_value("stepspace")
        set_value("dirhold")
        set_value("dirsetup")
        set_value("outputscale")
        set_value("3pwmscale")
        set_value("3pwmdeadtime")
        set_active("invertmotor")
        set_active("invertencoder")
        set_value("maxoutput")
        if amp_8i20:
            w[axis + "bldc_option"].set_active(True)
        else:
            set_active("bldc_option")
        
        set_active("bldc_no_feedback")
        set_active("bldc_absolute_feedback")
        set_active("bldc_incremental_feedback")
        set_active("bldc_use_hall")
        set_active("bldc_use_encoder" )
        set_active("bldc_use_index")
        set_active("bldc_fanuc_alignment")
        set_active("bldc_digital_output")
        set_active("bldc_six_outputs")
        set_active("bldc_emulated_feedback")
        set_active("bldc_output_hall")
        set_active("bldc_output_fanuc")
        set_active("bldc_force_trapz")

        set_active("bldc_reverse")
        set_value("bldc_scale")
        set_value("bldc_poles")
        set_value("bldc_lead_angle")
        set_value("bldc_inital_value")
        set_value("bldc_encoder_offset")
        set_value("bldc_drive_offset")
        set_value("bldc_pattern_out")
        set_value("bldc_pattern_in")
        set_value("8i20maxcurrent")

        w["encoderline"].set_value((d[axis+"encodercounts"]/4))
        set_value("stepscale")
        set_value("encoderscale")
        w[axis+"maxvel"].set_value(d[axis+"maxvel"]*60)
        set_value("maxacc")
        if not axis == "s" or axis == "s" and (encoder and (pwmgen or tppwm or stepdriven or sserial_scaling)):
            w[axis + "servo_info"].show()
        else:
            w[axis + "servo_info"].hide()
        if stepdriven or not (pwmgen or spindlepot):
            w[axis + "output_info"].hide()
        else:
            w[axis + "output_info"].show()
        w[axis + "invertencoder"].set_sensitive(encoder)
        w[axis + "encoderscale"].set_sensitive(encoder)
        w[axis + "stepscale"].set_sensitive(stepdriven)
        if stepdriven:
            w[axis + "stepper_info"].show()
        else:
            w[axis + "stepper_info"].hide()
        if pwmgen or sserial_scaling:
            w[axis + "outputscale"].show()
            w[axis + "outputscalelabel"].show()
        else:
            w[axis + "outputscale"].hide()
            w[axis + "outputscalelabel"].hide()
        if amp_8i20 or pwmgen and d.advanced_option == True:
            w[axis + "bldcframe"].show()
        else: w[axis + "bldcframe"].hide()
        if tppwm:
            w[axis + "3pwmdeadtime"].show()
            w[axis + "3pwmscale"].show()
            w[axis + "3pwmdeadtimelabel"].show()
            w[axis + "3pwmscalelabel"].show()
        else:
            w[axis + "3pwmdeadtime"].hide()
            w[axis + "3pwmscale"].hide()
            w[axis + "3pwmdeadtimelabel"].hide()
            w[axis + "3pwmscalelabel"].hide()
        w[axis + "drivertype"].set_active(self.drivertype_toindex(axis))
        if w[axis + "drivertype"].get_active_text()  == _("Custom"):
            w[axis + "steptime"].set_value(d[axis + "steptime"])
            w[axis + "stepspace"].set_value(d[axis + "stepspace"])
            w[axis + "dirhold"].set_value(d[axis + "dirhold"])
            w[axis + "dirsetup"].set_value(d[axis + "dirsetup"])
        gobject.idle_add(lambda: self.motor_encoder_sanity_check(None,axis))

        if axis == "s":
            unit = "rev"
            pitchunit =_("Gearbox Reduction Ratio")
        elif axis == "a":
            unit = "degree"
            pitchunit = _("Reduction Ratio")
        elif d.units ==_PD._METRIC:
            unit = "mm"
            pitchunit =_("Leadscrew Pitch")
        else:
            unit = "inch"
            pitchunit =_("Leadscrew TPI")
        if axis == "s" or axis =="a":
            w["labelmotor_pitch"].set_text(pitchunit)
            w["labelencoder_pitch"].set_text(pitchunit)
            w["motor_screwunits"].set_text(_("("+unit+" / rev)"))
            w["encoder_screwunits"].set_text(_("("+unit+" / rev)"))
        w[axis + "velunits"].set_text(_(unit+" / min"))
        w[axis + "accunits"].set_text(_(unit+" / sec²"))
        w["accdistunits"].set_text(unit)
        if stepdriven:
            w[ "resolutionunits1"].set_text(_(unit+" / Step"))
            w["scaleunits"].set_text(_("Steps / "+unit))
        else:
            w["resolutionunits1"].set_text(_(unit+" / encoder pulse"))
            w["scaleunits"].set_text(_("Encoder pulses / "+unit))
        if not axis =="s":
            w[axis + "homevelunits"].set_text(_(unit+" / min"))
            w[axis + "homelatchvelunits"].set_text(_(unit+" / min"))
            w[axis + "homefinalvelunits"].set_text(_(unit+" / min"))
            w[axis + "minfollowunits"].set_text(unit)
            w[axis + "maxfollowunits"].set_text(unit)
        if resolver:
            w[axis + "encoderscale_label"].set_text(_("Resolver Scale:"))
        if axis == 's':
            if vfd_spindle:
                w.serial_vfd_info.show()
            else:
                w.serial_vfd_info.hide()
            set_value("outputscale2")
            w.ssingleinputencoder.set_sensitive(encoder)
            w["sinvertencoder"].set_sensitive(encoder)
            w["ssingleinputencoder"].show()
            w["saxistest"].set_sensitive(pwmgen or spindlepot)
            w["sstepper_info"].set_sensitive(stepdriven)
            w["smaxvel"].set_sensitive(stepdriven)
            w["smaxacc"].set_sensitive(stepdriven)
            w["suseatspeed"].set_sensitive(not digital_at_speed and encoder)
            if encoder or resolver:
                if (self.d.pyvcp and self.d.pyvcphaltype == 1 and self.d.pyvcpconnect == 1) or (self.d.gladevcp 
                    and self.d.spindlespeedbar):
                    w["sfiltergain"].set_sensitive(True)
            set_active("useatspeed")
            w.snearrange_button.set_active(d.susenearrange)
            w["snearscale"].set_value(d["snearscale"]*100)
            w["snearrange"].set_value(d["snearrange"])
            set_value("filtergain")
            set_active("singleinputencoder")
            set_value("outputmaxvoltage")
            set_active("usenegativevoltage")
            set_active("useoutputrange2")
            self.useoutputrange2_toggled()
        else:
            if sserial_scaling:
                w[axis + "outputminlimit"].show()
                w[axis + "outputminlimitlabel"].show()
                w[axis + "outputmaxlimit"].show()
                w[axis + "outputmaxlimitlabel"].show()
            else:
                w[axis + "outputminlimit"].hide()
                w[axis + "outputminlimitlabel"].hide()
                w[axis + "outputmaxlimit"].hide()
                w[axis + "outputmaxlimitlabel"].hide()
            set_value("outputminlimit")
            set_value("outputmaxlimit")
            set_text("encodercounts")
            w[axis+"maxferror"].set_sensitive(True)
            w[axis+"minferror"].set_sensitive(True)
            set_value("maxferror")
            set_value("minferror")
            set_text_from_text("compfilename")
            set_active("comptype")
            set_active("usebacklash")
            set_value("backlash")
            set_active("usecomp")      
            set_text("homepos")
            set_text("minlim")
            set_text("maxlim")
            set_text("homesw")
            w[axis+"homesearchvel"].set_text("%d" % (d[axis+"homesearchvel"]*60))
            w[axis+"homelatchvel"].set_text("%d" % (d[axis+"homelatchvel"]*60))
            w[axis+"homefinalvel"].set_text("%d" % (d[axis+"homefinalvel"]*60))
            w[axis+"homesequence"].set_text("%d" % d[axis+"homesequence"])
            set_active("searchdir")
            set_active("latchdir")
            set_active("usehomeindex")
            thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis,"max-home-" + axis, "both-home-" + axis))
            homes = False
            for i in thisaxishome:
                test = self.d.findsignal(i)
                if test: homes = True
            w[axis + "homesw"].set_sensitive(homes)
            w[axis + "homesearchvel"].set_sensitive(homes)
            w[axis + "searchdir"].set_sensitive(homes)
            w[axis + "latchdir"].set_sensitive(homes)
            w[axis + "usehomeindex"].set_sensitive(encoder and homes)
            w[axis + "homefinalvel"].set_sensitive(homes)
            w[axis + "homelatchvel"].set_sensitive(homes)
            i = d[axis + "usecomp"]
            w[axis + "comptype"].set_sensitive(i)
            w[axis + "compfilename"].set_sensitive(i)
            i = d[axis + "usebacklash"]
            w[axis + "backlash"].set_sensitive(i)
        self.p.set_buttons_sensitive(1,0)
        self.motor_encoder_sanity_check(None,axis)



    def driver_changed(self, axis):
        d = self.d
        w = self.widgets
        v = w[axis + "drivertype"].get_active()
        if v < len(_PD.alldrivertypes):
            d = _PD.alldrivertypes[v]
            w[axis + "steptime"].set_value(d[2])
            w[axis + "stepspace"].set_value(d[3])
            w[axis + "dirhold"].set_value(d[4])
            w[axis + "dirsetup"].set_value(d[5])

            w[axis + "steptime"].set_sensitive(0)
            w[axis + "stepspace"].set_sensitive(0)
            w[axis + "dirhold"].set_sensitive(0)
            w[axis + "dirsetup"].set_sensitive(0)
        else:
            w[axis + "steptime"].set_sensitive(1)
            w[axis + "stepspace"].set_sensitive(1)
            w[axis + "dirhold"].set_sensitive(1)
            w[axis + "dirsetup"].set_sensitive(1)

    def drivertype_toindex(self, axis, what=None):
        if what is None: what = self.d[axis + "drivertype"]
        for i, d in enumerate(_PD.alldrivertypes):
            if d[0] == what: return i
        return len(_PD.alldrivertypes)

    def drivertype_toid(self, axis, what=None):
        if not isinstance(what, int): what = self.drivertype_toindex(axis, what)
        if what < len(_PD.alldrivertypes): return _PD.alldrivertypes[what][0]
        return "custom"

    def drivertype_fromindex(self, axis):
        i = self.widgets[axis + "drivertype"].get_active()
        if i < len(_PD.alldrivertypes): return _PD.alldrivertypes[i][1]
        return _("Custom")

    def comp_toggle(self, axis):
        i = self.widgets[axis + "usecomp"].get_active()   
        self.widgets[axis + "compfilename"].set_sensitive(i)
        self.widgets[axis + "comptype"].set_sensitive(i)
        if i:
            self.widgets[axis + "backlash"].set_sensitive(0)
            self.widgets[axis + "usebacklash"].set_active(0)

    def bldc_toggled(self, axis):
        i = self.widgets[axis + "bldc_option"].get_active()
        self.widgets[axis + "bldcoptionbox"].set_sensitive(i)

    def useatspeed_toggled(self):
        i = self.widgets.suseatspeed.get_active()
        self.widgets.snearscale.set_sensitive(self.widgets.snearscale_button.get_active() and i)
        self.widgets.snearrange.set_sensitive(self.widgets.snearrange_button.get_active() and i)

    def useoutputrange2_toggled(self):
        i = self.widgets.suseoutputrange2.get_active()
        self.widgets.soutputscale2.set_sensitive(i)

    def bldc_update(self,Widgets,axis):
        w = self.widgets
        i = False
        if w[axis+"bldc_incremental_feedback"].get_active():
            i = True
        w[axis+"bldc_pattern_in"].set_sensitive(i and  w[axis+"bldc_use_hall"].get_active() )
        w[axis+"bldc_inital_value"].set_sensitive(i and w[axis+"bldc_use_encoder"].get_active() and not w[axis+"bldc_use_hall"].get_active() )
        w[axis+"bldc_use_hall"].set_sensitive(i)
        w[axis+"bldc_use_encoder"].set_sensitive(i)
        w[axis+"bldc_use_index"].set_sensitive(i)
        w[axis+"bldc_fanuc_alignment"].set_sensitive(i)
        i = False
        if w[axis+"bldc_emulated_feedback"].get_active():
            i = True
        w[axis+"bldc_output_hall"].set_sensitive(i)
        w[axis+"bldc_output_fanuc"].set_sensitive(i)
        w[axis+"bldc_pattern_out"].set_sensitive(i and  w[axis+"bldc_output_hall"].get_active() )

    def backlash_toggle(self, axis):
        i = self.widgets[axis + "usebacklash"].get_active()   
        self.widgets[axis + "backlash"].set_sensitive(i)
        if i:
            self.widgets[axis + "compfilename"].set_sensitive(0)
            self.widgets[axis + "comptype"].set_sensitive(0)
            self.widgets[axis + "usecomp"].set_active(0)

    def axis_done(self, axis):
        d = self.d
        w = self.widgets
        def get_text(n): d[axis + n] = get_value(w[axis + n])
        def get_pagevalue(n): d[axis + n] = get_value(w[axis + n])
        def get_active(n): d[axis + n] = w[axis + n].get_active()
        stepdrive = self.d.findsignal(axis+"-stepgen-step")
        encoder = self.d.findsignal(axis+"-encoder-a")
        resolver = self.d.findsignal(axis+"-resolver")
        get_pagevalue("P")
        get_pagevalue("I")
        get_pagevalue("D")
        get_pagevalue("FF0")
        get_pagevalue("FF1")
        get_pagevalue("FF2")
        get_pagevalue("bias")
        get_pagevalue("deadband")
        if stepdrive:
            d[axis + "maxoutput"] = (get_value(w[axis + "maxvel"])/60) *1.25 # TODO should be X2 if using backlash comp ?
        if axis == "s":
            d[axis + "maxoutput"] = (get_value(w[axis +"outputscale"]))
        else:
            get_pagevalue("maxoutput")
        get_pagevalue("steptime")
        get_pagevalue("stepspace")
        get_pagevalue("dirhold")
        get_pagevalue("dirsetup")
        get_pagevalue("outputscale")
        get_pagevalue("3pwmscale")
        get_pagevalue("3pwmdeadtime")
        get_active("bldc_option")
        get_active("bldc_reverse")
        get_pagevalue("bldc_scale")
        get_pagevalue("bldc_poles")
        get_pagevalue("bldc_encoder_offset")
        get_pagevalue("bldc_drive_offset")
        get_pagevalue("bldc_pattern_out")
        get_pagevalue("bldc_pattern_in")
        get_pagevalue("bldc_lead_angle")
        get_pagevalue("bldc_inital_value")
        get_pagevalue("8i20maxcurrent")
        get_active("bldc_no_feedback")
        get_active("bldc_absolute_feedback")
        get_active("bldc_incremental_feedback")
        get_active("bldc_use_hall")
        get_active("bldc_use_encoder" )
        get_active("bldc_use_index")
        get_active("bldc_fanuc_alignment")
        get_active("bldc_digital_output")
        get_active("bldc_six_outputs")
        get_active("bldc_emulated_feedback")
        get_active("bldc_output_hall")
        get_active("bldc_output_fanuc")
        get_active("bldc_force_trapz")
        if w[axis + "bldc_option"].get_active():
            self.configure_bldc(axis)
        d[axis + "encodercounts"] = int(float(w["encoderline"].get_text())*4)
        if stepdrive: get_pagevalue("stepscale")
        if encoder: get_pagevalue("encoderscale")
        if resolver: get_pagevalue("encoderscale")
        get_active("invertmotor")
        get_active("invertencoder")
        d[axis + "maxvel"] = (get_value(w[axis + "maxvel"])/60)
        get_pagevalue("maxacc")
        d[axis + "drivertype"] = self.drivertype_toid(axis, w[axis + "drivertype"].get_active())
        if not axis == "s":
            get_pagevalue("outputminlimit")
            get_pagevalue("outputmaxlimit")
            get_pagevalue("maxferror")
            get_pagevalue("minferror")
            get_text("homepos")
            get_text("minlim")
            get_text("maxlim")
            get_text("homesw")
            d[axis + "homesearchvel"] = (get_value(w[axis + "homesearchvel"])/60)
            d[axis + "homelatchvel"] = (get_value(w[axis + "homelatchvel"])/60)
            d[axis + "homefinalvel"] = (get_value(w[axis + "homefinalvel"])/60)
            d[axis+"homesequence"] = (get_value(w[axis+"homesequence"]))
            get_active("searchdir")
            get_active("latchdir")
            get_active("usehomeindex")
            d[axis + "compfilename"] = w[axis + "compfilename"].get_text()
            get_active("comptype")
            d[axis + "backlash"]= w[axis + "backlash"].get_value()
            get_active("usecomp")
            get_active("usebacklash")
        else:
            get_active("useatspeed")
            d.susenearrange = w.snearrange_button.get_active()
            get_pagevalue("nearscale")
            d["snearscale"] = w["snearscale"].get_value()/100
            d["snearrange"] = w["snearrange"].get_value()
            get_pagevalue("filtergain")
            get_active("singleinputencoder")
            get_pagevalue("outputscale2")
            self.d.gsincrvalue0 = self.d.soutputscale
            self.d.gsincrvalue1 = self.d.soutputscale2
            get_active("useoutputrange2")
            self.d.scaleselect = self.d.suseoutputrange2
            get_active("usenegativevoltage")
            get_pagevalue("outputmaxvoltage")

    def configure_bldc(self,axis):
        d = self.d
        string = ""
        # Inputs
        if d[axis + "bldc_no_feedback"]: string = string + "n"
        elif d[axis +"bldc_absolute_feedback"]: string = string + "a"
        elif d[axis + "bldc_incremental_feedback"]:
            if d[axis + "bldc_use_hall"]: string = string + "h"
            if d[axis + "bldc_use_encoder" ]: string = string + "q"
        if d[axis + "bldc_use_index"]: string = string + "i"
        if d[axis + "bldc_fanuc_alignment"]: string = string + "f"
        # Outputs
        if d[axis + "bldc_digital_output"]: string = string + "B"
        if d[axis + "bldc_six_outputs"]: string = string + "6"
        if d[axis + "bldc_emulated_feedback"]:
            if d[axis + "bldc_output_hall"]: string = string + "H"
            if d[axis + "bldc_output_fanuc"]: string = string +"F"
        if d[axis + "bldc_force_trapz"]: string = string + "T"
        #print "axis ",axis,"bldc config ",string 
        d[axis+"bldc_config"] = string

    def calculate_spindle_scale(self):
        def get(n): return get_value(self.widgets[n])
        stepdrive = bool(self.d.findsignal("s-stepgen-step"))
        encoder = bool(self.d.findsignal("s-encoder-a"))
        resolver = bool(self.d.findsignal("s-resolver"))
        twoscales = self.widgets.suseoutputrange2.get_active()

        data_list=[ "steprev","microstep","motor_pulleydriver","motor_pulleydriven","motor_gear1driver","motor_gear1driven",
                        "motor_gear2driver","motor_gear2driven","motor_max"]
        templist1 = ["encoderline","steprev","microstep","motor_gear1driven","motor_gear1driver","motor_gear2driven","motor_gear2driver",
                        "motor_pulleydriven","motor_pulleydriver","motor_max"]
        checkbutton_list = ["cbmicrosteps","cbmotor_gear1","cbmotor_gear2","cbmotor_pulley","rbvoltage_5"
                    ]

        self.widgets.spindle_cbmicrosteps.set_sensitive(stepdrive)
        self.widgets.spindle_microstep.set_sensitive(stepdrive)
        self.widgets.spindle_steprev.set_sensitive(stepdrive)
        self.widgets.label_steps_per_rev.set_sensitive(stepdrive)
        self.widgets.spindle_motor_max.set_sensitive(not stepdrive)
        self.widgets.label_motor_at_max_volt.set_sensitive(not stepdrive)
        self.widgets.label_volt_at_max_rpm.set_sensitive(not stepdrive)
        self.widgets.spindle_rbvoltage_10.set_sensitive(not stepdrive)
        self.widgets.spindle_rbvoltage_5.set_sensitive(not stepdrive)
        self.widgets.spindle_cbnegative_rot.set_sensitive(not stepdrive)

        # pre set data
        for i in data_list:
            self.widgets['spindle_'+i].set_value(self.d['s'+i])
        for i in checkbutton_list:
            self.widgets['spindle_'+i].set_active(self.d['s'+i])
        self.widgets.spindle_encoderline.set_value(self.widgets.sencoderscale.get_value()/4)
        self.widgets.spindle_cbmotor_gear2.set_active(twoscales)
        self.widgets.spindle_cbnegative_rot.set_active(self.widgets.susenegativevoltage.get_active())

        # temparally add signals
        for i in templist1:
            self.d[i] = self.widgets['spindle_'+i].connect("value-changed", self.update_spindle_calculation)
        for i in checkbutton_list:
            self.d[i] = self.widgets['spindle_'+i].connect("toggled", self.update_spindle_calculation)
        self.update_spindle_calculation(None)
        # run dialog
        self.widgets.spindle_scaledialog.set_title(_("Spindle Scale Calculation"))
        self.widgets.spindle_scaledialog.show_all()
        result = self.widgets.spindle_scaledialog.run()
        self.widgets.spindle_scaledialog.hide()

        # remove signals
        for i in templist1:
            self.widgets['spindle_'+i].disconnect(self.d[i])
        for i in checkbutton_list:
            self.widgets['spindle_'+i].disconnect(self.d[i])

        if not result: return

        # record data values
        for i in data_list:
            self.d['s'+i] = get('spindle_'+i)
        for i in checkbutton_list:
            self.d['s'+i] = self.widgets['spindle_'+i].get_active()
        # set the widgets on the spindle page as per calculations 
        self.widgets.susenegativevoltage.set_active(self.widgets.spindle_cbnegative_rot.get_active())
        if self.widgets.spindle_rbvoltage_5.get_active():
            self.widgets.soutputmaxvoltage.set_value(5)
        else:
            self.widgets.soutputmaxvoltage.set_value(10)
        self.widgets.soutputscale.set_value(self.temp_max_motor_speed1)
        self.widgets.soutputscale2.set_value(self.temp_max_motor_speed2)
        self.widgets.smaxoutput.set_value(self.temp_max_motor_speed1)
        self.widgets.sencoderscale.set_value(self.widgets.spindle_encoderline.get_value()*4)
        self.widgets.suseoutputrange2.set_active(self.widgets.spindle_cbmotor_gear2.get_active())
        if stepdrive:
            motor_steps = get_value(self.widgets.spindle_steprev)
            if self.widgets.spindle_cbmicrosteps.get_active():
                microstepfactor = get_value(self.widgets.spindle_microstep)
            else:
                microstepfactor = 1
            self.widgets.sstepscale.set_value(motor_steps * microstepfactor)
        if encoder or resolver:
            self.widgets.sencoderscale.set_value(get("spindle_encoderline")*4)

    def update_spindle_calculation(self,widget):
        w= self.widgets
        def get(n): return get_value(w[n])
        motor_pulley_ratio = gear1_ratio = gear2_ratio = 1
        motor_rpm = get("spindle_motor_max")
        volts_at_max_rpm = 5
        if self.widgets.spindle_rbvoltage_10.get_active():
            volts_at_max_rpm = 10
        if w["spindle_cbmotor_pulley"].get_active():
            w["spindle_motor_pulleydriver"].set_sensitive(True)
            w["spindle_motor_pulleydriven"].set_sensitive(True)
            motor_pulley_ratio = (get("spindle_motor_pulleydriver") / get("spindle_motor_pulleydriven"))
        else:
            w["spindle_motor_pulleydriver"].set_sensitive(False)
            w["spindle_motor_pulleydriven"].set_sensitive(False)
            motor_pulley_ratio = 1
        if w["spindle_cbmotor_gear1"].get_active():
            w["spindle_motor_gear1driver"].set_sensitive(True)
            w["spindle_motor_gear1driven"].set_sensitive(True)
            gear1_ratio = (get("spindle_motor_gear1driver") / get("spindle_motor_gear1driven"))
        else:
            w["spindle_motor_gear1driver"].set_sensitive(False)
            w["spindle_motor_gear1driven"].set_sensitive(False)
            gear1_ratio = 1
        i = w["spindle_cbmotor_gear2"].get_active()
        w["spindle_motor_gear2driver"].set_sensitive(i)
        w["spindle_motor_gear2driven"].set_sensitive(i)
        w["label_rpm_at_max_motor2"].set_sensitive(i)
        w["label_gear2_max_speed"].set_sensitive(i)
        if i:
            gear2_ratio = (get("spindle_motor_gear2driver") / get("spindle_motor_gear2driven"))
        else:
            gear2_ratio = 1
        w["spindle_microstep"].set_sensitive(w["spindle_cbmicrosteps"].get_active())
        self.temp_max_motor_speed1 = (motor_pulley_ratio * gear1_ratio * motor_rpm)
        self.temp_max_motor_speed2 = (motor_pulley_ratio * gear2_ratio * motor_rpm)
        w["label_motor_at_max_volt"].set_markup("      <b>MOTOR</b> RPM at %d Volt Command"% volts_at_max_rpm)
        w["label_volt_at_max_rpm"].set_text("      Voltage for %d Motor RPM:"% motor_rpm)
        w["label_rpm_at_max_motor1"].set_text("Spindle RPM at %d Motor RPM -gear 1:"% motor_rpm)
        w["label_rpm_at_max_motor2"].set_text("Spindle RPM at %d Motor RPM -gear 2:"% motor_rpm)
        w["label_gear1_max_speed"].set_text("%d" % (motor_pulley_ratio * gear1_ratio * motor_rpm))
        w["label_gear2_max_speed"].set_text("%d" % (motor_pulley_ratio * gear2_ratio * motor_rpm))

    def calculate_scale(self,axis):
        def get(n): return get_value(self.widgets[n])
        stepdrive = self.d.findsignal(axis+"-stepgen-step")
        encoder = self.d.findsignal(axis+"-encoder-a")
        resolver = self.d.findsignal(axis+"-resolver")
        data_list=[ "steprev","microstep","motor_pulleydriver","motor_pulleydriven","motor_wormdriver","motor_wormdriven",
                    "encoder_pulleydriver","encoder_pulleydriven","encoder_wormdriver","encoder_wormdriven","motor_leadscrew",
                    "encoder_leadscrew","motor_leadscrew_tpi","encoder_leadscrew_tpi",
                    ]
        templist1 = ["encoderline","encoder_leadscrew","encoder_leadscrew_tpi","encoder_wormdriven",
                    "encoder_wormdriver","encoder_pulleydriven","encoder_pulleydriver","steprev","motor_leadscrew","motor_leadscrew_tpi",
                    "microstep","motor_wormdriven","motor_wormdriver","motor_pulleydriven","motor_pulleydriver"
                    ]
        checkbutton_list = [ "cbencoder_pitch","cbencoder_tpi","cbencoder_worm","cbencoder_pulley","cbmotor_pitch",
                        "cbmotor_tpi","cbmicrosteps","cbmotor_worm","cbmotor_pulley"
                    ]
        # pre set data
        for i in data_list:
            self.widgets[i].set_value(self.d[axis+i])
        for i in checkbutton_list:
            self.widgets[i].set_active(self.d[axis+i])

        # temparally add signals
        for i in templist1:
            self.d[i] = self.widgets[i].connect("value-changed", self.update_scale_calculation,axis)
        for i in checkbutton_list:
            self.d[i] = self.widgets[i].connect("toggled", self.update_scale_calculation,axis)
        # pre calculate
        self.update_scale_calculation(self.widgets,axis)
        # run dialog
        self.widgets.scaledialog.set_title(_("Axis Scale Calculation"))
        self.widgets.scaledialog.show_all()
        result = self.widgets.scaledialog.run()
        self.widgets.scaledialog.hide()
        # remove signals
        for i in templist1:
            self.widgets[i].disconnect(self.d[i])
        for i in checkbutton_list:
            self.widgets[i].disconnect(self.d[i])
        if not result: return
        # record data values
        for i in data_list:
            self.d[axis+i] = self.widgets[i].get_value()
        for i in checkbutton_list:
            self.d[axis+i] = self.widgets[i].get_active()
        # set the calculations result
        if encoder or resolver:
            self.widgets[axis+"encoderscale"].set_value(get("calcencoder_scale"))
        if stepdrive:
            self.widgets[axis+"stepscale"].set_value(get("calcmotor_scale"))
    def update_scale_calculation(self,widget,axis):
        w = self.widgets
        d = self.d
        def get(n): return get_value(w[n])
        stepdrive = self.d.findsignal(axis+"-stepgen-step")
        encoder = self.d.findsignal(axis+"-encoder-a")
        resolver = self.d.findsignal(axis+"-resolver")
        motor_pulley_ratio = encoder_pulley_ratio = 1
        motor_worm_ratio = encoder_worm_ratio = 1
        encoder_scale = motor_scale = 0
        microstepfactor = motor_pitch = encoder_pitch = motor_steps = 1
        if axis == "a": rotary_scale = 360
        else: rotary_scale = 1 
        try:
            if stepdrive:
                # stepmotor scale
                w["calcmotor_scale"].set_sensitive(True)
                w["stepscaleframe"].set_sensitive(True)
                if w["cbmotor_pulley"].get_active():
                    w["motor_pulleydriver"].set_sensitive(True)
                    w["motor_pulleydriven"].set_sensitive(True)
                    motor_pulley_ratio = (get("motor_pulleydriven") / get("motor_pulleydriver"))
                else:
                     w["motor_pulleydriver"].set_sensitive(False)
                     w["motor_pulleydriven"].set_sensitive(False)
                if w["cbmotor_worm"].get_active():
                    w["motor_wormdriver"].set_sensitive(True)
                    w["motor_wormdriven"].set_sensitive(True)
                    motor_worm_ratio = (get("motor_wormdriver") / get("motor_wormdriven"))
                else:
                    w["motor_wormdriver"].set_sensitive(False)
                    w["motor_wormdriven"].set_sensitive(False)
                if w["cbmicrosteps"].get_active():
                    w["microstep"].set_sensitive(True)
                    microstepfactor = get("microstep")
                else:
                    w["microstep"].set_sensitive(False)

                if w["cbmotor_pitch"].get_active():
                    w["motor_leadscrew"].set_sensitive(True)
                    w["cbmotor_tpi"].set_active(False)
                    if self.d.units == _PD._METRIC:
                        motor_pitch = 1./ get("motor_leadscrew")
                    else:
                        motor_pitch = 1./ (get("motor_leadscrew")* .03937008)
                else: w["motor_leadscrew"].set_sensitive(False)

                if w["cbmotor_tpi"].get_active():
                    w["motor_leadscrew_tpi"].set_sensitive(True)
                    w["cbmotor_pitch"].set_active(False)
                    if self.d.units == _PD._METRIC:
                        motor_pitch = (get("motor_leadscrew_tpi")* .03937008)
                    else:
                        motor_pitch = get("motor_leadscrew_tpi")
                else: w["motor_leadscrew_tpi"].set_sensitive(False)

                motor_steps = get("steprev")
                motor_scale = (motor_steps * microstepfactor * motor_pulley_ratio * motor_worm_ratio * motor_pitch) / rotary_scale
                w["calcmotor_scale"].set_text(locale.format("%.4f", (motor_scale)))
            else:
                w["calcmotor_scale"].set_sensitive(False)
                w["stepscaleframe"].set_sensitive(False)
            # encoder scale
            if encoder or resolver:
                w["calcencoder_scale"].set_sensitive(True)
                w["encoderscaleframe"].set_sensitive(True)
                if w["cbencoder_pulley"].get_active():
                    w["encoder_pulleydriver"].set_sensitive(True)
                    w["encoder_pulleydriven"].set_sensitive(True)
                    encoder_pulley_ratio = (get("encoder_pulleydriven") / get("encoder_pulleydriver"))
                else:
                     w["encoder_pulleydriver"].set_sensitive(False)
                     w["encoder_pulleydriven"].set_sensitive(False)
                if w["cbencoder_worm"].get_active():
                    w["encoder_wormdriver"].set_sensitive(True)
                    w["encoder_wormdriven"].set_sensitive(True)
                    encoder_worm_ratio = (get("encoder_wormdriver") / get("encoder_wormdriven"))
                else:
                    w["encoder_wormdriver"].set_sensitive(False)
                    w["encoder_wormdriven"].set_sensitive(False)
                if w["cbencoder_pitch"].get_active():
                    w["encoder_leadscrew"].set_sensitive(True)
                    w["cbencoder_tpi"].set_active(False)
                    if self.d.units == _PD._METRIC:
                        encoder_pitch = 1./ get("encoder_leadscrew")
                    else:
                        encoder_pitch = 1./ (get("encoder_leadscrew")*.03937008)
                else: w["encoder_leadscrew"].set_sensitive(False)
                if w["cbencoder_tpi"].get_active():
                    w["encoder_leadscrew_tpi"].set_sensitive(True)
                    w["cbencoder_pitch"].set_active(False)
                    if self.d.units == _PD._METRIC:
                        encoder_pitch = (get("encoder_leadscrew_tpi")*.03937008)
                    else:
                        encoder_pitch = get("encoder_leadscrew_tpi")
                else: w["encoder_leadscrew_tpi"].set_sensitive(False)

                encoder_cpr = get_value(w[("encoderline")]) * 4
                encoder_scale = (encoder_pulley_ratio * encoder_worm_ratio * encoder_pitch * encoder_cpr) / rotary_scale
                w["calcencoder_scale"].set_text(locale.format("%.4f", (encoder_scale)))
            else:
                w["calcencoder_scale"].set_sensitive(False)
                w["encoderscaleframe"].set_sensitive(False)
            #new stuff
            if stepdrive: scale = motor_scale
            else: scale = encoder_scale
            maxvps = (get_value(w[axis+"maxvel"]))/60
            pps = (scale * (maxvps))/1000
            if pps == 0: raise ValueError
            pps = abs(pps)
            w["khz"].set_text("%.1f" % pps)
            acctime = (maxvps) / get_value(w[axis+"maxacc"])
            accdist = acctime * .5 * (maxvps)
            if encoder or resolver:
                maxrpm = int(maxvps * 60 * (scale/encoder_cpr))
            else:
                maxrpm = int(maxvps * 60 * (scale/(microstepfactor * motor_steps)))
            w["acctime"].set_text("%.4f" % acctime)
            w["accdist"].set_text("%.4f" % accdist)
            w["chartresolution"].set_text("%.7f" % (1.0 / scale))
            w["calscale"].set_text(str(scale))
            w["maxrpm"].set_text("%d" % maxrpm)

        except (ValueError, ZeroDivisionError):
            w["calcmotor_scale"].set_text("200")
            w["calcencoder_scale"].set_text("1000")
            w["chartresolution"].set_text("")
            w["acctime"].set_text("")
            if not axis == 's':
                w["accdist"].set_text("")
            w["khz"].set_text("")
            w["calscale"].set_text("")

    def motor_encoder_sanity_check(self,widgets,axis):
        stepdrive = encoder = bad = resolver = pot = False
        if self.d.findsignal(axis+"-stepgen-step"): stepdrive = True
        if self.d.findsignal(axis+"-encoder-a"): encoder = True
        if self.d.findsignal(axis+"-resolver"): resolver = True
        if self.d.findsignal(axis+"-pot-outpot"): pot = True
        if encoder or resolver:
            if self.widgets[axis+"encoderscale"].get_value() < 1:
                self.widgets[axis+"encoderscale"].modify_bg(gtk.STATE_NORMAL, self.widgets[axis+"encoderscale"].get_colormap().alloc_color("red"))
                dbg('encoder resolver scale bad %f'%self.widgets[axis+"encoderscale"].get_value())
                bad = True
        if stepdrive:
            if self.widgets[axis+"stepscale"].get_value() < 1:
                self.widgets[axis+"stepscale"].modify_bg(gtk.STATE_NORMAL, self.widgets[axis+"stepscale"].get_colormap().alloc_color("red"))
                dbg('step scale bad')
                bad = True
        if not (encoder or resolver) and not stepdrive and not axis == "s":
            dbg('encoder %s resolver %s stepper %s axis %s'%(encoder,resolver,stepdrive,axis))
            bad = True
        if self.widgets[axis+"maxvel"] < 1:
            dbg('max vel low')
            bad = True
        if self.widgets[axis+"maxacc"] < 1:
            dbg('max accl low')
            bad = True
        if bad:
            dbg('motor %s_encoder sanity check -bad'%axis)
            self.p.set_buttons_sensitive(1,0)
            self.widgets[axis + "axistune"].set_sensitive(0)
            self.widgets[axis + "axistest"].set_sensitive(0)
        else:
            dbg('motor %s_encoder sanity check - good'%axis)
            self.widgets[axis+"encoderscale"].modify_bg(gtk.STATE_NORMAL, self.origbg)
            self.widgets[axis+"stepscale"].modify_bg(gtk.STATE_NORMAL, self.origbg)
            self.p.set_buttons_sensitive(1,1)
            self.widgets[axis + "axistune"].set_sensitive(1)
            self.widgets[axis + "axistest"].set_sensitive(1)

    def update_gladevcp(self):
        i = self.widgets.gladevcp.get_active()
        self.widgets.gladevcpbox.set_sensitive( i )
        if self.d.frontend == _PD._TOUCHY:
            self.widgets.centerembededgvcp.set_active(True)
            self.widgets.centerembededgvcp.set_sensitive(True)
            self.widgets.sideembededgvcp.set_sensitive(False)
            self.widgets.standalonegvcp.set_sensitive(False)
        elif self.d.frontend == _PD._GMOCCAPY or self.d.frontend == _PD._AXIS:
            self.widgets.sideembededgvcp.set_sensitive(True)
            self.widgets.centerembededgvcp.set_sensitive(True)
            self.widgets.standalonegvcp.set_sensitive(False)
            if not self.widgets.centerembededgvcp.get_active() and not self.widgets.sideembededgvcp.get_active():
                self.widgets.centerembededgvcp.set_active(True)
        else:
            self.widgets.sideembededgvcp.set_sensitive(False)
            self.widgets.centerembededgvcp.set_sensitive(False)
            self.widgets.standalonegvcp.set_sensitive(True)
            self.widgets.standalonegvcp.set_active(True)

        i = self.widgets.standalonegvcp.get_active()
        self.widgets.gladevcpsize.set_sensitive(i)
        self.widgets.gladevcpposition.set_sensitive(i)
        self.widgets.gladevcpforcemax.set_sensitive(i)
        if not i:
            self.widgets.gladevcpsize.set_active(False)
            self.widgets.gladevcpposition.set_active(False)
            self.widgets.gladevcpforcemax.set_active(False)
        i = self.widgets.gladevcpsize.get_active()
        self.widgets.gladevcpwidth.set_sensitive(i)
        self.widgets.gladevcpheight.set_sensitive(i)
        i = self.widgets.gladevcpposition.get_active()
        self.widgets.gladevcpxpos.set_sensitive(i)
        self.widgets.gladevcpypos.set_sensitive(i)
        for i in (("zerox","x"),("zeroy","y"),("zeroz","z"),("zeroa","a"),("autotouchz","z")):
            if not i[1] in(self.d.available_axes):
                self.widgets[i[0]].set_active(False)
                self.widgets[i[0]].set_sensitive(False)
            else:
                self.widgets[i[0]].set_sensitive(True)

    def has_spindle_speed_control(self):
        for test in ("s-stepgen-step", "s-pwm-pulse", "s-encoder-a", "spindle-on", "spindle-cw", "spindle-ccw", "spindle-brake",
                    "s-pot-output"):
            has_spindle = self.d.findsignal(test)
            print test,has_spindle
            if has_spindle:
                return True
        if self.d.serial_vfd and (self.d.mitsub_vfd or self.d.gs2_vfd):
            return True
        return False
   
    def clean_unused_ports(self, *args):
        # if parallel ports not used clear all signals
        parportnames = ("pp1","pp2","pp3")
        for check,connector in enumerate(parportnames):
            if self.d.number_pports >= (check+1):continue
            # initialize parport input / inv pins
            for i in (1,2,3,4,5,6,7,8,10,11,12,13,15):
                pinname ="%s_Ipin%d"% (connector,i)
                self.d[pinname] = _PD.UNUSED_INPUT
                pinname ="%s_Ipin%d_inv"% (connector,i)
                self.d[pinname] = False
            # initialize parport output / inv pins
            for i in (1,2,3,4,5,6,7,8,9,14,16,17):
                pinname ="%s_Opin%d"% (connector,i)
                self.d[pinname] = _PD.UNUSED_OUTPUT
                pinname ="%s_Opin%d_inv"% (connector,i)
                self.d[pinname] = False        
        # clear all unused mesa signals
        for boardnum in(0,1):
            for connector in(1,2,3,4,5,6,7,8,9):
                if self.d.number_mesa >= boardnum + 1 :
                    if connector in(self.d["mesa%d_currentfirmwaredata"% (boardnum)][_PD._NUMOFCNCTRS]) :
                        continue
                # This initializes GPIO input pins
                for i in range(0,16):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self.d[pinname] = _PD.UNUSED_INPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self.d[pinname] = _PD.GPIOI
                # This initializes GPIO output pins
                for i in range(16,24):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self.d[pinname] = _PD.UNUSED_OUTPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self.d[pinname] = _PD.GPIOO
                # This initializes the mesa inverse pins
                for i in range(0,24):
                    pinname ="mesa%dc%dpin%dinv"% (boardnum,connector,i)
                    self.d[pinname] = False
            # clear unused sserial signals
            keeplist =[]
            # if the current firmware supports sserial better check for used channels
            # and make a 'keeplist'. we don't want to clear them
            if self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._MAXSSERIALPORTS]:
                #search all pins for sserial port
                for concount,connector in enumerate(self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._NUMOFCNCTRS]) :
                    for pin in range (0,24):
                        firmptype,compnum = self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._STARTOFDATA+pin+(concount*24)]       
                        p = 'mesa%dc%dpin%d' % (boardnum, connector, pin)
                        ptype = 'mesa%dc%dpin%dtype' % (boardnum, connector , pin)
                        if self.d[ptype] in (_PD.TXDATA0,_PD.TXDATA1,_PD.TXDATA2,_PD.TXDATA3,_PD.TXDATA4,_PD.SS7I76M0,_PD.SS7I76M2,_PD.SS7I76M3,
                                                _PD.SS7I77M0,_PD.SS7I77M1,_PD.SS7I77M3,_PD.SS7I77M4) and not self.d[p] == _PD.UNUSED_SSERIAL:
                            if self.d[ptype] in (_PD.TXDATA0,_PD.SS7I76M0,_PD.SS7I77M0): channelnum = 0
                            elif self.d[ptype] in (_PD.TXDATA1,_PD.SS7I77M1): channelnum = 1
                            elif self.d[ptype] == _PD.TXDATA2: channelnum = 2
                            elif self.d[ptype] in (_PD.TXDATA3,_PD.SS7I76M3,_PD.SS7I77M3): channelnum = 3
                            elif self.d[ptype] in (_PD.TXDATA4,_PD.SS7I77M4): channelnum = 4
                            keeplist.append(channelnum)
            #print "board # %d sserial keeplist"%(boardnum),keeplist
            # ok clear the sserial pins unless they are in the keeplist
            port = 0# TODO hard code at only 1 sserial port 
            for channel in range(0,_PD._NUM_CHANNELS): #TODO hardcoded at 5 sserial channels instead of 8
                if channel in keeplist: continue
                # This initializes pins
                for i in range(0,self._p._SSCOMBOLEN):
                    pinname ="mesa%dsserial%d_%dpin%d"% (boardnum, port,channel,i)
                    if i < 24:
                        self.d[pinname] = _PD.UNUSED_INPUT
                    else:
                        self.d[pinname] = _PD.UNUSED_OUTPUT
                    pinname ="mesa%dsserial%d_%dpin%dtype"% (boardnum, port,channel,i)
                    if i < 24:
                        self.d[pinname] = _PD.GPIOI
                    else:
                        self.d[pinname] = _PD.GPIOO
                    pinname ="mesa%dsserial%d_%dpin%dinv"% (boardnum, port,channel,i)
                    self.d[pinname] = False

    def debug_iter(self,test,testwidget,message=None):
        print "#### DEBUG :",message
        for i in ("_gpioosignaltree","_gpioisignaltree","_steppersignaltree","_encodersignaltree","_muxencodersignaltree",
                    "_pwmcontrolsignaltree","_pwmrelatedsignaltree","_tppwmsignaltree",
                    "_gpioliststore","_encoderliststore","_muxencoderliststore","_pwmliststore","_tppwmliststore"):
            modelcheck = self.widgets[testwidget].get_model()
            if modelcheck == self.d[i]:print i;break

#********************
# Common Helper functions
#********************

    def tandem_check(self, letter):
        tandem_stepper = self.d.make_pinname(self.stepgen_sig("%s2"%letter))
        tandem_pwm = self.d.make_pinname(self.pwmgen_sig("%s2"%letter))
        print letter, bool(tandem_stepper or tandem_pwm), tandem_stepper, tandem_pwm
        return bool(tandem_stepper or tandem_pwm)

    def stepgen_sig(self, axis):
           thisaxisstepgen =  axis + "-stepgen-step" 
           test = self.d.findsignal(thisaxisstepgen)
           return test

    def stepgen_invert_pins(self,pinnumber):
        # sample pinname = mesa0c0pin11
        signallist = []
        pin = int(pinnumber[10:])
        connector = int(pinnumber[6:7])
        boardnum = int(pinnumber[4:5])
        channel = None
        pinlist = self.d.list_related_pins([_PD.STEPA,_PD.STEPB], boardnum, connector, channel, pin, 0)
        #print pinlist
        for i in pinlist:
            if self.d[i[0]+"inv"]:
                gpioname = self.d.make_pinname(self.d.findsignal( self.d[i[0]] ),True)
                #print gpioname
                signallist.append(gpioname)
        return signallist

    def spindle_invert_pins(self,pinnumber):
        # sample pinname = mesa0sserial0_0pin11
        signallist = []
        pin = int(pinnumber[18:])
        port = int(pinnumber[12:13])
        boardnum = int(pinnumber[4:5])
        channel = int(pinnumber[14:15])
        pinlist = self.d.list_related_pins([_PD.POTO,_PD.POTE], boardnum, port, channel, pin, 0)
        for i in pinlist:
            if self.d[i[0]+"inv"]:
                name = self.d[i[0]+"type"]
                signallist.append(name)
        return signallist

    def encoder_sig(self, axis): 
           thisaxisencoder = axis +"-encoder-a"
           test = self.d.findsignal(thisaxisencoder)
           return test

    def resolver_sig(self, axis):
        thisaxisresolver = axis +"-resolver"
        test = self.d.findsignal(thisaxisresolver)
        return test

    def amp_8i20_sig(self, axis):
        thisaxis8i20 = "%s-8i20"% axis
        test = self.d.findsignal(thisaxis8i20)
        return test

    def potoutput_sig(self,axis):
        thisaxispot = "%s-pot-output"% axis
        test = self.d.findsignal(thisaxispot)
        return test

    def pwmgen_sig(self, axis):
           thisaxispwmgen =  axis + "-pwm-pulse" 
           test = self.d.findsignal( thisaxispwmgen)
           return test

    def pwmgen_invert_pins(self,pinnumber):
        print "list pwm invert pins",pinnumber
        # sample pinname = mesa0c0pin11
        signallist = []
        pin = int(pinnumber[10:])
        connector = int(pinnumber[6:7])
        boardnum = int(pinnumber[4:5])
        channel = None
        pinlist = self.d.list_related_pins([_PD.PWMP, _PD.PWMD, _PD.PWME], boardnum, connector, channel, pin, 0)
        print pinlist
        for i in pinlist:
            if self.d[i[0]+"inv"]:
                gpioname = self.d.make_pinname(self.d.findsignal( self.d[i[0]] ),True)
                print gpioname
                signallist.append(gpioname)
        return signallist

    def tppwmgen_sig(self, axis):
           thisaxispwmgen =  axis + "-tppwm-a" 
           test = self.d.findsignal(thisaxispwmgen)
           return test

    def tppwmgen_has_6(self, axis):
           thisaxispwmgen =  axis + "-tppwm-anot" 
           test = self.d.findsignal(thisaxispwmgen)
           return test

    def home_sig(self, axis):
        thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis, "max-home-" + axis, "both-home-" + axis))
        for i in thisaxishome:
            if self.d.findsignal(i): return i
        return None

    def min_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "min-" + axis,"min-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               if self.d.findsignal(i): return i
           return None

    def max_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "max-" + axis, "max-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               if self.d.findsignal(i): return i
           return None

    def get_value(self,w):
        return get_value(w)

    def show_try_errors(self):
            exc_type, exc_value, exc_traceback = sys.exc_info()
            formatted_lines = traceback.format_exc().splitlines()
            print
            print "****Pncconf verbose debugging:",formatted_lines[0]
            traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
            print formatted_lines[-1]

    def hostmot2_command_string(self):
            # mesa stuff
            load_cmnds = []
            board0 = self.d.mesa0_currentfirmwaredata[_PD._BOARDNAME]
            board1 = self.d.mesa1_currentfirmwaredata[_PD._BOARDNAME]
            driver0 = ' %s'% self.d.mesa0_currentfirmwaredata[_PD._HALDRIVER]
            driver1 = ' %s'% self.d.mesa1_currentfirmwaredata[_PD._HALDRIVER]
            directory0 = self.d.mesa0_currentfirmwaredata[_PD._DIRECTORY]
            directory1 = self.d.mesa1_currentfirmwaredata[_PD._DIRECTORY]
            firm0 = self.d.mesa0_currentfirmwaredata[_PD._FIRMWARE]
            firm1 = self.d.mesa1_currentfirmwaredata[_PD._FIRMWARE]
            firmstring0 = firmstring1 = board0_ip = board1_ip = ""
            mesa0_3pwm = mesa1_3pwm = ''
            mesa0_ioaddr = mesa1_ioaddr = ''
            load_cmnds.append("loadrt hostmot2")
            if '7i43' in board0:
                mesa0_ioaddr = ' ioaddr=%s ioaddr_hi=0 epp_wide=1'% self.d.mesa0_parportaddrs
            if '7i43' in board1:
                mesa1_ioaddr = ' ioaddr=%s ioaddr_hi=0 epp_wide=1'% self.d.mesa1_parportaddrs
            if 'eth' in driver0:
                firmstring0 =''
                if self.d.mesa0_card_addrs:
                    board0_ip = ''' board_ip="%s"''' % self.d.mesa0_card_addrs
            elif not "5i25" in board0:
                firmstring0 = "firmware=hm2/%s/%s.BIT" % (directory0, firm0)
            if 'eth' in driver1:
                firmstring1 =''
                if self.d.mesa1_card_addrs:
                    board1_ip = ''' board_ip="%s"'''% self.d.mesa1_card_addrs
            elif not "5i25" in board1:
                firmstring1 = "firmware=hm2/%s/%s.BIT" % (directory1, firm1)

            # TODO fix this hardcoded hack: only one serialport
            ssconfig0 = ssconfig1 = resolver0 = resolver1 = temp = ""
            if self.d.mesa0_numof_sserialports:
                for i in range(1,_PD._NUM_CHANNELS+1):
                    if i <= self.d.mesa0_numof_sserialchannels:
                        # m number in the name signifies the required sserial mode
                        for j in ("123456789"):
                            if ("m"+j) in self.d["mesa0sserial0_%dsubboard"% (i-1)]:
                                temp = temp + j
                                break
                        else: temp = temp + "0" # default case
                    else:
                        temp = temp + "x"
                ssconfig0 = " sserial_port_0=%s"% temp
            if self.d.number_mesa == 2 and self.d.mesa1_numof_sserialports:
                for i in range(1,_PD._NUM_CHANNELS+1):
                    if i <= self.d.mesa1_numof_sserialchannels:
                        # m number in the name signifies the required sserial mode
                        for j in ("123456789"):
                            if ("m"+j) in self.d["mesa1sserial0_%dsubboard"% (i-1)]:
                                temp = temp + j
                                break
                        else: temp = temp + "0" # default case
                    else:
                        temp = temp + "x"
                ssconfig1 = " sserial_port_0=%s"% temp
            if self.d.mesa0_numof_resolvers:
                resolver0 = " num_resolvers=%d"% self.d.mesa0_numof_resolvers
            if self.d.mesa1_numof_resolvers:
                resolver1 = " num_resolvers=%d"% self.d.mesa1_numof_resolvers
            if self.d.mesa0_numof_tppwmgens:
                mesa0_3pwm = ' num_3pwmgens=%d' %self.d.mesa0_numof_tppwmgens
            if self.d.mesa1_numof_tppwmgens:
                mesa1_3pwm = ' num_3pwmgens=%d' %self.d.mesa1_numof_tppwmgens

            if self.d.number_mesa == 1:            
                load_cmnds.append( """loadrt%s%s%s config="%s num_encoders=%d num_pwmgens=%d%s num_stepgens=%d%s%s" """ % (
                    driver0, board0_ip, mesa0_ioaddr, firmstring0, self.d.mesa0_numof_encodergens, self.d.mesa0_numof_pwmgens, 
                    mesa0_3pwm, self.d.mesa0_numof_stepgens, ssconfig0, resolver0))
            elif self.d.number_mesa == 2 and (driver0 == driver1):
                load_cmnds.append( """loadrt%s%s%s config="%s num_encoders=%d num_pwmgens=%d%s num_stepgens=%d%s%s,\
                                %s%s num_encoders=%d num_pwmgens=%d%s num_stepgens=%d%s%s" """ % (
                    driver0, board0_ip, mesa0_ioaddr, firmstring0, self.d.mesa0_numof_encodergens, self.d.mesa0_numof_pwmgens,
                     mesa0_3pwm, self.d.mesa0_numof_stepgens, ssconfig0, resolver0, mesa1_ioaddr, firmstring1,
                    self.d.mesa1_numof_encodergens, self.d.mesa1_numof_pwmgens, mesa1_3pwm,
                    self.d.mesa1_numof_stepgens, ssconfig1, resolver1))
            elif self.d.number_mesa == 2:
                load_cmnds.append( """loadrt%s%s%s config="%s num_encoders=%d num_pwmgens=%d%s num_stepgens=%d%s%s" """ % (
                    driver0, board0_ip, mesa0_ioaddr, firmstring0, self.d.mesa0_numof_encodergens, self.d.mesa0_numof_pwmgens,
                    mesa0_3pwm, self.d.mesa0_numof_stepgens, ssconfig0, resolver0 ))
                load_cmnds.append( """loadrt%s%s%s config="%s num_encoders=%d num_pwmgens=%d%s num_stepgens=%d%s%s" """ % (
                    driver1, board1_ip, mesa1_ioaddr, firmstring1, self.d.mesa1_numof_encodergens, self.d.mesa1_numof_pwmgens,
                    mesa0_3pwm, self.d.mesa1_numof_stepgens, ssconfig1, resolver1 ))
            for boardnum in range(0,int(self.d.number_mesa)):
                if boardnum == 1 and (board0 == board1):
                    halnum = 1
                else:
                    halnum = 0
                if self.d["mesa%d_numof_pwmgens"% boardnum] > 0:
                    load_cmnds.append( "setp    hm2_%s.%d.pwmgen.pwm_frequency %d"% (
                     self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME],halnum, self.d["mesa%d_pwm_frequency"% boardnum] ))
                    load_cmnds.append( "setp    hm2_%s.%d.pwmgen.pdm_frequency %d"% ( 
                    self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME], halnum,self.d["mesa%d_pdm_frequency"% boardnum] ))
                load_cmnds.append( "setp    hm2_%s.%d.watchdog.timeout_ns %d"% ( 
                    self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME], halnum,self.d["mesa%d_watchdog_timeout"% boardnum] ))
            # READ
            read_cmnds = []
            for boardnum in range(0,int(self.d.number_mesa)):
                if boardnum == 1 and (self.d.mesa0_currentfirmwaredata[_PD._BOARDNAME] == self.d.mesa1_currentfirmwaredata[_PD._BOARDNAME]):
                    halnum = 1
                else:
                    halnum = 0         
                read_cmnds.append( "addf hm2_%s.%d.read          servo-thread"%
                    (self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME], halnum))
            # WRITE
            write_cmnds = []
            for boardnum in range(0,int(self.d.number_mesa)):
                if boardnum == 1 and (self.d.mesa0_currentfirmwaredata[_PD._BOARDNAME] == self.d.mesa1_currentfirmwaredata[_PD._BOARDNAME]):
                    halnum = 1
                else:
                    halnum = 0         
                write_cmnds.append( "addf hm2_%s.%d.write         servo-thread"%
                    (self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME], halnum))
                if '7i76e' in self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME] or \
                    '7i92' in self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME]:
                    write_cmnds.append( "setp hm2_%s.%d.dpll.01.timer-us -50"%
                        (self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME], halnum))
                    write_cmnds.append( "setp hm2_%s.%d.stepgen.timer-number 1"%
                        (self.d["mesa%d_currentfirmwaredata"% boardnum][_PD._BOARDNAME], halnum))
            return load_cmnds,read_cmnds,write_cmnds

    def pport_command_string(self):
        # LOAD
        load_cmnds = []
        # parport stuff
        port3name = port2name = port1name = port3dir = port2dir = port1dir = ""
        if self.d.number_pports>2:
             port3name = " " + self.d.ioaddr3
             if self.d.pp3_direction:
                port3dir =" out"
             else: 
                port3dir =" in"
        if self.d.number_pports>1:
             port2name = " " + self.d.ioaddr2
             if self.d.pp2_direction:
                port2dir =" out"
             else: 
                port2dir =" in"
        port1name = self.d.ioaddr1
        if self.d.pp1_direction:
            port1dir =" out"
        else: 
           port1dir =" in"
        load_cmnds.append("loadrt hal_parport cfg=\"%s%s%s%s%s%s\"" % (port1name, port1dir, port2name, port2dir, port3name, port3dir))
        # READ
        read_cmnds = []
        read_cmnds.append(      "addf parport.0.read           servo-thread")
        if self.d.number_pports > 1:
            read_cmnds.append(  "addf parport.1.read           servo-thread")
        if self.d.number_pports > 2:
            read_cmnds.append(  "addf parport.2.read           servo-thread")
        # WRITE
        write_cmnds = []
        write_cmnds.append(     "addf parport.0.write          servo-thread")
        if self.d.number_pports > 1:
            write_cmnds.append( "addf parport.1.write          servo-thread")
        if self.d.number_pports > 2:
            write_cmnds.append( "addf parport.2.write          servo-thread")
        return load_cmnds,read_cmnds,write_cmnds

# Boiler code
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# starting with 'pncconf -d' gives debug messages
if __name__ == "__main__":
    usage = "usage: pncconf -h for options"
    parser = OptionParser(usage=usage)
    parser.add_option("-d", action="store", metavar='all', dest="debug",
                        help="Print debug info and ignore realtime/kernel tests.\nUse 'alldev' to show all the page tabs. 'step' to stop at each debug print,'excl','5i25','rawfirm','curfirm'")
    (options, args) = parser.parse_args()
    if options.debug:
        app = App(dbgstate=options.debug)
    else:
        app = App('')
    gtk.main()

