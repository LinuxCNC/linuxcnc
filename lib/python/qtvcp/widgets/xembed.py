import sys
import subprocess

from PyQt5.QtCore import QSize, QEvent, pyqtProperty
from PyQt5.QtGui import QWindow, QResizeEvent
from PyQt5.QtWidgets import QWidget

from qtvcp.widgets.widget_baseclass import _HalWidgetBase

class XEmbeddable(QWidget):
    def __init__(self, parent=None):
        super(XEmbeddable, self).__init__(parent)

    def embed(self, command):
        try:
            window = QWindow.fromWinId(self.launch_xid(command))
            self.container = QWidget.createWindowContainer(window, self)
            self.show()
        except  Exception,e:
            print 'Error Embedding program: ',command
            print e

    def event(self, event):
        if event.type() ==  QEvent.Resize:
            w = QResizeEvent.size(event)
            try:
                self.container.resize(w.width(), w.height())
            except:
                pass
        return True

    def launch_xid(self,cmd):
        c = cmd.split()
        #print c
        self.ob = subprocess.Popen(c,stdin = subprocess.PIPE,
                                   stdout = subprocess.PIPE)
        sid = self.ob.stdout.readline()
        #print 'XID:',sid
        return int(sid)

class XEmbed(XEmbeddable, _HalWidgetBase):
    def __init__(self, parent=None):
        super(XEmbed, self).__init__(parent)
        self.command = None

    def _hal_init(self):
        self.embed(self.command)

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
