import os
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
# INFO holds INI dile details
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class FileManagerBase(QFileDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(FileManagerBase, self).__init__(parent)
        self.setWindowFlags(self.windowFlags() & ~Qt.Dialog)
        self.setOption(QFileDialog.DontUseNativeDialog)
        self.setFileMode(QFileDialog.ExistingFile)
        exts = INFO.get_qt_filter_extensions()
        self.setNameFilter(exts)
        self.default_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files/examples'))

        b = self.findChildren(QPushButton)
        # hide close button
        b[1].hide()

    def _hal_init(self):
        if self.PREFS_:
            last_path = self.PREFS_.getpref('last_file_path', self.default_path, str, 'BOOK_KEEPING')
            self.setDirectory(last_path)

    # override open button
    def accept(self):
        print 'open', self.selectedFiles()

        fname = self.selectedFiles()[0]
        path = self.directory().absolutePath()
        self.setDirectory(path)
        if fname:
            if self.PREFS_:
                self.PREFS_.putpref('last_file_path', path, str, 'BOOK_KEEPING')
            ACTION.OPEN_PROGRAM(fname)
            STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME')

class FileManager(QWidget):
    def __init__(self, parent=None):
        super(FileManager, self).__init__(parent)
        self.d = FileManagerBase()
        lay = QVBoxLayout()
        self.setLayout(lay)
        lay.addWidget(self.d)

if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    gui = W()
    gui.show()
    sys.exit(app.exec_())
