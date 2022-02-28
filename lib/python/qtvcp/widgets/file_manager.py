#!/usr/bin/env python3

import sys
import os
import shutil
from collections import OrderedDict

from PyQt5.QtWidgets import (QApplication, QFileSystemModel,
                 QWidget, QVBoxLayout, QHBoxLayout, QListView,
                 QComboBox, QPushButton, QToolButton, QSizePolicy,
                 QMenu, QAction, QLineEdit, QLabel, QFrame,
                    QTableView, QHeaderView)
from PyQt5.QtGui import QIcon
from PyQt5.QtCore import (QModelIndex, QDir, Qt, pyqtSlot,
                    QItemSelectionModel, QEvent, QItemSelection)

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger
from linuxcnc import OPERATOR_ERROR, NML_ERROR
LOW_ERROR = 255

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
        self._last = 0

        if INFO.PROGRAM_PREFIX is not None:
            self.user_path = os.path.expanduser(INFO.PROGRAM_PREFIX)
        else:
            self.user_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files'))
        user = os.path.split(os.path.expanduser('~') )[-1]
        self.media_path = (os.path.join('/media', user))
        temp = [('User', self.user_path), ('Media', self.media_path)]
        self._jumpList = OrderedDict(temp)
        self.currentPath = None
        self.currentFolder = None
        self.PREFS_ = None
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
        self.list.resize(640, 480)
        self.list.clicked[QModelIndex].connect(self.listClicked)
        self.list.activated.connect(self._getPathActivated)
        self.list.setAlternatingRowColors(True)
        self.list.hide()

        self.table = QTableView()
        self.table.setModel(self.model)
        self.table.resize(640, 480)
        self.table.clicked[QModelIndex].connect(self.listClicked)
        self.table.activated.connect(self._getPathActivated)
        self.table.setAlternatingRowColors(True)

        header = self.table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.Stretch)
        header.setSectionResizeMode(1, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.ResizeToContents)
        header.swapSections(1,3)
        header.setSortIndicator(1,Qt.AscendingOrder)

        self.table.setSortingEnabled(True)
        self.table.setColumnHidden(2, True) # type
        self.table.verticalHeader().setVisible(False) # row count header

        self.cb = QComboBox()
        self.cb.currentIndexChanged.connect(self.filterChanged)
        self.fillCombobox(INFO.PROGRAM_FILTERS_EXTENSIONS)
        self.cb.setMinimumHeight(30)
        self.cb.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))

        self.button2 = QToolButton()
        self.button2.setText('User')
        self.button2.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))
        self.button2.setMinimumSize(60, 30)
        self.button2.setToolTip('Jump to User directory.\nLong press for Options.')
        self.button2.clicked.connect(self.onJumpClicked)

        self.button3 = QToolButton()
        self.button3.setText('Add Jump')
        self.button3.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))
        self.button3.setMinimumSize(60, 30)
        self.button3.setToolTip('Add current directory to jump button list')
        self.button3.clicked.connect(self.onActionClicked)

        self.settingMenu = QMenu(self)
        self.button2.setMenu(self.settingMenu)

        hbox = QHBoxLayout()
        hbox.addWidget(self.button2)
        hbox.addWidget(self.button3)
        hbox.insertStretch (2, stretch = 0)
        hbox.addWidget(self.cb)

        windowLayout = QVBoxLayout()
        windowLayout.addLayout(pasteBox)
        windowLayout.addWidget(self.copyBox)
        windowLayout.addWidget(self.list)
        windowLayout.addWidget(self.table)
        windowLayout.addLayout(hbox)
        self.setLayout(windowLayout)
        self.show()

    def _hal_init(self):
        if self.PREFS_:
            last_path = self.PREFS_.getpref('last_loaded_directory', self.user_path, str, 'BOOK_KEEPING')
            LOG.debug("lAST FILE PATH: {}".format(last_path))
            if not last_path == '':
                self.updateDirectoryView(last_path)
            else:
                self.updateDirectoryView(self.user_path)


            # get all the saved jumplist paths
            temp = self.PREFS_.getall('FILEMANAGER_JUMPLIST')
            self._jumpList.update(temp)

        else:
            LOG.debug("lAST FILE PATH: {}".format(self.user_path))
            self.updateDirectoryView(self.user_path)

        # install jump paths into toolbutton menu
        for i in self._jumpList:
            self.addAction(i)

        # set recorded columns sort settings
        self.SETTINGS_.beginGroup("FileManager-{}".format(self.objectName()))
        sect = self.SETTINGS_.value('sortIndicatorSection', type = int)
        order = self.SETTINGS_.value('sortIndicatorOrder', type = int)
        self.SETTINGS_.endGroup()
        if not None in(sect,order):
            self.table.horizontalHeader().setSortIndicator(sect,order)

    # when qtvcp closes this gets called
    # record jump list paths
    def _hal_cleanup(self):
        if self.PREFS_:
            for i, key in enumerate(self._jumpList):
                if i in(0,1):
                    continue
                self.PREFS_.putpref(key, self._jumpList.get(key), str, 'FILEMANAGER_JUMPLIST')

        # record sorted columns
        h = self.table.horizontalHeader()
        self.SETTINGS_.beginGroup("FileManager-{}".format(self.objectName()))
        self.SETTINGS_.setValue('sortIndicatorSection', h.sortIndicatorSection())
        self.SETTINGS_.setValue('sortIndicatorOrder', h.sortIndicatorOrder())
        self.SETTINGS_.endGroup()

    #########################
    # callbacks
    #########################

    # add shown text and hidden filter data from the INI
    def fillCombobox(self, data):
        for i in data:
            self.cb.addItem(i[0],i[1])

    def folderChanged(self, data):
        data = os.path.normpath(data)
        self.currentFolder = data
        self.textLine.setText(data)

    def updateDirectoryView(self, path, quiet = False):
        if os.path.exists(path):
            self.list.setRootIndex(self.model.setRootPath(path))
            self.table.setRootIndex(self.model.setRootPath(path))
        else:
            LOG.debug("Set directory view error - no such path {}".format(path))
            if not quiet:
                STATUS.emit('error', LOW_ERROR, "File Manager error - No such path: {}".format(path))

    # retrieve selected filter (it's held as QT.userData)
    def filterChanged(self, index):
        userdata =  self.cb.itemData(index)
        self.model.setNameFilters(userdata)

    def listClicked(self, index):
        # the signal passes the index of the clicked item
        dir_path = os.path.normpath(self.model.filePath(index))
        if self.model.fileInfo(index).isFile():
            self.currentPath = dir_path
            self.textLine.setText(self.currentPath)
            return
        root_index = self.model.setRootPath(dir_path)
        self.list.setRootIndex(root_index)
        self.table.setRootIndex(root_index)

    def onUserClicked(self):
        self.showUserDir()

    def onMediaClicked(self):
        self.showMediaDir()

    # jump directly to a saved path shown on the button
    def onJumpClicked(self):
        data = self.button2.text()
        if data.upper() == 'MEDIA':
            self.showMediaDir()
        elif data.upper() == 'USER':
            self.showUserDir()
        else:
            temp = self._jumpList.get(data)
            if temp is not None:
                self.updateDirectoryView(temp)
            else:
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, 'file jumopath: {} not valid'.format(data))
                log.debug('file jumopath: {} not valid'.format(data))

    # jump directly to a saved path from the menu
    def jumpTriggered(self, data):
        if data.upper() == 'MEDIA':
            self.button2.setText('{}'.format(data))
            self.button2.setToolTip('Jump to Media directory.\nLong press for Options.')
            self.showMediaDir()
        elif data.upper() == 'USER':
            self.button2.setText('{}'.format(data))
            self.button2.setToolTip('Jump to User directory.\nLong press for Options.')
            self.showUserDir()
        else:
            self.button2.setText('{}'.format(data))
            self.button2.setToolTip('Jump to directory:\n{}'.format(self._jumpList.get(data)))
            self.updateDirectoryView(self._jumpList.get(data))

    # add a jump list path
    def onActionClicked(self):
        i = self.currentFolder
        try:
            self._jumpList[i] = i
        except Exception as e:
            print(e)
        button = QAction(QIcon.fromTheme('user-home'), i, self)
        # weird lambda i=i to work around 'function closure'
        button.triggered.connect(lambda state, i=i: self.jumpTriggered(i))
        self.settingMenu.addAction(button)

    # get current selection and update the path
    # then if the path is good load it into linuxcnc
    # record it in the preference file if available
    def _getPathActivated(self):
        if self.list.isVisible():
            row = self.list.selectionModel().currentIndex()
        else:
            row = self.table.selectionModel().currentIndex()
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

    def addAction(self, i):
        axisButton = QAction(QIcon.fromTheme('user-home'), i, self)
        # weird lambda i=i to work around 'function closure'
        axisButton.triggered.connect(lambda state, i=i: self.jumpTriggered(i))
        self.settingMenu.addAction(axisButton)

    def showList(self, state=True):
        if state:
            self.table.hide()
            self.list.show()
        else:
            self.table.show()
            self.list.hide()

    def showTable(self, state=True):
        self.showList(not state)

    def showCopyControls(self, state):
        if state:
            self.copyBox.show()
            self.pasteButton.show()
        else:
            self.copyBox.hide()
            self.pasteButton.hide()

    def showMediaDir(self, quiet = False):
        self.updateDirectoryView(self.media_path, quiet)

    def showUserDir(self, quiet = False):
        self.updateDirectoryView(self.user_path, quiet)

    def copyFile(self, s, d):
        try:
            shutil.copy(s, d)
            return True
        except Exception as e:
            LOG.error("Copy file error: {}".format(e))
            STATUS.emit('error', OPERATOR_ERROR, "Copy file error: {}".format(e))
            return False

    @pyqtSlot(float)
    @pyqtSlot(int)
    def scroll(self, data):
        if data > self._last:
            self.up()
        elif data < self._last:
            self.down()
        self._last = data

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
        if self.list.isVisible():
            i = self.list.rootIndex()
            selectionModel = self.list.selectionModel()
        else:
            i = self.table.rootIndex()
            selectionModel = self.table.selectionModel()

        row = selectionModel.currentIndex().row()
        self.rows = self.model.rowCount(i)

        if style == 'last':
            row = self.rows
        elif style == 'up':
            if row > 0:
                row -= 1
            else:
                row = 0
        elif style == 'down':
            if row < self.rows-1:
                row += 1
            else:
                row = self.rows-1
        else:
            return
        top = self.model.index(row, 0, i)
        selectionModel.setCurrentIndex(top, QItemSelectionModel.Select | QItemSelectionModel.Rows)
        selection = QItemSelection(top, top)
        selectionModel.clearSelection()
        selectionModel.select(selection, QItemSelectionModel.Select)

    # returns the current highlighted (selected) path as well as
    # whether it's a file or not.
    def getCurrentSelected(self):
        if self.list.isVisible():
            selectionModel = self.list.selectionModel()
        else:
            selectionModel = self.table.selectionModel()
        index = selectionModel.currentIndex()
        dir_path = os.path.normpath(self.model.filePath(index))
        if self.model.fileInfo(index).isFile():
            return (dir_path, True)
        else:
            return (dir_path, False)

    # This can be class patched to do something else
    def load(self, fname=None):
        try:
            if fname is None:
                self._getPathActivated()
                return
            self.recordBookKeeping()
            ACTION.OPEN_PROGRAM(fname)
            STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME')
        except Exception as e:
            LOG.error("Load file error: {}".format(e))
            STATUS.emit('error', NML_ERROR, "Load file error: {}".format(e))

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
