#!/usr/bin/env python3
import sys, time
import os
import subprocess

from PyQt5.QtCore import QSize, QEvent, pyqtProperty
from PyQt5.QtGui import QWindow, QResizeEvent, QMoveEvent
from PyQt5.QtWidgets import QWidget

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.lib import xembed
from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class XEmbeddable(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(XEmbeddable, self).__init__(parent)

    def _hal_init(self):
        pass

    def embed(self, command):
        if '-x {XID}' in command:
            # convert gladevcp to other type of embedding
            command = command.replace('-x {XID}', ' --xid ')
            self.embed_program(command)
        elif '{XID}' in command:
            self.let_program_embed(command)
        else:
            self.embed_program(command)

    # foreign program embeds it's self in our window
    def let_program_embed(self,command):
        self.w = QWindow()
        self.container = QWidget.createWindowContainer(self.w, self)

        xid = int(self.w.winId())
        cmd = command.replace('{XID}', str(xid))
        self.launch(cmd)
        self.show()
        # there seems to be a race - sometimes the foreign window doesn't embed
        time.sleep(.2)

    # we embed foreign program into our window 
    def embed_program(self, command): 
        try:
            self.external_id = self.launch_xid(command)
            window = QWindow.fromWinId(self.external_id)
            self.container = QWidget.createWindowContainer(window, self)
            self.show()
            # there seems to be a race - sometimes the foreign window doesn't embed
            time.sleep(.2)
            return True
        except  Exception as e:
            LOG.warning('Exception:{}'.format(command))
            LOG.warning('Exception:{}'.format( e))
            raise Exception(e)
            return False

    def event(self, event):
        if event.type() ==  QEvent.Resize:
            w = QResizeEvent.size(event)
            try:
                self.container.resize(w.width(), w.height())
            except:
                pass
        elif event.type() == QEvent.Move:
                try:
                    self.container.move(QMoveEvent.pos(event))
                except:
                    pass
        return True

    # launches program that embeds it's self into our window
    # The foreign program never resizes as the window changes
    def launch(self, cmd):
        c = cmd.split()
        self.ob = subprocess.Popen(c,stdin = subprocess.PIPE,
                                   stdout = subprocess.PIPE)

    # returns program's XID number so we can embed it into our window
    # This works the best - the window resizes properly
    # still not great qt4 worked so much better
    def launch_xid(self,cmd):
        c = cmd.split()
        self.ob = subprocess.Popen(c,stdin = subprocess.PIPE,
                                   stdout = subprocess.PIPE)
        sid = self.ob.stdout.readline()
        LOG.debug( 'XID: {}'.format(sid))
        return int(sid)

    def _hal_cleanup(self):
        try:
            self.ob.terminate()
        except Exception as e:
            print(e)

class XEmbed(XEmbeddable, _HalWidgetBase):
    def __init__(self, parent=None):
        super(XEmbed, self).__init__(parent)
        self.command = None

    def _hal_init(self):
        # send embedded program our X window id so it can forward events to us.
        wid = int(self.QTVCP_INSTANCE_.winId())
        print('parent wind id', wid)
        os.environ['QTVCP_FORWARD_EVENTS_TO'] = str(wid)
        LOG.debug( 'Emed command: {}'.format(self.command))
        result = self.embed(self.command)
        if result:
            return
            self.x11mess = xembed.X11ClientMessage(self.external_id)


    def send_x11_message(self,):
        self.x11mess.send_client_message()

    def set_command(self, data):
        self.command = data
    def get_command(self):
        return self.command
    def reset_command(self):
        self.command = None

    command_string = pyqtProperty(str, get_command, set_command, reset_command)

if __name__ == '__main__':
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    ex = XEmbed()
    ex.embed('halcmd loadusr gladevcp --xid  ../../gladevcp/offsetpage.glade')
    ex.show()
    ex.setWindowTitle('embed')
    sys.exit(app.exec_())
