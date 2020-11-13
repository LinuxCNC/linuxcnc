#!/usr/bin/python3

import sys, os
from PyQt5.QtWidgets import (QApplication, QFileSystemModel,
                 QWidget, QVBoxLayout, QHBoxLayout, QListView,
                 QComboBox, QPushButton, QSizePolicy)
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

# Force the log level for this module
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
        self.user_path = (os.path.join('/media'))
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
        self.list.activated.connect(self._getPathActivated)
        #self.list.currentChanged = self.currentChanged
        self.list.setAlternatingRowColors(True)

        self.cb = QComboBox()
        self.cb.currentTextChanged.connect(self.filterChanged)
        self.cb.addItems(sorted({'*.ngc','*.py','*'}))
        #self.cb.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))

        self.button = QPushButton()
        self.button.setText('Media')
        self.button.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))
        self.button.setToolTip('Jump to Media directory')
        self.button.clicked.connect(self.onMediaClicked)

        self.button2 = QPushButton()
        self.button2.setText('User')
        self.button2.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))
        self.button2.setToolTip('Jump to linuxcnc directory')
        self.button2.clicked.connect(self.onUserClicked)

        hbox = QHBoxLayout()
        hbox.addWidget(self.button)
        hbox.addWidget(self.button2)
        hbox.addWidget(self.cb)

        windowLayout = QVBoxLayout()
        windowLayout.addWidget(self.list)
        windowLayout.addLayout(hbox)
        self.setLayout(windowLayout)
        self.show()

    # this could return the current/previous selected as it's selected.
    # need to uncomment monkey patch of self.list.currentChanged above
    # so far this is not needed
    def currentChanged(self,c,p):
        dir_path = self.model.filePath(c)
        print('-> ',dir_path)

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

    def onMediaClicked(self):
        self.updateDirectoryView(self.user_path)

    def onUserClicked(self):
        self.updateDirectoryView(self.default_path)

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

    # returns the current highlighted (selected) path as well as
    # whether it's a file or not.
    def getCurrentSelected(self):
        selectionModel = self.list.selectionModel()
        index = selectionModel.currentIndex()
        dir_path = self.model.filePath(index)
        if self.model.fileInfo(index).isFile():
            return (dir_path, True)
        else:
            return (dir_path, False)

    def _hal_init(self):
        if self.PREFS_:
            last_path = self.PREFS_.getpref('last_loaded_directory', self.default_path, str, 'BOOK_KEEPING')
            self.updateDirectoryView(last_path)
            LOG.debug("lAST FILE PATH: {}".format(last_path))
        else:
            LOG.debug("lAST FILE PATH: {}".format(self.default_path))
            self.updateDirectoryView(self.default_path)

    # get current selection and update the path
    # then if the path is good load it into linuxcnc
    # record it in the preference file if available
    def _getPathActivated(self):
        row = self.list.selectionModel().currentIndex()
        self.clicked(row)

        fname = self.currentPath
        if fname is None: 
            return
        if fname:
            self.load(fname)

    # this can be class patched to do something else
    def load(self, fname=None):
        if fname is None:
            self._getPathActivated()
            return
        self.recordBookKeeping()
        ACTION.OPEN_PROGRAM(fname)
        STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME')

    # this can be class patched to do something else
    def recordBookKeeping(self):
        fname = self.currentPath
        if fname is None: 
            return
        if self.PREFS_:
            self.PREFS_.putpref('last_loaded_directory', self.model.rootPath(), str, 'BOOK_KEEPING')
            self.PREFS_.putpref('RecentPath_0', fname, str, 'BOOK_KEEPING')

    # moves the selection up
    # used with MPG scrolling
    def up(self):
        self.select_row('up')

    # moves the selection down
    # used with MPG scrolling
    def down(self):
        self.select_row('down')

if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    gui = FileManager()
    gui.show()
    sys.exit(app.exec_())
