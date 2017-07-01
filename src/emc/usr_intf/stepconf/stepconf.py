#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    stepconf 1.1 revamped by Chris Morley 2014
#    replaced Gnome Druid as that is not available in future linux distrubutions
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#import pygtk
#pygtk.require("2.0")

from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Gdk

import sys
import os
from optparse import Option, OptionParser

import xml.dom.minidom
import hashlib
import math
import errno
import textwrap
import commands
import shutil
import time
from multifilebuilder_gtk3 import MultiFileBuilder

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
    m = Gtk.MessageDialog(w,
                Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                Gtk.MessageType.ERROR, Gtk.ButtonsType.OK,
                _("Stepconf encountered an error.  The following "
                "information may be useful in troubleshooting:\n\n")
                + "".join(lines))
    m.show()
    m.run()
    m.destroy()
sys.excepthook = excepthook

BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))

# translations,locale
import locale, gettext
LOCALEDIR = os.path.join(BASE, "share", "locale")
domain = "linuxcnc"
gettext.install(domain, localedir=LOCALEDIR, unicode=True)
locale.setlocale(locale.LC_ALL, '')
locale.bindtextdomain(domain, LOCALEDIR)
gettext.bindtextdomain(domain, LOCALEDIR)
# Due to traslation put here module with locale
from stepconf.definitions import *

datadir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "linuxcnc","stepconf")
main_datadir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "share", "linuxcnc")
wizard = os.path.join(datadir, "linuxcnc-wizard.gif")
if not os.path.isfile(wizard):
    wizard = os.path.join(main_datadir, "linuxcnc-wizard.gif")
if not os.path.isfile(wizard):
    print "cannot find linuxcnc-wizard.gif, looked in %s and %s" % (datadir, main_datadir)
    sys.exit(1)


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
from stepconf import preset
from stepconf import build_INI
from stepconf import build_HAL

debug = False

def makedirs(d):
    try:
        os.makedirs(d)
    except os.error, detail:
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

        self.md5sums = []

        self.machinename = _("my-mill")
        self.axes = 0 # XYZ
        self.units = 1 # mm
        self.drivertype = "Other"
        self.steptime = 5000
        self.stepspace = 5000
        self.dirhold = 20000 
        self.dirsetup = 20000
        self.latency = 15000
        self.period = 25000
        
        self.global_preset = 0
        self.lparport = self.find_parport()
        self.ioaddr = "0"
        self.ioaddr2 = "1"
        self.pp2_direction = 0 # output
        self.ioaddr3 = "2"
        self.pp3_direction = 0 # output
        self.number_pports = 1

        self.halui = 0
        self.halui_list = []
        self.halui_custom = 0
        self.halui_list_custom = []

        self.hal_postgui = 0
        self.hal_postgui_list = []
        self.hal_postgui_custom = 0
        self.hal_postgui_list_custom = []

        self.manualtoolchange = 1
        self.customhal = 1 # include custom hal file
        self.pyvcp = 1 # default include
        self.pyvcptype = 0 # include default pyvcp gui
        self.pyvcpname = "blank.xml"
        self.pyvcphaltype = 0 # no HAL connections specified
        #self.pyvcpconnect = 1 # HAL connections allowed

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
        self.gladevcphaluicmds = 0 # not used
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
        self.gladevcpname = "blank.ui"
        
        # Position of probe switch
        self.probe_x_pos = 10
        self.probe_y_pos = 10
        self.probe_z_pos = 100
        self.probe_sensor_height = 40.0 # mm

        self.classicladder = 0 # not included
        self.tempexists = 0 # not present
        self.laddername = "custom.clp"
        self.modbus = 0
        self.ladderhaltype = 0 # no HAL connections specified
        self.ladderconnect = 1 # HAL connections allowed

        self.select_axis = True
        self.select_gmoccapy = False

        self.pin1inv = False
        self.pin2inv = False
        self.pin3inv = False
        self.pin4inv = False
        self.pin5inv = False
        self.pin6inv = False
        self.pin7inv = False
        self.pin8inv = False
        self.pin9inv = False
        self.pin10inv = False
        self.pin11inv = False
        self.pin12inv = False
        self.pin13inv = False
        self.pin14inv = False
        self.pin15inv = False
        self.pin16inv = False
        self.pin17inv = False

        self.pin1 = d_hal_output[ESTOP]
        self.pin2 = d_hal_output[XSTEP]
        self.pin3 = d_hal_output[XDIR]
        self.pin4 = d_hal_output[YSTEP]
        self.pin5 = d_hal_output[YDIR]
        self.pin6 = d_hal_output[ZSTEP]
        self.pin7 = d_hal_output[ZDIR]
        self.pin8 = d_hal_output[ASTEP]
        self.pin9 = d_hal_output[ADIR]
        self.pin14 = d_hal_output[CW]
        self.pin16 = d_hal_output[PWM]
        self.pin17 = d_hal_output[AMP]

        self.pin10 = d_hal_input[UNUSED_INPUT]
        self.pin11 = d_hal_input[UNUSED_INPUT]
        self.pin12 = d_hal_input[UNUSED_INPUT]
        self.pin13 = d_hal_input[UNUSED_INPUT]
        self.pin15 = d_hal_input[UNUSED_INPUT]
        # pp1 preset
        self.pport1_preset = 0

        #   port 2
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            self[p] = d_hal_output[UNUSED_OUTPUT]
            p = 'pp2_pin%dinv' % pin
            self[p] = 0
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            self[p] = d_hal_input[UNUSED_INPUT]
            p = 'pp2_pin%d_in_inv' % pin
            self[p] = 0

        for i in ('x','y','z','a', 'u','v'):
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

             # Varibles for test axis
             self[i+'testmaxvel'] = None
             self[i+'testmaxacc'] = None
             # Preset
             self[i+'preset'] = 0

        # set xyzuv axes defaults depending on units true = imperial
        self.set_axis_unit_defaults(False)

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
        self.createsymlink = 1
        self.createshortcut = 1

    def get_machine_preset(self, combo):
        tree_iter = combo.get_active_iter()
        if tree_iter != None:
            model = combo.get_model()
            name, row_id = model[tree_iter][:2]
            
            lcurrent_machine = filter(lambda element: element['index'] == row_id, preset.preset_machines)
            if(lcurrent_machine != []):
                # Just first element
                current_machine = lcurrent_machine[0]
                return(current_machine)
        else:
            # Other selected
            return(None)

    def select_combo_machine(self, combo, index):
        liststore = combo.get_model ()
        treeiter = liststore.get_iter_first()
        while treeiter != None:
            name, row_id = liststore[treeiter][:2]
            if(row_id == index):
                combo.set_active_iter(treeiter)
                return
            treeiter = liststore.iter_next(treeiter)

    def find_parport(self):
        # Try to find parallel port
        lparport=[]
        # open file.
        try:
            in_file = open("/proc/ioports","r")
        except:
            print "Unable to open /proc/ioports"
            return([])

        try:
            for line in in_file:
                if "parport" in line:
                    tmprow = line.strip()
                    lrow = tmprow.split(":")
                    address_range = lrow[0].strip()
                    init_address = address_range.split("-")[0].strip()
                    lparport.append("0x" + init_address)
        except:
            print "Error find parport"
            in_file.close()
            return([])
        in_file.close()
        if lparport == []:
            return([])
        return(lparport)
                    
    # change the XYZ axis defaults to metric or imperial
    # This only sets data that makes sense to change eg gear ratio don't change
    def set_axis_unit_defaults(self, units=False):
        if units: # imperial
            for i in ('x','y','z','a','u','v'):
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
            for i in ('x','y','z','a','u','v'):
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
        else:
            print 'error in ideal period calculation - number of axes unrecognized'
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
        #print filename
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

        d2.writexml(open(filename, "wb"), addindent="  ", newl="\n")

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
                dialog = Gtk.MessageDialog(app.w.window1,
                    Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                    Gtk.MessageType.WARNING, Gtk.ButtonsType.OK,
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

        for p in (10,11,12,13,15):
            pin = "pin%d" % p
            p = self[pin]
        for p in (1,2,3,4,5,6,7,8,9,14,16,17):
            pin = "pin%d" % p
            p = self[pin]



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
        if r is None: raise AttributeError, "No widget %r" % attr
        return r
    def __getitem__(self, attr):
        r = self._xml.get_object(attr)
        if r is None: raise IndexError, "No widget %r" % attr
        return r

class StepconfApp:
    def __init__(self, dbgstate):
        global debug
        debug = self.debug = dbgstate
        global dbg
        dbg = self.dbg
        self.program_path = BASE
        self.distdir = distdir
        # For axis test
        self.jogminus = 0
        self.jogplus = 0

        self.icondir = os.path.join(os.path.abspath(os.path.dirname(__file__)), "..")
        self.linuxcncicon = os.path.join(self.icondir, "linuxcncicon.png")
        if not os.path.isfile(self.linuxcncicon):
            self.linuxcncicon = os.path.join("/etc/linuxcnc/linuxcnc-wizard.gif")
        if not os.path.isfile(self.linuxcncicon):
            self.linuxcncicon = os.path.join("/usr/share/linuxcnc/linuxcncicon.png")
        
        self.recursive_block = False
        self.axis_under_test = None
        # Private data holds the array of pages to load, signals, and messages
        self._p = Private_Data()
        self.d = Data(self._p)
        # build the glade files
        self.builder = MultiFileBuilder()
        self.builder.add_from_file(os.path.join(datadir,'main_page.glade'))
        window = self.builder.get_object("window1")
        notebook1 = self.builder.get_object("notebook1")
        for reference,title,state in (available_page):
            if reference == 'intro':continue
            dbg("loading glade page REFERENCE:%s TITLE:%s STATE:%s"% (reference,title,state))
            self.builder.add_from_file(os.path.join(datadir, '%s.glade'%reference))
            page = self.builder.get_object(reference)
            notebook1.append_page(page, Gtk.Label(reference))
        notebook1.set_show_tabs(False)

        self.w = Widgets(self.builder)
        self.p = pages.Pages(self)
        self.INI = build_INI.INI(self)
        self.HAL = build_HAL.HAL(self)
        self.builder.set_translation_domain(domain) # for locale translations
        self.builder.connect_signals( self.p ) # register callbacks from Pages class
        #wiz_pic = Gdk.pixbuf_new_from_file(wizard)
        """
        for w in self.w:
            myid = w.get_id()
            print myid, w.get_name()
            #w.set_name(myid)
        """
        image = Gtk.Image()
        image.set_from_file(wizard)
        wiz_pic = image.get_pixbuf()
        self.w.wizard_image.set_from_pixbuf(wiz_pic)
        self.d.load_preferences()
        self.p.initialize()
        window.show()
        #self.w.xencoderscale.realize()



#*******************
# GUI Helper functions
#*******************
    # print debug strings
    def dbg(self,str):
        global debug
        if not debug: return
        print "DEBUG: %s"%str



    # pop up dialog
    def warning_dialog(self,message,is_ok_type):
        if is_ok_type:
           dialog = Gtk.MessageDialog(app.w.window1,
                Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                Gtk.MessageType.WARNING, Gtk.ButtonsType.OK,message)
           dialog.show_all()
           result = dialog.run()
           dialog.destroy()
           return True
        else:   
            dialog = Gtk.MessageDialog(self.w.window1,
               Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
               Gtk.MessageType.QUESTION, Gtk.ButtonsType.YES_NO, message)
            dialog.show_all()
            result = dialog.run()
            dialog.destroy()
            if result == Gtk.ResponseType.YES:
                return True
            else:
                return False


    # check for spindle output signals
    def has_spindle_speed_control(self):
        d = self.d

        # Check pp1 for output signals
        pp1_check =  d_hal_output[PWM] in (d.pin1, d.pin2, d.pin3, d.pin4, d.pin5, d.pin6,
            d.pin7, d.pin8, d.pin9, d.pin14, d.pin16, d.pin17)
        if pp1_check is True: return True

        # now check port 2, which can be set to 'in' or 'out' mode: so can have
        # other pins number to check then pp1
        # output pins:
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            if d[p] == d_hal_output[PWM]: return True

        # if we get to here - there are no spindle control signals
        return False

    def has_spindle_encoder(self):
        d = self.d

        # pp1 input pins
        if d_hal_input[PPR] in (d.pin10, d.pin11, d.pin12, d.pin13, d.pin15): return True
        if d_hal_input[PHA] in (d.pin10, d.pin11, d.pin12, d.pin13, d.pin15): return True

        # pp2 input pins
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            if d[p] in (d_hal_input[PPR], d_hal_input[PHA]): return True

        # if we get to here - there are no spindle encoder signals
        return False


    # pport functions
    # disallow some signal combinations
    def do_exclusive_inputs(self, pin,port):
        # If initializing the Pport pages we don't want the signal calls to register here.
        # if we are working in here we don't want signal calls because of changes made in here
        # GTK supports signal blocking but then you can't assign signal names in GLADE -slaps head
        if self._p.in_pport_prepare or self.recursive_block: return
        self.recursive_block = True
        v = pin.get_active()
        ex = exclusive_input.get(v, ())
        
        # This part is probably useless. It is just an exercise with the GTK3 combobox.
        tree_iter = pin.get_active_iter()
        if tree_iter != None:
            model = pin.get_model()
            current_text = model[tree_iter][0]
        # Find function with current selected index
        lcurrent_function = filter(lambda element: element['index'] == v, hal_input)
        current_function = lcurrent_function[0]
        name = current_function["name"]
        index = current_function["index"]

        # search pport1 for the illegal signals and change them to unused.
        dbg( 'looking for %s in pport1'%name)
        for pin1 in (10,11,12,13,15):
            p = 'pin%d' % pin1
            if self.w[p] == pin: continue
            v1 = hal_input[self.w[p].get_active()]
            if v1["index"] in ex or v1["name"] == name:
                dbg( 'found %s, at %s'%(name,p))
                #self.w[p].set_active(self._p.hal_input_names.index(UNUSED_INPUT))
                self.w[p].set_active(UNUSED_INPUT)
                if not port ==1: # if on the other page must change the data model too
                    dbg( 'found on other pport page')
                    self.d[p] = d_hal_input[UNUSED_INPUT]
        # search pport2 for the illegal signals and change them to unused.
        dbg( 'looking for %s in pport2'%name)
        for pin1 in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p2 = 'pp2_pin%d_in' % pin1
            if self.w[p2] == pin: continue
            #v2 = self._p.hal_input_names[self.w[p2].get_active()]
            v2 = hal_input[self.w[p2].get_active()]
            if v2["index"] in ex or v2["name"] == name:
                dbg( 'found %s, at %s'%(name,p2))
                #self.w[p2].set_active(self._p.hal_input_names.index(UNUSED_INPUT))
                self.w[p2].set_active(UNUSED_INPUT)
                if not port ==2:# if on the other page must change the data model too
                    dbg( 'found on other pport page')
                    self.d[p2] = d_hal_input[UNUSED_INPUT]
        self.recursive_block = False
  

#**********************************
# Common helper functions
#**********************************

    def build_input_set(self):
        input_set =[self.d.pin10, self.d.pin11, self.d.pin12, self.d.pin13, self.d.pin15]
        if self.d.number_pports > 1:
            #print "More pport"
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
        inputs = self.build_input_set()
        thisaxishome = set((d_hal_input[ALL_HOME], d_hal_input[ALL_LIMIT_HOME], "home-" + axis, "min-home-" + axis,
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
    usage = "usage: Stepconf -[options]"
    parser = OptionParser(usage=usage)
    parser.add_option("-d", action="store_true", dest="debug",help="Print debug info and ignore realtime/kernel tests")
    (options, args) = parser.parse_args()
    if options.debug:
        app = StepconfApp(dbgstate=True)
    else:
        app = StepconfApp(False)

    # Prepare Style
    cssProvider = Gtk.CssProvider()
    cssProvider.load_from_data(style)
    screen = Gdk.Screen.get_default()
    styleContext = Gtk.StyleContext()
    styleContext.add_provider_for_screen(screen, cssProvider, Gtk.STYLE_PROVIDER_PRIORITY_USER)

    Gtk.main()

