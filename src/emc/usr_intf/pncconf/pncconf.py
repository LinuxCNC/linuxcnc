#!/usr/bin/python2.4
# -*- encoding: utf-8 -*-
#    This is pncconf, a graphical configuration editor for EMC2
#    Chris Morley copyright 2009
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
# this is for importing modules from lib/python/pncconf
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
libdir = os.path.join(BASE, "lib", "python","pncconf")
sys.path.insert(0, libdir)
import pwd
import errno
import time
import md5
import pickle
import shutil
import math
import getopt
import textwrap
import locale
import copy
import commands
import fnmatch
import subprocess
import gobject
import gtk
import gtk.glade
import gnome.ui

import xml.dom.minidom
import xml.etree.ElementTree
import xml.etree.ElementPath
import traceback

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
gettext.install("emc2", localedir=LOCALEDIR, unicode=True)
gtk.glade.bindtextdomain("emc2", LOCALEDIR)
gtk.glade.textdomain("emc2")

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
    distdir = "/usr/share/doc/emc2/examples/sample-configs/common"
helpdir = os.path.join(BASE, "share", "emc", "pncconf", "pncconf-help")
if not os.path.exists(helpdir):
    helpdir = os.path.join(BASE, "src", "emc", "usr_intf", "pncconf", "pncconf-help")
firmdir = "/lib/firmware/hm2/"
mesablacklist = ["5i22","7i43","4i65","4i68","SVST8_3P.xml"]

# internalname / displayed name / steptime / step space / direction hold / direction setup
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

(GPIOI, GPIOO, GPIOD, ENCA, ENCB, ENCI, ENCM, STEPA, STEPB, STEPC, STEPD, STEPE, STEPF, PWMP, PWMD, PWME, PDMP, PDMD, PDME ) = pintype_names = [
_("GPIO Input"),_("GPIO Output"),_("GPIO O Drain"),
_("Quad Encoder-A"),_("Quad Encoder-B"),_("Quad Encoder-I"),_("Quad Encoder-M"),
_("Step Gen-A"),_("Dir Gen-B"),_("Step/Dir Gen-C"),_("Step/Dir Gen-D"),_("Step/Dir Gen-E"),_("Step/dir Gen-F"),
_("Pulse Width Gen-P"),_("Pulse Width Gen-D"),_("Pulse Width Gen-E"),
_("Pulse Density Gen-P"),_("Pulse Density Gen-D"),_("Pulse Density Gen-E") ]

( GPIOI, GPIOO, GPIOD ) = pintype_gpio = [ _("GPIO Input"),_("GPIO Output"),_("GPIO O Drain") ]
( ENCA, ENCB, ENCI, ENCM ) = pintype_encoder = [_("Quad Encoder-A"),_("Quad Encoder-B"),_("Quad Encoder-I"),_("Quad Encoder-M") ]
(  MXEA, MXEB, MXEI, MXEM, MXES ) = pintype_muxencoder = [_("Muxed Encoder-A"),_("Muxed Encoder-B"),_("Muxed Encoder-I"),_("Muxed Encoder-M"),
    _("Mux Enc Select") ]
( STEPA, STEPB, STEPC, STEPD, STEPE, STEPF ) = pintype_stepper = [_("Step Gen-A"),_("Dir Gen-B"),_("Step/Dir Gen-C"), _("Step/Dir Gen-D"),
    _("Step/Dir Gen-E"),_("Step/dir Gen-F") ]
( PWMP, PWMD, PWME ) = pintype_pwm = [ _("Pulse Width Gen-P"),_("Pulse Width Gen-D"),_("Pulse Width Gen-E") ]
( PDMP, PDMD, PDME ) = pintype_pdm = [ _("Pulse Density Gen-P"),_("Pulse Density Gen-D"),_("Pulse Density Gen-E") ]
( TPPWMA,TPPWMB,TPPWMC,TPPWMAN,TPPWMBN,TPPWMCN,TPPWME,TPPWMF ) = pintype_tp_pwm = [ _("Motor Phase A"),_("Motor Phase B"),_("Motor Phase C"),
    _("Motor Phase A Not"),_("Motor Phase B Not") ,_("Motor Phase C Not"), _("Motor Enable"), _("Motor Fault") ]

_BOARDTITLE = 0;_BOARDNAME = 1;_FIRMWARE = 2;_DIRECTORY = 3;_HALDRIVER = 4;_MAXENC = 5;_ENCPINS = 6;_MAXPWM = 7;_MAXTPPWM = 8;
_MAXSTEP = 9;_STEPPINS = 10;_HASWATCHDOG = 11;_MAXGPIO = 12;_LOWFREQ = 13;_HIFREQ = 14;_NUMOFCNCTRS = 15;_STARTOFDATA = 16
_AXIS = 1;_TKEMC = 2;_MINI = 3;_TOUCHY = 4
_IMPERIAL = 0;_METRIC = 1
# boardname, firmwarename, firmware directory,Hal driver name,
# max encoders, max pwm gens, 
# max step gens, number of pins per encoder,
# number of pins per step gen, 
# has watchdog, max GPIOI, 
# low frequency rate , hi frequency rate, 
# available connector numbers,  then list of component type and logical number
mesafirmwaredata = [
    ["5i20", "5i20", "SV12", "5i20", "hm2_pci", 12, 3, 12, 0, 0, 0, 1, 72 , 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10] ],
    ["5i20", "5i20", "SVST8_4", "5i20", "hm2_pci", 8, 3, 8, 0, 4, 6, 1, 72, 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i20", "5i20", "SVST2_4_7I47", "5i20", "hm2_pci", 4, 3, 2, 0, 4, 2, 1, 72, 33, 100, [2,3,4],
        [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[ENCA,0],[ENCA,2],[ENCB,0],[ENCB,2],[ENCI,0],[ENCI,2],[ENCA,1],[ENCA,3],
                [ENCB,1],[ENCB,3],[ENCI,1],[ENCI,3],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[PWMP,0],[PWMD,0],[PWMP,1],[PWMD,1],
        [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
        [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0], ],
    ["5i20", "5i20", "SVST2_8", "5i20", "hm2_pci", 2, 3, 2, 0, 8, 6, 1, 72, 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
        [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i20", "5i20", "SVST8_4IM2", "5i20", "hm2_pci", 8, 4, 8, 0, 4, 2, 1, 72, 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCM,0],[ENCM,1],[ENCM,2],[ENCM,3],[ENCM,4],[ENCM,5],[ENCM,6],[ENCM,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                 [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3] ],
    ["5i22-1", "5i22", "SV16", "5i22", "hm2_pci", 16, 3, 16, 0, 0, 0, 1, 96, 48, 96, [2,3,4,5],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10],
        [ENCB,13],[ENCA,13],[ENCB,12],[ENCA,12],[ENCI,13],[ENCI,12],[PWMP,13],[PWMP,12],[PWMD,13],[PWMD,12],[PWME,13],[PWME,12],
                  [ENCB,15],[ENCA,15],[ENCB,14],[ENCA,14],[ENCI,15],[ENCI,14],[PWMP,15],[PWMP,14],[PWMD,15],[PWMD,14],[PWME,15],[PWME,14] ],
    ["5i22-1", "5i22", "SVST8_8", "5i22", "hm2_pci", 8, 3, 8, 0, 8, 6, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i22-1", "5i22", "SVS8_24", "5i22", "hm2_pci", 8, 3, 8, 0, 24, 2, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
       [STEPA,12],[STEPB,12],[STEPA,13],[STEPB,13],[STEPA,14],[STEPB,14],[STEPA,15],[STEPB,15],[STEPA,16],[STEPB,16],[STEPA,17],[STEPB,17],
                [STEPA,18],[STEPB,18],[STEPA,19],[STEPB,19],[STEPA,20],[STEPB,20],[STEPA,21],[STEPB,21],[STEPA,22],[STEPB,22],[STEPA,23],[STEPB,23] ],
    ["5i22-1.5", "5i22", "SV16", "5i22", "hm2_pci", 16, 3, 16, 0, 0, 0, 1, 96, 48, 96, [2,3,4,5],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10],
        [ENCB,13],[ENCA,13],[ENCB,12],[ENCA,12],[ENCI,13],[ENCI,12],[PWMP,13],[PWMP,12],[PWMD,13],[PWMD,12],[PWME,13],[PWME,12],
                  [ENCB,15],[ENCA,15],[ENCB,14],[ENCA,14],[ENCI,15],[ENCI,14],[PWMP,15],[PWMP,14],[PWMD,15],[PWMD,14],[PWME,15],[PWME,14] ],
    ["5i22-1.5", "5i22", "SVST8_8", "5i22", "hm2_pci", 8, 3, 8, 0, 8, 6, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i22-1.5", "5i22", "SVS8_24", "5i22", "hm2_pci", 8, 3, 8, 0, 24, 2, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
       [STEPA,12],[STEPB,12],[STEPA,13],[STEPB,13],[STEPA,14],[STEPB,14],[STEPA,15],[STEPB,15],[STEPA,16],[STEPB,16],[STEPA,17],[STEPB,17],
                [STEPA,18],[STEPB,18],[STEPA,19],[STEPB,19],[STEPA,20],[STEPB,20],[STEPA,21],[STEPB,21],[STEPA,22],[STEPB,22],[STEPA,23],[STEPB,23] ],
    ["5i23", "5i23", "SV12", "5i23", "hm2_pci", 12, 3, 12, 0, 0, 0, 1, 72 , 48, 96, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10] ],
    ["5i23", "5i23", "SVST8_4", "5i23", "hm2_pci", 8, 3, 8, 0, 4, 6, 1, 72, 48, 96, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i23", "5i23", "SVST4_8", "5i23", "hm2_pci", 4, 3, 4, 8, 6, 1, 72, 48, 96, [2,3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                 [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                 [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["7i43-2", "7i43", "SV8", "7i43-2", "hm2_7i43", 8, 3, 8, 0, 0, 1, 48, 50, 100, [4,3],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6] ],
    ["7i43-2", "7i43", "SVST4_4", "7i43-2", "hm2_7i43", 4, 3, 4, 0, 4, 6, 1, 48, 50, 100, [4,3],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["7i43-2", "7i43", "SVST4_6", "7i43-2", "hm2_7i43", 4, 4, 6, 0, 3, 4, 1, 48, 50, 100, [4,3],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],
                  [STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0] ],
    ["7i43-4", "7i43", "SV8", "7i43-4", "hm2_7i43", 8, 3, 8, 0, 0, 0, 1, 48, 50, 100, [4,3],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6] ],
    ["7i43-4", "7i43", "SVST4_4", "7i43-4", "hm2_7i43", 4, 3, 4, 0, 4, 6, 1, 48, 50, 100, [4,3],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["7i43-4", "7i43", "SVST4_6", "7i43-4", "hm2_7i43", 4, 3, 4, 6, 4, 1, 48, 50, 100, [4,3],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],
                  [STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0] ],
    ["7i43-4", "7i43", "SVST4_12", "7i43-4", "hm2_7i43", 4, 3, 4, 0, 12, 2, 1, 48, 50, 100, [4,3],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                  [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11] ],
    ["3x20-1", "3x20", "SV24", "3x20-1", "hm2_pci", 24, 3, 24, 0, 0, 0, 1, 144, 50, 100, [4,5,6,9,8,7],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10],
        [ENCB,13],[ENCA,13],[ENCB,12],[ENCA,12],[ENCI,13],[ENCI,12],[PWMP,13],[PWMP,12],[PWMD,13],[PWMD,12],[PWME,13],[PWME,12],
                 [ENCB,15],[ENCA,15],[ENCB,14],[ENCA,14],[ENCI,15],[ENCI,14],[PWMP,15],[PWMP,14],[PWMD,15],[PWMD,14],[PWME,15],[PWME,14],
        [ENCB,17],[ENCA,17],[ENCB,16],[ENCA,16],[ENCI,17],[ENCI,16],[PWMP,17],[PWMP,16],[PWMD,17],[PWMD,16],[PWME,17],[PWME,16],
                 [ENCB,19],[ENCA,19],[ENCB,18],[ENCA,18],[ENCI,19],[ENCI,18],[PWMP,19],[PWMP,18],[PWMD,19],[PWMD,18],[PWME,19],[PWME,18],
        [ENCB,21],[ENCA,21],[ENCB,20],[ENCA,20],[ENCI,21],[ENCI,20],[PWMP,21],[PWMP,20],[PWMD,21],[PWMD,20],[PWME,21],[PWME,20],
                 [ENCB,23],[ENCA,23],[ENCB,22],[ENCA,22],[ENCI,23],[ENCI,22],[PWMP,23],[PWMP,22],[PWMD,23],[PWMD,22],[PWME,23],[PWME,22] ],
    ["3x20-1", "3x20", "SVST16_24", "3x20-1", "hm2_pci", 16, 3, 16, 0, 24, 2, 1, 144, 50, 100, [4,5,6,9,8,7],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10],
        [ENCB,13],[ENCA,13],[ENCB,12],[ENCA,12],[ENCI,13],[ENCI,12],[PWMP,13],[PWMP,12],[PWMD,13],[PWMD,12],[PWME,13],[PWME,12],
                 [ENCB,15],[ENCA,15],[ENCB,14],[ENCA,14],[ENCI,15],[ENCI,14],[PWMP,15],[PWMP,14],[PWMD,15],[PWMD,14],[PWME,15],[PWME,14],
        [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
        [STEPA,12],[STEPB,12],[STEPA,13],[STEPB,13],[STEPA,14],[STEPB,14],[STEPA,15],[STEPB,15],[STEPA,16],[STEPB,16],[STEPA,17],[STEPB,17],
                [STEPA,18],[STEPB,18],[STEPA,19],[STEPB,19],[STEPA,20],[STEPB,20],[STEPA,21],[STEPB,21],[STEPA,22],[STEPB,22],[STEPA,23],[STEPB,23] ],

]
# boardname, firmwarename, Hal driver name,
# max encoders, max pwm gens, 
# max step gens, number of pins per encoder,
# number of pins per step gen, 
# has watchdog, max GPIOI, 
# low frequency rate , hi frequency rate, 
# available connector numbers,  then list of component type and logical number
mesaboardnames = [ "5i20", "5i22-1", "5i22-1.5", "5i23", "7i43-2", "7i43-4","3x20-1" ]

(UNUSED_OUTPUT,
ON, CW, CCW, BRAKE,
MIST, FLOOD, ESTOP, AMP,
PUMP, DOUT0, DOUT1, DOUT2, DOUT3) = hal_output_names = [
"unused-output", 
"spindle-enable", "spindle-cw", "spindle-ccw", "spindle-brake",
"coolant-mist", "coolant-flood", "estop-out", "enable",
"charge-pump", "dout-00", "dout-01", "dout-02", "dout-03" ]

spindle_output = [_("Spindle ON"),_("Spindle CW"), _("Spindle CCW"), _("Spindle Brake") ]
coolant_output = [_("Coolant Mist"), _("Coolant Flood")]
control_output = [_("ESTOP Out"), _("Amplifier Enable"),_("Charge Pump")]
digital_output = [_("Digital out 0"), _("Digital out 1"), _("Digital out 2"), _("Digital out 3")]
human_output_names = [ [_("Unused Output"),[None]],[_("Spindle"),spindle_output],[_("Coolant"),coolant_output],
    [_("Control"),control_output],[_("Digital"),digital_output],[_("Custom Signals"),[None]]  ]

limit = [_("X Minimum Limit"), _("Y Minimum Limit"), _("Z Minimum Limit"), _("A Minimum Limit"),
    _("X Maximum Limit"), _("Y Maximum Limit"), _("Z Maximum Limit"), _("A Maximum Limit"),
    _("X Both Limit"), _("Y Both Limit"), _("Z Both Limit"), _("A Both Limit"),
    _("All Limits") ]
home = [_("X Home"), _("Y Home"), _("Z Home"), _("A Home"),_("All Home") ]
home_limits_shared = [ _("X Minimum Limit + Home"), _("Y Minimum Limit + Home"), _("Z Minimum Limit + Home"), _("A Minimum Limit + Home"),
    _("X Maximum Limit + Home"), _("Y Maximum Limit + Home"), _("Z Maximum Limit + Home"), _("A Maximum Limit + Home"),
    _("X Both Limit + Home"), _("Y Both Limit + Home"), _("Y Both Limit + Home"), _("A Both Limit + Home") ]
digital = [ _("Digital in 0"), _("Digital in 1"), _("Digital in 2"), _("Digital in 3") ]
axis_select = [_("Joint select A"),_("Joint select B"),_("Joint select C"), _("Joint select D") ]
override = [_("Jog incr A"),_("Jog incr B"),_("Jog incr C"),_("Jog incr D"),_("Feed Override incr A"),_("Feed Override incr B"),
    _("Feed Override incr C"),_("Feed Override incr D") ]
spindle = [ _("Manual Spindle CW"),_("Manual Spindle CCW"),_("Manual Spindle Stop"),_("Spindle Up-To-Speed") ]
operation =  [_("Cycle Start"),_("Abort"),_("Single Step") ]
control = [_("ESTOP In"), _("Probe In") ]
rapid = [_("Jog X +"),_("Jog X -"),_("Jog Y +"),_("Jog Y -"),_("Jog Z +"),_("Jog Z -"),_("Jog A +"),_("Jog A -"),
    _("Jog button selected +"),_("Jog button selected -") ]

human_input_names = [ [_("Unused Input"),[None]],[_("Limits"),limit],[_("Home"),home],[_("Limts/Home Shared"),home_limits_shared],
    [_("Digital"),digital],[_("Axis Selection"),axis_select],[_("Overrides"),override],[_("Spindle"),spindle],
    [_("Operation"),operation],[_("External Control"),control],[_("Axis rapid"),rapid],[_("Custom Signals"),[None]] ]

(UNUSED_INPUT,
MIN_X, MIN_Y, MIN_Z, MIN_A,
MAX_X, MAX_Y, MAX_Z, MAX_A,
BOTH_X, BOTH_Y, BOTH_Z, BOTH_A,ALL_LIMIT, 
HOME_X, HOME_Y, HOME_Z, HOME_A,ALL_HOME,
MIN_HOME_X, MIN_HOME_Y, MIN_HOME_Z, MIN_HOME_A,
MAX_HOME_X, MAX_HOME_Y, MAX_HOME_Z, MAX_HOME_A,
BOTH_HOME_X, BOTH_HOME_Y, BOTH_HOME_Z, BOTH_HOME_A,
DIN0, DIN1, DIN2, DIN3,
SELECT_A, SELECT_B, SELECT_C, SELECT_D,
JOGA, JOGB, JOGC, JOGD, FOA, FOB, FOC, FOD,
SPINDLE_CW, SPINDLE_CCW, SPINDLE_STOP,SPINDLE_AT_SPEED,
CYCLE_START, ABORT, SINGLE_STEP,
ESTOP_IN, PROBE,
JOGX_P,JOGX_N,JOGY_P,JOGY_N,JOGZ_P,JOGZ_N,JOGA_P,JOGA_N,JOGSLCT_P, JOGSLCT_N    ) = hal_input_names = ["unused-input",
"min-x", "min-y", "min-z", "min-a",
"max-x", "max-y", "max-z", "max-a",
"both-x", "both-y", "both-z", "both-a","all-limit",
"home-x", "home-y", "home-z", "home-a","all-home",
"min-home-x", "min-home-y", "min-home-z", "min-home-a",
"max-home-x", "max-home-y", "max-home-z", "max-home-a",
"both-home-x", "both-home-y", "both-home-z", "both-home-a",
"din-00", "din-01", "din-02", "din-03",
"joint-select-a","joint-select-b","joint-select-c","joint-select-d",
"jog-incr-a","jog-incr-b","jog-incr-c","jog-incr-d","fo-incr-a","fo-incr-b","fo-incr-c","fo-incr-d",
"spindle-manual-cw","spindle-manual-ccw","spindle-manual-stop","spindle-at-speed",
"cycle-start","abort","single-step",
"estop-ext", "probe-in",
"jog-x-pos","jog-x-neg","jog-y-pos","jog-y-neg",
"jog-z-pos","jog-z-neg","jog-a-pos","jog-a-neg","jog-selected-pos","jog-selected-neg"]


human_names_multi_jog_buttons = [_("Jog X +"),_("Jog X -"),
_("Jog Y +"),_("Jog Y -"),
_("Jog Z +"),_("Jog Z -"),
_("Jog A +"),_("Jog A -")]

human_names_shared_home = [_("X Minimum Limit + Home"), _("Y Minimum Limit + Home"),
_("Z Minimum Limit + Home"), _("A Minimum Limit + Home"),
_("X Maximum Limit + Home"), _("Y Maximum Limit + Home"),
_("Z Maximum Limit + Home"), _("A Maximum Limit + Home"),
_("X Both Limit + Home"), _("Y Both Limit + Home"),
_("Z Both Limit + Home"), _("A Both Limit + Home")]

human_names_limit_only = [ _("X Minimum Limit"), _("Y Minimum Limit"),
_("Z Minimum Limit"), _("A Minimum Limit"),
_("X Maximum Limit"), _("Y Maximum Limit"),
_("Z Maximum Limit"), _("A Maximum Limit"),
_("X Both Limit"), _("Y Both Limit"),
_("Z Both Limit"), _("A Both Limit"), _("All Limits")]

(UNUSED_PWM,
X_PWM_PULSE, X_PWM_DIR, X_PWM_ENABLE, Y_PWM_PULSE, Y_PWM_DIR, Y_PWM_ENABLE, 
Z_PWM_PULSE, Z_PWM_DIR, Z_PWM_ENABLE, A_PWM_PULSE, A_PWM_DIR, A_PWM_ENABLE, 
SPINDLE_PWM_PULSE, SPINDLE_PWM_DIR, SPINDLE_PWM_ENABLE,   ) = hal_pwm_output_names = ["unused-pwm",
"x-pwm-pulse", "x-pwm-dir", "x-pwm-enable", "y-pwm-pulse", "y-pwm-dir", "y-pwm-enable",
"z-pwm-pulse", "z-pwm-dir", "z-pwm-enable", "a-pwm-pulse", "a-pwm-dir", "a-pwm-enable", 
"s-pwm-pulse", "s-pwm-dir", "s-pwm-enable"]

human_pwm_output_names =[ [_("Unused PWM Gen"),[None]],[_("X Axis PWM"),[None]],[_("Y Axis PWM"),[None]],
                        [_("Z Axis PWM"),[None]],[_("A Axis PWM"),[None]],[_("Spindle PWM"),[None]],[_("Custom Signals"),[None]] ]

(UNUSED_ENCODER, 
X_ENCODER_A, X_ENCODER_B, X_ENCODER_I, X_ENCODER_M,
Y_ENCODER_A, Y_ENCODER_B, Y_ENCODER_I, Y_ENCODER_M,
Z_ENCODER_A, Z_ENCODER_B, Z_ENCODER_I, Z_ENCODER_M, 
A_ENCODER_A, A_ENCODER_B, A_ENCODER_I, A_ENCODER_M, 
SPINDLE_ENCODER_A, SPINDLE_ENCODER_B, SPINDLE_ENCODER_I, SPINDLE_ENCODER_M,
X_MPG_A, X_MPG_B, X_MPG_I, X_MPG_M, Y_MPG_A, Y_MPG_B, Y_MPG_I, Y_MPG_M,
Z_MPG_A, Z_MPG_B, Z_MPG_I, Z_MPG_M, A_MPG_A, A_MPG_B, A_MPG_I,A_MPG_M,
SELECT_MPG_A, SELECT_MPG_B, SELECT_MPG_I, SELECT_MPG_M,
FO_MPG_A,FO_MPG_B,FO_MPG_I,FO_MPG_M,SO_MPG_A,SO_MPG_B,SO_MPG_I,SO_MPG_I,)  = hal_encoder_input_names = [ "unused-encoder",
"x-encoder-a", "x-encoder-b", "x-encoder-i", "x-encoder-m",
"y-encoder-a", "y-encoder-b", "y-encoder-i", "y-encoder-m",
"z-encoder-a", "z-encoder-b", "z-encoder-i", "z-encoder-m", 
"a-encoder-a", "a-encoder-b", "a-encoder-i", "a-encoder-m",
"s-encoder-a","s-encoder-b","s-encoder-i", "s-encoder-m",
"x-mpg-a","x-mpg-b", "x-mpg-i", "x-mpg-m", "y-mpg-a", "y-mpg-b", "y-mpg-i", "y-mpg-m",
"z-mpg-a","z-mpg-b", "z-mpg-i", "z-mpg-m", "a-mpg-a", "a-mpg-b", "a-mpg-i", "a-mpg-m",
"select-mpg-a", "select-mpg-b", "select-mpg-i", "select-mpg-m",
"fo-mpg-a","fo-mpg-b","fo-mpg-i","fo-mpg-m","so-mpg-a","so-mpg-b","so-mpg-i","so-mpg-m"]

axis = [_("X Encoder"),_("Y Encoder"), _("Z Encoder"),_("A Encoder"),_("Spindle Encoder")]
mpg = [_("X Hand Wheel"), _("Y Hand Wheel"), _("Z Hand Wheel"), _("A Hand Wheel") ,_("Multi Hand Wheel")]
over = [_("Feed Override"),_("spindle Override")]
human_encoder_input_names = [ [_("Unused Encoder"),[None]],[_("Axis Encoder"), axis],[_("MPG Jog Controls"), mpg],[_("Override MPG control"), over],
                            [_("Custom Signals"),[None]] ] 


(UNUSED_STEPGEN, 
X_STEPGEN_STEP, X_STEPGEN_DIR, X_STEPGEN_PHC, X_STEPGEN_PHD, X_STEPGEN_PHE, X_STEPGEN_PHF,
Y_STEPGEN_STEP, X_STEPGEN_DIR, X_STEPGEN_PHC, X_STEPGEN_PHD, X_STEPGEN_PHE, X_STEPGEN_PHF,
Z_STEPGEN_STEP, Z_STEPGEN_DIR, Z_STEPGEN_PHC, Z_STEPGEN_PHD, Z_STEPGEN_PHE, Z_STEPGEN_PHF,
A_STEPGEN_STEP, A_STEPGEN_DIR, A_STEPGEN_PHC, A_STEPGEN_PHD, A_STEPGEN_PHE, A_STEPGEN_PHF,
SPINDLE_STEPGEN_STEP, SPINDLE_STEPGEN_DIR, SPINDLE_STEPGEN_PHC, SPINDLE_STEPGEN_PHD, SPINDLE_STEPGEN_PHE, SPINDLE_STEPGEN_PHF) = hal_stepper_names = ["unused-stepgen", 
"x-stepgen-step", "x-stepgen-dir", "x-stepgen-phase-c", "x-stepgen-phase-d", "x-stepgen-phase-e", "x-stepgen-phase-f", 
"y-stepgen-step", "y-stepgen-dir", "y-stepgen-phase-c", "y-stepgen-phase-d", "y-stepgen-phase-e", "y-stepgen-phase-f",
"z-stepgen-step", "z-stepgen-dir", "z-stepgen-phase-c", "z-stepgen-phase-d", "z-stepgen-phase-e", "z-stepgen-phase-f",
"a-stepgen-step", "a-stepgen-dir", "a-stepgen-phase-c", "a-stepgen-phase-d", "a-stepgen-phase-e", "a-stepgen-phase-f",
"s-stepgen-step", "s-stepgen-dir", "s-stepgen-phase-c", "s-stepgen-phase-d", "s-stepgen-phase-e", 
"s-stepgen-phase-f"]

human_stepper_names = [ [_("Unused StepGen"),[None]],[_("X Axis StepGen"),[None]],[_("Y Axis StepGen"),[None]],[_("Z Axis StepGen"),[None]],
                        [_("A Axis StepGen"),[None]],[_("Spindle StepGen"),[None]],[_("Custom Signals"),[None]] ]

(UNUSED_TPPWM,
X_TPPWM_A, X_TPPWM_B,X_TPPWM_C,X_TPPWM_AN,X_TPPWM_BN,X_TPPWM_CN,X_TPPWM_ENABLE,X_TPPWM_FAULT,
Y_TPPWM_A, Y_TPPWM_B,Y_TPPWM_C,Y_TPPWM_AN,Y_TPPWM_BN,Y_TPPWM_CN,Y_TPPWM_ENABLE,Y_TPPWM_FAULT,
Z_TPPWM_A, Z_TPPWM_B,Z_TPPWM_C,Z_TPPWM_AN,Z_TPPWM_BN,Z_TPPWM_CN,Z_TPPWM_ENABLE,Z_TPPWM_FAULT,
A_TPPWM_A, A_TPPWM_B,A_TPPWM_C,A_TPPWM_AN,A_TPPWM_BN,A_TPPWM_CN,A_TPPWM_ENABLE,A_TPPWM_FAULT,
S_TPPWM_A, S_TPPWM_B,S_TPPWM_C,S_TPPWM_AN,S_TPPWM_BN,S_TPPWM_CN,S_TPPWM_ENABLE,S_TPPWM_FAULT) = hal_tppwm_output_names= ["unused-tppwm",
"x-tppwm-a","x-tppwm-b","x-tppwm-c","x-tppwm-anot","x-tppwm-bnot","x-tppwm-cnot", "x-tppwm-enable","x-tppwm-fault",
"y-tppwm-a","y-tppwm-b","y-tppwm-c","y-tppwm-anot","y-tppwm-bnot","y-tppwm-cnot", "y-tppwm-enable","y-tppwm-fault",
"z-tppwm-a","z-tppwm-b","z-tppwm-c","z-tppwm-anot","z-tppwm-bnot","z-tppwm-cnot", "z-tppwm-enable","z-tppwm-fault",
"a-tppwm-a","a-tppwm-b","a-tppwm-c","a-tppwm-anot","a-tppwm-bnot","a-tppwm-cnot", "a-tppwm-enable","a-tppwm-fault",
"s-tppwm-a","s-tppwm-b","s-tppwm-c","s-tppwm-anot","s-tppwm-bnot","s-tppwm-cnot", "s-tppwm-enable","s-tppwm-fault"]

human_tppwm_output_names = [ [_("Unused TPPWM Gen"),[None]],[_("X Axis BL Driver"),[None]],[ _("Y Axis BL Driver"),[None]],
    [_("Z Axis BL Driver"),[None]],[_("A Axis BL Driver"),[None]],[_("S Axis BL Driver"),[None]],[_("Custom Signals"),[None]] ]



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

class Intrnl_data:
    def __init__(self):
        self.mesa0_configured = False
        self.mesa1_configured = False
        self.components_is_prepared = False
        #self.available_axes = []
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class Data:
    def __init__(self):
        pw = pwd.getpwuid(os.getuid())
        # custom signal name lists
        self.halencoderinputsignames = []
        self.halmuxencodersignames = []
        self.halpwmoutputsignames = []
        self.haltppwmoutputsignames = []
        self.halinputsignames = []
        self.haloutputsignames = []
        self.halsteppersignames = []

        # internal flags
        self._arrayloaded = False

        # internal data
        self._gpioliststore = None
        self._stepperliststore = None
        self._encoderliststore = None
        self._muxencoderliststore = None
        self._pwmliststore = None
        self._tppwmliststore = []
        self._gpioosignaltree = None
        self._gpioisignaltree = None
        self._steppersignaltree = None
        self._encodersignaltree = None
        self._muxencodersignaltree = None
        self._pwmcontrolsignaltree = None
        self._pwmrelatedsignaltree = None
        self._tppwmsignaltree = None

        # pncconf default options
        self.createsymlink = 1
        self.createshortcut = 0  
        self._lastconfigname= ""
        self._chooselastconfig = True

        # basic machine data
        self.help = "help-welcome.txt"
        self.machinename = _("my_EMC_machine")
        self.frontend = _AXIS 
        self.axes = 0 # XYZ
        self.available_axes = []
        self.baseperiod = 50000
        self.servoperiod = 1000000
        self.units = _IMPERIAL # inch
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
        self.joycmdrapid = ""
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

        # GUI frontend defaults
        self.position_offset = 1 # relative
        self.position_feedback = 1 # actual
        self.max_feed_override = 2.0 # percentage
        self.min_spindle_override = .5
        self.max_spindle_override = 1.0
        # These are for AXIS gui only
        self.default_linear_velocity = .25 # units per second
        self.min_linear_velocity = .01
        self.max_linear_velocity = 1.0
        self.default_angular_velocity = .25
        self.min_angular_velocity = .01
        self.max_angular_velocity = 1.0
        self.increments_metric = "5mm 1mm .5mm .1mm .05mm .01mm .005mm"
        self.increments_imperial= ".1in .05in .01in .005in .001in .0005in .0001in"
        self.editor = "gedit"
        self.geometry = "xyz"

        # EMC assorted defaults and options
        self.toolchangeprompt = True
        self.multimpg = False
        self.require_homing = True
        self.individual_homing = False
        self.restore_joint_position = False
        self.random_toolchanger = False
        self.raise_z_on_toolchange = False
        self.allow_spindle_on_toolchange = False
        self.customhal = False # include custom hal file
        self.userneededpid = 0
        self.userneededabs = 0
        self.userneededscale = 0
        self.userneededmux16 = 0
        self.userneededlowpass = 0

        # pyvcp data
        self.pyvcp = 0 # not included
        self.pyvcpname = "custom.xml"
        self.pyvcphaltype = 0 # no HAL connections specified
        self.pyvcpconnect = 1 # HAL connections allowed

        # gladevcp data
        self.gladevcp = 0 # not included
        self.gladevcpname = "custom.xml"
        self.gladevcphaltype = 0 # no HAL connections specified
        self.gladevcpconnect = 1 # HAL connections allowed

        # classicladder data
        self.classicladder = 0 # not included
        self.digitsin = 15 # default number of pins
        self.digitsout = 15
        self.s32in = 10
        self.s32out = 10
        self.floatsin = 10
        self.floatsout = 10
        self.tempexists = 0 # not present
        self.laddername = "custom.clp"
        self.modbus = 0 # not included
        self.ladderhaltype = 0 # no HAL connections specified
        self.ladderconnect = 1 # HAL connections allowed

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
        self.ioaddr = "0x0278"
        self.ioaddr2 = _("Enter Address")
        self.pp2_direction = 0 # input
        self.ioaddr3 = _("Enter Address")
        self.pp3_direction = 0 # input
        self.number_pports = 0

        for connector in("pp1","pp2","pp3"):
            # initialize parport input / inv pins
            for i in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                pinname ="%sIpin%d"% (connector,i)
                self[pinname] = UNUSED_INPUT
                pinname ="%sIpin%dinv"% (connector,i)
                self[pinname] = False
            # initialize parport output / inv pins
            for i in (1,2,3,4,5,6,7,8,9,14,16,17):
                pinname ="%sOpin%d"% (connector,i)
                self[pinname] = UNUSED_OUTPUT
                pinname ="%sOpin%dinv"% (connector,i)
                self[pinname] = False

        self.number_mesa = 1 # number of cards
        # for first mesa card
        self.mesa0_currentfirmwaredata = mesafirmwaredata[1]       
        self.mesa0_boardtitle = "5i20"        
        self.mesa0_firmware = "SVST8_4"
        self.mesa0_parportaddrs = "0x378"
        self.mesa0_isawatchdog = 1
        self.mesa0_pwm_frequency = 100000
        self.mesa0_pdm_frequency = 100000
        self.mesa0_watchdog_timeout = 10000000
        self.mesa0_numof_encodergens = 4
        self.mesa0_numof_pwmgens = 4
        self.mesa0_numof_tppwmgens = 0
        self.mesa0_numof_stepgens = 0
        self.mesa0_numof_gpio = 48
        # second mesa card
        self.mesa1_currentfirmwaredata = mesafirmwaredata[1]
        self.mesa1_boardtitle = "5i20"
        self.mesa1_firmware = "SVST8_4"
        self.mesa1_parportaddrs = "0x378"
        self.mesa1_isawatchdog = 1
        self.mesa1_pwm_frequency = 100000
        self.mesa1_pdm_frequency = 100000
        self.mesa1_watchdog_timeout = 10000000
        self.mesa1_numof_encodergens = 4
        self.mesa1_numof_pwmgens = 4
        self.mesa1_numof_tppwmgens = 0
        self.mesa1_numof_stepgens = 0
        self.mesa1_numof_gpio = 48

        for boardnum in(0,1):
            connector = 2
            pinname ="mesa%dc%dpin"% (boardnum,connector)
            self[pinname+"0"] = UNUSED_ENCODER
            self[pinname+"0type"] = ENCB
            self[pinname+"1"] = UNUSED_ENCODER
            self[pinname+"1type"] = ENCA
            self[pinname+"2"] = UNUSED_ENCODER
            self[pinname+"2type"] = ENCB
            self[pinname+"3"] = UNUSED_ENCODER
            self[pinname+"3type"] = ENCA
            self[pinname+"4"] = UNUSED_ENCODER
            self[pinname+"4type"] = ENCI
            self[pinname+"5"] = UNUSED_ENCODER
            self[pinname+"5type"] = ENCI
            self[pinname+"6"] = UNUSED_PWM
            self[pinname+"6type"] = PWMP
            self[pinname+"7"] = UNUSED_PWM
            self[pinname+"7type"] = PWMP
            self[pinname+"8"] = UNUSED_PWM
            self[pinname+"8type"] = PWMD
            self[pinname+"9"] = UNUSED_PWM
            self[pinname+"9type"] = PWMD
            self[pinname+"10"] = UNUSED_PWM
            self[pinname+"10type"] = PWME
            self[pinname+"11"] = UNUSED_PWM
            self[pinname+"11type"] = PWME
            self[pinname+"12"] = UNUSED_ENCODER
            self[pinname+"12type"] = ENCB
            self[pinname+"13"] = UNUSED_ENCODER
            self[pinname+"13type"] = ENCA
            self[pinname+"14"] = UNUSED_ENCODER
            self[pinname+"14type"] = ENCB
            self[pinname+"15"] = UNUSED_ENCODER
            self[pinname+"15type"] = ENCA
            self[pinname+"16"] = UNUSED_ENCODER
            self[pinname+"16type"] = ENCI
            self[pinname+"17"] = UNUSED_ENCODER
            self[pinname+"17type"] = ENCI
            self[pinname+"18"] = UNUSED_PWM
            self[pinname+"18type"] = PWMP
            self[pinname+"19"] = UNUSED_PWM
            self[pinname+"19type"] = PWMP
            self[pinname+"20"] = UNUSED_PWM
            self[pinname+"20type"] = PWMD
            self[pinname+"21"] = UNUSED_PWM
            self[pinname+"21type"] = PWMD
            self[pinname+"22"] = UNUSED_PWM
            self[pinname+"22type"] = PWME
            self[pinname+"23"] = UNUSED_PWM
            self[pinname+"23type"] = PWME
        for boardnum in(0,1):
            for connector in(3,4,5,6,7,8,9):
                # This initializes GPIO input pins
                for i in range(0,16):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self[pinname] = UNUSED_INPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self[pinname] = GPIOI
                # This initializes GPIO output pins
                for i in range(16,24):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self[pinname] = UNUSED_OUTPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self[pinname] = GPIOO
            for connector in(2,3,4,5,6,7,8,9):
                # This initializes the mesa inverse pins
                for i in range(0,24):
                    pinname ="mesa%dc%dpin%dinv"% (boardnum,connector,i)
                    self[pinname] = False

        # halui data
        self.halui = 0 # not included
        # Command list
        for i in range(1,16):
                pinname ="halui_cmd%s"% i
                self[pinname] = ""

        #HAL component command list
        self.loadcompservo = []
        self.addcompservo = []
        self.loadcompbase = []
        self.addcompbase = []

        # axis x data
        self.xdrivertype = "custom"
        self.xsteprev = 200
        self.xmicrostep = 5
        self.xmotor_pulleydriver = 1
        self.xmotor_pulleydriven = 1
        self.xmotor_wormdriver = 1
        self.xmotor_wormdriven = 1
        self.xencoder_pulleydriver = 1
        self.xencoder_pulleydriven = 1
        self.xencoder_wormdriver = 1
        self.xencoder_wormdriven = 1
        self.xmotor_leadscrew = 5
        self.xencoder_leadscrew = 5
        self.xencodercounts = 4000
        self.xusecomp = 0
        self.xcompfilename = "xcompensation"
        self.xcomptype = 0
        self.xusebacklash = 0
        self.xbacklash = 0
        self.xmaxvel = 1.667
        self.xmaxacc = 2
        self.xinvertmotor = 0
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
        self.xbias = 0
        self.xdeadband = 0
        self.xsteptime = 1000
        self.xstepspace = 1000
        self.xdirhold = 1000
        self.xdirsetup = 1000
        self.xminferror = .0005
        self.xmaxferror = .005
        self.xhomepos = 0
        self.xminlim =  0
        self.xmaxlim =  8
        self.xhomesw =  0
        self.xhomesearchvel = .05
        self.xhomelatchvel = .025
        self.xhomefinalvel = 0
        self.xlatchdir = 0
        self.xsearchdir = 0
        self.xusehomeindex = 1
        self.xhomesequence = 1203
        self.xstepscale = 0
        self.xencoderscale = 0

        # axis y data
        self.ydrivertype = "custom"
        self.ysteprev = 200
        self.ymicrostep = 5
        self.ymotor_pulleydriver = 1
        self.ymotor_pulleydriven = 1
        self.ymotor_wormdriver = 1
        self.ymotor_wormdriven = 1
        self.yencoder_pulleydriver = 1
        self.yencoder_pulleydriven = 1
        self.yencoder_wormdriver = 1
        self.yencoder_wormdriven = 1
        self.ymotor_leadscrew = 5
        self.yencoder_leadscrew = 5
        self.yencodercounts = 4000
        self.yusecomp = 0
        self.ycompfilename = "ycompensation"
        self.ycomptype = 0
        self.yusebacklash = 0
        self.ybacklash = 0
        self.ymaxvel = 1.67
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
        self.ybias = 0
        self.ydeadband = 0
        self.ysteptime = 1000
        self.ystepspace = 1000
        self.ydirhold = 1000
        self.ydirsetup = 1000
        self.yminferror = 0.125
        self.ymaxferror = 0.250
        self.yhomepos = 0
        self.yminlim =  0
        self.ymaxlim =  8
        self.yhomesw =  0
        self.yhomesearchvel = .05
        self.yhomelatchvel = .025
        self.yhomefinalvel = 0
        self.ysearchdir = 0
        self.ylatchdir = 0
        self.yusehomeindex = 0
        self.ystepscale = 0
        self.yencoderscale = 0
   
        # axis z data
        self.zdrivertype = "custom"     
        self.zsteprev = 200
        self.zmicrostep = 5
        self.zmotor_pulleydriver = 1
        self.zmotor_pulleydriven = 1
        self.zmotor_wormdriver = 1
        self.zmotor_wormdriven = 1
        self.zencoder_pulleydriver = 1
        self.zencoder_pulleydriven = 1
        self.zencoder_wormdriver = 1
        self.zencoder_wormdriven = 1
        self.zmotor_leadscrew = 5
        self.zencoder_leadscrew = 5
        self.zencodercounts = 1000
        self.zusecomp = 0
        self.zcompfilename = "zcompensation"
        self.zcomptype = 0
        self.zusebacklash = 0
        self.zbacklash = 0
        self.zmaxvel = 1.67
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
        self.zbias = 0
        self.zdeadband = 0
        self.zsteptime = 1000
        self.zstepspace = 1000
        self.zdirhold = 1000
        self.zdirsetup = 1000
        self.zminferror = 0.0005
        self.zmaxferror = 0.005
        self.zhomepos = 0
        self.zminlim = -4
        self.zmaxlim =  0
        self.zhomesw = 0
        self.zhomesearchvel = .05
        self.zhomelatchvel = .025
        self.zhomefinalvel = 0
        self.zsearchdir = 0
        self.zlatchdir = 0
        self.zusehomeindex = 0
        self.zstepscale = 0
        self.zencoderscale = 0


        # axis a data
        self.adrivertype = "custom"
        self.asteprev = 200
        self.amicrostep = 5
        self.amotor_pulleydriver = 1
        self.amotor_pulleydriven = 1
        self.amotor_wormdriver = 1
        self.amotor_wormdriven = 1
        self.aencoder_pulleydriver = 1
        self.aencoder_pulleydriven = 1
        self.aencoder_wormdriver = 1
        self.aencoder_wormdriven = 1
        self.amotor_leadscrew = 8
        self.aencoder_leadscrew = 8
        self.aencodercounts = 1000
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
        self.abias = 0
        self.adeadband = 0
        self.asteptime = 1000
        self.astepspace = 1000
        self.adirhold = 1000
        self.adirsetup = 1000
        self.aminferror = 0.0005
        self.amaxferror = 0.005
        self.ahomepos = 0
        self.aminlim = -9999
        self.amaxlim =  9999
        self.ahomesw =  0
        self.ahomesearchvel = .05
        self.ahomelatchvel = .025
        self.ahomefinalvel = 0
        self.asearchdir = 0
        self.alatchdir = 0
        self.ausehomeindex = 0
        self.astepscale = 0
        self.aencoderscale = 0

        # axis s (spindle) data
        self.sdrivertype = "custom"
        self.ssteprev = 200
        self.smicrostep = 5
        self.smotor_pulleydriver = 1
        self.smotor_pulleydriven = 1
        self.smotor_wormdriver = 1
        self.smotor_wormdriven = 1
        self.sencoder_pulleydriver = 1
        self.sencoder_pulleydriven = 1
        self.sencoder_wormdriver = 1
        self.sencoder_wormdriven = 1
        self.smotor_leadscrew = 5
        self.sencoder_leadscrew = 5
        self.sencodercounts = 1000
        self.smaxvel = 1.67
        self.smaxacc = 2
        self.sinvertmotor = 0
        self.sinvertencoder = 0
        self.sscale = 0
        self.soutputscale = 1
        self.soutputoffset = 0
        self.smaxoutput = 10
        self.sP = 1.0
        self.sI = 0
        self.sD = 0
        self.sFF0 = 0
        self.sFF1 = 0
        self.sFF2 = 0
        self.sbias = 0
        self.sdeadband = 0
        self.ssteptime = 1000
        self.sstepspace = 1000
        self.sdirhold = 1000
        self.sdirsetup = 1000
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
        self.sstepscale = 0
        self.sencoderscale = 0
        self.spidcontrol = False

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
        
        # this loads custom signal names created by the user
        # adds endings to the custom signal name when put in
        # hal signal name arrays
        #encoders
        temp =[]; i = False
        for i in  self.halencoderinputsignames:
            temp.append(i)
            for j in(["-a","-b","-i","-m"]):
                hal_encoder_input_names.append(i+j)
        if i:  human_encoder_input_names[4][1]= temp
        #pwm
        temp =[]; i = False
        for i in  self.halpwmoutputsignames:
            temp.append(i)
            for j in(["-pulse","-dir","-enable"]):
                hal_pwm_output_names.append(i+j)
        if i: human_pwm_output_names[6][1]= temp
        # TODO add tppwm

        # GPIO Input
        temp = []; i = False
        for i in  self.halinputsignames:
            temp.append(i)
            hal_input_names.append(i)
        if i: human_input_names[11][1]= temp
        # GPIO Output
        temp = []; i = False
        for i in  self.haloutputsignames:
            temp.append(i)
            hal_output_names.append(i)
        if i: human_output_names[5][1]= temp
        # steppers
        temp = []; i = False
        for i in  self.halsteppersignames:
            temp.append(i)
            for j in(["-step","-dir","-c","-d","-e","-f"]):
                hal_stepper_names.append(i+j)
        if i: human_stepper_names[6][1]= temp

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
        print >>file, "DEBUG = 0"

        print >>file
        print >>file, "[DISPLAY]"
        if self.frontend == _AXIS:
            print >>file, "DISPLAY = axis"
        elif self.frontend == _TKEMC:
            print >>file, "DISPLAY = tkemc"
        elif self.frontend == _MINI:
            print >>file, "DISPLAY = mini"
        elif self.frontend == _TOUCHY:
            print >>file, "DISPLAY = touchy"
        if self.position_offset == 1: temp ="RELATIVE"
        else: temp = "MACHINE"
        print >>file, "POSITION_OFFSET = %s"% temp
        if self.position_feedback == 1: temp ="ACTUAL"
        else: temp = "COMMANDED"
        print >>file, "POSITION_FEEDBACK = %s"% temp
        print >>file, "MAX_FEED_OVERRIDE = %f"% self.max_feed_override
        print >>file, "MAX_SPINDLE_OVERRIDE = %f"% self.max_spindle_override
        print >>file, "MIN_SPINDLE_OVERRIDE = %f"% self.min_spindle_override
        print >>file, "INTRO_GRAPHIC = emc2.gif"
        print >>file, "INTRO_TIME = 5"
        print >>file, "PROGRAM_PREFIX = %s" % \
                                    os.path.expanduser("~/emc2/nc_files")
        if self.pyvcp:
            print >>file, "PYVCP = custompanel.xml"
        # these are for AXIS GUI only
        if self.units == _METRIC:
            print >>file, "INCREMENTS = %s"% self.increments_metric
        else:
            print >>file, "INCREMENTS = %s"% self.increments_imperial
        if self.axes == 2:
            print >>file, "LATHE = 1"
        if self.position_offset:
            temp = "RELATIVE"
        else:
            temp = "MACHINE"
        print >>file, "POSITION_OFFSET = %s"% temp
        if self.position_feedback:
            temp = "ACTUAL"
        else:
            temp = "COMMANDED"
        print >>file, "POSITION_FEEDBACK = %s"% temp
        print >>file, "DEFAULT_LINEAR_VELOCITY = %f"% self.default_linear_velocity
        print >>file, "MAX_LINEAR_VELOCITY = %f"% self.max_linear_velocity
        print >>file, "MIN_LINEAR_VELOCITY = %f"% self.min_linear_velocity
        print >>file, "DEFAULT_ANGULAR_VELOCITY = %f"% self.default_angular_velocity
        print >>file, "MAX_ANGULAR_VELOCITY = %f"% self.max_angular_velocity
        print >>file, "MIN_ANGULAR_VELOCITY = %f"% self.min_angular_velocity
        print >>file, "EDITOR = %s"% self.editor
        print >>file, "GEOMETRY = %s"% self.geometry 

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

        #base_period = self.ideal_period()

        print >>file
        print >>file, "[EMCMOT]"
        print >>file, "EMCMOT = motmod"
        print >>file, "COMM_TIMEOUT = 1.0"
        print >>file, "COMM_WAIT = 0.010"
        #print >>file, "BASE_PERIOD = %d" % self.baseperiod
        print >>file, "SERVO_PERIOD = %d" % self.servoperiod
        print >>file
        print >>file, "# [HOSTMOT2]"
        print >>file, "# This is for info only"
        print >>file, "# DRIVER0=%s"% self.mesa0_currentfirmwaredata[_HALDRIVER]
        print >>file, "# BOARD0=%s"% self.mesa0_currentfirmwaredata[_BOARDNAME]
        print >>file, """# CONFIG0="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_3pwmgens=%d num_stepgens=%d" """ % (
                    self.mesa0_boardtitle, self.mesa0_firmware, self.mesa0_numof_encodergens, 
                    self.mesa0_numof_pwmgens, self.mesa0_numof_tppwmgens, self.mesa0_numof_stepgens )
        if self.number_mesa == 2:
            print >>file, "# DRIVER1=%s" % self.mesa1_currentfirmwaredata[_HALDRIVER]
            print >>file, "# BOARD1=%s"% self.mesa1_currentfirmwaredata[_BOARDNAME]
            print >>file, """# CONFIG1="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_3pwmgens=%d num_stepgens=%d" """ % (
                     self.mesa1_boardtitle, self.mesa1_firmware, self.mesa1_numof_encodergens, 
                     self.mesa1_numof_pwmgens, self.mesa1_numof_tppwmgens, self.mesa1_numof_stepgens )
        print >>file
        print >>file, "[HAL]"
        print >>file, "HALUI = halui"          
        print >>file, "HALFILE = %s.hal" % self.machinename
        if self.customhal:
            print >>file, "HALFILE = custom.hal"
        if self.pyvcp or self.customhal:
            print >>file, "POSTGUI_HALFILE = custom_postgui.hal"
        print >>file, "SHUTDOWN = shutdown.hal"
        print >>file
        print >>file, "[HALUI]"          
        if self.halui == True:
            for i in range(1,16):
                cmd =self["halui_cmd" + str(i)]
                if cmd =="": break
                print >>file,"MDI_COMMAND = %s"% cmd           

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
        if self.units == _METRIC:
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
        if self.restore_joint_position:
            print >>file, "POSITION_FILE = position.txt"
        if not self.require_homing:
            print >>file, "NO_FORCE_HOMING = 1"
        print >>file
        print >>file, "[EMCIO]"
        print >>file, "EMCIO = io"
        print >>file, "CYCLE_TIME = 0.100"
        print >>file, "TOOL_TABLE = tool.tbl"
        if self.allow_spindle_on_toolchange:
            print >>file, "TOOLCHANGE_WITH_SPINDLE_ON = 1"
        if self.raise_z_on_toolchange:
            print >>file, "TOOLCHANGE_QUILL_UP = 1"
        if self.random_toolchanger:
            print >>file, "RANDOM_TOOLCHANGER = 1"
        
        all_homes = self.home_sig("x") and self.home_sig("z")
        if self.axes != 2: all_homes = all_homes and self.home_sig("y")
        if self.axes == 4: all_homes = all_homes and self.home_sig("a")

        self.write_one_axis(file, 0, "x", "LINEAR", all_homes)
        if self.axes != 2:
            self.write_one_axis(file, 1, "y", "LINEAR", all_homes)
        self.write_one_axis(file, 2, "z", "LINEAR", all_homes)
        if self.axes == 1:
            self.write_one_axis(file, 3, "a", "ANGULAR", all_homes)
        self.write_one_axis(file, 9, "s", "null", all_homes)

        file.close()
        self.add_md5sum(filename)

    def write_one_axis(self, file, num, letter, type, all_homes):
        order = "1203"
        def get(s): return self[letter + s]       
        pwmgen = self.pwmgen_sig(letter)
        stepgen = self.stepgen_sig(letter)
        encoder = self.encoder_sig(letter)
        closedloop = False
        if stepgen and encoder: closedloop = True
        if letter == "s" and self.spidcontrol == True: closedloop = True
        if not letter == "s" and pwmgen and encoder: closedloop = True
        print "INI ",letter + " is closedloop? "+ str(closedloop),encoder

        print >>file
        print >>file, "#********************"
        if letter == 's':
            print >>file, "# Spindle "
            print >>file, "#********************"
            print >>file, "[SPINDLE_%d]" % num
        else:
            print >>file, "# Axis %s" % letter.upper()
            print >>file, "#********************"
            print >>file, "[AXIS_%d]" % num
            print >>file, "TYPE = %s" % type
            print >>file, "HOME = %s" % get("homepos")
            print >>file, "FERROR = %s"% get("maxferror")
            print >>file, "MIN_FERROR = %s" % get("minferror")
        print >>file, "MAX_VELOCITY = %s" % get("maxvel")
        print >>file, "MAX_ACCELERATION = %s" % get("maxacc")
        if encoder:
            if closedloop:
                print >>file, "P = %s" % get("P")
                print >>file, "I = %s" % get("I") 
                print >>file, "D = %s" % get("D")
                print >>file, "FF0 = %s" % get("FF0")
                print >>file, "FF1 = %s" % get("FF1")
                print >>file, "FF2 = %s" % get("FF2")
                print >>file, "BIAS = %s"% get("bias") 
                print >>file, "DEADBAND = %s"% get("deadband")
            if get("invertencoder"):
                temp = -1
            else: temp = 1
            print >>file, "ENCODER_SCALE = %s" % (get("encoderscale") * temp)
        if pwmgen:
            if get("invertmotor"):
                temp = -1
            else: temp = 1
            print >>file, "OUTPUT_SCALE = %s" % (get("outputscale") * temp)
            print >>file, "OUTPUT_OFFSET = %s" % get("outputoffset")
            print >>file, "MAX_OUTPUT = %s" % get("maxoutput")

        if stepgen:
            print >>file, "# these are in nanoseconds"
            print >>file, "DIRSETUP   = %d"% int(get("dirsetup"))
            print >>file, "DIRHOLD    = %d"% int(get("dirhold"))
            print >>file, "STEPLEN    = %d"% int(get("steptime"))          
            print >>file, "STEPSPACE  = %d"% int(get("stepspace"))
            if get("invertmotor"):
                temp = -1
            else: temp = 1
            print >>file, "STEP_SCALE = %s"% (get("stepscale") * temp)
        if letter == 's':return
        if self[letter + "usecomp"]:
            print >>file, "COMP_FILE = %s" % get("compfilename")
            print >>file, "COMP_FILE_TYPE = %s" % get("comptype")
        if self[letter + "usebacklash"]:
            print >>file, "BACKLASH = %s" % get("backlash")
        # emc2 doesn't like having home right on an end of travel,
        # so extend the travel limit by up to .01in or .1mm
        minlim = -abs(get("minlim"))
        maxlim = get("maxlim")
        home = get("homepos")
        if self.units == _METRIC: extend = .01
        else: extend = .001
        minlim = min(minlim, home - extend)
        maxlim = max(maxlim, home + extend)
        print >>file, "MIN_LIMIT = %s" % minlim
        print >>file, "MAX_LIMIT = %s" % maxlim

        thisaxishome = set(("all-home", "home-" + letter, "min-home-" + letter, "max-home-" + letter, "both-home-" + letter))
        ignore = set(("min-home-" + letter, "max-home-" + letter, "both-home-" + letter))
        homes = False
        for i in thisaxishome:
            if self.findsignal(i): homes = True
        # set homing speeds and directions
        # search direction : True = positive direction
        # latch direction :  True = opposite direction
        if homes:
            searchvel = abs(get("homesearchvel"))
            latchvel = abs(get("homelatchvel"))
            print get("searchdir")
            if get("searchdir") == 0:
                 searchvel = -searchvel
                 if get("latchdir") == 0: 
                    latchvel = -latchvel 
            else:
                if get("latchdir") == 1: 
                    latchvel = -latchvel
            print >>file, "HOME_OFFSET = %f" % get("homesw")
            print >>file, "HOME_SEARCH_VEL = %f" % searchvel                      
            print >>file, "HOME_LATCH_VEL = %f" % latchvel
            print >>file, "HOME_FINAL_VEL = %f" % get("homefinalvel")
            if get("usehomeindex"):useindex = "YES"
            else: useindex = "NO"   
            print >>file, "HOME_USE_INDEX = %s" % useindex
            for i in ignore:
                if self.findsignal(i):
                    print >>file, "HOME_IGNORE_LIMITS = YES"
                    break
            if all_homes and not self.individual_homing:
                print >>file, "HOME_SEQUENCE = %s" % order[num]
        else:
            print >>file, "HOME_OFFSET = %s" % get("homepos")

    def home_sig(self, axis):
        thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis, "max-home-" + axis, "both-home-" + axis))
        for i in thisaxishome:
            if self.findsignal(i): return i
        return None

    def min_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "min-" + axis,"min-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               if self.findsignal(i): return i
           return None

    def max_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "max-" + axis, "max-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               if self.findsignal(i): return i
           return None

    def stepgen_sig(self, axis):
           thisaxisstepgen =  axis + "-stepgen-step" 
           test = self.findsignal(thisaxisstepgen)
           return test

    def stepgen_invert_pins(self,pinnumber):
        signallist = []
        pin = int(pinnumber[10:])
        connector = int(pinnumber[6:7])
        boardnum = int(pinnumber[4:5])
        pinlist = self.list_related_pins([STEPA,STEPB], boardnum, connector, pin, 0)
        print pinlist
        for i in pinlist:
            if self[i[0]+"inv"]:
                gpioname = self.make_pinname(self.findsignal( self[i[0]] ),True)
                print gpioname
                signallist.append(gpioname)
        return signallist

    def encoder_sig(self, axis): 
           thisaxisencoder = axis +"-encoder-a"
           test = self.findsignal(thisaxisencoder)
           return test

    def pwmgen_sig(self, axis):
           thisaxispwmgen =  axis + "-pwm-pulse" 
           test = self.findsignal( thisaxispwmgen)
           return test

    def tppwmgen_sig(self, axis):
           thisaxispwmgen =  axis + "-tppwm-a" 
           test = self.findsignal(thisaxispwmgen)
           return test

    def connect_axis(self, file, num, let):
        axnum = "xyzabcuvws".index(let)
        title = 'AXIS'
        if let == 's':
            title = 'SPINDLE'
        closedloop = False
        pwmpinname = self.make_pinname(self.pwmgen_sig(let))
        tppwmpinname = self.make_pinname(self.tppwmgen_sig(let))
        steppinname = self.make_pinname(self.stepgen_sig(let))
        if steppinname:
            stepinvertlist = self.stepgen_invert_pins(self.stepgen_sig(let))
        encoderpinname = self.make_pinname(self.encoder_sig(let))
        if steppinname and encoderpinname: closedloop = True
        if let == "s" and self.spidcontrol == True: closedloop = True
        if not let == "s" and encoderpinname and encoderpinname: closedloop = True
        print let + " is closedloop? "+ str(closedloop),encoderpinname

        lat = self.latency
        print >>file, "#*******************"
        print >>file, "#  %s %s" % (title, let.upper())
        print >>file, "#*******************"
        print >>file
         
        if closedloop:
            if closedloop:
                print >>file, "setp   pid.%s.Pgain     [%s_%d]P" % (let, title, axnum)
                print >>file, "setp   pid.%s.Igain     [%s_%d]I" % (let, title, axnum)
                print >>file, "setp   pid.%s.Dgain     [%s_%d]D" % (let, title, axnum)
                print >>file, "setp   pid.%s.bias      [%s_%d]BIAS" % (let, title, axnum)
                print >>file, "setp   pid.%s.FF0       [%s_%d]FF0" % (let, title, axnum)
                print >>file, "setp   pid.%s.FF1       [%s_%d]FF1" % (let, title, axnum)
                print >>file, "setp   pid.%s.FF2       [%s_%d]FF2" % (let, title, axnum)
                print >>file, "setp   pid.%s.deadband  [%s_%d]DEADBAND" % (let, title, axnum)
                print >>file, "setp   pid.%s.maxoutput [%s_%d]MAX_OUTPUT" % (let, title, axnum)
                print >>file
                if let == 's':
                    name = "spindle"
                else:
                    name = let
                print >>file, "net %s-index-enable  <=>  pid.%s.index-enable" % (name, let)
                print >>file

            if tppwmpinname:
                print >>file, "# ---TPPWM Generator signals/setup---"
                print >>file, "# TODO write some commands!"
                print >>file

            if pwmpinname:
                print >>file, "# ---PWM Generator signals/setup---"
                print >>file
                print >>file, "setp   "+pwmpinname+".output-type 1" 
                print >>file, "setp   "+pwmpinname+".scale  [%s_%d]OUTPUT_SCALE"% (title, axnum)
                print >>file
                if let == 's':  
                    
                    x1 = self.spindlepwm1
                    x2 = self.spindlepwm2
                    y1 = self.spindlespeed1
                    y2 = self.spindlespeed2
                    scale = (y2-y1) / (x2-x1)
                    offset = x1 - y1 / scale
                    print >>file
                    
                    #print >>file, "    setp pwmgen.0.pwm-freq %s" % self.spindlecarrier        
                    #print >>file, "    setp pwmgen.0.scale %s" % scale
                    #print >>file, "    setp pwmgen.0.offset %s" % offset
                    #print >>file, "    setp pwmgen.0.dither-pwm true"
                    if self.spidcontrol == True:
                        print >>file, "net spindle-vel-cmd     => pid.%s.command" % (let)
                        print >>file, "net spindle-output     pid.%s.output      => "% (let) + pwmpinname + ".value"
                        print >>file, "net spindle-enable      => pid.%s.enable" % (let)
                        print >>file, "net spindle-enable      => " + pwmpinname +".enable"
                        print >>file, "net spindle-vel-fb      => pid.%s.feedback"% (let)    
                    else:
                        print >>file, "net spindle-vel-cmd     => " + pwmpinname + ".value"
                        print >>file, "net spindle-enable      => " + pwmpinname +".enable"
                else:
                    print >>file, "net %senable     => pid.%s.enable" % (let, let)
                    print >>file, "net %soutput     pid.%s.output           => "% (let, let) + pwmpinname + ".value"
                    print >>file, "net %spos-cmd    axis.%d.motor-pos-cmd   => pid.%s.command" % (let, axnum , let)
                    print >>file, "net %senable     axis.%d.amp-enable-out  => "% (let,axnum) + pwmpinname +".enable"

                print >>file    
        if steppinname:
            print >>file, "# Step Gen signals/setup"
            print >>file
            print >>file, "setp   " + steppinname + ".dirsetup        [%s_%d]DIRSETUP"% (title, axnum)
            print >>file, "setp   " + steppinname + ".dirhold         [%s_%d]DIRHOLD"% (title, axnum)
            print >>file, "setp   " + steppinname + ".steplen         [%s_%d]STEPLEN"% (title, axnum)
            print >>file, "setp   " + steppinname + ".stepspace       [%s_%d]STEPSPACE"% (title, axnum)
            print >>file, "setp   " + steppinname + ".position-scale  [%s_%d]STEP_SCALE"% (title, axnum)
            print >>file, "setp   " + steppinname + ".step_type        0"
            if closedloop or let == "s":
                print >>file, "setp   " + steppinname + ".control-type     1"
            else:
                print >>file, "setp   " + steppinname + ".control-type     0"
            if let =="s":
                print >>file, "setp   " + steppinname + ".maxaccel         [%s_%d]MAX_ACCELERATION"% (title, axnum)
                print >>file, "setp   " + steppinname + ".maxvel           [%s_%d]MAX_VELOCITY"% (title, axnum)
            else:
                print >>file, "setp   " + steppinname + ".maxaccel         0"
                print >>file, "setp   " + steppinname + ".maxvel           0"
            if let == "s":
                print >>file
                print >>file, "net spindle-enable          =>  " + steppinname + ".enable" 
                print >>file, "net spindle-vel-cmd-rps     =>  "+ steppinname + ".velocity-cmd"
                if not encoderpinname:
                    print >>file, "net spindle-vel-fb         <=  "+ steppinname + ".velocity-fb"
            elif closedloop:
                print >>file, "closedloop stepper signals"
                print >>file, "net %senable                             => pid.%s.enable" % (let, let)
                print >>file, "net %spos-cmd    axis.%d.motor-pos-cmd   => pid.%s.command" % (let, axnum , let)
                print >>file, "net %soutput     pid.%s.output           => "% (let, let) + steppinname + ".velocity-cmd"
                print >>file, "net %senable     axis.%d.amp-enable-out  => "% (let,axnum) + steppinname +".enable"
            else:
                print >>file
                print >>file, "net %spos-fb     axis.%d.motor-pos-fb   <=  "% (let, axnum) + steppinname + ".position-fb"
                print >>file, "net %spos-cmd    axis.%d.motor-pos-cmd  =>  "% (let, axnum) + steppinname + ".position-cmd"
                print >>file, "net %senable     axis.%d.amp-enable-out =>  "% (let, axnum) + steppinname + ".enable"
            for i in stepinvertlist:
                   print >>file, "setp    "+i+".invert_output true"
            print >>file

        if encoderpinname:             
                countmode = 0
                print >>file, "# ---Encoder feedback signals/setup---"
                print >>file             
                print >>file, "setp    "+encoderpinname+".counter-mode %d"% countmode
                print >>file, "setp    "+encoderpinname+".filter 1" 
                print >>file, "setp    "+encoderpinname+".index-invert 0"
                print >>file, "setp    "+encoderpinname+".index-mask 0" 
                print >>file, "setp    "+encoderpinname+".index-mask-invert 0"              
                print >>file, "setp    "+encoderpinname+".scale  [%s_%d]ENCODER_SCALE"% (title, axnum)
                print >>file
                if let == 's':
                    print >>file, "net spindle-revs              <=  " + encoderpinname + ".position"
                    print >>file, "net spindle-vel-fb            <=  " + encoderpinname + ".velocity"
                    print >>file, "net spindle-index-enable     <=>  " + encoderpinname + ".index-enable" 
               
                else: 
                    print >>file, "net %spos-fb               <=  "% (let) + encoderpinname+".position"
                    print >>file, "net %spos-fb               =>  pid.%s.feedback"% (let,let)
                    print >>file, "net %spos-fb               =>  axis.%d.motor-pos-fb" % (let, axnum)
                    print >>file, "net %s-index-enable    axis.%d.index-enable  <=>  "% (let, axnum) + encoderpinname + ".index-enable"
                print >>file

        if let =='s':
            print >>file, "# ---setup spindle control signals---" 
            print >>file
            print >>file, "net spindle-vel-cmd-rps    <=  motion.spindle-speed-out-rps"
            print >>file, "net spindle-vel-cmd        <=  motion.spindle-speed-out"
            print >>file, "net spindle-enable         <=  motion.spindle-on"
            print >>file, "net spindle-cw             <=  motion.spindle-forward"
            print >>file, "net spindle-ccw            <=  motion.spindle-reverse"
            print >>file, "net spindle-brake          <=  motion.spindle-brake"            
            print >>file, "net spindle-revs           =>  motion.spindle-revs"
            print >>file, "net spindle-at-speed       =>  motion.spindle-at-speed"
            print >>file, "net spindle-vel-fb         =>  motion.spindle-speed-in"
            print >>file, "net spindle-index-enable  <=>  motion.spindle-index-enable"
            if not self.findsignal("spindle-at-speed"):
                print >>file
                print >>file, "# ---Setup spindle at speed signals---"
                print >>file
                if encoderpinname:
                    print >>file, "net spindle-vel-cmd-rps    =>  near.0.in1"
                    print >>file, "net spindle-vel-fb         =>  near.0.in2"
                    print >>file, "net spindle-at-speed       <=  near.0.out"
                    print >>file, "setp near.0.scale .9"
                else:
                    print >>file, "sets spindle-at-speed true"
            return

        min_limsig = self.min_lim_sig(let)
        if not min_limsig: min_limsig = "%s-neg-limit" % let
        max_limsig = self.max_lim_sig(let)  
        if not max_limsig: max_limsig = "%s-pos-limit" % let 
        homesig = self.home_sig(let)
        if not homesig: homesig = "%s-home-sw" % let
        print >>file, "# ---setup home / limit switch signals---"       
        print >>file       
        print >>file, "net %s     =>  axis.%d.home-sw-in" % (homesig, axnum)       
        print >>file, "net %s     =>  axis.%d.neg-lim-sw-in" % (min_limsig, axnum)       
        print >>file, "net %s     =>  axis.%d.pos-lim-sw-in" % (max_limsig, axnum)
        print >>file                

    def connect_input(self, file):
        print >>file, "# external input signals"
        print >>file
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = self['pp1Ipin%d' % pin]
            i = self['pp1Ipin%dinv' % pin]
            if p == UNUSED_INPUT: continue
            if i: print >>file, "net %s     <= parport.0.pin-%02d-in-not" % (p, pin)
            else: print >>file, "net %s     <= parport.0.pin-%02d-in" % (p, pin)
        print >>file
        for boardnum in range(0,int(self.number_mesa)):
            for concount,connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                for pin in range(0,24):
                    p = self['mesa%dc%dpin%d' % (boardnum,connector, pin)]
                    i = self['mesa%dc%dpin%dinv' % (boardnum,connector, pin)]
                    t = self['mesa%dc%dpin%dtype' % (boardnum,connector, pin)]
                    truepinnum = pin + ((connector-2)*24)
                    # for input pins
                    if t == GPIOI:
                        if p == "unused-input":continue 
                        pinname = self.make_pinname(self.findsignal( p )) 
                        print >>file, "# ---",p.upper(),"---"
                        if i: print >>file, "net %s     <=  "% (p)+pinname +".in_not"
                        else: print >>file, "net %s     <=  "% (p)+pinname +".in"
                    # for encoder pins
                    elif t in (ENCA):
                        if p == "unused-encoder":continue
                        for sig in (self.halencoderinputsignames):
                            if p == sig+"-a":
                                pinname = self.make_pinname(self.findsignal( p ))
                                print >>file, "# ---",sig.upper(),"---"
                                print >>file, "net %s         <=  "% (sig+"-position")+pinname +".position"
                                print >>file, "net %s            <=  "% (sig+"-count")+pinname +".count"
                                print >>file, "net %s         <=  "% (sig+"-velocity")+pinname +".velocity"
                                print >>file, "net %s            <=  "% (sig+"-reset")+pinname +".reset"
                                print >>file, "net %s     <=  "% (sig+"-index-enable")+pinname +".index-enable"
                                break
                    else: continue

    def connect_output(self, file):
        
        print >>file, "# external output signals"
        print >>file
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = self['pp1Opin%d' % pin]
            i = self['pp1Opin%dinv' % pin]
            if p == UNUSED_OUTPUT: continue
            print >>file, "net %s     =>  parport.0.pin-%02d-out" % (p, pin)
            if i: print >>file, "setp    parport.0.pin-%02d-out-invert true" % pin           
        print >>file
        for boardnum in range(0,int(self.number_mesa)):
            for concount,connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                for pin in range(0,24):
                    p = self['mesa%dc%dpin%d' % (boardnum,connector, pin)]
                    i = self['mesa%dc%dpin%dinv' % (boardnum,connector, pin)]
                    t = self['mesa%dc%dpin%dtype' % (boardnum,connector, pin)]
                    #print "**** INFO output:",p,t,boardnum,connector,pin
                    truepinnum = pin + ((connector-2)*24)
                    # for output /open drain pins
                    if t in (GPIOO,GPIOD):
                        if p == "unused-output":continue
                        pinname = self.make_pinname(self.findsignal( p ))
                        print >>file, "# ---",p.upper(),"---"
                        print >>file, "setp    "+pinname +".is_output true"
                        if i: print >>file, "setp    "+pinname+".invert_output true"
                        if t == 2: print >>file, "setp    "+pinname+".is_opendrain  true"   
                        print >>file, "net %s     =>  "% (p)+pinname +".out"              
                    # for pwm pins
                    elif t in (PWMP,PDMP):
                        if p == "unused-pwm":continue
                        for sig in (self.halpwmoutputsignames):
                            if p == (sig+"-pulse"):
                                pinname = self.make_pinname(self.findsignal( p ))
                                print >>file, "# ---",sig.upper(),"---"
                                if t == PWMP:
                                    print >>file, "setp    "+pinname +".output-type 1"
                                elif t == PDMP:
                                    print >>file, "setp    "+pinname +".output-type 3"
                                print >>file, "net %s     <=  "% (sig+"-enable")+pinname +".enable"
                                print >>file, "net %s      <=  "% (sig+"-value")+pinname +".value"
                                break
                    # fot TP pwm pins
                    elif t == (TPPWMA):
                        if p == "unused-tppwmgen":continue
                        for sig in (self.halttpwmsignames):
                            if p == (sig+"-a"):
                                pinname = self.make_pinname(self.findsignal( p ),ini_style) 
                                print >>file, "# ---",sig.upper(),"---"
                                print >>file, "net %s           <=  "% (sig+"-enable")+pinname +".enable" 
                    # for stepper pins
                    elif t == (STEPA):
                        if p == "unused-stepgen":continue
                        for sig in (self.halsteppersignames):
                            if p == (sig+"-step"):
                                pinname = self.make_pinname(self.findsignal( p )) 
                                print >>file, "# ---",sig.upper(),"---"
                                print >>file, "net %s           <=  "% (sig+"-enable")+pinname +".enable"  
                                print >>file, "net %s            <=  "% (sig+"-count")+pinname +".counts" 
                                print >>file, "net %s     <=  "% (sig+"-cmd-position")+pinname +".position-cmd"  
                                print >>file, "net %s     <=  "% (sig+"-act-position")+pinname +".position-fb" 
                                print >>file, "net %s         <=  "% (sig+"-velocity")+pinname +".velocity-fb"
                                pinlist = self.list_related_pins([STEPA,STEPB], boardnum, connector, pin, 0)
                                print pinlist
                                for i in pinlist:
                                    if self[i[0]+"inv"]:
                                        gpioname = self.make_pinname(self.findsignal( self[i[0]] ),True)
                                        print gpioname
                                        print >>file, "setp    "+gpioname+".invert_output true"
                                break
                    else:continue

    def write_halfile(self, base):
        filename = os.path.join(base, self.machinename + ".hal")
        file = open(filename, "w")
        print >>file, _("# Generated by PNCconf at %s") % time.asctime()
        print >>file, _("# If you make changes to this file, they will be")
        print >>file, _("# overwritten when you run PNCconf again")
        print >>file
        print >>file, "loadrt trivkins"
        print >>file, "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[TRAJ]AXES"
        print >>file, "loadrt probe_parport"
        print >>file, "loadrt hostmot2"
        board0 = self.mesa0_currentfirmwaredata[_BOARDNAME]
        board1 = self.mesa1_currentfirmwaredata[_BOARDNAME]
        driver0 = self.mesa0_currentfirmwaredata[_HALDRIVER]
        driver1 = self.mesa1_currentfirmwaredata[_HALDRIVER]
        directory0 = self.mesa0_currentfirmwaredata[_DIRECTORY]
        directory1 = self.mesa1_currentfirmwaredata[_DIRECTORY]
        firm0 = self.mesa0_currentfirmwaredata[_FIRMWARE]
        firm1 = self.mesa1_currentfirmwaredata[_FIRMWARE]
        if self.number_mesa == 1:            
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d numof_tppwmgens%d num_stepgens=%d" """ % (
                    driver0, directory0, firm0, self.mesa0_numof_encodergens, self.mesa0_numof_pwmgens, self.mesa0_numof_tppwmgens, self.mesa0_numof_stepgens )
        elif self.number_mesa == 2 and (driver0 == driver1):
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d numof_tppwmgens%d num_stepgens=%d,firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"
                    """ % (
                    driver0, directory0, firm0, self.mesa0_numof_encodergens, self.mesa0_numof_pwmgens, self.mesa0_numof_stepgens,
                    directory1, firm1, self.mesa1_numof_encodergens, self.mesa1_numof_pwmgens, self.mesa1_numof_tppwmgens, self.mesa1_numof_stepgens )
        elif self.number_mesa == 2:
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d numof_tppwmgens%d num_stepgens=%d" """ % (
                    driver0, directory0, firm0, self.mesa0_numof_encodergens, self.mesa0_numof_pwmgens, self.mesa0_numof_tppwmgens,self.mesa0_numof_stepgens )
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d numof_tppwmgens%d num_stepgens=%d" """ % (
                    driver1, directory1, firm1, self.mesa1_numof_encodergens, self.mesa1_numof_pwmgens, self.mesa0_numof_tppwmgens,self.mesa1_numof_stepgens )
        for boardnum in range(0,int(self.number_mesa)):
            if boardnum == 1 and (board0 == board1):
                halnum = 1
            else:
                halnum = 0
            if self["mesa%d_numof_pwmgens"% boardnum] > 0:
                print >>file, "setp     hm2_%s.%d.pwmgen.pwm_frequency %d"% ( self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum, self["mesa%d_pwm_frequency"% boardnum] )
                print >>file, "setp     hm2_%s.%d.pwmgen.pdm_frequency %d"% ( self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum,self["mesa%d_pdm_frequency"% boardnum] )
            print >>file, "setp     hm2_%s.%d.watchdog.timeout_ns %d"% ( self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum,self["mesa%d_watchdog_timeout"% boardnum] )

        if self.number_pports>0:
            port3name = port2name = port1name = port3dir = port2dir = port1dir = ""
            if self.number_pports>2:
                 port3name = " " + self.ioaddr3
                 if self.pp3_direction:
                    port3dir =" out"
                 else: 
                    port3dir =" in"
            if self.number_pports>1:
                 port2name = " " + self.ioaddr2
                 if self.pp2_direction:
                    port2dir =" out"
                 else: 
                    port2dir =" in"
            port1name = self.ioaddr
            if self.pp1_direction:
               port1dir =" out"
            else: 
               port1dir =" in"
            print >>file, "loadrt hal_parport cfg=\"%s%s%s%s%s%s\"" % (port1name, port1dir, port2name, port2dir, port3name, port3dir)

        if self.joystickjog:
            print >>file, "loadusr -W hal_input -KRAL %s\n"% self.usbdevicename

        spindle_enc = counter = probe = pwm = pump = estop = False 
        enable = spindle_on = spindle_cw = spindle_ccw = False
        mist = flood = brake = at_speed = False

        if self.findsignal("s-encoder-a"):
            spindle_enc = True        
        if self.findsignal("probe"):
            probe = True
        if self.findsignal("s-pwm-pulse"):
            pwm = True
        if self.findsignal("charge-pump"):
            pump = True
        if self.findsignal("estop-ext"):
            estop = True
        if self.findsignal("enable"):
            enable = True
        if self.findsignal("spindle-enable"):
            spindle_on = True
        if self.findsignal("spindle-cw"):
            spindle_cw = True
        if self.findsignal("spindle-ccw"):
            spindle_ccw = True
        if self.findsignal("coolant-mist"):
            mist = True
        if self.findsignal("coolant-flood"):
            flood = True
        if self.findsignal("spindle-brake"):
            brake = True
        if self.findsignal("spindle-at-speed"):
            at_speed = True

        if self.pyvcp or self.userneededabs >0:
            self.absnames=""
            if self.pyvcphaltype == 1 and self.pyvcpconnect == 1 and self.pyvcp:
                self.absnames=self.absnames+"abs.spindle"
                if self.userneededabs >0:
                    self.absnames=self.absnames+","
            for i in range(0,self.userneededabs):
                self.absnames = self.absnames+"abs.%d"% (i)
                if i <> self.userneededabs-1:
                    self.absnames = self.absnames+","
            print >>file, "loadrt abs names=%s"% self.absnames

        if self.pyvcp or self.userneededlowpass >0:
            self.lowpassnames=""
            for i in range(0,self.userneededlowpass):
                self.lowpassnames = self.lowpassnames+"lowpass.%d,"% (i)
            if self.pyvcphaltype == 1 and self.pyvcpconnect == 1 and self.pyvcp:
                self.lowpassnames=self.lowpassnames+"lowpass.spindle"
            temp = self.lowpassnames.rstrip(",")
            print >>file, "loadrt lowpass names=%s"% temp

        if self.pyvcp and self.pyvcphaltype == 1 and self.pyvcpconnect == 1 and spindle_enc or self.userneededscale >0:
            self.scalenames=""
            if spindle_enc and self.pyvcp:
                self.scalenames=self.scalenames+"scale.spindle"
                if self.userneededscale >0:
                    self.scalenames=self.scalenames+","
            for i in range(0,self.userneededscale):
                self.scalenames = self.scalenames+"scale.%d"% (i)
                if  i <> self.userneededscale-1:
                    self.scalenames = self.scalenames+","
            print >>file, "loadrt scale names=%s"% self.scalenames
        if pump:
            print >>file, "loadrt charge_pump"
        if not at_speed:
            print >>file, "loadrt near"
        if self.classicladder:
            print >>file, "loadrt classicladder_rt numPhysInputs=%d numPhysOutputs=%d numS32in=%d numS32out=%d numFloatIn=%d numFloatOut=%d" %(self.digitsin , self.digitsout , self.s32in, self.s32out, self.floatsin, self.floatsout)
        
        if self.externalmpg or self.externalfo or self.externalso or self.joystickjog or self.userneededmux16 > 0:
            self.mux16names=""
            for i in range(0,self.userneededmux16):
                self.mux16names = self.mux16names+"%d,"% (i)
            if self.joystickjog: 
                self.mux16names = self.mux16names+"jogspeed,"
            if self.externalmpg: 
                self.mux16names = self.mux16names+"jogincr,"  
            if self.externalfo: 
                self.mux16names = self.mux16names+"foincr," 
            if self.externalso: 
                self.mux16names = self.mux16names+"soincr,"
            temp = self.mux16names.rstrip(",")
            self.mux16names = temp
            print >>file, "loadrt mux16 names=%s"% (self.mux16names)

        # load user custom components
        for i in self.loadcompbase:
            if i == '': continue
            else:              
                print >>file, i 
        for i in self.loadcompservo:
            if i == '': continue
            else:              
                print >>file, i 

        if self.pyvcp and not self.frontend == _AXIS:
            print >>file, "loadusr -Wn custompanel pyvcp -c custompanel [DISPLAY](PYVCP)"
        
        print >>file
        if self.number_pports > 0:
            print >>file, "addf parport.0.read servo-thread"
        if self.number_pports > 1:
            print >>file, "addf parport.1.read servo-thread"
        if self.number_pports > 2:
            print >>file, "addf parport.2.read servo-thread"
 
        if pump: print >>file, "addf charge-pump servo-thread"
           
        for i in self.addcompbase:
            if not i == '':
                print >>file, i +" base-thread"

        if self.number_pports > 0:
            print >>file, "addf parport.0.write servo-thread"         
        if self.number_pports > 1:
            print >>file, "addf parport.1.write servo-thread"
        if self.number_pports > 2:
            print >>file, "addf parport.2.write servo-thread"
        if self.number_mesa:
            for boardnum in range(0,int(self.number_mesa)):
                if boardnum == 1 and (self.mesa0_currentfirmwaredata[_BOARDNAME] == self.mesa1_currentfirmwaredata[_BOARDNAME]):
                    halnum = 1
                else:
                    halnum = 0
                if self.number_mesa> 0:
                    print >>file, "addf hm2_%s.%d.read servo-thread"% (self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum)
            
        print >>file, "addf motion-command-handler servo-thread"
        print >>file, "addf motion-controller servo-thread"
        temp = 0
        axislet = []
        for i in self.available_axes:
            #if axis needs pid- (has pwm)
            print "looking at available axis : ",i
            if not self.findsignal(i+"-encoder-a"): 
                continue 
            if (self.spidcontrol == False and i == 's') :   
                continue
            temp = temp +1 
            axislet.append(i)
            # add axis letter to 'need pid' string
            #if axis is needed
        temp = temp + self.userneededpid
        if temp <> 0 : 
            print >>file, "loadrt pid num_chan=%d"% temp          
            #use 'need pid string' to add calcs and make aliases 
            for j in range(0,temp ):
                print >>file, "addf pid.%d.do-pid-calcs servo-thread"% j
            for axnum,j in enumerate(axislet):
                print >>file, "alias pin    pid.%d.Pgain         pid.%s.Pgain" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.Igain         pid.%s.Igain" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.Dgain         pid.%s.Dgain" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.bias          pid.%s.bias" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.FF0           pid.%s.FF0" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.FF1           pid.%s.FF1" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.FF2           pid.%s.FF2" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.deadband      pid.%s.deadband" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.maxoutput     pid.%s.maxoutput" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.enable        pid.%s.enable" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.command       pid.%s.command" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.feedback      pid.%s.feedback" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.output        pid.%s.output" % (axnum + self.userneededpid, j)
                print >>file, "alias pin    pid.%d.index-enable  pid.%s.index-enable" % (axnum + self.userneededpid, j)
                print >>file
        if self.classicladder:
            print >>file,"addf classicladder.0.refresh servo-thread"

        if self.externalmpg or self.externalfo or self.externalso or self.joystickjog or self.userneededmux16 > 0: 
            temp=self.mux16names.split(",")
            for j in (temp):
                print >>file, "addf %s servo-thread"% j
        if self.pyvcp and self.pyvcphaltype == 1 and self.pyvcpconnect == 1 or self.userneededabs > 0:
            temp=self.absnames.split(",")
            for j in (temp):
                print >>file, "addf %s servo-thread"% j
        if self.pyvcp and self.pyvcphaltype == 1 and self.pyvcpconnect == 1 or self.userneededscale > 0:
            if spindle_enc or self.userneededscale > 0:
                temp=self.scalenames.split(",")
                for j in (temp):
                    print >>file, "addf %s servo-thread"% j
        if self.pyvcp and self.pyvcphaltype == 1 and self.pyvcpconnect == 1 or self.userneededlowpass > 0:
            temp=self.lowpassnames.split(",")
            for j in (temp):
                print >>file, "addf %s servo-thread"% j

        for i in self.addcompservo:
            if not i == '':
                print >>file, i +" servo-thread"
        if not at_speed:
            print >>file, "addf near.0                   servo-thread"
        if self.number_mesa:
            for boardnum in range(0,int(self.number_mesa)):
                if boardnum == 1 and (self.mesa0_currentfirmwaredata[_BOARDNAME] == self.mesa1_currentfirmwaredata[_BOARDNAME]):
                    halnum = 1
                else:
                    halnum = 0         
                print >>file, "addf hm2_%s.%d.write         servo-thread"% (self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum)
                print >>file, "addf hm2_%s.%d.pet_watchdog  servo-thread"% (self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum)
            
        print >>file
        self.connect_output(file)              
        print >>file
        self.connect_input(file)
        print >>file

        if self.axes == 2:
            self.connect_axis(file, 0, 'x')
            self.connect_axis(file, 1, 'z')
            self.connect_axis(file, 2, 's')
        elif self.axes == 0:
            self.connect_axis(file, 0, 'x')
            self.connect_axis(file, 1, 'y')
            self.connect_axis(file, 2, 'z')
            self.connect_axis(file, 3, 's')
        elif self.axes == 1:
            self.connect_axis(file, 0, 'x')
            self.connect_axis(file, 1, 'y')
            self.connect_axis(file, 2, 'z')
            self.connect_axis(file, 3, 'a')
            self.connect_axis(file, 4, 's')

        print >>file
        print >>file, "#******************************"
        print >>file, _("# connect miscellaneous signals") 
        print >>file, "#******************************"
        print >>file    
        if pump:    
            print >>file, _("#  ---charge pump signals---")
            print >>file, "net estop-out       =>  charge-pump.enable"
            print >>file, "net charge-pump     <=  charge-pump.out"
            print >>file
        print >>file, _("#  ---coolant signals---")
        print >>file
        print >>file, "net coolant-mist      <=  iocontrol.0.coolant-mist"
        print >>file, "net coolant-flood     <=  iocontrol.0.coolant-flood"
        print >>file
        print >>file, _("#  ---probe signal---")
        print >>file
        print >>file, "net probe-in     =>  motion.probe-input"
        print >>file
        if self.externaljog:
            print >>file, _("# ---jog button signals---")
            print >>file
            print >>file, "net jog-speed            halui.jog-speed "
            print >>file, "sets    jog-speed %f"% self.jograpidrate
            temp = ("x","y","z","a")
            if self.multijogbuttons:
                for axnum,axletter in enumerate(temp):
                    if axletter in self.available_axes:
                        print >>file, "net jog-%s-pos            halui.jog.%d.plus"% (axletter,axnum)
                        print >>file, "net jog-%s-neg            halui.jog.%d.minus"% (axletter,axnum)
            else:
                for axnum,axletter in enumerate(temp):
                    if axletter in self.available_axes:
                        print >>file, "net joint-select-%s         halui.joint.%d.select"% (chr(axnum+97),axnum)
                print >>file, "net jog-selected-pos     halui.jog.selected.plus"
                print >>file, "net jog-selected-neg     halui.jog.selected.minus"
            print >>file, "net spindle-manual-cw     halui.spindle.forward"
            print >>file, "net spindle-manual-ccw    halui.spindle.reverse"
            print >>file, "net spindle-manual-stop   halui.spindle.stop"
            print >>file

        if self.joystickjog:
            print >>file, _("# ---USB device jog button signals---")
            print >>file
            print >>file, "# connect selectable mpg jog speeds "
            print >>file, "net jog-speed-a           =>  jogspeed.sel0"
            print >>file, "net jog-speed-b           =>  jogspeed.sel1"
            print >>file, "net jog-speed             halui.jog-speed  <=  jogspeed.out-f"
            print >>file, "setp    jogspeed.in00          %f"% (self.joystickjograpidrate0)
            print >>file, "setp    jogspeed.in01          %f"% (self.joystickjograpidrate1)
            print >>file, "setp    jogspeed.in02          %f"% (self.joystickjograpidrate2)
            print >>file, "setp    jogspeed.in03          %f"% (self.joystickjograpidrate3)
            if not self.joycmdrapida =="":
                print >>file, "net jog-speed-a           <=  %s"% (self.joycmdrapida)
            if not self.joycmdrapidb =="":
                print >>file, "net jog-speed-b           <=  %s"% (self.joycmdrapidb)
            temp = ("x","y","z","a")
            for axnum,axletter in enumerate(temp):
                if axletter in self.available_axes:
                    pin_pos = self["joycmd"+axletter+"pos"]
                    pin_neg = self["joycmd"+axletter+"neg"]
                    if pin_pos == "" or pin_neg =="": continue
                    print >>file, "net jog-%s-pos            halui.jog.%d.plus"% (axletter,axnum)
                    print >>file, "net jog-%s-pos            %s"% (axletter,pin_pos)
                    print >>file, "net jog-%s-neg            halui.jog.%d.minus"% (axletter,axnum)
                    print >>file, "net jog-%s-neg            %s"% (axletter,pin_neg)
            print >>file

        pinname = self.make_pinname(self.findsignal("select-mpg-a"))
        if pinname:
            print >>file, "# ---jogwheel signals to mesa encoder - shared MPG---"
            print >>file
            print >>file, "net joint-selected-count     <=  %s.count"% (pinname)
            print >>file, "setp    %s.filter true" % pinname
            print >>file, "setp    %s.counter-mode true" % pinname
            print >>file
            if self.externalmpg:
                    print >>file, _("#  ---mpg signals---")
                    print >>file
                    if not self.multimpg:
                        temp = ("x","y","z","a")
                        for axnum,axletter in enumerate(temp):
                            if axletter in self.available_axes:
                                print >>file, "#       for axis %s MPG" % (axletter)
                                print >>file, "setp    axis.%d.jog-vel-mode 0" % axnum
                                print >>file, "net selected-jog-incr    =>  axis.%d.jog-scale" % (axnum)
                                print >>file, "net joint-select-%s       =>  axis.%d.jog-enable"% (chr(axnum+97),axnum)
                                print >>file, "net joint-selected-count =>  axis.%d.jog-counts"% (axnum)
                            print >>file
        temp = ("x","y","z","a")
        for axnum,axletter in enumerate(temp):
            if axletter in self.available_axes:
                pinname = self.make_pinname(self.findsignal(axletter+"-mpg-a"))
                if pinname:
                    print >>file, "# ---jogwheel signals to mesa encoder - %s axis MPG---"% axletter
                    print >>file
                    print >>file, "net %s-jog-count          <=  %s.count"% (axletter, pinname)
                    print >>file, "setp    %s.filter true" % pinname
                    print >>file, "setp    %s.counter-mode false" % pinname
                    print >>file
                    if self.externalmpg:
                        print >>file, _("#  ---mpg signals---")
                        print >>file
                        if self.multimpg:
                            print >>file, "setp    axis.%d.jog-vel-mode 0" % axnum
                            print >>file, "net %s-jog-enable    axis.%d.jog-enable"% (axletter, axnum)            
                            print >>file, "net %s-jog-count          =>  axis.%d.jog-counts" % (axletter, axnum)
                            print >>file, "net selected-jog-incr    =>  axis.%d.jog-scale" % (axnum)
                            print >>file, "sets %s-jog-enable    true"% (axletter)
                            print >>file
        if self.externalmpg and not self.frontend == _TOUCHY:# TOUCHY GUI sets its own jog increments:
            if self.incrselect :
                print >>file, "# connect selectable mpg jog increments "  
                print >>file, "net jog-incr-a           =>  jogincr.sel0"
                print >>file, "net jog-incr-b           =>  jogincr.sel1"
                print >>file, "net jog-incr-c           =>  jogincr.sel2"
                print >>file, "net jog-incr-d           =>  jogincr.sel3"
                print >>file, "net selected-jog-incr    <=  jogincr.out-f"
                print >>file, "    setp jogincr.suppress-no-input true"
                for i in range(0,16):
                    value = self["mpgincrvalue%d"% i]
                    print >>file, "    setp jogincr.in%02d          %f"% (i,value)
                print >>file
            else:
                print >>file, "sets selected-jog-incr     %f"% (self.mpgincrvalue0)

        pinname = self.make_pinname(self.findsignal("fo-mpg-a"))
        if pinname:
            print >>file, "# ---feed override signals to mesa encoder - mpg---"
            print >>file
            print >>file, "net fo-count     <=  %s.count"% (pinname)
            print >>file, "setp    %s.filter true" % pinname
            print >>file, "setp    %s.counter-mode true" % pinname
            print >>file
        if self.externalfo:
            if self.fo_usempg:
                print >>file, "# connect feed overide increments - MPG" 
                print >>file, "    setp halui.feed-override.count-enable true"
                print >>file, "    setp halui.feed-override.direct-value false"
                print >>file, "net fo-count =>  halui.feed-override.counts"
                print >>file, "    setp halui.feed-override.scale .01"
                print >>file
            elif self.fo_useswitch:
                print >>file, "# connect feed overide increments - switches" 
                print >>file, "    setp halui.feed-override.count-enable true"
                print >>file, "    setp halui.feed-override.direct_value true"
                print >>file, "net feedoverride-incr <=  halui.feed-override.counts"
                print >>file, "    setp halui.feed-override.scale .01"
                print >>file, "net fo-incr-a           =>  foincr.sel0"
                print >>file, "net fo-incr-b           =>  foincr.sel1"
                print >>file, "net fo-incr-c           =>  foincr.sel2"
                print >>file, "net fo-incr-d           =>  foincr.sel3"
                print >>file, "net feedoverride-incr   <=  foincr.out-s"
                print >>file, "    setp foincr.suppress-no-input true"
                for i in range(0,16):
                    value = self["foincrvalue%d"% i]
                    print >>file, "    setp foincr.in%02d          %f"% (i,value)
                print >>file

        pinname = self.make_pinname(self.findsignal("so-mpg-a"))
        if pinname:
            print >>file, "# ---spindle override signals to mesa encoder - mpg---"
            print >>file
            print >>file, "net so-count     <=  %s.count"% (pinname)
            print >>file, "setp    %s.filter true" % pinname
            print >>file, "setp    %s.counter-mode true" % pinname
            print >>file
        if self.externalso:
            if self.so_usempg:
                print >>file, "# connect spindle overide increments - MPG" 
                print >>file, "    setp halui.feed-override.count-enable true"
                print >>file, "    setp halui.feed-override.direct-value false"
                print >>file, "net so-count =>  halui.feed-override.counts"
                print >>file, "    setp halui.feed-override.scale .01"
                print >>file
            elif self.so_useswitch:
                print >>file, "# connect spindle overide increments " 
                print >>file, "    setp halui.spindle-override.count-enable true"
                print >>file, "    setp halui.spindle-override.direct_value true"
                print >>file, "net spindleoverride-incr <=  halui.spindle-override.counts"
                print >>file, "    setp halui.spindle-override.scale .01"
                print >>file, "net so-incr-a           =>  soincr.sel0"
                print >>file, "net so-incr-b           =>  soincr.sel1"
                print >>file, "net so-incr-c           =>  soincr.sel2"
                print >>file, "net so-incr-d           =>  soincr.sel3"
                print >>file, "net spindleoverride-incr   <=  soincr.out-s"
                print >>file, "    setp soincr.suppress-no-input true"
                for i in range(0,16):
                    value = self["soincrvalue%d"% i]
                    print >>file, "    setp soincr.in%02d          %f"% (i,value)
                print >>file

        print >>file, _("#  ---digital in / out signals---")
        print >>file
        for i in range(4):
            dout = "dout-%02d" % i
            if self.findsignal(dout):
                print >>file, "net %s     <=  motion.digital-out-%02d" % (dout, i)
        for i in range(4):
            din = "din-%02d" % i
            if self.findsignal(din):
                print >>file, "net %s     =>  motion.digital-in-%02d" % (din, i)
        print >>file, _("#  ---estop signals---")
        print >>file
        print >>file, "net estop-out     <=  iocontrol.0.user-enable-out"
        if  self.classicladder and self.ladderhaltype == 1 and self.ladderconnect: # external estop program
            print >>file
            print >>file, _("# **** Setup for external estop ladder program -START ****")
            print >>file
            print >>file, "net estop-out     => classicladder.0.in-00"
            print >>file, "net estop-ext     => classicladder.0.in-01"
            print >>file, "net estop-strobe     classicladder.0.in-02   <=  iocontrol.0.user-request-enable"
            print >>file, "net estop-outcl      classicladder.0.out-00  =>  iocontrol.0.emc-enable-in"
            print >>file
            print >>file, _("# **** Setup for external estop ladder program -END ****")
        elif estop:
            print >>file, "net estop-ext     =>  iocontrol.0.emc-enable-in"
        else:
            print >>file, "net estop-out     =>  iocontrol.0.emc-enable-in"
        if enable:
            print >>file, "net enable        =>  motion.motion-enabled"

        print >>file
        if self.toolchangeprompt:
            print >>file, _("#  ---manual tool change signals---")
            print >>file
            print >>file, "loadusr -W hal_manualtoolchange"
            print >>file, "net tool-change-request     iocontrol.0.tool-change       =>  hal_manualtoolchange.change"
            print >>file, "net tool-change-confirmed   iocontrol.0.tool-changed      <=  hal_manualtoolchange.changed"
            print >>file, "net tool-number             iocontrol.0.tool-prep-number  =>  hal_manualtoolchange.number"
            print >>file, "net tool-prepare-loopback   iocontrol.0.tool-prepare      =>  iocontrol.0.tool-prepared"
            print >>file
        else:
            print >>file, _("#  ---toolchange signals for custom tool changer---")
            print >>file
            print >>file, "net tool-number             <=  iocontrol.0.tool-prep-number"
            print >>file, "net tool-change-request     <=  iocontrol.0.tool-change"
            print >>file, "net tool-change-confirmed   =>  iocontrol.0.tool-changed" 
            print >>file, "net tool-prepare-request    <=  iocontrol.0.tool-prepare"
            print >>file, "net tool-prepare-confirmed  =>  iocontrol.0.tool-prepared" 
            print >>file
        if self.classicladder:
            print >>file
            if self.modbus:
                print >>file, _("# Load Classicladder with modbus master included (GUI must run for Modbus)")
                print >>file
                print >>file, "loadusr classicladder --modmaster custom.clp"
                print >>file
            else:
                print >>file, _("# Load Classicladder without GUI (can reload LADDER GUI in AXIS GUI")
                print >>file
                print >>file, "loadusr classicladder --nogui custom.clp"
                print >>file
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
            print >>f1, _("""# The commands in this file are run after the AXIS GUI (including PyVCP panel) starts""") 
            print >>f1
            if self.pyvcphaltype == 1 and self.pyvcpconnect: # spindle speed display
                  print >>f1, _("# **** Setup of spindle speed display using pyvcp -START ****")
                  print >>f1
                  if spindle_enc:
                      print >>f1, _("# **** Use ACTUAL spindle velocity from spindle encoder")
                      print >>f1, _("# **** spindle-velocity bounces around so we filter it with lowpass")
                      print >>f1, _("# **** spindle-velocity is signed so we use absolute compoent to remove sign") 
                      print >>f1, _("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
                      print >>f1
                      print >>f1
                      print >>f1, ("setp     scale.spindle.gain .01667")
                      print >>f1, ("setp     lowpass.spindle.gain 0.01")
                      print >>f1, ("net spindle-vel-fb => lowpass.spindle.in")
                      print >>f1, ("net spindle-rps-filtered <= lowpass.spindle.out")
                      print >>f1, ("net spindle-rps-filtered => abs.spindle.in")
                      print >>f1, ("net spindle-absolute-rps    abs.spindle.out => scale.spindle.in")
                      print >>f1, ("net spindle-filtered-rpm    scale.spindle.out => pyvcp.spindle-speed")
                  else:
                      print >>f1, _("# **** Use COMMANDED spindle velocity from EMC because no spindle encoder was specified")
                      print >>f1, _("# **** COMMANDED velocity is signed so we use absolute component to remove sign")
                      print >>f1
                      print >>f1, ("net spindle-vel-cmd                       =>  abs.spindle.in")
                      print >>f1, ("net absolute-spindle-vel    abs.spindle.out =>  pyvcp.spindle-speed")
                  print >>f1, ("net spindle-at-speed => pyvcp.spindle-at-speed-led")
                  print >>f1
                  print >>f1, _("# **** Setup of spindle speed display using pyvcp -END ****")
                  print >>f1
            if self.pyvcphaltype == 2 and self.pyvcpconnect: # Hal_UI example
                      print >>f1, _("# **** Setup of pyvcp buttons and MDI commands using HAL_UI and pyvcp - START ****")
                      print >>f1
                      print >>f1, ("net jog-x-pos  <=    pyvcp.jog-x+")
                      print >>f1, ("net jog-x-neg  <=    pyvcp.jog-x-")
                      print >>f1, ("net jog-y-pos  <=    pyvcp.jog-y+")
                      print >>f1, ("net jog-y-neg  <=    pyvcp.jog-y-")
                      print >>f1, ("net jog-z-pos  <=    pyvcp.jog-z+")
                      print >>f1, ("net jog-z-neg  <=    pyvcp.jog-z-")
                      print >>f1, ("net jog-speed  <=    pyvcp.jog-speed")
                      print >>f1, ("net optional-stp-on     pyvcp.ostop-on     =>  halui.program.optional-stop.on")
                      print >>f1, ("net optional-stp-off    pyvcp.ostop-off    =>  halui.program.optional-stop.off")
                      print >>f1, ("net optional-stp-is-on  pyvcp.ostop-is-on  =>  halui.program.optional-stop.is-on")
                      print >>f1, ("net program-pause       pyvcp.pause        =>  halui.program.pause")
                      print >>f1, ("net program-resume      pyvcp.resume       =>  halui.program.resume")
                      print >>f1, ("net program-single-step pyvcp.step         =>  halui.program.step")
                      print >>f1
                      print >>f1, _("# **** The following mdi-comands are specified in the machine named INI file under [HALUI] heading")
                      print >>f1, ("# **** command 00 - rapid to Z 0 ( G0 Z0 )")
                      print >>f1, ("# **** command 01 - rapid to reference point ( G 28 )")
                      print >>f1, ("# **** command 02 - zero X axis in G54 cordinate system")
                      print >>f1, ("# **** command 03 - zero Y axis in G54 cordinate system")
                      print >>f1, ("# **** command 04 - zero Z axis in G54 cordinate system")
                      print >>f1
                      print >>f1, ("net MDI-Z-up            pyvcp.MDI-z_up          =>  halui.mdi-command-00")
                      print >>f1, ("net MDI-reference-pos   pyvcp.MDI-reference     =>  halui.mdi-command-01")
                      print >>f1, ("net MDI-zero_X          pyvcp.MDI-zerox         =>  halui.mdi-command-02")
                      print >>f1, ("net MDI-zero_Y          pyvcp.MDI-zeroy         =>  halui.mdi-command-03")
                      print >>f1, ("net MDI-zero_Z          pyvcp.MDI-zeroz         =>  halui.mdi-command-04")
                      print >>f1, ("net MDI-clear-offset    pyvcp.MDI-clear-offset  =>  halui.mdi-command-05")
                      print >>f1
                      print >>f1, _("# **** Setup of pyvcp buttons and MDI commands using HAL_UI and pyvcp - END ****")

        if self.customhal or self.classicladder or self.halui:
            custom = os.path.join(base, "custom.hal")
            if not os.path.exists(custom):
                f1 = open(custom, "w")
                print >>f1, _("# Include your customized HAL commands here")
                print >>f1, _("# This file will not be overwritten when you run PNCconf again")

        if self.frontend == _TOUCHY:# TOUCHY GUI
                touchyfile = os.path.join(base, "touchy.hal")
            #if not os.path.exists(touchyfile):
                f1 = open(touchyfile, "w")
                print >>f1, _("# These commands are required for Touchy GUI")
                print >>f1, ("net cycle-start          =>   touchy.cycle-start")
                print >>f1, ("net abort                =>   touchy.abort")
                print >>f1, ("net single-step          =>   touchy.single-block")
                print >>f1, ("net selected-jog-incr    <=   touchy.jog.wheel.increment")
                print >>f1, ("net joint-selected-count =>   touchy.wheel-counts")
                print >>f1, ("net jog-x-pos  => touchy.jog.continuous.x.positive")
                print >>f1, ("net jog-x-neg  => touchy.jog.continuous.x.negative")
                print >>f1, ("net jog-y-pos  => touchy.jog.continuous.y.positive")
                print >>f1, ("net jog-y-neg  => touchy.jog.continuous.y.negative")
                print >>f1, ("net jog-z-pos  => touchy.jog.continuous.z.positive")
                print >>f1, ("net jog-z-neg  => touchy.jog.continuous.z.negative")
                print >>f1, ("net quillup  => touchy.quill-up")
                temp = ("x","y","z","a")
                for axnum,axletter in enumerate(temp):
                    if axletter in self.available_axes:
                        print >>f1, "net joint-select-%s   <=   touchy.jog.wheel.%s"% (chr(axnum+97), axletter)

        shutdown = os.path.join(base, "shutdown.hal")
        if not os.path.exists(shutdown):
            f1 = open(shutdown, "w")
            print >>f1, _("# Include your optional shutdown HAL commands here")
            print >>f1, _("# This file will not be overwritten when you run PNCconf again")
        file.close()
        self.add_md5sum(filename)

    def write_readme(self, base):
        filename = os.path.join(base, "README")
        file = open(filename, "w")
        print >>file, _("Generated by PNCconf at %s") % time.asctime()
        print >>file
        if  self.units == _IMPERIAL: unit = "an imperial"
        else: unit = "a metric"
        if self.frontend == _AXIS: display = "AXIS"
        elif self.frontend == _TKEMC: display = "Tkemc"
        elif self.frontend == _MINI: display = "Mini"
        elif self.frontend == _TOUCHY: display = "TOUCHY"
        else: display = "an unknown"
        if self.axes == 0:machinetype ="XYZ"
        elif self.axes == 1:machinetype ="XYZA"
        elif self.axes == 2:machinetype ="XZ-Lathe"
        print >>file, self.machinename,_("configures EMC2 as:\n")
        print >>file, unit,machinetype,_("type CNC\n")
        print >>file, display,_("will be used as the frontend display")
        print >>file
        if self.number_mesa <> 0:
            for boardnum in range(0,int(self.number_mesa)):
                print >>file, "Mesa hardware I/O card - board %d is designated as\n"% boardnum,self["mesa%d_currentfirmwaredata"% boardnum][_BOARDTITLE] 
                print >>file, "with", self["mesa%d_currentfirmwaredata"% boardnum][9], "I/O pins and firmware is:", self["mesa%d_firmware"% boardnum]
                print >>file
            for boardnum in range(0,int(self.number_mesa)):
                for concount,connector in enumerate(self["mesa%d_currentfirmwaredata"% boardnum][_NUMOFCNCTRS]) :
                    print >>file,"** Mesa %s -> Board #"% self["mesa%d_boardtitle"% boardnum],boardnum,_(" connector")," %d **\n"% connector
                    for pin in range (0,24):
                        temp = self["mesa%dc%dpin%d" % (boardnum,connector,pin) ]
                        tempinv = self["mesa%dc%dpin%dinv" % (boardnum,connector,pin) ]
                        temptype = self["mesa%dc%dpin%dtype" % (boardnum,connector,pin) ]
                        if tempinv: 
                            invmessage = _("-> inverted")
                        else: invmessage =""
                        print >>file, ("pin# %(pinnum)d (%(type)s)               "%{ 'type':temptype,'pinnum':pin})
                        print >>file, ("    connected to signal:'%(data)s'%(mess)s\n" %{'data':temp, 'mess':invmessage}) 
            print >>file
        templist = ("pp1","pp2","pp3")
        for j, k in enumerate(templist):
            if self.number_pports < (j+1): break 
            print >>file, _("%(name)s Parport" % { 'name':k})
            for x in (2,3,4,5,6,7,8,9,10,11,12,13,15): 
                temp = self["%sIpin%d" % (k, x)]
                tempinv = self["%sIpin%dinv" % (k, x)]
                if tempinv: 
                    invmessage = _("-> inverted")
                else: invmessage =""
                print >>file,_("pin# %(pinnum)d is connected to input signal:'%(data)s' %(mesag)s" 
                %{ 'pinnum':x,'data':temp,'mesag':invmessage})          
            for x in (1,2,3,4,5,6,7,8,9,14,16,17):  
                temp = self["%sOpin%d" % (k, x)]
                tempinv = self["%sOpin%dinv" % (k, x)]
                if tempinv: 
                    invmessage = _("-> inverted")
                else: invmessage =""
                print >>file,_("pin# %(pinnum)d is connected to output signal:'%(data)s' %(mesag)s" 
                %{ 'pinnum':x,'data':temp,'mesag':invmessage})   
            print >>file 
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

        filename = os.path.expanduser("~/.pncconf-preferences")
        print filename
        d2 = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, "int-pncconf", None)
        e2 = d2.documentElement

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
        n2.setAttribute('name', "chooselastconfig")
        n2.setAttribute('value', str("%s"% self._chooselastconfig))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'string')
        n2.setAttribute('name', "machinename")
        n2.setAttribute('value', str("%s"%self.machinename))

        d2.writexml(open(filename, "wb"), addindent="  ", newl="\n")

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
            print >>file,"Comment=" + _("Desktop Launcher for EMC config made by PNCconf")
            print >>file,"Icon=%s"% emc2icon
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
                    key = self["pp%dIpin%d" %(i,s)]
                    ppinput[key] = "pp%dIpin%d" %(i,s) 
                for s in (1,2,3,4,5,6,7,8,9,14,16,17):
                    key = self["pp%dOpin%d" %(i,s)]
                    ppoutput[key] = "pp%dOpin%d" %(i,s) 
        mesa = {}
        for boardnum in range(0,int(self.number_mesa)):
            for concount,connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
                    key =   self["mesa%dc%dpin%d"% (boardnum,connector,s)]
                    mesa[key] = "mesa%dc%dpin%d" %(boardnum,connector,s)     
        try:
            return mesa[sig]
        except:
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
    # if is the right component type and number, che3ck the relatedsearch array for a match
    # if its a match add it to a list of pins (pinlist) that need to be updated
    def list_related_pins(self, relatedsearch, boardnum, connector, pin, style):
        pinlist =[]
        for concount,i in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]):
            if i == connector:
                currentptype,currentcompnum = self["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pin+(concount*24)]
                for t_concount,t_connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]):
                    for t_pin in range (0,24):
                        comptype,compnum = self["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+t_pin+(t_concount*24)]
                        if compnum != currentcompnum: continue
                        if comptype not in (relatedsearch): continue
                        if style == 0:
                            tochange = ['mesa%dc%dpin%d'% (boardnum,t_connector,t_pin),boardnum,t_connector,t_pin]
                        if style == 1:
                            tochange = ['mesa%dc%dpin%dtype'% (boardnum,t_connector,t_pin),boardnum,t_connector,t_pin]
                        if style == 2:
                            tochange = ['mesa%dc%dpin%dinv'% (boardnum,t_connector,t_pin),boardnum,t_connector,t_pin]
                        pinlist.append(tochange)
        return pinlist

    # This method takes a signalname data pin (eg mesa0c3pin1)
    # and converts it to a HAL pin names (eg hm2_5i20.0.gpio.01)
    # component number conversion is for adjustment of position of pins related to the
    # 'controlling pin' eg encoder-a (controlling pin) encoder-b encoder -I
    # (a,b,i are related pins for encoder component) 
    def make_pinname(self, pin, gpionumber = False):
        test = str(pin)  
        halboardnum = 0
        if test == "None": return None
        elif 'mesa' in test:
            boardnum = int(test[4:5])
            boardname = self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME]
            if boardnum == 1 and self.mesa1_currentfirmwaredata[_BOARDNAME] == self.mesa0_currentfirmwaredata[_BOARDNAME]:
                halboardnum = 1
            if gpionumber:
                ptype = GPIOI
            else:
                ptype = self[pin+"type"] 
            signalname = self[pin]
            pinnum = int(test[10:])
            connum = int(test[6:7])

            for concount,i in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]):
                if i == connum:
                    dummy,compnum = self["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pinnum+(concount*24)]
                    break
            type_name = { GPIOI:"gpio", GPIOO:"gpio", GPIOD:"gpio", ENCA:"encoder", ENCB:"encoder",ENCI:"encoder",ENCM:"encoder", 
                PWMP:"pwmgen",PWMD:"pwmgen", PWME:"pwmgen", PDMP:"pwmgen", PDMD:"pwmgen", PDME:"pwmgen",STEPA:"stepgen", STEPB:"stepgen",
                TPPWMA:"tppwmgen",TPPWMB:"tppwmgen",TPPWMC:"tppwmgen",TPPWMAN:"tppwmgen",TPPWMBN:"tppwmgen",TPPWMCN:"tppwmgen",
                TPPWME:"tppwmgen",TPPWMF:"tppwmgen","Error":"None" }

            # we iter over this dic because of locale translation problems when using
            # comptype = type_name[ptype]
            comptype = "ERROR FINDING COMPONENT TYPE"
            for key,value in type_name.iteritems():
                if key == ptype: comptype = value
            if value == "Error":
                print "pintype error in make_pinname: ptype = ",ptype
                return None
            elif ptype in(GPIOI,GPIOO,GPIOD):
                compnum = int(pinnum)+(concount*24)
                return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%03d"% (compnum)          
            elif ptype in (ENCA,ENCB,ENCI,ENCM,PWMP,PWMD,PWME,PDMP,PDMD,PDME,STEPA,STEPB,STEPC,STEPD,STEPE,STEPF):
                return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%02d"% (compnum)
  
        elif 'pp' in test:
            print test
            ending = "-out"
            test = str(pin) 
            print  self[pin]
            pintype = str(test[3:4])
            pinnum = int(test[7:])
            connum = int(test[2:3])-1
            if pintype == 'I': ending = "-in"
            return "parport."+str(connum)+".pin-%02d"%(pinnum)+ending
        else:
            print "pintype error in make_pinname: pinname = ",test
            return None

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
        axispage = self._getwidget(doc, 'pp1pport').parentNode.cloneNode(True)
        nextpage = self._getwidget(doc, 'xaxismotor').parentNode
        widget = self._getwidget(axispage, "pp1pport")
        for node in widget.childNodes:
            if (node.nodeType == xml.dom.Node.ELEMENT_NODE
                    and node.tagName == "property"
                    and node.getAttribute('name') == "title"):
                node.childNodes[0].data = _("%s Parallel Port Setup") % axisname
        for node in axispage.getElementsByTagName("widget"):
            id = node.getAttribute('id')
            if id.startswith("pp1"):
                node.setAttribute('id', axisname + id[3:])
            else:
                node.setAttribute('id', axisname + id)
        for node in axispage.getElementsByTagName("signal"):
            handler = node.getAttribute('handler')
            node.setAttribute('handler', handler.replace("on_pp1", "on_" + axisname))
        for node in axispage.getElementsByTagName("property"):
            name = node.getAttribute('name')
            if name == "mnemonic_widget":
                node.childNodes[0].data = axisname + node.childNodes[0].data[1:]
        nextpage.parentNode.insertBefore(axispage, nextpage)

    def splash_screen(self):
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        self.window.set_type_hint(gtk.gdk.WINDOW_TYPE_HINT_SPLASHSCREEN)     
        self.window.set_title("Pncconf setup")
        self.window.set_border_width(10)

        vbox = gtk.VBox(False, 5)
        vbox.set_border_width(10)
        self.window.add(vbox)
        vbox.show()
        align = gtk.Alignment(0.5, 0.5, 0, 0)
        vbox.pack_start(align, False, False, 5)
        align.show()

        self.pbar = gtk.ProgressBar()
        self.pbar.set_text("Pncconf is setting up")
        self.pbar.set_fraction(.1)

        align.add(self.pbar)
        self.pbar.show()
        self.window.show()
        while gtk.events_pending():
            gtk.main_iteration()

    def __init__(self):
        gnome.init("pncconf", "0.6") 
        
        self.splash_screen()
        glade = xml.dom.minidom.parse(os.path.join(datadir, self.fname))
        self.make_axispage(glade, 'y')
        self.make_axispage(glade, 'z')
        self.make_axispage(glade, 'a')
        self.pbar.set_fraction(.2)
        while gtk.events_pending():
            gtk.main_iteration()
        self.make_axismotorpage(glade, 'y')
        self.make_axismotorpage(glade, 'z')
        self.make_axismotorpage(glade, 'a')
        self.pbar.set_fraction(.3)
        while gtk.events_pending():
            gtk.main_iteration()
        self.make_pportpage(glade, 'pp2')
        self.make_pportpage(glade, 'pp3')
        self.pbar.set_fraction(.4)
        while gtk.events_pending():
            gtk.main_iteration()
        doc = glade.toxml().encode("utf-8")
        self.pbar.set_fraction(.75)
        while gtk.events_pending():
            gtk.main_iteration()
        self.xml = gtk.glade.xml_new_from_buffer(doc, len(doc), domain="axis")
        self.window.hide()

        self.widgets = Widgets(self.xml)
        
        self.watermark = gtk.gdk.pixbuf_new_from_file(wizard)
        axisdiagram = os.path.join(helpdir,"axisdiagram1.png")
        self.widgets.helppic.set_from_file(axisdiagram)
        self.widgets.openloopdialog.hide()
        self.widgets.druidpagestart1.set_watermark(self.watermark)
        self.widgets.complete.set_watermark(self.watermark)
        self.widgets.druidpagestart1.show()
        self.widgets.complete.show()
        
        self.xml.signal_autoconnect(self)

        self.in_pport_prepare = False
        self.axis_under_test = False
        self.jogminus = self.jogplus = 0
       
        self.intrnldata = Intrnl_data()
        self.data = Data()
        # add some custom signals for motor/encoder scaling
        for axis in ["x","y","z","a","s"]:
            cb = ["encoderscale","stepscale"]
            for i in cb:
                self.widgets[axis + i].connect("value-changed", self.motor_encoder_sanity_check,axis)
        # connect signals with pin designation data to mesa signal comboboxes and pintype comboboxes
        # record the signal ID numbers so we can block the signals later in the mesa routines
        # have to do it here manually (instead of autoconnect) because glade doesn't handle added
        # user info (board/connector/pin number designations) and doesn't record the signal ID numbers
        # none of this is done if mesa is not checked off in pncconf
        # TODO we should check to see if signals are already present as each time user goes though this page
        # the signals get added again causing multple calls to the functions.

        if (self.data.number_mesa): 
            for boardnum in (0,1):
                cb = "mesa%d_comp_update"% (boardnum)
                i = "_mesa%dsignalhandler_comp_update"% (boardnum)
                self.intrnldata[i] = int(self.widgets[cb].connect("clicked", self.on_mesa_component_value_changed,boardnum))
                cb = "mesa%d_boardtitle"% (boardnum)
                i = "_mesa%dsignalhandler_boardname_change"% (boardnum)
                self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_boardname_changed,boardnum))
                cb = "mesa%d_firmware"% (boardnum)
                i = "_mesa%dsignalhandler_firmware_change"% (boardnum)
                self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_firmware_changed,boardnum))
                for connector in (2,3,4,5,6,7,8,9):
                    for pin in range(0,24):
                      cb = "mesa%dc%ipin%i"% (boardnum,connector,pin)
                      i = "_mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)
                      self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_pin_changed,boardnum,connector,pin,False))
                      i = "_mesa%dactivatehandlerc%ipin%i"% (boardnum,connector,pin)
                      self.intrnldata[i] = int(self.widgets[cb].child.connect("activate", self.on_mesa_pin_changed,boardnum,connector,pin,True))
                      cb = "mesa%dc%ipin%itype"% (boardnum,connector,pin)
                      i = "_mesa%dptypesignalhandlerc%ipin%i"% (boardnum,connector,pin)
                      self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_pintype_changed,boardnum,connector,pin))

        # search for firmware packages
        if os.path.exists(firmdir):
            global mesaboardnames
            mesaboardnames = []
            for root, dirs, files in os.walk(firmdir):
                folder = root.lstrip(firmdir)
                if folder in mesablacklist:continue
                if folder == "":continue
                mesaboardnames.append(folder)
                print "\n**** ",folder,":\n"
        else:
            #TODO what if there are no external firmware is this enough?
            self.warning_dialog(_("You are have no hostmot2 firmware in folder:%s"%firmdir),True)
        #TODO is this right place?
        self.fill_pintype_model()
        self.fill_combobox_models()

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

    def on_helpwindow_delete_event(self, *args):
        self.widgets.helpwindow.hide()
        return True

    def on_druid1_help(self, *args):
        helpfilename = os.path.join(helpdir, "%s"% self.data.help)
        textbuffer = self.widgets.helpview.get_buffer()
        try :
            infile = open(helpfilename, "r")
            if infile:
                string = infile.read()
                infile.close()
                textbuffer.set_text(string)
                self.widgets.helpwindow.set_title(_("Help Pages") )
                self.widgets.helpnotebook.set_current_page(0)
                self.widgets.helpwindow.show_all()
                self.widgets.helpwindow.present()
        except:
            text = _("Help page is unavailable\n")
            self.warning_dialog(text,True)

    def check_for_rt(self,fussy=True):
        actual_kernel = os.uname()[2]
        if hal.is_sim == 1 :
            if fussy:
                self.warning_dialog(_("You are using a simulated-realtime version of EMC, so testing / tuning of external hardware is unavailable."),True)
                return False
            else:
                return True
        elif hal.is_rt and not hal.kernel_version == actual_kernel:
            self.warning_dialog(_("""You are using a realtime version of EMC but didn't load a realtime kernel so testing / tuning of external  hardware is unavailable.\n This is probably because you updated the OS and it doesn't load the RTAI kernel anymore\n You are using the %(actual)s kernel instead of %(needed)s""")% {'actual':actual_kernel, 'needed':hal.kernel_version},True)
            return False
        else:
            return True
       
    def on_page_newormodify_prepare(self, *args):
        self.data.help = "help-load.txt"
        filename = os.path.expanduser("~/.pncconf-preferences")
        link = short = False
        if os.path.exists(filename):
            match =  open(filename).read()
            textbuffer = self.widgets.textoutput.get_buffer()
            try :
                textbuffer.set_text("%s\n\n"% filename)
                textbuffer.insert_at_cursor(match)
            except:
                pass
            d = xml.dom.minidom.parse(open(filename, "r"))
            for n in d.getElementsByTagName("property"):
                name = n.getAttribute("name")
                text = n.getAttribute('value')
                if name == "always_shortcut":
                    short = eval(text)
                if name == "always_link":
                    link = eval(text)
                if name == "machinename":
                    self.data._lastconfigname = text
                if name == "choosetconfig":
                    self.data._chooselastconfig = eval(text)
        self.widgets.createsymlink.set_active(link)
        self.widgets.createshortcut.set_active(short)

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
            if not self.data._lastconfigname == "" and self.data._chooselastconfig:
                dialog.set_filename(os.path.expanduser("~/emc2/configs/%s.pncconf"% self.data._lastconfigname))
            dialog.add_shortcut_folder(os.path.expanduser("~/emc2/configs"))
            dialog.set_current_folder(os.path.expanduser("~/emc2/configs"))
            dialog.show_all()
            result = dialog.run()
            if result == gtk.RESPONSE_OK:
                filename = dialog.get_filename()
                dialog.destroy()
                self.data.load(filename, self)
                self.intrnldata.mesa0_configured = False
                self.intrnldata.mesa1_configured = False
            else:
                dialog.destroy()
                return True
        self.data.createsymlink = self.widgets.createsymlink.get_active()
        self.data.createshortcut = self.widgets.createshortcut.get_active()
        self.widgets.window1.set_title(_("Point and click configuration - %s.pncconf ") % self.data.machinename)

    def on_page_newormodify_back(self, *args):
        self.data.help = "help-welcome.txt"

    def on_basicinfo_prepare(self, *args):
        self.data.help = "help-basic.txt"
        self.widgets.machinename.set_text(self.data.machinename)
        self.widgets.axes.set_active(self.data.axes)
        self.widgets.units.set_active(self.data.units)
        self.widgets.servoperiod.set_value(self.data.servoperiod)
        self.widgets.machinename.grab_focus()
        if self.data.number_mesa == 1:
            self.widgets.mesa0_checkbutton.set_active(True)
            self.widgets.mesa1_checkbutton.set_active(False)
        elif self.data.number_mesa == 2:
            self.widgets.mesa0_checkbutton.set_active(True)
            self.widgets.mesa1_checkbutton.set_active(True)
        else:
            self.widgets.mesa0_checkbutton.set_active(False)
            self.widgets.mesa1_checkbutton.set_active(False)
        self.widgets.ioaddr.set_text(self.data.ioaddr)
        self.widgets.ioaddr2.set_text(self.data.ioaddr2) 
        self.widgets.ioaddr3.set_text(self.data.ioaddr3)
        self.widgets.pp1_direction.set_active(self.data.pp1_direction)
        self.widgets.pp2_direction.set_active(self.data.pp2_direction)
        self.widgets.pp3_direction.set_active(self.data.pp3_direction)
        if self.data.number_pports>0:
             self.widgets.pp1_checkbutton.set_active(1)
        else :
             self.widgets.pp1_checkbutton.set_active(0)
        if self.data.number_pports>1:
             self.widgets.pp2_checkbutton.set_active(1)
        if self.data.number_pports>2:
             self.widgets.pp3_checkbutton.set_active(1)
        if self.data.frontend == _AXIS : self.widgets.GUIAXIS.set_active(True)
        elif self.data.frontend == _TKEMC: self.widgets.GUITKEMC.set_active(True)
        elif self.data.frontend == _MINI: self.widgets.GUIMINI.set_active(True)
        elif self.data.frontend == _TOUCHY: self.widgets.GUITOUCHY.set_active(True)
        
        # here we initalise the mesa configure page data
        if not self.data._arrayloaded:
            print "fill boardtitle array"
            for boardnum in(0,1):
                model = self.widgets["mesa%d_boardtitle"% boardnum].get_model()
                model.clear()
                for search,item in enumerate(mesaboardnames):
                    model.append((item,))
                    if mesaboardnames[search]  == self.data["mesa%d_boardtitle"% boardnum]:
                        self.widgets["mesa%d_boardtitle"% boardnum].set_active(search)

    def on_mesa_checkbutton_toggled(self, *args): 
        i = self.widgets.mesa0_checkbutton.get_active()
        j = self.widgets.mesa1_checkbutton.get_active()
        self.widgets.mesa0_boardtitle.set_sensitive(i)
        self.widgets.mesa1_boardtitle.set_sensitive(j)
        if j and not i:
            self.widgets.mesa1_checkbutton.set_active(False)
            self.widgets.mesa1_boardtitle.set_sensitive(False)
        
    def on_pp1_checkbutton_toggled(self, *args): 
        i = self.widgets.pp1_checkbutton.get_active()   
        self.widgets.pp1_direction.set_sensitive(i)
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
        self.widgets.pp2_direction.set_sensitive(i)
        self.widgets.ioaddr2.set_sensitive(i)
        if i == 0:
           self.widgets.pp3_checkbutton.set_active(i)
           self.widgets.ioaddr3.set_sensitive(i)

    def on_pp3_checkbutton_toggled(self, *args): 
        i = self.widgets.pp3_checkbutton.get_active() 
        if self.widgets.pp2_checkbutton.get_active() == 0:
          i = 0  
          self.widgets.pp3_checkbutton.set_active(0)
        self.widgets.pp3_direction.set_sensitive(i)
        self.widgets.ioaddr3.set_sensitive(i)      

    def on_basicinfo_next(self, *args):
        machinename= self.widgets.machinename.get_text()
        self.data.machinename = machinename.replace(" ","_")
        self.widgets.window1.set_title(_("Point and click configuration - %s.pncconf ") % self.data.machinename)
        self.data.axes = self.widgets.axes.get_active()
        if self.data.axes == 0: self.data.available_axes = ['x','y','z','s']
        elif self.data.axes == 1: self.data.available_axes = ['x','y','z','a','s']
        elif self.data.axes == 2: self.data.available_axes = ['x','z','s']
        self.data.units = self.widgets.units.get_active()
        self.data.servoperiod = self.widgets.servoperiod.get_value()
        self.data.ioaddr = self.widgets.ioaddr.get_text()
        self.data.ioaddr2 = self.widgets.ioaddr2.get_text()
        self.data.ioaddr3 = self.widgets.ioaddr3.get_text()
        i = self.widgets.mesa0_checkbutton.get_active()
        j = self.widgets.mesa1_checkbutton.get_active()
        self.data.number_mesa = int(i)+int(j)
        if self.widgets.pp3_checkbutton.get_active() and self.widgets.pp2_checkbutton.get_active():
            self.data.number_pports = 3
        elif self.widgets.pp2_checkbutton.get_active() and self.widgets.pp1_checkbutton.get_active():
            self.data.number_pports = 2
        elif self.widgets.pp1_checkbutton.get_active():
            self.data.number_pports = 1
        else :
            self.data.number_pports = 0
        if self.data.number_pports == 0 and self.data.number_mesa== 0 :
           self.warning_dialog(_("You need to designate a parport and/or mesa I/O device before continuing."),True)
           self.widgets.druid1.set_page(self.widgets.basicinfo)
           return True 
        self.data.pp1_direction = self.widgets.pp1_direction.get_active()
        self.data.pp2_direction = self.widgets.pp2_direction.get_active()
        self.data.pp3_direction = self.widgets.pp3_direction.get_active()
        if self.widgets.GUIAXIS.get_active():
           self.data.frontend = _AXIS
        elif self.widgets.GUITKEMC.get_active():
           self.data.frontend = _TKEMC
        elif self.widgets.GUIMINI.get_active():
           self.data.frontend = _MINI
        elif self.widgets.GUITOUCHY.get_active():
           self.data.frontend = _TOUCHY
        i = self.widgets.mesa0_boardtitle.get_active_text()
        j = self.widgets.mesa1_boardtitle.get_active_text()
        print i,self.data.mesa0_boardtitle,j,self.data.mesa1_boardtitle 
        if not self.data._arrayloaded or not self.data.mesa0_boardtitle == i or not self.data.mesa1_boardtitle == j:
            if os.path.exists(os.path.join(firmdir,i)) or os.path.exists(os.path.join(firmdir,j)):
                global mesafirmwaredata
                mesafirmwaredata = []
                self.mesa_firmware_search(i)
                #print mesafirmwaredata
                if self.data.number_mesa == 2 and not i == j: self.mesa_firmware_search(j)
                self.data._arrayloaded = True
            for boardnum in (0,1):
                model = self.widgets["mesa%d_firmware"% boardnum].get_model()
                model.clear()
                for search, item in enumerate(mesafirmwaredata):
                    d = mesafirmwaredata[search]
                    if not d[_BOARDTITLE] == self.widgets["mesa%d_boardtitle"% boardnum].get_active_text():continue
                    model.append((d[_FIRMWARE],))
                for search,item in enumerate(model):
                    if model[search][0]  == self.data["mesa%d_firmware"% boardnum]:
                        self.widgets["mesa%d_firmware"% boardnum].set_active(search)
                self.widgets["mesa%d_pwm_frequency"% boardnum].set_value(self.data["mesa%d_pwm_frequency"% boardnum])
                self.widgets["mesa%d_pdm_frequency"% boardnum].set_value(self.data["mesa%d_pdm_frequency"% boardnum])
                self.widgets["mesa%d_watchdog_timeout"% boardnum].set_value(self.data["mesa%d_watchdog_timeout"% boardnum])
                self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(self.data["mesa%d_numof_encodergens"% boardnum])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(self.data["mesa%d_numof_pwmgens"% boardnum])
                self.widgets["mesa%d_numof_tppwmgens"% boardnum].set_value(self.data["mesa%d_numof_tppwmgens"% boardnum])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(self.data["mesa%d_numof_stepgens"% boardnum])
                self.widgets["mesa%d_numof_gpio"% boardnum].set_text("%d" % self.data["mesa%d_numof_gpio"% boardnum])
        self.data.mesa0_boardtitle = self.widgets.mesa0_boardtitle.get_active_text()
        self.data.mesa1_boardtitle = self.widgets.mesa1_boardtitle.get_active_text()


    def mesa_firmware_search(self,boardtitle,*args):
        print "**** INFO mesa-firmware-search"
        #TODO if no firm packages set up for internal data?
        #TODO don't do this if the firmware is already loaded
        self.pbar.set_text("Loading external firmware")
        self.pbar.set_fraction(1)
        self.window.show()
        while gtk.events_pending():
            gtk.main_iteration()
        firmlist = []
        for root, dirs, files in os.walk(firmdir):
            folder = root.lstrip(firmdir)
            if folder in mesablacklist:continue
            if not folder == boardtitle:continue
            for name in files:
                if name in mesablacklist:continue
                if ".xml" in name:
                    print name
                    temp = name.strip(".xml")
                    firmlist.append(temp)
        for currentfirm in(firmlist):
            root = xml.etree.ElementTree.parse(os.path.join(firmdir,boardtitle,currentfirm+".xml"))
            watchdog = encoder = pwmgen = led = muxedqcount = stepgen = tppwmgen = 0
            numencoderpins = 3; numstepperpins = 2
            boardname = root.find("boardname").text;print boardname, currentfirm
            maxgpio  = int(root.find("iowidth").text) ; #print maxgpio
            numcnctrs  = root.find("ioports").text ; #print numcnctrs
            lowfreq = int(root.find("clocklow").text)/1000000 ; #print lowfreq
            hifreq = int(root.find("clockhigh").text)/1000000 ; #print hifreq
            modules = root.findall("//modules")[0]
            if "7i43" in boardname:
                driver = "hm2_7i43"
            else:
                driver = "hm2_pci"
            for i,j in enumerate(modules):
                k = modules[i].find("tagname").text
                if k == "Watchdog": 
                    l = modules[i].find("numinstances").text;#print l,k
                    watchdog = int(l)
                elif k == "Encoder": 
                    l = modules[i].find("numinstances").text;#print l,k
                    encoder = int(l)
                elif k == "PWMGen":
                    l = modules[i].find("numinstances").text;#print l,k
                    pwmgen = int(l)
                elif k == "LED": 
                    l = modules[i].find("numinstances").text;#print l,k
                    led = int(l)
                elif k == "MuxedQCount": 
                    l = modules[i].find("numinstances").text;#print l,k
                    muxedqcount = int(l)
                elif k == "StepGen": 
                    l = modules[i].find("numinstances").text;#print l,k
                    stepgen = int(l)
                elif k == "TPPWM": 
                    l = modules[i].find("numinstances").text;#print l,k
                    tppwmgen = int(l)
                elif k in ("IOPort","AddrX","MuxedQCountSel"):
                    continue
                else:
                    print "**** WARNING: Pncconf parsing firmware: tagname (%s) not reconized"% k
    
            pins = root.findall("//pins")[0]
            temppinlist = []
            tempconlist = []
            pinconvertenc = {"Phase A (in)":ENCA,"Phase B (in)":ENCB,"Index (in)":ENCI,"IndexMask (in)":ENCM,
                "Muxed Phase A (in)":MXEA,"Muxed Phase B (in)":MXEB,"Muxed Index (in)":MXEI,"Muxed Index Mask (in)":MXEM,"Muxed Encoder Select 0 (out)":MXES}
            pinconvertstep = {"Step (out)":STEPA,"Dir (out)":STEPB}
                #"StepTable 2 (out)":STEPC,"StepTable 3 (out)":STEPD,"StepTable 4 (out)":STEPE,"StepTable 5 (out)":STEPF
            pinconvertppwm = {"PWM/Up (out)":PWMP,"Dir/Down (out)":PWMD,"Enable (out)":PWME}
            pinconverttppwm = {"PWM A (out)":TPPWMA,"PWM B (out)":TPPWMB,"PWM C (out)":TPPWMC,"PWM /A (out)":TPPWMAN,"PWM /B (out)":TPPWMBN,
                "PWM /C (out)":TPPWMCN,"Fault (in)":TPPWMF,"Enable (out)":TPPWME}
            for i,j in enumerate(pins):
                temppinunit = []
                temp = pins[i].find("connector").text
                tempcon = int(temp.strip("P"))
                temp = pins[i].find("secondaryfunctionname").text
                try:
                    modulename = pins[i].find("secondarymodulename").text
                    #print temp,modulename
                    if modulename in ("Encoder","MuxedQCount","MuxedQCountSel"):
                        convertedname = pinconvertenc[temp]
                    elif modulename == "PWMGen":
                        convertedname = pinconvertppwm[temp]
                    elif modulename == "StepGen":
                        convertedname = pinconvertstep[temp]
                    elif modulename == "TPPWM":
                        convertedname = pinconverttppwm[temp]
                    else: raise ValueError
                except:
                    # must be GPIO pins if there is no secondary mudule name
                    #print "GPIO"
                    temppinunit.append(GPIOI)
                    temppinunit.append(int(pins[i].find("index").text))
                else:
                    temppinunit.append(convertedname)
                    temppinunit.append(int(pins[i].find("secondaryinstance").text))
                    tempmod = pins[i].find("secondarymodulename").text
                    tempfunc = pins[i].find("secondaryfunctionname").text
                    if tempmod in("Encoder","MuxedQCount") and tempfunc in ("Muxed Index Mask (in)","IndexMask (in)"):
                        numencoderpins = 4
                if not tempcon in tempconlist:
                    tempconlist.append(tempcon)
                temppinlist.append(temppinunit)

            temp = [boardtitle,boardname,currentfirm,boardtitle,driver,encoder + muxedqcount,numencoderpins,pwmgen,tppwmgen,stepgen,
                    numstepperpins,watchdog,maxgpio,lowfreq,hifreq,tempconlist]
            print temp
            for i in temppinlist:
                temp.append(i)
            mesafirmwaredata.append(temp)
        self.window.hide()

    def on_machinename_changed(self, *args):
        self.widgets.confdir.set_text(
            "~/emc2/configs/%s" % self.widgets.machinename.get_text())

    def on_external_cntrl_prepare(self, *args):
        self.data.help = "help-extcontrols.txt"
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
        if self.data.multimpg :
            self.widgets.multimpg.set_active(1)
        else:
            self.widgets.sharedmpg.set_active(1)
        if self.data.fo_usempg :
            self.widgets.fo_usempg.set_active(1)
        else:
            self.widgets.fo_useswitch.set_active(1)
        if self.data.so_usempg :
            self.widgets.so_usempg.set_active(1)
        else:
            self.widgets.so_useswitch.set_active(1)
        self.widgets.jograpidrate.set_value(self.data.jograpidrate)
        self.widgets.singlejogbuttons.set_active(self.data.singlejogbuttons)
        self.widgets.multijogbuttons.set_active(self.data.multijogbuttons)
        self.widgets.externalmpg.set_active(self.data.externalmpg)
        self.widgets.externaljog.set_active(self.data.externaljog)
        self.widgets.externalfo.set_active(self.data.externalfo)
        self.widgets.externalso.set_active(self.data.externalso)
        self.widgets.sharedmpg.set_active(self.data.sharedmpg)
        self.widgets.multimpg.set_active(self.data.multimpg)
        self.widgets.incrselect.set_active(self.data.incrselect)
        if self.data.units == _IMPERIAL :
            tempunits = "in"
        else:
            tempunits = "mm"      
        for i in range(0,16):          
            self.widgets["foincrvalue"+str(i)].set_value(self.data["foincrvalue"+str(i)])
            self.widgets["soincrvalue"+str(i)].set_value(self.data["soincrvalue"+str(i)])
            self.widgets["mpgincr"+str(i)].set_text(tempunits)

        self.widgets.jograpidunits.set_text(tempunits+" / min")
        for i in range(0,4):
            self.widgets["joystickjograpidunits%d"%i].set_text(tempunits+" / min")
        for i in range(0,8):
            self.widgets["mpgincrvalue"+str(i)].set_value(self.data["mpgincrvalue"+str(i)])
        self.widgets.joystickjog.set_active(self.data.joystickjog)
        self.widgets.usbdevicename.set_text(self.data.usbdevicename)
        for i in range(0,4):
            self.widgets["joystickjograpidrate%d"%i].set_value(self.data["joystickjograpidrate%d"%i])
        for temp in ("joycmdxpos","joycmdxneg","joycmdypos","joycmdyneg","joycmdzpos","joycmdzneg","joycmdrapida","joycmdrapidb"):
            self.widgets[temp].set_text(self.data[temp])

    def on_joystickjog_toggled(self, *args):
        if self.widgets.externaljog.get_active() == True and self.widgets.joystickjog.get_active() == True:
            self.widgets.externaljog.set_active(False)
        self.on_external_options_toggled()

    def on_externaljog_toggled(self, *args):
        if self.widgets.joystickjog.get_active() == True and self.widgets.externaljog.get_active() == True:
            self.widgets.joystickjog.set_active(False)
        self.on_external_options_toggled()

    def on_external_options_toggled(self, *args):
        self.widgets.externaljogbox.set_sensitive(self.widgets.externaljog.get_active())
        self.widgets.externalmpgbox.set_sensitive(self.widgets.externalmpg.get_active())
        self.widgets.externalfobox.set_sensitive(self.widgets.externalfo.get_active())
        self.widgets.externalsobox.set_sensitive(self.widgets.externalso.get_active())      
        self.widgets.foexpander.set_sensitive(self.widgets.fo_useswitch.get_active())
        self.widgets.soexpander.set_sensitive(self.widgets.so_useswitch.get_active())
        self.widgets.joystickjogbox.set_sensitive(self.widgets.joystickjog.get_active())
        i =  self.widgets.incrselect.get_active()
        for j in range(1,16):
            self.widgets["incrlabel%d"% j].set_sensitive(i)
            self.widgets["mpgincrvalue%d"% j].set_sensitive(i)
            self.widgets["mpgincr%d"% j].set_sensitive(i)

    def on_addrule_clicked(self, *args):
        text = []
        sourcefile = "/tmp/"
        if os.path.exists("/etc/udev/rules.d/50-EMC2-general.rules"):
            text.append( "General rule already exists\n")
        else:
            text.append("adding a general rule first\nso your device will be found\n")
            filename = os.path.join(sourcefile, "EMCtempGeneral.rules")
            file = open(filename, "w")
            print >>file, ("# This is a rule for EMC2's hal_input\n")
            print >>file, ("""SUBSYSTEM="input", mode="0660", group="plugdev" """) 
            file.close()
            p=os.popen("gksudo cp  %sEMCtempGeneral.rules /etc/udev/rules.d/50-EMC2-general.rules"% sourcefile )
            time.sleep(.1)
            p.flush()
            p.close()
            os.remove('%sEMCtempGeneral.rules'% sourcefile)
        text.append(("disconect USB device please\n"))
        if not self.warning_dialog("\n".join(text),False):return

        os.popen('less /proc/bus/input/devices >> %sEMCnojoytemp.txt'% sourcefile)
        text = ["Plug in USB device please"]
        if not self.warning_dialog("\n".join(text),False):return
        time.sleep(1)

        os.popen('less /proc/bus/input/devices >> %sEMCjoytemp.txt'% sourcefile).read()
        diff = os.popen (" less /proc/bus/input/devices  | diff   %sEMCnojoytemp.txt %sEMCjoytemp.txt "%(sourcefile, sourcefile) ).read()
        self.widgets.helpwindow.set_title(_("USB device Info Search"))

        os.remove('%sEMCnojoytemp.txt'% sourcefile)
        os.remove('%sEMCjoytemp.txt'% sourcefile)
        if diff =="":
            text = ["No new USB device found"]
            if not self.warning_dialog("\n".join(text),True):return
        else:
            textbuffer = self.widgets.textoutput.get_buffer()
            try :         
                textbuffer.set_text(diff)
                self.widgets.helpnotebook.set_current_page(2)
                self.widgets.helpwindow.show_all()
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
            tempname = sourcefile+"EMCtempspecific.rules"
            file = open(tempname, "w")
            print >>file, ("# This is a rule for EMC2's hal_input\n")
            print >>file, ("# For devicename=%s\n"% name)
            print >>file, ("""SYSFS{idProduct}=="%s", SYSFS{idVendor}=="%s", mode="0660", group="plugdev" """%(product,vendor)) 
            file.close()
            # remove illegal filename characters
            for i in ("(",")"):
                temp = name.replace(i,"")
                name = temp
            newname = "50-EMC2-%s.rules"% name.replace(" ","_")
            os.popen("gksudo cp  %s /etc/udev/rules.d/%s"% (tempname,newname) )
            time.sleep(1)
            os.remove('%sEMCtempspecific.rules'% sourcefile)
            text = ["Please unplug and plug in your device again"]
            if not self.warning_dialog("\n".join(text),True):return

    def on_joysticktest_clicked(self, *args):
        halrun = subprocess.Popen("halrun -f  ", shell=True,stdin=subprocess.PIPE,stdout=subprocess.PIPE )   
        print "requested devicename = ",self.widgets.usbdevicename.get_text()
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
                buttonlist = buttonlist + "  %s  %s      %s"% ( i,temp[index+1],temp[index+3] )
        if buttonlist =="": return
        textbuffer = self.widgets.textoutput.get_buffer()
        try :         
            textbuffer.set_text(buttonlist)
            self.widgets.helpnotebook.set_current_page(2)
            self.widgets.helpwindow.show_all()
        except:
            text = _("Pin names are unavailable\n")
            self.warning_dialog(text,True)

    def on_joysearch_clicked(self, *args):
        flag = False
        textbuffer = self.widgets.textoutput.get_buffer()
        textbuffer.set_text("Searching for device rules in folder:    /etc/udev/rules.d\n\n")
        for entry in os.listdir("/etc/udev/rules.d"):
            if fnmatch.fnmatch( entry,"50-EMC2-*"):
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
                            self.widgets.helpwindow.show_all()
                        except:
                            text = _("Device names are unavailable\n")
                            self.warning_dialog(text,True)
        if flag == False:
            text = _("No Pncconf made device rules were found\n")
            textbuffer.insert_at_cursor(text)
            self.warning_dialog(text,True)

    def on_external_cntrl_next(self, *args):
        self.data.limitshared = self.widgets.limittype_shared.get_active()
        self.data.limitsnone = self.widgets.limittype_none.get_active()
        self.data.limitswitch = self.widgets.limittype_switch.get_active()
        self.data.limitshared = self.widgets.limittype_shared.get_active()
        self.data.homenone = self.widgets.home_none.get_active()
        self.data.homeindex = self.widgets.home_index.get_active()
        self.data.homeswitch = self.widgets.home_switch.get_active()
        self.data.homeboth = self.widgets.home_both.get_active()
        self.data.multimpg = self.widgets.multimpg.get_active()
        self.data.fo_usempg = self.widgets.fo_usempg.get_active()
        self.data.fo_useswitch = self.widgets.fo_useswitch.get_active()
        self.data.so_usempg = self.widgets.so_usempg.get_active()
        self.data.so_useswitch = self.widgets.so_useswitch.get_active()
        self.data.jograpidrate = self.widgets.jograpidrate.get_value()
        self.data.singlejogbuttons = self.widgets.singlejogbuttons.get_active()
        self.data.multijogbuttons = self.widgets.multijogbuttons.get_active()
        self.data.externalmpg = self.widgets.externalmpg.get_active()
        self.data.externaljog = self.widgets.externaljog.get_active()
        self.data.externalfo = self.widgets.externalfo.get_active()
        self.data.externalso = self.widgets.externalso.get_active()
        self.data.sharedmpg = self.widgets.sharedmpg.get_active()
        self.data.multimpg = self.widgets.multimpg.get_active()
        self.data.incrselect = self.widgets.incrselect.get_active()
        for i in range (0,16):
            self.data["foincrvalue"+str(i)] = self.widgets["foincrvalue"+str(i)].get_value()
            self.data["soincrvalue"+str(i)] = self.widgets["soincrvalue"+str(i)].get_value()
            self.data["mpgincrvalue"+str(i)] = self.widgets["mpgincrvalue"+str(i)].get_value()
        self.data.usbdevicename = self.widgets.usbdevicename.get_text()
        self.data.joystickjog = self.widgets.joystickjog.get_active()
        for i in range(0,4):
            self.data["joystickjograpidrate%d"%i] = self.widgets["joystickjograpidrate%d"%i].get_value()
        for temp in ("joycmdxpos","joycmdxneg","joycmdypos","joycmdyneg","joycmdzpos","joycmdzneg","joycmdrapida","joycmdrapidb"):
            self.data[temp] = self.widgets[temp].get_text()
        self.widgets.joyjogexpander.set_expanded(False)

    def on_GUI_config_prepare(self, *args):
        self.data.help = "help-gui.txt"
        self.widgets.pyvcp.set_active(self.data.pyvcp)
        self.on_pyvcp_toggled()
        if  not self.widgets.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custompanel.xml" % self.data.machinename)):
                self.widgets.pyvcpexist.set_active(True)
        self.widgets.default_linear_velocity.set_value( self.data.default_linear_velocity*60)
        self.widgets.max_linear_velocity.set_value( self.data.max_linear_velocity*60)
        self.widgets.min_linear_velocity.set_value( self.data.min_linear_velocity*60)
        self.widgets.default_angular_velocity.set_value( self.data.default_angular_velocity*60)
        self.widgets.max_angular_velocity.set_value( self.data.max_angular_velocity*60)
        self.widgets.min_angular_velocity.set_value( self.data.min_angular_velocity*60)
        self.widgets.editor.set_text(self.data.editor)
        if self.data.units == _IMPERIAL :
            temp = self.data.increments_imperial
            tempunits = "in / min"
        else:
            temp = self.data.increments_metric
            tempunits = "mm / min"
        self.widgets.increments.set_text(temp)
        for i in (0,1,2):
            self.widgets["velunits"+str(i)].set_text(tempunits)
        self.widgets.position_offset.set_active(self.data.position_offset)
        self.widgets.position_feedback.set_active(self.data.position_feedback)
        self.widgets.geometry.set_text(self.data.geometry)
        self.widgets.pyvcpconnect.set_active(self.data.pyvcpconnect)
        self.widgets.require_homing.set_active(self.data.require_homing)
        self.widgets.individual_homing.set_active(self.data.individual_homing)
        self.widgets.restore_joint_position.set_active(self.data.restore_joint_position) 
        self.widgets.random_toolchanger.set_active(self.data.random_toolchanger) 
        self.widgets.raise_z_on_toolchange.set_active(self.data.raise_z_on_toolchange) 
        self.widgets.allow_spindle_on_toolchange.set_active(self.data.allow_spindle_on_toolchange)
        self.widgets.toolchangeprompt.set_active(self.data.toolchangeprompt)
        
    def on_GUI_config_next(self, *args):
        self.data.default_linear_velocity = self.widgets.default_linear_velocity.get_value()/60
        self.data.max_linear_velocity = self.widgets.max_linear_velocity.get_value()/60
        self.data.min_linear_velocity = self.widgets.min_linear_velocity.get_value()/60
        self.data.default_angular_velocity = self.widgets.default_angular_velocity.get_value()/60
        self.data.max_angular_velocity = self.widgets.max_angular_velocity.get_value()/60
        self.data.min_angular_velocity = self.widgets.min_angular_velocity.get_value()/60
        self.data.editor = self.widgets.editor.get_text()
        if self.data.units == _IMPERIAL :self.data.increments_imperial = self.widgets.increments.get_text()
        else:self.data.increments_metric = self.widgets.increments.get_text()
        self.data.geometry = self.widgets.geometry.get_text()
        self.data.position_offset = self.widgets.position_offset.get_active()
        self.data.position_feedback = self.widgets.position_feedback.get_active()
        self.data.require_homing = self.widgets.require_homing.get_active()
        self.data.individual_homing = self.widgets.individual_homing.get_active()
        self.data.restore_joint_position = self.widgets.restore_joint_position.get_active() 
        self.data.random_toolchanger = self.widgets.random_toolchanger.get_active() 
        self.data.raise_z_on_toolchange = self.widgets.raise_z_on_toolchange.get_active() 
        self.data.allow_spindle_on_toolchange = self.widgets.allow_spindle_on_toolchange.get_active()
        self.data.toolchangeprompt = self.widgets.toolchangeprompt.get_active()
        if not self.data.number_mesa:
           self.widgets.druid1.set_page(self.widgets.pp1pport)
           return True
        self.data.pyvcp = self.widgets.pyvcp.get_active()
        self.data.pyvcpconnect = self.widgets.pyvcpconnect.get_active() 
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
              self.data.halui_cmd1="G0 G53 Z0"
              self.data.halui_cmd2="G28"
              self.data.halui_cmd3="G92 X0"
              self.data.halui_cmd4="G92 Y0"
              self.data.halui_cmd5="G92 Z0"
              self.data.halui_cmd6="G92.1"               
           if self.widgets.pyvcpexist.get_active() == True:
              self.data.pyvcpname = "custompanel.xml"
           else:
              if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custompanel.xml" % self.data.machinename)):
                 if not self.warning_dialog(_("OK to replace existing custom pyvcp panel and custom_postgui.hal file ?\nExisting custompanel.xml and custom_postgui.hal will be renamed custompanel_backup.xml and postgui_backup.hal.\nAny existing file named custompanel_backup.xml and custom_postgui.hal will be lost. "),False):
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

        p = 'pp1Ipin%d' % pin
        v = self.widgets[p].get_active()
        ex = exclusive.get(hal_input_names[v], ())

        for pin1 in (10,11,12,13,15):
            if pin1 == pin: continue
            p = 'pp1Ipin%d' % pin1
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

    def on_mesa_boardname_changed(self, widget,boardnum):
        print "**** INFO boardname changed"
        model = self.widgets["mesa%d_boardtitle"% boardnum].get_model()
        active = self.widgets["mesa%d_boardtitle"% boardnum].get_active()
        if active < 0:
          title = None
        else: title = model[active][0]
        if title == None:return
        self.widgets["mesa%dtitle"%boardnum].set_text(title)
        model = self.widgets["mesa%d_firmware"% boardnum].get_model()
        model.clear()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[_BOARDTITLE] == title:continue
            model.append((d[_FIRMWARE],))
            break
        self.widgets["mesa%d_firmware"% boardnum].set_active(0)
        print "boardname-" + d[_BOARDNAME]
        if  "7i43" in d[_BOARDNAME] :
            self.widgets["mesa%d_parportaddrs"% boardnum].set_sensitive(1)
        else:
            self.widgets["mesa%d_parportaddrs"% boardnum].set_sensitive(0)
        self.on_mesa_firmware_changed(self,boardnum)

    def on_mesa_firmware_changed(self, widget,boardnum):
        model = self.widgets["mesa%d_boardtitle"% boardnum].get_model()
        active = self.widgets["mesa%d_boardtitle"% boardnum].get_active()
        if active < 0:
          title = None
        else: title = model[active][0]
        firmware = self.widgets["mesa%d_firmware"% boardnum].get_active_text()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[_BOARDTITLE] == title:continue
            if d[_FIRMWARE] == firmware:
                self.widgets["mesa%d_numof_encodergens"%boardnum].set_range(0,d[_MAXENC])
                self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(d[_MAXENC])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_range(0,d[_MAXPWM])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(d[_MAXPWM])
                self.widgets["mesa%d_numof_tppwmgens"% boardnum].set_range(0,d[_MAXTPPWM])
                self.widgets["mesa%d_numof_tppwmgens"% boardnum].set_value(d[_MAXTPPWM])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_range(0,d[_MAXSTEP])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(d[_MAXSTEP])
                self.widgets["mesa%d_totalpins"% boardnum].set_text("%s"% d[_MAXGPIO])
                break
            self.on_gpio_update(self,boardnum)

    def on_gpio_update(self,*args):
        for boardnum in (0,1):
            title = self.widgets["mesa%d_boardtitle"% boardnum].get_active_text()
            firmware = self.widgets["mesa%d_firmware"% boardnum].get_active_text()
            for search, item in enumerate(mesafirmwaredata):
                d = mesafirmwaredata[search]
                if not d[_BOARDTITLE] == title:continue
                if d[_FIRMWARE] == firmware:      
                    i = (int(self.widgets["mesa%d_numof_pwmgens"% boardnum].get_value()) * 3)
                    j = (int(self.widgets["mesa%d_numof_stepgens"% boardnum].get_value()) * d[_STEPPINS])
                    k = (int(self.widgets["mesa%d_numof_encodergens"% boardnum].get_value()) * d[_ENCPINS])
                    l = (int(self.widgets["mesa%d_numof_tppwmgens"% boardnum].get_value()) * 6)
                    total = (d[_MAXGPIO]-i-j-k-l)
                    self.widgets["mesa%d_numof_gpio"% boardnum].set_text("%d" % total)
  
    # This method converts data from the GUI page to signal names for pncconf's mesa data variables
    # It starts by checking pin type to set up the proper lists to search
    # then depending on the pin type widget data is converted to signal names.
    # if the signal name is not in the list add it to Human_names, signal_names
    # and disc-saved signalname lists
    # if encoder, pwm, or stepper pins the related pin are also set properly
    # it does this by searching the current firmware array and finding what the
    # other related pins numbers are then changing them to the appropriate signalname.    
    def mesa_data_transfer(self,boardnum):
        for concount,connector in enumerate(self.data["mesa%d_currentfirmwaredata"% boardnum][_NUMOFCNCTRS]) :
            for pin in range(0,24):
                foundit = False
                p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                pinv = 'mesa%dc%dpin%dinv' % (boardnum,connector,pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                piter = self.widgets[p].get_active_iter()
                ptiter = self.widgets[ptype].get_active_iter()
                pintype = self.widgets[ptype].get_active_text()
                selection = self.widgets[p].get_active_text()
                print "**** INFO mesa-data-transfer:",p," selection: ",selection,"  pintype: ",pintype
                print "**** INFO mesa-data-transfer:",ptiter,piter
                # type GPIO input
                if pintype == GPIOI:
                    signaltree = self.data._gpioisignaltree
                    ptypetree = self.data._gpioliststore
                    nametocheck = human_input_names
                    signaltocheck = hal_input_names
                    addsignalto = self.data.halinputsignames
                # type gpio output and open drain
                elif pintype in (GPIOO,GPIOD):
                    signaltree = self.data._gpioosignaltree
                    ptypetree = self.data._gpioliststore
                    nametocheck = human_output_names
                    signaltocheck = hal_output_names
                    addsignalto = self.data.haloutputsignames
                #type encoder
                elif pintype in (ENCA,ENCB,ENCI,ENCM):
                    signaltree = self.data._encodersignaltree
                    ptypetree = self.data._encoderliststore
                    nametocheck = human_encoder_input_names
                    signaltocheck = hal_encoder_input_names
                    addsignalto = self.data.halencoderinputsignames
                    relatedsignals =["DUMMY",ENCB,ENCI,ENCM]
                    relatedending = ["-a","-b","-i","-m"]
                    addedending = "-a"
                    unusedname = "Unused Encoder"
                # type PWM gen
                elif pintype in( PDMP,PDMD,PDME):
                    print "pdm"
                    signaltree = self.data._pwmsignaltree
                    if pintype == PDMP:
                        ptypetree = self.data._pwmcontrolliststore
                    else:
                        ptypetree = self.data._pdmrelatedliststore
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    relatedsignals =["DUMMY",PDMD,PDME]
                    relatedending = ["-pulse","-dir","-enable"]
                    addedending = "-pulse"
                    unusedname = "Unused PWM Gen"
                elif pintype in( PWMP,PWMD,PWME):
                    print "pwm"
                    signaltree = self.data._pwmsignaltree
                    if pintype == PWMP:
                        ptypetree = self.data._pwmcontrolliststore
                    else:
                        ptypetree = self.data._pwmrelatedliststore
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    relatedsignals =["DUMMY",PWMD,PWME]
                    relatedending = ["-pulse","-dir","-enable"]
                    addedending = "-pulse"
                    unusedname = "Unused PWM Gen"
                # type step gen
                elif pintype in (STEPA,STEPB):
                    signaltree = self.data._steppersignaltree
                    ptypetree = self.data._stepperliststore
                    nametocheck = human_stepper_names
                    signaltocheck = hal_stepper_names
                    addsignalto = self.data.halsteppersignames
                    relatedsignals =["DUMMY",STEPB,STEPC,STEPD,STEPE,STEPF]
                    relatedending = ["-step","-dir","-c","-d","-e","-f"]
                    addedending = "-a"
                    unusedname = "Unused StepGen"
                else :
                    print "**** ERROR mesa-data-transfer: error unknown pin type:",pintype
                    return
                
                # **Start widget to data Convertion**                    
                # for encoder pins
                if piter == None:
                        print "callin pin changed !!!"
                        self.on_mesa_pin_changed(p,boardnum,connector,pin,True)  
                        print "back !!!"
                        selection = self.widgets[p].get_active_text()
                        piter = self.widgets[p].get_active_iter()
                        print "found signame -> ",selection," "
                # ok we have a piter with a signal type now- lets convert it to a signalname
                dummy, index = signaltree.get(piter,0,1)
                print "signaltree: ",dummy
                widgetptype, index2 = ptypetree.get(ptiter,0,1)
                print "ptypetree: ",dummy
                if pintype in (GPIOI,GPIOO,GPIOD) or (index == 0):index2 = 0
                self.data[p] = signaltocheck[index+index2]
                self.data[ptype] = widgetptype
                self.data[pinv] = self.widgets[pinv].get_active()
                print p,self.data[p],widgetptype
        self.data["mesa%d_pwm_frequency"% boardnum] = self.widgets["mesa%d_pwm_frequency"% boardnum].get_value()
        self.data["mesa%d_pdm_frequency"% boardnum] = self.widgets["mesa%d_pdm_frequency"% boardnum].get_value()
        self.data["mesa%d_watchdog_timeout"% boardnum] = self.widgets["mesa%d_watchdog_timeout"% boardnum].get_value()
  
    # If we just reloaded a config then update the page right now
    # as we already know what board /firmware /components are.
    def on_mesa0_prepare(self, *args):
        self.data.help = "help-mesa.txt"
        boardnum = 0
        if not self.widgets.createconfig.get_active() and not self.intrnldata.mesa0_configured  :
            self.set_mesa_options(boardnum,self.data.mesa0_boardtitle,self.data.mesa0_firmware,self.data.mesa0_numof_pwmgens,
                    self.data.mesa0_numof_tppwmgens,self.data.mesa0_numof_stepgens,self.data.mesa0_numof_encodergens)
        elif not self.intrnldata.mesa0_configured:
            self.widgets.mesa0con2table.hide()
            self.widgets.mesa0con3table.hide()   
            self.widgets.mesa0con4table.hide()
            self.widgets.mesa0con5table.hide()           
        self.widgets.mesa0_parportaddrs.set_text(self.data.mesa0_parportaddrs)

    def on_mesa0_next(self,*args):
        model = self.widgets.mesa0_boardtitle.get_model()
        active = self.widgets.mesa0_boardtitle.get_active()
        if active < 0:
            title = None
        else: title = model[active][0]
        if not self.intrnldata.mesa0_configured:
            self.warning_dialog(_("You need to configure the mesa0 page.\n Choose the board type, firmware, component amounts and press 'Accept component changes' button'"),True)
            self.widgets.druid1.set_page(self.widgets.mesa0)
            return True
        if not self.data.mesa0_currentfirmwaredata[_BOARDTITLE] ==  title:
            self.warning_dialog(_("The chosen Mesa0 board is different from the current displayed.\nplease press 'Accept component changes' button'"),True)
            self.widgets.druid1.set_page(self.widgets.mesa0)
            return True
        self.data.mesa0_parportaddrs = self.widgets.mesa0_parportaddrs.get_text()
        self.mesa_data_transfer(0) 
        if self.data.number_mesa > 1:
           self.widgets.druid1.set_page(self.widgets.mesa1)
           return True
        if self.data.number_pports<1:
           self.widgets.druid1.set_page(self.widgets.xaxismotor)
           return True
        else:
           self.widgets.druid1.set_page(self.widgets.pp1pport)
           return True
      
    # If we just reloaded a config then update the page right now
    # as we already know what board /firmware /components are wanted.
    def on_mesa1_prepare(self,*args):
        self.data.help = "help-mesa.txt"
        boardnum = 1
        if not self.widgets.createconfig.get_active() and not self.intrnldata.mesa1_configured  :
            self.set_mesa_options(boardnum,self.data.mesa1_boardtitle,self.data.mesa1_firmware,self.data.mesa1_numof_pwmgens,
                    self.data.mesa1_numof_tppwmgens,self.data.mesa1_numof_stepgens,self.data.mesa1_numof_encodergens)
        elif not self.intrnldata.mesa1_configured:           
            self.widgets.mesa1con2table.hide()
            self.widgets.mesa1con3table.hide()           
            self.widgets.mesa1con4table.hide()
            self.widgets.mesa1con5table.hide()
        self.widgets.mesa1_parportaddrs.set_text(self.data.mesa1_parportaddrs)

    def on_mesa1_next(self,*args):
        model = self.widgets.mesa1_boardtitle.get_model()
        active = self.widgets.mesa1_boardtitle.get_active()
        if active < 0:
            title = None
        else: title = model[active][0]
        if not self.intrnldata.mesa1_configured:
            self.warning_dialog(_("You need to configure the mesa1 page.\n Choose the board type, firmware, component amounts and press 'Accept component changes' button'"),True)
            self.widgets.druid1.set_page(self.widgets.mesa1)
            return True
        if not self.data.mesa1_currentfirmwaredata[_BOARDTITLE] ==  title:
            self.warning_dialog(_("The chosen Mesa1 board is different from the current displayed.\nplease press 'Accept component changes' button'"),True)
            self.widgets.druid1.set_page(self.widgets.mesa1)
            return True    
        self.data.mesa1_parportaddrs = self.widgets.mesa1_parportaddrs.get_text()
        self.mesa_data_transfer(1) 
        if self.data.number_pports<1:
           self.widgets.druid1.set_page(self.widgets.xaxismotor)
           return True
        
    def on_mesapanel_clicked(self, *args):
        #self.m5i20test(self)
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w") 
        halrun.write("loadrt threads period1=50000 name1=fast fp1=0 period2=1000000 name2=slow\n")
        self.hal_cmnds("LOAD")
        self.hal_cmnds("READ")
        self.hal_cmnds("WRITE")
        halrun.write("start\n")
        halrun.flush()
        time.sleep(1)
        PyApp(self,self.data,self.widgets)    
        print "back, after making panel"
        return
        for boardnum in range(0,int(self.data.number_mesa)):
            print "mesa boardnum-%d"% boardnum
            board = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_BOARDNAME]+".%d"% boardnum
            for concount,connector in enumerate(self.data["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                for pin in range (0,24):
                    firmptype,compnum = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pin+(concount*24)]
                    pinv = 'mesa%dc%dpin%dinv' % (boardnum,connector,pin)
                    ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                    pintype = self.widgets[ptype].get_active_text()
                    pininv = self.widgets[pinv].get_active()
                    truepinnum = (concount*24) + pin
                    # for output / open drain pins
                    if  pintype in (GPIOO,GPIOD):                
                        halrun.write("setp hm2_%s.gpio.%03d.is_output true\n"% (board,truepinnum ))
                        if pininv:  halrun.write("setp hm2_%s.gpio.%03d.invert_output true\n"% (board,truepinnum ))
                        halrun.write("net b%d_signal_out%d testpanel.brd.%d.switch.%d hm2_%s.gpio.%03d.out\
                                    \n"%  (boardnum,truepinnum,boardnum,truepinnum,board,truepinnum))
                    # for input pins
                    elif pintype == GPIOI:                                    
                       
                        if pininv: halrun.write("net b%d_signal_in%d hm2_%s.gpio.%03d.in_not testpanel.brd.%d.led.%d\
                            \n"%(boardnum,truepinnum,board,truepinnum,boardnum,truepinnum))
                        else:   halrun.write("net b%d_signal_in%d hm2_%s.gpio.%03d.in testpanel.brd.%d.led.%d\
                            \n"% (boardnum,truepinnum,board,truepinnum,boardnum,truepinnum))
                    # for encoder pins
                    elif pintype in (ENCA,ENCB,ENCI,ENCM):                                        
                        if not pintype == ENCA: continue                 
                        halrun.write("net b%d_enc_reset%d hm2_%s.encoder.%02d.reset testpanel.brd.%d.enc.%d.reset\
                                    \n"% (boardnum,compnum,board,compnum,boardnum,compnum))
                        halrun.write("net b%d_enc_count%d hm2_%s.encoder.%02d.count testpanel.brd.%d.enc.%d.count\
                                    \n"% (boardnum,compnum,board,compnum,boardnum,compnum))
                    # for PWM pins
                    elif pintype in (PWMP,PWMD,PWME,PDMP,PDMD,PDME):                     
                        if not pintype in (PWMP,PDMP): continue        
                        halrun.write("net b%d_pwm_enable%d hm2_%s.pwmgen.%02d.enable testpanel.brd.%d.pwm.%d.enable\
                                    \n"% (boardnum,compnum,board,compnum,boardnum,compnum)) 
                        halrun.write("net b%d_pwm_value%d hm2_%s.pwmgen.%02d.value testpanel.brd.%d.pwm.%d.value\
                                    \n"% (boardnum,compnum,board,compnum,boardnum,compnum)) 
                        halrun.write("setp hm2_%s.pwmgen.%02d.scale 10\n"% (board,compnum)) 
                    # for Stepgen pins
                    elif pintype in (STEPA,STEPB):                      
                        if not pintype == STEPA : continue                        
                        halrun.write("net b%d_step_enable%d hm2_%s.stepgen.%02d.enable testpanel.brd.%d.stp.%d.enable\
                                    \n"% (boardnum,compnum,board,compnum,boardnum,compnum))
                        halrun.write("net b%d_step_cmd%d hm2_%s.stepgen.%02d.position-cmd testpanel.brd.%d.stp.%d.cmd\
                                    \n"% (boardnum,compnum,board,compnum,boardnum,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.maxaccel 0 \n"% (board,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.maxvel 0 \n"% (board,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.steplen 2000 \n"% (board,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.stepspace 2000 \n"% (board,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.dirhold 2000 \n"% (board,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.dirsetup 2000 \n"% (board,compnum))
                    else: 
                        print "pintype error IN mesa test panel method pintype %s boardnum %d connector %d pin %d"% (pintype,boardnum,connector,pin)
        halrun.flush()
        time.sleep(.01)
    
    def on_mesa_pintype_changed(self, widget,boardnum,connector,pin):
                p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                ptype = 'mesa%dc%dpin%dtype' %  (boardnum,connector,pin)    
                old = self.data[ptype]
                new = self.widgets[ptype].get_active_text()    
                if (new == None or new == old): return 
                if old == GPIOI and new in (GPIOO,GPIOD):
                    print "switch GPIO input ",p," to output",new
                    blocksignal = "_mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)  
                    self.widgets[p].handler_block(self.intrnldata[blocksignal])
                    self.widgets[p].set_model(self.data._gpioosignaltree)
                    self.widgets[p].handler_unblock(self.intrnldata[blocksignal])  
                    self.widgets[p].set_active(0)
                    self.data[p] = UNUSED_OUTPUT
                    self.data[ptype] = new
                elif old in (GPIOO,GPIOD) and new == GPIOI:
                    print "switch GPIO output ",p,"to input"
                    blocksignal = "_mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)  
                    self.widgets[p].handler_block(self.intrnldata[blocksignal])              
                    self.widgets[p].set_model(self.data._gpioisignaltree)
                    self.widgets[p].handler_unblock(self.intrnldata[blocksignal])  
                    self.widgets[p].set_active(0)
                    self.data[p] = UNUSED_INPUT
                    self.data[ptype] = new
                elif (old == GPIOO and new == GPIOD) :
                    print "switch GPIO output ",p,"to open drain"
                    self.data[ptype] = new
                elif (old == GPIOD and new == GPIOO):
                    print "switch GPIO opendrain ",p,"to output"
                    self.data[ptype] = new
                elif old == PWMP and new == PDMP:
                    relatedpins = [PWMP,PWMD,PWME]
                    pinlist = self.data.list_related_pins(relatedpins, boardnum, connector, pin, 1)
                    for i in (pinlist):
                        if i[0] == ptype :continue
                        j = self.widgets[i[0]].get_active()
                        self.widgets[i[0]].set_model(self.data._pdmrelatedliststore)
                        self.widgets[i[0]].set_active(j)
                    #print "switch PWM  ",p,"to PDM"
                    self.data[ptype] = new
                elif old == PDMP and new == PWMP:
                    relatedpins = [PWMP,PWMD,PWME]
                    pinlist = self.data.list_related_pins(relatedpins, boardnum, connector, pin, 1)
                    for i in (pinlist):
                        if i[0] == ptype :continue
                        j = self.widgets[i[0]].get_active()
                        self.widgets[i[0]].set_model(self.data._pwmrelatedliststore)
                        self.widgets[i[0]].set_active(j)
                    #print "switch PDM  ",p,"to PWM"
                    self.data[ptype] = new
                elif old in(GPIOI,GPIOO,GPIOD) and new in (ENCA,ENCB,ENCI,ENCM):
                    print "switch ",old,"to ",new," on pin ",p
                else: print "pintype error in pinchanged method old",old,"new ",new,"\npinnumber ",p

    def on_mesa_component_value_changed(self, widget,boardnum):
        self.in_mesa_prepare = True
        self.data["mesa%d_pwm_frequency"% boardnum] = self.widgets["mesa%d_pwm_frequency"% boardnum].get_value()
        self.data["mesa%d_pdm_frequency"% boardnum] = self.widgets["mesa%d_pdm_frequency"% boardnum].get_value()
        self.data["mesa%d_watchdog_timeout"% boardnum] = self.widgets["mesa%d_watchdog_timeout"% boardnum].get_value()
        numofpwmgens = self.data["mesa%d_numof_pwmgens"% boardnum] = int(self.widgets["mesa%d_numof_pwmgens"% boardnum].get_value())
        numoftppwmgens = self.data["mesa%d_numof_tppwmgens"% boardnum] = int(self.widgets["mesa%d_numof_tppwmgens"% boardnum].get_value())
        numofstepgens = self.data["mesa%d_numof_stepgens"% boardnum] = int(self.widgets["mesa%d_numof_stepgens"% boardnum].get_value())
        numofencoders = self.data["mesa%d_numof_encodergens"% boardnum] = int(self.widgets["mesa%d_numof_encodergens"% boardnum].get_value())
        title = self.data["mesa%d_boardtitle"% boardnum] = self.widgets["mesa%d_boardtitle"% boardnum].get_active_text()
        firmware = self.data["mesa%d_firmware"% boardnum] = self.widgets["mesa%d_firmware"% boardnum].get_active_text()
        self.set_mesa_options(boardnum,title,firmware,numofpwmgens,numoftppwmgens,numofstepgens,numofencoders)
        return True

    # This method sets up the mesa GUI page and is used when changing component values / firmware or boards from config page.
    # it changes the component comboboxes according to the firmware max and user requested amounts
    # it adds signal names to the signal name combo boxes according to component type and in the
    # case of GPIO options selected on the basic page such as limit/homing types.
    # it will grey out I/O tabs according to the selected board type. 
    # it uses GTK signal blocking to block on_mesa_pin_change and on_mesa_pintype_changed methods.
    # Since this method is for intialization, there is no need to check for changes and this speeds up
    # the update.  
    # 'mesafirmwaredata' holds all the firmware data.
    # 'self.data.mesaX_currentfirmwaredata' hold the current selected firmware data (X is 0 or 1)

    def set_mesa_options(self,boardnum,title,firmware,numofpwmgens,numoftppwmgens,numofstepgens,numofencoders): 
        self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
        self.pbar.set_text("Setting up Mesa tabs")
        self.pbar.set_fraction(0)
        self.window.show()
        while gtk.events_pending():
            gtk.main_iteration()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[_BOARDTITLE] == title:continue
            if d[_FIRMWARE] == firmware:
                self.data["mesa%d_currentfirmwaredata"% boardnum] = mesafirmwaredata[search]
                break
        print mesafirmwaredata[search]
        self.widgets["mesa%dcon3table"% boardnum].set_sensitive(1) 
        self.widgets["mesa%dcon3tab"% boardnum].set_sensitive(1)
        self.widgets["mesa%dcon3table"% boardnum].show()
        self.widgets["mesa%dcon4table"% boardnum].set_sensitive(1) 
        self.widgets["mesa%dcon4tab"% boardnum].set_sensitive(1) 
        self.widgets["mesa%dcon4table"% boardnum].show() 
        if self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME] == "5i20" or self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME] == "5i23":
            self.widgets["mesa%dcon2table"% boardnum].show()
            self.widgets["mesa%dcon3table"% boardnum].show()
            self.widgets["mesa%dcon4table"% boardnum].show()
            self.widgets["mesa%dcon5table"% boardnum].hide()
        if self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME] == "5i22":
            self.widgets["mesa%dcon2table"% boardnum].show()
            self.widgets["mesa%dcon3table"% boardnum].show()
            self.widgets["mesa%dcon4table"% boardnum].show()
            self.widgets["mesa%dcon5table"% boardnum].show()   
        if self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME] == "7i43":
            self.widgets["mesa%dcon2table"% boardnum].hide()
            self.widgets["mesa%dcon3table"% boardnum].show()
            self.widgets["mesa%dcon4table"% boardnum].show()
            self.widgets["mesa%dcon5table"% boardnum].hide()
        if self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME] == "3x20":
            self.widgets["mesa%dcon2table"% boardnum].hide()
            self.widgets["mesa%dcon3table"% boardnum].hide()
            self.widgets["mesa%dcon4table"% boardnum].show()
            self.widgets["mesa%dcon5table"% boardnum].show()
            self.widgets["mesa%dcon6table"% boardnum].show()
            self.widgets["mesa%dcon7table"% boardnum].show()
            self.widgets["mesa%dcon8table"% boardnum].show()
            self.widgets["mesa%dcon9table"% boardnum].show()
        else:
            self.widgets["mesa%dcon6table"% boardnum].hide()
            self.widgets["mesa%dcon7table"% boardnum].hide()
            self.widgets["mesa%dcon8table"% boardnum].hide()
            self.widgets["mesa%dcon9table"% boardnum].hide()

        self.widgets["mesa%d"%boardnum].set_title("Mesa%d Configuration-Board: %s firmware: %s"% (boardnum,self.data["mesa%d_boardtitle"%boardnum],
            self.data["mesa%d_currentfirmwaredata"% boardnum][_FIRMWARE]))

        for concount,connector in enumerate(self.data["mesa%d_currentfirmwaredata"% boardnum][_NUMOFCNCTRS]) :
            for pin in range (0,24):
                self.pbar.set_fraction((pin+1)/24.0)
                while gtk.events_pending():
                    gtk.main_iteration()
                firmptype,compnum = self.data["mesa%d_currentfirmwaredata"% boardnum][_STARTOFDATA+pin+(concount*24)]       
                p = 'mesa%dc%dpin%d' % (boardnum, connector, pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum, connector , pin)
                print "**** INFO set-mesa-options DATA:",self.data[p],p,self.data[ptype]
                pinv = 'mesa%dc%dpin%dinv' % (boardnum, connector , pin)
                blocksignal = "_mesa%dsignalhandlerc%ipin%i" % (boardnum, connector, pin)    
                ptypeblocksignal  = "_mesa%dptypesignalhandlerc%ipin%i" % (boardnum, connector,pin)  
                actblocksignal = "_mesa%dactivatehandlerc%ipin%i"  % (boardnum, connector, pin) 
                # kill all widget signals:
                self.widgets[ptype].handler_block(self.intrnldata[ptypeblocksignal])
                self.widgets[p].handler_block(self.intrnldata[blocksignal]) 
                self.widgets[p].child.handler_block(self.intrnldata[actblocksignal])                                            
                # *** convert widget[ptype] to component specified in firmwaredata  *** 
                if self.intrnldata["mesa%d_configured"% boardnum]:
                    print "clearing data"
                    if self.data[ptype] not in (GPIOI,GPIOO,GPIOD):
                        if firmptype in ( ENCA,ENCB,ENCI,ENCM ): 
                            self.data[p] =  UNUSED_ENCODER
                        elif firmptype in ( PWMP,PWMD,PWME,PDMP,PDMD,PDME ):
                            self.data[p] =  UNUSED_PWM
                        elif firmptype in ( STEPA,STEPB ):
                            self.data[p] =  UNUSED_STEPGEN
                        elif firmptype == GPIOI:
                            self.data[p] = UNUSED_INPUT
                        else:
                            self.data[p] = UNUSED_OUTPUT
                        self.data[ptype] = firmptype
                        self.widgets[p].set_active(0) 

                # ---SETUP GUI FOR ENCODER FAMILY COMPONENT--- 
                # check that we are not converting more encoders that user requested
                # if we are then we trick this routine into thinking the firware asked for GPIO:
                # we can do that by changing the variable 'firmptype' to ask for GPIO
                if firmptype in ( ENCA,ENCB,ENCI,ENCM ): 
                    if numofencoders >= (compnum+1):
                        # if the combobox is not already displaying the right component:
                        # then we need to set up the comboboxes for this pin, otherwise skip it
                        if not self.widgets[ptype].get_active_text() == firmptype:  
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0)
                            ptmodel = self.widgets[ptype].set_model(self.data._encoderliststore)
                            pmodel = self.widgets[p].set_model(self.data._encodersignaltree)
                            # we only add every 4th human name so the user can only select
                            # the encoder's 'A' signal name. If its the other signals
                            # we can add them all because pncconf controls what the user sees
                            if firmptype == ENCA:
                                self.widgets[p].set_active(0)
                                self.widgets[p].set_sensitive(1)
                                self.widgets[ptype].set_sensitive(0)
                                self.widgets[ptype].set_active(0)
                            # pncconf control what the user sees with these ones:
                            elif firmptype in(ENCB,ENCI,ENCM):
                                self.widgets[p].set_active(0)   
                                self.widgets[p].set_sensitive(0)
                                self.widgets[ptype].set_sensitive(0)
                                for i,j in enumerate((ENCB,ENCI,ENCM)):
                                    if firmptype == j:break 
                                self.widgets[ptype].set_active(i+1)
                            # if the data stored ptype is the encoder family then use the data stored signal name
                            # else set to unused_encoder signal name 
                            # no sense in deleting the user's selected signal if it is for the right ptype
                            if self.data[ptype] == firmptype: 
                                signalindex = hal_encoder_input_names.index(self.data[p]) - pintype_encoder.index(self.data[ptype])
                                #print "dataptype:",self.data[ptype]," dataptype:",self.data[p],signalindex
                                count = -3
                                if signalindex > 0:
                                    for row,parent in enumerate(human_encoder_input_names):
                                        if parent[1][0] == None:continue
                                        for column,child in enumerate(parent[1]):
                                            count +=4
                                            #print row,column,count,parent[0],child
                                            if count == signalindex:
                                                #print "match",row,column
                                                break
                                        if count >= signalindex:break
                                    temp = (row,column)
                                else:
                                    temp = (0) # set unused encoder
                                treeiter = self.data._encodersignaltree.get_iter(temp)
                                self.widgets[p].set_active_iter(treeiter)
                            # otherwise set it to unused so the user sees it has changed                                         
                            else:
                                self.data[p] =  UNUSED_ENCODER
                                self.data[ptype] = firmptype
                                self.widgets[p].set_active(0)  
                            continue                
                    else:   
                        # user requested this encoder component to be GPIO instead
                        # We cheat a little and tell the rest of the method that the firmware says
                        # it should be GPIO
                        firmptype = GPIOI
                # --- mux encoder ---
                if firmptype in (MXEA,MXEB,MXEI,MXEM):
                    print "**** INFO: MUX ENCODER:",firmptype
                    if numofencoders >= (compnum+1):
                        # if the combobox is not already displaying the right component:
                        # then we need to set up the comboboxes for this pin, otherwise skip it
                        if 1==1:  
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0)
                            pmodel = self.widgets[p].set_model(self.data._muxencodersignaltree)
                            ptmodel = self.widgets[ptype].set_model(self.data._muxencoderliststore)
                            self.widgets[ptype].set_active(pintype_muxencoder.index(firmptype))
                            self.widgets[ptype].set_sensitive(0)
                            self.widgets[p].set_active(0)
                            if firmptype == MXEA:
                                self.widgets[p].set_sensitive(1)
                            else: 
                                self.widgets[p].set_sensitive(0)
                           
                    else:
                        firmptype = GPIOI
                # special case mux select
                if firmptype == (MXES):
                    self.widgets[pinv].set_sensitive(0)
                    self.widgets[pinv].set_active(0)
                    pmodel = self.widgets[p].set_model(self.data._muxencodersignaltree)
                    ptmodel = self.widgets[ptype].set_model(self.data._muxencoderliststore)
                    self.widgets[ptype].set_active(pintype_muxencoder.index(firmptype))
                    self.widgets[ptype].set_sensitive(0)
                    self.widgets[p].set_active(0)

                # ---SETUP GUI FOR PWM FAMILY COMPONENT---
                # the user has a choice of pulse width or pulse density modulation

                elif firmptype in ( PWMP,PWMD,PWME,PDMP,PDMD,PDME ):
                    if numofpwmgens >= (compnum+1):
                        if 1==1 :
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0)
                            pmodel = self.widgets[p].set_model(self.data._pwmsignaltree)
                            
                            # only add the -pulse signal names for the user to see
                            if firmptype in(PWMP,PDMP):
                                ptmodel = self.widgets[ptype].set_model(self.data._pwmcontrolliststore)
                                self.widgets[ptype].set_sensitive(1)
                                self.widgets[p].set_sensitive(1)
                                self.widgets[p].set_active(0)
                                self.widgets[ptype].set_active(0)
                            # add them all here      
                            elif firmptype in (PWMD,PWME,PDMD,PDME):
                                if firmptype in (PWMD,PWME):
                                    ptmodel = self.widgets[ptype].set_model(self.data._pwmrelatedliststore)
                                else:
                                    ptmodel = self.widgets[ptype].set_model(self.data._pdmrelatedliststore)
                                self.widgets[p].set_sensitive(0)
                                self.widgets[p].set_active(0) 
                                self.widgets[ptype].set_sensitive(0)
                                temp = 1
                                if firmptype in (PWME,PDME): temp = 2
                                self.widgets[ptype].set_active(temp)
                # This is for PWM conversions
                # check to see data is already set to PWM family
                # set the ptype to PWM or PDM 
                # if in PWM family - set to widget signal name 
                # else change to unused_PWM signal name
                            dataptype = self.data[ptype]
                            print dataptype
                            if dataptype in (PWMD,PWME) and not  dataptype == PWMP:
                                print "Model PWM"
                                ptmodel = self.widgets[ptype].set_model(self.data._pwmrelatedliststore)
                            elif dataptype in (PDMD,PDME) and not dataptype == PDMP:
                                print "Model PDM"
                                ptmodel = self.widgets[ptype].set_model(self.data._pdmrelatedliststore)
                            if  dataptype in ( PWMP,PWMD,PWME,PDMP,PDMD,PDME ) :
                                if dataptype in(PWMP,PDMP):
                                    self.widgets[ptype].set_active(0 +(dataptype == PDMP) )
                                    ptypeindex = 0
                                elif dataptype in (PWMD,PWME,PDMD,PDME):
                                    temp = 1
                                    if dataptype in (PWME,PDME): temp = 2
                                    self.widgets[ptype].set_active(temp)
                                if dataptype in (PWMD,PWME):
                                    ptypeindex = pintype_pwm.index(dataptype)
                                elif dataptype in (PDMD,PDME):
                                    ptypeindex = pintype_pdm.index(dataptype)
                                signalindex = hal_pwm_output_names.index(self.data[p])
                                print "dataptype:",self.data[ptype]," dataptype:",self.data[p],signalindex,ptypeindex
                                count = -2
                                if signalindex > 0:
                                    for row,parent in enumerate(human_pwm_output_names):
                                        if row == 0: continue
                                        if parent[1][0] == None:
                                                count += 3
                                                print row,count,parent[0]
                                                if count == signalindex - ptypeindex:
                                                    print "match",row
                                                    temp = (row)
                                                    break
                                                continue
                                        for column,child in enumerate(parent[1]):
                                            count +=3
                                            print row,column,count,parent[0],child
                                            if count == signalindex - ptypeindex:
                                                print "match",row
                                                temp = (row,column)
                                                break
                                        if count >= signalindex:break
                                else:
                                    temp = (0) # set unused pwm
                                treeiter = self.data._pwmsignaltree.get_iter(temp)
                                self.widgets[p].set_active_iter(treeiter)                          

                            else:
                                self.data[p] =  UNUSED_PWM
                                self.data[ptype] = firmptype
                                self.widgets[p].set_active(0) 
                                if firmptype in (PWMP,PWMD,PWME):self.widgets[ptype].set_active(0)
                                else:self.widgets[ptype].set_active(1) 
                            continue
                    else:
                        firmptype = GPIOI
                # ---SETUP GUI FOR TP PWM FAMILY COMPONENT---   
                elif firmptype in ( TPPWMA,TPPWMB,TPPWMC,TPPWMAN,TPPWMBN,TPPWMCN,TPPWME,TPPWMF ):
                    if numoftppwmgens >= (compnum+1):
                        if not self.widgets[ptype].get_active_text() == firmptype:
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0)
                            pmodel = self.widgets[p].set_model(self.data._tppwmsignaltree)
                            ptmodel = self.widgets[ptype].set_model(self.data._tppwmliststore)
                            self.widgets[ptype].set_sensitive(0)
                            self.widgets[ptype].set_active(pintype_tp_pwm.index(firmptype))
                            self.widgets[p].set_active(0)
                            # only add the -a signal names for the user to change
                            if firmptype == TPPWMA:
                                self.widgets[p].set_sensitive(1)
                            # the rest the user can't change      
                            else:
                                self.widgets[p].set_sensitive(0)
                    else:
                        firmptype = GPIOI
                # ---SETUP FOR STEPPER FAMILY COMPONENT---
                elif firmptype in (STEPA,STEPB):
                    if numofstepgens >= (compnum+1):
                        if 1 == 1:
                            self.widgets[pinv].set_sensitive(1)
                            self.widgets[pinv].set_active(self.data[pinv])
                            ptmodel = self.widgets[ptype].set_model(self.data._stepperliststore)
                            pmodel = self.widgets[p].set_model(self.data._steppersignaltree)
                            if firmptype == STEPA:
                                steptype = 0
                                self.widgets[p].set_sensitive(1)
                                self.widgets[ptype].set_sensitive(0)
                            elif firmptype == STEPB:
                                steptype = 1
                                self.widgets[p].set_sensitive(0)
                                self.widgets[ptype].set_sensitive(0)
                            if self.data[ptype] == firmptype:
                                self.widgets[ptype].set_active(steptype)
                                self.widgets[p].set_active(0)
                                signalindex = hal_stepper_names.index(self.data[p])
                                ptypeindex = pintype_stepper.index(firmptype)
                                count = -5
                                #print "stepper,dataptype:",self.data[ptype]," dataptype:",self.data[p],signalindex,ptypeindex
                                if signalindex > 0:
                                    for row,parent in enumerate(human_stepper_names):
                                            if row == 0:continue
                                            if parent[1][0] == None:
                                                count += 6
                                                #print row,column,count,parent[0]
                                                if count == signalindex - ptypeindex:
                                                    #print "match",row
                                                    temp = (row)
                                                    break
                                                continue
                                            for column,child in enumerate(parent[1]):
                                                count +=6
                                                #print row,column,count,parent[0],child
                                                if count == signalindex - ptypeindex:
                                                    #print "match",row
                                                    temp = (row,column)
                                                    break
                                            if count >= signalindex:break
                                else:
                                    temp = (0) # set unused stepper
                                treeiter = self.data._steppersignaltree.get_iter(temp)
                                self.widgets[p].set_active_iter(treeiter)     
                                #model = self.widgets[p].get_model()
                                #for search,item in enumerate(model):
                                #    if model[search][0]  == human_stepper_names[hal_stepper_names.index(self.data[p])]:
                                 #       self.widgets[p].set_active(search)
                                 #       break
                            else:
                                self.data[p] =  UNUSED_STEPGEN
                                self.data[pinv] = 0
                                self.data[ptype] = firmptype
                                self.widgets[p].set_active(0)
                                self.widgets[ptype].set_active(steptype)                     
                            continue
                    else:firmptype = GPIOI
                # ---SETUP FOR GPIO FAMILY COMPONENT---
                # first check to see if firmware says it should be in GPIO family
                # (note this can be because firmware says it should be some other 
                # type but the user wants to deselect it so as to use it as GPIO
                # this is done in the firmptype checks before this check. 
                # They will change firmptype variable to GPIOI)       
                # check if firmptype is in GPIO family
                # check if widget is already configured
                # check to see if data says it is in GPIO family
                # if not change datatype to GPIOI and signal to unused input
                # block GTK signals from widget pintype and add names to ptype combobox
                # block GTK signals from widget pin 
                # if GPIOI then add input signal names to pin combobox exclude unselected signal names
                # if not then add output signal names 
                if firmptype in (GPIOI,GPIOO,GPIOD): 
                    if not self.widgets[ptype].get_active_text() in (GPIOI,GPIOO,GPIOD):
                        self.widgets[p].set_sensitive(1)
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[ptype].set_sensitive(1)
                        self.widgets[ptype].set_model(self.data._gpioliststore)
                        if self.data[ptype] in (GPIOI,GPIOO,GPIOD):
                            tempptype = self.data[ptype]
                            tempp = self.data[p]
                        else:
                            tempptype = firmptype
                            tempp = UNUSED_INPUT # unused gpio
                        # signal names for GPIO INPUT
                        if tempptype == GPIOI:  
                            self.widgets[ptype].set_active(0)
                            self.widgets[pinv].set_active(self.data[pinv])
                            human = human_input_names
                            signal = hal_input_names
                            tree = self.data._gpioisignaltree
                        # signal names for GPIO OUTPUT and OPEN DRAIN OUTPUT
                        elif tempptype in (GPIOO,GPIOD):
                            if firmptype == GPIOO: self.widgets[ptype].set_active(2)
                            else: self.widgets[ptype].set_active(1)
                            human = human_output_names
                            signal = hal_output_names
                            tree = self.data._gpioosignaltree

                        self.widgets[p].set_model(tree)
                        signalindex = signal.index(tempp)
                        ptypeindex = pintype_gpio.index(firmptype)
                        ##print "gpio temp ptype:",tempptype," temp p:",tempp,signalindex,ptypeindex
                        count = 0
                        temp = (0) # set unused gpio
                        if signalindex > 0:
                            for row,parent in enumerate(human):
                                if parent[1][0] == None:continue
                                for column,child in enumerate(parent[1]):
                                    count +=1
                                    #print row,column,count,parent[0],child
                                    if count == signalindex- ptypeindex:
                                        #print "match",row,column
                                        break
                                if count >= signalindex:break
                            temp = (row,column)
                        treeiter = tree.get_iter(temp)
                        self.widgets[p].set_active_iter(treeiter)
                        self.widgets[pinv].set_active(self.data[pinv])
        
        self.data["mesa%d_numof_stepgens"% boardnum] = numofstepgens
        self.data["mesa%d_numof_pwmgens"% boardnum] = numofpwmgens
        self.data["mesa%d_numof_encodergens"% boardnum] = numofencoders
        temp = (numofstepgens * self.data["mesa%d_currentfirmwaredata"% boardnum][_STEPPINS])
        temp1 = (numofencoders * self.data["mesa%d_currentfirmwaredata"% boardnum][_ENCPINS])
        temp2 = (numofpwmgens * 3)
        total = (self.data["mesa%d_currentfirmwaredata"% boardnum][_MAXGPIO]-temp-temp1-temp2)
        self.data["mesa%d_numof_gpio"% boardnum] = total     
        self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(numofstepgens)
        self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(numofencoders)      
        self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(numofpwmgens)
        self.in_mesa_prepare = False   
        self.intrnldata["mesa%d_configured"% boardnum] = True
        # unblock all the widget signals again
        for concount,connector in enumerate(self.data["mesa%d_currentfirmwaredata"% boardnum][_NUMOFCNCTRS]) :
            for pin in range (0,24):      
                p = 'mesa%dc%dpin%d' % (boardnum, connector, pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum, connector , pin)
                blocksignal = "_mesa%dsignalhandlerc%ipin%i" % (boardnum, connector, pin)    
                ptypeblocksignal  = "_mesa%dptypesignalhandlerc%ipin%i" % (boardnum, connector,pin)  
                actblocksignal = "_mesa%dactivatehandlerc%ipin%i"  % (boardnum, connector, pin) 
                self.widgets[ptype].handler_unblock(self.intrnldata[ptypeblocksignal])
                self.widgets[p].handler_unblock(self.intrnldata[blocksignal]) 
                self.widgets[p].child.handler_unblock(self.intrnldata[actblocksignal])          
        self.window.hide()
        self.widgets.druid1.set_buttons_sensitive(1,1,1,1)

    def fill_pintype_model(self):
        # gpio
        self.data._gpioliststore = gtk.ListStore(str,int)
        for number,text in enumerate(pintype_gpio):
            self.data._gpioliststore.append([text,number])
        # stepper
        self.data._stepperliststore = gtk.ListStore(str,int)
        for number,text in enumerate(pintype_stepper):
            self.data._stepperliststore.append([text,number])
        # encoder
        self.data._encoderliststore = gtk.ListStore(str,int)
        for number,text in enumerate(pintype_encoder):
            self.data._encoderliststore.append([text,number])
        # mux encoder
        self.data._muxencoderliststore = gtk.ListStore(str,int)
        for number,text in enumerate(pintype_muxencoder):
            self.data._muxencoderliststore.append([text,number])
        # pwm
        self.data._pwmrelatedliststore = gtk.ListStore(str,int)
        for number,text in enumerate(pintype_pwm):
            self.data._pwmrelatedliststore.append([text,number])
        self.data._pdmrelatedliststore = gtk.ListStore(str,int)
        for number,text in enumerate(pintype_pdm):
            self.data._pdmrelatedliststore.append([text,number])
        self.data._pwmcontrolliststore = gtk.ListStore(str,int)
        self.data._pwmcontrolliststore.append([pintype_pwm[0],0])
        self.data._pwmcontrolliststore.append([pintype_pdm[0],0])
        #tppwm
        self.data._tppwmliststore = gtk.ListStore(str,int)
        for number,text in enumerate(pintype_tp_pwm):
            self.data._tppwmliststore.append([text,number])

    def fill_combobox_models(self):
        templist = [ ["_gpioosignaltree",human_output_names,1],["_gpioisignaltree",human_input_names,1],["_encodersignaltree",human_encoder_input_names,4],
                     ["_pwmsignaltree",human_pwm_output_names,3],["_tppwmsignaltree",human_tppwm_output_names,8],["_steppersignaltree",human_stepper_names,6],
                     ["_muxencodersignaltree",human_encoder_input_names,4] ]
        for item in templist:
            count = 0
            end = len(item[1])-1
            self.data[item[0]]= gtk.TreeStore(str,int)
            for i,parent in enumerate(item[1]):
                if parent[1][0] == None:
                    if i == end:temp = 0
                    else:temp = count
                    piter = self.data[item[0]].append(None, [parent[0], temp])
                    if count == 0: count = 1
                    else: count +=item[2]
                else:
                    piter = self.data[item[0]].append(None, [parent[0],0])
                    for j,child in enumerate(parent[1]):
                        #print i,count,parent[0],child
                        self.data[item[0]].append(piter, [child, count])
                        count +=item[2]

    # This is for when a user picks a signal name or creates a custom signal (by pressing enter)
    # if searches for the 'related pins' of a component so it can update them too
    # it also handles adding and updating custom signal names
    def on_mesa_pin_changed(self, widget, boardnum, connector, pin, custom):
                p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                pinchanged =  self.widgets[p].get_active_text()
                piter = self.widgets[p].get_active_iter()
                widgetptype = self.widgets[ptype].get_active_text()
                print "*** INFO mesa-pin-changed: pin:",p,"custom:",custom
                print "*** INFO mesa-pin-changed: ptype:",widgetptype,"pinchaanged:",pinchanged
                if piter == None and not custom:
                    print "*** INFO mesa-pin-changed: no iter and not custom"
                    return
                if widgetptype in (ENCB,ENCI,ENCM,
                                    MXEB,MXEI,MXEM,MXES,
                                    STEPB,STEPC,STEPD,STEPE,STEPF,
                                    PDMD,PDME,PWMD,PWME,
                                    TPPWMB,TPPWMC,TPPWMAN,TPPWMBN,TPPWMCN,TPPWME,TPPWMF
                                    ):return
                # for GPIO output
                if widgetptype in (GPIOO,GPIOD):
                    #print"ptype GPIOO\n"
                    signaltree = self.data._gpioosignaltree
                    halsignallist = hal_output_names
                    humansignallist = human_output_names
                    addsignalto = self.data.haloutputsignames
                    relatedsearch = ["dummy"]
                    relatedending = [""]
                # for GPIO input
                elif widgetptype == GPIOI:
                    #print"ptype GPIOI\n"
                    signaltree = self.data._gpioisignaltree
                    halsignallist = hal_input_names
                    humansignallist = human_input_names
                    addsignalto = self.data.halinputsignames
                    relatedsearch = ["dummy"]
                    relatedending = [""]
                # for stepgen pins
                elif widgetptype == STEPA:
                    #print"ptype step\n"
                    signaltree = self.data._steppersignaltree
                    halsignallist = hal_stepper_names
                    humansignallist = human_stepper_names
                    addsignalto = self.data.halsteppersignames
                    relatedsearch = [STEPA,STEPB,STEPC,STEPD,STEPE,STEPF]
                    relatedending = ["-step","-dir","-c","-d","-e","-f"]
                # for encoder pins
                elif widgetptype == ENCA: 
                    #print"\nptype encoder"
                    signaltree = self.data._encodersignaltree
                    halsignallist = hal_encoder_input_names
                    humansignallist = human_encoder_input_names
                    addsignalto = self.data.halencoderinputsignames
                    relatedsearch = [ENCA,ENCB,ENCI,ENCM]
                    relatedending = ["-a","-b","-i","-m"]
                # for mux encoder pins
                elif widgetptype == MXEA: 
                    #print"\nptype encoder"
                    signaltree = self.data._muxencodersignaltree
                    halsignallist = hal_encoder_input_names
                    humansignallist = human_encoder_input_names
                    addsignalto = self.data.halencoderinputsignames
                    relatedsearch = [MXEA,MXEB,MXEI,MXEM]
                    relatedending = ["-a","-b","-i","-m"]
                # for PWM pins
                elif widgetptype == PWMP: 
                    print"ptype pwmp\n"
                    signaltree = self.data._pwmsignaltree
                    halsignallist = hal_pwm_output_names
                    humansignallist = human_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    relatedsearch = [PWMP,PWMD,PWME]
                    relatedending = ["-pulse","-dir","-enable"]
                # for PDM pins
                elif widgetptype == PDMP: 
                    print"ptype pdmp\n"
                    signaltree = self.data._pwmsignaltree
                    halsignallist = hal_pwm_output_names
                    humansignallist = human_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    relatedsearch = [PWMP,PWMD,PWME]
                    relatedending = ["-pulse","-dir","-enable"]
                elif widgetptype == TPPWMA: 
                    print"ptype pdmp\n"
                    signaltree = self.data._tppwmsignaltree
                    halsignallist = hal_tppwm_output_names
                    humansignallist = human_tppwm_output_names
                    addsignalto = self.data.haltppwmoutputsignames
                    relatedsearch = [TPPWMA,TPPWMB,TPPWMC,TPPWMAN,TPPWMBN,TPPWMCN,TPPWME,TPPWMF]
                    relatedending = ["-a","-b","c","-anot","-bnot","cnot","-enable","-fault"]
                else: 
                    print"**** INFO: pncconf on_mesa_pin_changed:  pintype not found:%s\n",widgetptype
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
                    print "*** INFO mesa-pin-changed: PITER:",piter," length:",len(signaltree)
                    if pinchanged in (addsignalto):return
                    for i in (humansignallist):
                        if pinchanged == i[0]:return
                        if pinchanged in i[1]:return
                    length = len(signaltree)
                    index = len(halsignallist) - len(relatedsearch)
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
                print "*** INFO mesa-pin-changed: index",index
                # This finds the pin type and component number of the pin that has changed
                pinlist = []
                if widgetptype in(GPIOI,GPIOO,GPIOD):
                    pinlist = [["%s"%p,boardnum,connector,pin]]
                else:
                    pinlist = self.data.list_related_pins(relatedsearch, boardnum, connector, pin, 0)
                    
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
                    endoftree = len(signaltree)-1
                    customiter = signaltree.get_iter((endoftree,))
                    newiter = signaltree.append(customiter, [legal_name,index])
                    for offset,i in enumerate(relatedsearch):
                        with_endings = legal_name + relatedending[offset]
                        print "new signal:",with_endings
                        halsignallist.append ((with_endings))
                for data in(pinlist):
                    blocksignal = "_mesa%dsignalhandlerc%ipin%i" % (data[1], data[2], data[3]) 
                    self.widgets[data[0]].handler_block(self.intrnldata[blocksignal])
                    blocksignal = "_mesa%dactivatehandlerc%ipin%i"  % (data[1], data[2], data[3]) 
                    self.widgets[data[0]].child.handler_block(self.intrnldata[blocksignal])
                    if custom:
                        self.widgets[data[0]].set_active_iter(newiter)
                    else:
                        self.widgets[data[0]].set_active_iter(piter)
                    self.widgets[data[0]].child.handler_unblock(self.intrnldata[blocksignal])
                    blocksignal = "_mesa%dsignalhandlerc%ipin%i" % (data[1], data[2], data[3]) 
                    self.widgets[data[0]].handler_unblock(self.intrnldata[blocksignal])

    def on_pp1pport_prepare(self, *args):
        self.data.help = 5
        self.in_pport_prepare = True
        self.prepare_parport("pp1")
        c = self.data.pp1_direction
        if c:
            self.widgets.pp1pport.set_title(_("First Parallel Port set for OUTPUT"))
        else:
            self.widgets.pp1pport.set_title(_("First Parallel Port set for INPUT"))   

    def on_pp1pport_next(self, *args):
        self.next_parport("pp1")
        if self.data.number_pports<2:
            self.widgets.druid1.set_page(self.widgets.xaxismotor)
            return True

    def on_pp1pport_back(self, *args):
        if self.data.number_mesa == 2:
            self.widgets.druid1.set_page(self.widgets.mesa1)
            return True
        elif self.data.number_mesa == 1:
            self.widgets.druid1.set_page(self.widgets.mesa0)
            return True
        elif not self.data.number_mesa:
            self.widgets.druid1.set_page(self.widgets.GUIconfig)
            return True

    def on_pp2pport_prepare(self, *args):
         self.data.help = 5
         self.prepare_parport("pp2")
         c = self.data.pp2_direction
         if c:
            self.widgets.pp2pport.set_title(_("Second Parallel Port set for OUTPUT"))
         else:
            self.widgets.pp2pport.set_title(_("Second Parallel Port set for INPUT"))

    def on_pp2pport_next(self, *args):
        self.next_parport("pp2")
        if self.data.number_pports<3:
                self.widgets.druid1.set_page(self.widgets.xaxismotor)
                return True

    def on_pp3pport_prepare(self, *args):
         self.prepare_parport("pp3")
         c = self.data.pp3_direction
         if c:
                self.widgets.pp3pport.set_title(_("Third Parallel Port set for OUTPUT"))
         else:
                self.widgets.pp3pport.set_title(_("Third Parallel Port set for INPUT"))
  
    def on_pp3pport_next(self, *args):
        self.data.help = 5
        self.next_parport("pp3")

    def prepare_parport(self,portname):
        self.data.help = "help-parport.txt"
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = '%sOpin%d' % (portname,pin)
            model = self.widgets[p].get_model()
            model.clear()
            for name in human_output_names: model.append((name,))
            self.widgets[p].set_active(hal_output_names.index(self.data[p]))
            self.widgets[p].set_wrap_width(3)
            p = '%sOpin%dinv' % (portname, pin)
            self.widgets[p].set_active(self.data[p])
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = '%sIpin%d' % (portname, pin)
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
            for search,item in enumerate(model):
                if model[search][0]  == human_input_names[hal_input_names.index(self.data[p])]:
                    self.widgets[p].set_active(search)
            self.widgets[p].set_wrap_width(3)
            p = '%sIpin%dinv' % (portname, pin)
            self.widgets[p].set_active(self.data[p])
        self.in_pport_prepare = False
        c = self.data[portname+"_direction"]
        for pin in (2,3,4,5,6,7,8,9):
            p = '%sOpin%dlabel' % (portname, pin)
            self.widgets[p].set_sensitive(c)
            p = '%sOpin%dinv' % (portname, pin)
            self.widgets[p].set_sensitive(c)
            p = '%sOpin%d' % (portname, pin)
            self.widgets[p].set_sensitive(c)
            if not c :self.widgets[p].set_active(hal_output_names.index("unused-output"))
            p = '%sIpin%dlabel' % (portname, pin)
            self.widgets[p].set_sensitive(not c)
            p = '%sIpin%d' % (portname, pin)
            self.widgets[p].set_sensitive(not c)
            if c :self.widgets[p].set_active(hal_input_names.index("unused-input"))
            p = '%sIpin%dinv' % (portname, pin)
            self.widgets[p].set_sensitive(not c)

    def next_parport(self,portname):
        #check input pins
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):           
            p = '%sIpin%d' % (portname, pin)       
            foundit = 0
            selection = self.widgets[p].get_active_text()
            for index , i in enumerate(human_input_names):
                if selection == i : 
                    foundit = True
                    break               
            if not foundit:
                selection = selection.replace(" ","_")
                model = self.widgets[p].get_model()
                model.append((selection,))
                g = human_input_names
                g.append ((selection))
                hal_input_names.append ((selection))
                self.data.halinputsignames.append ((selection))
            self.data[p] = hal_input_names[index]
            p = '%sIpin%dinv' % (portname, pin)
            self.data[p] = self.widgets[p].get_active()
        # check output pins
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):           
            foundit = 0
            p = '%sOpin%d' % (portname, pin)
            selection = self.widgets[p].get_active_text()
            for i in human_output_names:
               if selection == i : foundit = 1
            if not foundit:
                selection = selection.replace(" ","_")
                model = self.widgets[p].get_model()
                model.append((selection,))
                g = human_output_names
                g.append ((selection))
                hal_output_names.append ((selection))
                self.data.haloutputsignames.append ((selection))
            self.data[p] = hal_output_names[self.widgets[p].get_active()]
            p = '%sOpin%dinv' % (portname, pin)
            self.data[p] = self.widgets[p].get_active() 
    
    def on_parportpanel_clicked(self, *args):self.parporttest(self)
        
    def signal_sanity_check(self, *args):
        warnings = []
        do_warning = False
        for i in self.data.available_axes:
            step = self.data.findsignal(i+"-stepgen-step")
            enc = self.data.findsignal(i+"-encoder-a")
            pwm = self.data.findsignal(i+"-pwm-pulse")

            if i == 's':
                if step and pwm:
                    warnings.append(_("You can not have both steppers and pwm signals for spindle control\n") )
                    do_warning = True
                continue
            if not step and not pwm:
                warnings.append(_("You forgot to designate a stepper or pwm signal for axis %s\n")% i)
                do_warning = True
            if pwm and not enc: 
                warnings.append(_("You forgot to designate an encoder signal for axis %s servo\n")% i)
                do_warning = True
            if enc and not pwm and not step: 
                warnings.append(_("You forgot to designate a pwm signal or stepper signal for axis %s\n")% i)
                do_warning = True
            if step and pwm: 
                warnings.append(_("You can not have both steppers and pwm signals for axis %s\n")% i)
                do_warning = True
        if self.data.frontend == _TOUCHY:# TOUCHY GUI
            abort = self.data.findsignal("abort")
            cycle = self.data.findsignal("cycle-start")
            single = self.data.findsignal("single-step")
            mpg = self.data.findsignal("select-mpg-a")
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
            if not self.data.externalmpg:
                warnings.append(_("Touchy require 'external mpg jogging' to be selected on the external control page\n"))
                do_warning = True
            if self.data.multimpg:
                warnings.append(_("Touchy require the external mpg to be in 'shared mpg' mode on the external controls page\n"))
                do_warning = True
            if self.data.incrselect:
                warnings.append(_("Touchy require selectable increments to be unchecked on the external controls page\n"))
                do_warning = True
        if do_warning: self.warning_dialog("\n".join(warnings),True)

    def on_xaxismotor_prepare(self, *args):
        self.data.help = "help-axismotor.txt"
        self.signal_sanity_check()
        self.axis_prepare('x')
    def on_xaxismotor_next(self, *args):  
        self.data.help = "help-axisconfig.txt"   
        self.axis_done('x')
        self.widgets.druid1.set_page(self.widgets.xaxis)
        return True
    def on_xaxismotor_back(self, *args):
        self.axis_done('x')  
        if self.data.number_pports==1:
                self.widgets.druid1.set_page(self.widgets.pp1pport)
                return True
        elif self.data.number_pports==2:
                self.widgets.druid1.set_page(self.widgets.pp2pport)
                return True
        elif self.data.number_pports==3:
                self.widgets.druid1.set_page(self.widgets.pp3pport)
                return True
        elif self.data.number_mesa == 2:
                self.widgets.druid1.set_page(self.widgets.mesa1)
                return True   
        elif self.data.number_mesa == 1:
                self.widgets.druid1.set_page(self.widgets.mesa0)
                return True 
 
    def on_yaxismotor_prepare(self, *args):
        self.data.help = "help-axismotor.txt"
        self.axis_prepare('y')
    def on_yaxismotor_next(self, *args):
        self.data.help = "help-axisconfig.txt"
        self.axis_done('y')
        self.widgets.druid1.set_page(self.widgets.yaxis)
        return True
    def on_yaxismotor_back(self, *args):      
        self.axis_done('y')  
        self.widgets.druid1.set_page(self.widgets.xaxis)
        return True
    
    def on_zaxismotor_prepare(self, *args):
        self.data.help = "help-axismotor.txt"
        self.axis_prepare('z')
    def on_zaxismotor_next(self, *args):
        self.data.help = "help-axisconfig.txt"
        self.axis_done('z')
        self.widgets.druid1.set_page(self.widgets.zaxis)
        return True
    def on_zaxismotor_back(self, *args):   
        self.axis_done('z')  
        if self.data.axes == 2:
            self.widgets.druid1.set_page(self.widgets.xaxis)
            return True    
        else:
            self.widgets.druid1.set_page(self.widgets.yaxis)
            return True

    def on_aaxismotor_prepare(self, *args):
        self.data.help = "help-axismotor.txt"
        self.axis_prepare('a')
    def on_aaxismotor_next(self, *args):
        self.data.help = "help-axisconfig.txt"
        self.axis_done('a')
        self.widgets.druid1.set_page(self.widgets.aaxis)
        return True
    def on_aaxismotor_back(self, *args):   
        self.axis_done('a')      
        self.widgets.druid1.set_page(self.widgets.zaxis)
        return True

    def on_xcalculatescale_clicked(self, *args): self.calculate_scale('x')
    def on_ycalculatescale_clicked(self, *args): self.calculate_scale('y')
    def on_zcalculatescale_clicked(self, *args): self.calculate_scale('z')
    def on_acalculatescale_clicked(self, *args): self.calculate_scale('a')
    def on_scalculatescale_clicked(self, *args): self.calculate_scale('s')

    def axis_prepare(self, axis):
        d = self.data
        w = self.widgets
        def set_text(n): w[axis + n].set_text("%s" % d[axis + n])
        def set_value(n): w[axis + n].set_value(d[axis + n])
        def set_active(n): w[axis + n].set_active(d[axis + n])
        stepdriven = encoder = pwmgen = False
        if self.data.findsignal(axis+"-stepgen-step"): stepdriven = True
        if self.data.findsignal(axis+"-encoder-a"): encoder = True
        if self.data.findsignal(axis+"-pwm-pulse"): pwmgen = True
        model = w[axis+"drivertype"].get_model()
        model.clear()
        for i in drivertypes:
            model.append((i[1],))
        model.append((_("Custom"),))   
        w["steprev"].set_text("%s" % d[axis+"steprev"])
        w["microstep"].set_text("%s" % d[axis +"microstep"])
        set_value("P")
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
        set_value("outputoffset")
        set_active("invertmotor")
        set_active("invertencoder")  
        set_value("maxoutput")
        w["motor_pulleydriver"].set_value(d[axis +"motor_pulleydriver"])
        w["motor_pulleydriven"].set_value(d[axis +"motor_pulleydriven"])
        w["encoder_pulleydriver"].set_value(d[axis +"encoder_pulleydriver"])
        w["encoder_pulleydriven"].set_value(d[axis +"encoder_pulleydriven"])
        w["motor_leadscrew"].set_value(d[axis +"motor_leadscrew"])
        w["encoder_leadscrew"].set_value(d[axis +"encoder_leadscrew"])
        w["encoderline"].set_value((d[axis+"encodercounts"]/4))
        set_text("encodercounts")
        set_value("stepscale")
        set_value("encoderscale")
        w[axis+"maxvel"].set_value(d[axis+"maxvel"]*60)
        set_value("maxacc")
        w[axis + "servo_info"].set_sensitive(encoder) 
        w[axis + "invertencoder"].set_sensitive(encoder)
        w[axis + "encoderscale"].set_sensitive(encoder)
        w[axis + "stepper_info"].set_sensitive(stepdriven) 
        w[axis + "stepscale"].set_sensitive(stepdriven)
        w[axis + "drivertype"].set_active(self.drivertype_toindex(axis))
        if w[axis + "drivertype"].get_active_text()  == _("Custom"):
            w[axis + "steptime"].set_value(d[axis + "steptime"])
            w[axis + "stepspace"].set_value(d[axis + "stepspace"])
            w[axis + "dirhold"].set_value(d[axis + "dirhold"])
            w[axis + "dirsetup"].set_value(d[axis + "dirsetup"])
        gobject.idle_add(lambda: self.update_pps(axis))

        if axis == 's':
            self.widgets.spidcontrol.set_active( self.data.spidcontrol )        
            
            w["labelmotor_pitch"].set_text(_("Gearbox Reduction Ratio"))
            w["labelencoder_pitch"].set_text(_("Gearbox Reduction Ratio"))
            w["motor_screwunits"].set_text((""))
            w["encoder_screwunits"].set_text((""))        
            w.sencodercounts.set_sensitive(encoder)
            w[axis + "invertencoder"].set_sensitive(encoder)
            w["soutputscale"].set_sensitive(pwmgen)
            w["soutputoffset"].set_sensitive(pwmgen)
            w["smaxoutput"].set_sensitive(pwmgen)
            w["sservo_info"].set_sensitive(pwmgen)
            self.on_spidcontrol_toggled()
            w["saxistest"].set_sensitive(pwmgen)
            w["sstepper_info"].set_sensitive(stepdriven)    
        else:
            set_value("maxferror")
            set_value("minferror")
            set_text("compfilename")
            set_active("comptype")
            set_value("backlash")
            set_active("usecomp")      
            set_text("homepos")
            set_text("minlim")
            set_text("maxlim")
            set_text("homesw")
            w[axis+"homesearchvel"].set_text("%d" % (d[axis+"homesearchvel"]*60))
            w[axis+"homelatchvel"].set_text("%d" % (d[axis+"homelatchvel"]*60))
            w[axis+"homefinalvel"].set_text("%d" % (d[axis+"homefinalvel"]*60))
            set_active("searchdir")
            set_active("latchdir")
            set_active("usehomeindex")
            if axis == "a":
                w["labelmotor_pitch"].set_text(_("Reduction Ratio"))
                w["labelencoder_pitch"].set_text(_("Reduction Ratio"))
                w["motor_screwunits"].set_text(_("degrees / rev"))
                w["encoder_screwunits"].set_text(_("degrees / rev"))
                w[axis + "velunits"].set_text(_("degrees / min"))
                w[axis + "accunits"].set_text(_("degrees / sec²"))
                w[axis + "homevelunits"].set_text(_("degrees / min"))
                w[axis + "homelatchvelunits"].set_text(_("degrees / min"))
                w[axis + "homefinalvelunits"].set_text(_("degrees / min"))
                w[axis + "accdistunits"].set_text(_("degrees"))
                if stepdriven:
                    w["resolutionunits1"].set_text(_("degree / Step"))        
                    w["scaleunits"].set_text(_("Steps / degree"))
                else:
                    w[ "resolutionunits1"].set_text(_("degrees / encoder pulse"))
                    w["scaleunits"].set_text(_("Encoder pulses / degree"))
                w[axis + "minfollowunits"].set_text(_("degrees"))
                w[axis + "maxfollowunits"].set_text(_("degrees"))
    
            elif d.units == _METRIC:
                w["labelmotor_pitch"].set_text(_("Leadscrew Pitch"))
                w["labelencoder_pitch"].set_text(_("Leadscrew Pitch"))
                w["motor_screwunits"].set_text(_("(mm / rev)"))
                w["encoder_screwunits"].set_text(_("(mm / rev)"))
                w[axis + "velunits"].set_text(_("mm / min"))
                w[axis + "accunits"].set_text(_("mm / sec²"))
                w[axis + "homevelunits"].set_text(_("mm / min"))
                w[axis + "homelatchvelunits"].set_text(_("mm / min"))
                w[axis + "homefinalvelunits"].set_text(_("mm / min"))
                w[axis + "accdistunits"].set_text(_("mm"))
                if stepdriven:
                    w[ "resolutionunits1"].set_text(_("mm / Step"))        
                    w["scaleunits"].set_text(_("Steps / mm"))
                else:
                    w["resolutionunits1"].set_text(_("mm / encoder pulse"))          
                    w["scaleunits"].set_text(_("Encoder pulses / mm"))
               
                w[axis + "minfollowunits"].set_text(_("mm"))
                w[axis + "maxfollowunits"].set_text(_("mm"))
               
            else:
                w["labelmotor_pitch"].set_text(_("Leadscrew TPI"))
                w["labelencoder_pitch"].set_text(_("Leadscrew TPI"))
                w["motor_screwunits"].set_text(_("(rev / inch)"))
                w["encoder_screwunits"].set_text(_("(rev / inch)"))
                w[axis + "velunits"].set_text(_("inches / min"))
                w[axis + "accunits"].set_text(_("inches / sec²"))
                w[axis + "homevelunits"].set_text(_("inches / min"))
                w[axis + "homelatchvelunits"].set_text(_("inches / min"))
                w[axis + "homefinalvelunits"].set_text(_("inches / min"))
                w["accdistunits"].set_text(_("inches"))
                if stepdriven:
                    w[ "resolutionunits1"].set_text(_("inches / Step"))        
                    w[ "scaleunits"].set_text(_("Steps / inch"))
                else:
                    w[ "resolutionunits1"].set_text(_("inches / encoder pulse"))        
                    w["scaleunits"].set_text(_("Encoder pulses / inch"))
               
                w[axis + "minfollowunits"].set_text(_("inches"))
                w[axis + "maxfollowunits"].set_text(_("inches"))
            thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis,"max-home-" + axis, "both-home-" + axis))
            homes = False
            for i in thisaxishome:
                test = self.data.findsignal(i)
                if test: homes = True
            w[axis + "homesw"].set_sensitive(homes)
            w[axis + "homesearchvel"].set_sensitive(homes)
            w[axis + "searchdir"].set_sensitive(homes)
            w[axis + "latchdir"].set_sensitive(homes)
            w[axis + "usehomeindex"].set_sensitive(homes)
            w[axis + "homefinalvel"].set_sensitive(homes)
            w[axis + "homelatchvel"].set_sensitive(homes)
            i = d[axis + "usecomp"]
            w[axis + "comptype"].set_sensitive(i)
            w[axis + "compfilename"].set_sensitive(i)
            i = d[axis + "usebacklash"]
            w[axis + "backlash"].set_sensitive(i)
            self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
            self.motor_encoder_sanity_check(None,axis)

    def on_xusecomp_toggled(self, *args): self.comp_toggle('x')
    def on_yusecomp_toggled(self, *args): self.comp_toggle('y')
    def on_zusecomp_toggled(self, *args): self.comp_toggle('z')
    def on_ausecomp_toggled(self, *args): self.comp_toggle('a')
    def on_xusebacklash_toggled(self, *args): self.backlash_toggle('x')
    def on_yusebacklash_toggled(self, *args): self.backlash_toggle('y')
    def on_zusebacklash_toggled(self, *args): self.backlash_toggle('z')
    def on_ausebacklash_toggled(self, *args): self.backlash_toggle('a')

    def on_xdrivertype_changed(self, *args): self.driver_changed('x')
    def on_ydrivertype_changed(self, *args): self.driver_changed('y')
    def on_zdrivertype_changed(self, *args): self.driver_changed('z')
    def on_adrivertype_changed(self, *args): self.driver_changed('a')
    def on_sdrivertype_changed(self, *args): self.driver_changed('s')

    def driver_changed(self, axis):
        d = self.data
        w = self.widgets
        v = w[axis + "drivertype"].get_active()
        if v < len(drivertypes):
            d = drivertypes[v]
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
        if what is None: what = self.data[axis + "drivertype"]
        for i, d in enumerate(drivertypes):
            if d[0] == what: return i
        return len(drivertypes)

    def drivertype_toid(self, axis, what=None):
        if not isinstance(what, int): what = self.drivertype_toindex(axis, what)
        if what < len(drivertypes): return drivertypes[what][0]
        return "custom"

    def drivertype_fromindex(self, axis):
        i = self.widgets[axis + "drivertype"].get_active()
        if i < len(drivertypes): return drivertypes[i][1]
        return _("Custom")

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
        def get_text(n): d[axis + n] = get_value(w[axis + n])
        def get_pagevalue(n): d[axis + n] = get_value(w[axis + n])
        def get_active(n): d[axis + n] = w[axis + n].get_active()
        stepdrive = self.data.findsignal(axis+"-stepgen-step")
        encoder = self.data.findsignal(axis+"-encoder-a")
        get_pagevalue("P")
        get_pagevalue("I")
        get_pagevalue("D")
        get_pagevalue("FF0")
        get_pagevalue("FF1")
        get_pagevalue("FF2")
        get_pagevalue("bias")
        get_pagevalue("deadband")
        get_pagevalue("steptime")
        get_pagevalue("stepspace")
        get_pagevalue("dirhold")
        get_pagevalue("dirsetup")
        get_pagevalue("outputscale")
        get_pagevalue("outputoffset")
        get_pagevalue("maxoutput")
        d[axis + "encodercounts"] = int(float(w["encoderline"].get_text())*4)
        if stepdrive: get_pagevalue("stepscale")
        if encoder: get_pagevalue("encoderscale")
        get_active("invertmotor")
        get_active("invertencoder")
        d[axis + "steprev"] = int(get_value(w["steprev"]))
        d[axis + "microstep"] = int(get_value(w["microstep"]))
        d[axis + "motor_pulleydriver"] = int(get_value(w["motor_pulleydriver"]))
        d[axis + "motor_pulleydriven"] = int(get_value(w["motor_pulleydriven"]))
        d[axis + "encoder_pulleydriver"] = int(get_value(w["encoder_pulleydriver"]))
        d[axis + "encoder_pulleydriven"] = int(get_value(w["encoder_pulleydriven"]))
        d[axis + "motor_leadscrew"] = int(get_value(w["motor_leadscrew"]))
        d[axis + "encoder_leadscrew"] = int(get_value(w["encoder_leadscrew"]))
        d[axis + "maxvel"] = (get_value(w[axis + "maxvel"])/60)
        get_text("maxacc")
        d[axis + "drivertype"] = self.drivertype_toid(axis, w[axis + "drivertype"].get_active())
        if not axis =="s":
            get_pagevalue("maxferror")
            get_pagevalue("minferror")
            get_text("homepos")
            get_text("minlim")
            get_text("maxlim")
            get_text("homesw")
            d[axis + "homesearchvel"] = (get_value(w[axis + "homesearchvel"])/60)
            d[axis + "homelatchvel"] = (get_value(w[axis + "homelatchvel"])/60)
            d[axis + "homefinalvel"] = (get_value(w[axis + "homefinalvel"])/60)
            get_active("searchdir")
            get_active("latchdir")
            get_active("usehomeindex")
            d[axis + "compfilename"] = w[axis + "compfilename"].get_text()
            get_active("comptype")
            d[axis + "backlash"]= w[axis + "backlash"].get_value()
            get_active("usecomp")
            get_active("usebacklash")
        else:
            get_active("pidcontrol") 

    def calculate_scale(self,axis):
        def get(n): return get_value(self.widgets[n])
        stepdrive = self.data.findsignal(axis+"-stepgen-step")
        encoder = self.data.findsignal(axis+"-encoder-a")
        # temparally add signals
        templist1 = ["encoderline","encoder_leadscrew","encoder_wormdriven","encoder_wormdriver","encoder_pulleydriven","encoder_pulleydriver",
                "steprev","motor_leadscrew","microstep","motor_wormdriven","motor_wormdriver","motor_pulleydriven","motor_pulleydriver"]
        for i in templist1:
            self.intrnldata[i] = self.widgets[i].connect("value-changed", self.update_scale_calculation,axis)
        templist2 = [ "cbencoder_pitch","cbencoder_worm","cbencoder_pulley","cbmotor_pitch","cbmicrosteps","cbmotor_worm","cbmotor_pulley"]
        for i in templist2:
            self.intrnldata[i] = self.widgets[i].connect("toggled", self.update_scale_calculation,axis)

        self.update_scale_calculation(self.widgets,axis)
        self.widgets.scaledialog.set_title(_("Axis Scale Calculation"))
        self.widgets.scaledialog.show_all()
        result = self.widgets.scaledialog.run()
        self.widgets.scaledialog.hide()
        # remove signals
        for i in templist1:
            self.widgets[i].disconnect(self.intrnldata[i])
        for i in templist2:
            self.widgets[i].disconnect(self.intrnldata[i])
        if not result: return
        if encoder:
            self.widgets[axis+"encoderscale"].set_value(get("calcencoder_scale"))
        if stepdrive:
            self.widgets[axis+"stepscale"].set_value(get("calcmotor_scale"))

    def update_scale_calculation(self,widgets,axis):
        w = self.widgets
        d = self.data
        def get(n): return get_value(w[n])
        stepdrive = self.data.findsignal(axis+"-stepgen-step")
        encoder = self.data.findsignal(axis+"-encoder-a")
        motor_pulley_ratio = encoder_pulley_ratio = 1
        motor_worm_ratio = encoder_worm_ratio = 1
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
                    if self.data.units == _METRIC: 
                        motor_pitch = 1./ get("motor_leadscrew")
                    else:  
                        motor_pitch = get("motor_leadscrew")
                else: w["motor_leadscrew"].set_sensitive(False)
                motor_steps = get("steprev")
                motor_scale = (motor_steps * microstepfactor * motor_pulley_ratio * motor_worm_ratio * motor_pitch) / rotary_scale
                w["calcmotor_scale"].set_text("%.4f" % motor_scale)
            else:
                w["calcmotor_scale"].set_sensitive(False)
                w["stepscaleframe"].set_sensitive(False)
            # encoder scale
            if encoder:
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
                    if self.data.units == _METRIC: 
                        encoder_pitch = 1./ get("encoder_leadscrew")
                    else:  
                        encoder_pitch = get("encoder_leadscrew")
                else: w["encoder_leadscrew"].set_sensitive(False)
                encoder_cpr = get_value(w[("encoderline")]) * 4
                encoder_scale = (encoder_pulley_ratio * encoder_worm_ratio * encoder_pitch * encoder_cpr) / rotary_scale
                w["calcencoder_scale"].set_text("%.4f" % encoder_scale)
            else:
                w["calcencoder_scale"].set_sensitive(False)
                w["encoderscaleframe"].set_sensitive(False)
        except (ValueError, ZeroDivisionError):
            w["calcmotor_scale"].set_text("1")
            w["calcencoder_scale"].set_text("1")
        self.update_pps(axis)

    def update_pps(self, axis):
        w = self.widgets
        d = self.data
        worm_ratio = motor_ratio = 1       
        def get(n): return get_value(w[n])
        stepdrive = self.data.findsignal(axis+"-stepgen-step")
        encoder = self.data.findsignal(axis+"-encoder-a")
        try:
            if encoder:
                scale = get("calcencoder_scale")
                encoder_cpr = get_value(w[("encoderline")]) * 4
            elif stepdrive:
                scale = get("calcmotor_scale")
            else: return
            if stepdrive:
                motor_steps = get("steprev")
                if w["cbmicrosteps"].get_active():
                    microstepfactor = get("microstep")
                else: microstepfactor = 1
            maxvps = (get_value(w[axis+"maxvel"]))/60
            pps = (scale * (maxvps))/1000
            if pps == 0: raise ValueError
            pps = abs(pps)
            w["khz"].set_text("%.1f" % pps)
            acctime = (maxvps) / get_value(w[axis+"maxacc"])
            accdist = acctime * .5 * (maxvps)
            if encoder:
                maxrpm = int(maxvps * 60 * (scale/encoder_cpr))
            else:
                maxrpm = int(maxvps * 60 * (scale/(microstepfactor * motor_steps)))
            w["acctime"].set_text("%.4f" % acctime)
            if not axis == 's':
                w["accdist"].set_text("%.4f" % accdist)                 
            w["chartresolution"].set_text("%.7f" % (1.0 / scale))
            w["calscale"].set_text(str(scale))
            w["maxrpm"].set_text("%d" % maxrpm)
            #self.widgets.druid1.set_buttons_sensitive(1,1,1,1)
            #w[axis + "axistune"].set_sensitive(1)
        except (ValueError, ZeroDivisionError): # Some entries not numbers or not valid
            w["chartresolution"].set_text("")
            w["acctime"].set_text("")
            if not axis == 's':
                w["accdist"].set_text("")
            w["khz"].set_text("")
            w["calscale"].set_text("")
            #self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
            #w[axis + "axistune"].set_sensitive(0)

    def motor_encoder_sanity_check(self,widgets,axis):
        stepdrive = encoder = bad = False
        if self.data.findsignal(axis+"-stepgen-step"): stepdrive = True
        if self.data.findsignal(axis+"-encoder-a"): encoder = True
        if encoder:
            if self.widgets[axis+"encoderscale"].get_value() < 1: bad = True
        if stepdrive:
            if self.widgets[axis+"stepscale"].get_value() < 1: bad = True
        if not encoder and not stepdrive : bad = True
        if self.widgets[axis+"maxvel"] < 1: bad = True
        if self.widgets[axis+"maxacc"] < 1: bad = True
        if bad:
            self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
            self.widgets[axis + "axistune"].set_sensitive(0)
            self.widgets[axis + "axistest"].set_sensitive(0)
        else:
            self.widgets.druid1.set_buttons_sensitive(1,1,1,1)
            self.widgets[axis + "axistune"].set_sensitive(1)
            self.widgets[axis + "axistest"].set_sensitive(1)

    def on_spindle_info_changed(self, *args): self.update_pps('s')
    def on_xaxis_info_changed(self, *args): self.update_pps('x')
    def on_yaxis_info_changed(self, *args): self.update_pps('y')
    def on_zaxis_info_changed(self, *args): self.update_pps('z')
    def on_aaxis_info_changed(self, *args): self.update_pps('a')
        
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
        self.axis_done('z')
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
    def on_saxistest_clicked(self, *args): self.test_axis('s')

    def on_xaxistune_clicked(self, *args): self.tune_axis('x')
    def on_yaxistune_clicked(self, *args): self.tune_axis('y')
    def on_zaxistune_clicked(self, *args): self.tune_axis('z')
    def on_aaxistune_clicked(self, *args): self.tune_axis('a')
    def on_saxistune_clicked(self, *args): self.tune_axis('s')

    def on_spindle_prepare(self, *args):
        self.data.help = "help-spindle.txt"
        self.axis_prepare('s')      
    def on_spindle_next(self, *args):
        self.axis_done('s')      
    def on_spindle_back(self, *args):
        self.on_spindle_next()
        if self.data.axes != 1:
            self.widgets.druid1.set_page(self.widgets.zaxis)
        else:
            self.widgets.druid1.set_page(self.widgets.aaxis)
        return True

    def has_spindle_speed_control(self):
        for test in ("s-stepgen-step", "s-pwm-pulse", "s-encoder-a", "spindle-enable", "spindle-cw", "spindle-ccw", "spindle-brake"):
            has_spindle = self.data.findsignal(test)
            if has_spindle:
                return True
        return False

    def on_spidcontrol_toggled(self, *args):
        test = self.data.findsignal("s-pwm-pulse")
        pwmdriven = 0
        if test: pwmdriven = 1
        if self.widgets.spidcontrol.get_active() == False: pwmdriven = 0
        self.widgets.sP.set_sensitive(pwmdriven)
        self.widgets.sI.set_sensitive(pwmdriven)
        self.widgets.sD.set_sensitive(pwmdriven)
        self.widgets.sFF0.set_sensitive(pwmdriven)
        self.widgets.sFF1.set_sensitive(pwmdriven)
        self.widgets.sFF2.set_sensitive(pwmdriven)
        self.widgets.sbias.set_sensitive(pwmdriven)
        self.widgets.sdeadband.set_sensitive(pwmdriven)
       
    def on_advanced_prepare(self, *args):       
        self.data.help = "help-advanced.txt"
        self.widgets.classicladder.set_active(self.data.classicladder)
        self.widgets.modbus.set_active(self.data.modbus)
        self.widgets.digitsin.set_value(self.data.digitsin)
        self.widgets.digitsout.set_value(self.data.digitsout)
        self.widgets.s32in.set_value(self.data.s32in)
        self.widgets.s32out.set_value(self.data.s32out)
        self.widgets.floatsin.set_value(self.data.floatsin)
        self.widgets.floatsout.set_value(self.data.floatsout)
        self.widgets.halui.set_active(self.data.halui)
        self.on_halui_toggled()
        for i in range(1,16):
            self.widgets["halui_cmd"+str(i)].set_text(self.data["halui_cmd"+str(i)])  
        self.widgets.ladderconnect.set_active(self.data.ladderconnect)      
        self.on_classicladder_toggled()
        if  not self.widgets.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custom.clp" % self.data.machinename)):
                self.widgets.ladderexist.set_active(True)

    def on_advanced_next(self, *args):
        self.data.classicladder = self.widgets.classicladder.get_active()
        self.data.modbus = self.widgets.modbus.get_active()
        self.data.digitsin = self.widgets.digitsin.get_value()
        self.data.digitsout = self.widgets.digitsout.get_value()
        self.data.s32in = self.widgets.s32in.get_value()
        self.data.s32out = self.widgets.s32out.get_value()
        self.data.floatsin = self.widgets.floatsin.get_value()
        self.data.floatsout = self.widgets.floatsout.get_value()
        self.data.halui = self.widgets.halui.get_active() 
        for i in range(1,16):
            self.data["halui_cmd"+str(i)] = self.widgets["halui_cmd"+str(i)].get_text()         
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
              if not has_estop:
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
                  if not self.warning_dialog(_("OK to replace existing custom ladder program?\nExisting Custom.clp will be\
                     renamed custom_backup.clp.\nAny existing file named -custom_backup.clp- will be lost. "),False):
                     self.widgets.druid1.set_page(self.widgets.advanced)
                     return True 
           if self.widgets.ladderexist.get_active() == False:
              if os.path.exists(os.path.join(distdir, "configurable_options/ladder/TEMP.clp")):
                 if not self.warning_dialog(_("You edited a ladder program and have selected a different program to copy\
                     to your configuration file.\nThe edited program will be lost.\n\nAre you sure?  "),False):
                   self.widgets.druid1.set_page(self.widgets.advanced)
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
 
    def on_halui_toggled(self, *args):
        i= self.widgets.halui.get_active()
        self.widgets.haluitable.set_sensitive(i)

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
        self.widgets.pyvcpgeometry.set_sensitive(i)
        if  self.widgets.createconfig.get_active():
            self.widgets.pyvcpexist.set_sensitive(False)
        else:
            self.widgets.pyvcpexist.set_sensitive(i)
        self.widgets.displaypanel.set_sensitive(i)
        self.widgets.pyvcpconnect.set_sensitive(i)

    def on_displaypanel_clicked(self,*args):
        self.testpanel(self)

    def on_realtime_components_prepare(self,*args):
        self.data.help = "help-realtime.txt"
        self.widgets.userneededpid.set_value(self.data.userneededpid)
        self.widgets.userneededabs.set_value(self.data.userneededabs)
        self.widgets.userneededscale.set_value(self.data.userneededscale)
        self.widgets.userneededmux16.set_value(self.data.userneededmux16)

        if not self.intrnldata.components_is_prepared:
            textbuffer = self.widgets.loadcompservo.get_buffer()
            for i in self.data.loadcompservo:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            textbuffer = self.widgets.addcompservo.get_buffer()
            for i in self.data.addcompservo:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            textbuffer = self.widgets.loadcompbase.get_buffer()
            for i in self.data.loadcompbase:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            textbuffer = self.widgets.addcompbase.get_buffer()
            for i in self.data.addcompbase:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            self.intrnldata.components_is_prepared = True

    def on_realtime_components_next(self,*args):
        self.data.userneededpid = int(self.widgets.userneededpid.get_value())
        self.data.userneededabs = int(self.widgets.userneededabs.get_value())
        self.data.userneededscale = int(self.widgets.userneededscale.get_value())
        self.data.userneededmux16 = int(self.widgets.userneededmux16.get_value())

        textbuffer = self.widgets.loadcompservo.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.data.loadcompservo = i
        textbuffer = self.widgets.addcompservo.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.data.addcompservo = i
        textbuffer = self.widgets.loadcompbase.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.data.loadcompbase = i
        textbuffer = self.widgets.addcompbase.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.data.addcompbase = i

    def on_complete_back(self, *args):
        self.widgets.druid1.set_page(self.widgets.advanced)
        return True
   
    def on_complete_finish(self, *args):
        # if parallel ports not used clear all signals
        parportnames = ("pp1","pp2","pp3")
        for check,connector in enumerate(parportnames):
            if self.data.number_pports >= (check+1):continue
            # initialize parport input / inv pins
            for i in (1,2,3,4,5,6,7,8,10,11,12,13,15):
                pinname ="%sIpin%d"% (connector,i)
                self.data[pinname] = UNUSED_INPUT
                pinname ="%sIpin%dinv"% (connector,i)
                self.data[pinname] = False
            # initialize parport output / inv pins
            for i in (1,2,3,4,5,6,7,8,9,14,16,17):
                pinname ="%sOpin%d"% (connector,i)
                self.data[pinname] = UNUSED_OUTPUT
                pinname ="%sOpin%dinv"% (connector,i)
                self.data[pinname] = False        
        # clear all unused mesa signals
        for boardnum in(0,1):
            for connector in(2,3,4,5,6,7,8,9):
                if self.data.number_mesa >= boardnum + 1 :
                    if connector in(self.data["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                        continue
                # This initializes GPIO input pins
                for i in range(0,16):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self.data[pinname] = UNUSED_INPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self.data[pinname] = GPIOI
                # This initializes GPIO output pins
                for i in range(16,24):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self.data[pinname] = UNUSED_OUTPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self.data[pinname] = GPIOO
                # This initializes the mesa inverse pins
                for i in range(0,24):
                    pinname ="mesa%dc%dpin%dinv"% (boardnum,connector,i)
                    self.data[pinname] = False

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
        self.widgets.baseperiod.set_text("%d ns" % minperiod)
        self.widgets.maxsteprate.set_text("%d Hz" % maxhz)

    def on_latency_test_clicked(self, w):
        self.latency_pid = os.spawnvp(os.P_NOWAIT,
                                "latency-test", ["latency-test"])
        self.widgets['window1'].set_sensitive(0)
        gobject.timeout_add(1, self.latency_running_callback)

    def latency_running_callback(self):
        pid, status = os.waitpid(self.latency_pid, os.WNOHANG)
        if pid:
            self.widgets['window1'].set_sensitive(1)
            return False
        return True

    def m5i20test(self,w): 
        for i in range(0,int(self.data.number_mesa)): 
            if self.data["mesa%d_currentfirmwaredata"% (i)][_BOARDNAME] in( "5i22", "7i43"):
                self.warning_dialog( _(" The test panel for this board and/or firmware should work fine for GPIO but\
                     maybe not so fine for other components.\n work in progress. \n You must have the board installed for it to work.") , True)  
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )   
        halrun.write("loadrt threads period1=200000 name1=fast fp1=0 period2=1000000 name2=slow\n")
        self.hal_cmnds("LOAD")
        halrun.write("loadrt or2 count=72\n")
        self.hal_cmnds("READ")
        for i in range(0,72):
            halrun.write("addf or2.%d slow\n"% i)
        self.hal_cmnds("WRITE")
        halrun.write("start\n")
        halrun.write("loadusr -Wn mesa0test pyvcp -g +700+0 -c mesa0test %(panel)s\n" %{'panel':"m5i20panel.xml",})
        halrun.write("loadusr -Wn mesa1test pyvcp -g +700+200 -c mesa1test %(panel)s\n" %{'panel':"m5i20panel.xml",})
        halrun.write("loadusr halmeter -g 0 500\n")
        halrun.write("loadusr halmeter -g 0 620\n")
        for boardnum in range(0,int(self.data.number_mesa)):
            board = self.data["mesa%d_currentfirmwaredata"% (boardnum)][0]+".%d"% boardnum
            for concount,connector in enumerate(self.data["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                for pin in range (0,24):
                    firmptype,compnum = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pin+(concount*24)]
                    pinv = 'mesa%dc%dpin%dinv' % (boardnum,connector,pin)
                    ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                    pintype = self.widgets[ptype].get_active_text()
                    pininv = self.widgets[pinv].get_active()
                    truepinnum = (concount*24) + pin
                    # for output / open drain pins
                    if  pintype in (GPIOO,GPIOD):                
                        halrun.write("setp mesa0test.led.%d.disable true\n"% truepinnum )
                        halrun.write("setp mesa0test.button.%d.disable false\n"% truepinnum )
                        halrun.write("setp hm2_%s.gpio.%03d.is_output true\n"% (board,truepinnum ))
                        if pininv:  halrun.write("setp hm2_%s.gpio.%03d.invert_output true\n"% (board,truepinnum ))
                        halrun.write("net signal_out%d or2.%d.out hm2_%s.gpio.%03d.out\n"% (truepinnum,truepinnum,board,truepinnum))
                        halrun.write("net pushbutton.%d or2.%d.in1 mesa0test.button.%d\n"% (truepinnum,truepinnum,truepinnum))
                        halrun.write("net latchbutton.%d or2.%d.in0 mesa0test.checkbutton.%d\n"% (truepinnum,truepinnum,truepinnum))
                    # for input pins
                    elif pintype == GPIOI:                                    
                        halrun.write("setp mesa0test.button.%d.disable true\n"% truepinnum )
                        halrun.write("setp mesa0test.led.%d.disable false\n"% truepinnum )
                        if pininv: halrun.write("net blue_in%d hm2_%s.gpio.%03d.in_not mesa0test.led.%d\n"%(truepinnum,board,truepinnum,truepinnum))
                        else:   halrun.write("net blue_in%d hm2_%s.gpio.%03d.in mesa0test.led.%d\n"% (truepinnum,board,truepinnum,truepinnum))
                    # for encoder pins
                    elif pintype in (ENCA,ENCB,ENCI,ENCM):
                        halrun.write("setp mesa0test.led.%d.disable true\n"% truepinnum )
                        halrun.write("setp mesa0test.button.%d.disable true\n"% truepinnum )                   
                        if not pintype == ENCA: continue                 
                        if pin == 3 :encpinnum = (connector-2)*4 
                        elif pin == 1 :encpinnum = 1+((connector-2)*4) 
                        elif pin == 15 :encpinnum = 2+((connector-2)*4) 
                        elif pin == 13 :encpinnum = 3+((connector-2)*4) 
                        halrun.write("setp mesa0test.enc.%d.reset.disable false\n"% encpinnum )
                        halrun.write("net yellow_reset%d hm2_%s.encoder.%02d.reset mesa0test.enc.%d.reset\
                        \n"% (encpinnum,board,encpinnum,encpinnum))
                        halrun.write("net yellow_count%d hm2_%s.encoder.%02d.count mesa0test.number.%d\n"% (encpinnum,board,encpinnum,encpinnum))
                    # for PWM pins
                    elif pintype in (PWMP,PWMD,PWME,PDMP,PDMD,PDME):
                        halrun.write("setp mesa0test.led.%d.disable true\n"% truepinnum )
                        halrun.write("setp mesa0test.button.%d.disable true\n"% truepinnum )
                        if not pintype in (PWMP,PDMP): continue    
                        if pin == 7 :encpinnum = (connector-2)*4 
                        elif pin == 6 :encpinnum = 1 + ((connector-2)*4) 
                        elif pin == 19 :encpinnum = 2 + ((connector-2)*4) 
                        elif pin == 18 :encpinnum = 3 + ((connector-2)*4)        
                        halrun.write("net green_enable%d hm2_%s.pwmgen.%02d.enable mesa0test.dac.%d.enbl\n"% (encpinnum,board,encpinnum,encpinnum)) 
                        halrun.write("net green_value%d hm2_%s.pwmgen.%02d.value mesa0test.dac.%d-f\n"% (encpinnum,board,encpinnum,encpinnum)) 
                        halrun.write("setp hm2_%s.pwmgen.%02d.scale 10\n"% (board,encpinnum)) 
                    # for Stepgen pins
                    elif pintype in (STEPA,STEPB):
                        halrun.write("setp mesa0test.led.%d.disable true\n"% truepinnum )
                        halrun.write("setp mesa0test.button.%d.disable true\n"% truepinnum ) 
                        if not pintype == STEPA : continue 
                        
                        halrun.write("net brown_enable%d hm2_%s.stepgen.%02d.enable mesa0test.step.%d.enbl\n"% (compnum,board,compnum,compnum))
                        halrun.write("net brown_value%d hm2_%s.stepgen.%02d.position-cmd mesa0test.anaout.%d\n"% (compnum,board,compnum,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.maxaccel 0 \n"% (board,compnum))
                        halrun.write("setp hm2_%s.stepgen.%02d.maxvel 0 \n"% (board,compnum))
                    else: 
                        print "pintype error IN mesa test panel method pintype %s boardnum %d connector %d pin %d"% (pintype,boardnum,connector,pin)
            if not board == "5i22":
                for pin in range (0,24):
                    truepinnum = (72) + pin
                    halrun.write("setp mesa0test.led.%d.disable true\n"% truepinnum )
                    halrun.write("setp mesa0test.button.%d.disable true\n"% truepinnum )
                for pin in range (8,12):
                    halrun.write("setp mesa0test.enc.%d.reset.disable true\n"% pin )
        halrun.write("waitusr mesa0test\n"); halrun.flush()
        halrun.close()
        self.widgets['window1'].set_sensitive(1)

    def on_address_search_clicked(self,w):
        self.on_druid1_help()
        match =  os.popen('lspci -v').read()
        textbuffer = self.widgets.textoutput.get_buffer()
        try :         
            textbuffer.set_text(match)
            self.widgets.helpnotebook.set_current_page(2)
            self.widgets.helpwindow.show_all()
        except:
            text = _("PCI search page is unavailable\n")
            self.warning_dialog(text,True)

    def parporttest(self,w):
        if not self.check_for_rt(self):
            return
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )  
        halrun.write("loadrt threads period1=100000 name1=fast fp1=0 period2=%d name2=slow\n"% self.data.servoperiod)
        self.hal_cmnds("LOAD")
        for i in range(0,self.data.number_pports ):
            halrun.write("loadusr -Wn parport%(number)dtest pyvcp -g +%(pos)d+0 -c parport%(number)dtest %(panel)s\n" 
                    % {'pos':(i*300),'number':i,'panel':"parportpanel.xml\n",})
        halrun.write("loadrt or2 count=%d\n"%(self.data.number_pports * 12))
        self.hal_cmnds("READ")
        for i in range(0,(self.data.number_pports * 12)):
           halrun.write("addf or2.%d fast\n"% i)
        halrun.write("loadusr halmeter pin parport.0.pin-01-out -g 0 500\n")
        self.hal_cmnds("WRITE")
        
        templist = ("pp1","pp2","pp3")
        for j in range(self.data.number_pports):         
            if self.data[templist[j]+"_direction"] == 1:
                inputpins = (10,11,12,13,15)
                outputpins = (1,2,3,4,5,6,7,8,9,14,16,17)               
                for x in (2,3,4,5,6,7,8,9):
                    halrun.write( "setp parport%dtest.led.%d.disable true\n"%(j, x))
                    halrun.write( "setp parport%dtest.led_text.%d.disable true\n"%(j, x))
            else:
                inputpins = (2,3,4,5,6,7,8,9,10,11,12,13,15)
                outputpins = (1,14,16,17)
                for x in (2,3,4,5,6,7,8,9):
                    halrun.write( "setp parport%dtest.button.%d.disable true\n"% (j , x))
                    halrun.write( "setp parport%dtest.button_text.%d.disable true\n"% (j , x))

            for x in inputpins: 
                i = self.widgets["%sIpin%dinv" % (templist[j], x)].get_active()
                if i:  halrun.write( "net red_in_not.%d parport%dtest.led.%d <= parport.%d.pin-%02d-in-not\n" % (x, j, x, j, x))
                else:  halrun.write( "net red_in.%d parport%dtest.led.%d <= parport.%d.pin-%02d-in\n" % (x, j, x, j ,x))
            for num, x in enumerate(outputpins):  
                i = self.widgets["%sOpin%dinv" % (templist[j], x)].get_active()
                if i:  halrun.write( "setp parport.%d.pin-%02d-out-invert true\n" %(j, x))
                halrun.write("net signal_out%d or2.%d.out parport.%d.pin-%02d-out\n"% (x, num, j, x))
                halrun.write("net pushbutton.%d or2.%d.in1 parport%dtest.button.%d\n"% (x, num, j, x))
                halrun.write("net latchbutton.%d or2.%d.in0 parport%dtest.checkbutton.%d\n"% (x, num, j, x))           
        halrun.write("start\n")
        halrun.write("waitusr parport0test\n"); halrun.flush()
        halrun.close()
        self.widgets['window1'].set_sensitive(1)

    # This is for pyvcp test panel
    def testpanel(self,w):
        if not self.check_for_rt(True):
            return 
        pos = "+0+0"
        size = ""
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
        if self.widgets.pyvcpposcheckbutton.get_active() == True:
            xpos = self.widgets.pyvcpxpos.get_value()
            ypos = self.widgets.pyvcpypos.get_value()
            pos = "+%d+%d"% (xpos,ypos)
        if self.widgets.pyvcpsizecheckbutton.get_active() == True:
            width = self.widgets.pyvcpwidth.get_value()
            height = self.widgets.pyvcpheight.get_value()
            size = "%dx%d"% (width,height)    
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )    
        halrun.write("loadusr -Wn displaytest pyvcp -g %(size)s%(pos)s -c displaytest %(panel)s\n" %{'size':size,'pos':pos,'panel':panel,})
        if self.widgets.pyvcp1.get_active() == True:
                halrun.write("setp displaytest.spindle-speed 1000\n")
                #halrun.write("setp displaytest.toolnumber 4\n")
        halrun.write("waitusr displaytest\n"); halrun.flush()
        halrun.close()

    # for classicladder test  
    def load_ladder(self,w): 
        if not self.check_for_rt(True):
            return  
        newfilename = os.path.join(distdir, "configurable_options/ladder/TEMP.clp")    
        self.data.modbus = self.widgets.modbus.get_active()
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")
        halrun.write(""" 
              loadrt threads period1=%(period)d name1=fast fp1=0 period2=%(period2)d name2=slow 
              loadrt classicladder_rt numPhysInputs=%(din)d numPhysOutputs=%(dout)d numS32in=%(sin)d\
               numS32out=%(sout)d numFloatIn=%(fin)d numFloatOut=%(fout)d
               addf classicladder.0.refresh slow
               start\n""" % {
                      'din': self.widgets.digitsin.get_value(),
                      'dout': self.widgets.digitsout.get_value(),
                      'sin': self.widgets.s32in.get_value(),
                      'sout': self.widgets.s32out.get_value(), 
                      'fin':self.widgets.floatsin.get_value(),
                      'fout':self.widgets.floatsout.get_value(),
                      'period':100000, 
                      'period2':self.data.servoperiod
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
            halrun.write("loadusr -w classicladder --modmaster --newpath=%(newname)s %(filename)s\n" %                                  {'newname':newfilename,'filename':filename})
        else:
            halrun.write("loadusr -w classicladder --newpath=%(newname)s %(filename)s\n" % { 'newname':newfilename ,'filename':filename })
        halrun.write("start\n"); halrun.flush()
        halrun.close()
        if os.path.exists(newfilename):
            self.data.tempexists = True
            self.widgets.newladder.set_text('Edited ladder program')
            self.widgets.ladderexist.set_active(True)
        else:
            self.data.tempexists = 0
      
    # servo and stepper test  
    def tune_axis(self, axis):
        if not self.check_for_rt(self):
            return
        d = self.data
        w = self.widgets
        self.updaterunning = False
        axnum = "xyzas".index(axis)
        self.axis_under_tune = axis
        self.stepgen = self.data.stepgen_sig(axis)
        print axis," stepgen--",self.stepgen
        self.encoder = self.data.encoder_sig(axis)
        print axis," encoder--",self.encoder
        self.pwmgen  = self.data.pwmgen_sig(axis)
        print axis," pwgen--",self.pwmgen
        w.tuneaxispage.set_current_page(axnum)
        w[axis+"tunepage"].set_sensitive(1)

        if self.stepgen:
            w[axis+"tuningnotebook"].set_current_page(1)
            w[axis+"pid"].set_sensitive(0)
            w[axis+"tuneinvertencoder"].set_sensitive(0)
            w[axis+"pidtable"].set_sensitive(0)
        else:
            w[axis+"tuningnotebook"].set_current_page(0)
            w[axis+"step"].set_sensitive(0)
            w[axis+"steptable"].set_sensitive(0)
            text = _("Servo tuning is not finished / working\n")
            self.warning_dialog(text,True)

        if axis == "a":
            w[axis + "tunedistunits"].set_text(_("degrees"))
            w[axis + "tunevelunits"].set_text(_("degrees / minute"))
            w[axis + "tuneaccunits"].set_text(_("degrees / second²"))
        elif axis == "s":
            w[axis + "tunedistunits"].set_text(_("revolutions"))
            w[axis + "tunevelunits"].set_text(_("rpm"))
            w[axis + "tuneaccunits"].set_text(_("revs / second²"))
        elif d.units == _METRIC:
            w[axis + "tunedistunits"].set_text(_("mm"))
            w[axis + "tunevelunits"].set_text(_("mm / minute"))
            w[axis + "tuneaccunits"].set_text(_("mm / second²"))
        else:
            w[axis + "tunedistunits"].set_text(_("inches"))
            w[axis + "tunevelunits"].set_text(_("inches / minute"))
            w[axis + "tuneaccunits"].set_text(_("inches / second²"))
        w[axis+"tunevel"].set_value(get_value(w[axis+"maxvel"]))
        w[axis+"tuneacc"].set_value(get_value(w[axis+"maxacc"]))
        w[axis+"tunecurrentP"].set_value(w[axis+"P"].get_value())
        w[axis+"tuneorigP"].set_text("%s" % w[axis+"P"].get_value())
        w[axis+"tunecurrentI"].set_value(w[axis+"I"].get_value())
        w[axis+"tuneorigI"].set_text("%s" % w[axis+"I"].get_value())
        w[axis+"tunecurrentD"].set_value(w[axis+"D"].get_value())
        w[axis+"tuneorigD"].set_text("%s" % w[axis+"D"].get_value())
        w[axis+"tunecurrentFF0"].set_value(w[axis+"FF0"].get_value())
        w[axis+"tuneorigFF0"].set_text("%s" % w[axis+"FF0"].get_value())
        w[axis+"tunecurrentFF1"].set_value(w[axis+"FF1"].get_value())
        w[axis+"tuneorigFF1"].set_text("%s" % w[axis+"FF1"].get_value())
        w[axis+"tunecurrentFF2"].set_value(w[axis+"FF2"].get_value())
        w[axis+"tuneorigFF2"].set_text("%s" % w[axis+"FF2"].get_value())
        w[axis+"tunecurrentbias"].set_value(w[axis+"bias"].get_value())
        w[axis+"tuneorigbias"].set_text("%s" % w[axis+"bias"].get_value())
        w[axis+"tunecurrentdeadband"].set_value(w[axis+"deadband"].get_value())
        w[axis+"tuneorigdeadband"].set_text("%s" % w[axis+"deadband"].get_value())
        w[axis+"tunecurrentsteptime"].set_value(w[axis+"steptime"].get_value())
        w[axis+"tuneorigsteptime"].set_text("%s" % w[axis+"steptime"].get_value())
        w[axis+"tunecurrentstepspace"].set_value(get_value(w[axis+"stepspace"]))
        w[axis+"tuneorigstepspace"].set_text("%s" % w[axis+"stepspace"].get_value())
        w[axis+"tunecurrentdirhold"].set_value(get_value(w[axis+"dirhold"]))
        w[axis+"tuneorigdirhold"].set_text("%s" % w[axis+"dirhold"].get_value())
        w[axis+"tunecurrentdirsetup"].set_value(get_value(w[axis+"dirsetup"]))
        w[axis+"tuneorigdirsetup"].set_text("%s" % w[axis+"dirsetup"].get_value())
        self.tunejogplus = self.tunejogminus = 0
        w[axis+"tunedir"].set_active(0)
        w[axis+"tunerun"].set_active(0)
        w[axis+"tuneinvertmotor"].set_active(w[axis+"invertmotor"].get_active())
        w[axis+"tuneinvertencoder"].set_active(w[axis+"invertencoder"].get_active())
        
             
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")
        halrun.write("""
        loadrt threads period1=%(period)d name1=fast fp1=0 period2=%(period2)d name2=slow
        loadusr halscope
        loadrt scale names=scale_to_rpm
        loadrt steptest     
        """ % {'period':100000, 'period2':self.data.servoperiod })   
        if not self.stepgen: 
            halrun.write("loadrt pid num_chan=1\n")
        self.hal_cmnds("LOAD")
        self.hal_cmnds("READ")       
        halrun.write("addf steptest.0 slow \n")
        if not self.stepgen: 
            halrun.write("addf pid.0.do-pid-calcs slow \n")
        halrun.write("addf scale_to_rpm slow \n")
        self.hal_cmnds("WRITE")
        # for encoder signals
        if self.encoder: 
            print self.encoder,"--",self.encoder[4:5],self.encoder[10:],self.encoder[6:7] 
            self.enc_signalname = self.data.make_pinname(self.encoder)                 
            halrun.write("setp %s.counter-mode 0\n"% (self.enc_signalname))
            halrun.write("setp %s.filter 1\n"% (self.enc_signalname))
            halrun.write("setp %s.index-invert 0\n"% (self.enc_signalname))
            halrun.write("setp %s.index-mask 0\n"% (self.enc_signalname))
            halrun.write("setp %s.index-mask-invert 0\n"% (self.enc_signalname)) 
            halrun.write("setp %s.scale %d\n"% (self.enc_signalname, get_value(w[axis + "encoderscale"])))                         
            halrun.write("loadusr halmeter -s pin %s.velocity -g 0 625 330\n"% (self.enc_signalname))
            halrun.write("loadusr halmeter -s pin %s.position -g 0 675 330\n"% (self.enc_signalname))
            halrun.write("loadusr halmeter pin %s.velocity -g 275 415\n"% (self.enc_signalname))
        # for pwm components
        if self.pwmgen:                             
            self.pwm_signalname = self.data.make_pinname(self.pwmgen)  
            print "got to pwm", self.pwmgen," -- ",self.pwm_signalname                        
            halrun.write("setp %s.scale 10\n"% (self.pwm_signalname))                        
            halrun.write("setp %s.output-type 1\n"% (self.pwm_signalname))                             
            halrun.write("loadusr halmeter pin %s.enable -g 0 415\n"% (self.pwm_signalname))
            halrun.write("loadusr halmeter -s pin %s.enable -g 0 525 330\n"% (self.pwm_signalname))
            halrun.write("loadusr halmeter -s pin %s.value -g 0 575 330\n"% (self.pwm_signalname)) 
        # for step gen components
        if self.stepgen:                        
            # check current component number to signal's component number                             
            self.step_signalname = self.data.make_pinname(self.stepgen) 
            print "step_signal--",self.step_signalname   
            if w[axis+"invertmotor"].get_active():
                self.scale = get_value(w[axis + "stepscale"]) * -1
            else:
                self.scale = get_value(w[axis + "stepscale"]) * 1
                #halrun.write("setp %s.gpio.%03d.invert_output %d \n"% (self.step_signalname,self.invert,guiinvert))
            halrun.write("setp %s.step_type 0 \n"% (self.step_signalname))
            halrun.write("setp %s.position-scale %f \n"% (self.step_signalname,self.scale))
            halrun.write("setp %s.steplen %d \n"% (self.step_signalname,w[axis+"steptime"].get_value()))
            halrun.write("setp %s.stepspace %d \n"% (self.step_signalname,w[axis+"stepspace"].get_value()))
            halrun.write("setp %s.dirhold %d \n"% (self.step_signalname,w[axis+"dirhold"].get_value()))
            halrun.write("setp %s.dirsetup %d \n"% (self.step_signalname,w[axis+"dirsetup"].get_value()))
            halrun.write("setp steptest.0.epsilon %f\n"% abs(1. / get_value(w[axis + "stepscale"]))  )
            halrun.write("setp %s.maxaccel 0 \n"% (self.step_signalname))
            halrun.write("setp %s.maxvel 0 \n"% (self.step_signalname))
            halrun.write("net enable => %s.enable \n"% (self.step_signalname))
            halrun.write("net cmd steptest.0.position-cmd => %s.position-cmd \n"% (self.step_signalname))
            halrun.write("net feedback steptest.0.position-fb <= %s.position-fb \n"% (self.step_signalname))
            halrun.write("net speed_rps scale_to_rpm.in <= %s.velocity-fb \n"% (self.step_signalname))
            halrun.write("net speed_rpm scale_to_rpm.out\n")
            halrun.write("setp scale_to_rpm.gain 60\n")
            halrun.write("loadusr halmeter sig speed_rpm -g 0 415\n")
            halrun.write("loadusr halmeter -s pin %s.velocity-fb -g 0 575 350\n"% (self.step_signalname))
            halrun.write("loadusr halmeter -s pin %s.position-fb -g 0 525 350\n"% (self.step_signalname))
        # set up enable output pin if used
        temp = self.data.findsignal( "enable")
        amp = self.data.make_pinname(temp)
        if amp:
            if "hm2" in amp:    
                halrun.write("setp %s true\n"% (amp + ".is_output"))             
                halrun.write("net enable %s \n"% (amp + ".out"))
                if self.data[temp+"inv"] == True:
                    halrun.write("setp %s true\n"%  (amp + ".invert_output"))
        # set up estop output if used
        temp = self.data.findsignal( "estop-out")
        estop = self.data.make_pinname(temp)
        if estop:        
            if "hm2" in estop:
                halrun.write("setp %s true\n"%  (estop + ".is_output"))    
                halrun.write("net enable %s\n"%  (estop + ".out"))
                if self.data[temp+"inv"] == True:
                    halrun.write("setp %s true\n"%  (estop + ".invert_output"))
        # set up as servo system if no step generator...
        if not self.encoder:
            halrun.write("setp pid.0.Pgain     %d\n"% ( w[axis+"P"].get_value() ))
            halrun.write("setp pid.0.Igain     %d\n"% ( w[axis+"I"].get_value() ))
            halrun.write("setp pid.0.Dgain     %d\n"% ( w[axis+"D"].get_value() ))
            halrun.write("setp pid.0.bias      %d\n"% ( w[axis+"bias"].get_value() ))
            halrun.write("setp pid.0.FF0       %d\n"% ( w[axis+"FF0"].get_value() ))
            halrun.write("setp pid.0.FF1       %d\n"% ( w[axis+"FF1"].get_value() ))
            halrun.write("setp pid.0.FF2       %d\n"% ( w[axis+"FF2"].get_value() ))
            halrun.write("setp pid.0.deadband  %d\n"% ( w[axis+"deadband"].get_value() ))
            halrun.write("setp pid.0.maxoutput  %d\n"% ( w[axis+"maxoutput"].get_value() ))
            halrun.write("net enable     => pid.0.enable\n")
            halrun.write("net output     pid.0.output      => %s.value\n"% (self.pwm_signalname))
            halrun.write("net pos-cmd    steptest.0.position-cmd => pid.0.command\n")
            halrun.write("net enable     =>  %s.enable\n"% (self.pwm_signalname))
            halrun.write("net feedback steptest.0.position-fb <= %s.position \n"% (self.enc_signalname))
   
        self.updaterunning = True
        halrun.write("start\n"); halrun.flush()
        w.tunedialog.set_title(_("%s Axis Tune") % axis.upper())
        w.tunedialog.move(550,0)
        w.tunedialog.show_all()
        self.widgets['window1'].set_sensitive(0)
        result = w.tunedialog.run()
        w.tunedialog.hide()
        if result == gtk.RESPONSE_OK:
            w[axis+"maxvel"].set_value( get_value(w[axis+"tunevel"]))
            w[axis+"maxacc"].set_value( get_value(w[axis+"tuneacc"]))
            w[axis+"P"].set_value( get_value(w[axis+"tunecurrentP"]))
            w[axis+"I"].set_value( get_value(w[axis+"tunecurrentI"]))
            w[axis+"D"].set_value( get_value(w[axis+"tunecurrentD"]))
            w[axis+"FF0"].set_value( get_value(w[axis+"tunecurrentFF0"]))
            w[axis+"FF1"].set_value( get_value(w[axis+"tunecurrentFF1"]))
            w[axis+"FF2"].set_value( get_value(w[axis+"tunecurrentFF2"]))
            w[axis+"bias"].set_value( get_value(w[axis+"tunecurrentbias"]))
            w[axis+"deadband"].set_value( get_value(w[axis+"tunecurrentdeadband"]))
            w[axis+"tunecurrentbias"].set_value(w[axis+"bias"].get_value())
            w[axis+"steptime"].set_value(get_value(w[axis+"tunecurrentsteptime"]))
            w[axis+"stepspace"].set_value(get_value(w[axis+"tunecurrentstepspace"]))
            w[axis+"dirhold"].set_value(get_value(w[axis+"tunecurrentdirhold"]))
            w[axis+"dirsetup"].set_value(get_value(w[axis+"tunecurrentdirsetup"]))
            w[axis+"invertmotor"].set_active(w[axis+"tuneinvertmotor"].get_active())
            w[axis+"invertencoder"].set_active(w[axis+"tuneinvertencoder"].get_active())      
        halrun.write("sets enable false\n")   
        time.sleep(.001)   
        halrun.close()  
        self.widgets['window1'].set_sensitive(1)

    def update_tune_axis_params(self, *args):       
        axis = self.axis_under_tune
        if axis is None or not self.updaterunning: return   
        temp = not self.widgets[axis+"tunerun"].get_active()
        self.widgets[axis+"tuneinvertmotor"].set_sensitive( temp)
        self.widgets[axis+"tuneamplitude"].set_sensitive( temp)
        self.widgets[axis+"tunedir"].set_sensitive( temp)
        self.widgets[axis+"tunejogminus"].set_sensitive(temp)
        self.widgets[axis+"tunejogplus"].set_sensitive(temp)
        temp = self.widgets[axis+"tuneenable"].get_active()
        if not self.widgets[axis+"tunerun"].get_active():
            self.widgets[axis+"tunejogminus"].set_sensitive(temp)
            self.widgets[axis+"tunejogplus"].set_sensitive(temp)
        self.widgets[axis+"tunerun"].set_sensitive(temp)
        halrun = self.halrun
        if self.stepgen:
            halrun.write("""
                setp %(stepgen)s.steplen %(len)d
                setp %(stepgen)s.stepspace %(space)d
                setp %(stepgen)s.dirhold %(hold)d
                setp %(stepgen)s.dirsetup %(setup)d
                setp %(stepgen)s.maxaccel %(accel)f
                setp %(stepgen)s.maxvel %(velps)f
                setp %(stepgen)s.position-scale %(scale)f  
                setp steptest.0.jog-minus %(jogminus)s
                setp steptest.0.jog-plus %(jogplus)s
                setp steptest.0.run %(run)s
                setp steptest.0.amplitude %(amplitude)f
                setp steptest.0.maxvel %(velps)f
                setp steptest.0.maxaccel %(accel)f
                setp steptest.0.dir %(dir)s
                setp steptest.0.pause %(pause)d
                sets enable %(enable)s
            """ % {
                'scale':self.scale,
                'len':self.widgets[axis+"tunecurrentsteptime"].get_value(),
                'space':self.widgets[axis+"tunecurrentstepspace"].get_value(),
                'hold':self.widgets[axis+"tunecurrentdirhold"].get_value(),
                'setup':self.widgets[axis+"tunecurrentdirsetup"].get_value(),
                'stepgen': self.step_signalname,               
                'jogminus': self.tunejogminus,
                'jogplus': self.tunejogplus,
                'run': self.widgets[axis+"tunerun"].get_active(),
                'amplitude': self.widgets[axis+"tuneamplitude"].get_value(),
                'accel': self.widgets[axis+"tuneacc"].get_value(),
                'vel': self.widgets[axis+"tunevel"].get_value(),
                'velps': (self.widgets[axis+"tunevel"].get_value()/60),
                'dir': self.widgets[axis+"tunedir"].get_active(),
                'pause':int(self.widgets[axis+"tunepause"].get_value()),
                'enable':self.widgets[axis+"tuneenable"].get_active()
            })
        else:
            halrun.write("""  
                setp pid.0.Pgain  %(p)f
                setp pid.0.Igain  %(i)f
                setp pid.0.Dgain  %(d)f 
                setp pid.0.bias   %(bias)f
                setp pid.0.FF0    %(ff0)f
                setp pid.0.FF1    %(ff1)f     
                setp pid.0.FF2    %(ff2)f
                setp pid.0.bias   %(bias)f
                setp pid.0.deadband  %(deadband)f
                setp steptest.0.jog-minus %(jogminus)s
                setp steptest.0.jog-plus %(jogplus)s
                setp steptest.0.run %(run)s
                setp steptest.0.amplitude %(amplitude)f
                setp steptest.0.maxvel %(velps)f
                setp steptest.0.maxaccel %(accel)f
                setp steptest.0.dir %(dir)s
                setp steptest.0.pause %(pause)d
                sets enable %(enable)s
            """ % {
                'p':self.widgets[axis+"tunecurrentP"].get_value(),
                'i':self.widgets[axis+"tunecurrentI"].get_value(),
                'd':self.widgets[axis+"tunecurrentD"].get_value(),
                'ff0':self.widgets[axis+"tunecurrentFF0"].get_value(),
                'ff1':self.widgets[axis+"tunecurrentFF1"].get_value(),
                'ff2':self.widgets[axis+"tunecurrentFF2"].get_value(),
                'bias':self.widgets[axis+"tunecurrentbias"].get_value(),
                'deadband':self.widgets[axis+"tunecurrentdeadband"].get_value(),
                'invert':self.widgets[axis+"tuneinvertmotor"].get_active(),
                'jogminus': self.tunejogminus,
                'jogplus': self.tunejogplus,
                'run': self.widgets[axis+"tunerun"].get_active(),
                'amplitude': self.widgets[axis+"tuneamplitude"].get_value(),
                'accel': self.widgets[axis+"tuneacc"].get_value(),
                'vel': self.widgets[axis+"tunevel"].get_value(),
                'velps': (self.widgets[axis+"tunevel"].get_value()/60),
                'dir': self.widgets[axis+"tunedir"].get_active(),
                'pause':int(self.widgets[axis+"tunepause"].get_value()),
                'enable':self.widgets[axis+"tuneenable"].get_active()
            })
        halrun.flush()

    def on_tunejogminus_pressed(self, w):
        self.tunejogminus = 1
        self.update_tune_axis_params()
    def on_tunejogminus_released(self, w):
        self.tunejogminus = 0
        self.update_tune_axis_params()
    def on_tunejogplus_pressed(self, w):
        self.tunejogplus = 1
        self.update_tune_axis_params()
    def on_tunejogplus_released(self, w):
        self.tunejogplus = 0
        self.update_tune_axis_params()
    # TODO fix scaling for servos:
    def on_tuneinvertmotor_toggled(self, w):
        axis = self.axis_under_tune
        w = self.widgets
        if w[axis+"tuneinvertmotor"].get_active():
            self.scale = get_value(w[axis + "stepscale"]) * -1
        else:
            self.scale = get_value(w[axis + "stepscale"])                 
        self.update_tune_axis_params()

    # openloop servo test
    def test_axis(self, axis):
        if not self.check_for_rt(self):
            return
        if not self.data.findsignal( (axis + "-pwm-pulse")) or not self.data.findsignal( (axis + "-encoder-a")):
             self.warning_dialog( _(" You must designate a ENCODER signal and a PWM signal for this axis test") , True)     
             return
        self.halrun = halrun = os.popen("halrun -sf > /dev/null", "w")  
        data = self.data
        widgets = self.widgets
        axnum = "xyzas".index(axis)
        pump = False
        fastdac = get_value(widgets["fastdac"])
        slowdac = get_value(widgets["slowdac"])
        dacspeed = widgets.Dac_speed_fast.get_active()
        enc_scale = get_value(widgets[axis+"encoderscale"])
        pump = self.data.findsignal("charge-pump")

        halrun.write("loadrt threads period1=%d name1=fast fp1=0 period2=%d name2=slow \n" % (100000, self.data.servoperiod  ))       
        self.hal_cmnds("LOAD")
        halrun.write("loadrt steptest\n")
        halrun.write("loadusr halscope\n")
        self.hal_cmnds("READ")
        if pump:
            halrun.write( "loadrt charge_pump\n")
            halrun.write( "setp charge-pump.enable true\n")
            halrun.write( "net charge-pump <= charge-pump.out\n")
            halrun.write( "addf charge-pump slow\n")                 
        halrun.write("addf steptest.0 slow\n")
        self.hal_cmnds("WRITE")
        # set enable pin if used (output)
        temp = self.data.findsignal( "enable")
        self.amp = self.data.make_pinname(temp)
        if self.amp:
            if "hm2_" in self.amp:    
                print "got here"
                halrun.write("setp %s true\n"% (self.amp + ".is_output"))             
                halrun.write("setp %s false\n"% (self.amp + ".out"))
                if self.data[temp+"inv"] == True:
                    halrun.write("setp %s true\n"%  (self.amp + ".invert_output"))
                self.amp = self.amp + ".out"             
            if "parport" in self.amp:
                halrun.write("    setp %s true\n" % (self.amp ))
                if self.data[temp+"inv"] == True:
                    halrun.write("    setp %s true\n" % (self.amp + "-invert"))  
            halrun.write("loadusr halmeter -s pin %s -g 0 475 330\n"%  (self.amp))     
        # setup pwm generator
        temp = self.data.findsignal( "estop-out")
        estop = self.data.make_pinname(temp)
        if estop:        
            if "hm2_" in estop:
                halrun.write("setp %s true\n"%  (estop + ".is_output"))    
                halrun.write("setp %s true\n"%  (estop + ".out"))
                if self.data[temp+"inv"] == True:
                    halrun.write("setp %s true\n"%  (estop + ".invert_output"))
                estop = estop + ".out"
            if "parport" in estop:
                halrun.write("    setp %s true\n" % (estop))
                if self.data[temp+"inv"] == True:
                    halrun.write("    setp %s true\n" % (estop + "-invert"))  
            halrun.write("loadusr halmeter -s pin %s -g 0 550 330\n"%  (estop)) 
        # set charge pump if used
        temp = self.data.findsignal( "charge-pump")
        pump = self.data.make_pinname(temp)
        if pump:        
            if "hm2_" in pump:
                halrun.write("setp %s true\n"%  (pump + ".is_output"))    
                halrun.write("setp %s true\n"%  (pump + ".out"))
                if self.data[temp+"inv"] == True:
                    halrun.write("setp %s true\n"%  (pump + ".invert_output"))
                pump = pump + ".out"              
            if "parport" in pump:
                halrun.write("    setp %s true\n" % (pump))
                if self.data[temp+"inv"] == True:
                    halrun.write("    setp %s true\n" % (pump + "-invert"))  
            halrun.write( "net charge-pump %s\n"%(pump))
            halrun.write("loadusr halmeter -s pin %s -g 0 500 330\n"%  (pump))             
        # setup pwm generator
        pwm = self.data.make_pinname(self.data.findsignal( (axis + "-pwm-pulse")))
        if pwm:          
            halrun.write("net dac %s \n"%  (pwm +".value"))
            halrun.write("setp %s \n"%  (pwm +".enable true"))
            halrun.write("setp %s \n"%  (pwm +".scale 10"))
            halrun.write("loadusr halmeter -s pin %s -g 550 500 330\n"%  (pwm +".value"))
            halrun.write("loadusr halmeter pin %s -g 550 375\n"% (pwm +".value") )
        # set up encoder     
        self.enc = self.data.make_pinname(self.data.findsignal( (axis + "-encoder-a")))
        if self.enc:           
            halrun.write("net enc-reset %s \n"%  (self.enc +".reset"))
            halrun.write("setp %s.scale %f \n"%  (self.enc, enc_scale))
            halrun.write("setp %s \n"%  (self.enc +".filter true"))
            halrun.write("loadusr halmeter -s pin %s -g 550 550 330\n"%  (self.enc +".position"))
            halrun.write("loadusr halmeter -s pin %s -g 550 600 330\n"%  (self.enc +".velocity"))
       
        widgets.openloopdialog.set_title(_("%s Axis Test") % axis.upper())
        widgets.openloopdialog.move(550,0)
        self.jogplus = self.jogminus = self.enc_reset = self.enable_amp = 0
        self.axis_under_test = axis
        widgets.testinvertmotor.set_active(widgets[axis+"invertmotor"].get_active())
        widgets.testinvertencoder.set_active(widgets[axis+"invertencoder"].get_active())
        widgets.testoutputoffset.set_value(widgets[axis+"outputoffset"].get_value())
        widgets.testenc_scale.set_value(float(enc_scale))   
        self.update_axis_params()      
        halrun.write("start\n"); halrun.flush()
        self.widgets['window1'].set_sensitive(0)
        self.widgets.jogminus.set_sensitive(0)
        self.widgets.jogplus.set_sensitive(0)
        widgets.openloopdialog.show_all()
        result = widgets.openloopdialog.run()

        widgets.openloopdialog.hide()
        if self.amp:
             halrun.write("setp %s false\n"% (self.amp))
        if estop:
             halrun.write("setp %s false\n"% (estop))
        time.sleep(.001)
        halrun.close()        
        if result == gtk.RESPONSE_OK:
            #widgets[axis+"maxacc"].set_text("%s" % widgets.testacc.get_value())
            widgets[axis+"invertmotor"].set_active(widgets.testinvertmotor.get_active())
            widgets[axis+"invertencoder"].set_active(widgets.testinvertencoder.get_active())
            widgets[axis+"outputoffset"].set_value(widgets.testoutputoffset.get_value())
            widgets[axis+"encoderscale"].set_value(widgets.testenc_scale.get_value())
            #widgets[axis+"maxvel"].set_text("%s" % widgets.testvel.get_value())
        self.axis_under_test = None
        self.widgets['window1'].set_sensitive(1)
    
    def update_axis_params(self, *args):
        axis = self.axis_under_test
        if axis is None: return
        halrun = self.halrun
        enc_scale = self.widgets.testenc_scale.get_value()
        if self.widgets.testinvertencoder.get_active() == True: 
            enc_invert = -1
        else: 
            enc_invert = 1
        if self.widgets.Dac_speed_fast.get_active() == True:
            output = get_value(self.widgets.fastdac)
        else: 
            output = get_value(self.widgets.slowdac)
        if self.jogminus == 1:
            output = output * -1
        elif not self.jogplus == 1:
            output = 0
        if self.widgets.testinvertmotor.get_active() == True: 
            output = output * -1
        output += get_value(self.widgets.testoutputoffset)
        if self.amp:
            halrun.write("setp %s %d\n"% (self.amp, self.enable_amp))
        halrun.write("""setp %(scalepin)s.scale %(scale)f\n""" % { 'scalepin':self.enc, 'scale': (enc_scale * enc_invert)})
        halrun.write("""sets dac %(output)f\n""" % { 'output': output})
        halrun.write("""sets enc-reset %(reset)d\n""" % { 'reset': self.enc_reset})
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
    def on_resetbutton_pressed(self, w):
        self.enc_reset = 1
        self.update_axis_params()
    def on_resetbutton_released(self, w):
        self.enc_reset = 0
        self.update_axis_params()
    def on_testinvertmotor_toggled(self, w):
        self.update_axis_params()
    def on_testinvertencoder_toggled(self, w):
        self.update_axis_params()
    def on_testoutputoffset_value_changed(self, w):
        self.update_axis_params()
    def on_enableamp_toggled(self, w):
        self.enable_amp = self.enable_amp * -1 + 1
        self.widgets.jogminus.set_sensitive(self.enable_amp)
        self.widgets.jogplus.set_sensitive(self.enable_amp)
        self.update_axis_params()

    def run(self, filename=None):
        if filename is not None:
            self.data.load(filename, self)
            self.widgets.druid1.set_page(self.widgets.basicinfo)
        gtk.main()
   
    def hal_cmnds(self,command ):
        halrun = self.halrun
        if command == "LOAD":
            halrun.write("loadrt probe_parport\n")
            # parport stuff
            if self.data.number_pports>0:
                port3name = port2name = port1name = port3dir = port2dir = port1dir = ""
                if self.data.number_pports>2:
                     port3name = " " + self.data.ioaddr3
                     if self.data.pp3_direction:
                        port3dir =" out"
                     else: 
                        port3dir =" in"
                if self.data.number_pports>1:
                     port2name = " " + self.data.ioaddr2
                     if self.data.pp2_direction:
                        port2dir =" out"
                     else: 
                        port2dir =" in"
                port1name = self.data.ioaddr
                if self.data.pp1_direction:
                   port1dir =" out"
                else: 
                   port1dir =" in"
                halrun.write("loadrt hal_parport cfg=\"%s%s%s%s%s%s\"\n" % (port1name, port1dir, port2name, port2dir, port3name, port3dir))
            # mesa stuff
            halrun.write("loadrt hostmot2\n")
            board0 = self.data.mesa0_currentfirmwaredata[_BOARDNAME]
            board1 = self.data.mesa1_currentfirmwaredata[_BOARDNAME]
            driver0 = self.data.mesa0_currentfirmwaredata[_HALDRIVER]
            driver1 = self.data.mesa1_currentfirmwaredata[_HALDRIVER]
            directory0 = self.data.mesa0_currentfirmwaredata[_DIRECTORY]
            directory1 = self.data.mesa1_currentfirmwaredata[_DIRECTORY]
            firm0 = self.data.mesa0_currentfirmwaredata[_FIRMWARE]
            firm1 = self.data.mesa1_currentfirmwaredata[_FIRMWARE]
            if self.data.number_mesa == 1:            
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_tppwmgens=%d num_stepgens=%d"\n """ % (
                    driver0, directory0, firm0, self.data.mesa0_numof_encodergens, self.data.mesa0_numof_pwmgens, self.mesa0_numof_tppwmgens, self.data.mesa0_numof_stepgens ))
            elif self.data.number_mesa == 2 and (driver0 == driver1):
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_tppwmgens=%d num_stepgens=%d,\
                                firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"\n
                    """ % (
                    driver0, directory0, firm0, self.data.mesa0_numof_encodergens, self.data.mesa0_numof_pwmgens, self.data.mesa0_numof_stepgens,
                    directory1, firm1, self.data.mesa1_numof_encodergens, self.data.mesa1_numof_pwmgens, self.mesa0_numof_tppwmgens, self.data.mesa1_numof_stepgens ))
            elif self.data.number_mesa == 2:
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_tppwmgens=%d num_stepgens=%d"\n """ % (
                    driver0, directory0, firm0, self.data.mesa0_numof_encodergens, self.data.mesa0_numof_pwmgens, self.mesa0_numof_tppwmgens, self.data.mesa0_numof_stepgens ))
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_tppwmgens=%d num_stepgens=%d"\n """ % (
                    driver1, directory1, firm1, self.data.mesa1_numof_encodergens, self.data.mesa1_numof_pwmgens, self.mesa0_numof_tppwmgens, self.data.mesa1_numof_stepgens ))
            for boardnum in range(0,int(self.data.number_mesa)):
                if boardnum == 1 and (board0 == board1):
                    halnum = 1
                else:
                    halnum = 0
                if self.data["mesa%d_numof_pwmgens"% boardnum] > 0:
                    halrun.write( "    setp hm2_%s.%d.pwmgen.pwm_frequency %d\n"% (
                     self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME],halnum, self.data["mesa%d_pwm_frequency"% boardnum] ))
                    halrun.write( "    setp hm2_%s.%d.pwmgen.pdm_frequency %d\n"% ( 
                    self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum,self.data["mesa%d_pdm_frequency"% boardnum] ))
                halrun.write( "    setp hm2_%s.%d.watchdog.timeout_ns %d\n"% ( 
                    self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum,self.data["mesa%d_watchdog_timeout"% boardnum] ))  
        if command == "READ":
            if self.data.number_pports > 0:
                halrun.write( "addf parport.0.read fast\n")
            if self.data.number_pports > 1:
                halrun.write( "addf parport.1.read fast\n")
            if self.data.number_pports > 2:
                halrun.write( "addf parport.2.read fast\n")
            for boardnum in range(0,int(self.data.number_mesa)):
                if boardnum == 1 and (self.data.mesa0_currentfirmwaredata[_BOARDNAME] == self.data.mesa1_currentfirmwaredata[_BOARDNAME]):
                    halnum = 1
                else:
                    halnum = 0         
                halrun.write( "addf hm2_%s.%d.read slow\n"% (self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum))
                halrun.write( "addf hm2_%s.%d.pet_watchdog  slow\n"% (self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum))
        if command == "WRITE":
            if self.data.number_pports > 0:
                halrun.write( "addf parport.0.write fast\n")
            if self.data.number_pports > 1:
                halrun.write( "addf parport.1.write fast\n")
            if self.data.number_pports > 2:
                halrun.write( "addf parport.2.write fast\n")
            for boardnum in range(0,int(self.data.number_mesa)):
                if boardnum == 1 and (self.data.mesa0_currentfirmwaredata[_BOARDNAME] == self.data.mesa1_currentfirmwaredata[_BOARDNAME]):
                    halnum = 1
                else:
                    halnum = 0         
                halrun.write( "addf hm2_%s.%d.write slow\n"% (self.data["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum))

#***************************************************************
# testpanel code
class hal_interface:
    def __init__(self):  
        try: 
            self.c = hal.component("testpanel")      
        except:
            print"problem in HAL routine"
class Data2:
    def __init__(self):
        self.inv = []
        self.swch = []
        self.led = []
        self.enc = []
        self.pwm = []
        self.stp = []
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class LED(gtk.DrawingArea):

    def __init__(self, parent):
        self.par = parent       
        super(LED, self).__init__() 
        self._dia = 10
        self._state = 0
        self._on_color = [0.3, 0.4, 0.6]
        self._off_color = [0.9, 0.1, 0.1]
        self.set_size_request(25, 25)
        self.connect("expose-event", self.expose)
        

    # This method draws our widget
    # it draws a black circle for a rim around LED
    # Then depending on self.state
    # fills in that circle with on or off color.
    # the dim depends on self.diam
    def expose(self, widget, event):
        cr = widget.window.cairo_create()
        cr.set_line_width(3)
        #cr.set_source_rgb(0, 0, 0.0)    
        self.set_size_request(25, 25)  
        #cr.set_source_rgb(0, 0, 0.0)    
        #self.set_size_request(self._dia*2+5, self._dia*2+5) 
        w = self.allocation.width
        h = self.allocation.height
        cr.translate(w/2, h/2)
        #cr = widget.window.cairo_create()
        lg2 = cairo.RadialGradient(0, 0, 0,  0, 0, self._dia)
        if self._state:
            r = self._on_color[0]
            g = self._on_color[1]
            b = self._on_color[2]
        else:
            r = self._off_color[0]
            g = self._off_color[1]
            b = self._off_color[2]
        lg2.add_color_stop_rgba(1, r/.25,g/.25,b/.25, 1)
        lg2.add_color_stop_rgba(.5, r,g,b, .5)
        #lg2.add_color_stop_rgba(0, 0, 0, 0, 1)
        cr.arc(0, 0, self._dia, 0, 2*math.pi)
        cr.stroke_preserve()
        #cr.rectangle(20, 20, 300, 100)
        cr.set_source(lg2)
        cr.fill()

        return False
      
    # This sets the LED on or off
    # and then redraws it
    # Usage: ledname.set_active(True) 
    def set_active(self, data2 ):
        self._state = data2
        self.queue_draw()
    
    # This allows setting of the on and off color
    # Usage: ledname.set_color("off",[r,g,b])
    def set_color(self, state, color = [0,0,0] ):
        if state == "off":
            self._off_color = color
        elif state == "on":
            self._on_color = color
        else:
            return

    def set_dia(self, dia):
        self._dia = dia
        self.queue_draw()
 
class PyApp(gtk.Window): 

    def switch_callback(self, widget, component , boardnum,number, data=None):   
        print component,boardnum,number,data
        if component == "switch":
            invrt = self.data2["brd%dinv%d" % (boardnum,number)].get_active()
            if (data and not invrt ) or (not data and invrt):
                self.hal.c["brd.%d.switch.%d"% (boardnum, number)] = True
            else:
                self.hal.c["brd.%d.switch.%d"% (boardnum, number)] = False
        if component == "invert":
            self.switch_callback(None,"switch",boardnum,number,False)

    def pwm_callback(self, widget, component , boardnum,number, data=None):
        if component == "pwm":
            value = self.data2["brd%dpwm%dadj" % (boardnum,number)].get_value()
            active = self.data2["brd%dpmw_ckbutton%d"% (boardnum,number)].get_active()
            self.hal.c["brd.%d.pwm.%d.enable"% (boardnum, number)] = active
            if active:
                self.hal.c["brd.%d.pwm.%d.value"% (boardnum, number)] = value
            else:
                 self.hal.c["brd.%d.pwm.%d.value"% (boardnum, number)] = 0
    
    def stp_callback(self, widget, component , boardnum,number, data=None):
        if component == "stp":
            value = self.data2["brd%dstp%dcmd" % (boardnum,number)].get_value()
            active = self.data2["brd%dstp_ckbutton%d"% (boardnum,number)].get_active()
            self.hal.c["brd.%d.stp.%d.enable"% (boardnum, number)] = active
            if active:
                self.hal.c["brd.%d.stp.%d.cmd"% (boardnum, number)] = value
            

    def quit(self,widget):  
        self.widgets['window1'].set_sensitive(1)                 
        gobject.source_remove(self.timer) 
        self.hal.c.exit()
        self.app.halrun.close()
        print "**** Mesa test panel closed out."
        return True

    def update(self):      
        if hal.component_exists("testpanel"):
            for i in (0,1):
                for j in range(0,72):
                    try:
                        self.data2["brd%dled%d"%(i,j)].set_active(self.hal.c["brd.%d.led.%d"% (i,j)]) 
                    except :
                        continue    
                for k in range(0,16):
                    try:
                        self.data2["brd%denc%dcount"%(i,k)].set_text("%s"% str(self.hal.c["brd.%d.enc.%d.count"% (i,k)])) 
                    except :
                        continue 
            return True # keep running this event
        else:
            return False # kill the event

    # This creates blank labels for placemarks for components
    # such as encoders that use 3 or 4 pins as input
    # but only need one line for user interaction
    # this keeps the page uniform
    def make_blank(self,container,boardnum,number):
        #blankname = "enc-%d" % (number)
        #self.data2["brd%denc%d" % (boardnum,number)]= gtk.Button("Reset-%d"% number)
        #self.hal.c.newpin(encname, hal.HAL_S32, hal.HAL_IN)
        label = gtk.Label("     ")
        container.pack_start(label, False, False, 10)
        label = gtk.Label("      ")
        container.pack_start(label, False, False, 10)
  
    # This creates widgets and HAL pins for encoder controls
    def make_enc(self,container,boardnum,number):
        encname = "brd.%d.enc.%d.reset" % (boardnum,number)   
        print"making HAL pin enc bit Brd %d,num %d"%(boardnum,number)   
        self.hal.c.newpin(encname, hal.HAL_BIT, hal.HAL_OUT)
        hal.new_sig(encname+"-signal","bit")
        hal.connect("testpanel."+encname,encname+"-signal")
        self.data2["brd%denc%dreset" % (boardnum,number)]= gtk.Button("Reset-%d"% number)
        container.pack_start(self.data2["brd%denc%dreset" % (boardnum,number)], False, False, 10)
        encname = "brd.%d.enc.%d.count" % (boardnum,number)
        print"making HAL pin enc s32 brd %d num %d"%(boardnum,number)      
        self.hal.c.newpin(encname, hal.HAL_S32, hal.HAL_IN)
        hal.new_sig(encname+"-signal","s32")
        hal.connect("testpanel."+encname,encname+"-signal")
        label = self.data2["brd%denc%dcount" % (boardnum,number)] = gtk.Label("Encoder-%d"% (number))
        label.set_size_request(100, -1)
        container.pack_start(label, False, False, 10)
    
    # This creates widgets and HAL pins for stepper controls 
    def make_stp(self,container,boardnum,number):
        stpname = "brd.%d.stp.%d.cmd" % (boardnum,number)
        self.hal.c.newpin(stpname, hal.HAL_FLOAT, hal.HAL_OUT)
        hal.new_sig(stpname+"-signal","float")
        hal.connect("testpanel."+stpname,stpname+"-signal")
        stpname = "brd.%d.stp.%d.enable" % (boardnum,number)
        self.hal.c.newpin(stpname, hal.HAL_BIT, hal.HAL_OUT)
        hal.new_sig(stpname+"-signal","bit")
        hal.connect("testpanel."+stpname,stpname+"-signal")
        adj = gtk.Adjustment(0.0, -1000.0, 1000.0, 1.0, 5.0, 0.0)
        spin = self.data2["brd%dstp%dcmd" % (boardnum,number)]= gtk.SpinButton(adj, 0, 1)  
        adj.connect("value_changed", self.stp_callback,"stp",boardnum,number,None)    
        container.pack_start(spin, False, False, 10)
        ckb = self.data2["brd%dstp_ckbutton%d"% (boardnum,number)] = gtk.CheckButton("Enable %d"% (number))
        ckb.connect("toggled", self.stp_callback, "stp",boardnum,number,None)
        container.pack_start(ckb, False, False, 10)
        

    # This places a spinbox for pwm value and a checkbox to enable pwm
    # It creates two HAL pins
    def make_pwm(self,container,boardnum,number):
        pwmname = "brd.%d.pwm.%d.value" % (boardnum,number)
        print"making HAL pin pwm float brd%d num %d"%(boardnum,number)
        self.hal.c.newpin(pwmname, hal.HAL_FLOAT, hal.HAL_OUT)
        hal.new_sig(pwmname+"-signal","float")
        hal.connect("testpanel."+pwmname,pwmname+"-signal")
        pwmname = "brd.%d.pwm.%d.enable" % (boardnum,number)
        print"making HAL pin pwm bit brd %d num %d"%(boardnum,number)
        self.hal.c.newpin(pwmname, hal.HAL_BIT, hal.HAL_OUT)
        hal.new_sig(pwmname+"-signal","bit")
        hal.connect("testpanel."+pwmname,pwmname+"-signal")
        adj = self.data2["brd%dpwm%dadj" % (boardnum,number)] = gtk.Adjustment(0.0, -10.0, 10.0, 0.1, 0.5, 0.0)
        adj.connect("value_changed", self.pwm_callback,"pwm",boardnum,number,None)      
        pwm = self.data2["brd%dpwm%d" % (boardnum,number)] = gtk.HScale(adj)
        pwm.set_digits(1)
        pwm.set_size_request(100, -1)      
        container.pack_start(pwm, False, False, 10)        
        ckb = self.data2["brd%dpmw_ckbutton%d"% (boardnum,number)] = gtk.CheckButton("PWM-%d\nON"% (number))
        ckb.connect("toggled", self.pwm_callback, "pwm",boardnum,number,None)
        container.pack_start(ckb, False, False, 10)
    
    # This places a LED and a label in specified container
    # it specifies the led on/off colors
    # and creates a HAL pin
    def make_led(self,container,boardnum,number):
        ledname = "brd.%d.led.%d" % (boardnum,number)
        print"making HAL pin led bit brd %d num %d"%(boardnum,number)
        self.hal.c.newpin(ledname, hal.HAL_BIT, hal.HAL_IN)
        hal.new_sig(ledname+"-signal","bit")
        hal.connect("testpanel."+ledname,ledname+"-signal")
        led = self.data2["brd%dled%d" % (boardnum,number)] = LED(self)
        led.set_color("off",[1,0,0]) # red
        led.set_color("on",[0,1,0]) # Green
        container.pack_start(led, False, False, 10)
        label = gtk.Label("<--GPIO-%d"% (number))
        container.pack_start(label, False, False, 10)

    # This is for placing a button (switch) and an invert check box into
    # a specified container. It also creates the HAL pin
    # and connects some signals. 
    def make_switch(self,container,boardnum,number):
        # make a HAL pin
        switchname = "brd.%d.switch.%d" % (boardnum,number)
        print"making HAL pin switch bit brd %d num %d"%(boardnum,number)
        self.hal.c.newpin(switchname, hal.HAL_BIT, hal.HAL_OUT)
        hal.new_sig(switchname+"-signal","bit")
        hal.connect("testpanel."+switchname,switchname+"-signal")
        # add button to container using boarnum and number as a reference     
        button = self.data2["brd%dswch%d" % (boardnum,number)]= gtk.Button("OUT-%d"% number)
        container.pack_start(button, False, False, 10)
        # connect signals
        button.connect("pressed", self.switch_callback, "switch",boardnum,number,True)
        button.connect("released", self.switch_callback, "switch",boardnum,number,False) 
        # add invert switch
        ckb = self.data2["brd%dinv%d" % (boardnum,number)]= gtk.CheckButton("Invert")
        container.pack_start(ckb, False, False, 10) 
        ckb.connect("toggled", self.switch_callback, "invert",boardnum,number,None)
    
    def __init__(self,App,data,widgets):
        super(PyApp, self).__init__()
        print "init super pyapp"
        self.data2 = Data2()
        self.data = data
        self.app = App
        self.widgets = widgets
        #self.halrun = self.app.halrun
        print "entering HAL init"
        self.hal = hal_interface()
        print "done HAL init"
        self.set_title("Mesa Test Panel")
        self.set_size_request(450, 450)        
        self.set_position(gtk.WIN_POS_CENTER)
        self.connect_after("destroy", self.quit)
        self.timer = gobject.timeout_add(100, self.update)
        print "added timer"
        brdnotebook = gtk.Notebook()
        brdnotebook.set_tab_pos(gtk.POS_TOP)
        brdnotebook.show()
        self.add(brdnotebook)             
        
        for boardnum in range(0,int(self.data.number_mesa)):
            board = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_BOARDNAME]+".%d"% boardnum
            self.data2["notebook%d"%boardnum] = gtk.Notebook()
            self.data2["notebook%d"%boardnum].set_tab_pos(gtk.POS_TOP)
            self.data2["notebook%d"%boardnum].show()
            label = gtk.Label("Mesa Board Number %d"% (boardnum))      
            brdnotebook.append_page(self.data2["notebook%d"%boardnum], label)
            for concount,connector in enumerate(self.data["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                table = gtk.Table(12, 3, False)
                seperator = gtk.VSeparator()
                table.attach(seperator, 1, 2, 0, 12,True)
                for pin in range (0,24):
                    if pin >11:
                        column = 2
                        adjust = -12    
                    else:
                        column = 0
                        adjust = 0
                    firmptype,compnum = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pin+(concount*24)]
                    pinv = 'mesa%dc%dpin%dinv' % (boardnum,connector,pin)
                    ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                    pintype = self.widgets[ptype].get_active_text()
                    pininv = self.widgets[pinv].get_active()
                    truepinnum = (concount*24) + pin
                    # for output / open drain pins
                    if  pintype in (GPIOO,GPIOD): 
                        h = gtk.HBox(False,2)
                        self.make_switch(h,boardnum,truepinnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                        hal.set_pin("hm2_%s.gpio.%03d.is_output"% (board,truepinnum ),"true")
                        if pininv:  hal.set_pin("hm2_%s.gpio.%03d.invert_output"% (board,truepinnum ),"true")
                        hal.connect("hm2_%s.gpio.%03d.out"% (board,truepinnum ),"brd.%d.switch.%d-signal" % (boardnum,truepinnum))
                    # for input pins
                    elif pintype == GPIOI: 
                        h = gtk.HBox(False,2)
                        self.make_led(h,boardnum,truepinnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                        if pininv: hal.connect("hm2_%s.gpio.%03d.in_not"% (board,truepinnum),"brd.%d.led.%d-signal"% (boardnum,truepinnum))
                        else:   hal.connect("hm2_%s.gpio.%03d.in"% (board,truepinnum),"brd.%d.led.%d-signal"% (boardnum,truepinnum))
                    # for encoder pins
                    elif pintype in (ENCA,ENCB,ENCI,ENCM):
                        h = gtk.HBox(False,2)
                        if pintype == ENCA:
                            self.make_enc(h,boardnum,compnum)
                            hal.connect("hm2_%s.encoder.%02d.reset"% (board,compnum), "brd.%d.enc.%d.reset-signal"% (boardnum,compnum))
                            hal.connect("hm2_%s.encoder.%02d.count"% (board,compnum), "brd.%d.enc.%d.count-signal"% (boardnum,compnum))
                        else:
                            self.make_blank(h,boardnum,compnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                    # for PWM pins
                    elif pintype in (PWMP,PWMD,PWME,PDMP,PDMD,PDME):
                        h = gtk.HBox(False,2)
                        if pintype in (PWMP,PDMP):
                            self.make_pwm(h,boardnum,compnum)
                            hal.connect("hm2_%s.pwmgen.%02d.enable"% (board,compnum),"brd.%d.pwm.%d.enable-signal"% (boardnum,compnum)) 
                            hal.connect("hm2_%s.pwmgen.%02d.value"% (board,compnum),"brd.%d.pwm.%d.value-signal"% (boardnum,compnum)) 
                            hal.set_pin("hm2_%s.pwmgen.%02d.scale"% (board,compnum),"10") 
                        else:
                            self.make_blank(h,boardnum,compnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                    # for Stepgen pins
                    elif pintype in (STEPA,STEPB):
                        h = gtk.HBox(False,2)
                        if pintype == STEPA:          
                            self.make_stp(h,boardnum,compnum)
                            hal.connect("hm2_%s.stepgen.%02d.enable"% (board,compnum),"brd.%d.stp.%d.enable-signal"% (boardnum,compnum))
                            hal.connect("hm2_%s.stepgen.%02d.position-cmd"% (board,compnum),"brd.%d.stp.%d.position-cmd-signal"% (boardnum,compnum))   
                            hal.set_pin("hm2_%s.stepgen.%02d.maxaccel"% (board,compnum),"0")
                            hal.set_pin("hm2_%s.stepgen.%02d.maxvel"% (board,compnum),"2000")
                            hal.set_pin("hm2_%s.stepgen.%02d.steplen"% (board,compnum),"2000")
                            hal.set_pin("hm2_%s.stepgen.%02d.stepspace"% (board,compnum),"2000")
                            hal.set_pin("hm2_%s.stepgen.%02d.dirhold"% (board,compnum),"2000")
                            hal.set_pin("hm2_%s.stepgen.%02d.dirsetup"% (board,compnum),"2000")
                        else:
                            self.make_blank(h,boardnum,compnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                    else:
                        print "pintype error IN mesa test panel method pintype %s boardnum %d connector %d pin %d"% (pintype,boardnum,connector,pin)
                label = gtk.Label("Mesa %d-Connector %d"% (boardnum,connector))      
                self.data2["notebook%d"%boardnum].append_page(table, label)
           
        self.show_all() 
        self.widgets['window1'].set_sensitive(0) 
        self.hal.c.ready()
        print "got to end of panel"

        
        
# testpanel code end
#****************************************************************
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
