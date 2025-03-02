#!/usr/bin/env python3

import sys
import os
import shutil
from collections import OrderedDict

from PyQt5.QtCore import QCoreApplication
from PyQt5.QtWidgets import (QApplication, QFileSystemModel, QWidget, QVBoxLayout, QHBoxLayout,
                             QListView, QComboBox, QPushButton, QToolButton, QSizePolicy,
                             QMenu, QAction, QLineEdit, QCheckBox, QTableView, QHeaderView,
                                QMessageBox)
from PyQt5.QtGui import QIcon, QDragEnterEvent, QDropEvent
from PyQt5.QtCore import (QModelIndex, QDir, Qt, pyqtSlot, pyqtSignal, pyqtProperty, QFileInfo, QMimeData,
                          QItemSelectionModel, QItemSelection, QSortFilterProxyModel)

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

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

_translate = QCoreApplication.translate

class SharedViewMixin:
    itemDropped = pyqtSignal(list)
    def dragEnterEvent(self, event: QDragEnterEvent):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dragMoveEvent(self, event: QDragEnterEvent):
        event.acceptProposedAction()

    def dropEvent(self, event: QDropEvent):
        if event.mimeData().hasUrls():
            for url in event.mimeData().urls():
                source_path = url.toLocalFile()
                proxy_index = self.indexAt(event.pos())
                if proxy_index.isValid():
                    source_index = self.proxy_model.mapToSource(proxy_index)
                    if source_index.isValid():
                        destination_path = self.source_model.filePath(source_index)
                        if os.path.isdir(destination_path):
                            dest_file = os.path.join(destination_path, os.path.basename(source_path))
                        else:
                            dest_file = os.path.join(os.path.dirname(destination_path), os.path.basename(source_path))
                else:
                    destination_path = self.source_model.rootPath()
                    dest_file = os.path.join(destination_path, os.path.basename(source_path))
#                isdir = True if os.path.isdir(source_path) else False
                self.itemDropped.emit([source_path, dest_file])
            event.acceptProposedAction()
        else:
            event.ignore()

    
class CustomTableView(QTableView, SharedViewMixin):
    def __init__(self, proxy_model, parent=None):
        super().__init__(parent)
        self.setModel(proxy_model)
        self.proxy_model = proxy_model
        self.source_model = proxy_model.sourceModel()
        
        self.setDragEnabled(True)
        self.setAcceptDrops(True)
        self.setDropIndicatorShown(True)
        self.setDragDropMode(QTableView.DragDrop)


class CustomListView(QListView, SharedViewMixin):
    def __init__(self, proxy_model, parent=None):
        super().__init__(parent)
        self.setModel(proxy_model)
        self.proxy_model = proxy_model
        self.source_model = proxy_model.sourceModel()

        self.setDragEnabled(True)
        self.setAcceptDrops(True)
        self.setDropIndicatorShown(True)
        self.setDragDropMode(QListView.DragDrop)


class RestrictedProxyModel(QSortFilterProxyModel):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.base_path = '/home'
        self.restricted = False

    def filterAcceptsRow(self, source_row, source_parent):
        if not self.restricted:
            return True
        model = self.sourceModel()
        index = model.index(source_row, 0, source_parent)
        if not index.isValid():
            return False
        # Only allow paths inside the base path
        file_path = QDir(model.filePath(index)).canonicalPath()
        return file_path.startswith(self.base_path)

    def set_rootpath(self, path):
        self.base_path = path

    def get_rootpath(self):
        return self.base_path

    def set_restricted(self, state: bool):
        self.restricted = state
        self.invalidateFilter()

    def lessThan(self, left, right):
        # name
        if left.column() == 0:
            left_data = self.sourceModel().fileInfo(left)
            right_data = self.sourceModel().fileInfo(right)
            if left_data.isDir() and not right_data.isDir():
                return True
            if not left_data.isDir() and right_data.isDir():
                return False
            return left_data.fileName().lower() < right_data.fileName().lower()

        # size
        if left.column() == 1:
            left_data = self.sourceModel().size(left)
            right_data = self.sourceModel().size(right)
            return left_data < right_data

        # last modified
        if left.column() == 3:
            left_data = self.sourceModel().fileInfo(left)
            right_data = self.sourceModel().fileInfo(right)
            return left_data.lastModified() < right_data.lastModified()

        # default
        return False

class FileManager(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(FileManager, self).__init__(parent)
        self.title = _translate('FileManager','Qtvcp File System View')
        self._last = 0
        self._doubleClick = False
        self._showListView = False

        if INFO.PROGRAM_PREFIX is not None:
            self.user_path = os.path.expanduser(INFO.PROGRAM_PREFIX)
        else:
            self.user_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files'))
        user = os.path.split(os.path.expanduser('~') )[-1]

        # check for Ubuntu/Mint path first
        media = os.path.join('/media', user)
        if os.path.exists(media):
            self.media_path = media
        else:
            self.media_path = '/media'
        temp = [('User', self.user_path), ('Media', self.media_path)]
        self._jumpList = OrderedDict(temp)
        self.currentPath = None
        self.currentFolder = None
        self.jump_delete = []
        self.PREFS_ = None
        self.initUI()

    def initUI(self):
        self.setWindowTitle(self.title)
        self.setSizePolicy(QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred))
        line_policy = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        button_policy = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        box_policy = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)

        self.textLine = QLineEdit()
        self.textLine.setToolTip('Current Directory/selected File')
        self.textLine.setSizePolicy(line_policy)
        self.textLine.setMinimumHeight(40)
        self.pasteButton = QPushButton()
        self.pasteButton.setSizePolicy(button_policy)
        self.pasteButton.setMinimumSize(80, 40)
        self.pasteButton.setEnabled(False)
        self.pasteButton.setText('Paste')
        self.pasteButton.setToolTip('Copy file from copy path to current directory/file')
        self.pasteButton.clicked.connect(self.paste)
        self.pasteButton.hide()
        pasteBox = QHBoxLayout()
        pasteBox.addWidget(self.textLine)
        pasteBox.addWidget(self.pasteButton)

        self.copyLine = QLineEdit()
        self.copyLine.setSizePolicy(line_policy)
        self.copyLine.setToolTip('File path to copy from when pasting')
        self.copyLine.setMinimumHeight(40)
        self.copyLine.setReadOnly(True)
        self.copyButton = QPushButton()
        self.copyButton.setSizePolicy(button_policy)
        self.copyButton.setMinimumSize(80, 40)
        self.copyButton.setText('Copy')
        self.copyButton.setToolTip('Record current file as copy path')
        self.copyButton.clicked.connect(self.recordCopyPath)
        self.copyBox = QHBoxLayout()
        self.copyBox.addWidget(self.copyButton)
        self.copyBox.addWidget(self.copyLine)
        self.copyLine.hide()
        self.copyButton.hide()

        self.model = QFileSystemModel()
        self.model.setRootPath(QDir.currentPath())
        self.model.setFilter(QDir.AllDirs | QDir.NoDot | QDir.Files)
        self.model.setNameFilterDisables(False)
        self.model.rootPathChanged.connect(self.folderChanged)

        self.proxy_model = RestrictedProxyModel()
        self.proxy_model.setSourceModel(self.model)

        self.list = CustomListView(self.proxy_model)
        self.list.setModel(self.proxy_model)
        self.list.resize(640, 480)
        self.list.setAlternatingRowColors(True)
        self.list.hide()

        self.table = CustomTableView(self.proxy_model)
        self.table.setModel(self.proxy_model)
        self.table.resize(640, 480)
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
        self.cb.setMinimumSize(200,40)
        self.cb.setSizePolicy(QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed))

        self.jumpButton = QToolButton()
        self.jumpButton.setText(_translate('FileManager','User'))
        self.jumpButton.setSizePolicy(line_policy)
        self.jumpButton.setMinimumSize(40, 40)
        self.jumpButton.setMaximumSize(80, 40)
        self.jumpButton.setToolTip('Jump to User directory.\nLong press for Options.')
        self.jumpButton.clicked.connect(self.onJumpClicked)

        self.addButton = QPushButton()
        self.addButton.setText(_translate('FileManager','Add\nJump'))
        self.addButton.setSizePolicy(line_policy)
        self.addButton.setMinimumSize(40, 40)
        self.addButton.setMaximumSize(80, 40)
        self.addButton.setToolTip('Add current directory to jump button list')
        self.addButton.clicked.connect(self.onActionClicked)

        self.delButton = QPushButton()
        self.delButton.setText(_translate('FileManager','Del\nJump'))
        self.delButton.setSizePolicy(line_policy)
        self.delButton.setMinimumSize(40, 40)
        self.delButton.setMaximumSize(80, 40)
        self.delButton.setToolTip('Delete current directory from jump button list')
        self.delButton.clicked.connect(self.onActionClicked)

        self.loadButton = QPushButton()
        self.loadButton.setText(_translate('FileManager','Load'))
        self.loadButton.setSizePolicy(line_policy)
        self.loadButton.setMinimumSize(40, 40)
        self.loadButton.setMaximumSize(80, 40)
        self.loadButton.setToolTip('Load selected file')
        self.loadButton.clicked.connect(self._getPathActivated)

        self.chk_restricted = QCheckBox(_translate('FileManager','Restricted'))
        self.chk_restricted.setSizePolicy(box_policy)
        self.chk_restricted.setToolTip('Restrict navigation past root path')
        self.chk_restricted.stateChanged.connect(lambda state: self.setRestricted(state))

        self.copy_control = QCheckBox(_translate('FileManager','Copy Controls'))
        self.copy_control.setSizePolicy(box_policy)
        self.copy_control.stateChanged.connect(lambda state: self.showCopyControls(state))

        self.checkbox = QHBoxLayout()
        self.checkbox.addWidget(self.chk_restricted)
        self.checkbox.addWidget(self.copy_control)
        self.checkbox.insertStretch(2, stretch=0)

        self.settingMenu = QMenu(self)
        self.jumpButton.setMenu(self.settingMenu)

        hbox = QHBoxLayout()
        hbox.addWidget(self.jumpButton)
        hbox.addWidget(self.addButton)
        hbox.addWidget(self.delButton)
        hbox.addWidget(self.loadButton)
        hbox.addWidget(self.cb)

        self.windowLayout = QVBoxLayout()
        self.windowLayout.addLayout(pasteBox)
        self.windowLayout.addLayout(self.copyBox)
        self.windowLayout.addWidget(self.list)
        self.windowLayout.addWidget(self.table)
        self.windowLayout.addLayout(hbox)
        self.windowLayout.addLayout(self.checkbox)
        self.setLayout(self.windowLayout)

    def _hal_init(self):
        if self.PREFS_:
            last_path = self.PREFS_.getpref('last_loaded_directory', self.user_path, str, 'BOOK_KEEPING')
            LOG.debug("LAST FILE PATH: {}".format(last_path))
            if not last_path == '' and os.path.exists(last_path):
                self.updateDirectoryView(last_path)
            else:
                self.updateDirectoryView(self.user_path)

            # get all the saved jumplist paths
            temp = self.PREFS_.getall('FILEMANAGER_JUMPLIST')
            self._jumpList.update(temp)

        else:
            LOG.debug("LAST FILE PATH: {}".format(self.user_path))
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

        self.connectSelection()

    # when qtvcp closes this gets called
    # record jump list paths
    def _hal_cleanup(self):
        if self.PREFS_:
            for opt in self.jump_delete:
                self.PREFS_.removepref(opt, 'FILEMANAGER_JUMPLIST')
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
            self.list.selectionModel().clearSelection()
            self.list.setRootIndex(QModelIndex())
            self.table.selectionModel().clearSelection()
            self.table.setRootIndex(QModelIndex())
            index = self.model.setRootPath(path)
            if index.isValid():
                proxy_index = self.proxy_model.mapFromSource(index)
                self.list.setRootIndex(proxy_index)
                self.table.setRootIndex(proxy_index)
                self.proxy_model.invalidate()
        else:
            LOG.debug("Set directory view error - no such path {}".format(path))
            if not quiet:
                STATUS.emit('error', STATUS.TEMPARARY_MESSAGE, "File Manager error - No such path: {}".format(path))

    # retrieve selected filter (it's held as QT.userData)
    def filterChanged(self, index):
        userdata =  self.cb.itemData(index)
        self.model.setNameFilters(userdata)

    def listClicked(self, index):
        # the signal passes the index of the clicked item
        source_index = self.proxy_model.mapToSource(index)
        dir_path = os.path.normpath(self.model.filePath(source_index))
        if self.model.fileInfo(source_index).isFile():
            self.currentPath = dir_path
            self.textLine.setText(self.currentPath)
            return
        else:
            self.currentPath = None
            self.textLine.setText(os.path.abspath(dir_path))
            self.updateDirectoryView(dir_path)

    def onUserClicked(self):
        self.showUserDir()

    def onMediaClicked(self):
        self.showMediaDir()

    # jump directly to a saved path shown on the button
    def onJumpClicked(self):
        data = self.jumpButton.text()
        if data.upper() == 'MEDIA':
            self.showMediaDir()
        elif data.upper() == 'USER':
            self.showUserDir()
        elif data in self._jumpList:
            temp = self._jumpList[data]
            if temp is not None:
                self.updateDirectoryView(temp)
            else:
                STATUS.emit('error',STATUS.OPERATOR_ERROR, 'file jumppath: {} not valid'.format(data))
                log.debug('file jumppath: {} not valid'.format(data))
        else:
            self.jumpButton.setText('User')

    # jump directly to a saved path from the menu
    def jumpTriggered(self, data):
        name = data
        self.jumpButton.setText(name)
        if data.upper() == 'MEDIA':
            self.jumpButton.setToolTip('Jump to Media directory.\nLong press for Options.')
            self.showMediaDir()
        elif data.upper() == 'USER':
            self.jumpButton.setToolTip('Jump to User directory.\nLong press for Options.')
            self.showUserDir()
        else:
            self.jumpButton.setToolTip('Jump to directory:\n{}'.format(self._jumpList.get(name)))
            self.updateDirectoryView(self._jumpList.get(name))

    # add or remove a jump list path
    def onActionClicked(self):
        name = os.path.basename(self.currentFolder)
        btn = self.sender()
        if btn == self.addButton:
            try:
                self._jumpList[name] = self.currentFolder
                self.addAction(name)
            except Exception as e:
                print(e)
        elif btn == self.delButton:
            try:
                self.jump_delete.append(name)
                self._jumpList.pop(name)
                self.settingMenu.clear()
                for key in self._jumpList:
                    self.addAction(key)
                self.jumpButton.setText('User')
            except Exception as e:
                print(e)

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
            STATUS.emit('error', STATUS.OPERATOR_ERROR, 'Can only copy a file, not a folder')

    def paste(self):
        res = self.copyFile(self.copyLine.text(), self.textLine.text())
        if res:
            self.copyLine.setText('')
            self.pasteButton.setEnabled(False)

    ########################
    # helper functions
    ########################

    def setRestricted(self, state):
        self.proxy_model.set_restricted(state)
        self.updateDirectoryView(self.proxy_model.get_rootpath())

    def addAction(self, i):
        action = QAction(QIcon.fromTheme('user-home'), i, self)
        # weird lambda i=i to work around 'function closure'
        action.triggered.connect(lambda state, i=i: self.jumpTriggered(i))
        self.settingMenu.addAction(action)

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
            self.copyLine.show()
            self.copyButton.show()
            self.pasteButton.show()
        else:
            self.copyLine.hide()
            self.copyButton.hide()
            self.pasteButton.hide()

    def showMediaDir(self, quiet = False):
        self.proxy_model.set_rootpath(self.media_path)
        self.jumpButton.setText(_translate('FileManager','Media'))
        self.updateDirectoryView(self.media_path, quiet)

    def showUserDir(self, quiet = False):
        self.proxy_model.set_rootpath(self.user_path)
        self.jumpButton.setText(_translate('FileManager','User'))
        self.updateDirectoryView(self.user_path, quiet)

    def dropCopy(self, data):
        source, dest = data
        self.copyChecks(source, dest)

    def copyFile(self, s, d):
        self.copyChecks(source, dest)

    def overwriteMessage(self, d):
        title = "File Already Exists"
        info = f"Overwrite {d}?"
        buttons = QMessageBox.No | QMessageBox.Yes
        retval =  QMessageBox.warning(
            self, title, info, buttons)
        if retval == QMessageBox.No:
            return False
        return True

    # likely class patch candidate 
    def copyChecks(self, s, d):
        if os.path.isfile(d) or os.path.isdir(d):
            retval = self.overwriteMessage(d)
            if retval == False:
                txt = f'File {d} not copied'
                mess = {'SHORTTEXT':txt,'TITLE':'FileManager',
                    'DETAILS':txt}
                opt = {'LOG':True,'LEVEL':STATUS.WARNING}
                STATUS.emit('status-message',mess,opt)
                return False
        self.copy(s, d)

    def copy(self, s, d):
        try:
            if os.path.isdir(s):
                shutil.copytree(s, d)
            else:
                shutil.copy2(s, d)
            txt = f' Copied {s} to {d}'
            mess = {'SHORTTEXT':txt,'TITLE':'FileManager',
                    'DETAILS':txt}
            opt = {'LOG':True,'LEVEL':STATUS.DEFAULT}
            STATUS.emit('status-message',mess,opt)
            return True
        except Exception as e:
            STATUS.emit('error', STATUS.OPERATOR_ERROR, "Copy file error: {}".format(e))
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
        index = self.proxy_model.mapToSource(selectionModel.currentIndex())
        dir_path = os.path.normpath(self.model.filePath(index))
        if self.model.fileInfo(index).isFile():
            return (dir_path, True)
        elif dir_path == '.':
            return (self.proxy_model.get_rootpath(), False)
        else:
            return (dir_path, False)

    # This can be class patched to do something else
    def load(self, fname=None):
        return
        try:
            if fname is None:
                self._getPathActivated()
                return
            self.recordBookKeeping()
            ACTION.OPEN_PROGRAM(fname)
            STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME,SUCCESS')
        except Exception as e:
            LOG.error("Load file error: {}".format(e))
            STATUS.emit('error', STATUS.NML_ERROR, "Load file error: {}".format(e))

    # This can be class patched to do something else
    def recordBookKeeping(self):
        fname = self.currentPath
        if fname is None:
            return
        if self.PREFS_:
            self.PREFS_.putpref('last_loaded_directory', self.model.rootPath(), str, 'BOOK_KEEPING')
            self.PREFS_.putpref('RecentPath_0', fname, str, 'BOOK_KEEPING')

    def connectSelection(self):
        try:
            self.list.disconnect()
            self.table.disconnect()
            self.list.activated.connect(self._getPathActivated)
            self.table.activated.connect(self._getPathActivated)
        except:
            pass

        self.list.itemDropped.connect(self.dropCopy)
        self.table.itemDropped.connect(self.dropCopy)

        # choose double click or single click for folder selection
        if self._doubleClick:
            self.list.doubleClicked[QModelIndex].connect(self.listClicked)
            self.table.doubleClicked[QModelIndex].connect(self.listClicked)
        else:
            self.list.clicked[QModelIndex].connect(self.listClicked)
            self.table.clicked[QModelIndex].connect(self.listClicked)

    ######################
    # Properties
    ######################

    # Double Click folder selection
    def setDoubleClickSelection(self, state):
        self._doubleClick = state
        self.connectSelection()
    def getDoubleClickSelection(self):
        return self._doubleClick
    def resetDoubleClickSelection(self, state):
        self._doubleClick = False
        self.connectSelection()
    doubleClickSelection = pyqtProperty(bool, getDoubleClickSelection, setDoubleClickSelection,  resetDoubleClickSelection)

    # list/table view selection
    def setShowListView(self, state):
        self._showListView = state
        self.showList(state)
    def getShowListView(self):
        return self._showListView
    def resetShowListView(self, state):
        self._showListView = False
        self.showList(False)
    showListView = pyqtProperty(bool, getShowListView, setShowListView,  resetShowListView)

if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    gui = FileManager()
    gui.show()
    gui.PREFS_ = True
    gui.connectSelection()
    gui.onUserClicked()
#    gui.onMediaClicked()
    gui.setRestricted(True)
    sys.exit(app.exec_())

