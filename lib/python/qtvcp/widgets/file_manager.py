#!/usr/bin/python3

import sys
import os
import shutil

from PyQt5.QtWidgets import (QApplication, QFileSystemModel,
                 QWidget, QVBoxLayout, QHBoxLayout, QListView,
                 QComboBox, QPushButton, QToolButton,QSizePolicy,
                 QMenu, QAction, QLineEdit, QLabel, QFrame)
from PyQt5.QtGui import QIcon
from PyQt5.QtCore import (QModelIndex, QDir, Qt,
                    QItemSelectionModel, QEvent, QItemSelection)

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger
from linuxcnc import OPERATOR_ERROR

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
# INFO holds INI file details
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
        self.title = 'Qtvcp File System View'
        self.left = 10
        self.top = 10
        self.width = 640
        self.height = 480
        self.media_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files'))
        user = os.path.split(os.path.expanduser('~') )[-1]
        self.user_path = (os.path.join('/media', user))
        self.currentPath = None
        self.currentFolder = None
        self.initUI()

    def initUI(self):
        self.setWindowTitle(self.title)
        self.setGeometry(self.left, self.top, self.width, self.height)

        pasteBox = QHBoxLayout()
        self.textLine = QLineEdit()
        self.textLine.setToolTip('Current Director/selected File')
        self.pasteButton = QToolButton()
        self.pasteButton.setEnabled(False)
        self.pasteButton.setText('Paste')
        self.pasteButton.setToolTip('Copy file from copy path to current directory/file')
        self.pasteButton.clicked.connect(self.paste)
        self.pasteButton.hide()
        pasteBox.addWidget(self.textLine)
        pasteBox.addWidget(self.pasteButton)

        self.copyBox = QFrame()
        hbox = QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        self.copyLine = QLineEdit()
        self.copyLine.setToolTip('File path to copy from, when pasting')
        self.copyButton = QToolButton()
        self.copyButton.setText('Copy')
        self.copyButton.setToolTip('Record current file as copy path')
        self.copyButton.clicked.connect(self.recordCopyPath)
        hbox.addWidget(self.copyButton)
        hbox.addWidget(self.copyLine)
        self.copyBox.setLayout(hbox)
        self.copyBox.hide()

        self.model = QFileSystemModel()
        self.model.setRootPath(QDir.currentPath())
        self.model.setFilter(QDir.AllDirs | QDir.NoDot | QDir.Files)
        self.model.setNameFilterDisables(False)
        self.model.rootPathChanged.connect(self.folderChanged)

        self.list = QListView()
        self.list.setModel(self.model)
        self.updateDirectoryView(self.media_path)
        self.list.resize(640, 480)
        self.list.clicked[QModelIndex].connect(self.listClicked)
        self.list.activated.connect(self._getPathActivated)
        self.list.setAlternatingRowColors(True)

        self.cb = QComboBox()
        self.cb.currentIndexChanged.connect(self.filterChanged)
        self.fillCombobox(INFO.PROGRAM_FILTERS_EXTENSIONS)
        self.cb.setMinimumHeight(30)
        self.cb.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))

        self.button2 = QToolButton()
        self.button2.setText('Media')
        self.button2.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))
        self.button2.setMinimumSize(60, 30)
        self.button2.setToolTip('Jump to Media directory')
        self.button2.clicked.connect(self.onJumpClicked)

        SettingMenu = QMenu(self)
        self.settingMenu = SettingMenu
        for i in ('Media', 'User'):
            axisButton = QAction(QIcon.fromTheme('user-home'), i, self)
            # weird lambda i=i to work around 'function closure'
            axisButton.triggered.connect(lambda state, i=i: self.jumpTriggered(i))
            SettingMenu.addAction(axisButton)
        self.button2.setMenu(SettingMenu)

        self.button3 = QToolButton()
        self.button3.setText('Add Jump')
        self.button3.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))
        self.button3.setMinimumSize(60, 30)
        self.button3.setToolTip('Add current directory to jump button list')
        self.button3.clicked.connect(self.onActionClicked)

        hbox = QHBoxLayout()
        hbox.addWidget(self.button2)
        hbox.addWidget(self.button3)
        hbox.insertStretch (2, stretch = 0)
        hbox.addWidget(self.cb)

        windowLayout = QVBoxLayout()
        windowLayout.addLayout(pasteBox)
        windowLayout.addWidget(self.copyBox)
        windowLayout.addWidget(self.list)
        windowLayout.addLayout(hbox)
        self.setLayout(windowLayout)
        self.show()

    def _hal_init(self):
        if self.PREFS_:
            last_path = self.PREFS_.getpref('last_loaded_directory', self.media_path, str, 'BOOK_KEEPING')
            self.updateDirectoryView(last_path)
            LOG.debug("lAST FILE PATH: {}".format(last_path))
        else:
            LOG.debug("lAST FILE PATH: {}".format(self.media_path))
            self.updateDirectoryView(self.media_path)

    #########################
    # callbacks
    #########################

    # add shown text and hidden filter data from the INI
    def fillCombobox(self, data):
        for i in data:
            self.cb.addItem(i[0],i[1])

    def folderChanged(self, data):
        self.currentFolder = data
        self.textLine.setText(data)

    def updateDirectoryView(self, path):
        self.list.setRootIndex(self.model.setRootPath(path))

    # retrieve selected filter (it's held as QT.userData)
    def filterChanged(self, index):
        userdata =  self.cb.itemData(index)
        self.model.setNameFilters(userdata)

    def listClicked(self, index):
        # the signal passes the index of the clicked item
        dir_path = self.model.filePath(index)
        if self.model.fileInfo(index).isFile():
            self.currentPath = dir_path
            self.textLine.setText(self.currentPath)
            return
        root_index = self.model.setRootPath(dir_path)
        self.list.setRootIndex(root_index)

    def onUserClicked(self):
        self.showUserDir()

    def onMediaClicked(self):
        self.showMediaDir()

    def onJumpClicked(self):
        data = self.button2.text()
        if data == 'Media':
            self.showMediaDir()
        elif data == 'User':
            self.showUserDir()
        else:
            self.updateDirectoryView(self.button2.text())

    def jumpTriggered(self, data):
        if data == 'Media':
            self.button2.setText('{}'.format(data))
            self.button2.setToolTip('Jump to Media directory')
            self.showMediaDir()
        elif data == 'User':
            self.button2.setText('{}'.format(data))
            self.button2.setToolTip('Jump to User directory')
            self.showUserDir()
        else:
            self.button2.setText('{}'.format(data))
            self.button2.setToolTip('Jump to directory: {}'.format(data))
            self.updateDirectoryView(self.button2.text())

    def onActionClicked(self):
        i = self.currentFolder
        button = QAction(QIcon.fromTheme('user-home'), i, self)
        # weird lambda i=i to work around 'function closure'
        button.triggered.connect(lambda state, i=i: self.jumpTriggered(i))
        self.settingMenu.addAction(button)

    # get current selection and update the path
    # then if the path is good load it into linuxcnc
    # record it in the preference file if available
    def _getPathActivated(self):
        row = self.list.selectionModel().currentIndex()
        self.listClicked(row)

        fname = self.currentPath
        if fname is None: 
            return
        if fname:
            self.load(fname)

    def recordCopyPath(self):
        data, isFile = self.getCurrentSelected()
        if isFile:
            self.copyLine.setText(os.path.normpath(data))
            self.pasteButton.setEnabled(True)
        else:
            self.copyLine.setText('')
            self.pasteButton.setEnabled(False)
            STATUS.emit('error', OPERATOR_ERROR, 'Can only copy a file, not a folder')

    def paste(self):
        res = self.copyFile(self.copyLine.text(), self.textLine.text())
        if res:
            self.copyLine.setText('')
            self.pasteButton.setEnabled(False)

    ########################
    # helper functions
    ########################

    def showCopyControls(self, state):
        if state:
            self.copyBox.show()
            self.pasteButton.show()
        else:
            self.copyBox.hide()
            self.pasteButton.hide()

    def showMediaDir(self):
        self.updateDirectoryView(self.user_path)

    def showUserDir(self):
        self.updateDirectoryView(self.media_path)

    def copyFile(self, s, d):
        try:
            shutil.copy(s, d)
            return True
        except Exception as e:
            LOG.error("Copy file error: {}".format(e))
            STATUS.emit('error', OPERATOR_ERROR, "Copy file error: {}".format(e))
            return False

    # moves the selection up
    # used with MPG scrolling
    def up(self):
        self.select_row('up')

    # moves the selection down
    # used with MPG scrolling
    def down(self):
        self.select_row('down')

    def select_row(self, style='down'):
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

    # This can be class patched to do something else
    def load(self, fname=None):
        if fname is None:
            self._getPathActivated()
            return
        self.recordBookKeeping()
        ACTION.OPEN_PROGRAM(fname)
        STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME')

    # This can be class patched to do something else
    def recordBookKeeping(self):
        fname = self.currentPath
        if fname is None: 
            return
        if self.PREFS_:
            self.PREFS_.putpref('last_loaded_directory', self.model.rootPath(), str, 'BOOK_KEEPING')
            self.PREFS_.putpref('RecentPath_0', fname, str, 'BOOK_KEEPING')


if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    gui = FileManager()
    gui.showCopyControls(True)
    gui.show()
    sys.exit(app.exec_())
