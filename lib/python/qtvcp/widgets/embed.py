#!/usr/bin/python
# -*- coding: utf-8 -*-

"""
PyQt4 widget that embeds a pygtk gremlin widget in it's self.
Chris Morley 
"""

import sys
import os
import subprocess

from PyQt5.QtCore import pyqtProperty, QSize, QEvent
from PyQt5.QtWidgets import QWidget
from PyQt5.QtGui import QWindow, QResizeEvent
from qtvcp.qt_glib import GStat, IStat
import thread
import gobject
from subprocess import Popen
import linuxcnc

from Xlib.protocol import event as _event
from Xlib import display, X
from Xlib.xobject import drawable

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

ISTAT = IStat()

##############################################
# Container class
# We embed Gremlin GTK object into this
##############################################
class Embed(QWidget):
    def __init__(self, parent = None):
        super(Embed, self).__init__(parent)
        self.WID=None
        self.fw = None

    def get_xWinID(self):
        return int(self.winId())

    def launch(self, cmd):
        #c = cmd
        c = cmd.replace('{XID}', str(self.get_xWinID()))
        print c
        child = Popen(c.split(), stdin = subprocess.PIPE,
                                                   stdout = subprocess.PIPE,
                                                   )
        sid = child.stdout.readline()
        self.WID = long(sid)
        print 'XID:',sid

    def launch_xid(self,cmd):
        c = cmd.split()
        print c
        self.ob = subprocess.Popen( c,            stdin = subprocess.PIPE,
                                                   stdout = subprocess.PIPE,
                                                   )
        sid = self.ob.stdout.readline()
        print 'XID:',sid
        self.embed_plug(int(sid))


    def launch_ob(self):
        self.ob = subprocess.Popen( ["matchbox-keyboard", "--xid"],
                                                   stdin = subprocess.PIPE,
                                                   stdout = subprocess.PIPE,
                                                   close_fds = True )
        sid = self.ob.stdout.readline()
        print 'XID:',sid
        self.WID = int(sid)
        self.embed_plug(int(sid))

    # Embed a X11 window into a QT window using X window ID
    def embed_plug(self, WID):
        d = display.Display()
        self.fw = drawable.Window(d.display, WID, 0)

        self.haveContainer = True
        subWindow = QWindow.fromWinId(int(WID))
        container = self.createWindowContainer(subWindow,self)
        sub_win_id = int(container.winId())
        container.setParent(self)
        #container.setGeometry(500, 500, 450, 400)
        container.show()
        container.resize(330,360)
        return sub_win_id

    def sizeHint(self):
        return QSize(300, 300)

    def closeEvent(self, event):
        pass

    def event(self, event):
        if event.type() ==  QEvent.Resize:
            w = QResizeEvent.size(event)
            print( w.width(), w.height())
            if self.WID:
                self.xlib_size_request(w.width(),w.height())
            #self.resize(QResizeEvent.size(event))
        return True



    def xlib_size_request(self,w,h):
            if self.fw == None:return
            print w,h
            Klass = _event.ResizeRequest
            kw = dict(window=self.fw, width = w, height = h)
            msg = Klass(**kw)
            self.fw.send_event(msg)

############################
# dynamic tabs
############################

class Nothing():

        def send_message(self,socket,dest_xid,message):
            event = gtk.gdk.Event(gtk.gdk.CLIENT_EVENT)
            event.window = socket.get_window()                  # needs sending gdk window
            event.message_type = gtk.gdk.atom_intern('Gladevcp')    # change to any text you like
            event.data_format = 8                               # 8 bit (char) data (options: long,short)
            event.data = message                                # must be exactly 20 char bytes (options: 5 long or 10 short)
            event.send_event = True                             # signals this was sent explicedly
            event.send_client_message(dest_xid)                 # uses destination XID window number




# For testing purposes, include code to allow a widget to be created and shown
# if this file is run.

if __name__ == "__main__":

    import sys
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    widget = Embed()
    #widget.launch('halcmd loadusr gladevcp -x {XID} ../../gladevcp/offsetpage.glade')
    #widget.launch_xid('halcmd loadusr gladevcp -d --xid  ../../gladevcp/offsetpage.glade &')
    widget.launch('mplayer -wid {XID} tv://0 -vf rectangle=-1:2:-1:240,rectangle=2:-1:320:-1')
    #widget.launch_ob()
    #widget.sizeHint(300,300)
    widget.show()
    sys.exit(app.exec_())
