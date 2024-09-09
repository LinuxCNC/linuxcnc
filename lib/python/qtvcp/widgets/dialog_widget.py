#!/usr/bin/env python3
# QTVcp Widget
#
# Copyright (c) 2017 Chris Morley
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.



import os
import hal

from PyQt5.QtWidgets import (QMessageBox, QFileDialog, QDesktopWidget,
        QDialog, QDialogButtonBox, QVBoxLayout, QPushButton, QHBoxLayout,
        QHBoxLayout, QLineEdit, QPushButton, QDialogButtonBox, QTabWidget,
        QTextEdit,QLabel)
from PyQt5.QtGui import QColor
from PyQt5.QtCore import Qt, pyqtSlot, pyqtProperty, QEvent, QUrl
from PyQt5 import uic

from qtvcp.widgets.widget_baseclass import _HalWidgetBase, hal
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFVIEW_WIDGET
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOLVIEW_WIDGET
from qtvcp.widgets.macro_widget import MacroTab
from qtvcp.widgets.versa_probe import VersaProbe
from qtvcp.widgets.entry_widget import SoftInputWidget
from qtvcp.widgets.virtualkeyboard import VirtualKeyboard
from qtvcp.widgets.calculator import Calculator
from qtvcp.widgets.machine_log import MachineLog
from qtvcp.lib.notify import Notify
from qtvcp.widgets.dialogMixin import GeometryMixin
from qtvcp.core import Status, Action, Info, Tool
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
# INFO holds INI file details
# TOOL gives tool file access
# NOTICE is for the desktop notify system
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
TOOL = Tool()
NOTICE = Notify()
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

    #########################################
    # GeometryMixin helper class
    #########################################
    # adds geometry class and _HalWidgetBase class into the dialogs.
    # the geometry class parses the geometry string and places
    # the dialog based on what it finds.
    # it needed to be in a separate file do to circular imports
    # found when using versaProbe

################################################################################
# Generic messagebox Dialog
################################################################################
class LcncDialog(QMessageBox, GeometryMixin):
    OK = 'OK'
    NONE = 'NONE'
    YESNO = 'YESNO'
    OKCANCEL = 'OKCANCEL'
    CLOSEPROMPT = 'CLOSEPROMPT'

    def __init__(self, parent=None):
        super(LcncDialog, self).__init__(parent)
        self.setTextFormat(Qt.RichText)
        self.setText('<b>Sample Text?</b>')
        self.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        self.setIcon(QMessageBox.Critical)
        self.setDetailedText('')
        self.mtype = 'OK'
        self._possibleTypes = ('OK','YESNO','OKCANCEL','CLOSEPROMPT','NONE')
        self._state = False
        self._color = QColor(0, 0, 0, 150)
        self._request_name = 'MESSAGE'
        self._nblock = False
        self._message = None
        self._return_callback = None
        self._pinname = None
        self._title = 'Message Dialog'
        self._forcedFlag = True
        self._use_exec = False
        self._geoName = 'LncMessage-geometry'
        self.set_default_geometry()
        self.hide()
        self.buttonClicked.connect(self.msgbtn)

    def _hal_init(self):
        self.read_preference_geometry(self._geoName)
        STATUS.connect('dialog-request', self._external_request)

    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        self._message = message
        if message.get('NAME') == self._request_name:

            # request to close dialog early
            if message.get('CLOSE'):
                self.close()
                STATUS.emit('focus-overlay-changed', False, None, None)
                return

            geo = message.get('GEONAME') or 'LncMessage-geometry'
            t = message.get('TITLE')
            if not t:
                t = 'Message Dialog'

            messtext = message.get('MESSAGE') or None
            more = message.get('MORE') or None
            details = message.get('DETAILS') or None
            mtype = message.get('TYPE')
            if mtype is None: mtype = 'OK'
            icon = message.get('ICON') or 'INFO'
            pin = message.get('PINNAME')
            ftext = message.get('FOCUSTEXT')
            fcolor = message.get('FOCUSCOLOR')
            alert = message.get('PLAYALERT')
            nblock = message.get('NONBLOCKING')
            forceOpen = message.get('NONBLOCKING')
            callback = message.get('CALLBACK') # this needs testing
            self.showdialog(messtext, more, details, mtype, 
                                    icon, pin, ftext, fcolor, alert,
                                    nblock, geoname=geo,title = t, return_callback = callback,
                                    force_open = forceOpen)

    # This actually builds and displays the dialog.
    # there are three ways to get results:
    # - through a return by status message  (return_callback = None, use_exec = False)
    # - callback return                     (return_callback = function_name)
    # - by direct return statement          (use_exec = True)
    def showdialog(self, messagetext, more_info=None, details=None, display_type='OK',
                   icon=QMessageBox.Information, pinname=None, focus_text=None,
                   focus_color=None, play_alert=None, nblock=False,
                   return_callback = None, flags = None, setflags = None,
                    title = None, use_exec = False,geoname=None,
                    force_open = None):

        self._pinname = pinname
        self._nblock = nblock
        self._return_callback = return_callback
        self._use_exec = use_exec

        # block response to main window?
        if nblock:
            self.setWindowModality(Qt.NonModal)
        else:
            self.setWindowModality(Qt.ApplicationModal)

        # set standard flags or allow external selection
        if flags is None:
            if nblock:
                self.setWindowFlags(self.windowFlags() | Qt.Tool |
                                Qt.Dialog | Qt.WindowStaysOnTopHint
                                | Qt.WindowSystemMenuHint)
                if display_type == LcncDialog.NONE:
                    self.setWindowFlags(self.windowFlags() | Qt.CustomizeWindowHint)
                    self.setWindowFlag(Qt.WindowCloseButtonHint, False)

            else:
                self.setWindowFlags(self.windowFlags() | Qt.Tool |
                                Qt.FramelessWindowHint | Qt.Dialog |
                                Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        else:
            self.setWindowFlags(self.windowFlags() | flags)

        # allow external setting of flags - probably hide close button
        if not setflags is None:
            for i in setflags:
                self.setWindowFlag(i,setflags[i])

        self.setWindowTitle(title)

        if focus_color is not None:
            color = focus_color
        else:
            color = self._color

        # convert text descriptions to actual icons
        if icon == 'QUESTION': icon = QMessageBox.Question
        elif icon == 'INFO' or isinstance(icon,str): icon = QMessageBox.Information
        elif icon == 'WARNING': icon = QMessageBox.Warning
        elif icon == 'CRITICAL': icon = QMessageBox.Critical
        self.setIcon(icon)

        self.setText('<b>%s</b>' % messagetext)

        if more_info is not None:
            self.setInformativeText(more_info)
        else:
            self.setInformativeText('')

        if details is not None:
            self.setDetailedText(details)
        else:
            self.setDetailedText('')

        # convert display type text to buttons layouts
        display_type = display_type.upper()
        if display_type not in self._possibleTypes:
            display_type = LcncDialog.OK
        if display_type == LcncDialog.OK:
            self.setStandardButtons(QMessageBox.Ok)
        elif display_type == LcncDialog.YESNO:
            self.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        elif display_type == LcncDialog.OKCANCEL:
            self.setStandardButtons(QMessageBox.Ok | QMessageBox.Cancel)
        elif display_type == LcncDialog.NONE:
            self.setStandardButtons(QMessageBox.NoButton)


        if not nblock:
            STATUS.emit('focus-overlay-changed', True, focus_text, color)
        if play_alert:
            STATUS.emit('play-sound', play_alert)
 
        if geoname is None:
            geoname = self._geoName
        self.read_preference_geometry(geoname)
        self.show()
        if not force_open is None:
            self._forcedFlag = force_open
        self.forceDetailsOpen()

        if use_exec:
            retval = self.exec_()
            STATUS.emit('focus-overlay-changed', False, None, None)
            LOG.debug('Value of pressed button: {}'.format(retval))
            return self.qualifiedReturn(retval)

    # hack to force details box to present open on first display
    def forceDetailsOpen(self):
        if self._forcedFlag: return
        try:
            # force the details box open on first time display
            for i in self.buttons():
                if self.buttonRole(i) == QMessageBox.ActionRole:
                    for j in self.children():
                        for k in j.children():
                            if isinstance( k, QTextEdit):
                                #i.hide()
                                if not k.isVisible():
                                    i.click()
        except:
            pass
        self._forcedFlag = True

    def qualifiedReturn(self, retval):
        if retval in(QMessageBox.No, QMessageBox.Cancel):
            return False
        elif retval in(QMessageBox.Ok, QMessageBox.Yes):
            return True
        else:
            return self.buttonRole(self.clickedButton())

    # move dialog when shown
    def showEvent(self, event):
        self.set_geometry()
        super(LcncDialog, self).showEvent(event)
        return
        if self._nblock:
            self.set_geometry()
        else:
            geom = self.frameGeometry()
            geom.moveCenter(QDesktopWidget().availableGeometry().center())
            self.setGeometry(geom)
        super(LcncDialog, self).showEvent(event)


    def msgbtn(self, i):
        LOG.debug('Button pressed is: {}'.format(i.text()))

        # update the dialog position
        self.record_geometry()

        if self._use_exec:
            return

        self.hide()

        btn = self.standardButton(self.clickedButton())
        result = self.qualifiedReturn(btn)
        LOG.debug('Value of {} pressed button: {}'.format(self, result))

        # these directly call a function with btn info
        if not self._return_callback is None:
            self._return_callback(self, result)
        # these return via status messages
        elif self._message is not None:
            self._message['RETURN'] = result
            STATUS.emit('general', self._message)
            STATUS.emit('focus-overlay-changed', False, None, None)
            self._message = None
        # just return result
        else:
            LOG.error('No callback or STATUS message specified for: {}'.format(self.objectName()))

    def setGeometry(self,*args):
        #print(args,len(args))
        if len(args) == 1:
            super().setGeometry(args[0])
            width = args[0].width()
        else:
            super().setGeometry(args[0],args[1],args[2],args[3])
            width = args[2]
        if isinstance(self,QMessageBox) and width < 200:
            if isinstance(self,CloseDialog):
                self.findChild(QLabel, "qt_msgbox_label").setFixedWidth(200)
            else:
                self.findChild(QLabel, "qt_msgbox_label").setFixedWidth(350)

    # **********************
    # Designer properties
    # **********************
    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'MESSAGE'

    overlay_color = pyqtProperty(QColor, getColor, setColor)
    state = pyqtProperty(bool, getState, setState, resetState)
    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)

################################################################################
# Close Dialog
################################################################################
class CloseDialog(LcncDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(CloseDialog, self).__init__(parent)
        self.shutdown = self.addButton('System\nShutdown',QMessageBox.DestructiveRole)
        self._request_name = 'CLOSEPROMPT'
        self._title = 'QtVCP'

################################################################################
# Tool Change Dialog
################################################################################
class ToolDialog(LcncDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(ToolDialog, self).__init__(parent)
        self.setText('<b>Manual Tool Change Request</b>')
        self.setInformativeText('Please Insert Tool 0')
        self.setStandardButtons(QMessageBox.Ok)
        self.setDefaultButton(QMessageBox.Ok)
        self._useDesktopNotify = False
        self._frameless = False
        self._curLine = 0
        self._actionbutton = self.addButton('Pause For Jogging', QMessageBox.ApplyRole)
        self._actionbutton.setEnabled(False)
        self._flag = True
        self._forcedFlag = False

    # We want the tool change HAL pins the same as what's used in AXIS so it is
    # easier for users to connect to.
    # So we need to trick the HAL component into doing this for these pins,
    # but not any other Qt widgets.
    # So we record the original base name of the component, make our pins, then
    # switch it back
    def _hal_init(self):
        self.set_default_geometry()
        self.read_preference_geometry('ToolChangeDialog-geometry')

        if not hal.component_exists('hal_manualtoolchange'):
            oldname = self.HAL_GCOMP_.comp.getprefix()
            self.HAL_GCOMP_.comp.setprefix('hal_manualtoolchange')
            self.hal_pin = self.HAL_GCOMP_.newpin('change', hal.HAL_BIT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(self.tool_change)
            self.tool_number = self.HAL_GCOMP_.newpin('number', hal.HAL_S32, hal.HAL_IN)
            self.changed = self.HAL_GCOMP_.newpin('changed', hal.HAL_BIT, hal.HAL_OUT)
            self.ext_ack = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + 'change_button', hal.HAL_BIT, hal.HAL_IN)
            self.ext_ack.value_changed.connect(self.external_acknowledge)
            self.HAL_GCOMP_.comp.setprefix(oldname)
        else:
            LOG.warning("""Detected hal_manualtoolchange component already loaded
   Qtvcp recommends to allow use of it's own component by not loading the original. 
   Qtvcp Integrated toolchange dialog will not show until then""")
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('toolDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.speak = self.PREFS_.getpref('toolDialog_speak', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('toolDialog_sound_type', 'READY', str, 'DIALOG_OPTIONS')
        else:
            self.play_sound = False

    # process callback from 'change' HAL pin
    def tool_change(self, change):
        if change:
            try:
                STATUS.stat.poll()
                self._curLine = STATUS.stat.motion_line
            except:
                self._curLine = 0
            # enable/disable pause at jog button
            if self._curLine > 0:
                self._actionbutton.setEnabled(True)
                jpause = True
            else:
                self._actionbutton.setEnabled(False)
                jpause = False

            MORE = 'Please Insert Tool %d' % self.tool_number.get()
            try:
                tool_table_line = TOOL.GET_TOOL_INFO(self.tool_number.get())
                comment = str(tool_table_line[TOOL.COMMENTS])
            except TypeError:
                comment = ''
            MESS = 'Manual Tool Change Request'
            DETAILS = ' Tool Info: %s'% comment

            STATUS.emit('focus-overlay-changed', True, MESS, self._color)
            if self.PREFS_:
                if self.speak:
                    STATUS.emit('play-sound', 'speak %s' % MORE)
                if self.play_sound:
                    STATUS.emit('play-sound', self.sound_type)

            # show desktop notify dialog rather then a qt dialog
            if self._useDesktopNotify:
               self.deskNotice = NOTICE.show_toolchange_notification(MESS,
                                    MORE +'\n' + comment,
                                    None, 0,
                                    self._processChange,
                                    jogpause = jpause)
            else:
                # ok show Qt dialog
                self.showdialog(MESS, MORE, DETAILS,
                                frameless = self._frameless)
        elif not change:
            self.changed.set(False)

    # process callback for 'change-button' HAL pin
    # hide the message dialog or desktop notify message
    def external_acknowledge(self, state):
        #print('external acklnowledge: {}'.format(state))
        if state:
            if self._useDesktopNotify:
                self.deskNotice.close()
            elif self.isVisible():
                self.hide()
            self._processChange(True)


    # This also is called from DesktopDialog
    def _processChange(self,answer):
        #print('proces change: {}'.format(answer))
        if answer == -1:
            self.changed.set(True)
            ACTION.ABORT()
            STATUS.emit('update-machine-log', 'tool change paused for jogging; launched run-from-line', 'TIME')
            STATUS.emit('dialog-request', {'NAME': 'RUNFROMLINE',
                        'LINE':self._curLine+2, 'NONBLOCKING':True})
        elif answer == True:
            self.changed.set(True)
        elif answer == False:
            ACTION.ABORT()
            STATUS.emit('update-machine-log', 'tool change Aborted', 'TIME')

        self.record_geometry()
        STATUS.emit('focus-overlay-changed', False, None, None)

    ###### overridden functions ################

    def showdialog(self, message, more_info=None, details=None,
                    play_alert=None,
                    frameless = True):

        self.setWindowModality(Qt.ApplicationModal)
        if frameless:
            self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.FramelessWindowHint | Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        else:
            self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint
                            | Qt.WindowSystemMenuHint)
        self.setWindowFlags(self.windowFlags() | Qt.CustomizeWindowHint)
        self.setWindowFlag(Qt.WindowCloseButtonHint, False)

        self.setWindowTitle(self._title)
        self.setIcon(QMessageBox.Critical)
        self.setText('<b>%s</b>' % message)
        self.setInformativeText(more_info)
        self.setDetailedText(details)
        self.setStandardButtons(QMessageBox.Ok)
        self.buttonClicked.connect(self.msgbtn)
        # force the details box open on first time display
        if self._flag and details != ' Tool Info: ':
            for i in self.buttons():
                if self.buttonRole(i) == QMessageBox.ActionRole:
                    if i is self._actionbutton: continue
                    i.click()
                    self._flag = False
        if play_alert:
            STATUS.emit('play-sound', play_alert)
        self.show()

    # set the geometry when dialog shown
    def showEvent(self, event):
        self.set_geometry()
        super(LcncDialog, self).showEvent(event)

    # decode button presses
    def msgbtn(self, i):
        LOG.debug('Button pressed is: {}'.format(i.text()))
        if self.clickedButton() == self._actionbutton:
            self._processChange(-1)
        elif self.standardButton(self.clickedButton()) == QMessageBox.Ok:
            self._processChange(True)
        else:
            self._processChange(False)

    ############################################

    # **********************
    # Designer properties
    # **********************
    # inherited from lcncDialog
    # plus :

    def setFrameless(self, value):
        self._frameless = value
    def getFrameless(self):
        return self._frameless
    def resetFrameless(self):
        self._frameless = False

    frameless = pyqtProperty(bool, getFrameless, setFrameless, resetFrameless)

    def setUseDesktopNotify(self, value):
        self._useDesktopNotify = value
    def getUseDesktopNotify(self):
        return self._useDesktopNotify
    def resetUseDesktopNotify(self):
        self._useDesktopNotify = False

    useDesktopNotify = pyqtProperty(bool, getUseDesktopNotify, setUseDesktopNotify, resetUseDesktopNotify)

################################################################################
# File Open Dialog
################################################################################
class FileDialog(QFileDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(FileDialog, self).__init__(parent)
        self._state = False
        self._load_request_name = 'LOAD'
        self._save_request_name = 'SAVE'
        self._color = QColor(0, 0, 0, 150)
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        self.setOptions(options)
        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)

        self.INI_exts = INFO.get_qt_filter_extensions()
        self.setNameFilter(self.INI_exts)
        self.default_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files/examples'))

        # sidebar links
        urls = []
        urls.append(QUrl.fromLocalFile(os.path.expanduser('~')))
        local = os.path.join(os.path.expanduser('~'),'linuxcnc/nc_files')
        if os.path.exists(local):
            urls.append(QUrl.fromLocalFile(local))
        self.setSidebarUrls(urls)

    def _hal_init(self):
        self.set_default_geometry()

        STATUS.connect('dialog-request', self._external_request)
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('fileDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('fileDialog_sound_type', 'READY', str, 'DIALOG_OPTIONS')
            last_path = self.PREFS_.getpref('last_loaded_directory', self.default_path, str, 'BOOK_KEEPING')
            self.setDirectory(last_path)
        else:
            self.play_sound = False

    def _external_request(self, w, message):
        name = message.get('NAME')
        if name in (self._load_request_name,self._save_request_name):
            ext = message.get('EXTENSIONS')
            pre = message.get('FILENAME')
            dir = message.get('DIRECTORY')
            if dir is None:
                dir = self.PREFS_.getpref('last_loaded_directory', self.default_path, str, 'BOOK_KEEPING')
            geo = message.get('GEONAME') or 'FileDialog-geometry'
            self.read_preference_geometry(geo)
            if name == self._load_request_name:
                # if there is an ID then a file name response is expected
                if message.get('ID'):
                    message['RETURN'] = self.load_dialog(ext, pre, dir, True)
                    STATUS.emit('general', message)
                else:
                    self.load_dialog(extensions = ext, preselect = pre, directory = dir,)
            else:
                if message.get('ID'):
                    message['RETURN'] = self.save_dialog(ext, pre, dir)
                    STATUS.emit('general', message)

    def showdialog(self):
        self.load_dialog()

    def load_dialog(self, extensions = None, preselect = None, directory = None, return_path=False):
        self.setFileMode(QFileDialog.ExistingFile)
        self.setAcceptMode(QFileDialog.AcceptOpen)

        if extensions:
            self.setNameFilter(extensions)
        else:
            self.setNameFilter(self.INI_exts)
        if preselect:
            self.selectFile(preselect)
        else:
            self.selectFile('')
        if directory:
            self.setDirectory(directory)
        else:
            self.setDirectory(self.default_path)
        self.setWindowTitle('Open')
        STATUS.emit('focus-overlay-changed', True, 'Open Gcode', self._color)
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        fname = None
        if (self.exec_()):
            fname = self.selectedFiles()[0]
            path = self.directory().absolutePath()
            self.setDirectory(path)

        self.record_geometry()
        if fname and not return_path:
            if self.PREFS_:
                self.PREFS_.putpref('last_loaded_directory', path, str, 'BOOK_KEEPING')
                self.PREFS_.putpref('RecentPath_0', fname, str, 'BOOK_KEEPING')
            ACTION.OPEN_PROGRAM(fname)
            STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME')
            # overlay hides it's self after loading
        STATUS.emit('focus-overlay-changed', False, None, None)
        return fname

    def save_dialog(self, extensions = None, preselect = None, directory = None):
        self.setFileMode(QFileDialog.AnyFile)
        self.setAcceptMode(QFileDialog.AcceptSave)
        self.setDefaultSuffix('ngc')
        if extensions:
            self.setNameFilter(extensions)
        else:
            self.setNameFilter(self.INI_exts)
        if preselect:
            self.selectFile(preselect)
        else:
            self.selectFile(' ')
        if directory:
            self.setDirectory(directory)
        self.setWindowTitle('Save')
        STATUS.emit('focus-overlay-changed', True, 'Save Gcode', self._color)
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        fname = None
        if (self.exec_()):
            fname = self.selectedFiles()[0]
            path = self.directory().absolutePath()
            self.setDirectory(path)
        else:
            fname = None
        STATUS.emit('focus-overlay-changed', False, None, None)
        self.record_geometry()
        if fname: 
            if self.PREFS_:
                self.PREFS_.putpref('last_saved_directory', path, str, 'BOOK_KEEPING')
                self.PREFS_.putpref('RecentSavedPath_0', fname, str, 'BOOK_KEEPING')
        return fname

    #**********************
    # Designer properties
    #**********************

    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

    def getLoadIdName(self):
        return self._load_request_name
    def setLoadIdName(self, name):
        self._load_request_name = name
    def resetLoadIdName(self):
        self._load_request_name = 'LOAD'

    def getSaveIdName(self):
        return self._save_request_name
    def setSaveIdName(self, name):
        self._save_request_name = name
    def resetSaveIdName(self):
        self._save_request_name = 'SAVE'

    launch_load_id = pyqtProperty(str, getLoadIdName, setLoadIdName, resetLoadIdName)
    launch_save_id = pyqtProperty(str, getSaveIdName, setSaveIdName, resetSaveIdName)

################################################################################
# origin Offset Dialog
################################################################################
class OriginOffsetDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(OriginOffsetDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self._state = False
        self._request_name = 'ORIGINOFFSET'

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        self.setMinimumSize(200, 200)
        buttonBox = QDialogButtonBox()
        buttonBox.setEnabled(False)
        STATUS.connect('not-all-homed', lambda w, axis: buttonBox.setEnabled(False))
        STATUS.connect('all-homed', lambda w: buttonBox.setEnabled(True))
        STATUS.connect('state-estop', lambda w: buttonBox.setEnabled(False))
        STATUS.connect('state-estop-reset', lambda w: buttonBox.setEnabled(STATUS.machine_is_on()
                                                    and STATUS.is_all_homed()))
        STATUS.connect('interp-idle', lambda w: buttonBox.setEnabled(STATUS.machine_is_on()
                                                    and STATUS.is_all_homed()))
        STATUS.connect('interp-run', lambda w: buttonBox.setEnabled(False))
        for i in('X', 'Y', 'Z'):
            b = 'button_%s' % i
            self[b] = QPushButton('Zero %s' % i)
            self[b].clicked.connect(self.zeroPress('%s' % i))
            buttonBox.addButton(self[b], 3)

        v = QVBoxLayout()
        h = QHBoxLayout()
        self._o = OFFVIEW_WIDGET()
        self._o.setObjectName('__dialogOffsetViewWidget')
        self.setLayout(v)
        v.addWidget(self._o)
        b = QPushButton('OK')
        b.clicked.connect(lambda: self.close())
        h.addWidget(b)
        h.addWidget(buttonBox)
        v.addLayout(h)
        self.setModal(True)

    def _hal_init(self):
        self._o.hal_init()
        self.set_default_geometry()
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
            geo = message.get('GEONAME') or 'OriginOffsetDialog-geometry'
            self.read_preference_geometry(geo)
            self.load_dialog()

    # This weird code is just so we can get the axis
    # letter
    # using clicked.connect() apparently can't easily
    # add user data
    def zeroPress(self, data):
        def calluser():
            self.zeroAxis(data)
        return calluser

    def zeroAxis(self, index):
        ACTION.SET_AXIS_ORIGIN(index, 0)

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Set Origin Offsets', self._color)
        self.set_geometry()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        self.record_geometry()

    def _hal_cleanup(self):
        self._o._hal_cleanup()

    # usual boiler code
    # (used so we can use code such as self[SomeDataName]
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

    # **********************
    # Designer properties
    # **********************

    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'ORIGINOFFSET'

    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)


################################################################################
# Tool Offset Dialog
################################################################################
class ToolOffsetDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(ToolOffsetDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self._state = False
        self._request_name = 'TOOLOFFSET'

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        self.setMinimumSize(200, 200)

        self._o = TOOLVIEW_WIDGET()
        self._o.setObjectName('__dialogToolViewWidget')
        self._o._hal_init()

        buttonBox = QDialogButtonBox()
        buttonBox.setEnabled(False)
        STATUS.connect('not-all-homed', lambda w, axis: buttonBox.setEnabled(False))
        STATUS.connect('all-homed', lambda w: buttonBox.setEnabled(True))
        STATUS.connect('state-estop', lambda w: buttonBox.setEnabled(False))
        STATUS.connect('state-estop-reset', lambda w: buttonBox.setEnabled(STATUS.machine_is_on()
                                                    and STATUS.is_all_homed()))
        STATUS.connect('interp-idle', lambda w: buttonBox.setEnabled(STATUS.machine_is_on()
                                                    and STATUS.is_all_homed()))
        STATUS.connect('interp-run', lambda w: buttonBox.setEnabled(False))
        self.addtool = QPushButton('Add Tool')
        self.addtool.clicked.connect(lambda: self.addTool())
        buttonBox.addButton(self.addtool, 3)
        self.deletetool = QPushButton('Delete Tool')
        self.deletetool.clicked.connect(lambda: self.deleteTool())
        buttonBox.addButton(self.deletetool, 3)
        #for i in('X', 'Y', 'Z'):
        #    b = 'button_%s' % i
        #    self[b] = QPushButton('Zero %s' % i)
        #    self[b].clicked.connect(self.zeroPress('%s' % i))
        #    buttonBox.addButton(self[b], 3)

        v = QVBoxLayout()
        h = QHBoxLayout()
        self.setLayout(v)
        v.addWidget(self._o)
        b = QPushButton('OK')
        b.clicked.connect(lambda: self.close())
        h.addWidget(b)
        h.addWidget(buttonBox)
        v.addLayout(h)
        self.setModal(True)

    def _hal_init(self):
        self.set_default_geometry()
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
            geo = message.get('GEONAME') or 'ToolOffsetDialog-geometry'
            self.read_preference_geometry(geo)
            self.load_dialog()

    def addTool(self):
        self._o.add_tool()

    def deleteTool(self):
        self._o.delete_tools()

    # This weird code is just so we can get the axis
    # letter
    # using clicked.connect() apparently can't easily
    # add user data
    def zeroPress(self, data):
        def calluser():
            self.zeroAxis(data)
        return calluser

    def zeroAxis(self, index):
        ACTION.SET_AXIS_ORIGIN(index, 0)

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Set Tool Offsets', self._color)
        self.set_geometry()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        self.record_geometry()

    # usual boiler code
    # (used so we can use code such as self[SomeDataName]
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

    # **********************
    # Designer properties
    # **********************

    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'TOOLOFFSET'

    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)


################################################################################
# CamView Dialog
################################################################################
class CamViewDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(CamViewDialog, self).__init__(parent)
        from qtvcp.widgets.camview_widget import CamView
        self._color = QColor(0, 0, 0, 150)
        self._state = False
        self._request_name = 'CAMVIEW'

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        self.setMinimumSize(200, 200)
        h = QHBoxLayout()
        h.addStretch(1)
        self.b = QPushButton('Close')
        self.b.clicked.connect(lambda: self.close())
        h.addWidget(self.b)
        l = QVBoxLayout()
        self.camV = CamView()
        self.setLayout(l)
        l.addWidget(self.camV)
        l.addLayout(h)

    def _hal_init(self):
        self.camV.hal_init(HAL_NAME='')
        self.set_default_geometry()
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
            geo = message.get('GEONAME') or 'CamViewOffsetDialog-geometry'
            self.read_preference_geometry(geo)
            nblock = message.get('NONBLOCKING')
            if nblock:
                self.setWindowModality(Qt.NonModal)
                self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint
                            | Qt.WindowSystemMenuHint)
                self.load_dialog_nonblocking()
            else:
                self.setWindowModality(Qt.ApplicationModal)
                self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.FramelessWindowHint | Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
                self.load_dialog()

    def close(self):
        self.record_geometry()
        super(CamViewDialog, self).close()

    def load_dialog_nonblocking(self):
        self.set_geometry()
        self.show()

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Cam View Dialog', self._color)
        self.set_geometry()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)

    # **********************
    # Designer properties
    # **********************

    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'CAMVIEW'

    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)


################################################################################
# MacroTab Dialog
################################################################################
class MacroTabDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(MacroTabDialog, self).__init__(parent)
        self.setWindowTitle('Qtvcp Macro Menu')
        self._color = QColor(0, 0, 0, 150)
        self._state = False
        self._request_name = 'MACROTAB'
        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        self.setMinimumSize(00, 200)
        self.resize(600, 400)
        # patch class to call our button methods rather then the
        # original methods (Gotta do before instantiation)
        MacroTab.closeChecked = self._close
        MacroTab.runChecked = self._run
        MacroTab.setTitle = self._setTitle
        # ok now instantiate patched class
        self.tab = MacroTab()
        self.tab.setObjectName('macroTabInternal_')
        l = QVBoxLayout()
        self.setLayout(l)
        l.addWidget(self.tab)
        #we need the close button
        self.tab.closeButton.setVisible(True)

    def _hal_init(self):
        self.set_default_geometry()
        # gotta call this since we instantiated this out of qtvcp's knowledge
        self.tab._hal_init()

        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
            geo = message.get('GEONAME') or 'MacroTabDialog-geometry'
            self.read_preference_geometry(geo)
            self.load_dialog()

    # This method is called instead of MacroTab's closeChecked method
    # we do this so we can use it's buttons to hide our dialog
    # rather then close the MacroTab widget
    def _close(self):
        self.close()

    # This method is called instead of MacroTab's runChecked() method
    # we do this so we can use it's buttons to hide our dialog
    # rather then close the MacroTab widget
    def _run(self):
        self.tab.runMacro()
        self.close()

    def _setTitle(self, string):
        self.setWindowTitle(string)

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Lathe Macro Dialog', self._color)
        self.tab.stack.setCurrentIndex(0)
        self.set_geometry()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        self.record_geometry()

    # **********************
    # Designer properties
    # **********************

    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'MACROTAB'

    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

################################################################################
# Versaprobe Dialog
################################################################################
class VersaProbeDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(VersaProbeDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self._state = False
        self._request_name  = 'VERSAPROBE'
        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        self.setMinimumSize(200, 200)
        buttonBox = QDialogButtonBox(QDialogButtonBox.Ok)
        b = buttonBox.button(QDialogButtonBox.Ok)
        b.clicked.connect(lambda: self.close())
        l = QVBoxLayout()
        self._o = VersaProbe()
        self.setLayout(l)
        l.addWidget(self._o)
        l.addWidget(buttonBox)

    def _hal_init(self):
        self._o.hal_init()
        self.set_default_geometry()
        STATUS.connect('dialog-request', self._external_request)

    def _hal_cleanup(self):
        self._o._hal_cleanup()

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
            geo = message.get('GEONAME') or 'VersaProbeDialog-geometry'
            self.read_preference_geometry(geo)
            self.load_dialog()

    def showdialog(self):
        self.load_dialog()

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'VersaProbe Dialog', self._color)
        self.set_geometry()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        self.record_geometry()

    # **********************
    # Designer properties
    # **********************

    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'VERSAPROBE'

    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

############################################
# Entry Dialog
############################################
class EntryDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(EntryDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self.play_sound = False
        self._request_name = 'ENTRY'
        self._title = 'Numerical Entry'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)
        self._softKey = SoftInputWidget(self, 'numeric')

        self.Num = QLineEdit()
        self.Num.installEventFilter(self)
        self.Num.keyboard_type = 'numeric'
        # actiate touch input
        self.Num.keyboard_enable = True

        self.Num.returnPressed.connect(lambda: self.accept())


        gl = QVBoxLayout()
        gl.addWidget(self.Num)

        self.bBox = QDialogButtonBox()
        self.bBox.addButton('Apply', QDialogButtonBox.AcceptRole)
        self.bBox.addButton('Cancel', QDialogButtonBox.RejectRole)
        self.bBox.rejected.connect(self.reject)
        self.bBox.accepted.connect(self.accept)

        gl.addWidget(self.bBox)
        self.setLayout(gl)

    def _hal_init(self):
        self.set_default_geometry()
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('EntryDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('EntryDialog_sound_type', 'READY', str, 'DIALOG_OPTIONS')
        else:
            self.play_sound = False
        STATUS.connect('dialog-request', self._external_request)

    def eventFilter(self, widget, event):
        if self.Num.focusWidget() == widget and event.type() == QEvent.MouseButtonPress:
            if self.Num.keyboard_enable:
                self._softKey.show_input_panel(widget)
        return False

    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        if message.get('NAME') == self._request_name:
            geo = message.get('GEONAME') or 'EntryDialog-geometry'
            self.read_preference_geometry(geo)
            t = message.get('TITLE')
            if t:
                self._title = t
            else:
                self._title = 'Numerical Entry'
            overlay = message.get('OVERLAY')
            if overlay is None :
                overlay = True
            preload = message.get('PRELOAD')
            num = self.showdialog(preload=preload, overlay=overlay)
            message['RETURN'] = num
            STATUS.emit('general', message)

    def showdialog(self, preload=None,overlay=True):
        conversion = {'x':0, 'y':1, "z":2, 'a':3, "b":4, "c":5, 'u':6, 'v':7, 'w':8}
        if overlay:
            STATUS.emit('focus-overlay-changed', True, '', self._color)
        self.setWindowTitle(self._title);
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        if preload is not None:
            self.Num.setText(str(preload))
        flag = False
        while flag == False:
            self.Num.setFocus()
            retval = self.exec_()
            if retval:
                try:
                    answer = float(self.Num.text())
                    flag = True
                except Exception as e:
                    try:
                        p,relp,dtg = STATUS.get_position()
                        otext = text = self.Num.text().lower()
                        for let in INFO.AVAILABLE_AXES:
                            let = let.lower()
                            if let in text:
                                Pos = relp[conversion[let]]
                                text = text.replace('%s'%let,'%s'%Pos)
                        process = eval(text)
                        answer = float(process)
                        STATUS.emit('update-machine-log', 'Convert Entry from: {} to {}'.format(otext,text), 'TIME')
                        flag = True
                    except Exception as e:
                        self.setWindowTitle('%s'%e)
            else:
                flag = True
                answer = None
        if overlay:
            STATUS.emit('focus-overlay-changed', False, None, None)
        self.record_geometry()
        LOG.debug('Value of pressed button: {}'.format(retval))
        if answer is None:
            return None
        return answer

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'ENTRY'

    def set_soft_keyboard(self, data):
        self.Num.keyboard_enable = data
    def get_soft_keyboard(self):
        return self.Num.keyboard_enable
    def reset_soft_keyboard(self):
        self.Num.keyboard_enable = True

    # designer will show these properties in this order:
    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    overlay_color = pyqtProperty(QColor, getColor, setColor)
    soft_keyboard_option = pyqtProperty(bool, get_soft_keyboard, set_soft_keyboard, reset_soft_keyboard)

############################################
# Keyboard Dialog
############################################
class KeyboardDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(KeyboardDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self.play_sound = False
        self._request_name = 'KEYBOARD'
        self._title = 'Keyboard Entry'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)
        self.keybrd = VirtualKeyboard()
        self.edit = QLineEdit()

        gl = QVBoxLayout()
        gl.addWidget(self.edit)
        gl.addWidget(self.keybrd)

        self.bBox = QDialogButtonBox()
        self.bBox.addButton('Apply', QDialogButtonBox.AcceptRole)
        self.bBox.addButton('Cancel', QDialogButtonBox.RejectRole)
        self.bBox.rejected.connect(self.reject)
        self.bBox.accepted.connect(self.accept)

        gl.addWidget(self.bBox)
        self.setLayout(gl)

    def _hal_init(self):
        self.set_default_geometry()
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('KeyboardDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('KeyboardDialog_sound_type', 'READY', str, 'DIALOG_OPTIONS')
        else:
            self.play_sound = False
        STATUS.connect('dialog-request', self._external_request)

    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        if message.get('NAME') == self._request_name:
            geo = message.get('GEONAME') or 'KeyboardDialog-geometry'
            self.read_preference_geometry(geo)
            t = message.get('TITLE')
            if t:
                self._title = t
            else:
                self._title = 'Keyboard Entry'
            preload = message.get('PRELOAD')
            overlay = message.get('OVERLAY')
            if overlay is None :
                overlay = True
            text = self.showdialog(preload)
            message['RETURN'] = text
            STATUS.emit('general', message)

    def showdialog(self, preload=None, overlay=True):
        if overlay:
            STATUS.emit('focus-overlay-changed', True, '', self._color)
        self.setWindowTitle(self._title);
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        self.edit.setFocus()
        if preload is not None:
            self.edit.setFocus()
            self.edit.setText(str(preload))
            self.edit.deselect()
        retval = self.exec_()
        answer = self.edit.text()
        if retval:   
            STATUS.emit('update-machine-log', 'keyboard Entry {}'.format(answer), 'TIME')
        else:
            answer = None

        if overlay:
            STATUS.emit('focus-overlay-changed', False, None, None)
        self.record_geometry()
        LOG.debug('Value of pressed button: {}'.format(retval))
        if answer is None:
            return None
        return answer

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'ENTRY'

    def set_soft_keyboard(self, data):
        self.Num.keyboard_enable = data
    def get_soft_keyboard(self):
        return self.Num.keyboard_enable
    def reset_soft_keyboard(self):
        self.Num.keyboard_enable = True

    # designer will show these properties in this order:
    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    overlay_color = pyqtProperty(QColor, getColor, setColor)
    soft_keyboard_option = pyqtProperty(bool, get_soft_keyboard, set_soft_keyboard, reset_soft_keyboard)


############################################
# Calculator Dialog
############################################
class CalculatorDialog(Calculator, GeometryMixin):
    def __init__(self, parent=None):
        super(CalculatorDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self.play_sound = False
        self._request_name = 'CALCULATOR'
        self._title = 'Calculator Entry'
        self._nblock = False
        self._message = None
        self._overlay = None
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)

    def _hal_init(self):
        self.set_default_geometry()
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('CalculatorDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('CalculatorDialog_sound_type', 'READY', str, 'DIALOG_OPTIONS')
        else:
            self.play_sound = False
        STATUS.connect('dialog-request', self._external_request)

    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        self._message = message
        if message.get('NAME') == self._request_name:
            geo = message.get('GEONAME') or 'CalculatorDialog-geometry'
            self.read_preference_geometry(geo)
            t = message.get('TITLE')
            if t:
                self._title = t
            else:
                self._title = 'Calculator Entry'
            preload = message.get('PRELOAD')
            axis = message.get('AXIS')

            next = message.get('NEXT', False)
            cycle = message.get('WIDGETCYCLE', False)

            if axis in ('X','Y','Z','A','B','C','U','V','W'):
                self.axisTriggered(axis)
            self._nblock = message.get('NONBLOCKING')
            overlay = message.get('OVERLAY')
            if overlay is None:
                overlay = True
            self._overlay = overlay
            if next:
                self.updatedialog(preload=preload)
                message['NEXT'] = False
            else:
                num = self.showdialog(preload=preload, overlay=overlay, cycle=cycle)

    def updatedialog(self, preload=None):
        self.setWindowTitle(self._title);
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        if self._overlay:
            STATUS.emit('focus-overlay-changed', True, '', self._color)
        if preload is not None:
            self.display.setText(str(preload))

    def showdialog(self, preload=None, overlay=True, cycle=False):
        self.setWindowTitle(self._title);
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        if preload is not None:
            self.display.setText(str(preload))

        # show/hide the widget cycling buttons
        self.applyNextButton.setVisible(cycle)
        self.nextButton.setVisible(cycle)
        self.backButton.setVisible(cycle)

        if self._nblock:
            self.show()
        else:
            if overlay:
                STATUS.emit('focus-overlay-changed', True, '', self._color)
            retval = self.exec_()
            if overlay:
                STATUS.emit('focus-overlay-changed', False, None, None)

    def accept(self):
        self.record_geometry()
        super(CalculatorDialog, self).accept()
        try:
            num =  float(self.display.text())
            LOG.debug('Displayed value when accepted: {}'.format(num))
            if self._message is not None:
                self._message['RETURN'] = num
                self._message['NEXT'] = False
                STATUS.emit('general', self._message)
                self._message = None
        except Exception as e:
                print(e)

    def reject(self):
        self.record_geometry()
        super(CalculatorDialog, self).reject()
        self._message['RETURN'] = None
        self._message['NEXT'] = False
        STATUS.emit('general', self._message)
        self._message = None

    # used for cycling between different widgets.
    # the actual cycling is done in the calling code
    def backAction(self):
        try:
            if self._message is not None:
                self._message['RETURN'] = None
                self._message['NEXT'] = False
                self._message['BACK'] = True
                STATUS.emit('general', self._message)
        except Exception as e:
                print(e)

    # used for cycling between different widgets.
    # the actual cycling is done in the calling code
    def nextAction(self):
        try:
            if self._message is not None:
                self._message['RETURN'] = None
                self._message['NEXT'] = True
                self._message['BACK'] = False
                STATUS.emit('general', self._message)
        except Exception as e:
                print(e)

    # used to apply and then cycle to the next widget.
    # the actual cycling is done in the calling code
    def applyAction(self):
        try:
            num =  float(self.display.text())
            if self._message is not None:
                self._message['RETURN'] = num
                self._message['NEXT'] = True
                self._message['BACK'] = False
                STATUS.emit('general', self._message)
        except Exception as e:
                print(e)

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'CALCULATOR'

    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

############################################
# machine Log Dialog
############################################
class MachineLogDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(MachineLogDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self.play_sound = False
        self._request_name = 'MACHINELOG'
        self._title = 'Machine Log'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)


    def _hal_init(self):
        self.buildWidget()
        self.set_default_geometry()
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('MachineLogDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('MachineLogDialog_sound_type', 'READY', str, 'DIALOG_OPTIONS')
        else:
            self.play_sound = False
        STATUS.connect('dialog-request', self._external_request)

    def buildWidget(self):
        # add a vertical layout to dialog
        l = QVBoxLayout()
        self.setLayout(l)

        # build tab widget
        tabs = QTabWidget()
        # build tabs
        tab_mlog = MachineLog()
        tab_mlog._hal_init()
        tab_ilog = MachineLog()
        tab_ilog.set_integrator_log(True)
        tab_ilog._hal_init()
        # add tabs to tab widget
        tabs.addTab(tab_mlog,'Machine Log')
        tabs.addTab(tab_ilog,'Integrator Log')
        # add tab to layout
        l.addWidget(tabs)

        # build dialog buttons
        self.bBox = QDialogButtonBox()
        self.bBox.addButton('Ok', QDialogButtonBox.AcceptRole)
        self.bBox.accepted.connect(self.accept)
        # add buttons to layout
        l.addWidget(self.bBox)


    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        if message.get('NAME') == self._request_name:
            geo = message.get('GEONAME') or 'MachineLogDialog-geometry'
            self.read_preference_geometry(geo)
            t = message.get('TITLE')
            if t:
                self._title = t
            else:
                self._title = 'Machine Log'
            nonblock = message.get('NONBLOCKING')
            num = self.showdialog(nonblock)
            message['RETURN'] = num
            STATUS.emit('general', message)

    def showdialog(self, nonblock=None):
        if nonblock is not None:
            STATUS.emit('focus-overlay-changed', True, 'Machine Log', self._color)
        self.setWindowTitle(self._title);
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        if nonblock is not None:
            self.exec_()
            STATUS.emit('focus-overlay-changed', False, None, None)
            self.record_geometry()
            return False
        else:
            self.show()

    def accept(self):
        self.record_geometry()
        super(MachineLogDialog, self).accept()

    def reject(self):
        self.record_geometry()
        super(MachineLogDialog, self).reject()

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'MACHINELOG'

    # designer will show these properties in this order:
    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

############################################
# Run from line prestart Dialog
############################################

class RunFromLineDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(RunFromLineDialog, self).__init__(parent)
        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'runFromLine_dialog.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)
        self.start_line = None
        self._color = QColor(0, 0, 0, 150)
        self.play_sound = False
        self._request_name = 'RUNFROMLINE'
        self._title = 'Run from line preset Dialog'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)
        self.buttonBox.clicked.connect(self.Clicked)

    def _hal_init(self):
        self.set_default_geometry()
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('RunFromLineDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('RunFromLineDialog_sound_type', 'READY', str, 'DIALOG_OPTIONS')
        else:
            self.play_sound = False
        STATUS.connect('dialog-request', self._external_request)

        def homed_on_test():
            return (STATUS.machine_is_on() 
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(homed_on_test()))

    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        if message.get('NAME') == self._request_name:
            geo = message.get('GEONAME') or 'RunFromLineDialog-geometry'
            self.read_preference_geometry(geo)
            l = message.get('LINE')
            t = message.get('TITLE')
            if t:
                self._title = t
            else:
                self._title = 'Run From Line: {}'.format(l)
            nblock = message.get('NONBLOCKING')
            mess = message.get('MESSAGE')
            num = self.showdialog(line = l, message=mess, nonblock = nblock)
            message['RETURN'] = num
            STATUS.emit('general', message)

    def showdialog(self, line = 1, message=None, nonblock=None):
        self.start_line = int(line)
        if message is not None:
            self.label_line.setText(message)
        if not nonblock:
            STATUS.emit('focus-overlay-changed', True, 'Machine Log', self._color)
        self.setWindowTitle(self._title);
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        if not nonblock:
            self.exec_()
            STATUS.emit('focus-overlay-changed', False, None, None)
            self.record_geometry()
            return False
        else:
            self.show()

    # accept button applies presets and if line number given starts linuxcnc
    def accept(self):
        self.preset()
        if self.start_line:
            ACTION.RUN(self.start_line)
        super(RunFromLineDialog, self).accept()

    #apply button only applies presets
    def Clicked(self, button):
        if self.buttonBox.buttonRole(button) == QDialogButtonBox.ApplyRole:
            self.preset()

    # preset spindle before running
    def preset(self):
        if self.radioButton_cw.isChecked():
            direction = 'M3'
        else:
            direction = 'M4'
        speed  = self.spinBox_rpm.value()
        ACTION.CALL_MDI_WAIT('s{} {}'.format(speed,direction), mode_return=True)

################################################################################
# About Dialog
################################################################################
class AboutDialog(QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(AboutDialog, self).__init__(parent)
        self._geometry_string = 'half'
        self._color = QColor(0, 0, 0, 150)
        self._request_name = 'ABOUT'
        self._title = 'QtVCP About'
        self.play_sound = False
        self.text  = QTextEdit('This is an ABOUT dialog')
        self.text.setReadOnly(True)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)

    def _hal_init(self):
        self.buildWidget()
        self.set_default_geometry()
        self.read_preference_geometry('AboutDialog-geometry')
        STATUS.connect('dialog-request', self._external_request)

    def buildWidget(self):
        # add a vertical layout to dialog
        l = QVBoxLayout()
        self.setLayout(l)
        l.addWidget(self.text)
        # build dialog buttons
        self.bBox = QDialogButtonBox()
        self.bBox.addButton('Ok', QDialogButtonBox.AcceptRole)
        self.bBox.accepted.connect(self.accept)
        # add buttons to layout
        l.addWidget(self.bBox)

    def setText(self, txt):
        self.text.setText(txt)
        self.adjustSize()

    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        if message.get('NAME') == self._request_name:
            geo = message.get('GEONAME') or 'AboutDialog-geometry'
            self.read_preference_geometry(geo)
            t = message.get('TITLE')
            if t:
                self._title = t
            else:
                self._title = 'About'
            nonblock = message.get('NONBLOCKING')
            num = self.showdialog(nonblock)
            message['RETURN'] = num
            STATUS.emit('general', message)

    def showdialog(self, nonblock=None):
        if nonblock is not None:
            STATUS.emit('focus-overlay-changed', True, 'Machine Log', self._color)
        self.setWindowTitle(self._title);
        if self.play_sound:
            STATUS.emit('play-sound', self.sound_type)
        self.set_geometry()
        if nonblock is not None:
            self.exec_()
            STATUS.emit('focus-overlay-changed', False, None, None)
            self.record_geometry()
            return False
        else:
            self.show()

    def accept(self):
        self.record_geometry()
        super(AboutDialog, self).accept()

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    def getIdName(self):
        return self._request_name
    def setIdName(self, name):
        self._request_name = name
    def resetIdName(self):
        self._request_name = 'ABOUT'

    # designer will show these properties in this order:
    launch_id = pyqtProperty(str, getIdName, setIdName, resetIdName)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

################################
# for testing without editor:
################################
def main():
    import sys
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    widget = AboutDialog()
    widget.setText('</b>This is new text<\b>')
   # widget = KeyboardDialog()
    #widget = CalculatorDialog()
    #widget = RunFromLineDialog()
    #widget = MachineLogDialog()
    #widget = EntryDialog()
    #widget = CamViewDialog()
    #widget = VersaProbeDialog()
    #widget = MacroTabDialog()
    #widget = CamViewDialog()
    #widget = ToolOffsetDialog()
    #widget = OriginOffsetDialog()
    #widget = FileDialog()
    #widget = ToolDialog()

    widget.HAL_NAME_ = 'test'
    widget.PREFS_ = None
    widget._hal_init()
    t = widget.showdialog(1)
    print (t)
    sys.exit()
if __name__ == '__main__':
    main()
