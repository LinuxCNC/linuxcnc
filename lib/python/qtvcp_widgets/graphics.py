#!/usr/bin/python
# -*- coding: utf-8 -*-

"""
PyQt4 widget that embeds a pygtk gremlin widget in it's self.
Chris Morley 


"""

import sys
import os
import warnings

# supress 'RuntimeWarning: PyOS_InputHook is not available for interactive'
# this warning is caused by pyqt owning the Inputhook
warnings.filterwarnings("ignore")
import gtk
warnings.filterwarnings("default")

from PyQt4.QtCore import pyqtProperty
from PyQt4.QtGui import QX11EmbedContainer
from qtvcp.qt_glib import QStat
import thread
import gobject

import linuxcnc
import gremlin

gobject.threads_init()

def run_gtk(self):
    try:
        gtk.gdk.threads_enter()
        gtk.main()
        gtk.gdk.threads_leave()
    except:
        pass

# We subclass gremlin so we can change/add some behaivor
class modded_gremlin(gremlin.Gremlin):
    def __init__(self,  *a, **kw):
        #inifile = '/home/chris/emc2-dev/configs/sim/axis/axis.ini'
        inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        inifile = linuxcnc.ini(inifile)
        gremlin.Gremlin.__init__(self, inifile)
        self.enable_dro = True
        self.colors['overlay_background'] = (0.0, 0.0, 0.57)
        self.colors['back'] = (0.0, 0.0, 0.75)
        self.qstat = QStat()
        self.qstat.file_loaded.connect(self.fileloaded)
        self.qstat.reload_display.connect(self.reloadfile)

    def setview(self, value):
        view = str(value).lower()
        if self.lathe_option:
            if view not in ['p','y','y2']:
                return False
        elif view not in ['p', 'x', 'y', 'z', 'z2']:
            return False
        self.current_view = view
        if self.initialised:
            self.set_current_view()

    def reloadfile(self):
        try:
            self.fileloaded(None,self._reload_filename)
        except:
            pass

    def fileloaded(self,f):
        self._reload_filename=f
        try:
            self._load(f)
        except AttributeError,detail:
               #AttributeError: 'NoneType' object has no attribute 'gl_end'
            print 'hal_gremlin: continuing after',detail

    # This overrides glcannon.py method so we can change the DRO 
    def dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
            if not self.enable_dro:
                return limit, homed, [''], ['']

            if self.metric_units:
                format = "% 6s:% 9.3f"
                if self.show_dtg:
                    droformat = " " + format + "  DTG %1s:% 9.3f"
                else:
                    droformat = " " + format
                offsetformat = "% 5s %1s:% 9.3f  G92 %1s:% 9.3f"
                rotformat = "% 5s %1s:% 9.3f"
            else:
                format = "% 6s:% 9.4f"
                if self.show_dtg:
                    droformat = " " + format + "  DTG %1s:% 9.4f"
                else:
                    droformat = " " + format
                offsetformat = "% 5s %1s:% 9.4f  G92 %1s:% 9.4f"
                rotformat = "% 5s %1s:% 9.4f"
            diaformat = " " + format

            posstrs = []
            droposstrs = []
            for i in range(9):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    posstrs.append(format % (a, positions[i]))
                    if self.show_dtg:
                        droposstrs.append(droformat % (a, positions[i], a, axisdtg[i]))
                    else:
                        droposstrs.append(droformat % (a, positions[i]))
            droposstrs.append("")

            for i in range(9):
                index = s.g5x_index
                if index<7:
                    label = "G5%d" % (index+3)
                else:
                    label = "G59.%d" % (index-6)

                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(offsetformat % (label, a, g5x_offset[i], a, g92_offset[i]))
            droposstrs.append(rotformat % (label, 'R', s.rotation_xy))

            droposstrs.append("")
            for i in range(9):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(rotformat % ("TLO", a, tlo_offset[i]))

            # if its a lathe only show radius or diameter as per property
            # we have to adjust the homing icon to line up:
            if self.is_lathe():
                if homed[0]:
                    homed.pop(0)
                    homed.pop(0)
                    homed.insert(0,1)
                    homed.insert(0,0)
                posstrs[0] = ""
                if self.show_lathe_radius:
                    posstrs.insert(1, format % ("Rad", positions[0]))
                else:
                    posstrs.insert(1, format % ("Dia", positions[0]*2.0))
                droposstrs[0] = ""
                if self.show_dtg:
                    if self.show_lathe_radius:
                        droposstrs.insert(1, droformat % ("Rad", positions[0], "R", axisdtg[0]))
                    else:
                        droposstrs.insert(1, droformat % ("Dia", positions[0]*2.0, "D", axisdtg[0]*2.0))
                else:
                    if self.show_lathe_radius:
                        droposstrs.insert(1, droformat % ("Rad", positions[0]))
                    else:
                        droposstrs.insert(1, diaformat % ("Dia", positions[0]*2.0))

            if self.show_velocity:
                if self.is_lathe:
                    pos=1
                    posstrs.append(format % ("IPR", spd/200)) #FIXME 200 should be current RPM
                else:
                    pos=0
                    posstrs.append(format % ("Vel", spd))

                for i in range(9):
                    if s.axis_mask & (1<<i): pos +=1

                droposstrs.insert(pos, " " + format % ("Vel", spd))

            if self.show_dtg:
                posstrs.append(format % ("DTG", dtg))

            return limit, homed, posstrs, droposstrs

# This is the GTK embedding plug that will put the modded gremlin object into.
# This get embedded into a QT container
# It runs in it's own thread to update the GTK side of things
class PyApp(gtk.Plug):
    def __init__(self,):
        super(PyApp, self).__init__(0l)

        self.set_size_request(300, 300)
        self.connect("destroy", self.on_destroy)

        self.plug_id = self.get_id()
        vbox = gtk.VBox()
        self.add(vbox)
        self.gremlin = modded_gremlin()
        vbox.add(self.gremlin)

        self.show_all()

    def on_destroy(self,w):
        gtk.gdk.threads_enter()
        gtk.main_quit()
        gtk.gdk.threads_leave()

# This is the QT embedding container object.
class Graphics(QX11EmbedContainer):
    def __init__(self, parent = None):
        QX11EmbedContainer.__init__(self, parent)
        self.pygtk = PyApp()
        self.gremlin = self.pygtk.gremlin
        # run GTK in a separate thread
        try:
            thread.start_new_thread( run_gtk,(None) )
        except:
            pass
        #QtCore.QObject.connect(self, QtCore.SIGNAL("error()"), self.embeddederror)

        # embed GTK gremkin 
        self.embedClient (self.pygtk.plug_id)

        # gremlin defaults
        #self.gremlin.enable_dro = False

        self.gremlin.metric_units = False
        self.setview('p')

    def embeddederror(self):
        print 'embed error'

    def sizeHint(self):
        return QSize(300, 300)

    def closeEvent(self, event):
        self.pygtk.on_destroy(None)

# property getter/setters

    # VIEW
    def setview(self, view):
        self.gremlin.setview(view)
    def getview(self):
        return self.gremlin.current_view
    def resetview(self):
        self.gremlin.setview('p')
    view = pyqtProperty(str, getview, setview, resetview)

    # DRO
    def setdro(self,state):
        self.gremlin.enable_dro = state
        self.gremlin.expose()
    def getdro(self):
        return self.gremlin.enable_dro
    dro = pyqtProperty(bool, getdro, setdro)

    # DTG
    def setdtg(self,state):
        self.gremlin.show_dtg = state
        self.gremlin.expose()
    def getdtg(self):
        return self.gremlin.show_dtg
    dtg = pyqtProperty(bool, getdtg, setdtg)

    # METRIC
    def setmetric(self,state):
        self.gremlin.metric_units  = state
        self.gremlin.expose()
    def getmetric(self):
        return self.gremlin.metric_units
    metric = pyqtProperty(bool, getmetric, setmetric)

# For testing purposes, include code to allow a widget to be created and shown
# if this file is run.

if __name__ == "__main__":

    import sys
    from PyQt4.QtGui import QApplication

    app = QApplication(sys.argv)
    widget = Graphics()
    widgeet,sizeHint(300,300)
    widget.show()
    sys.exit(app.exec_())
