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
    wizdir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
    wizard = os.path.join(wizdir, "emc2-wizard.gif")

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
_("Step/Dir Gen-A"),_("Step/Dir Gen-B"),_("Step/Dir Gen-C"),_("Step/Dir Gen-D"),_("Step/Dir Gen-E"),_("Step/dir Gen-F"),
_("Pulse Width Gen-P"),_("Pulse Width Gen-D"),_("Pulse Width Gen-E"),
_("Pulse Density Gen-P"),_("Pulse Density Gen-D"),_("Pulse Density Gen-E") ]

_BOARDTITLE = 0;_BOARDNAME = 1;_FIRMWARE = 2;_DIRECTORY = 3;_HALDRIVER = 4;_MAXENC = 5;_MAXPWM = 6;_MAXSTEP = 7;_ENCPINS = 8
_STEPPINS = 9;_HASWATCHDOG = 10;_MAXGPIO = 11;_LOWFREQ = 12;_HIFREQ = 13;_NUMOFCNCTRS = 14;_STARTOFDATA = 15
# boardname, firmwarename, firmware directory,Hal driver name,
# max encoders, max pwm gens, 
# max step gens, number of pins per encoder,
# number of pins per step gen, 
# has watchdog, max GPIOI, 
# low frequency rate , hi frequency rate, 
# available connector numbers,  then list of component type and logical number
mesafirmwaredata = [
    ["5i20", "5i20", "SV12", "5i20", "hm2_pci", 12, 12, 0, 3, 0, 1, 72 , 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10] ],
    ["5i20", "5i20", "SVST8_4", "5i20", "hm2_pci", 8, 8, 4, 3, 6, 1, 72, 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i20", "5i20", "SVST2_4_7i47", "5i20", "hm2_pci", 4, 2, 4, 3, 2, 1, 72, 33, 100, [2,3,4],
        [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[ENCA,0],[ENCA,2],[ENCB,0],[ENCB,2],[ENCI,0],[ENCI,2],[ENCA,1],[ENCA,3],
                [ENCB,1],[ENCB,3],[ENCI,1],[ENCI,3],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[PWMP,0],[PWMD,0],[PWMP,1],[PWMD,1],
        [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
        [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0], ],
    ["5i20", "5i20", "SVST2_8", "5i20", "hm2_pci", 2, 2, 8, 3, 6, 1, 72, 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
        [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i20", "5i20", "SVST8_4IM2", "5i20", "hm2_pci", 8, 8, 4, 4, 2, 1, 72, 33, 100, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCM,0],[ENCM,1],[ENCM,2],[ENCM,3],[ENCM,4],[ENCM,5],[ENCM,6],[ENCM,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                 [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3] ],
    ["5i22-1", "5i22", "SV16", "5i22", "hm2_pci", 16, 16, 0, 3, 0, 1, 96, 48, 96, [2,3,4,5],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10],
        [ENCB,13],[ENCA,13],[ENCB,12],[ENCA,12],[ENCI,13],[ENCI,12],[PWMP,13],[PWMP,12],[PWMD,13],[PWMD,12],[PWME,13],[PWME,12],
                  [ENCB,15],[ENCA,15],[ENCB,14],[ENCA,14],[ENCI,15],[ENCI,14],[PWMP,15],[PWMP,14],[PWMD,15],[PWMD,14],[PWME,15],[PWME,14] ],
    ["5i22-1", "5i22", "SVST8_8", "5i22", "hm2_pci", 8, 8, 8, 3, 6, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i22-1", "5i22", "SVS8_24", "5i22", "hm2_pci", 8, 8, 24, 3, 2, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
       [STEPA,12],[STEPB,12],[STEPA,13],[STEPB,13],[STEPA,14],[STEPB,14],[STEPA,15],[STEPB,15],[STEPA,16],[STEPB,16],[STEPA,17],[STEPB,17],
                [STEPA,18],[STEPB,18],[STEPA,19],[STEPB,19],[STEPA,20],[STEPB,20],[STEPA,21],[STEPB,21],[STEPA,22],[STEPB,22],[STEPA,23],[STEPB,23] ],
    ["5i22-1.5", "5i22", "SV16", "5i22", "hm2_pci", 16, 16, 0, 3, 0, 1, 96, 48, 96, [2,3,4,5],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10],
        [ENCB,13],[ENCA,13],[ENCB,12],[ENCA,12],[ENCI,13],[ENCI,12],[PWMP,13],[PWMP,12],[PWMD,13],[PWMD,12],[PWME,13],[PWME,12],
                  [ENCB,15],[ENCA,15],[ENCB,14],[ENCA,14],[ENCI,15],[ENCI,14],[PWMP,15],[PWMP,14],[PWMD,15],[PWMD,14],[PWME,15],[PWME,14] ],
    ["5i22-1.5", "5i22", "SVST8_8", "5i22", "hm2_pci", 8, 8, 8, 3, 6, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i22-1.5", "5i22", "SVS8_24", "5i22", "hm2_pci", 8, 8, 24, 3, 2, 1, 96, 48, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
       [STEPA,12],[STEPB,12],[STEPA,13],[STEPB,13],[STEPA,14],[STEPB,14],[STEPA,15],[STEPB,15],[STEPA,16],[STEPB,16],[STEPA,17],[STEPB,17],
                [STEPA,18],[STEPB,18],[STEPA,19],[STEPB,19],[STEPA,20],[STEPB,20],[STEPA,21],[STEPB,21],[STEPA,22],[STEPB,22],[STEPA,23],[STEPB,23] ],
    ["5i23", "5i23", "SV12", "5i23", "hm2_pci", 12, 12, 0, 3, 0, 1, 72 , 48, 96, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10] ],
    ["5i23", "5i23", "SVST8_4", "5i23", "hm2_pci", 8, 8, 4, 3, 6, 1, 72, 48, 96, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i23", "5i23", "SVST4_8", "5i23", "hm2_pci", 4, 4, 8, 3, 6, 1, 72, 48, 96, [2,3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                 [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                 [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["7i43-2", "7i43", "SV8", "7i43", "hm2_7i43", 8, 8, 0, 3, 0, 1, 48, 50, 100, [3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6] ],
    ["7i43-2", "7i43", "SV4_4", "7i43", "hm2_7i43", 4, 4, 4, 3, 6, 1, 48, 50, 100, [3,4],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2] ],
    ["7i43-2", "7i43", "SV4_6", "7i43", "hm2_7i43", 4, 4, 6, 3, 4, 1, 48, 50, 100, [3,4],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],
                [STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2] ],
    ["7i43-2", "7i43", "SV4_12", "7i43", "hm2_7i43", 4, 4, 12, 3, 2, 1, 48, 50, 100, [3,4],
        [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2] ],
    ["7i43-4", "7i43", "SV8", "7i43", "hm2_7i43", 8, 8, 0, 3, 0, 1, 48, 50, 100, [3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6] ],
    ["7i43-4", "7i43", "SV4_4", "7i43", "hm2_7i43", 4, 4, 4, 3, 6, 1, 48, 50, 100, [3,4],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2] ],
    ["7i43-4", "7i43", "SV4_6", "7i43", "hm2_7i43", 4, 4, 6, 3, 4, 1, 48, 50, 100, [3,4],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],
                [STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2] ],
    ["7i43-4", "7i43", "SV4_12", "7i43", "hm2_7i43", 4, 4, 12, 3, 2, 1, 48, 50, 100, [3,4],
        [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2] ],
    ["3x20-1", "3x20", "SV24", "3x20-1", "hm2_pci", 24, 24, 0, 3, 0, 1, 144, 50, 100, [4,5,6,7,8,9],
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
    ["3x20-1", "3x20", "SV16_24", "3x20-1", "hm2_pci", 16, 16, 24, 3, 2, 1, 144, 50, 100, [4,5,6,7,8,9],
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
ini_style = False


(UNUSED_OUTPUT,
ON, CW, CCW, BRAKE,
MIST, FLOOD, ESTOP, AMP,
PUMP, DOUT0, DOUT1, DOUT2, DOUT3) = hal_output_names = [
"unused-output", 
"spindle-enable", "spindle-cw", "spindle-ccw", "spindle-brake",
"coolant-mist", "coolant-flood", "estop-out", "enable",
"charge-pump", "dout-00", "dout-01", "dout-02", "dout-03"
]

(UNUSED_INPUT,
ESTOP_IN, PROBE,
HOME_X, HOME_Y, HOME_Z, HOME_A,
MIN_HOME_X, MIN_HOME_Y, MIN_HOME_Z, MIN_HOME_A,
MAX_HOME_X, MAX_HOME_Y, MAX_HOME_Z, MAX_HOME_A,
BOTH_HOME_X, BOTH_HOME_Y, BOTH_HOME_Z, BOTH_HOME_A,
MIN_X, MIN_Y, MIN_Z, MIN_A,
MAX_X, MAX_Y, MAX_Z, MAX_A,
BOTH_X, BOTH_Y, BOTH_Z, BOTH_A,
ALL_LIMIT, ALL_HOME, DIN0, DIN1, DIN2, DIN3,
JOGA, JOGB, JOGC, SELECT_A, SELECT_B, SELECT_C, SELECT_D,
JOGX_P,JOGX_N,JOGY_P,JOGY_N,JOGZ_P,JOGZ_N,JOGA_P,JOGA_N,
JOGSLCT_P, JOGSLCT_N, SPINDLE_CW, SPINDLE_CCW, SPINDLE_STOP,
SPINDLE_AT_SPEED   ) = hal_input_names = ["unused-input",
"estop-ext", "probe-in",
"home-x", "home-y", "home-z", "home-a",
"min-home-x", "min-home-y", "min-home-z", "min-home-a",
"max-home-x", "max-home-y", "max-home-z", "max-home-a",
"both-home-x", "both-home-y", "both-home-z", "both-home-a",
"min-x", "min-y", "min-z", "min-a",
"max-x", "max-y", "max-z", "max-a",
"both-x", "both-y", "both-z", "both-a",
"all-limit", "all-home", "din-00", "din-01", "din-02", "din-03",
"jog-incr-a","jog-incr-b","jog-incr-c",
"joint-select-a","joint-select-b","joint-select-c","joint-select-d",
"jog-x-pos","jog-x-neg","jog-y-pos","jog-y-neg",
"jog-z-pos","jog-z-neg","jog-a-pos","jog-a-neg",
"jog-selected-pos","jog-selected-neg","spindle-manual-cw",
"spindle-manual-ccw","spindle-manual-stop",
"spindle-at-speed"]

human_output_names = [ _("Unused Output"),
_("Spindle ON"),_("Spindle CW"), _("Spindle CCW"), _("Spindle Brake"),
_("Coolant Mist"), _("Coolant Flood"), _("ESTOP Out"), _("Amplifier Enable"),
_("Charge Pump"),
_("Digital out 0"), _("Digital out 1"), _("Digital out 2"), _("Digital out 3")]

human_input_names = [ _("Unused Input"), _("ESTOP In"), _("Probe In"),
_("X Home"), _("Y Home"), _("Z Home"), _("A Home"),
_("X Minimum Limit + Home"), _("Y Minimum Limit + Home"), _("Z Minimum Limit + Home"), _("A Minimum Limit + Home"),
_("X Maximum Limit + Home"), _("Y Maximum Limit + Home"), _("Z Maximum Limit + Home"), _("A Maximum Limit + Home"),
_("X Both Limit + Home"), _("Y Both Limit + Home"), _("Y Both Limit + Home"), _("A Both Limit + Home"),
_("X Minimum Limit"), _("Y Minimum Limit"), _("Z Minimum Limit"), _("A Minimum Limit"),
_("X Maximum Limit"), _("Y Maximum Limit"), _("Z Maximum Limit"), _("A Maximum Limit"),
_("X Both Limit"), _("Y Both Limit"), _("Z Both Limit"), _("A Both Limit"),
_("All Limits"), _("All Home"),
_("Digital in 0"), _("Digital in 1"), _("Digital in 2"), _("Digital in 3"),
_("Jog incr A"),_("Jog incr B"),_("Jog incr C"),
_("Joint select A"),_("Joint select B"),_("Joint select C"), _("Joint select D"),
_("Jog X +"),_("Jog X -"),_("Jog Y +"),_("Jog Y -"),_("Jog Z +"),_("Jog Z -"),
_("Jog A +"),_("Jog A -"),_("Jog button selected +"),_("Jog button selected -"),_("Manual Spindle CW"),
_("Manual Spindle CCW"),_("Manual Spindle Stop"),_("Spindle Up-To-Speed")]

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

human_pwm_output_names =[ _("Unused PWM Gen"),
_("X Axis PWM"), _("X Axis PWM"), _("X Axis PWM"),
_("Y Axis PWM"), _("Y AXIS PWM"), _("Y Axis PWM"),
_("Z Axis PWM"), _("Z Axis PWM"), _("Z Axis PWM"),
_("A Axis PWM"), _("A Axis PWM"), _("A Axis PWM"),
_("Spindle PWM"), _("Spindle PWM"), _("Spindle PWM"),  ]

(UNUSED_ENCODER, 
X_ENCODER_A, X_ENCODER_B, X_ENCODER_I, X_ENCODER_M,
Y_ENCODER_A, Y_ENCODER_B, Y_ENCODER_I, Y_ENCODER_M,
Z_ENCODER_A, Z_ENCODER_B, Z_ENCODER_I, Z_ENCODER_M, 
A_ENCODER_A, A_ENCODER_B, A_ENCODER_I, A_ENCODER_M, 
SPINDLE_ENCODER_A, SPINDLE_ENCODER_B, SPINDLE_ENCODER_I, SPINDLE_ENCODER_M,
X_MPG_A, X_MPG_B, X_MPG_I, X_MPG_M, Y_MPG_A, Y_MPG_B, Y_MPG_I, Y_MPG_M,
Z_MPG_A, Z_MPG_B, Z_MPG_I, Z_MPG_M, A_MPG_A, A_MPG_B, A_MPG_I,A_MPG_m,
SELECT_MPG_A, SELECT_MPG_B, SELECT_MPG_I, SELECT_MPG_M)  = hal_encoder_input_names = [ "unused-encoder",
"x-encoder-a", "x-encoder-b", "x-encoder-i", "x-encoder-m",
"y-encoder-a", "y-encoder-b", "y-encoder-i", "y-encoder-m",
"z-encoder-a", "z-encoder-b", "z-encoder-i", "z-encoder-m", 
"a-encoder-a", "a-encoder-b", "a-encoder-i", "a-encoder-m",
"s-encoder-a","s-encoder-b","s-encoder-i", "s-encoder-m",
"x-mpg-a","x-mpg-b", "x-mpg-i", "x-mpg-m", "y-mpg-a", "y-mpg-b", "y-mpg-i", "y-mpg-m",
"z-mpg-a","z-mpg-b", "z-mpg-i", "z-mpg-m", "a-mpg-a", "a-mpg-b", "a-mpg-i", "a-mpg-m",
"select-mpg-a", "select-mpg-b", "select-mpg-i", "select-mpg-m"]

human_encoder_input_names = [ _("Unused Encoder"),
_("X Encoder"), _("X Encoder"), _("X Encoder"), _("X Encoder"),
_("Y Encoder"), _("Y Encoder"), _("Y Encoder"), _("Y Encoder"), 
_("Z Encoder"), _("Z Encoder"), _("Z Encoder"), _("Z Encoder"),
_("A Encoder"), _("A Encoder"), _("A Encoder"), _("A Encoder"),
_("Spindle Encoder"), _("Spindle  Encoder"), _("Spindle Encoder"), _("Spindle Encoder"),
_("X Hand Wheel"), _("X Hand Wheel"), _("X Hand Wheel"), _("X Hand Wheel"),
_("Y Hand wheel"), _("Y Hand Wheel"), _("Y Hand Wheel"), _("Y Hand Wheel"),
_("Z Hand Wheel"), _("Z Hand Wheel"), _("Z Hand Wheel"), _("Z Hand Wheel"),
_("A Hand Wheel"), _("A Hand Wheel"), _("A Hand Wheel"), _("A Hand Wheel"),
_("Multi Hand Wheel"), _("Multi Hand Wheel"), _("Multi Hand Wheel"), _("Multi Hand Wheel")]

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
"s-stepgen-phase-f",]

human_stepper_names = [_("Unused StepGen"), _("X Axis StepGen"), _("X Axis StepGen"), _("X reserved c"), _("X reserved d"),
_("X reserved e"), _("X reserved f"), _("Y Axis StepGen"), _("Y Axis StepGen"), _("Y reserved c"), _("Y reserved d"), _("Y reserved e"),
_("Y reserved f"), _("Z Axis StepGen"), _("Z Axis StepGen"), _("Z reserved c"), _("Z reserved d"), _("Z reserved e"), _("Z reserved f"),
_("A Axis StepGen"), _("A Axis StepGen"), _("A reserved c"), _("A reserved d"), _("A reserved e"), _("A reserved f"),
_("Spindle StepGen"), _("Spindle StepGen"), _("Spindle reserved c"), _("Spindle reserved d"), _("Spindle reserved e"),
_("Spindle reserved f"), ]



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
        self.halpwmoutputsignames = []
        self.halinputsignames = []
        self.haloutputsignames = []
        self.halsteppersignames = []

        # pncconf default options
        self.createsymlink = 1
        self.createshortcut = 0  

        # basic machine data
        self.help = "help-welcome.txt"
        self.machinename = _("my_EMC_machine")
        self.frontend = 1 # AXIS
        self.axes = 0 # XYZ
        self.available_axes = []
        self.baseperiod = 50000
        self.servoperiod = 1000000
        self.units = 0 # inch
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
        self.userneededmux8 = 0
        self.userneededabs = 0
        self.userneededscale = 0


        # pyvcp data
        self.pyvcp = 0 # not included
        self.pyvcpname = "custom.xml"
        self.pyvcphaltype = 0 # no HAL connections specified
        self.pyvcpconnect = 1 # HAL connections allowed

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
        self.xmicrostep = 2
        self.xpulleydriver = 1
        self.xpulleydriven = 1
        self.xleadscrew = 5
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
        self.xencodercounts = 4000
        self.xscale = 0

        # axis y data
        self.ydrivertype = "custom"
        self.ysteprev = 200
        self.ymicrostep = 2
        self.ypulleydriver = 1
        self.ypulleydriven = 1
        self.yleadscrew = 5
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
        self.yencodercounts =4000
        self.yscale = 0
   
        # axis z data
        self.zdrivertype = "custom"     
        self.zsteprev = 200
        self.zmicrostep = 2
        self.zpulleydriver = 1
        self.zpulleydriven = 1
        self.zleadscrew = 5
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
        self.zencodercounts = 1000
        self.zscale = 0


        # axis a data
        self.adrivertype = "custom"
        self.asteprev = 200
        self.amicrostep = 2
        self.apulleydriver = 1
        self.apulleydriven = 1
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
        self.aencodercounts = 1000
        self.ascale = 0

        # axis s (spindle) data
        self.sdrivertype = "custom"
        self.ssteprev = 200
        self.smicrostep = 2
        self.spulleydriver = 1
        self.spulleydriven = 1
        self.sleadscrew = 5
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
        self.sencodercounts = 1000
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
        # strips endings off of custom signal name when put in
        # human names arrays 
        for i in  self.halencoderinputsignames:
            hal_encoder_input_names.append(i)
            for j in(["-a","-b","-i","-m"]):
                if i.endswith(j):
                    k = i.rstrip(j)
            human_encoder_input_names.append(k)
        for i in  self.halpwmoutputsignames:
            hal_pwm_output_names.append(i)
            for j in(["-pulse","-dir","-enable"]):
                if i.endswith(j):
                    k = i.rstrip(j)
            human_pwm_output_names.append(k)
        for i in  self.halinputsignames:
            hal_input_names.append(i)
            human_input_names.append(i)
        for i in  self.haloutputsignames:
            hal_output_names.append(i)
            human_output_names.append(i)
        for i in  self.halsteppersignames:
            hal_stepper_names.append(i)
            for j in(["-step","-dir","-c","-d","-e","-f"]):
                if i.endswith(j):
                    k = i.rstrip(j)
            human_stepper_names.append(k)


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
        if self.frontend == 1:
            print >>file, "DISPLAY = axis"
        elif self.frontend == 2:
            print >>file, "DISPLAY = tkemc"
        else:
            print >>file, "DISPLAY = mini"
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
        if self.units:
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
        print >>file, """# CONFIG0="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d" """ % (
                    self.mesa0_boardtitle, self.mesa0_firmware, self.mesa0_numof_encodergens, 
                    self.mesa0_numof_pwmgens, self.mesa0_numof_stepgens )
        if self.number_mesa == 2:
            print >>file, "# DRIVER1=%s" % self.mesa1_currentfirmwaredata[_HALDRIVER]
            print >>file, "# BOARD1=%s"% self.mesa1_currentfirmwaredata[_BOARDNAME]
            print >>file, """# CONFIG1="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d" """ % (
                     self.mesa1_boardtitle, self.mesa1_firmware, self.mesa1_numof_encodergens, 
                     self.mesa1_numof_pwmgens, self.mesa1_numof_stepgens )
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

    def hz(self, axname):
        steprev = getattr(self, axname+"steprev")
        microstep = getattr(self, axname+"microstep")
        pulleydriver = getattr(self, axname+"pulleydriver")
        pulleydriven = getattr(self, axname+"pulleydriven")
        leadscrew = getattr(self, axname+"leadscrew")
        maxvel = getattr(self, axname+"maxvel")
        if self.units or axname == 'a': leadscrew = 1./leadscrew
        pps = leadscrew * steprev * microstep * (pulleydriver/pulleydriven) * maxvel
        return abs(pps)

    def minperiod(self, steptime=None, stepspace=None, latency=None):
        if steptime is None: steptime = self.steptime
        if stepspace is None: stepspace = self.stepspace
        if latency is None: latency = self.latency
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
        pwmgen = self.pwmgen_sig(letter)
        stepgen = self.stepgen_sig(letter)
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
        if stepgen == "false":
            if (self.spidcontrol == True and letter == 's') or not letter == 's':
                print >>file, "P = %s" % get("P")
                print >>file, "I = %s" % get("I") 
                print >>file, "D = %s" % get("D")
                print >>file, "FF0 = %s" % get("FF0")
                print >>file, "FF1 = %s" % get("FF1")
                print >>file, "FF2 = %s" % get("FF2")
                print >>file, "BIAS = %s"% get("bias") 
                print >>file, "DEADBAND = %s"% get("deadband")
            if get("invertmotor"):
                temp = -1
            else: temp = 1
            print >>file, "OUTPUT_SCALE = %s" % (get("outputscale") * temp)
            print >>file, "OUTPUT_OFFSET = %s" % get("outputoffset")
            print >>file, "MAX_OUTPUT = %s" % (get("maxoutput") * temp)
            if get("invertencoder"):
                temp = -1
            else: temp = 1
            print >>file, "INPUT_SCALE = %s" % get("scale")
        else:
            print >>file, "# these are in nanoseconds"
            print >>file, "DIRSETUP   = %d"% int(get("dirsetup"))
            print >>file, "DIRHOLD    = %d"% int(get("dirhold"))
            print >>file, "STEPLEN    = %d"% int(get("steptime"))          
            print >>file, "STEPSPACE  = %d"% int(get("stepspace"))
            if get("invertmotor"):
                temp = -1
            else: temp = 1
            print >>file, "SCALE = %s"% (get("scale") * temp)
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
            if not self.findsignal(i) == "false": homes = True
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
                if not self.findsignal(i) == "false":
                    print >>file, "HOME_IGNORE_LIMITS = YES"
                    break
            if all_homes and not self.individual_homing:
                print >>file, "HOME_SEQUENCE = %s" % order[num]
        else:
            print >>file, "HOME_OFFSET = %s" % get("homepos")

    def home_sig(self, axis):
        thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis, "max-home-" + axis, "both-home-" + axis))
        for i in thisaxishome:
            if not self.findsignal(i) == "false": return i
        return "false"

    def min_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "min-" + axis,"min-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               if not self.findsignal(i) == "false": return i
           return "false"

    def max_lim_sig(self, axis):
           thisaxishome = set(("all-limit", "max-" + axis, "max-home-" + axis, "both-" + axis, "both-home-" + axis))
           for i in thisaxishome:
               if not self.findsignal(i) == "false": return i
           return "false"

    def stepgen_sig(self, axis):
           thisaxisstepgen =  axis + "-stepgen-step" 
           test = self.findsignal(thisaxisstepgen)
           if not test == "false": return test
           else:return "false"

    def encoder_sig(self, axis): 
           thisaxisencoder = axis +"-encoder-a"
           test = self.findsignal(thisaxisencoder)
           if not test == "false": return test
           else:return "false"

    def pwmgen_sig(self, axis):
           thisaxispwmgen =  axis + "-pwm-pulse" 
           test = self.findsignal( thisaxispwmgen)
           if not test == "false": return test
           else:return "false"

    def connect_axis(self, file, num, let):
        axnum = "xyzabcuvws".index(let)
        title = 'AXIS'
        if let == 's':
            title = 'SPINDLE'
        pwmgen = self.pwmgen_sig(let)
        stepgen = self.stepgen_sig(let)
        encoder = self.encoder_sig(let)
        lat = self.latency
        print >>file, "#*******************"
        print >>file, "#  %s %s" % (title, let.upper())
        print >>file, "#*******************"
        print >>file
         
        if not pwmgen == "false":
            if (self.spidcontrol == True and let == 's') or not let == 's':
                print >>file, "    setp pid.%s.Pgain     [%s_%d]P" % (let, title, axnum)
                print >>file, "    setp pid.%s.Igain     [%s_%d]I" % (let, title, axnum)
                print >>file, "    setp pid.%s.Dgain     [%s_%d]D" % (let, title, axnum)
                print >>file, "    setp pid.%s.bias      [%s_%d]BIAS" % (let, title, axnum)
                print >>file, "    setp pid.%s.FF0       [%s_%d]FF0" % (let, title, axnum)
                print >>file, "    setp pid.%s.FF1       [%s_%d]FF1" % (let, title, axnum)
                print >>file, "    setp pid.%s.FF2       [%s_%d]FF2" % (let, title, axnum)
                print >>file, "    setp pid.%s.deadband  [%s_%d]DEADBAND" % (let, title, axnum)
                print >>file, "    setp pid.%s.maxoutput [%s_%d]MAX_OUTPUT" % (let, title, axnum)
                if let == 's':
                    name = "spindle"
                else:
                    name = let
                print >>file, "net %s-index-enable  <=>  pid.%s.index-enable" % (name, let)
                print >>file
               
            if 'mesa' in pwmgen:
                pinname = self.make_pinname(pwmgen,ini_style)
                print >>file, "# PWM Generator signals/setup"
                print >>file
                print >>file, "    setp "+pinname+".output-type 1" 
                print >>file, "    setp "+pinname+".scale  [%s_%d]OUTPUT_SCALE"% (title, axnum)  
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
                        print >>file, "net spindle-output     pid.%s.output      => "% (let) + pinname + ".value"
                        print >>file, "net spindle-enable      => pid.%s.enable" % (let)
                        print >>file, "net spindle-enable      => " + pinname +".enable"
                        print >>file, "net spindle-vel-fb      => pid.%s.feedback"% (let)    
                    else:
                        print >>file, "net spindle-vel-cmd     => " + pinname + ".value"
                        print >>file, "net spindle-enable      => " + pinname +".enable"
                else:
                    print >>file, "net %senable     => pid.%s.enable" % (let, let)
                    print >>file, "net %soutput     pid.%s.output           => "% (let, let) + pinname + ".value"
                    print >>file, "net %spos-cmd    axis.%d.motor-pos-cmd   => pid.%s.command" % (let, axnum , let)
                    print >>file, "net %senable     axis.%d.amp-enable-out  => "% (let,axnum) + pinname +".enable"
                print >>file    
        if not stepgen == "false":
            pinname = self.make_pinname(stepgen,ini_style)
            print >>file, "# Step Gen signals/setup"
            print >>file
            print >>file, "    setp " + pinname + ".dirsetup        [%s_%d]DIRSETUP"% (title, axnum)
            print >>file, "    setp " + pinname + ".dirhold         [%s_%d]DIRHOLD"% (title, axnum)
            print >>file, "    setp " + pinname + ".steplen         [%s_%d]STEPLEN"% (title, axnum)
            print >>file, "    setp " + pinname + ".stepspace       [%s_%d]STEPSPACE"% (title, axnum)
            print >>file, "    setp " + pinname + ".position-scale  [%s_%d]SCALE"% (title, axnum)
            if let =="s":
                print >>file, "    setp " + pinname + ".maxaccel         [%s_%d]MAX_ACCELERATION"% (title, axnum)
                print >>file, "    setp " + pinname + ".maxvel           [%s_%d]MAX_VELOCITY"% (title, axnum)
            else:
                print >>file, "    setp " + pinname + ".maxaccel         0"
                print >>file, "    setp " + pinname + ".maxvel           0"
            print >>file, "    setp " + pinname + ".step_type        0"        
            if let == 's':  
                print >>file, "    setp " + pinname + ".control-type    1"
                print >>file
                print >>file, "net spindle-enable          =>  " + pinname + ".enable" 
                print >>file, "net spindle-vel-cmd-rps     =>  "+ pinname + ".velocity-cmd"
                if encoder == "false":
                    print >>file, "net spindle-vel-fb         <=  "+ pinname + ".velocity-fb"     
            else:
                print >>file
                print >>file, "net %spos-fb     axis.%d.motor-pos-fb   <=  "% (let, axnum) + pinname + ".position-fb"
                print >>file, "net %spos-cmd    axis.%d.motor-pos-cmd  =>  "% (let, axnum) + pinname + ".position-cmd"
                print >>file, "net %senable     axis.%d.amp-enable-out =>  "% (let, axnum) + pinname + ".enable"
            print >>file

        if 'mesa' in encoder:
                pinname = self.make_pinname(encoder,ini_style)              
                countmode = 0
                print >>file, "# ---Encoder feedback signals/setup---"
                print >>file             
                print >>file, "    setp "+pinname+".counter-mode %d"% countmode
                print >>file, "    setp "+pinname+".filter 1" 
                print >>file, "    setp "+pinname+".index-invert 0"
                print >>file, "    setp "+pinname+".index-mask 0" 
                print >>file, "    setp "+pinname+".index-mask-invert 0"              
                print >>file, "    setp "+pinname+".scale  [%s_%d]INPUT_SCALE"% (title, axnum)               
                if let == 's':
                    print >>file, "net spindle-revs              <=  " + pinname + ".position"
                    print >>file, "net spindle-vel-fb            <=  " + pinname + ".velocity"
                    print >>file, "net spindle-index-enable     <=>  " + pinname + ".index-enable" 
                    if self.findsignal("spindle-at-speed") == "false":
                        print >>file
                        print >>file, "# ---Setup spindle at speed signals---"
                        print >>file
                        if not stepgen =="false" or not encoder == "false":
                            print >>file, "net spindle-vel-cmd-rps    =>  near.0.in1"
                            print >>file, "net spindle-vel-fb         =>  near.0.in2"
                            print >>file, "net spindle-at-speed       <=  near.0.out"
                            print >>file, "    setp near.0.scale .9"
                        else:
                            print >>file, "    sets spindle-at-speed true"               
                else: 
                    print >>file, "net %spos-fb               <=  "% (let) + pinname+".position"
                    print >>file, "net %spos-fb               =>  pid.%s.feedback"% (let,let)
                    print >>file, "net %spos-fb               =>  axis.%d.motor-pos-fb" % (let, axnum)
                    print >>file, "net %s-index-enable    axis.%d.index-enable  <=>  "% (let, axnum) + pinname + ".index-enable"
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
            return
        
        min_limsig = self.min_lim_sig(let)
        if  min_limsig == "false": min_limsig = "%s-neg-limit" % let
        max_limsig = self.max_lim_sig(let)  
        if  max_limsig == "false": max_limsig = "%s-pos-limit" % let 
        homesig = self.home_sig(let)
        if homesig == "false": homesig = "%s-home-sw" % let
        print >>file, "# ---setup home / limit switch signals---"       
        print >>file       
        print >>file, "net %s     =>  axis.%d.home-sw-in" % (homesig, axnum)       
        print >>file, "net %s     =>  axis.%d.neg-lim-sw-in" % (min_limsig, axnum)       
        print >>file, "net %s     =>  axis.%d.pos-lim-sw-in" % (max_limsig, axnum)
        print >>file                

    def connect_input(self, file):
        print >>file, "# external input signals"
        print >>file
        for q in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = self['pp1Ipin%d' % q]
            i = self['pp1Ipin%dinv' % q]
            if p == UNUSED_INPUT: continue
            if i: print >>file, "net %s     <= parport.0.pin-%02d-in-not" % (p, q)
            else: print >>file, "net %s     <= parport.0.pin-%02d-in" % (p, q)
        print >>file
        for boardnum in range(0,int(self.number_mesa)):
            for concount,connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                for q in range(0,24):
                    p = self['mesa%dc%dpin%d' % (boardnum,connector, q)]
                    i = self['mesa%dc%dpin%dinv' % (boardnum,connector, q)]
                    t = self['mesa%dc%dpin%dtype' % (boardnum,connector, q)]
                    truepinnum = q + ((connector-2)*24)
                    # for input pins
                    if t == GPIOI:
                        if p == "unused-input":continue 
                        pinname = self.make_pinname(self.findsignal( p ),ini_style) 
                        print >>file, "# ---",p.upper(),"---"
                        if i: print >>file, "net %s     <=  "% (p)+pinname +".in_not"
                        else: print >>file, "net %s     <=  "% (p)+pinname +".in"
                    # for encoder pins
                    elif t in (ENCA):
                        if p == "unused-encoder":continue
                        if p in (self.halencoderinputsignames): 
                            pinname = self.make_pinname(self.findsignal( p ),ini_style) 
                            sig = p.rstrip("-a")
                            print >>file, "# ---",sig.upper(),"---"
                            print >>file, "net %s         <=  "% (sig+"-position")+pinname +".position"   
                            print >>file, "net %s            <=  "% (sig+"-count")+pinname +".count"     
                            print >>file, "net %s         <=  "% (sig+"-velocity")+pinname +".velocity"
                            print >>file, "net %s            <=  "% (sig+"-reset")+pinname +".reset"      
                            print >>file, "net %s     <=  "% (sig+"-index-enable")+pinname +".index-enable"      
                    else: continue

    def connect_output(self, file):
        
        print >>file, "# external output signals"
        print >>file
        for q in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = self['pp1Opin%d' % q]
            i = self['pp1Opin%dinv' % q]
            if p == UNUSED_OUTPUT: continue
            print >>file, "net %s     =>  parport.0.pin-%02d-out" % (p, q)
            if i: print >>file, "    setp parport.0.pin-%02d-out-invert true" % q           
        print >>file
        for boardnum in range(0,int(self.number_mesa)):
            for concount,connector in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                for q in range(0,24):
                    p = self['mesa%dc%dpin%d' % (boardnum,connector, q)]
                    i = self['mesa%dc%dpin%dinv' % (boardnum,connector, q)]
                    t = self['mesa%dc%dpin%dtype' % (boardnum,connector, q)]
                    truepinnum = q + ((connector-2)*24)
                    # for output /open drain pins
                    if t in (GPIOO,GPIOD):
                        if p == "unused-output":continue
                        pinname = self.make_pinname(self.findsignal( p ),ini_style)
                        print >>file, "# ---",p.upper(),"---"
                        print >>file, "    setp "+pinname +".is_output true"
                        if i: print >>file, "    setp "+pinname+".invert_output true"
                        if t == 2: print >>file, "    setp "+pinname+".is_opendrain  true"   
                        print >>file, "net %s     =>  "% (p)+pinname +".out"              
                    # for pwm pins
                    elif t in (PWMP,PDMP):
                        if p == "unused-pwm":continue
                        if p in (self.halpwmoutputsignames): 
                            pinname = self.make_pinname(self.findsignal( p ),ini_style) 
                            sig = p.rstrip("-pulse")
                            print >>file, "# ---",sig.upper(),"---"
                            if t == PWMP:
                                print >>file, "    setp "+pinname +".output-type 1"
                            elif t == PDMP:
                                print >>file, "    setp "+pinname +".output-type 3"
                            print >>file, "net %s     <=  "% (sig+"-enable")+pinname +".enable"  
                            print >>file, "net %s      <=  "% (sig+"-value")+pinname +".value" 
                    # for stepper pins
                    elif t == (STEPA):
                        if p == "unused-stepgen":continue
                        if p in (self.halsteppersignames): 
                            pinname = self.make_pinname(self.findsignal( p ),ini_style) 
                            sig = p.rstrip("-step")
                            print >>file, "# ---",sig.upper(),"---"
                            print >>file, "net %s           <=  "% (sig+"-enable")+pinname +".enable"  
                            print >>file, "net %s            <=  "% (sig+"-count")+pinname +".counts" 
                            print >>file, "net %s     <=  "% (sig+"-cmd-position")+pinname +".position-cmd"  
                            print >>file, "net %s     <=  "% (sig+"-act-position")+pinname +".position-fb" 
                            print >>file, "net %s         <=  "% (sig+"-velocity")+pinname +".velocity-fb"
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
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d" """ % (
                    driver0, directory0, firm0, self.mesa0_numof_encodergens, self.mesa0_numof_pwmgens, self.mesa0_numof_stepgens )
        elif self.number_mesa == 2 and (driver0 == driver1):
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d,firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"
                    """ % (
                    driver0, directory0, firm0, self.mesa0_numof_encodergens, self.mesa0_numof_pwmgens, self.mesa0_numof_stepgens,
                    directory1, firm1, self.mesa1_numof_encodergens, self.mesa1_numof_pwmgens, self.mesa1_numof_stepgens )
        elif self.number_mesa == 2:
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d" """ % (
                    driver0, directory0, firm0, self.mesa0_numof_encodergens, self.mesa0_numof_pwmgens, self.mesa0_numof_stepgens )
            print >>file, """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d" """ % (
                    driver1, directory1, firm1, self.mesa1_numof_encodergens, self.mesa1_numof_pwmgens, self.mesa1_numof_stepgens )
        for boardnum in range(0,int(self.number_mesa)):
            if boardnum == 1 and (board0 == board1):
                halnum = 1
            else:
                halnum = 0
            if self["mesa%d_numof_pwmgens"% boardnum] > 0:
                print >>file, "    setp hm2_%s.%d.pwmgen.pwm_frequency %d"% ( self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum, self["mesa%d_pwm_frequency"% boardnum] )
                print >>file, "    setp hm2_%s.%d.pwmgen.pdm_frequency %d"% ( self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum,self["mesa%d_pdm_frequency"% boardnum] )
            print >>file, "    setp hm2_%s.%d.watchdog.timeout_ns %d"% ( self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME], halnum,self["mesa%d_watchdog_timeout"% boardnum] )

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

        if not self.findsignal("s-encoder-a") == "false":
            spindle_enc = True        
        if not self.findsignal("probe") =="false":
            probe = True
        if not self.findsignal("s-pwm-pulse") =="false":
            pwm = True
        if not self.findsignal("charge-pump") =="false":
            pump = True
        if not self.findsignal("estop-ext") =="false":
            estop = True
        if not self.findsignal("enable") =="false":
            enable = True
        if not self.findsignal("spindle-enable") =="false":
            spindle_on = True
        if not self.findsignal("spindle-cw") =="false":
            spindle_cw = True
        if not self.findsignal("spindle-ccw") =="false":
            spindle_ccw = True
        if not self.findsignal("coolant-mist") =="false":
            mist = True
        if not self.findsignal("coolant-flood") =="false":
            flood = True
        if not self.findsignal("spindle-brake") =="false":
            brake = True
        if not self.findsignal("spindle-at-speed") =="false":
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
        
        if self.externalmpg or self.joystickjog or self.userneededmux8 > 0:
            self.mux8names=""
            if self.joystickjog: 
                self.mux8names = self.mux8names+"mux8.jogspeed"
                if self.userneededmux8 > 0 or self.externalmpg:
                    self.mux8names = self.mux8names+","
            if self.externalmpg: 
                self.mux8names = self.mux8names+"mux8.jogincr"
                if self.userneededmux8 > 0:
                    self.mux8names = self.mux8names+","
            for i in range(0,self.userneededmux8):
                self.mux8names = self.mux8names+"mux8.%d"% (i)
                if i <> self.userneededmux8-1:
                    self.mux8names = self.mux8names+","
            print >>file, "loadrt mux8 names=%s"% (self.mux8names)
        # load user custom components
        for i in self.loadcompbase:
            if i == '': continue
            else:              
                print >>file, i 
        for i in self.loadcompservo:
            if i == '': continue
            else:              
                print >>file, i 

        if self.pyvcp and not self.frontend == 1:
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
            if self.findsignal(i+"-encoder-a") == "false": 
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
        if self.externalmpg or self.joystickjog or self.userneededmux8 > 0: 
            temp=self.mux8names.split(",")
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
            print >>file, "     sets jog-speed %f"% self.jograpidrate
            if self.multijogbuttons:
                for axnum,axletter in enumerate(self.available_axes):
                    if not axletter == "s":
                        print >>file, "net jog-%s-pos            halui.jog.%d.plus"% (axletter,axnum)
                        print >>file, "net jog-%s-neg            halui.jog.%d.minus"% (axletter,axnum)
            else:
                for axnum,axletter in enumerate(self.available_axes):
                    if not axletter == "s":
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
            print >>file, "net jog-speed-a           =>  mux8.jogspeed.sel0"
            print >>file, "net jog-speed-b           =>  mux8.jogspeed.sel1"
            print >>file, "net jog-speed             halui.jog-speed  <=  mux8.jogspeed.out"
            print >>file, "    setp mux8.jogspeed.in0          %f"% (self.joystickjograpidrate0)
            print >>file, "    setp mux8.jogspeed.in1          %f"% (self.joystickjograpidrate1)
            print >>file, "    setp mux8.jogspeed.in2          %f"% (self.joystickjograpidrate2)
            print >>file, "    setp mux8.jogspeed.in3          %f"% (self.joystickjograpidrate3)
            if not self.joycmdrapida =="":
                print >>file, "net jog-speed-a           <=  %s"% (self.joycmdrapida)
            if not self.joycmdrapidb =="":
                print >>file, "net jog-speed-b           <=  %s"% (self.joycmdrapidb)
            for axnum,axletter in enumerate(self.available_axes):
                if not axletter == "s":
                    pin_pos = self["joycmd"+axletter+"pos"]
                    pin_neg = self["joycmd"+axletter+"neg"]
                    if pin_pos == "" or pin_neg =="": continue
                    print >>file, "net jog-%s-pos            halui.jog.%d.plus"% (axletter,axnum)
                    print >>file, "net jog-%s-pos            %s"% (axletter,pin_pos)
                    print >>file, "net jog-%s-neg            halui.jog.%d.minus"% (axletter,axnum)
                    print >>file, "net jog-%s-neg            %s"% (axletter,pin_neg)
            print >>file

        if self.externalmpg:
            print >>file, _("#  ---mpg signals---")
            print >>file
            if self.multimpg:  
                for axnum,axletter in enumerate(self.available_axes):
                    if not axletter == "s":
                        pinname = self.make_pinname(self.findsignal(axletter+"-mpg-a"),ini_style)
                        print pinname
                        if 'hm2' in pinname:      
                            print >>file, "# connect jogwheel signals to mesa encoder - %s axis MPG "% axletter       
                            print >>file, "    setp  axis.%d.jog-vel-mode 0" % axnum
                            print >>file, "    setp  axis.%d.jog-enable true"% (axnum)
                            print >>file, "    setp  %s.filter true" % pinname
                            print >>file, "    setp  %s.counter-mode true" % pinname
                            print >>file, "net %s-jog-count          <=  %s.count"% (axletter, pinname)                  
                            print >>file, "net %s-jog-count          =>  axis.%d.jog-counts" % (axletter,axnum)
                            print >>file, "net selected-jog-incr    =>  axis.%d.jog-scale" % (axnum)
                            print >>file                    
            else:
                 pinname = self.make_pinname(self.findsignal("select-mpg-a"),ini_style)
                 if 'hm2' in pinname:      
                    print >>file, "# connect jogwheel signals to mesa encoder - shared MPG " 
                    print >>file, "net joint-selected-count     <=  %s.count"% (pinname)      
                    print >>file, "    setp %s.filter true" % pinname
                    print >>file, "    setp %s.counter-mode true" % pinname                
                    for axnum,axletter in enumerate(self.available_axes):
                        if not axletter == "s":
                            print >>file, "#       for axis %s MPG" % (axletter)
                            print >>file, "    setp  axis.%d.jog-vel-mode 0" % axnum
                            print >>file, "net selected-jog-incr    =>  axis.%d.jog-scale" % (axnum)
                            print >>file, "net joint-select-%s       =>  axis.%d.jog-enable"% (chr(axnum+97),axnum)
                            print >>file, "net joint-selected-count =>  axis.%d.jog-counts"% (axnum)
                    print >>file
            if self.incrselect:
                print >>file, "# connect selectable mpg jog increments "  
                print >>file, "net jog-incr-a           =>  mux8.jogincr.sel0"
                print >>file, "net jog-incr-b           =>  mux8.jogincr.sel1"
                print >>file, "net jog-incr-c           =>  mux8.jogincr.sel2"
                print >>file, "net selected-jog-incr    <=  mux8.jogincr.out"
                print >>file, "    setp mux8.jogincr.in0          %f"% (self.mpgincrvalue0)
                print >>file, "    setp mux8.jogincr.in1          %f"% (self.mpgincrvalue1)
                print >>file, "    setp mux8.jogincr.in2          %f"% (self.mpgincrvalue2)
                print >>file, "    setp mux8.jogincr.in3          %f"% (self.mpgincrvalue3)
                print >>file, "    setp mux8.jogincr.in4          %f"% (self.mpgincrvalue4)
                print >>file, "    setp mux8.jogincr.in5          %f"% (self.mpgincrvalue5)
                print >>file, "    setp mux8.jogincr.in6          %f"% (self.mpgincrvalue6)
                print >>file, "    setp mux8.jogincr.in7          %f"% (self.mpgincrvalue7)
                print >>file
            else:
                print >>file, "net selected-jog-incr    <= %f"% (self.mpgincrvalue0)

        print >>file, _("#  ---digital in / out signals---")
        print >>file
        for i in range(4):
            dout = "dout-%02d" % i
            if not self.findsignal(dout) =="false":
                print >>file, "net %s     <=  motion.digital-out-%02d" % (dout, i)
        for i in range(4):
            din = "din-%02d" % i
            if not self.findsignal(din) =="false":
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
                      print >>f1, _("# **** spindle-velocity is signed so we use absolute compoent to remove sign") 
                      print >>f1, _("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
                      print >>f1
                      print >>f1
                      print >>f1, ("    setp scale.spindle.gain .01667")
                      print >>f1, ("net spindle-velocity-fb  => abs.spindle.in")
                      print >>f1, ("net absolute-spindle-vel <= abs.spindle.out => scale.spindle.in")
                      print >>f1, ("net scaled-spindle-vel <= scale.spindle.out => pyvcp.spindle-speed")
                  else:
                      print >>f1, _("# **** Use COMMANDED spindle velocity from EMC because no spindle encoder was specified")
                      print >>f1, _("# **** COMMANDED velocity is signed so we use absolute component (abs.0) to remove sign")
                      print >>f1
                      print >>f1, ("net spindle-vel-cmd                       =>  abs.spindle.in")
                      print >>f1, ("net absolute-spindle-vel    abs.spindle.out =>  pyvcp.spindle-speed")                     
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
        if  self.units == 0: unit = "an imperial"
        else: unit = "a metric"
        if self.frontend == 1: display = "AXIS"
        elif self.frontend == 2: display = "Tkemc"
        elif self.frontend == 3: display = "Mini"
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
            print >>file,"Comment=" + _("Desktop Launcher for EMC config made by PNCconf")
            print >>file,"Icon=/etc/emc2/emc2icon.png"
            file.close()

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
        except :   
            if self.number_pports:             
                try:
                    return ppinput[sig]
                except :
                    try: 
                        return ppoutput[sig]
                    except :
                        return "false"    
            else: return "false"            

    # This method takes a signalname data pin (eg mesa0c3pin1)
    # and converts it to a HAL pin names (eg hm2_5i20.0.gpio.01)
    # component number conversion is for adjustment of position of pins related to the
    # 'controlling pin' eg encoder-a (controlling pin) encoder-b encoder -I
    # (a,b,i are related pins for encoder component) 
    def make_pinname(self, pin, ini_style = False):
        test = str(pin)  
        halboardnum = 0     
        if 'mesa' in test:
            boardnum = int(test[4:5])
            if ini_style:
                boardname = "[hosmot2](board%d)"% boardnum                
            else:
                boardname = self["mesa%d_currentfirmwaredata"% boardnum][_BOARDNAME]
            if boardnum == 1 and self.mesa1_currentfirmwaredata[_BOARDNAME] == self.mesa0_currentfirmwaredata[_BOARDNAME]:
                halboardnum = 1
            ptype = self[pin+"type"] 
            signalname = self[pin]
            pinnum = int(test[10:])
            connum = int(test[6:7])

            for concount,i in enumerate(self["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]):
                if i == connum:
                    dummy,compnum = self["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pinnum+(concount*24)]
                    break
            type_name = { GPIOI:"gpio", GPIOO:"gpio", GPIOD:"gpio", ENCA:"encoder", ENCB:"encoder",ENCI:"encoder",ENCM:"encoder", 
                PWMP:"pwmgen",PWMD:"pwmgen", PWME:"pwmgen", PDMP:"pwmgen", PDMD:"pwmgen", PDME:"pwmgen",STEPA:"stepgen", STEPB:"stepgen" }

            # we iter over this dic because of locale translation problems when using
            # comptype = type_name[ptype]
            comptype = "ERROR FINDING COMPONENT TYPE"
            for key,value in type_name.iteritems():
                if key == ptype: comptype = value
            if ptype in(GPIOI,GPIOO,GPIOD):
                compnum = int(pinnum)+(concount*24)
                return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%03d"% (compnum)          
            elif ptype in (ENCA,ENCB,ENCI,ENCM,PWMP,PWMD,PWME,PDMP,PDMD,PDME,STEPA,STEPB,STEPC,STEPD,STEPE,STEPF):
                return "hm2_%s.%d."% (boardname,halboardnum) + comptype+".%02d"% (compnum)
            else: 
                print "pintype error"
                return 
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
        else: return "false"

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
        self.widgets.latency.set_value(self.data.latency)
        self.widgets.baseperiod.set_value(self.data.baseperiod)
        self.widgets.servoperiod.set_value(self.data.servoperiod)
        self.widgets.machinename.grab_focus()
        if self.data.number_mesa:
            self.widgets.mesa5i20_checkbutton.set_active(True)
        else:
            self.widgets.mesa5i20_checkbutton.set_active(False)
        self.widgets.number_mesa.set_value(self.data.number_mesa)
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
        
    def on_mesa5i20_checkbutton_toggled(self, *args): 
        i = self.widgets.mesa5i20_checkbutton.get_active()   
        self.widgets.number_mesa.set_sensitive(i)
        
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
        self.data.latency = self.widgets.latency.get_value()
        self.data.baseperiod = self.widgets.baseperiod.get_value()
        self.data.servoperiod = self.widgets.servoperiod.get_value()
        self.data.ioaddr = self.widgets.ioaddr.get_text()
        self.data.ioaddr2 = self.widgets.ioaddr2.get_text()
        self.data.ioaddr3 = self.widgets.ioaddr3.get_text()
        if self.widgets.mesa5i20_checkbutton.get_active():
            self.data.number_mesa = self.widgets.number_mesa.get_value()
        else:
            self.data.number_mesa = 0
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
        
        # connect signals with pin designation data to mesa signal comboboxes and pintype comboboxes
        # record the signal ID numbers so we can block the signals later in the mesa routines
        # have to do it here manually (instead of autoconnect) because glade doesn't handle added
        # user info (board/connector/pin number designations) and doesn't record the signal ID numbers
        # none of this is done if mesa is not checked off in pncconf

        if (self.data.number_mesa): 
            for boardnum in (0,1):
                cb = "mesa%d_comp_update"% (boardnum)
                i = "mesa%dsignalhandler_comp_update"% (boardnum)
                self.intrnldata[i] = int(self.widgets[cb].connect("clicked", self.on_mesa_component_value_changed,boardnum))
                cb = "mesa%d_boardtitle"% (boardnum)
                i = "mesa%dsignalhandler_boardname_change"% (boardnum)
                self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_boardname_changed,boardnum))
                cb = "mesa%d_firmware"% (boardnum)
                i = "mesa%dsignalhandler_firmware_change"% (boardnum)
                self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_firmware_changed,boardnum))
                for connector in (2,3,4,5,6,7,8,9):
                    for pin in range(0,24):
                      cb = "mesa%dc%ipin%i"% (boardnum,connector,pin)
                      i = "mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)
                      self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_pin_changed,boardnum,connector,pin,False))
                      i = "mesa%dactivatehandlerc%ipin%i"% (boardnum,connector,pin)
                      self.intrnldata[i] = int(self.widgets[cb].child.connect("activate", self.on_mesa_pin_changed,boardnum,connector,pin,True))
                      cb = "mesa%dc%ipin%itype"% (boardnum,connector,pin)
                      i = "mesa%dptypesignalhandlerc%ipin%i"% (boardnum,connector,pin)
                      self.intrnldata[i] = int(self.widgets[cb].connect("changed", self.on_mesa_pintype_changed,boardnum,connector,pin))

            # here we initalise the mesa configure page data
            for boardnum in(0,1):
                model = self.widgets["mesa%d_boardtitle"% boardnum].get_model()
                model.clear()
                for i in mesaboardnames:
                    model.append((i,))      
                for search,item in enumerate(mesaboardnames):
                    if mesaboardnames[search]  == self.data["mesa%d_boardtitle"% boardnum]:
                        self.widgets["mesa%d_boardtitle"% boardnum].set_active(search)  
                model = self.widgets["mesa%d_firmware"% boardnum].get_model()
                model.clear()
                for search, item in enumerate(mesafirmwaredata):
                    d = mesafirmwaredata[search]
                    if not d[_BOARDTITLE] == self.data["mesa%d_boardtitle"% boardnum]:continue
                    model.append((d[_FIRMWARE],))        
                for search,item in enumerate(model):           
                    if model[search][0]  == self.data["mesa%d_firmware"% boardnum]:
                        self.widgets["mesa%d_firmware"% boardnum].set_active(search)   
    
                self.widgets["mesa%d_pwm_frequency"% boardnum].set_value(self.data["mesa%d_pwm_frequency"% boardnum])
                self.widgets["mesa%d_pdm_frequency"% boardnum].set_value(self.data["mesa%d_pdm_frequency"% boardnum])
                self.widgets["mesa%d_watchdog_timeout"% boardnum].set_value(self.data["mesa%d_watchdog_timeout"% boardnum])
                self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(self.data["mesa%d_numof_encodergens"% boardnum])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(self.data["mesa%d_numof_pwmgens"% boardnum])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(self.data["mesa%d_numof_stepgens"% boardnum])
                self.widgets["mesa%d_numof_gpio"% boardnum].set_text("%d" % self.data["mesa%d_numof_gpio"% boardnum])          

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
        self.widgets.jograpidrate.set_value(self.data.jograpidrate)
        self.widgets.singlejogbuttons.set_active(self.data.singlejogbuttons)
        self.widgets.multijogbuttons.set_active(self.data.multijogbuttons)
        self.widgets.externalmpg.set_active(self.data.externalmpg)
        self.widgets.externaljog.set_active(self.data.externaljog)
        self.widgets.sharedmpg.set_active(self.data.sharedmpg)
        self.widgets.multimpg.set_active(self.data.multimpg)
        self.widgets.incrselect.set_active(self.data.incrselect)
        if self.data.units == 0 :
            tempunits = "in"
        else:
            tempunits = "mm"      
        for i in range(0,8):
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
        self.widgets.joystickjogbox.set_sensitive(self.widgets.joystickjog.get_active())
        i =  self.widgets.incrselect.get_active()
        for j in range(1,8):
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
        if self.widgets.multimpg.get_active():
            self.data.multimpg == True            
        else:
            self.data.multimpg == False
        self.data.jograpidrate = self.widgets.jograpidrate.get_value()
        self.data.singlejogbuttons = self.widgets.singlejogbuttons.get_active()
        self.data.multijogbuttons = self.widgets.multijogbuttons.get_active()
        self.data.externalmpg = self.widgets.externalmpg.get_active()
        self.data.externaljog = self.widgets.externaljog.get_active()
        self.data.sharedmpg = self.widgets.sharedmpg.get_active()
        self.data.multimpg = self.widgets.multimpg.get_active()
        self.data.incrselect = self.widgets.incrselect.get_active()
        for i in range (0,8):
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
        if self.data.frontend == 1 : self.widgets.GUIAXIS.set_active(True)
        elif self.data.frontend == 2: self.widgets.GUITKEMC.set_active(True)
        else:   self.widgets.GUIMINI.set_active(True)
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
        if self.data.units == 0 :
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
        if self.widgets.GUIAXIS.get_active():
           self.data.frontend = 1
        elif self.widgets.GUITKEMC.get_active():
           self.data.frontend = 2
        else:
            self.data.frontend = 3
        self.data.default_linear_velocity = self.widgets.default_linear_velocity.get_value()/60
        self.data.max_linear_velocity = self.widgets.max_linear_velocity.get_value()/60
        self.data.min_linear_velocity = self.widgets.min_linear_velocity.get_value()/60
        self.data.default_angular_velocity = self.widgets.default_angular_velocity.get_value()/60
        self.data.max_angular_velocity = self.widgets.max_angular_velocity.get_value()/60
        self.data.min_angular_velocity = self.widgets.min_angular_velocity.get_value()/60
        self.data.editor = self.widgets.editor.get_text()
        if self.data.units == 0 :self.data.increments_imperial = self.widgets.increments.get_text()
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
        title = self.widgets["mesa%d_boardtitle"%boardnum].get_active_text()
        model = self.widgets["mesa%d_firmware"% boardnum].get_model()
        model.clear()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[_BOARDTITLE] == title:continue
            model.append((d[_FIRMWARE],))       
        self.widgets["mesa%d_firmware"% boardnum].set_active(0)
        if  d[_BOARDNAME] =="7i43":
            self.widgets["mesa%d_parportaddrs"% boardnum].set_sensitive(1)
        else:
            self.widgets["mesa%d_parportaddrs"% boardnum].set_sensitive(0)
        self.on_mesa_firmware_changed(self,boardnum)

    def on_mesa_firmware_changed(self, widget,boardnum):
        title = self.widgets["mesa%d_boardtitle"% boardnum].get_active_text()
        firmware = self.widgets["mesa%d_firmware"% boardnum].get_active_text()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[_BOARDTITLE] == title:continue
            if d[_FIRMWARE] == firmware:
                self.widgets["mesa%d_numof_encodergens"%boardnum].set_range(0,d[_MAXENC])
                self.widgets["mesa%d_numof_encodergens"% boardnum].set_value(d[_MAXENC])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_range(0,d[_MAXPWM])
                self.widgets["mesa%d_numof_pwmgens"% boardnum].set_value(d[_MAXPWM])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_range(0,d[_MAXSTEP])
                self.widgets["mesa%d_numof_stepgens"% boardnum].set_value(d[_MAXSTEP])
                self.widgets["mesa%d_totalpins"% boardnum].set_text("%s"% d[_MAXGPIO])
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
                    total = (d[_MAXGPIO]-i-j-k)
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
                foundit = 0
                p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                pinv = 'mesa%dc%dpin%dinv' % (boardnum,connector,pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                pintype = self.widgets[ptype].get_active_text()
                selection = self.widgets[p].get_active_text()
                if pintype in (ENCB,ENCI,ENCM,PDMD,PDME,PWMD,PWME,STEPB): continue
                # type GPIO input
                if pintype == GPIOI:
                    nametocheck = human_input_names
                    signaltocheck = hal_input_names
                    addsignalto = self.data.halinputsignames
                # type gpio output and open drain
                elif pintype in (GPIOO,GPIOD):
                    nametocheck = human_output_names
                    signaltocheck = hal_output_names
                    addsignalto = self.data.haloutputsignames
                #type encoder
                elif pintype == ENCA:
                    nametocheck = human_encoder_input_names
                    signaltocheck = hal_encoder_input_names
                    addsignalto = self.data.halencoderinputsignames
                    relatedsignals =["DUMMY",ENCB,ENCI,ENCM]
                    relatedending = ["-a","-b","-i","-m"]
                    addedending = "-a"
                    unusedname = "Unused Encoder"
                # type PWM gen
                elif pintype in( PDMP):
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    relatedsignals =["DUMMY",PDMD,PDME]
                    relatedending = ["-pulse","-dir","-enable"]
                    addedending = "-pulse"
                    unusedname = "Unused PWM Gen"
                elif pintype in( PWMP):
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    relatedsignals =["DUMMY",PWMD,PWME]
                    relatedending = ["-pulse","-dir","-enable"]
                    addedending = "-pulse"
                    unusedname = "Unused PWM Gen"
                # type step gen
                elif pintype == STEPA:
                    nametocheck = human_stepper_names
                    signaltocheck = hal_stepper_names
                    addsignalto = self.data.halsteppersignames
                    relatedsignals =["DUMMY",STEPB,STEPC,STEPD,STEPE,STEPF]
                    relatedending = ["-step","-dir","c","d","e","f"]
                    addedending = "-a"
                    unusedname = "Unused StepGen"
                else :
                    print "error unknown pin type"
                    return
                # check apropriote signal array for current signalname
                # if not found, user made a new signalname -add it to array
                for index , i in enumerate(nametocheck):
                    if selection == i: 
                        foundit = True
                        #print "found it",nametocheck[index],"in ",p,"\n"
                        break
                
                # **Start widget to data Convertion**                    
                # for encoder pins
                if pintype not in(GPIOI,GPIOO,GPIOD) :
                    if not foundit:
                       # print "callin pin changed !!!"
                        self.on_mesa_pin_changed(p,boardnum,connector,pin,True)  
                       # print "back !!!"
                        for index , i in enumerate(nametocheck):
                            selection = self.widgets[p].get_active_text()
                        #    print "looking for signame -> ",selection," ",i,index
                            if selection == i : 
                                foundit = True
                         #       print "found it",nametocheck[index],"in ",p,"at index ",index,"\n"
                                break
            
                        
                    # set related encoder pins
                    # searches the current firmware data array to find were the relate pins are
                    # adds the widget signalname to the data unless the signal is Unused Encoder
                    # then just adds that 
                    flag = 1
                    if selection == unusedname:flag = 0
                    currentfirm,currentcompnum = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pin+(concount*24)]
                    # print "current firm type, number-",currentfirm,currentcompnum
                    for t_concount,t_connector in enumerate(self.data["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                        for t_pin in range (0,24):
                            comptype,compnum = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+t_pin+(t_concount*24)]
                            if compnum != currentcompnum: continue                             
                            if comptype not in (relatedsignals): continue
                           # print "checking-",comptype, compnum 
                            for offset,i in enumerate(relatedsignals):
                                if i == comptype:
                                    d = 'mesa%dc%dpin%d' % (boardnum,t_connector,t_pin)  
                                  #  print "index",index,"offset",offset                                 
                                    self.data[d] = signaltocheck[(index+offset)*flag]
                                  #  print d," <- ", self.data[d]
                              
                # for input and output
                elif pintype in(GPIOI,GPIOO,GPIOD):
                    if not foundit:
                        model = self.widgets[p].get_model()
                        index = index +1
                        model.append((selection,))
                        signaltocheck.append ((selection))
                        nametocheck.append ((selection))
                        addsignalto.append ((selection))
                else:
                        print "pintype error pintype =",pintype
                #  set data from widget for current pin
                self.data[p] = signaltocheck[index]
                self.data[pinv] = self.widgets[pinv].get_active()
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
                    self.data.mesa0_numof_stepgens,self.data.mesa0_numof_encodergens)
        elif not self.intrnldata.mesa0_configured:
            self.widgets.mesa0con2table.hide()
            self.widgets.mesa0con3table.hide()   
            self.widgets.mesa0con4table.hide()
            self.widgets.mesa0con5table.hide()           
        self.widgets.mesa0_parportaddrs.set_text(self.data.mesa0_parportaddrs)
            
    def on_mesa0_next(self,*args):
        if not self.intrnldata.mesa0_configured:
            self.warning_dialog(_("You need to configure the mesa page.\n Choose the board type, firmware, component amounts and press 'Accept component changes' button'"),True)
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
                    self.data.mesa1_numof_stepgens,self.data.mesa1_numof_encodergens)
        elif not self.intrnldata.mesa1_configured:           
            self.widgets.mesa1con2table.hide()
            self.widgets.mesa1con3table.hide()           
            self.widgets.mesa1con4table.hide()
            self.widgets.mesa1con5table.hide()
        self.widgets.mesa1_parportaddrs.set_text(self.data.mesa1_parportaddrs)

    def on_mesa1_next(self,*args):
        if not self.intrnldata.mesa1_configured:
            self.warning_dialog(_("You need to configure the mesa page.\n Choose the board type, firmware, component amounts and press 'Accept component changes' button'"),True)
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
                        if pin == 3 :encpinnum = (connector-2)*4 
                        elif pin == 1 :encpinnum = 1+((connector-2)*4) 
                        elif pin == 15 :encpinnum = 2+((connector-2)*4) 
                        elif pin == 13 :encpinnum = 3+((connector-2)*4) 
                       
                        halrun.write("net b%d_enc_reset%d hm2_%s.encoder.%02d.reset testpanel.brd.%d.enc.%d.reset\
                                    \n"% (boardnum,encpinnum,board,encpinnum,boardnum,encpinnum))
                        halrun.write("net b%d_enc_count%d hm2_%s.encoder.%02d.count testpanel.brd.%d.enc.%d.count\
                                    \n"% (boardnum,encpinnum,board,encpinnum,boardnum,encpinnum))
                    # for PWM pins
                    elif pintype in (PWMP,PWMD,PWME,PDMP,PDMD,PDME):
                        
                        if not pintype in (PWMP,PDMP): continue    
                        if pin == 7 :encpinnum = (connector-2)*4 
                        elif pin == 6 :encpinnum = 1 + ((connector-2)*4) 
                        elif pin == 19 :encpinnum = 2 + ((connector-2)*4) 
                        elif pin == 18 :encpinnum = 3 + ((connector-2)*4)        
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
        
    
    def on_mesa_pintype_changed(self, widget,boardnum,connector,pin):
                p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                ptype = 'mesa%dc%dpin%dtype' %  (boardnum,connector,pin)    
                old = self.data[ptype]
                new = self.widgets[ptype].get_active_text()    
                if (new == None or new == old): return 
                if old == GPIOI and new in (GPIOO,GPIOD):
                    print "switch GPIO input ",p," to output",new
                    model = self.widgets[p].get_model()
                    blocksignal = "mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)  
                    self.widgets[p].handler_block(self.intrnldata[blocksignal])
                    model.clear()
                    for name in human_output_names: model.append((name,))
                    self.widgets[p].handler_unblock(self.intrnldata[blocksignal])  
                    self.widgets[p].set_active(0)
                    self.data[p] = UNUSED_OUTPUT
                    self.data[ptype] = new
                elif old in (GPIOO,GPIOD) and new == GPIOI:
                    print "switch GPIO output ",p,"to input"
                    model = self.widgets[p].get_model()
                    model.clear()
                    blocksignal = "mesa%dsignalhandlerc%ipin%i"% (boardnum,connector,pin)  
                    self.widgets[p].handler_block(self.intrnldata[blocksignal])              
                    for name in human_input_names:
                        if self.data.limitshared or self.data.limitsnone:
                            if name in human_names_limit_only: continue 
                        if self.data.limitswitch or self.data.limitsnone:
                            if name in human_names_shared_home: continue                          
                        if self.data.homenone or self.data.limitshared:
                            if name in (_("X Home"), _("Y Home"), _("Z Home"), _("A Home"), _("All Home")): continue
                        model.append((name,))
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
                    print "switch PWM  ",p,"to PDM"
                    self.data[ptype] = new
                elif old == PDMP and new == PWMP:
                    print "switch PDM  ",p,"to PWM"
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
        numofstepgens = self.data["mesa%d_numof_stepgens"% boardnum] = int(self.widgets["mesa%d_numof_stepgens"% boardnum].get_value())
        numofencoders = self.data["mesa%d_numof_encodergens"% boardnum] = int(self.widgets["mesa%d_numof_encodergens"% boardnum].get_value())
        title = self.data["mesa%d_boardtitle"% boardnum] = self.widgets["mesa%d_boardtitle"% boardnum].get_active_text()
        firmware = self.data["mesa%d_firmware"% boardnum] = self.widgets["mesa%d_firmware"% boardnum].get_active_text()
        self.set_mesa_options(boardnum,title,firmware,numofpwmgens,numofstepgens,numofencoders)
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

    def set_mesa_options(self,boardnum,title,firmware,numofpwmgens,numofstepgens,numofencoders): 
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



        for concount,connector in enumerate(self.data["mesa%d_currentfirmwaredata"% boardnum][_NUMOFCNCTRS]) :
            for pin in range (0,24):
                self.pbar.set_fraction((pin+1)/24.0)
                while gtk.events_pending():
                    gtk.main_iteration()
                firmptype,compnum = self.data["mesa%d_currentfirmwaredata"% boardnum][_STARTOFDATA+pin+(concount*24)]       
                p = 'mesa%dc%dpin%d' % (boardnum, connector, pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum, connector , pin)
                pinv = 'mesa%dc%dpin%dinv' % (boardnum, connector , pin)
                blocksignal = "mesa%dsignalhandlerc%ipin%i" % (boardnum, connector, pin)    
                ptypeblocksignal  = "mesa%dptypesignalhandlerc%ipin%i" % (boardnum, connector,pin)  
                actblocksignal = "mesa%dactivatehandlerc%ipin%i"  % (boardnum, connector, pin) 
                # kill all widget signals:
                self.widgets[ptype].handler_block(self.intrnldata[ptypeblocksignal])
                self.widgets[p].handler_block(self.intrnldata[blocksignal]) 
                self.widgets[p].child.handler_block(self.intrnldata[actblocksignal])                                            
                # *** convert widget[ptype] to component specified in firmwaredata  *** 
                if self.intrnldata["mesa%d_configured"% boardnum]: 
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
                            model = self.widgets[ptype].get_model()
                            model.clear() 
                            model.append((firmptype,))
                            self.widgets[ptype].set_active(0)
                            model = self.widgets[p].get_model()
                            model.clear()
                            # we only add every 4th human name so the user can only select
                            # the encoder's 'A' signal name. If its the other signals
                            # we can add them all because pncconf controls what the user sees
                            if firmptype == ENCA: 
                                temp = -1                               
                                for name in human_encoder_input_names:                      
                                    temp = temp +1
                                    if temp in (2,3): continue
                                    if temp == 4:
                                        temp = 0
                                        continue
                                    model.append((name,))
                                self.widgets[p].set_active(0)
                                
                                self.widgets[p].set_sensitive(1)
                                self.widgets[ptype].set_sensitive(0)
                            # pncconf control what the user sees with these ones:
                            elif firmptype in(ENCB,ENCI,ENCM):                           
                                for name in human_encoder_input_names:model.append((name,)) 
                                self.widgets[p].set_active(0)   
                                self.widgets[p].set_sensitive(0)
                                self.widgets[ptype].set_sensitive(0)
                            self.widgets[p].set_wrap_width(1)
                            # if the data stored ptype is the encoder family then use the data stored signal name
                            # else set to unused_encoder signal name 
                            # no sense in deleting the user's selected signal if it is for the right ptype
                            if self.data[ptype] == firmptype: 
                                #print self.data[p]
                                self.widgets[p].set_active(0) 
                                model = self.widgets[p].get_model()
                                for search,item in enumerate(model):
                                    if model[search][0]  == human_encoder_input_names[hal_encoder_input_names.index(self.data[p])]:
                                        self.widgets[p].set_active(search)
                                        break 
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
                # ---SETUP GUI FOR PWM FAMILY COMPONENT---
                # the user has a choice of pulse width or pulse density modulation
                elif firmptype in ( PWMP,PWMD,PWME,PDMP,PDMD,PDME ):
                    if numofpwmgens >= (compnum+1):
                        if not self.widgets[ptype].get_active_text() == firmptype:
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0) 
                            # add the two choices PWM and PDM to ptype combobox   
                            model = self.widgets[ptype].get_model()
                            model.clear() 
                            model.append((firmptype,))
                            temp = pintype_names[16]
                            model.append((temp,))                  
                            model = self.widgets[p].get_model()
                            model.clear()
                            # only add the -pulse signal names for the user to see
                            if firmptype in(PWMP,PDMP):
                                temp = -1                               
                                for name in human_pwm_output_names:                       
                                    temp = temp +1
                                    if temp == 2: continue
                                    if temp == 3:
                                        temp = 0
                                        continue
                                    model.append((name,))
                                self.widgets[ptype].set_sensitive(1)
                                self.widgets[p].set_sensitive(1)
                                self.widgets[p].set_active(0)   
                            # add them all here      
                            elif firmptype in (PWMD,PWME,PDMD,PDME):                             
                                self.widgets[p].set_sensitive(0)
                                for name in human_pwm_output_names: model.append((name,))
                                self.widgets[p].set_active(0) 
                                self.widgets[ptype].set_sensitive(0)
                            self.widgets[p].set_wrap_width(1)
                # This is for PWM conversions
                # check to see data is already set to PWM family
                # set the ptype to PWM or PDM 
                # if in PWM family - set to widget signal name 
                # else change to unused_PWM signal name 
                            if self.data[ptype] == firmptype  : 
                                if self.data[ptype] in (PWMP,PWMD,PWME):self.widgets[ptype].set_active(0)
                                else:self.widgets[ptype].set_active(1)
                                self.widgets[p].set_active(0)
                                model = self.widgets[p].get_model()
                                for search,item in enumerate(model):
                                    if model[search][0]  == human_pwm_output_names[hal_pwm_output_names.index(self.data[p])]:
                                        self.widgets[p].set_active(search)
                                        break                               
                            else:
                                self.data[p] =  UNUSED_PWM
                                self.data[ptype] = firmptype
                                self.widgets[p].set_active(0) 
                                if firmptype in (PWMP,PWMD,PWME):self.widgets[ptype].set_active(0)
                                else:self.widgets[ptype].set_active(1) 
                            continue
                    else:
                        firmptype = GPIOI
                # ---SETUP FOR STEPPER FAMILY COMPONENT---
                elif firmptype in (STEPA,STEPB):  
                    if numofstepgens >= (compnum+1):               
                        if not self.widgets[ptype].get_active_text() == firmptype:
                            self.widgets[pinv].set_sensitive(0)
                            self.widgets[pinv].set_active(0)
                            model = self.widgets[ptype].get_model()
                            model.clear() 
                            model.append((firmptype,))
                            model = self.widgets[p].get_model()
                            model.clear() 
                            # We have to step over some extra signalnames that hostmot2 currently
                            # doesn't support yet. support missing for direct coil control stepping
                            if firmptype == STEPA:
                                temp = -1                              
                                for name in (human_stepper_names):
                                    temp = temp + 1
                                    if temp in(2,3,4,5): continue
                                    if temp == 6:
                                        temp = 0
                                        continue
                                    model.append((name,))
                                self.widgets[p].set_sensitive(1)
                                self.widgets[ptype].set_sensitive(0)
                            elif firmptype == STEPB:                               
                                    for name in human_stepper_names: model.append((name,))
                                    self.widgets[p].set_sensitive(0)
                                    self.widgets[p].set_active(0)
                                    self.widgets[ptype].set_sensitive(0) 
                            self.widgets[p].set_wrap_width(1)
                            if self.data[ptype] == firmptype: 
                                self.widgets[ptype].set_active(0)  
                                self.widgets[p].set_active(0)
                                model = self.widgets[p].get_model()
                                for search,item in enumerate(model):
                                    if model[search][0]  == human_stepper_names[hal_stepper_names.index(self.data[p])]:
                                        self.widgets[p].set_active(search)
                                        break
                            else:
                                self.data[p] =  UNUSED_STEPGEN
                                self.data[pinv] = 0
                                self.data[ptype] = firmptype
                                self.widgets[p].set_active(0)
                                self.widgets[ptype].set_active(0)                     
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
                        if not self.data[ptype] in (GPIOI,GPIOO,GPIOD): 
                            self.data[p] =  UNUSED_INPUT
                            self.data[pinv] = 0
                            self.data[ptype] = GPIOI
                        self.widgets[p].set_sensitive(1)
                        self.widgets[pinv].set_sensitive(1)
                        self.widgets[ptype].set_sensitive(1)
                        model = self.widgets[ptype].get_model()
                        model.clear()
                        #  add 'input, output, and open drain' names to GPIO combobox
                        for j in (0,1,2):
                            temp = pintype_names[j]
                            model.append((temp,))
                        model = self.widgets[p].get_model()
                        model.clear()
                        # signal names for GPIO INPUT
                        # add human names to widget excluding signalnames specified in homing limit and spindle
                        if self.data[ptype] == GPIOI:  
                            self.widgets[ptype].set_active(0)                                     
                            for name in human_input_names:
                                if self.data.limitshared or self.data.limitsnone:
                                    if name in human_names_limit_only: continue 
                                if self.data.limitswitch or self.data.limitsnone:
                                    if name in human_names_shared_home: continue                          
                                if self.data.homenone or self.data.limitshared:
                                    if name in (_("X Home"), _("Y Home"), _("Z Home"), _("A Home"),_("All home")): continue
                                model.append((name,))  
                            self.widgets[p].set_active(0)
                            model = self.widgets[p].get_model()
                            for search,item in enumerate(model):
                                if model[search][0]  == human_input_names[hal_input_names.index(self.data[p])]:
                                    self.widgets[p].set_active(search)
                                    break
                            self.widgets[p].set_wrap_width(3)
                            self.widgets[pinv].set_active(self.data[pinv])
                            continue
                        # signal names for GPIO OUTPUT and OPEN DRAIN OUTPUT
                        elif self.data[ptype] in (GPIOO,GPIOD):     
                            if firmptype == GPIOO:self.widgets[ptype].set_active(2)
                            else:self.widgets[ptype].set_active(1)  
                            for name in human_output_names: model.append((name,))
                            self.widgets[p].set_active(0)  
                            model = self.widgets[p].get_model()
                            for search,item in enumerate(model):
                                if model[search][0]  == human_output_names[hal_output_names.index(self.data[p])]:
                                    self.widgets[p].set_active(search)
                                    break   
                            self.widgets[p].set_wrap_width(3)
                            self.widgets[pinv].set_active(self.data[pinv])
                            continue  
        
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
                blocksignal = "mesa%dsignalhandlerc%ipin%i" % (boardnum, connector, pin)    
                ptypeblocksignal  = "mesa%dptypesignalhandlerc%ipin%i" % (boardnum, connector,pin)  
                actblocksignal = "mesa%dactivatehandlerc%ipin%i"  % (boardnum, connector, pin) 
                self.widgets[ptype].handler_unblock(self.intrnldata[ptypeblocksignal])
                self.widgets[p].handler_unblock(self.intrnldata[blocksignal]) 
                self.widgets[p].child.handler_unblock(self.intrnldata[actblocksignal])          
        self.window.hide()
        self.widgets.druid1.set_buttons_sensitive(1,1,1,1)

    # This is for when a user picks a signal name or creates a custom signal (by pressing enter)
    def on_mesa_pin_changed(self, widget, boardnum, connector, pin, custom):
                #if self.in_mesa_prepare == True: return       
                p = 'mesa%dc%dpin%d' % (boardnum,connector,pin)
                ptype = 'mesa%dc%dpin%dtype' % (boardnum,connector,pin)
                pinchanged =  self.widgets[p].get_active_text() 
                dataptype = self.data[ptype]
                used = 0
                #print"pin change method ",ptype," = ",dataptype,"active ",pinchanged,"\n"
                if dataptype in (ENCB,ENCI,ENCM,STEPB,STEPC,STEPD,STEPE,STEPF,PDMD,PDME,PWMD,PWME,GPIOI,GPIOO,GPIOD):return
                # for stepgen pins
                if dataptype == STEPA:
                    #print"ptype step\n"
                    nametocheck = human_stepper_names
                    signaltocheck = hal_stepper_names
                    addsignalto = self.data.halsteppersignames
                    unusedcheck = "Unused StepGen"
                    relatedsearch = [STEPA,STEPB,STEPC,STEPD,STEPE,STEPF] 
                    relatedending = ["-step","-dir","c","d","e","f"]
                # for encoder pins
                elif dataptype == ENCA: 
                    #print"ptype encoder\n"
                    nametocheck = human_encoder_input_names
                    signaltocheck = hal_encoder_input_names
                    addsignalto = self.data.halencoderinputsignames
                    unusedcheck = "Unused Encoder"
                    relatedsearch = [ENCA,ENCB,ENCI,ENCM]
                    relatedending = ["-a","-b","-i","-m"]
                # for PWM pins
                elif dataptype == PWMP: 
                    #print"ptype pwmp\n"
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    unusedcheck = "Unused PWM Gen"
                    relatedsearch = [PWMP,PWMD,PWME]
                    relatedending = ["-pulse","-dir","-enable"]
                # for PDM pins
                elif dataptype == PDMP: 
                    datatype = PWMP
                    #print"ptype pdmp\n"
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    unusedcheck = "Unused PWM Gen"
                    relatedsearch = [PWMP,PWMD,PWME]
                    relatedending = ["-pulse","-dir","-enable"]
                else: 
                    print" pintype not found\n"
                    return   
                # *** change the related pin's signal names ***
                     
                # see if the signal name is in our list of signals
                # or if we are at the end of list and the custom flag is true we will add the signal name
                # either way we have to search the current firmware array for the pin numbers of the related
                # pins so we can change them to the related signal name 
                # all signal names have related signal (eg encoders have A and B phase and index and index mask)
                # except 'unused' signal it is a special case as there is no related signal names with it.

                # have to deep copy because we are going to add names to namestocheck array
                # and that cause recursion
                nametocheck_copy = copy.deepcopy(nametocheck)
                for index, name in enumerate(nametocheck_copy):
                    #print index,name,pinchanged,custom
                    if name == pinchanged or (index+1 == len(nametocheck_copy) and custom == True) :
                        if not pinchanged == unusedcheck:used = 1
                        if name == pinchanged: custom = False
                        for concount,i in enumerate(self.data["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]):
                            if i == connector:
                                # This finds the pin type and component number of the pin that has changed
                                currentptype,currentcompnum = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+pin+(concount*24)]
                                # search all the current firmware array for related pins
                                # if not the same component number as the pin that changed or
                                # if not in the relate component type keep searching
                                # if is the right component type and number search the relatedsearch array for a match
                                # while search with or without a match:
                                # if custom flag set and the component type is for the user selectable signal (first relatedsearch item)
                                # add the signal name with new related endings to the search arrays 
                                # and widget combobox list - again unless the component type is for the user selectable signal then
                                # only put the first signalname ending in it.
                                # if we found a match display it in combobox (if a custom signal we have to search for the index number)
                                for t_concount,t_connector in enumerate(self.data["mesa%d_currentfirmwaredata"% (boardnum)][_NUMOFCNCTRS]) :
                                    for t_pin in range (0,24):
                                        comptype,compnum = self.data["mesa%d_currentfirmwaredata"% (boardnum)][_STARTOFDATA+t_pin+(t_concount*24)]
                                        if compnum != currentcompnum: continue                             
                                        if comptype not in (relatedsearch): continue
                                        tochange = 'mesa%dc%dpin%d' % (boardnum,t_connector,t_pin)
                                        #print "checking-",comptype,"num-",compnum,"in ",tochange
                                        blocksignal = "mesa%dsignalhandlerc%ipin%i" % (boardnum, t_connector, t_pin) 
                                        self.widgets[tochange].handler_block(self.intrnldata[blocksignal])
                                        blocksignal = "mesa%dactivatehandlerc%ipin%i"  % (boardnum, t_connector, t_pin) 
                                        self.widgets[tochange].child.handler_block(self.intrnldata[blocksignal])
                                        for offset,i in enumerate(relatedsearch):                                     
                                            #print "rawname-"+pinchanged,"    offset ",offset
                                            if custom :
                                                legal_name = pinchanged.replace(" ","_")
                                                with_endings = legal_name + relatedending[offset]                                               
                                                if comptype == relatedsearch[0]:
                                                    #print "*** adding names to arrays:" 
                                                    #print "legalname:",legal_name, ",",with_endings
                                                    signaltocheck.append ((with_endings))
                                                    nametocheck.append ((legal_name))
                                                    addsignalto.append ((with_endings))
                                                if comptype != relatedsearch[0] or (comptype == relatedsearch[0] and offset == 0):
                                                    #print "*** adding names to widget:"
                                                    model = self.widgets[tochange].get_model()
                                                    model.append((legal_name,))
                                            if i == comptype:
                                                #print "*** comptype found- "+ i," pinchanged:",pinchanged 
                                               
                                                if custom :  
                                                    searchword = legal_name                                                                 
                                                else:
                                                    searchword = pinchanged
                                                model = self.widgets[tochange].get_model()
                                                for search,item in enumerate(model):
                                                    #print "signal-> ",model[search][0],"<-",customname," ",pinchanged
                                                    if model[search][0]  == searchword:
                                                        self.widgets[tochange].set_active(search)
                                                        break
                                        self.widgets[tochange].child.handler_unblock(self.intrnldata[blocksignal])
                                        blocksignal = "mesa%dsignalhandlerc%ipin%i" % (boardnum, t_connector, t_pin) 
                                        self.widgets[tochange].handler_unblock(self.intrnldata[blocksignal])
                                        if i == comptype :break

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
                if not step == "false" and not pwm == "false":
                    warnings.append(_("You can not have both steppers and pwm signals for spindle control\n") )
                    do_warning = True
                continue
            if step == "false" and pwm == "false" and enc =="false":
                warnings.append(_("You forgot to designate a stepper or pwm signal for axis %s\n")% i)
                do_warning = True
            if not pwm == "false" and enc == "false": 
                warnings.append(_("You forgot to designate a servo encoder signal for axis %s\n")% i)
                do_warning = True
            if pwm == "false" and not enc == "false": 
                warnings.append(_("You forgot to designate a servo pwm signal for axis %s\n")% i)
                do_warning = True
            if not step == "false" and not enc == "false": 
                warnings.append(_("You can not have encoders with steppers for axis %s\n")% i)
                do_warning = True
            if not step == "false" and not pwm == "false": 
                warnings.append(_("You can not have both steppers and pwm signals for axis %s\n")% i)
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
        test = self.data.findsignal(axis+"-stepgen-step")
        stepdriven = 1
        if test == "false":stepdriven = 0
        d = self.data
        w = self.widgets
        def set_text(n): w[axis + n].set_text("%s" % d[axis + n])
        def set_value(n): w[axis + n].set_value(d[axis + n])
        def set_active(n): w[axis + n].set_active(d[axis + n])
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
        w["pulleydriver"].set_value(d[axis+"pulleydriver"])
        w["pulleydriven"].set_value(d[axis +"pulleydriven"])
        w["leadscrew"].set_value(d[axis +"leadscrew"])
        w["encoderline"].set_value((d[axis+"encodercounts"]/4))
        set_text("encodercounts")
        set_value("scale")
        w[axis+"maxvel"].set_value(d[axis+"maxvel"]*60)
        set_value("maxacc")
        w[axis + "servo_info"].set_sensitive(not stepdriven)
        w[axis + "stepper_info"].set_sensitive(stepdriven)  
        w[axis + "invertencoder"].set_sensitive(not stepdriven)
        w[axis + "drivertype"].set_active(self.drivertype_toindex(axis))
        if w[axis + "drivertype"].get_active_text()  == _("Custom"):
            w[axis + "steptime"].set_value(d[axis + "steptime"])
            w[axis + "stepspace"].set_value(d[axis + "stepspace"])
            w[axis + "dirhold"].set_value(d[axis + "dirhold"])
            w[axis + "dirsetup"].set_value(d[axis + "dirsetup"])
        gobject.idle_add(lambda: self.update_pps(axis))

        if axis == 's':
            self.widgets.spidcontrol.set_active( self.data.spidcontrol )
            test = self.data.findsignal("s-stepgen-step")
            stepdriven = 1
            if test == "false":
                stepdriven = 0
            test = self.data.findsignal("s-pwm-pulse")
            pwmdriven = 1
            if test == "false":
                pwmdriven = 0
            if stepdriven:
                w["sresolutionunits"].set_text(_("revolution / Step"))        
                w["sscaleunits"].set_text(_("Steps / revolution"))
            else:
                w["sresolutionunits"].set_text(_("revolution / encoder pulse"))
                w["sscaleunits"].set_text(_("Encoder pulses / revolution"))
            w["leadscrewlabel"].set_text(_("Gearbox Reduction Ratio"))
            w["screwunits"].set_text((""))
            #self.widgets['spindlecarrier'].set_text("%s" % self.data.spindlecarrier)
            w['spindlespeed1'].set_text("%s" % d.spindlespeed1)
            w['spindlespeed2'].set_text("%s" % d.spindlespeed2)
            w['spindlepwm1'].set_text("%s" % d.spindlepwm1)
            w['spindlepwm2'].set_text("%s" % d.spindlepwm2)
            #self.widgets['spindlecpr'].set_text("%s" % self.data.spindlecpr)
            has_spindle_pha = self.data.findsignal("s-encoder-a")
            if has_spindle_pha == "false":
                
                w.sencodercounts.set_sensitive(0)
            else: 
                
                w.sencodercounts.set_sensitive(1) 
            w[axis + "invertencoder"].set_sensitive(True)
            w["soutputscale"].set_sensitive(pwmdriven)
            w["soutputoffset"].set_sensitive(pwmdriven)
            w["smaxoutput"].set_sensitive(pwmdriven)
            w["sservo_info"].set_sensitive(pwmdriven)
            self.on_spidcontrol_toggled()
            w["saxistest"].set_sensitive(pwmdriven)
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
                w["leadscrewlabel"].set_text(_("Reduction Ratio"))
                w["screwunits"].set_text(_("degrees / rev"))
                w[axis + "velunits"].set_text(_("degrees / min"))
                w[axis + "accunits"].set_text(_("degrees / sec²"))
                w[axis + "homevelunits"].set_text(_("degrees / min"))
                w[axis + "homelatchvelunits"].set_text(_("degrees / min"))
                w[axis + "homefinalvelunits"].set_text(_("degrees / min"))
                w[axis + "accdistunits"].set_text(_("degrees"))
                if stepdriven:
                    w[axis + "resolutionunits1"].set_text(_("degree / Step"))        
                    w[axis + "scaleunits"].set_text(_("Steps / degree"))
                else:
                    w[axis + "resolutionunits1"].set_text(_("degrees / encoder pulse"))
                    w[axis + "scaleunits"].set_text(_("Encoder pulses / degree"))
                w[axis + "minfollowunits"].set_text(_("degrees"))
                w[axis + "maxfollowunits"].set_text(_("degrees"))
    
            elif d.units:
                w["leadscrewlabel"].set_text(_("Leadscrew Pitch"))
                w["screwunits"].set_text(_("(mm / rev)"))
                w[axis + "velunits"].set_text(_("mm / min"))
                w[axis + "accunits"].set_text(_("mm / sec²"))
                w[axis + "homevelunits"].set_text(_("mm / min"))
                w[axis + "homelatchvelunits"].set_text(_("mm / min"))
                w[axis + "homefinalvelunits"].set_text(_("mm / min"))
                w[axis + "accdistunits"].set_text(_("mm"))
                if stepdriven:
                    w[axis + "resolutionunits1"].set_text(_("mm / Step"))        
                    w[axis + "scaleunits"].set_text(_("Steps / mm"))
                else:
                    w[axis + "resolutionunits1"].set_text(_("mm / encoder pulse"))          
                    w[axis + "scaleunits"].set_text(_("Encoder pulses / mm"))
               
                w[axis + "minfollowunits"].set_text(_("mm"))
                w[axis + "maxfollowunits"].set_text(_("mm"))
               
            else:
                w["leadscrewlabel"].set_text(_("Leadscrew TPI"))
                w["screwunits"].set_text(_("(rev / inch)"))
                w[axis + "velunits"].set_text(_("inches / min"))
                w[axis + "accunits"].set_text(_("inches / sec²"))
                w[axis + "homevelunits"].set_text(_("inches / min"))
                w[axis + "homelatchvelunits"].set_text(_("inches / min"))
                w[axis + "homefinalvelunits"].set_text(_("inches / min"))
                w[axis + "accdistunits"].set_text(_("inches"))
                if stepdriven:
                    w[axis + "resolutionunits1"].set_text(_("inches / Step"))        
                    w[axis + "scaleunits"].set_text(_("Steps / inch"))
                else:
                    w[axis + "resolutionunits1"].set_text(_("inches / encoder pulse"))        
                    w[axis + "scaleunits"].set_text(_("Encoder pulses / inch"))
               
                w[axis + "minfollowunits"].set_text(_("inches"))
                w[axis + "maxfollowunits"].set_text(_("inches"))
            thisaxishome = set(("all-home", "home-" + axis, "min-home-" + axis,"max-home-" + axis, "both-home-" + axis))
            homes = False
            for i in thisaxishome:
                test = self.data.findsignal(i)
                if not test == "false": homes = True
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
        d[axis + "steprev"] = int(get_value(w["steprev"]))
        d[axis + "microstep"] = int(get_value(w["microstep"]))
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
        get_pagevalue("scale")
        get_active("invertmotor")
        get_active("invertencoder") 
        d[axis + "pulleydriver"] = int(get_value(w["pulleydriver"]))
        d[axis + "pulleydriven"] = int(get_value(w["pulleydriven"]))
        d[axis + "leadscrew"] = int(get_value(w["leadscrew"]))        
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
            self.data.spindlespeed1 = get_value(self.widgets.spindlespeed1)
            self.data.spindlespeed2 = get_value(self.widgets.spindlespeed2)
            self.data.spindlepwm1 = get_value(self.widgets.spindlepwm1)
            self.data.spindlepwm2 = get_value(self.widgets.spindlepwm2)
            #self.data.spindlecarrier = get_value(self.widgets.spindlecarrier)
            #self.data.spindlecpr = get_value(self.widgets.spindlecpr)
            get_active("pidcontrol") 

    def calculate_scale(self, axis):
        w = self.widgets
        stepdriven = rotaryaxis = encoder = 1
        def get(n): return get_value(w[n])
        test = self.data.findsignal(axis+"-stepgen-step")    
        if test == "false":stepdriven = 0
        test = self.data.findsignal(axis+"-encoder-a")    
        if test == "false":encoder = 0
        w["steprev"].set_sensitive( stepdriven ) 
        w["microstep"].set_sensitive( stepdriven )
        w["encoderline"].set_sensitive( encoder )
        if not axis == 'a': rotaryaxis = 0
        w["wormdriver"].set_sensitive( rotaryaxis ) 
        w["wormdriven"].set_sensitive( rotaryaxis )
        w["leadscrew"].set_sensitive( not rotaryaxis )
        self.widgets.scaledialog.set_title(_("Axis Scale Calculation"))
        self.widgets.scaledialog.show_all()
        result = self.widgets.scaledialog.run()
        self.widgets.scaledialog.hide()
        try:
            worm_ratio = enc_count_per_rev = steps_per_rev = 1
            if axis == 'a': 
                pitch = 1
                worm_ratio = (get("wormdriver") / get("wormdriven"))
            elif self.data.units == 1: 
                pitch = 1./ get("leadscrew")
            else:  
                pitch = get("leadscrew")
            motor_ratio = (get("pulleydriver") / get("pulleydriven"))          
            if stepdriven :
                steps_per_rev = get("steprev") * get("microstep")
                scale = ( steps_per_rev * pitch * worm_ratio * motor_ratio)
            else:
                enc_count_per_rev = get_value(w[("encoderline")]) * 4
                scale =  ( enc_count_per_rev * pitch * worm_ratio * motor_ratio)
            w[axis + "encodercounts"].set_text( "%d" % ( enc_count_per_rev))
            if axis == 'a': scale = scale / 360
            w[axis + "calscale"].set_text("%.1f" % scale)
            w[axis + "scale"].set_value(scale)   
        except (ValueError, ZeroDivisionError):
            w[axis + "scale"].set_text( "")
        self.update_pps(axis)

    def update_pps(self, axis):
        w = self.widgets
        d = self.data
        worm_ratio = motor_ratio = 1       
        def get(n): return get_value(w[axis + n])

        try:
            if axis == 'a': 
                pitch = 1
                worm_ratio = (get_value(w.wormdriver) / get_value(w.wormdriven))
            elif self.data.units == 1: 
                pitch = 1./ get_value(w.leadscrew)
            else:  
                pitch = get_value(w.leadscrew)
            motor_ratio = (get_value(w.pulleydriver) / get_value(w.pulleydriven))  
            maxvps = (get("maxvel"))/60
            pps = (get_value(w[axis+"scale"]) * (maxvps))/1000
            if pps == 0: raise ValueError
            pps = abs(pps)
            w[axis + "khz"].set_text("%.1f" % pps)
            acctime = (maxvps) / get("maxacc")
            accdist = acctime * .5 * (maxvps)
            maxrpm = int(maxvps * 60 *  pitch /( worm_ratio * motor_ratio))
            w[axis + "acctime"].set_text("%.4f" % acctime)
            if not axis == 's':
                w[axis + "accdist"].set_text("%.4f" % accdist)                 
            w[axis + "chartresolution"].set_text("%.7f" % (1.0 / get_value(w[axis+"scale"])))
            w[axis + "calscale"].set_text(str(get_value(w[axis+"scale"])))
            w[axis + "maxrpm"].set_text("%d" % maxrpm)
            self.widgets.druid1.set_buttons_sensitive(1,1,1,1)
            w[axis + "axistune"].set_sensitive(1)
        except (ValueError, ZeroDivisionError): # Some entries not numbers or not valid
            w[axis + "chartresolution"].set_text("")
            w[axis + "acctime"].set_text("")
            if not axis == 's':
                w[axis + "accdist"].set_text("")
            w[axis + "khz"].set_text("")
            w[axis + "calscale"].set_text("")
            self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
            w[axis + "axistune"].set_sensitive(0)

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
            if not has_spindle == "false":
                return True
        return False

    def on_spidcontrol_toggled(self, *args):
        test = self.data.findsignal("s-pwm-pulse")
        pwmdriven = 1
        if test == "false":pwmdriven = 0
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
        self.widgets.userneededmux8.set_value(self.data.userneededmux8)
        self.widgets.userneededabs.set_value(self.data.userneededabs)
        self.widgets.userneededscale.set_value(self.data.userneededscale)
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
        self.data.userneededmux8 = int(self.widgets.userneededmux8.get_value())
        self.data.userneededabs = int(self.widgets.userneededabs.get_value())
        self.data.userneededscale = int(self.widgets.userneededscale.get_value())
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
        match =  os.popen('lspci -v').read()
        self.widgets.helpwindow.set_title(_("PCI Board Info Search"))
        textbuffer = self.widgets.helpview.get_buffer()
        try :         
            textbuffer.set_text(match)
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

        if not self.stepgen == "false":
            w[axis+"tuningnotebook"].set_current_page(1)
            w[axis+"pid"].set_sensitive(0)
            w[axis+"tuneinvertencoder"].set_sensitive(0)
            w[axis+"pidtable"].set_sensitive(0)
        else:
            w[axis+"tuningnotebook"].set_current_page(0)
            w[axis+"step"].set_sensitive(0)
            w[axis+"steptable"].set_sensitive(0)
            text = _("Servo tuning is not finished\n")
            self.warning_dialog(text,True)
            return

        if axis == "a":
            w[axis + "tunedistunits"].set_text(_("degrees"))
            w[axis + "tunevelunits"].set_text(_("degrees / minute"))
            w[axis + "tuneaccunits"].set_text(_("degrees / second²"))
        elif d.units:
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
        loadrt steptest     
        """ % {'period':100000, 'period2':self.data.servoperiod })   
        if self.stepgen == "false": 
            halrun.write("loadrt pid num_chan=1\n")
        self.hal_cmnds("LOAD")
        self.hal_cmnds("READ")       
        halrun.write("addf steptest.0 slow \n")
        if self.stepgen == "false": 
            halrun.write("addf pid.0.do-pid-calcs slow \n")
        self.hal_cmnds("WRITE")
        # for encoder signals
        if "mesa" in self.encoder: 
            print self.encoder,"--",self.encoder[4:5],self.encoder[10:],self.encoder[6:7] 
            self.enc_signalname = self.data.make_pinname(self.encoder)                 
            halrun.write("setp %s.counter-mode 0\n"% (self.enc_signalname))
            halrun.write("setp %s.filter 1\n"% (self.enc_signalname))
            halrun.write("setp %s.index-invert 0\n"% (self.enc_signalname))
            halrun.write("setp %s.index-mask 0\n"% (self.enc_signalname))
            halrun.write("setp %s.index-mask-invert 0\n"% (self.enc_signalname)) 
            halrun.write("setp %s.scale %d\n"% (self.enc_signalname, get_value(w[axis + "scale"])))                         
            halrun.write("loadusr halmeter -s pin %s.velocity -g 0 625 330\n"% (self.enc_signalname))
            halrun.write("loadusr halmeter -s pin %s.position -g 0 675 330\n"% (self.enc_signalname))
            halrun.write("loadusr halmeter pin %s.velocity -g 275 415\n"% (self.enc_signalname))
        # for pwm components
        if "mesa" in self.pwmgen:                             
            self.pwm_signalname = self.data.make_pinname(self.pwmgen)  
            print "got to pwm", self.pwmgen," -- ",self.pwm_signalname                        
            halrun.write("setp %s.scale 10\n"% (self.pwm_signalname))                        
            halrun.write("setp %s.output-type 1\n"% (self.pwm_signalname))                             
            halrun.write("loadusr halmeter pin %s.enable -g 0 415\n"% (self.pwm_signalname))
            halrun.write("loadusr halmeter -s pin %s.enable -g 0 525 330\n"% (self.pwm_signalname))
            halrun.write("loadusr halmeter -s pin %s.value -g 0 575 330\n"% (self.pwm_signalname)) 
        # for step gen components
        if "mesa" in self.stepgen:                        
            # check current component number to signal's component number                             
            self.step_signalname = self.data.make_pinname(self.stepgen) 
            print "step_signal--",self.step_signalname   
            if w[axis+"invertmotor"].get_active():
                self.scale = get_value(w[axis + "scale"]) * -1
            else:
                self.scale = get_value(w[axis + "scale"]) * 1
                #halrun.write("setp %s.gpio.%03d.invert_output %d \n"% (self.step_signalname,self.invert,guiinvert))
            halrun.write("setp %s.step_type 0 \n"% (self.step_signalname))
            halrun.write("setp %s.position-scale %f \n"% (self.step_signalname,self.scale))
            halrun.write("setp %s.steplen %d \n"% (self.step_signalname,w[axis+"steptime"].get_value()))
            halrun.write("setp %s.stepspace %d \n"% (self.step_signalname,w[axis+"stepspace"].get_value()))
            halrun.write("setp %s.dirhold %d \n"% (self.step_signalname,w[axis+"dirhold"].get_value()))
            halrun.write("setp %s.dirsetup %d \n"% (self.step_signalname,w[axis+"dirsetup"].get_value()))
            halrun.write("setp steptest.0.epsilon %f\n"% abs(1. / get_value(w[axis + "scale"]))  )
            halrun.write("setp %s.maxaccel 0 \n"% (self.step_signalname))
            halrun.write("setp %s.maxvel 0 \n"% (self.step_signalname))
            halrun.write("net enable => %s.enable \n"% (self.step_signalname))
            halrun.write("net cmd steptest.0.position-cmd => %s.position-cmd \n"% (self.step_signalname))
            halrun.write("net feedback steptest.0.position-fb <= %s.position-fb \n"% (self.step_signalname))
            halrun.write("loadusr halmeter pin %s.velocity-fb -g 0 415\n"% (self.step_signalname))
            halrun.write("loadusr halmeter -s pin %s.velocity-fb -g 0 575 350\n"% (self.step_signalname))
            halrun.write("loadusr halmeter -s pin %s.position-fb -g 0 525 350\n"% (self.step_signalname))
        # set up enable output pin if used
        temp = self.data.findsignal( "enable")
        amp = self.data.make_pinname(temp)
        if not amp == "false":
            if "hm2" in amp:    
                halrun.write("setp %s true\n"% (amp + ".is_output"))             
                halrun.write("net enable %s \n"% (amp + ".out"))
                if self.data[temp+"inv"] == True:
                    halrun.write("setp %s true\n"%  (amp + ".invert_output"))
        # set up estop output if used
        temp = self.data.findsignal( "estop-out")
        estop = self.data.make_pinname(temp)
        if not estop =="false":        
            if "hm2" in estop:
                halrun.write("setp %s true\n"%  (estop + ".is_output"))    
                halrun.write("net enable %s\n"%  (estop + ".out"))
                if self.data[temp+"inv"] == True:
                    halrun.write("setp %s true\n"%  (estop + ".invert_output"))
        # set up as servo system if no step generator...
        if self.stepgen == "false":
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
        if not self.stepgen == "false":
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
    def on_tuneinvertmotor_toggled(self, w):
        axis = self.axis_under_tune
        w = self.widgets
        if w[axis+"tuneinvertmotor"].get_active():
            self.scale = get_value(w[axis + "scale"]) * -1
        else:
            self.scale = get_value(w[axis + "scale"])                 
        self.update_tune_axis_params()

    # openloop servo test
    def test_axis(self, axis):
        if not self.check_for_rt(self):
            return
        if self.data.findsignal( (axis + "-pwm-pulse")) =="false" or self.data.findsignal( (axis + "-encoder-a")) =="false":
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
        enc_scale = get_value(widgets[axis+"scale"])          
        if not self.data.findsignal("charge-pump") =="false": 
            pump = True   
        
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
        if not self.amp == "false":
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
        if not estop == "false":        
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
        if not pump == "false":        
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
        if not pwm == "false":          
            halrun.write("net dac %s \n"%  (pwm +".value"))
            halrun.write("setp %s \n"%  (pwm +".enable true"))
            halrun.write("setp %s \n"%  (pwm +".scale 10"))
            halrun.write("loadusr halmeter -s pin %s -g 550 500 330\n"%  (pwm +".value"))
            halrun.write("loadusr halmeter pin %s -g 550 375\n"% (pwm +".value") )
        # set up encoder     
        self.enc = self.data.make_pinname(self.data.findsignal( (axis + "-encoder-a")))
        if not self.enc =="false":           
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
        if not self.amp == "false":
             halrun.write("setp %s false\n"% (self.amp))
        if not estop == "false":
             halrun.write("setp %s false\n"% (estop))
        time.sleep(.001)
        halrun.close()        
        if result == gtk.RESPONSE_OK:
            #widgets[axis+"maxacc"].set_text("%s" % widgets.testacc.get_value())
            widgets[axis+"invertmotor"].set_active(widgets.testinvertmotor.get_active())
            widgets[axis+"invertencoder"].set_active(widgets.testinvertencoder.get_active())
            widgets[axis+"outputoffset"].set_value(widgets.testoutputoffset.get_value())
            widgets[axis+"scale"].set_value(widgets.testenc_scale.get_value())
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
        if not self.amp == "false":
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
   
    def hal_cmnds(self,command = "nothing"):
        #print command
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
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"\n """ % (
                    driver0, directory0, firm0, self.data.mesa0_numof_encodergens, self.data.mesa0_numof_pwmgens, self.data.mesa0_numof_stepgens ))
            elif self.data.number_mesa == 2 and (driver0 == driver1):
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d,\
                                firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"\n
                    """ % (
                    driver0, directory0, firm0, self.data.mesa0_numof_encodergens, self.data.mesa0_numof_pwmgens, self.data.mesa0_numof_stepgens,
                    directory1, firm1, self.data.mesa1_numof_encodergens, self.data.mesa1_numof_pwmgens, self.data.mesa1_numof_stepgens ))
            elif self.data.number_mesa == 2:
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"\n """ % (
                    driver0, directory0, firm0, self.data.mesa0_numof_encodergens, self.data.mesa0_numof_pwmgens, self.data.mesa0_numof_stepgens ))
                halrun.write( """loadrt %s config="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"\n """ % (
                    driver1, directory1, firm1, self.data.mesa1_numof_encodergens, self.data.mesa1_numof_pwmgens, self.data.mesa1_numof_stepgens ))
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
        self.hal.c.exit()
        gobject.source_remove(self.timer) 
        self.app.halrun.close()     
        return False

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
        self.data2["brd%denc%dreset" % (boardnum,number)]= gtk.Button("Reset-%d"% number)
        container.pack_start(self.data2["brd%denc%dreset" % (boardnum,number)], False, False, 10)
        encname = "brd.%d.enc.%d.count" % (boardnum,number)
        print"making HAL pin enc s32 brd %d num %d"%(boardnum,number)      
        self.hal.c.newpin(encname, hal.HAL_S32, hal.HAL_IN)
        label = self.data2["brd%denc%dcount" % (boardnum,number)] = gtk.Label("Encoder-%d"% (number))
        label.set_size_request(100, -1)
        container.pack_start(label, False, False, 10)
    
    # This creates widgets and HAL pins for stepper controls 
    def make_stp(self,container,boardnum,number):
        stpname = "brd.%d.stp.%d.cmd" % (boardnum,number)
        self.hal.c.newpin(stpname, hal.HAL_FLOAT, hal.HAL_OUT)
        stpname = "brd.%d.stp.%d.enable" % (boardnum,number)
        self.hal.c.newpin(stpname, hal.HAL_BIT, hal.HAL_OUT)
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
        pwmname = "brd.%d.pwm.%d.enable" % (boardnum,number)
        print"making HAL pin pwm bit brd %d num %d"%(boardnum,number)
        self.hal.c.newpin(pwmname, hal.HAL_BIT, hal.HAL_OUT)
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
        self.halrun = self.app.halrun
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
                    # for input pins
                    elif pintype == GPIOI: 
                        h = gtk.HBox(False,2)
                        self.make_led(h,boardnum,truepinnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                    # for encoder pins
                    elif pintype in (ENCA,ENCB,ENCI,ENCM):
                        h = gtk.HBox(False,2)
                        if pintype == ENCA:
                            self.make_enc(h,boardnum,compnum)
                        else:
                            self.make_blank(h,boardnum,compnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                    # for PWM pins
                    elif pintype in (PWMP,PWMD,PWME,PDMP,PDMD,PDME):
                        h = gtk.HBox(False,2)
                        if pintype in (PWMP,PDMP):
                            self.make_pwm(h,boardnum,compnum)
                        else:
                            self.make_blank(h,boardnum,compnum)
                        table.attach(h, 0 + column, 1 + column, pin + adjust, pin +1+ adjust,True)
                    # for Stepgen pins
                    elif pintype in (STEPA,STEPB):
                        h = gtk.HBox(False,2)
                        if pintype == STEPA:          
                            self.make_stp(h,boardnum,compnum)
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
