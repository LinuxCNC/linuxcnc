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

(GPIOI, GPIOO, GPIOD, ENCA, ENCB, ENCI, ENCM, STEPA, STEPB, PWMP, PWMD, PWME, PDMP, PDMD, PDME ) = pintype_names = [
_("GPIO Input"),_("GPIO Output"),_("GPIO O Drain"),
_("HDW Encoder-A"),_("HDW Encoder-B"),_("HDW Encoder-I"),_("HDW Encoder-M"),
_("HDW Step Gen-A"),_("HDW Step Gen-B"),
_("HDW PWM Gen-P"),_("HDW PWM Gen-D"),_("HDW PWM Gen-E"),
_("HDW PDM Gen-P"),_("HDW PDM Gen-D"),_("HDW PDM Gen-E") ]

# boardname, firmwarename, max encoders, max pwm gens, max step gens, # of pins / encoder, # of pins / step gen, watchdog, max GPIOI + COMPONENT TYPES
mesafirmwaredata = [
    ["5i20", "SV12", 12, 12, 0, 3, 0, 1, 72 , [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10] ],
    ["5i20", "SVST8_4", 8, 8, 4, 3, 6, 1, 72, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i20", "SVST2_8", 2, 2, 8, 3, 6, 1, 72, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
        [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,3],[GPIOI,3],[GPIOI,3],[GPIOI,3],
        [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                  [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i20", "SVST8_4IM2", 8, 8, 4, 4, 2, 1, 72, [2,3,4],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCM,0],[ENCM,1],[ENCM,2],[ENCM,3],[ENCM,4],[ENCM,5],[ENCM,6],[ENCM,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                 [GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3] ],
    ["5i22", "SV16", 16, 16, 0, 3, 0, 1, 96, [2,3,4,5],
        [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                 [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
        [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                 [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
        [ENCB,9],[ENCA,9],[ENCB,8],[ENCA,8],[ENCI,9],[ENCI,8],[PWMP,9],[PWMP,8],[PWMD,9],[PWMD,8],[PWME,9],[PWME,8],
                 [ENCB,11],[ENCA,11],[ENCB,10],[ENCA,10],[ENCI,11],[ENCI,10],[PWMP,11],[PWMP,10],[PWMD,11],[PWMD,10],[PWME,11],[PWME,10],
        [ENCB,13],[ENCA,13],[ENCB,12],[ENCA,12],[ENCI,13],[ENCI,12],[PWMP,13],[PWMP,12],[PWMD,13],[PWMD,12],[PWME,13],[PWME,12],
                  [ENCB,15],[ENCA,15],[ENCB,14],[ENCA,14],[ENCI,15],[ENCI,14],[PWMP,15],[PWMP,14],[PWMD,15],[PWMD,14],[PWME,15],[PWME,14] ],
    ["5i22", "SVST8_8", 8, 8, 8, 3, 6, 1, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,3],[GPIOI,3],[GPIOI,3],[GPIOI,3],
       [STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,6],[STEPB,6],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,7],[STEPB,7],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0] ],
    ["5i22", "SVS8_24", 8, 8, 24, 3, 2, 1, 96, [2,3,4,5],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6],
       [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11],
       [STEPA,12],[STEPB,12],[STEPA,13],[STEPB,13],[STEPA,14],[STEPB,14],[STEPA,15],[STEPB,15],[STEPA,16],[STEPB,16],[STEPA,17],[STEPB,17],
                [STEPA,18],[STEPB,18],[STEPA,19],[STEPB,19],[STEPA,20],[STEPB,20],[STEPA,21],[STEPB,21],[STEPA,22],[STEPB,22],[STEPA,23],[STEPB,23] ],
    ["7i43", "SV8", 8, 8, 0, 3, 0, 1, 48, [3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [ENCB,5],[ENCA,5],[ENCB,4],[ENCA,4],[ENCI,5],[ENCI,4],[PWMP,5],[PWMP,4],[PWMD,5],[PWMD,4],[PWME,5],[PWME,4],
                [ENCB,7],[ENCA,7],[ENCB,6],[ENCA,6],[ENCI,7],[ENCI,6],[PWMP,7],[PWMP,6],[PWMD,7],[PWMD,6],[PWME,7],[PWME,6] ],
    ["7i43", "SV4_4", 4, 4, 4, 3, 6, 1, 48, [3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],
                [STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],[GPIOI,0],[GPIOI,0],[STEPA,3],[STEPB,3],[GPIOI,3],[GPIOI,3],[GPIOI,3],[GPIOI,3] ],      
    ["7i43", "SV4_6", 4, 4, 6, 3, 4, 1, 48, [3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [STEPA,0],[STEPB,0],[GPIOI,0],[GPIOI,0],[STEPA,1],[STEPB,1],[GPIOI,0],[GPIOI,0],[STEPA,2],[STEPB,2],[GPIOI,0],[GPIOI,0],
                [STEPA,3],[STEPB,3],[GPIOI,0],[GPIOI,0],[STEPA,4],[STEPB,4],[GPIOI,0],[GPIOI,0],[STEPA,5],[STEPB,5],[GPIOI,0],[GPIOI,0] ],
    ["7i43", "SV4_12", 4, 4, 12, 3, 2, 1, 48, [3,4],
       [ENCB,1],[ENCA,1],[ENCB,0],[ENCA,0],[ENCI,1],[ENCI,0],[PWMP,1],[PWMP,0],[PWMD,1],[PWMD,0],[PWME,1],[PWME,0],
                [ENCB,3],[ENCA,3],[ENCB,2],[ENCA,2],[ENCI,3],[ENCI,2],[PWMP,3],[PWMP,2],[PWMD,3],[PWMD,2],[PWME,3],[PWME,2],
       [STEPA,0],[STEPB,0],[STEPA,1],[STEPB,1],[STEPA,2],[STEPB,2],[STEPA,3],[STEPB,3],[STEPA,4],[STEPB,4],[STEPA,5],[STEPB,5],
                [STEPA,6],[STEPB,6],[STEPA,7],[STEPB,7],[STEPA,8],[STEPB,8],[STEPA,9],[STEPB,9],[STEPA,10],[STEPB,10],[STEPA,11],[STEPB,11] ],
]

mesaboardnames = [ "5i20", "5i22", "7i43" ]



(UNUSED_OUTPUT,
ON, CW, CCW, PWM, BRAKE,
MIST, FLOOD, ESTOP, AMP,
PUMP, DOUT0, DOUT1, DOUT2, DOUT3) = hal_output_names = [
"unused-output", 
"spindle-on", "spindle-cw", "spindle-ccw", "spindle-pwm", "spindle-brake",
"coolant-mist", "coolant-flood", "estop-out", "enable",
"charge-pump", "dout-00", "dout-01", "dout-02", "dout-03"
]

(UNUSED_INPUT,
ESTOP_IN, PROBE, PPR, PHA, PHB,
HOME_X, HOME_Y, HOME_Z, HOME_A,
MIN_HOME_X, MIN_HOME_Y, MIN_HOME_Z, MIN_HOME_A,
MAX_HOME_X, MAX_HOME_Y, MAX_HOME_Z, MAX_HOME_A,
BOTH_HOME_X, BOTH_HOME_Y, BOTH_HOME_Z, BOTH_HOME_A,
MIN_X, MIN_Y, MIN_Z, MIN_A,
MAX_X, MAX_Y, MAX_Z, MAX_A,
BOTH_X, BOTH_Y, BOTH_Z, BOTH_A,
ALL_LIMIT, ALL_HOME, DIN0, DIN1, DIN2, DIN3) = hal_input_names = ["unused-input",
"estop-ext", "probe-in", "spindle-index", "spindle-phase-a", "spindle-phase-b",
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

(UNUSED_PWM,
X_PWM_PULSE, X_PWM_DIR, X_PWM_ENABLE, Y_PWM_PULSE, Y_PWM_DIR, Y_PWM_ENABLE, 
Z_PWM_PULSE, Z_PWM_DIR, Z_PWM_ENABLE, A_PWM_PULSE, A_PWM_DIR, A_PWM_ENABLE, 
SPINDLE_PWM_PULSE, SPINDLE_PWM_DIR, SPINDLE_PWM_ENABLE,   ) = hal_pwm_output_names = ["unused-pwm",
"x-pwm-pulse", "x-pwm-dir", "x-pwm-enable", "y-pwm-pulse", "y-pwm-dir", "y-pwm-enable",
"z-pwm-pulse", "z-pwm-dir", "z-pwm-enable", "a-pwm-pulse", "a-pwm-dir", "a-pwm-enable", 
"spindle-pwm-pulse", "spindle-pwm-dir", "spindle-pwm-enable"]

human_pwm_output_names =[ _("Unused PWM Gen"), _("X PWM Pulse Stream"), _("X PWM Direction"), _("X PWM Enable"), _("Y PWM Pulse Stream"), _("Y PWM Direction"), _("Y PWM Enable"), _("Z PWM Pulse Stream"), _("Z PWM Direction"), _("Z PWM Enable"), _("A PWM Pulse Stream"),
_("A PWM Direction"), _("A PWM Enable"), _("Spindle PWM Pulse Stream"), _("Spindle PWM Direction"), _("Spindle PWM Enable"),  ]

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
"spindle-encoder-a","spindle-encoder-b","spindle-encoder-i", "spindle-encoder-m",
"x-mpg-a","x-mpg-b", "x-mpg-i", "x-mpg-m", "y-mpg-a", "y-mpg-b", "y-mpg-i", "y-mpg-m",
"z-mpg-a","z-mpg-b", "z-mpg-i", "z-mpg-m", "a-mpg-a", "a-mpg-b", "a-mpg-i", "a-mpg-m",
"select-mpg-a", "select-mpg-b", "select-mpg-i", "select-mpg-m"]

human_encoder_input_names = [ _("Unused Encoder"), _("X Encoder-A Phase"), _("X Encoder-B Phase"), _("X Encoder-I Phase"),  _("X Encoder-M Phase"),
_("Y Encoder-A Phase"), _("Y Encoder-B Phase"), _("Y Encoder-I Phase"), _("Y Encoder-M Phase"), _("Z Encoder-A Phase"), _("Z Encoder-B Phase"), 
_("Z Encoder-I Phase"), _("Z Encoder-M Phase"), _("A Encoder-A Phase"), _("A Encoder-B Phase"), _("A Encoder-I Phase"), _("A Encoder-M Phase"),
_("Spindle Encoder-A Phase"), _("Spindle  Encoder-B Phase"), _("Spindle Encoder-I Phase"), _("Spindle Encoder-M Phase"), _("X Hand Wheel-A Phase"), 
_("X Hand Wheel-B Phase"), _("X Hand Wheel-I Phase"), _("X Hand Wheel-M Phase"), _("Y Hand wheel-A Phase"), _("Y Hand Wheel-B Phase"), 
_("Y Hand Wheel-I Phase"), _("Y Hand Wheel-M Phase"), _("Z Hand Wheel-A Phase"), _("Z Hand Wheel-B Phase"), _("Z Hand Wheel-I Phase"), 
_("Z Hand Wheel-M Phase"), _("A Hand Wheel-A Phase"), _("A Hand Wheel-B Phase"), _("A Hand Wheel-I Phase"), _("A Hand Wheel-M Phase"), 
_("Multi Hand Wheel-A Phase"), _("Multi Hand Wheel-B Phase"), _("Multi Hand Wheel-I Phase"), _("Multi Hand Wheel-M Phase")]

(UNUSED_STEPGEN, 
X_STEPGEN_STEP, X_STEPGEN_DIR, X_STEPGEN_PHC, X_STEPGEN_PHD, X_STEPGEN_PHE, X_STEPGEN_PHF,
Y_STEPGEN_STEP, X_STEPGEN_DIR, X_STEPGEN_PHC, X_STEPGEN_PHD, X_STEPGEN_PHE, X_STEPGEN_PHF,
Z_STEPGEN_STEP, Z_STEPGEN_DIR, Z_STEPGEN_PHC, Z_STEPGEN_PHD, Z_STEPGEN_PHE, Z_STEPGEN_PHF,
A_STEPGEN_STEP, A_STEPGEN_DIR, A_STEPGEN_PHC, A_STEPGEN_PHD, A_STEPGEN_PHE, A_STEPGEN_PHF,) = hal_stepper_names = ["unused-stepgen", 
"x-stepgen-step", "x-stepgen-dir", "x-stepgen-phase-c", "x-stepgen-phase-d", "x-stepgen-phase-e", "x-stepgen-phase-f", 
"y-stepgen-step", "y-stepgen-dir", "y-stepgen-phase-c", "y-stepgen-phase-d", "y-stepgen-phase-e", "y-stepgen-phase-f",
"z-stepgen-step", "z-stepgen-dir", "z-stepgen-phase-c", "z-stepgen-phase-d", "z-stepgen-phase-e", "z-stepgen-phase-f",
"a-stepgen-step", "a-stepgen-dir", "a-stepgen-phase-c", "a-stepgen-phase-d", "a-stepgen-phase-e", "a-stepgen-phase-f",]

human_stepper_names = [_("Unused StepGen"), _("X StepGen-Step"), _("X StepGen-Direction"), _("X reserved c"), _("X reserved d"), 
_("X reserved e"), _("X reserved f"), _("Y StepGen-Step"), _("Y StepGen-Direction"), _("Y reserved c"), _("Y reserved d"), _("Y reserved e"), 
_("Y reserved f"), _("Z StepGen-Step"), _("Z StepGen-Direction"), _("Z reserved c"), _("Z reserved d"), _("Z reserved e"), _("Z reserved f"), 
_("A StepGen-Step"), _("A StepGen-Direction"), _("A reserved c"), _("A reserved d"), _("A reserved e"), _("A reserved f"), ]


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
        self.help = 0
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
        self.mesa_currentfirmwaredata = mesafirmwaredata[4]
        self.mesa_boardname = "5i22"
        self.mesa_firmware = "SV16"
        self.mesa_maxgpio = 96
        self.mesa_isawatchdog = 1
        self.mesa_pwm_frequency = 100000
        self.mesa_watchdog_timeout = 10000000
        self.numof_mesa_encodergens = 4
        self.numof_mesa_pwmgens = 4
        self.numof_mesa_stepgens = 0
        self.numof_mesa_gpio = 72
        self.pp1_direction = 1 # output
        self.ioaddr = "0x378"
        self.ioaddr2 = _("Enter Address")
        self.pp2_direction = 0 # input
        self.ioaddr3 = _("Enter Address")
        self.pp3_direction = 0 # input
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

        self.halencoderinputsignames = []
        self.halpwmoutputsignames = []
        self.halinputsignames = []
        self.haloutputsignames = []
        self.halsteppersignames = []

        # pp1Ipin0
        # pp1Opin0inv
        # pp1Ipin0
        # For parport one two and three
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

        # for mesa cards
        connector = 2
        pinname ="m5i20c%dpin"% (connector)
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
        for connector in(3,4,5):
            # This initializes GPIO input pins
            for i in range(0,16):
                pinname ="m5i20c%dpin%d"% (connector,i)
                self[pinname] = UNUSED_INPUT
                pinname ="m5i20c%dpin%dtype"% (connector,i)
                self[pinname] = GPIOI
            # This initializes GPIO output pins
            for i in range(16,24):
                pinname ="m5i20c%dpin%d"% (connector,i)
                self[pinname] = UNUSED_OUTPUT
                pinname ="m5i20c%dpin%dtype"% (connector,i)
                self[pinname] = GPIOO
        for connector in(2,3,4,5):
            # This initializes the mesa inverse pins
            for i in range(0,24):
                pinname ="m5i20c%dpin%dinv"% (connector,i)
                self[pinname] = False

        # halui comand list
        for i in range(1,16):
                pinname ="halui_cmd%s"% i
                self[pinname] = ""

        self.xdrivertype = "other"
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
        self.xbias = 0
        self.xdeadband = 0
        self.xsteptime = 0
        self.xstepspace = 0
        self.xdirhold = 0
        self.xdirsetup = 0
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

        self.ydrivertype = "other"
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
        self.ybias = 0
        self.ydeadband = 0
        self.ysteptime = 0
        self.ystepspace = 0
        self.ydirhold = 0
        self.ydirsetup = 0
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
   
        self.zdrivertype = "other"     
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
        self.zbias = 0
        self.zdeadband = 0
        self.zsteptime = 0
        self.zstepspace = 0
        self.zdirhold = 0
        self.zdirsetup = 0
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


        self.adrivertype = "other"
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
        self.abias = 0
        self.adeadband = 0
        self.asteptime = 0
        self.astepspace = 0
        self.adirhold = 0
        self.adirsetup = 0
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
        
        # this loads signal names created by the user
        for i in  self.halencoderinputsignames:
            hal_encoder_input_names.append(i)
            human_encoder_input_names.append(i)
        for i in  self.halpwmoutputsignames:
            hal_pwm_output_names.append(i)
            human_pwm_output_names.append(i)
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
        print >>file, "DEBUG = 0"

        print >>file
        print >>file, "[DISPLAY]"
        if self.frontend == 1:
            print >>file, "DISPLAY = axis"
        elif self.frontend == 2:
            print >>file, "DISPLAY = tkemc"
        else:
            print >>file, "DISPLAY = mini"
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
        print >>file, "[HOSTMOT2]"
        print >>file, "DRIVER=hm2_pci"
        print >>file, "BOARD=%s"% self.mesa_boardname
        print >>file, """CONFIG="firmware=hm2/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d" """ % (
        self.mesa_boardname, self.mesa_firmware, self.numof_mesa_encodergens, self.numof_mesa_pwmgens, self.numof_mesa_stepgens )
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
        pwmgen = self.pwmgen_sig(letter)
        stepgen = self.stepgen_sig(letter)
        print >>file
        print >>file, "#********************"
        print >>file, "# Axis %s" % letter.upper()
        print >>file, "#********************"
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
        print >>file, "FERROR = %s"% get("maxferror")
        print >>file, "MIN_FERROR = %s" % get("minferror")
        if stepgen == "false":
            print >>file, "P = %s" % get("P")
            print >>file, "I = %s" % get("I") 
            print >>file, "D = %s" % get("D")
            print >>file, "FF0 = %s" % get("FF0")
            print >>file, "FF1 = %s" % get("FF1")
            print >>file, "FF2 = %s" % get("FF2")
            print >>file, "BIAS = %s"% get("bias") 
            print >>file, "DEADBAND = %s"% get("deadband")
            print >>file, "OUTPUT_SCALE = %s" % get("outputscale")
            print >>file, "OUTPUT_OFFSET = %s" % get("outputoffset")
            print >>file, "MAX_OUTPUT = %s" % get("maxoutput")
            print >>file, "INPUT_SCALE = %s" % get("scale")
        else:
            print >>file, "# these are in nanoseconds"
            print >>file, "DIRSETUP   = %d"% int(get("dirsetup"))
            print >>file, "DIRHOLD    = %d"% int(get("dirhold"))
            print >>file, "STEPLEN    = %d"% int(get("steptime"))          
            print >>file, "STEPSPACE  = %d"% int(get("stepspace"))            
            print >>file, "SCALE = %s"% get("scale")     
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
            if not self.findsignal(i) == "false": homes = True
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
                if not self.findsignal(i) == "false":
                    print >>file, "HOME_IGNORE_LIMITS = YES"
                    break
            if all_homes:
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
        axnum = "xyza".index(let)
        pwmgen = self.pwmgen_sig(let)
        print self.make_pinname(pwmgen)
        stepgen = self.stepgen_sig(let)
        print self.make_pinname(stepgen)
        encoder = self.encoder_sig(let)
        print self.make_pinname(encoder)
        homesig = self.home_sig(let)
        max_limsig = self.max_lim_sig(let)
        min_limsig = self.min_lim_sig(let)
        lat = self.latency
        print >>file
        print >>file, "#**************"
        print >>file, "#  Axis %s" % let.upper()
        print >>file, "#**************"
                   
        if stepgen == "false":
            print >>file, "    setp pid.%d.Pgain [AXIS_%d]P" % (num, axnum)
            print >>file, "    setp pid.%d.Igain [AXIS_%d]I" % (num, axnum)
            print >>file, "    setp pid.%d.Dgain [AXIS_%d]D" % (num, axnum)
            print >>file, "    setp pid.%d.bias [AXIS_%d]BIAS" % (num, axnum)
            print >>file, "    setp pid.%d.FF0 [AXIS_%d]FF0" % (num, axnum)
            print >>file, "    setp pid.%d.FF1 [AXIS_%d]FF1" % (num, axnum)
            print >>file, "    setp pid.%d.FF2 [AXIS_%d]FF2" % (num, axnum)
            print >>file, "    setp pid.%d.deadband [AXIS_%d]DEADBAND" % (num, axnum)
            print >>file, "    setp pid.%d.maxoutput [AXIS_%d]MAX_OUTPUT" % (num, axnum)
            print >>file
            if 'm5i20' in encoder:
                pinname = self.make_pinname(encoder)
                #TODO do a check to see if encoder sig is from parport or mesa
                #also the encoder # needs to reflect pin number not axis number
                print >>file, "# Encoder feedback signals/setup"
                print >>file
                print >>file, "    setp "+pinname+".counter-mode 0"
                print >>file, "    setp "+pinname+".filter 1" 
                print >>file, "    setp "+pinname+".index-invert 0"
                print >>file, "    setp "+pinname+".index-mask 0" 
                print >>file, "    setp "+pinname+".index-mask-invert 0"              
                print >>file, "    setp "+pinname+".scale  [AXIS_%d]INPUT_SCALE"% (axnum)
                print >>file, "net %spos-fb <= "% (let) +pinname+".position"
                print >>file, "net %spos-fb => pid.%d.feedback"% (let,axnum) 
                print >>file, "net %spos-fb => axis.%d.motor-pos-fb" % (let, axnum)   
                print >>file        
            if 'm5i20' in pwmgen:
                pinname = self.make_pinname(pwmgen)
                #TODO do a check to see if encoder sig is from parport or mesa
                print >>file, "# PWM Generator signals/setup"
                print >>file
                print >>file, "    setp "+pinname+".output-type 1" 
                print >>file, "    setp "+pinname+".scale  1.0" 
                print >>file, "net %senable     axis.%d.amp-enable-out => "% (let,axnum) +pinname+".enable"
                print >>file, "net %senable     pid.%d.enable" % (let, axnum) 
                print >>file, "net %spos-cmd    axis.%d.motor-pos-cmd => pid.%d.command" % (let, axnum , axnum)
                print >>file, "net %soutput     pid.%d.output  => "% (let, axnum) +pinname+ ".value"      
        if not stepgen == "false":
            pinname = self.make_pinname(stepgen)
            print >>file, "# Step Gen signals/setup"
            print >>file
            print >>file, "setp " + pinname + ".dirsetup        [AXIS_%d]DIRSETUP"% axnum
            print >>file, "setp " + pinname + ".dirhold         [AXIS_%d]DIRHOLD"% axnum
            print >>file, "setp " + pinname + ".steplen         [AXIS_%d]STEPLEN"% axnum
            print >>file, "setp " + pinname + ".stepspace       [AXIS_%d]STEPSPACE"% axnum
            print >>file, "setp " + pinname + ".position-scale  [AXIS_%d]SCALE"% axnum
            print >>file, "setp " + pinname + ".maxaccel        [AXIS_%d]MAX_ACCELERATION"% axnum
            print >>file, "setp " + pinname + ".maxvel          [AXIS_%d]MAX_VELOCITY"% axnum
            print >>file, "setp " + pinname + ".step_type       0"
            print >>file, "net %spos-cmd    axis.%d.motor-pos-cmd => "% (let, axnum) + pinname + ".position-cmd"
            print >>file, "net %spos-fb     "% let  + pinname + ".position-fb => axis.%d.motor-pos-fb" %  axnum
            print >>file, "net %senable     axis.%d.amp-enable-out => "% (let, axnum) + pinname + ".enable"        
        if not homesig =="false":
            print >>file, "net %s => axis.%d.home-sw-in" % (homesig, axnum)       
        if not min_limsig =="false":
            print >>file, "net %s => axis.%d.neg-lim-sw-in" % (min_limsig, axnum)       
        if not max_limsig =="false":
            print >>file, "net %s => axis.%d.pos-lim-sw-in" % (max_limsig, axnum)

    def connect_input(self, file):
        print >>file
        print >>file, "# external input signals"
        for q in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = self['pp1Ipin%d' % q]
            i = self['pp1Ipin%dinv' % q]
            if p == UNUSED_INPUT: continue
            if i: print >>file, "net %s <= parport.0.pin-%02d-in-not" % (p, q)
            else: print >>file, "net %s <= parport.0.pin-%02d-in" % (p, q)
        print >>file
        for connector in (2,3,4):
            board = self.mesa_boardname
            for q in range(0,24):
                p = self['m5i20c%dpin%d' % (connector, q)]
                i = self['m5i20c%dpin%dinv' % (connector, q)]
                t = self['m5i20c%dpin%dtype' % (connector, q)]
                truepinnum = q + ((connector-2)*24)
                # for input pins
                if t == GPIOI:
                    if p == "unused-input":continue 
                    pinname = self.make_pinname(self.findsignal( p )) 
                    if i: print >>file, "net %s <= "% (p)+pinname +".in_not"
                    else: print >>file, "net %s <= "% (p)+pinname +".in"
                # for encoder pins
                elif t in (ENCA,ENCB,ENCI,ENCM):
                    if p == "unused-encoder":continue
                    if p in (self.halencoderinputsignames): 
                        print >>file, "net %s <= hm2_%s.0.encoder.%02d"  % (p, board, truepinnum)    
                else: continue

    def connect_output(self, file):
        print >>file
        print >>file, "# external output signals"
        for q in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = self['pp1Opin%d' % q]
            i = self['pp1Opin%dinv' % q]
            if p == UNUSED_OUTPUT: continue
            print >>file, "net %s => parport.0.pin-%02d-out" % (p, q)
            if i: print >>file, "    setp parport.0.pin-%02d-out-invert true" % q           
        print >>file
        for connector in (2,3,4):
            for q in range(0,24):
                p = self['m5i20c%dpin%d' % (connector, q)]
                i = self['m5i20c%dpin%dinv' % (connector, q)]
                t = self['m5i20c%dpin%dtype' % (connector, q)]
                truepinnum = q + ((connector-2)*24)
                # for output /open drain pins
                if t in (GPIOO,GPIOD):
                    if p == "unused-output":continue
                    pinname = self.make_pinname(self.findsignal( p ))
                    print >>file, "    setp "+pinname +".is_output true"
                    print >>file, "net %s => "% (p)+pinname +".out"
                    if i: print >>file, "    setp "+pinname+".invert_output true"
                    if t == 2: print >>file, "    setp "+pinname+".is_opendrain  true"                 
                # for pwm pins
                elif t in (PWMP,PWMD,PWME,PDMP,PDMD,PDME):
                    if p == "unused-pwm":continue
                    if p in (self.halpwmoutputsignames): 
                        print >>file, "net %s <= hm2_%s.0.pwm.%02d"  % (p, board, truepinnum)  
                # for stepper pins
                elif t in (STEPA,STEPB):
                    if p == "unused-stepgen":continue
                    if p in (self.halsteppersignames): 
                        print >>file, "net %s <= hm2_%s.0.stepgen.%02d"  % (p, board, truepinnum) 
                else:continue

    def write_halfile(self, base):
        filename = os.path.join(base, self.machinename + ".hal")
        file = open(filename, "w")
        print >>file, _("# Generated by PNCconf at %s") % time.asctime()
        print >>file, _("# If you make changes to this file, they will be")
        print >>file, _("# overwritten when you run PNCconf again")
        print >>file
        print >>file, "loadrt trivkins"
        print >>file, "loadrt [EMCMOT]EMCMOT base_period_nsec=[EMCMOT]BASE_PERIOD servo_period_nsec=[EMCMOT]SERVO_PERIOD num_joints=[TRAJ]AXES"
        print >>file, "loadrt probe_parport"
        if self.mesa5i20>0:
            print >>file, "loadrt hostmot2"
            print >>file, "loadrt [HOSTMOT2](DRIVER) config=[HOSTMOT2](CONFIG)"
            if self.numof_mesa_pwmgens > 0:
                print >>file, "    setp hm2_[HOSTMOT2](BOARD).0.pwmgen.pwm_frequency %d"% self.mesa_pwm_frequency
            print >>file, "    setp hm2_[HOSTMOT2](BOARD).0.watchdog.timeout_ns %d"% self.mesa_watchdog_timeout

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
            if self.doublestep():
                print >>file, "    setp parport.0.reset-time %d" % self.steptime

        spindle_enc = counter = probe = pwm = pump = estop = False 
        enable = spindle_on = spindle_cw = spindle_ccw = False
        mist = flood = brake = False

        if not self.findsignal("spindle-phase-a") == "false":
            spindle_enc = True        
        if self.findsignal("spindle-phase-b") =="false":
            counter = True
        if not self.findsignal("probe") =="false":
            probe = True
        if not self.findsignal("spindle-pwm") =="false":
            pwm = True
        if not self.findsignal("pump") =="false":
            pump = True
        if not self.findsignal("estop-ext") =="false":
            estop = True
        if not self.findsignal("enable") =="false":
            enable = True
        if not self.findsignal("spindle-on") =="false":
            spindle_on = True
        if not self.findsignal("spindle-cw") =="false":
            spindle_cw = True
        if not self.findsignal("spindl-ccw") =="false":
            spindle_ccw = True
        if not self.findsignal("mist") =="false":
            mist = True
        if not self.findsignal("flood") =="false":
            flood = True
        if not self.findsignal("brake") =="false":
            brake = True

        if self.axes == 2:
            #print >>file, "loadrt stepgen step_type=0,0"
            print >>file, "loadrt pid num_chan=2"
        elif self.axes == 1:
            #print >>file, "loadrt stepgen step_type=0,0,0,0"
            print >>file, "loadrt pid num_chan=4"
        else:
            #print >>file, "loadrt stepgen step_type=0,0,0"
            print >>file, "loadrt pid num_chan=3"

        if spindle_enc:
            print >>file, "loadrt encoder num_chan=1"
        if self.pyvcphaltype == 1 and self.pyvcpconnect == 1:
            print >>file, "loadrt abs count=1"
            if spindle_enc:
               print >>file, "loadrt scale count=1"

        if pump:
            print >>file, "loadrt charge_pump"
            print >>file, "net estop-out charge-pump.enable iocontrol.0.user-enable-out"
            print >>file, "net charge-pump <= charge-pump.out"

        if pwm:
            print >>file, "loadrt pwmgen output_type=0"

        if self.classicladder:
            print >>file, "loadrt classicladder_rt numPhysInputs=%d numPhysOutputs=%d numS32in=%d numS32out=%d numFloatIn=%d numFloatOut=%d" %(self.digitsin , self.digitsout , self.s32in, self.s32out, self.floatsin, self.floatsout)

        if self.pyvcp and not self.frontend == 1:
            print >>file, "loadusr -Wn custompanel pyvcp -c custompanel [DISPLAY](PYVCP)"
        print >>file
        if self.number_pports > 0:
            print >>file, "addf parport.0.read base-thread"
        if self.number_pports > 1:
            print >>file, "addf parport.1.read base-thread"
        if self.number_pports > 2:
            print >>file, "addf parport.2.read base-thread"

        #print >>file, "addf stepgen.make-pulses base-thread"
        if spindle_enc: print >>file, "addf encoder.update-counters base-thread"
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
            print >>file, "addf hm2_[HOSTMOT2](BOARD).0.read servo-thread" 
        #print >>file, "addf stepgen.capture-position servo-thread"
        if spindle_enc: print >>file, "addf encoder.capture-position servo-thread"
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
            if spindle_enc:
               print >>file, "addf scale.0 servo-thread"
        if self.mesa5i20>0:
            print >>file, "addf hm2_[HOSTMOT2](BOARD).0.write         servo-thread" 
            print >>file, "addf hm2_[HOSTMOT2](BOARD).0.pet_watchdog  servo-thread"
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
            print >>file, "    setp pwmgen.0.pwm-freq %s" % self.spindlecarrier        
            print >>file, "    setp pwmgen.0.scale %s" % scale
            print >>file, "    setp pwmgen.0.offset %s" % offset
            print >>file, "    setp pwmgen.0.dither-pwm true"
        else: 
            print >>file, "net spindle-cmd <= motion.spindle-speed-out"

        if spindle_on:
            print >>file, "net spindle-on <= motion.spindle-on"
        if spindle_cw:
            print >>file, "net spindle-cw <= motion.spindle-forward"
        if spindle_ccw:
            print >>file, "net spindle-ccw <= motion.spindle-reverse"
        if brake:
            print >>file, "net spindle-brake <= motion.spindle-brake"

        if mist:
            print >>file, "net coolant-mist <= iocontrol.0.coolant-mist"

        if flood:
            print >>file, "net coolant-flood <= iocontrol.0.coolant-flood"

        if spindle_enc:
            print >>file
            if counter:
                print >>file, "    setp encoder.0.position-scale %f"\
                     % self.spindlecpr
                print >>file, "    setp encoder.0.counter-mode 1"
            else:
                print >>file, "    setp encoder.0.position-scale %f" \
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
            if not self.findsignal(dout) =="false":
                print >>file, "net %s <= motion.digital-out-%02d" % (dout, i)

        for i in range(4):
            din = "din-%02d" % i
            if not self.findsignal(din) =="false":
                print >>file, "net %s => motion.digital-in-%02d" % (din, i)

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
        print >>file, _("#  estop signals")
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
        elif estop:
            print >>file, "net estop-ext => iocontrol.0.emc-enable-in"
        else:
            print >>file, "net estop-out => iocontrol.0.emc-enable-in"
        if enable:
             print >>file, "net enable => motion.motion-enabled"

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
                  print >>f1, _("# **** Setup of spindle speed and tool number display using pyvcp -START ****")
                  if spindle_enc:
                      print >>f1, _("# **** Use ACTUAL spindle velocity from spindle encoder")
                      print >>f1, _("# **** spindle-velocity is signed so we use absolute compoent to remove sign") 
                      print >>f1, _("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
                      print >>f1
                      print >>f1, ("setp scale.0.gain .01667")
                      print >>f1, ("net spindle-velocity => abs.0.in")
                      print >>f1, ("net absolute-spindle-vel <= abs.0.out => scale.0.in")
                      print >>f1, ("net scaled-spindle-vel <= scale.0.out => pyvcp.spindle-speed")
                  else:
                      print >>f1, _("# **** Use COMMANDED spindle velocity from EMC because no spindle encoder was specified")
                      print >>f1, _("# **** COMANDED velocity is signed so we use absolute component (abs.0) to remove sign")
                      print >>f1
                      print >>f1, ("net spindle-cmd => abs.0.in")
                      print >>f1, ("net absolute-spindle-vel <= abs.0.out => pyvcp.spindle-speed")                     
                  print >>f1, ("net tool-number => pyvcp.toolnumber")
                  print >>f1
                  print >>f1, _("# **** Setup of spindle speed and tool number display using pyvcp -END ****")
            if self.pyvcphaltype == 2 and self.pyvcpconnect: # Hal_UI example
                      print >>f1, _("# **** Setup of pyvcp buttons and MDI commands using HAL_UI and pyvcp - START ****")
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
                      print >>f1, _("# **** The following mdi-comands are specified in the machine named INI file under [HALUI] heading")
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
                      print >>f1, _("# **** Setup of pyvcp buttons and MDI commands using HAL_UI and pyvcp - END ****")

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
        print >>file,_("Mesa 5i20 connector 2 ")
        for x in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
            temp = self["m5i20c2pin%d" % x]
            tempinv = self["m5i20c2pin%dinv" %  x]
            temptype = self["m5i20c2pin%dtype" %  x]
            if tempinv: 
                invmessage = _("-> inverted")
            else: invmessage =""
            print >>file,("pin# %(pinnum)d (type %(type)s) is connected to signal:'%(data)s'%(mess)s" %{
            'type':temptype,'pinnum':x, 'data':temp, 'mess':invmessage}) 
        print >>file
        print >>file,_("Mesa 5i20 connector 3 ")
        for x in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
            temp = self["m5i20c3pin%d" % x]
            tempinv = self["m5i20c3pin%dinv" %  x]
            temptype = self["m5i20c3pin%dtype" %  x]
            if tempinv: 
                invmessage = _("-> inverted")
            else: invmessage =""
            print >>file,("pin# %(pinnum)d (type %(type)s) is connected to signal:'%(data)s'%(mess)s " %{ 
            'type':temptype, 'pinnum':x, 'data':temp,   'mess':invmessage}) 
        print >>file
        print >>file,_("Mesa 5i20 connector 4 ")
        for x in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23):
            temp = self["m5i20c4pin%d" % x]
            tempinv = self["m5i20c4pin%dinv" %  x]
            temptype = self["m5i20c4pin%dtype" %  x]
            if tempinv: 
                invmessage = _("-> inverted")
            else: invmessage =""
            print >>file,("pin# %(pinnum)d (type %(type)s) is connected to signal:'%(data)s'%(mess)s" %{
            'type':temptype,'pinnum':x, 'data':temp, 'mess':invmessage}) 
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

    # This method returns I/O pin designation (name and number) of a given HAL signalname.
    # It does not check to see if the signalname is in the list more then once.
    def findsignal(self, sig):
        ppinput = {}
        ppoutput = {}
        for i in (1,2,3):
            for s in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                key = self["pp%dIpin%d" %(i,s)]
                ppinput[key] = "pp%dIpin%d" %(i,s) 
            for s in (1,2,3,4,5,6,7,8,9,14,16,17):
                key = self["pp%dOpin%d" %(i,s)]
                ppoutput[key] = "pp%dOpin%d" %(i,s) 

        mesa2=dict([(self["m5i20c2pin%d" %s],"m5i20c2pin%d" %s) for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23)])
        mesa3=dict([(self["m5i20c3pin%d" %s],"m5i20c3pin%d" %s) for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23)])
        mesa4=dict([(self["m5i20c4pin%d" %s],"m5i20c4pin%d" %s) for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23)])
        mesa5=dict([(self["m5i20c5pin%d" %s],"m5i20c5pin%d" %s) for s in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23)])
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
                            try:
                                return mesa5[sig]
                            except :
                                return "false"

    # This method takes a signalname data pin (eg m5i20c3pin1)
    # and converts it to a HAL pin names (eg hm2_[HOSTMOT2](BOARD).0.gpio.01)
    # The adj variable is for adjustment of position of pins related to the
    # 'controlling pin' eg encoder-a (controlling pin) encoder-b encoder -I
    # (related pins) 
    def make_pinname(self, pin):
        test = str(pin)       
        if 'm5i20' in test:
            ptype = self[pin+"type"] 
            signalname = self[pin]
            print signalname
            pinnum = int(test[10:])
            connum = int(test[6:7])
            type_name = { GPIOI:"gpio", GPIOO:"gpio", GPIOD:"gpio", ENCA:"encoder", ENCB:"encoder",ENCI:"encoder",ENCM:"encoder", PWMP:"pwmgen", PWMD:"pwmgen", PWME:"pwmgen", PDMP:"pwmgen", PDMD:"pwmgen", PDME:"pwmgen",STEPA:"stepgen", STEPB:"stepgen" }
            try:
                comptype = type_name[ptype]
            except :
                comptype = "false"
            
            #print test,self[pin], ptype, pinnum
            # GPIO pins truenumber can be any number between 0 and 72 for 5i20 ( 96 in 5i22)
            if ptype in(GPIOI,GPIOO,GPIOD):
                truepinnum = int(pinnum)+(int(connum)-2)*24
                return "hm2_[HOSTMOT2](BOARD).0."+comptype+".%03d"% truepinnum 
            
            # Encoder 
            elif ptype in (ENCA,ENCB,ENCI,ENCM):
                adj = 0
                if ptype == ENCB:adj = -1
                if ptype == ENCI:
                    adj = 2
                    if pinnum in(1,13):adj = 3
                if pinnum ==  3 + adj:truepinnum = 0 +((connum-2)*4) 
                elif pinnum == 1 + adj:truepinnum = 1 +((connum-2)*4)
                elif pinnum == 15 + adj:truepinnum = 2 +((connum-2)*4)
                elif pinnum == 13 + adj:truepinnum = 3 +((connum-2)*4) 
                else:print "(encoder) pin number error pinnum = %d"% pinnum
            # PWMGen pins
            elif ptype in (PWMP,PWMD,PWME):
                adj = 0
                if signalname.endswith('dir'):adj = 2
                if signalname.endswith('enable'):adj = 4         
                if pinnum == 6 + adj:truepinnum = 0 +((connum-2)*4) 
                elif pinnum == 7 + adj:truepinnum = 1 +((connum-2)*4)
                elif pinnum == 18 + adj:truepinnum = 2 +((connum-2)*4)  
                elif pinnum == 19 + adj:truepinnum = 3 +((connum-2)*4) 
                else:print "(pwm) pin number error pinnum = %d"% pinnum
            # StepGen pins 
            elif ptype in (STEPA,STEPB):
                adj = 0
                if signalname.endswith('dir'):adj = 1
                if signalname.endswith('c'):adj = 2
                if signalname.endswith('d'):adj = 3
                if signalname.endswith('e'):adj = 4
                if signalname.endswith('f'):adj = 5
                if pinnum == 0 + adj:truepinnum = 0 
                elif pinnum == 6 + adj:truepinnum = 1 
                elif pinnum == 12 + adj:truepinnum = 2 
                elif pinnum == 18 + adj:truepinnum = 3
                else:print "(step) pin number error pinnum = %d"% pinnum
            else: print "pintype error"
            return "hm2_[HOSTMOT2](BOARD).0."+comptype+".%02d"% (truepinnum)
        elif 'pp' in test:
            print test
            ending = "-out"
            test = str(pin) 
            print  self[pin]
            pintype = str(test[3:4])
            pinnum = int(test[7:])
            connum = int(test[2:3])-1
            if pintype == 'I': ending = "-in"
            return "parport."+str(connum)+".pin-"+str(pinnum)+ending
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

    def __init__(self):
        gnome.init("pncconf", "0.6") 
        glade = xml.dom.minidom.parse(os.path.join(datadir, self.fname))
        self.make_axispage(glade, 'y')
        self.make_axispage(glade, 'z')
        self.make_axispage(glade, 'a')
        self.make_axismotorpage(glade, 'y')
        self.make_axismotorpage(glade, 'z')
        self.make_axismotorpage(glade, 'a')
        self.make_pportpage(glade, 'pp2')
        self.make_pportpage(glade, 'pp3')
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

    def test_callback(self,widget,data):
        print "pin designation-",data

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

    def on_druid1_help(self, *args):
        num = self.data.help
        text=_("I need to add help page %d"% num)
        if num == 1:
            text =_("""
Check 'desktop shortcut' to create a link on the desktop to the folder containing the configuration files
Check 'desktop launcher' to create a link on the desktop that will directly start your custom configuration""")
        self.warning_dialog(text,True)
       

    def on_page_newormodify_prepare(self, *args):
        self.data.help = 1
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
        self.data.help = 2
        self.widgets.machinename.set_text(self.data.machinename)
        self.widgets.axes.set_active(self.data.axes)
        self.widgets.units.set_active(self.data.units)
        self.widgets.latency.set_value(self.data.latency)
        self.widgets.machinename.grab_focus()
        self.widgets.mesa5i20_checkbutton.set_active(self.data.mesa5i20)
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
           self.warning_dialog(_("You need to designate a parport and/or mesa I/O device before continuing."),True)
           self.widgets.druid1.set_page(self.widgets.basicinfo)
           return True 
        self.data.pp1_direction = self.widgets.pp1_direction.get_active()
        self.data.pp2_direction = self.widgets.pp2_direction.get_active()
        self.data.pp3_direction = self.widgets.pp3_direction.get_active()
        self.data.limitshared = self.widgets.limittype_shared.get_active()
        self.data.limitsnone = self.widgets.limittype_none.get_active()
        self.data.limitswitch = self.widgets.limittype_switch.get_active()
        self.data.limitshared = self.widgets.limittype_shared.get_active()
        self.data.homenone = self.widgets.home_none.get_active()
        self.data.homeindex = self.widgets.home_index.get_active()
        self.data.homeswitch = self.widgets.home_switch.get_active()
        self.data.homeboth = self.widgets.home_both.get_active()
        
        # connect signals with pin designation data to mesa signal comboboxes and pintype comboboxes
        for connector in (2,3,4,5):
            for pin in range(0,24):
                cb="m5i20c%ipin%i"% (connector,pin)
                i= "mesasignalhandlerc%ipin%i"% (connector,pin)
                self.data[i] = int(self.widgets[cb].connect("changed", self.on_mesa_pin_changed,connector,pin))
                cb="m5i20c%ipin%itype"% (connector,pin)
                self.widgets[cb].connect("changed", self.on_mesa_pintype_changed,connector,pin)
        # add this here to speed up showing of mesa page
        model = self.widgets.mesa_boardname.get_model()
        model.clear()
        for i in mesaboardnames:
            model.append((i,))      
        for search,item in enumerate(mesaboardnames):
            if mesaboardnames[search]  == self.data.mesa_boardname:
                self.widgets.mesa_boardname.set_active(search)  
        model = self.widgets.mesa_firmware.get_model()
        model.clear()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[0] == self.data.mesa_boardname:continue
            model.append((d[1],))        
        for search,item in enumerate(model):           
            if model[search][0]  == self.data.mesa_firmware:
                self.widgets.mesa_firmware.set_active(search)     
        self.widgets.mesa_pwm_frequency.set_value(self.data.mesa_pwm_frequency)
        self.widgets.mesa_watchdog_timeout.set_value(self.data.mesa_watchdog_timeout)
        self.widgets.numof_mesa_encodergens.set_value(self.data.numof_mesa_encodergens)
        self.widgets.numof_mesa_pwmgens.set_value(self.data.numof_mesa_pwmgens)
        self.widgets.numof_mesa_stepgens.set_value(self.data.numof_mesa_stepgens)
        self.widgets.numof_mesa_gpio.set_text("%d" % self.data.numof_mesa_gpio)

        numofpwmgens = self.data.numof_mesa_pwmgens
        numofstepgens = self.data.numof_mesa_stepgens
        numofencoders = self.data.numof_mesa_encodergens 
        board = self.data.mesa_boardname 
        firmware = self.data.mesa_firmware 
        self.set_mesa_options(board,firmware,numofpwmgens,numofstepgens,numofencoders)

    def on_machinename_changed(self, *args):
        self.widgets.confdir.set_text(
            "~/emc2/configs/%s" % self.widgets.machinename.get_text())

    def on_GUI_config_prepare(self, *args):
        self.data.help = 3
        self.widgets.manualtoolchange.set_active(self.data.manualtoolchange)
        if self.data.frontend == 1 : self.widgets.GUIAXIS.set_active(True)
        elif self.data.frontend == 2: self.widgets.GUITKEMC.set_active(True)
        else:   self.widgets.GUIMINI.set_active(True)
        self.widgets.pyvcp.set_active(self.data.pyvcp)
        self.on_pyvcp_toggled()
        if  not self.widgets.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/emc2/configs/%s/custompanel.xml" % self.data.machinename)):
                self.widgets.pyvcpexist.set_active(True)
        self.widgets.pyvcpconnect.set_active(self.data.pyvcpconnect)

    def on_GUI_config_next(self, *args):
        if self.widgets.GUIAXIS.get_active():
           self.data.frontend = 1
        elif self.widgets.GUITKEMC.get_active():
           self.data.frontend = 2
        else:
            self.data.frontend = 3
        self.data.manualtoolchange = self.widgets.manualtoolchange.get_active()
        if not self.data.mesa5i20:
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

    def on_mesa_boardname_changed(self, *args):
        board = self.widgets.mesa_boardname.get_active_text()
        model = self.widgets.mesa_firmware.get_model()
        model.clear()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[0] == board:continue
            model.append((d[1],))
        
        self.widgets.mesa_firmware.set_active(0)  
        self.on_mesa_firmware_changed()

    def on_mesa_firmware_changed(self, *args):
        board = self.widgets.mesa_boardname.get_active_text()
        firmware = self.widgets.mesa_firmware.get_active_text()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[0] == board:continue
            if d[1] == firmware:
                self.widgets.numof_mesa_encodergens.set_range(0,d[2])
                self.widgets.numof_mesa_encodergens.set_value(d[2])
                self.widgets.numof_mesa_pwmgens.set_range(0,d[3])
                self.widgets.numof_mesa_pwmgens.set_value(d[3])
                self.widgets.numof_mesa_stepgens.set_range(0,d[4])
                self.widgets.numof_mesa_stepgens.set_value(d[4])
            self.on_gpio_update()

    def on_gpio_update(self, *args):
        board = self.widgets.mesa_boardname.get_active_text()
        firmware = self.widgets.mesa_firmware.get_active_text()
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[0] == board:continue
            if d[1] == firmware:      
                i = (int(self.widgets.numof_mesa_pwmgens.get_value()) * 3)
                j = (int(self.widgets.numof_mesa_stepgens.get_value()) * d[6])
                k = (int(self.widgets.numof_mesa_encodergens.get_value()) * d[5])
                total = (d[8]-i-j-k)
                self.widgets.numof_mesa_gpio.set_text("%d" % total)


    def on_mesa5i20_prepare(self, *args):
        self.data.help = 4
        #self.in_mesa_prepare = True      
        #self.in_mesa_prepare = False
  
    # This method converts data from the GUI to signal names for mesa data
    # It starts by checking pin type to set up the proper lists to search
    # then depending on the pin type widget data is converted to signal names.
    # if the signal name is not in the list add it to Human_names, signal_names
    # and disc-saved signalname lists
    # if encoder, pwm, or stepper pins the related pin are also set properly
    # eg if pin 0 is [encoder-A} then pin 2 is set to [encoder -B] and
    # pin 4 to [encoder-C]   
    def on_mesa5i20_next(self,*args):
        for connector in self.data.mesa_currentfirmwaredata[9] :
            for pin in range(0,24):
                foundit = 0
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                pinv = 'm5i20c%(con)dpin%(num)dinv' % {'con':connector ,'num': pin}
                ptype = 'm5i20c%(con)dpin%(num)dtype' % {'con':connector ,'num': pin}
                pintype = self.widgets[ptype].get_active_text()
                selection = self.widgets[p].get_active_text()
                if pintype in (ENCB,ENCI,ENCM,PWMD,PWME,STEPB): continue
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
                    if not pin in (1,3,13,15):
                        continue
                    nametocheck = human_encoder_input_names
                    signaltocheck = hal_encoder_input_names
                    addsignalto = self.data.halencoderinputsignames
                # type PWM gen
                elif pintype == PWMP:
                    if not pin in (6,7,18,19):continue
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                # type step gen
                elif pintype == STEPA:
                    if not pin in (0,6,12,18):continue
                    nametocheck = human_stepper_names
                    signaltocheck = hal_stepper_names
                    addsignalto = self.data.halsteppersignames
                else :
                    print "error unknown pin type"
                    return
                # check apropriote signal array for current signalname
                # if not found, user made a new signalname -add it to array
                for index , i in enumerate(nametocheck):
                    if selection == i : 
                        foundit = True
                        print "found it",nametocheck[index],"in ",p,"\n"
                        break         
                # **Start widget to data Convertion**                    
                # for encoder pins
                if pintype == ENCA :
                    if not foundit:
                        print " adding encoder pinname\n"
                        model = self.widgets[p].get_model()
                        model.append((selection+"-a",))
                        self.widgets[p].set_active( len(model))
                        index = index +1
                        for ending in ("-a","-b","-i","-m"):
                            signaltocheck.append ((selection + ending))
                            nametocheck.append ((selection + ending))
                            addsignalto.append ((selection + ending))
                    # set related encoder pins
                    flag = 1
                    if selection == "Unused Encoder":flag = 0
                    if pin in (1,13):
                        d = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin-1}
                        self.data[d] = signaltocheck[(index+1)*flag]
                        d = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+3}
                        self.data[d] = signaltocheck[(index+2)*flag]
                    elif pin in (3,15):
                        d = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin-1}
                        self.data[d] = signaltocheck[(index+1)*flag]
                        d = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+2}
                        self.data[d] = signaltocheck[(index+2)*flag]  
                    else:
                        print"Encoder pin config error"
                        continue
                    if self.data.mesa_currentfirmwaredata[5] == 4:                           
                            for count, name in enumerate((1,3,13,15)):
                                if name == pin:
                                    if connector == 3: count=count+4
                                    d = 'm5i20c%(con)dpin%(num)d' % {'con':4 ,'num': count}
                                    self.data[d] = signaltocheck[(index+3)*flag]
                # for PWM pins
                elif pintype == PWMP :
                    if not foundit:
                        model = self.widgets[p].get_model()
                        model.append((selection+"-pulse",))
                        index = index +1
                        for ending in ("-pulse","-dir","-enable"):
                            signaltocheck.append ((selection + ending))
                            nametocheck.append ((selection + ending))
                            addsignalto.append ((selection + ending))
                    # set related pwm pins
                    flag = 1
                    if selection == "Unused PWM Gen":flag = 0
                    if pin in (6,7,18,19):
                        d = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+2}
                        self.data[d] = signaltocheck[(index+1)*flag]
                        d = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+4}
                        self.data[d] = signaltocheck[(index+2)*flag]
                    else:
                        print "PWM pin config error"
                        continue
                    # for stepgen pins
                elif pintype == STEPA :
                    if not foundit:
                        model = self.widgets[p].get_model()
                        model.append((selection+"-step",))
                        index = index +1
                        for ending in ("-step","-dir"):
                            signaltocheck.append ((selection + ending))
                            nametocheck.append ((selection + ending))
                            addsignalto.append ((selection + ending))
                    # set related stepgen pins
                    flag = 1
                    if selection == "Unused StepGen":flag = 0
                    d = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+1}
                    self.data[d] = signaltocheck[(index+1)*flag]
                    
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
                # ** set data from widget for current pin
                self.data[p] = signaltocheck[index]
                self.data[pinv] = self.widgets[pinv].get_active()
        if self.data.number_pports<1:
           self.widgets.druid1.set_page(self.widgets.xaxismotor)
           return True

    def on_m5i20panel_clicked(self, *args):self.m5i20test(self)
    
    def on_mesa_pintype_changed(self, widget,connector,pin):
         
               # if self.in_mesa_prepare == True: return
                print "got to pintype change method ",connector,pin,"\n"
         
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                ptype = 'm5i20c%(con)dpin%(num)dtype' % {'con':connector ,'num': pin}    
                old = self.data[ptype]
                new = self.widgets[ptype].get_active_text()    
                if new == None :return 
                if old == GPIOI and new in (GPIOO,GPIOD):
                    print "switch GPIO input ",p," to output",new
                    model = self.widgets[p].get_model()
                    blocksignal = "mesasignalhandlerc%ipin%i"% (connector,pin)  
                    self.widgets[p].handler_block(self.data[blocksignal])
                    model.clear()
                    for name in human_output_names: model.append((name,))
                    self.widgets[p].handler_unblock(self.data[blocksignal])  
                    self.widgets[p].set_active(0)
                    self.data[p] = UNUSED_OUTPUT
                    self.data[ptype] = new
                elif old in (GPIOO,GPIOD) and new == GPIOI:
                    print "switch GPIO output ",p,"to input"
                    model = self.widgets[p].get_model()
                    model.clear()
                    blocksignal = "mesasignalhandlerc%ipin%i"% (connector,pin)  
                    self.widgets[p].handler_block(self.data[blocksignal])              
                    for name in human_input_names:
                        if self.data.limitshared or self.data.limitsnone:
                            if name in human_names_limit_only: continue 
                        if self.data.limitswitch or self.data.limitsnone:
                            if name in human_names_shared_home: continue                          
                        if self.data.homenone or self.data.limitshared:
                            if name in (_("Home X"), _("Home Y"), _("Home Z"), _("Home A"),_("All home")): continue
                        model.append((name,))
                    self.widgets[p].handler_unblock(self.data[blocksignal])  
                    self.widgets[p].set_active(0)
                    self.data[p] = UNUSED_INPUT
                    self.data[ptype] = new
                elif (old == GPIOI and new == GPIOD) :
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
                else: print "pintype error in pinchanged method old,new ",old,new,"\n"

    def on_mesa_component_value_changed(self, *args):
        self.in_mesa_prepare = True
        self.data.mesa_pwm_frequency = self.widgets.mesa_pwm_frequency.get_value()
        self.data.mesa_watchdog_timeout = self.widgets.mesa_watchdog_timeout.get_value()
        numofpwmgens = self.data.numof_mesa_pwmgens = int(self.widgets.numof_mesa_pwmgens.get_value())
        numofstepgens = self.data.numof_mesa_stepgens = int(self.widgets.numof_mesa_stepgens.get_value())
        numofencoders = self.data.numof_mesa_encodergens = int(self.widgets.numof_mesa_encodergens.get_value())
        board = self.data.mesa_boardname = self.widgets.mesa_boardname.get_active_text()
        firmware = self.data.mesa_firmware = self.widgets.mesa_firmware.get_active_text()
        self.set_mesa_options(board,firmware,numofpwmgens,numofstepgens,numofencoders)


    def set_mesa_options(self,board,firmware,numofpwmgens,numofstepgens,numofencoders):
        for search, item in enumerate(mesafirmwaredata):
            d = mesafirmwaredata[search]
            if not d[0] == board:continue
            if d[1] == firmware:
                self.data.mesa_currentfirmwaredata = mesafirmwaredata[search]
            if self.data.mesa_currentfirmwaredata[0] == "5i22":
                self.widgets.con5tab.set_sensitive(1)
                self.widgets.con5table.set_sensitive(1)
            else:
                self.widgets.con5tab.set_sensitive(0)
                self.widgets.con5table.set_sensitive(0)
            if self.data.mesa_currentfirmwaredata[0] == "7i43":
                self.widgets.con2table.set_sensitive(0)
                self.widgets.con2tab.set_sensitive(0)
            else:
                self.widgets.con2table.set_sensitive(1) 
                self.widgets.con2tab.set_sensitive(1)
        for concount,connector in enumerate(self.data.mesa_currentfirmwaredata[9]) :
            for pin in range (0,24):
                firmptype,compnum = self.data.mesa_currentfirmwaredata[10+pin+(concount*24)]
                print self.data.mesa_currentfirmwaredata[0]
                print self.data.mesa_currentfirmwaredata[1]
                print firmptype,"firmtype\n",compnum,"pinnum ",pin,",concount ",concount,"\n"
                
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                ptype = 'm5i20c%(con)dpin%(num)dtype' % {'con':connector ,'num': pin}
                pinv = 'm5i20c%(con)dpin%(num)dinv' % {'con':connector ,'num': pin}
                blocksignal = "mesasignalhandlerc%ipin%i" % (connector,pin) 
                #firmptype = self.data[ptype]
                # check to see if widget is GPIO or component firmware allows
                print  self.widgets[ptype].get_active_text(), firmptype
               
                # convert widget[ptype] to component specified in firmwaredata                      
                # add human names to widget removing signalnames specified in homing limit and spindle
                #print "TODO add human names\n "
                #self.widgets[pinv].set_active(self.data[pinv])
                # signal names for encoder 
                if firmptype in ( ENCA,ENCB,ENCI,ENCM ): 
                    #print numofencoders,compnum+1,"pinnnum ",pin,"\n"
                    if numofencoders >= (compnum+1):
                        if not self.widgets[ptype].get_active_text() == firmptype: 
                            #print "converting to encoder \n"
                            model = self.widgets[ptype].get_model()
                            model.clear() 
                            model.append((firmptype,))
                            self.widgets[ptype].set_active(0)
                            model = self.widgets[p].get_model()
                            model.clear()
                            self.widgets[pinv].set_sensitive(0)
                            # This sets up the 'controlling' combobox (signal phase A) 
                            if firmptype == ENCA: 
                                #print " encoder phase A \n"
                                temp = -1
                                self.widgets[p].handler_block(self.data[blocksignal]) 
                                for name in human_encoder_input_names:                      
                                    temp = temp +1
                                    if temp in (2,3): continue
                                    if temp == 4:
                                        temp = 0
                                        continue
                                    model.append((name,))
                                self.widgets[p].set_active(0)
                                self.widgets[p].set_sensitive(1)
                                self.widgets[ptype].set_sensitive(1)
                                self.widgets[p].handler_unblock(self.data[blocksignal])
                            # This sets up the 'following' combobox (signal phase B and I)
                            if firmptype in(ENCB,ENCI,ENCM):
                                #print " encoder phase B or I or M\n"                            
                                self.widgets[p].handler_block(self.data[blocksignal]) 
                                for name in human_encoder_input_names:model.append((name,)) 
                                self.widgets[p].handler_unblock(self.data[blocksignal])  
                                self.widgets[p].set_sensitive(0)
                                self.widgets[ptype].set_sensitive(0)
                                self.widgets[p].set_active(0)  
                        if self.data[ptype] in (ENCA,ENCB,ENCI,ENCM): 
                            #print self.data[p]
                            model = self.widgets[p].get_model()
                            for search,item in enumerate(model):
                                if model[search][0]  == human_encoder_input_names[hal_encoder_input_names.index(self.data[p])]:
                                    self.widgets[p].set_active(search)
                                else:print "unknown type in component_changed method -encoder\n"   
                        else:
                            self.data[p] =  UNUSED_ENCODER
                            self.data[ptype] = firmptype
                            self.widgets[p].set_active(0)  
                            #print "changed",p," to Encoder"    
                        continue                
                        #self.data[p] =  UNUSED_INPUT
                    else:   
                        #print "asking for GPIO instead of ENCODER\n"
                        firmptype = GPIOI
                # signal names for PWM
                elif firmptype in ( PWMP,PWMD,PWME ):
                    #print numofpwmgens,compnum+1,"pinnnum ",pin,"\n"
                    if numofpwmgens >= (compnum+1):
                        if not self.widgets[ptype].get_active_text() == firmptype:
                            #print "converting to pwm \n"
                            model = self.widgets[ptype].get_model()
                            model.clear() 
                            model.append((firmptype,))
                            temp = pintype_names[12]
                            model.append((temp,))
                            self.widgets[ptype].set_active(0)
                            model = self.widgets[p].get_model()
                            model.clear()
                            self.widgets[pinv].set_sensitive(0)
                            if firmptype == PWMP:
                                temp = -1
                                self.widgets[p].handler_block(self.data[blocksignal])
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
                                self.widgets[p].handler_unblock(self.data[blocksignal])
                            if firmptype in (PWMD,PWME):
                                self.widgets[p].set_sensitive(0)
                                self.widgets[p].handler_block(self.data[blocksignal])
                                for name in human_pwm_output_names: model.append((name,))
                                self.widgets[p].handler_unblock(self.data[blocksignal])
                                self.widgets[p].set_active(0) 
                                self.widgets[ptype].set_sensitive(0)
                        if self.data[ptype] in (PWMP,PWMD,PWME,PDMP,PDMD,PDME): 
                            #print self.data[p]
                            model = self.widgets[p].get_model()
                            for search,item in enumerate(model):
                                if model[search][0]  == human_pwm_output_names[hal_pwm_output_names.index(self.data[p])]:
                                    self.widgets[p].set_active(search)
                                else:print "unknown type in component_changed method -pwm\n"   
                        else:
                            self.data[p] =  UNUSED_PWM
                            self.data[ptype] = firmptype
                            self.widgets[p].set_active(0)  
                            #print "changed",p," to PWM "
                        continue
                    else:
                        #print "asking for GPIO instead of PWM\n"
                        firmptype = GPIOI
                # signal names for stepper
                elif firmptype in (STEPA,STEPB):  
                    if numofstepgens >= (compnum+1): 
                        if not self.widgets[ptype].get_active_text() == firmptype:
                            model = self.widgets[ptype].get_model()
                            model.clear() 
                            model.append((firmptype,))
                            self.widgets[ptype].set_active(0)
                            model = self.widgets[p].get_model()
                            model.clear()
                            self.widgets[pinv].set_sensitive(0) 
                            if firmptype == STEPA:
                                temp = -1
                                self.widgets[p].handler_block(self.data[blocksignal])
                                for name in (human_stepper_names):
                                    temp = temp + 1
                                    if temp in(2,3,4,5): continue
                                    if temp == 6:
                                        temp = 0
                                        continue
                                    model.append((name,))
                                self.widgets[p].set_sensitive(1)
                                self.widgets[ptype].set_sensitive(1)
                                self.widgets[p].handler_unblock(self.data[blocksignal])
                            if firmptype == STEPB:                               
                                    self.widgets[p].handler_block(self.data[blocksignal])
                                    for name in human_stepper_names: model.append((name,))
                                    self.widgets[p].handler_unblock(self.data[blocksignal])
                                    self.widgets[p].set_sensitive(0)
                                    self.widgets[p].set_active(0)
                                    self.widgets[ptype].set_sensitive(0) 
                        if self.data[ptype] in (STEPA,STEPB): 
                            model = self.widgets[p].get_model()
                            for search,item in enumerate(model):
                                if model[search][0]  == human_stepper_names[hal_stepper_names.index(self.data[p])]:
                                    self.widgets[p].set_active(search)
                                    break
                        else:
                            self.data[p] =  UNUSED_STEPGEN
                            self.data[ptype] = firmptype
                            self.widgets[p].set_active(0)
                            print "changed ",p," to stepgen"
                        
                        continue
                    else:firmptype = GPIOI
                # if GPIO combobox then only 'input, output, and open drain' in it  
                # else has only one pintype in it               
                if firmptype in (GPIOI,GPIOO,GPIOD):
                    #if self.widgets[ptype].get_active_text()  in (GPIOI,GPIOO,GPIOD): continue
                    print "converting to GPIO\n"
                    model = self.widgets[ptype].get_model()
                    model.clear()
                    # if GPIO combobox then 'input, output, and open drain' in it
                    for j in (0,1,2):
                        temp = pintype_names[j]
                        model.append((temp,))
                    self.widgets[ptype].set_sensitive(1)
                    model = self.widgets[p].get_model()
                    model.clear()
                    # signal names for GPIO INPUT
                    if not self.data[ptype] in (GPIOO,GPIOD):  
                        self.widgets[ptype].set_active(0)
                        blocksignal = "mesasignalhandlerc%ipin%i"% (connector,pin)  
                        self.widgets[p].handler_block(self.data[blocksignal])                
                        for name in human_input_names:
                            if self.data.limitshared or self.data.limitsnone:
                                if name in human_names_limit_only: continue 
                            if self.data.limitswitch or self.data.limitsnone:
                                if name in human_names_shared_home: continue                          
                            if self.data.homenone or self.data.limitshared:
                                if name in (_("Home X"), _("Home Y"), _("Home Z"), _("Home A"),_("All home")): continue
                            model.append((name,))  
                        self.widgets[p].handler_unblock(self.data[blocksignal])  
                        self.widgets[p].set_active(0)
                        self.widgets[p].set_sensitive(1)
                        self.widgets[pinv].set_sensitive(1)
                        if self.data[ptype] == GPIOI: 
                            model = self.widgets[p].get_model()
                            for search,item in enumerate(model):
                                if model[search][0]  == human_input_names[hal_input_names.index(self.data[p])]:
                                    self.widgets[p].set_active(search)
                                    break
                        else: 
                            self.data[p] =  UNUSED_INPUT
                            self.data[ptype] = GPIOI
                        continue
                    # signal names for GPIO OUTPUT and OPEN DRAIN OUTPUT
                    elif self.data[ptype] in (GPIOO,GPIOD):     
                        if firmptype == GPIOO:self.widgets[ptype].set_active(2)
                        else:self.widgets[ptype].set_active(1)  
                        blocksignal = "mesasignalhandlerc%ipin%i"% (connector,pin)  
                        self.widgets[p].handler_block(self.data[blocksignal]) 
                        for name in human_output_names: model.append((name,))
                        self.widgets[p].handler_unblock(self.data[blocksignal])  
                        self.widgets[p].set_active(0)  
                        self.widgets[p].set_sensitive(1)
                        self.widgets[pinv].set_sensitive(1)
                        model = self.widgets[p].get_model()
                        for search,item in enumerate(model):
                            if model[search][0]  == human_output_names[hal_output_names.index(self.data[p])]:
                                self.widgets[p].set_active(search)
                                break   
                        continue                              
                # This is for Stepgen / GPIO conversion
                if firmptype in (STEPA,STEPB):
                    if numofstepgens >= (compnum+1):
                        if self.data[ptype] in (STEPA,STEPB): 
                            for i in range(0,1000):
                                print hal_stepper_names.index(self.data[p]),"\n"
                            
                            continue
                        else:
                            self.data[p] =  UNUSED_STEPGEN
                            self.data[ptype] = firmptype
                            print "changed ",p," to stepgen"
                    else:   
                        if self.data[ptype] in (GPIOI,GPIOO,GPIOD) : continue
                        else:
                            self.data[p] =  UNUSED_INPUT
                            self.data[ptype] = GPIOI
                            self.widgets[p].set_sensitive(1)
                            print"changed",p," to GPIO"
                    self.data.numof_mesa_stepgens = numofstepgens

                # This is for Encoder / GPIO conversion
                if firmptype in (ENCA,ENCB,ENCI,ENCM):                
                    if numofencoders >= (compnum+1):
                        if self.data[ptype] in (ENCA,ENCB,ENCI,ENCM): continue
                        else:
                            self.data[p] =  UNUSED_ENCODER
                            self.data[ptype] = firmptype
                            print "changed",p," to Encoder"   
                    else: 
                        if self.data[ptype] in (GPIOI,GPIOO,GPIOD): continue
                        else:
                            self.data[p] =  UNUSED_INPUT
                            self.data[ptype] = GPIOI
                            self.widgets[p].set_sensitive(1)
                            print"changed",p,"to GPIO"
                self.data.numof_mesa_encodergens = numofencoders

                # This is for PWM / GPIO conversion               
                if firmptype in (PWMP,PWMD,PWME):
                    if numofpwmgens >= (compnum+1):
                        if self.data[ptype] in (PWMP,PWMD,PWME,PDMP,PDMD,PDME): continue
                        else:
                            self.data[p] =  UNUSED_PWM
                            self.data[ptype] = firmptype
                            print "changed",p," to pwm"   
                    else: 
                        if self.data[ptype] in (GPIOI,GPIOO,GPIOD): continue
                        else:
                            self.data[p] =  UNUSED_INPUT
                            self.data[ptype] = GPIOI
                            self.widgets[p].set_sensitive(1)
                            print"changed",p,"to GPIO"
                self.data.numof_mesa_pwmgens = numofpwmgens

                # This is for GPIO only conversion
                if firmptype in (GPIOI,GPIOO,GPIOD):
                    if self.data[ptype] in (GPIOI,GPIOO,GPIOD): continue
                    else:
                        self.data[p] =  UNUSED_INPUT
                        self.data[ptype] = firmptype
                        self.widgets[p].set_sensitive(1)
                        print"changed",p,"to GPIO"

        temp = (numofstepgens * self.data.mesa_currentfirmwaredata[6])
        temp1 = (numofencoders * self.data.mesa_currentfirmwaredata[5])
        temp2 = (numofpwmgens * 3)
        total = (self.data.mesa_currentfirmwaredata[8]-temp-temp1-temp2)
        self.data.numof_mesa_gpio = total     
        self.widgets.numof_mesa_stepgens.set_value(numofstepgens)
        self.widgets.numof_mesa_encodergens.set_value(numofencoders)      
        self.widgets.numof_mesa_pwmgens.set_value(numofpwmgens)
        self.in_mesa_prepare = False   
        self.on_mesa5i20_prepare()
       

    def on_mesa_pin_changed(self, widget, connector, pin):
                #if self.in_mesa_prepare == True: return       
                p = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin}
                ptype = 'm5i20c%(con)dpin%(num)dtype' % {'con':connector ,'num': pin}
                pinchanged =  self.widgets[p].get_active_text() 
                dataptype = self.data[ptype]
                used = 0
                #print"pin change method ",ptype," = ",dataptype,"active ",pinchanged,"\n"
                if dataptype in (ENCB,ENCI,ENCM,STEPB,PWMD,PWME,GPIOI,GPIOO,GPIOD):return
                # for stepgen pins
                if dataptype == STEPA:
                    #print"ptype step\n"
                    for index, name in enumerate(human_stepper_names):
                        if name == pinchanged:
                            if not pinchanged == "Unused StepGen":used = 1
                            tochange = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+1}
                            self.widgets[tochange].set_active((index+1)*used) 
                    return 
                # for encoder pins
                elif dataptype == ENCA: 
                    #print"ptype encoder\n"
                    nametocheck = human_encoder_input_names
                    signaltocheck = hal_encoder_input_names
                    addsignalto = self.data.halencoderinputsignames
                    unusedcheck = "Unused Encoder"
                # for PWM pins
                elif dataptype == PWMP: 
                    #print"ptype pwmp\n"
                    nametocheck = human_pwm_output_names
                    signaltocheck = hal_pwm_output_names
                    addsignalto = self.data.halpwmoutputsignames
                    unusedcheck = "Unused PWM Gen"
                else: 
                    print" pintype not found\n"
                    return   
                foundit = False            
                for index, name in enumerate(nametocheck):
                    if name == pinchanged:
                        if not pinchanged == unusedcheck:used = 1
                        # for encoder 0 amd 2 pins
                        if pin in (1,13):
                           # print"changing encoder b"
                            tochange = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin-1}
                            self.widgets[tochange].set_active((index+1)*used) 
                            tochange = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+3}
                            self.widgets[tochange].set_active((index+2)*used)
                        # for encoder 1 and 3 pins
                        elif pin in (3,15):
                            #print"changing encoder i"
                            tochange = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin-1}
                            self.widgets[tochange].set_active((index+1)*used) 
                            tochange = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+2}
                            self.widgets[tochange].set_active((index+2)*used) 
                        # for encoder mask pins
                        if self.data.mesa_currentfirmwaredata[5] == 4:                           
                            for count, name in enumerate((1,3,13,15)):
                                if name == pin:
                                    if connector == 3: count=count+4
                                    tochange = 'm5i20c%(con)dpin%(num)d' % {'con':4 ,'num': count}
                                    self.widgets[tochange].set_active((index+3)*used) 
                        # for pwm pins d and e
                        if pin in (6,7,18,19):
                            tochange = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+2}
                            self.widgets[tochange].set_active((index+1)*used)
                            tochange = 'm5i20c%(con)dpin%(num)d' % {'con':connector ,'num': pin+4}
                            self.widgets[tochange].set_active((index+2)*used)

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
        #self.findsignal("all-home")          
        #on_pport_back = on_pport_next
        if self.data.number_pports<2:
                self.widgets.druid1.set_page(self.widgets.xaxismotor)
                return True

    def on_pp1pport_back(self, *args):
         if not self.data.mesa5i20 :
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
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = '%sOpin%d' % (portname,pin)
            model = self.widgets[p].get_model()
            model.clear()
            for name in human_output_names: model.append((name,))
            self.widgets[p].set_active(hal_output_names.index(self.data[p]))
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
        
    def on_xaxismotor_prepare(self, *args):
        self.data.help = 6
        self.axis_prepare('x')

    def on_xaxismotor_next(self, *args):     
        self.axis_done('x')
        self.widgets.druid1.set_page(self.widgets.xaxis)
        return True

    def on_xaxismotor_back(self, *args):
        if self.data.number_pports==1:
                self.widgets.druid1.set_page(self.widgets.pp1pport)
                return True
        elif self.data.number_pports==2:
                self.widgets.druid1.set_page(self.widgets.pp2pport)
                return True
        elif self.data.number_pports==3:
                self.widgets.druid1.set_page(self.widgets.pp3pport)
                return True
        elif self.data.mesa5i20 :
                self.widgets.druid1.set_page(self.widgets.mesa5i20)
                return True    

    def on_yaxismotor_prepare(self, *args):
        self.data.help = 6
        self.axis_prepare('y')

    def on_yaxismotor_next(self, *args):
        self.axis_done('y')
        self.widgets.druid1.set_page(self.widgets.yaxis)
        return True

    def on_yaxismotor_back(self, *args):        
        self.widgets.druid1.set_page(self.widgets.xaxis)
        return True
    
    def on_zaxismotor_prepare(self, *args):
        self.data.help = 6
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
        self.data.help = 7
        self.axis_prepare('a')
    def on_aaxismotor_next(self, *args):
        self.axis_done('a')
        self.widgets.druid1.set_page(self.widgets.aaxis)
        return True
    def on_aaxismotor_back(self, *args):        
        self.widgets.druid1.set_page(self.widgets.zaxis)
        return True

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
        
        set_text("steprev")
        set_text("microstep")
        set_value("P")
        set_value("I")
        set_value("D")
        set_value("FF0")
        set_value("FF1")
        set_value("FF2")
        set_text("bias")
        set_text("deadband")
        set_text("steptime")
        set_text("stepspace")
        set_text("dirhold")
        set_text("dirsetup")
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
            w[axis + "leadscrewlabel"].set_text(_("Reduction Ratio"))
            w[axis + "screwunits"].set_text(_("degrees / rev"))
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
            w[axis + "leadscrewlabel"].set_text(_("Leadscrew Pitch"))
            w[axis + "screwunits"].set_text(_("(mm / rev)"))
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
            w[axis + "leadscrewlabel"].set_text(_("Leadscrew TPI"))
            w[axis + "screwunits"].set_text(_("(rev / inch)"))
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

        w[axis + "servo_info"].set_sensitive(not stepdriven)
        w[axis + "stepper_info"].set_sensitive(stepdriven)    
        w[axis + "drivertype"].set_active(self.drivertype_toindex(axis))
 
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

    def on_xdrivertype_changed(self, *args): self.driver_changed('x')
    def on_ydrivertype_changed(self, *args): self.driver_changed('y')
    def on_zdrivertype_changed(self, *args): self.driver_changed('z')
    def on_adrivertype_changed(self, *args): self.driver_changed('a')

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
        #self.on_calculate_ideal_period()

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
        def get_text(n): d[axis + n] = float(w[axis + n].get_text())
        def get_active(n): d[axis + n] = w[axis + n].get_active()
        get_text("steprev")
        get_text("microstep")
        get_text("P")
        get_text("I")
        get_text("D")
        get_text("FF0")
        get_text("FF1")
        get_text("FF2")
        get_text("bias")
        get_text("deadband")
        get_text("steptime")
        get_text("stepspace")
        get_text("dirhold")
        get_text("dirsetup")
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
        d[axis + "drivertype"] = self.drivertype_toid(axis, w[axis + "drivertype"].get_active())

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
            w[axis + "hz"].set_text("%.1f" % pps)
            w[axis + "encodercounts"].set_text( "%d" % ( 4 * float(w[axis+"encoderlines"].get_text())))
            test = self.data.findsignal(axis+"-stepgen-step")
            if test == "false":
                scale = self.data[axis + "scale"] = ( ( pitch) * get("encodercounts") 
                * (get("pulleynum") / get("pulleyden")))
            else:
                scale = self.data[axis + "scale"] = (1.0 * pitch * get("steprev")
                * get("microstep") * (get("pulleynum") / get("pulleyden")))
            w[axis + "scale"].set_text("%.1f" % scale)
            w[axis + "chartresolution"].set_text("%.7f" % (1.0 / scale))
            self.widgets.druid1.set_buttons_sensitive(1,1,1,1)
            w[axis + "axistune"].set_sensitive(1)
        except (ValueError, ZeroDivisionError): # Some entries not numbers or not valid
            w[axis + "chartresolution"].set_text("")
            w[axis + "acctime"].set_text("")
            w[axis + "accdist"].set_text("")
            w[axis + "hz"].set_text("")
            w[axis + "scale"].set_text("")
            self.widgets.druid1.set_buttons_sensitive(1,0,1,1)
            w[axis + "axistune"].set_sensitive(0)

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
                  if not self.warning_dialog(_("OK to replace existing custom ladder program?\nExisting Custom.clp will be renamed custom_backup.clp.\nAny existing file named -custom_backup.clp- will be lost. "),False):
                     self.widgets.druid1.set_page(self.widgets.advanced)
                     return True 
           if self.widgets.ladderexist.get_active() == False:
              if os.path.exists(os.path.join(distdir, "configurable_options/ladder/TEMP.clp")):
                 if not self.warning_dialog(_("You edited a ladder program and have selected a different program to copy to your configuration file.\nThe edited program will be lost.\n\nAre you sure?  "),False):
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
          
        # if mesa card not used clear all signals
        if self.data.mesa5i20 == 0:
            connector = 2
            # This initializes encoder pins
            for i in (0,1,2,3,4,5,12,13,14,15,16,17):
                pinname ="m5i20c%dpin%d"% (connector,i)
                self.data[pinname] = UNUSED_ENCODER
                pinname ="m5i20c%dpin%dtype"% (connector,i)
                self.data[pinname] = 3
            # This initializes PWM pins
            for i in (6,7,8,9,10,11,18,19,20,21,22,23):
                pinname ="m5i20c%dpin%d"% (connector,i)
                self.data[pinname] = UNUSED_PWM
                pinname ="m5i20c%dpin%dtype"% (connector,i)
                self.data[pinname] = 4
            for connector in(3,4):
                # This initializes GPIO input pins
                for i in range(0,16):
                    pinname ="m5i20c%dpin%d"% (connector,i)
                    self.data[pinname] = UNUSED_INPUT
                    pinname ="m5i20c%dpin%dtype"% (connector,i)
                    self.data[pinname] = 0
                # This initializes GPIO output pins
                for i in range(16,24):
                    pinname ="m5i20c%dpin%d"% (connector,i)
                    self.data[pinname] = UNUSED_OUTPUT
                    pinname ="m5i20c%dpin%dtype"% (connector,i)
                    self.data[pinname] = 1
            for connector in(2,3,4):
                # This initializes the mesa inverse pins
                for i in range(0,24):
                    pinname ="m5i20c%dpin%dinv"% (connector,i)
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
        board = self.data.mesa_boardname
        #self.widgets['window1'].set_sensitive(0)
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        #self.terminal = terminal = os.popen("gnome-terminal --title=joystick_search -x less /proc/bus/input/devices", "w" )  
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )   
        halrun.write("loadrt threads period1=200000 name1=fast fp1=0 period2=1000000 name2=slow\n")
        halrun.write("loadrt hostmot2\n")
        halrun.write("""loadrt hm2_pci config="firmware=hm2-trunk/%s/%s.BIT num_encoders=%d num_pwmgens=%d num_stepgens=%d"\n"""
         % (board, self.data.mesa_firmware, self.data.numof_mesa_encodergens,
            self.data.numof_mesa_pwmgens, self.data.numof_mesa_stepgens ))
        halrun.write("loadrt or2 count=72\n")
        halrun.write("addf hm2_%s.0.read slow\n"% board)
        for i in range(0,72):
            halrun.write("addf or2.%d slow\n"% i)
        halrun.write("addf hm2_%s.0.write slow\n"% board)
        halrun.write("addf hm2_%s.0.pet_watchdog fast\n"% board)
        halrun.write("start\n")
        halrun.write("loadusr -Wn m5i20test pyvcp -c m5i20test %(panel)s\n" %{'panel':"m5i20panel.xml",})
        halrun.write("loadusr halmeter\n")
        halrun.write("loadusr halmeter\n")
        for connector in (2,3,4):
           for pin in range(0,24):
                pinv = 'm5i20c%(con)dpin%(num)dinv' % {'con':connector ,'num': pin}
                ptype = 'm5i20c%(con)dpin%(num)dtype' % {'con':connector ,'num': pin}
                pintype = self.data[ptype]
                pininv = self.widgets[pinv].get_active()
                truepinnum = (connector-2)*24+ pin
                # for output / open drain pins
                if  pintype in (1,2):                
                    halrun.write("setp m5i20test.led.%d.disable true\n"% truepinnum )
                    halrun.write("setp m5i20test.button.%d.disable false\n"% truepinnum )
                    halrun.write("setp hm2_%s.0.gpio.%03d.is_output true\n"% (board,truepinnum ))
                    if pininv:  halrun.write("setp hm2_%s.0.gpio.%03d.invert_output true\n"% (board,truepinnum ))
                    halrun.write("net signal_out%d or2.%d.out hm2_%s.0.gpio.%03d.out\n"% (truepinnum,truepinnum,board,truepinnum))
                    halrun.write("net pushbutton.%d or2.%d.in1 m5i20test.button.%d\n"% (truepinnum,truepinnum,truepinnum))
                    halrun.write("net latchbutton.%d or2.%d.in0 m5i20test.checkbutton.%d\n"% (truepinnum,truepinnum,truepinnum))
                # for input pins
                elif pintype == 0:                                    
                    halrun.write("setp m5i20test.button.%d.disable true\n"% truepinnum )
                    halrun.write("setp m5i20test.led.%d.disable false\n"% truepinnum )
                    if pininv:  halrun.write("net blue_in%d hm2_%s.0.gpio.%03d.in_not m5i20test.led.%d\n"% (truepinnum,board,truepinnum,truepinnum))
                    else:   halrun.write("net blue_in%d hm2_%s.0.gpio.%03d.in m5i20test.led.%d\n"% (truepinnum,board,truepinnum,truepinnum))
                # for encoder pins
                elif pintype == 3:
                    halrun.write("setp m5i20test.led.%d.disable true\n"% truepinnum )
                    halrun.write("setp m5i20test.button.%d.disable true\n"% truepinnum )                   
                    if not pin in (0,2,12,14):
                        continue                 
                    if pin == 2 :encpinnum = (connector-2)*4 
                    elif pin == 0 :encpinnum = 1+((connector-2)*4) 
                    elif pin == 14 :encpinnum = 2+((connector-2)*4) 
                    elif pin == 12 :encpinnum = 3+((connector-2)*4) 
                    halrun.write("setp m5i20test.enc.%d.reset.disable false\n"% encpinnum )
                    halrun.write("net yellow_reset%d hm2_%s.0.encoder.%02d.reset m5i20test.enc.%d.reset\n"% (encpinnum,board,encpinnum,encpinnum))
                    halrun.write("net yellow_count%d hm2_%s.0.encoder.%02d.count m5i20test.number.%d\n"% (encpinnum,board,encpinnum,encpinnum))
                # for PWM pins
                elif pintype == 4:
                    halrun.write("setp m5i20test.led.%d.disable true\n"% truepinnum )
                    halrun.write("setp m5i20test.button.%d.disable true\n"% truepinnum )
                    if not pin in (6,7,18,19):
                        continue    
                    if pin == 7 :encpinnum = (connector-2)*4 
                    elif pin == 6 :encpinnum = 1 + ((connector-2)*4) 
                    elif pin == 19 :encpinnum = 2 + ((connector-2)*4) 
                    elif pin == 18 :encpinnum = 3 + ((connector-2)*4)        
                    halrun.write("net green_enable%d hm2_%s.0.pwmgen.%02d.enable m5i20test.dac.%d.enbl\n"% (encpinnum,board,encpinnum,encpinnum)) 
                    halrun.write("net green_value%d hm2_%s.0.pwmgen.%02d.value m5i20test.dac.%d-f\n"% (encpinnum,board,encpinnum,encpinnum)) 
                # for Stepgen pins
                elif pintype == 5:
                    halrun.write("setp m5i20test.led.%d.disable true\n"% truepinnum )
                    halrun.write("setp m5i20test.button.%d.disable true\n"% truepinnum ) 
                    if not pin in (0,6,12,18):
                        continue 
                    if pin == 0 :encpinnum = 0
                    elif pin == 6 :encpinnum = 1 
                    elif pin == 12 :encpinnum = 2 
                    elif pin == 18 :encpinnum = 3 
                    halrun.write("net brown_enable%d hm2_%s.0.stepgen.%02d.enable m5i20test.step.%d.enbl\n"% (encpinnum,board,encpinnum,encpinnum))
                    halrun.write("net brown_value%d hm2_%s.0.stepgen.%02d.position-cmd m5i20test.anaout.%d\n"% (encpinnum,board,encpinnum,encpinnum))
                    halrun.write("setp hm2_%s.0.stepgen.%02d.maxaccel 0 \n"% (board,encpinnum))
                    halrun.write("setp hm2_%s.0.stepgen.%02d.maxvel 0 \n"% (board,encpinnum))
                else: 
                    print "pintype error"
                    
        halrun.write("waitusr m5i20test\n"); halrun.flush()
        halrun.close()
        #terminal.close()
        self.widgets['window1'].set_sensitive(1)

    def parporttest(self,w):
        panelname = os.path.join(distdir, "configurable_options/pyvcp")
        #self.terminal = terminal = os.popen("gnome-terminal --title=joystick_search -x less /proc/bus/input/devices", "w" )  
        self.halrun = halrun = os.popen("cd %(panelname)s\nhalrun -sf > /dev/null"% {'panelname':panelname,}, "w" )  
        halrun.write("loadrt threads period1=200000 name1=fast fp1=0 period2=1000000 name2=slow\n")
        halrun.write("loadrt probe_parport\n")
        if self.data.number_pports>0:
            port3name = port2name = port1name = port3dir = port2dir = port1dir = ""
            if self.data.number_pports>2:
                 port3name = " " + self.ioaddr3
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
            halrun.write( "loadrt hal_parport cfg=\"%s%s%s%s%s%s\"\n" % (port1name, port1dir, port2name, port2dir, port3name, port3dir))
        halrun.write("loadrt or2 count=12\n")
        if self.data.number_pports > 0:
            halrun.write( "addf parport.0.read fast\n")
        if self.data.number_pports > 1:
            halrun.write("addf parport.1.read fast\n")
        if self.data.number_pports > 2:
            halrun.write("addf parport.2.read fast\n")
        for i in range(0,12):
            halrun.write("addf or2.%d fast\n"% i)
        if self.data.number_pports > 0:
            halrun.write( "addf parport.0.write fast\n")
        if self.data.number_pports > 1:
            halrun.write("addf parport.1.write fast\n")
        if self.data.number_pports > 2:
            halrun.write("addf parport.2.write fast\n")
        halrun.write("loadusr -Wn parporttest pyvcp -c parporttest %(panel)s\n" %{'panel':"parportpanel.xml\n",})
        halrun.write("loadusr halmeter\n")
        templist = ("pp1","pp2","pp3")
        for j, k in enumerate(templist):
            if self.data.number_pports < (j+1): break 
            if self.data[k+"_direction"] == 1:
                inputpins = (10,11,12,13,15)
                outputpins = (1,2,3,4,5,6,7,8,9,14,16,17)               
                for x in (2,3,4,5,6,7,8,9):
                    halrun.write( "setp parporttest.%s_led.%d.disable true\n"%(k, x))
                    halrun.write( "setp parporttest.%s_led_text.%d.disable true\n"%(k, x))
            else:
                inputpins = (2,3,4,5,6,7,8,9,10,11,12,13,15)
                outputpins = (1,14,16,17)
                for x in (2,3,4,5,6,7,8,9):
                    halrun.write( "setp parporttest.%s_button.%d.disable true\n"% (k, x))
                    halrun.write( "setp parporttest.%s_button_text.%d.disable true\n"% (k, x))
            for x in inputpins: 
                i = self.data["%sIpin%dinv" % (k, x)]
                if i:  halrun.write( "net red_in_not.%d parporttest.%s_led.%d <= parport.%d.pin-%02d-in-not\n" % (x, k, x, j, x))
                else:  halrun.write( "net red_in.%d parporttest.%s_led.%d <= parport.%d.pin-%02d-in\n" % (x, k, x, j , x))               
                         
            for num, x in enumerate(outputpins):  
                i = self.data["%sOpin%dinv" % (k, x)]
                if i:  halrun.write( "setp parport.%d.pin-%02d-out-invert true\n" %(j, x))
                halrun.write("net signal_out%d or2.%d.out parport.%d.pin-%02d-out\n"% (x, num, j, x))
                halrun.write("net pushbutton.%d or2.%d.in1 parporttest.%s_button.%d\n"% (x, num, k, x))
                halrun.write("net latchbutton.%d or2.%d.in0 parporttest.%s_checkbutton.%d\n"% (x, num, k, x))

            
        halrun.write("start\n")
        halrun.write("waitusr parporttest\n"); halrun.flush()
        halrun.close()
        #terminal.close()
        self.widgets['window1'].set_sensitive(1)

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
        halrun.write("""loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n""" % {'period': 30000   })
        halrun.write("loadusr halmeter\n")
        #halrun.write("loadrt probe_parport")
        #halrun.write("loadrt hal_parport cfg=%(ioaddr)s"% data.ioaddr)
        #halrun.write("addf parport.0.write fast")

       #TODO fix this to work with mesa and parport
        boardname = self.data.mesa_boardname
        amp = self.data.make_pinname(self.data.findsignal( "enable"))
        print "AMP =",amp
        print "encoder -A ->",self.data.make_pinname(self.data.findsignal( "x-encoder-a"))
        temp = self.data.findsignal( "x-encoder-b")
        print "encoder-B ->",temp +" -> "+self.data.make_pinname(temp)
        print "encoder-C ->",self.data.make_pinname(self.data.findsignal( "x-encoder-i"))
        if not amp == "false":
            if "HOSTMOT2" in amp:    
                amp = amp.replace("[HOSTMOT2](BOARD)",boardname)
            #if 'parport' in amp:
                
            halrun.write("setp %s true\n"% amp)

        estop = self.data.make_pinname(self.data.findsignal( "estop-out"))
        if not estop =="false":        
            if "HOSTMOT2" in estop:
                estop = estop.replace("[HOSTMOT2](BOARD)",boardname)     
           # if 'parport' in estop:
            halrun.write("setp %s true\n"%  estop)

      #  for pin in 1,2,3,4,5,6,7,8,9,14,16,17:
       #     inv = getattr(data, "pp1Opin%dinv" % pin)
       #     if inv:
       #         halrun.write("setp parport.0.pin-%(pin)02d-out-invert 1\n"
        #            % {'pin': pin}) 

        widgets.dialog1.set_title(_("%s Axis Test") % axis.upper())
        self.jogplus = self.jogminus = 0
        self.axis_under_test = axis
        self.update_axis_params()

        halrun.write("start\n"); halrun.flush()
        self.widgets['window1'].set_sensitive(0)
        widgets.dialog1.show_all()
        result = widgets.dialog1.run()
        widgets.dialog1.hide()
        
        if not amp == "false":
             halrun.write("setp %s false\n"% amp)
        if not estop == "false":
             halrun.write("setp %s false\n"% estop)

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
       # halrun.write("""
       #    setp steptest.0.jog-minus %(jogminus)s
       #     setp steptest.0.jog-plus %(jogplus)s
       # """ % {
       #     'jogminus': self.jogminus,
       #     'jogplus': self.jogplus,           
       # })
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
