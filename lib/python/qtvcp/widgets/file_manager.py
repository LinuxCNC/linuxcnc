import sys, os
from PyQt5.QtWidgets import (QApplication, QFileSystemModel,
                 QWidget, QVBoxLayout, QListView, QComboBox)
from PyQt5.QtGui import QIcon
from PyQt5.QtCore import (QModelIndex, QDir, Qt,
                    QItemSelectionModel, QEvent, QItemSelection)

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

class FileManager(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(FileManager, self).__init__(parent)
        self.title = 'PyQt5 file system view - pythonspot.com'
        self.left = 10
        self.top = 10
        self.width = 640
        self.height = 480
        self.default_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files/examples'))
        self.currentPath = None
        self.initUI()

    def initUI(self):
        self.setWindowTitle(self.title)
        self.setGeometry(self.left, self.top, self.width, self.height)

        self.model = QFileSystemModel()
        self.model.setRootPath(QDir.currentPath())
        self.model.setFilter(QDir.AllDirs | QDir.NoDot | QDir.Files)
        self.model.setNameFilterDisables(False)
        self.model.setNameFilters(["*.ngc",'*.py'])

        self.list = QListView()
        self.list.setModel(self.model)
        self.updateDirectoryView(self.default_path)
        self.list.setWindowTitle("Dir View")
        self.list.resize(640, 480)
        self.list.clicked[QModelIndex].connect(self.clicked)
        self.list.activated.connect(self.load)
        self.list.setAlternatingRowColors(True)

        self.cb = QComboBox()
        self.cb.currentTextChanged.connect(self.filterChanged)
        self.cb.addItems(sorted({'*.ngc','*.py','*'}))
        windowLayout = QVBoxLayout()
        windowLayout.addWidget(self.list)
        windowLayout.addWidget(self.cb)
        self.setLayout(windowLayout)
        self.show()

    def updateDirectoryView(self, path):
        self.list.setRootIndex(self.model.setRootPath(path))

    def filterChanged(self, text):
        self.model.setNameFilters([text])

    def clicked(self, index):
        # the signal passes the index of the clicked item
        dir_path = self.model.filePath(index)
        if self.model.fileInfo(index).isFile():
            self.currentPath = dir_path
            return
        root_index = self.model.setRootPath(dir_path)
        self.list.setRootIndex(root_index)

    def select_row(self, style):
        style = style.lower()
        selectionModel = self.list.selectionModel()
        row = selectionModel.currentIndex().row()
        self.rows = self.model.rowCount(self.list.rootIndex())

        if style == 'last':
            row = self.rows
        elif style == 'up':
            if row > 0:
                row -= 1
            else:
                row = 0
        elif style == 'down':
            if row < self.rows:
                row += 1
            else:
                row = self.rows
        else:
            return
        top = self.model.index(row, 0, self.list.rootIndex())
        selectionModel.setCurrentIndex(top, QItemSelectionModel.Select | QItemSelectionModel.Rows)
        selection = QItemSelection(top, top)
        selectionModel.clearSelection()
        selectionModel.select(selection, QItemSelectionModel.Select)

    def _hal_init(self):
        if self.PREFS_:
            last_path = self.PREFS_.getpref('last_file_path', self.default_path, str, 'BOOK_KEEPING')
            self.updateDirectoryView(last_path)
            LOG.debug("lAST FILE PATH: {}".format(last_path))
        else:
            LOG.debug("lAST FILE PATH: {}".format(self.default_path))
            self.updateDirectoryView(self.default_path)

    # get current selection and update the path
    # then if the path is good load it into linuxcnc
    # record it in the preference file if available
    def load(self):
        row = self.list.selectionModel().currentIndex()
        self.clicked(row)

        fname = self.currentPath
        if fname is None: 
            return
        if fname:
            if self.PREFS_:
                self.PREFS_.putpref('last_file_path', fname, str, 'BOOK_KEEPING')
            ACTION.OPEN_PROGRAM(fname)
            STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME')

    def up(self):
        self.select_row('up')

    def down(self):
        self.select_row('down')

if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    gui = FileManager()
    gui.show()
    sys.exit(app.exec_())
