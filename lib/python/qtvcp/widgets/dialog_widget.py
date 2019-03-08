#!/usr/bin/env python
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

from PyQt5.QtWidgets import (QMessageBox, QFileDialog, QDesktopWidget,
        QDialog, QDialogButtonBox, QVBoxLayout, QPushButton, QHBoxLayout,
        QHBoxLayout, QLineEdit, QPushButton, QDialogButtonBox)
from PyQt5.QtGui import QColor
from PyQt5.QtCore import Qt, pyqtSlot, pyqtProperty

from qtvcp.widgets.widget_baseclass import _HalWidgetBase, hal
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFVIEW_WIDGET
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOLVIEW_WIDGET
from qtvcp.widgets.camview_widget import CamView
from qtvcp.widgets.macro_widget import MacroTab
from qtvcp.widgets.versa_probe import VersaProbe
from qtvcp.widgets.entry_widget import TouchInputWidget
from qtvcp.widgets.calculator import Calculator
from qtvcp.lib.notify import Notify
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
NOTICE = Notify()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class LcncDialog(QMessageBox, _HalWidgetBase):
    def __init__(self, parent=None):
        super(LcncDialog, self).__init__(parent)
        self.setTextFormat(Qt.RichText)
        self.setText('<b>Sample Text?</b>')
        self.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        self.setIcon(QMessageBox.Critical)
        self.setDetailedText('')
        self.OK_TYPE = 1
        self.YN_TYPE = 0
        self._state = False
        self._color = QColor(0, 0, 0, 150)
        self.focus_text = ''
        self._request_name = 'MESSAGE'
        self.hide()

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('LcncDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        self.topParent = self.QTVCP_INSTANCE_
        STATUS.connect('dialog-request', self._external_request)

    # this processes STATUS called dialog requests
    # We check the cmd to see if it was for us
    # then we check for a id string
    # if all good show the dialog
    # and then send back the dialog response via a general message
    def _external_request(self, w, message):
        if message.get('NAME') == self._request_name:
            t = message.get('TITLE')
            if t:
                self.title = t
            else:
                self.title = 'Entry'
            mess = message.get('MESSAGE') or None
            more = message.get('MORE') or None
            details = message.get('DETAILS') or None
            ok_type = message.get('TYPE')
            if ok_type == None: ok_type = True
            icon = message.get('ICON') or 'INFO'
            pin = message.get('PINNAME') or None
            ftext = message.get('FOCUS_TEXT') or None
            fcolor = message.get('FOCUS_COLOR') or None
            alert = message.get('PLAY_ALERT') or None
            rtrn = self.showdialog(mess, more, details, ok_type, 
                                    icon, pin, ftext, fcolor, alert)
            message['RETURN'] = rtrn
            STATUS.emit('general', message)

    def showdialog(self, message, more_info=None, details=None, display_type=1,
                   icon=QMessageBox.Information, pinname=None, focus_text=None,
                   focus_color=None, play_alert=None):

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.FramelessWindowHint | Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
                            #Qt.X11BypassWindowManagerHint

        if focus_text is not None:
            self.focus_text = focus_text
        if focus_color is not None:
            color = focus_color
        else:
            color = self._color

        if icon == 'QUESTION': icon = QMessageBox.Question
        elif icon == 'INFO' or isinstance(icon,str): icon = QMessageBox.Information
        elif icon == 'WARNING': icon = QMessageBox.Warning
        elif icon == 'CRITICAL': icon = QMessageBox.Critical
        self.setIcon(icon)

        self.setText('<b>%s</b>' % message)
        if more_info is not None:
            self.setInformativeText(more_info)
        else:
            self.setInformativeText('')
        if details is not None:
            self.setDetailedText(details)
        else:
            self.setDetailedText('')
        if display_type == self.OK_TYPE:
            self.setStandardButtons(QMessageBox.Ok)
        else:
            self.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        self.buttonClicked.connect(self.msgbtn)
        STATUS.emit('focus-overlay-changed', True, self.focus_text, color)
        if play_alert:
            STATUS.emit('play-alert', play_alert)
        retval = self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        LOG.debug("Value of pressed button: {}".format(retval))
        if retval == QMessageBox.No:
            return False
        else:
            return True

    def showEvent(self, event):
        geom = self.frameGeometry()
        geom.moveCenter(QDesktopWidget().availableGeometry().center())
        self.setGeometry(geom)
        super(LcncDialog, self).showEvent(event)

    def msgbtn(self, i):
        LOG.debug("Button pressed is: {}".format(i.text()))
        return

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

    overlay_color = pyqtProperty(QColor, getColor, setColor)
    state = pyqtProperty(bool, getState, setState, resetState)


################################################################################
# Tool Change Dialog
################################################################################
class ToolDialog(LcncDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(ToolDialog, self).__init__(parent)
        self.setText('<b>Manual Tool Change Request</b>')
        self.setInformativeText('Please Insert Tool 0')
        self.setStandardButtons(QMessageBox.Ok)
        self.useDesktopDialog = False

    # We want the tool change HAL pins the same as whats used in AXIS so it is
    # easier for users to connect to.
    # So we need to trick the HAL component into doing this for these pins,
    # but not anyother Qt widgets.
    # So we record the original base name of the component, make our pins, then
    # switch it back
    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('ToolChangeDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'

        self.topParent = self.QTVCP_INSTANCE_
        #_HalWidgetBase._hal_init(self)
        oldname = self.HAL_GCOMP_.comp.getprefix()
        self.HAL_GCOMP_.comp.setprefix('hal_manualtoolchange')
        self.hal_pin = self.HAL_GCOMP_.newpin('change', hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(self.tool_change)
        self.tool_number = self.HAL_GCOMP_.newpin('number', hal.HAL_S32, hal.HAL_IN)
        self.changed = self.HAL_GCOMP_.newpin('changed', hal.HAL_BIT, hal.HAL_OUT)
        #self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + 'change_button', hal.HAL_BIT, hal.HAL_IN)
        self.HAL_GCOMP_.comp.setprefix(oldname)
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('toolDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.speak = self.PREFS_.getpref('toolDialog_speak', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('toolDialog_sound_type', 'RING', str, 'DIALOG_OPTIONS')
        else:
            self.play_sound = False

    def showtooldialog(self, message, more_info=None, details=None, display_type=1,
                       icon=QMessageBox.Information):

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.FramelessWindowHint | Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
        self.setIcon(icon)
        self.setTextFormat(Qt.RichText)
        self.setText('<b>%s</b>' % message)
        if more_info:
            self.setInformativeText(more_info)
        else:
            self.setInformativeText('')
        if details:
            self.setDetailedText(details)
        if display_type == self.OK_TYPE:
            self.setStandardButtons(QMessageBox.Ok)
        else:
            self.setStandardButtons(QMessageBox.Ok | QMessageBox.Cancel)
            self.setDefaultButton(QMessageBox.Ok)

        self.show()
        self.calculate_placement()
        retval = self.exec_()
        record_geometry(self,'ToolChangeDialog-geometry')
        if retval == QMessageBox.Cancel:
            return False
        else:
            return True

    def tool_change(self, change):
        if change:
            MORE = 'Please Insert Tool %d' % self.tool_number.get()
            MESS = 'Manual Tool Change Request'
            DETAILS = ' Tool Info:'

            STATUS.emit('focus-overlay-changed', True, MESS, self._color)
            if self.speak:
                STATUS.emit('play-alert', 'speak %s' % MORE)
            if self.play_sound:
                STATUS.emit('play-alert', self.sound_type)

            # desktop notify dialog
            if self.useDesktopDialog:
                NOTICE.notify_ok(MESS, MORE, None, 0, self._pin_change)
                return
            # Qt dialog
            answer = self.showtooldialog(MESS, MORE, DETAILS)
            STATUS.emit('focus-overlay-changed', False, None, None)
            self._pin_change(answer)
        elif not change:
            self.changed.set(False)

    # this also is called from Destop Dialog
    def _pin_change(self,answer):
                if answer:
                    self.changed.set(True)
                else:
                    ACTION.ABORT()
                    STATUS.emit('update-machine-log', 'tool change Aorted', 'TIME')
                STATUS.emit('focus-overlay-changed', False, None, None)

    def calculate_placement(self):
        geometry_parsing(self,'ToolChangeDialog-geometry')

    # **********************
    # Designer properties
    # **********************
    # inherited


################################################################################
# File Open Dialog
################################################################################
class FileDialog(QFileDialog, _HalWidgetBase):
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
        exts = INFO.get_qt_filter_extensions()
        self.setNameFilter(exts)
        self.default_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files/examples'))

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('FileDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        STATUS.connect('dialog-request', self._external_request)
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('fileDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('fileDialog_sound_type', 'RING', str, 'DIALOG_OPTIONS')
            last_path = self.PREFS_.getpref('last_file_path', self.default_path, str, 'BOOK_KEEPING')
            self.setDirectory(last_path)
        else:
            self.play_sound = False

    def _external_request(self, w, message):
        if message.get('NAME') == self._load_request_name:
            # if there is an ID then a file name response is expected
            if message.get('ID'):
                message['RETURN'] = self.load_dialog(True)
                STATUS.emit('general', message)
            else:
                self.load_dialog()
        elif message.get('NAME') == self._save_request_name:
            if message.get('ID'):
                message['RETURN'] = self.save_dialog()
                STATUS.emit('general', message)

    def load_dialog(self, return_path=False):
        self.setFileMode(QFileDialog.ExistingFile)
        self.setAcceptMode(QFileDialog.AcceptOpen)
        self.setWindowTitle('Open')
        STATUS.emit('focus-overlay-changed', True, 'Open Gcode', self._color)
        if self.play_sound:
            STATUS.emit('play-alert', self.sound_type)
        self.calculate_placement()
        fname = None
        if (self.exec_()):
            fname = self.selectedFiles()[0]
            path = self.directory().absolutePath()
            self.setDirectory(path)
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'FileDialog-geometry')
        if fname and not return_path: 
            if self.PREFS_:
                self.PREFS_.putpref('last_file_path', path, str, 'BOOK_KEEPING')
            ACTION.OPEN_PROGRAM(fname)
            STATUS.emit('update-machine-log', 'Loaded: ' + fname, 'TIME')
        return fname

    def save_dialog(self):
        self.setFileMode(QFileDialog.AnyFile)
        self.setAcceptMode(QFileDialog.AcceptSave)
        self.setWindowTitle('Save')
        STATUS.emit('focus-overlay-changed', True, 'Save Gcode', self._color)
        if self.play_sound:
            STATUS.emit('play-alert', self.sound_type)
        self.calculate_placement()
        fname = None
        if (self.exec_()):
            fname = self.selectedFiles()[0]
            path = self.directory().absolutePath()
            self.setDirectory(path)
        else:
            fname = None
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'FileDialog-geometry')
        if fname: 
            if self.PREFS_:
                self.PREFS_.putpref('last_file_path', path, str, 'BOOK_KEEPING')
        return fname

    def calculate_placement(self):
        geometry_parsing(self,'FileDialog-geometry')

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


################################################################################
# origin Offset Dialog
################################################################################
class OriginOffsetDialog(QDialog, _HalWidgetBase):
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
        for i in('X', 'Y', 'Z'):
            b = 'button_%s' % i
            self[b] = QPushButton('Zero %s' % i)
            self[b].clicked.connect(self.zeroPress('%s' % i))
            buttonBox.addButton(self[b], 3)

        v = QVBoxLayout()
        h = QHBoxLayout()
        self._o = OFFVIEW_WIDGET()
        self._o._hal_init()
        self.setLayout(v)
        v.addWidget(self._o)
        b = QPushButton('OK')
        b.clicked.connect(lambda: self.close())
        h.addWidget(b)
        h.addWidget(buttonBox)
        v.addLayout(h)
        self.setModal(True)

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('OriginOffsetDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        self.topParent = self.QTVCP_INSTANCE_
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
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
        self.calculate_placement()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'OriginOffsetDialog-geometry')

    def calculate_placement(self):
        geometry_parsing(self,'OriginOffsetDialog-geometry')

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

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)


################################################################################
# Tool Offset Dialog
################################################################################
class ToolOffsetDialog(QDialog, _HalWidgetBase):
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
        buttonBox = QDialogButtonBox()
        buttonBox.setEnabled(False)
        STATUS.connect('not-all-homed', lambda w, axis: buttonBox.setEnabled(False))
        STATUS.connect('all-homed', lambda w: buttonBox.setEnabled(True))
        STATUS.connect('state-estop', lambda w: buttonBox.setEnabled(False))
        STATUS.connect('state-estop-reset', lambda w: buttonBox.setEnabled(STATUS.machine_is_on()
                                                                           and STATUS.is_all_homed()))
        for i in('X', 'Y', 'Z'):
            b = 'button_%s' % i
            self[b] = QPushButton('Zero %s' % i)
            self[b].clicked.connect(self.zeroPress('%s' % i))
            buttonBox.addButton(self[b], 3)

        v = QVBoxLayout()
        h = QHBoxLayout()
        self._o = TOOLVIEW_WIDGET()
        self._o._hal_init()
        self.setLayout(v)
        v.addWidget(self._o)
        b = QPushButton('OK')
        b.clicked.connect(lambda: self.close())
        h.addWidget(b)
        h.addWidget(buttonBox)
        v.addLayout(h)
        self.setModal(True)

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('ToolOffsetDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        self.topParent = self.QTVCP_INSTANCE_
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
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
        self.calculate_placement()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'ToolOffsetDialog-geometry')

    def calculate_placement(self):
        geometry_parsing(self,'ToolOffsetDialog-geometry')

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

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)


################################################################################
# CamView Dialog
################################################################################
class CamViewDialog(QDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(CamViewDialog, self).__init__(parent)
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
        self.b = QPushButton("Close")
        self.b.clicked.connect(lambda: self.close())
        h.addWidget(self.b)
        l = QVBoxLayout()
        o = CamView()
        o._hal_init()
        self.setLayout(l)
        l.addWidget(o)
        l.addLayout(h)

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('CamViewDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        self.topParent = self.QTVCP_INSTANCE_
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
            self.load_dialog()

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Cam View Dialog', self._color)
        self.calculate_placement()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'CamViewDialog-geometry')

    def calculate_placement(self):
        geometry_parsing(self,'CamViewDialog-geometry')

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

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)


################################################################################
# MacroTab Dialog
################################################################################
class MacroTabDialog(QDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(MacroTabDialog, self).__init__(parent)
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
        # ok now instantiate patched class
        self.tab = MacroTab()
        l = QVBoxLayout()
        self.setLayout(l)
        l.addWidget(self.tab)
        #we need the close button
        self.tab.closeButton.setVisible(True)

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('MacroTabDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        # gotta call this since we instantiated this out of qtvcp's knowledge
        self.tab._hal_init()
        self.topParent = self.QTVCP_INSTANCE_
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
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

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Lathe Macro Dialog', self._color)
        self.tab.stack.setCurrentIndex(0)
        self.calculate_placement()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'MacroTabDialog-geometry')

    def calculate_placement(self):
        geometry_parsing(self,'MacroTabDialog-geometry')

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

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

################################################################################
# Versaprobe Dialog
################################################################################
class VersaProbeDialog(QDialog, _HalWidgetBase):
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
        self._o.hal_init(self.HAL_GCOMP_, self.HAL_NAME_, self.QT_OBJECT_,
                     self.QTVCP_INSTANCE_, self.PATHS_, self.PREFS_)
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('VersaProbeDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        self.topParent = self.QTVCP_INSTANCE_
        STATUS.connect('dialog-request', self._external_request)

    def _external_request(self, w, message):
        if message['NAME'] == self._request_name:
            self.load_dialog()

    def load_dialog(self):
        STATUS.emit('focus-overlay-changed', True, 'VersaProbe Dialog', self._color)
        self.calculate_placement()
        self.show()
        self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'VersaProbeDialog-geometry')

    def calculate_placement(self):
        geometry_parsing(self,'VersaProbeDialog-geometry')

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

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

############################################
# Entry Dialog
############################################
class EntryDialog(QDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(EntryDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self.play_sound = False
        self._request_name = 'ENTRY'
        self.title = 'Numerical Entry'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)

        l = QVBoxLayout()
        self.setLayout(l)

        o = TouchInputWidget()
        l.addWidget(o)

        self.Num = QLineEdit()
        # actiate touch input
        self.Num.keyboard_type = 'numeric'
        self.Num.returnPressed.connect(lambda: self.accept())

        gl = QVBoxLayout()
        gl.addWidget(self.Num)

        self.bBox = QDialogButtonBox()
        self.bBox.addButton('Apply', QDialogButtonBox.AcceptRole)
        self.bBox.addButton('Cancel', QDialogButtonBox.RejectRole)
        self.bBox.rejected.connect(self.reject)
        self.bBox.accepted.connect(self.accept)

        gl.addWidget(self.bBox)
        o.setLayout(gl)

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('EntryDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('toolDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('toolDialog_sound_type', 'RING', str, 'DIALOG_OPTIONS')
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
            t = message.get('TITLE')
            if t:
                self.title = t
            else:
                self.title = 'Entry'
            num = self.showdialog()
            message['RETURN'] = num
            STATUS.emit('general', message)

    def showdialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Origin Setting', self._color)
        self.setWindowTitle(self.title);
        if self.play_sound:
            STATUS.emit('play-alert', self.sound_type)
        self.calculate_placement()
        retval = self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'EntryDialog-geometry')
        LOG.debug("Value of pressed button: {}".format(retval))
        if retval is not None:
            try:
                return float(self.Num.text())
            except Exception as e:
                print e
        return None

    def calculate_placement(self):
        geometry_parsing(self,'EntryDialog-geometry')

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    overlay_color = pyqtProperty(QColor, getColor, setColor)

############################################
# Calculator Dialog
############################################
class CalculatorDialog(Calculator, _HalWidgetBase):
    def __init__(self, parent=None):
        super(CalculatorDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self.play_sound = False
        self._request_name = 'CALCULATOR'
        self.title = 'Calculator Entry'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)

    def _hal_init(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        geo = '%s %s %s %s'% (x,y,w,h)
        self._default_geometry=[x,y,w,h]
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref('CalculatorDialog-geometry', geo, str, 'DIALOG_OPTIONS')
        else:
            self._geometry_string = 'default'
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('CalculatorDialog_play_sound', True, bool, 'DIALOG_OPTIONS')
            self.sound_type = self.PREFS_.getpref('CalculatorDialog_sound_type', 'RING', str, 'DIALOG_OPTIONS')
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
            t = message.get('TITLE')
            if t:
                self.title = t
            else:
                self.title = 'Entry'
            num = self.showdialog()
            message['RETURN'] = num
            STATUS.emit('general', message)

    def showdialog(self):
        STATUS.emit('focus-overlay-changed', True, 'Origin Setting', self._color)
        self.setWindowTitle(self.title);
        if self.play_sound:
            STATUS.emit('play-alert', self.sound_type)
        self.calculate_placement()
        retval = self.exec_()
        STATUS.emit('focus-overlay-changed', False, None, None)
        record_geometry(self,'EntryDialog-geometry')
        LOG.debug("Value of pressed button: {}".format(retval))
        if retval is not None:
            try:
                return float(self.display.text())
            except:
                pass
        return None

    def calculate_placement(self):
        geometry_parsing(self,'EntryDialog-geometry')

    def getColor(self):
        return self._color
    def setColor(self, value):
        self._color = value
    def resetState(self):
        self._color = QColor(0, 0, 0, 150)

    overlay_color = pyqtProperty(QColor, getColor, setColor)


# This general function parses the geometry string and places
# the dialog based on what it finds.
# there are directive words allowed.
# If there are no letters in thw string , it will check the
# preference file (if there is one) to see what the last position
# was. If all else fails it uses it's natural Designer stated
# geometry
def geometry_parsing(widget, prefname):
        def go(x,y,w,h):
            widget.setGeometry(x,y,w,h)
        try:
            if widget._geometry_string.replace(' ','').isdigit():
                widget._geometry_string = widget.PREFS_.getpref(prefname, '', str, 'DIALOG_OPTIONS')
            # If there is a preference file object use it to load the geometry
            if widget._geometry_string in('default',''):
                x,y,w,h = widget._default_geometry
                go(x,y,w,h)
            elif 'center' in widget._geometry_string.lower():
                geom = widget.frameGeometry()
                geom.moveCenter(QDesktopWidget().availableGeometry().center())
                widget.setGeometry(geom)
                return
            elif 'bottomleft' in widget._geometry_string.lower():
                # move to botton left of parent
                ph = widget.topParent.geometry().height()
                px = widget.topParent.geometry().x()
                py = widget.topParent.geometry().y()
                dw = widget.geometry().width()
                dh = widget.geometry().height()
                go(px, py+ph-dh, dw, dh)
            elif 'onwindow' in widget._geometry_string.lower():
                # move relative to parent position
                px = widget.topParent.geometry().x()
                py = widget.topParent.geometry().y()
                # remove everything except digits and spaces
                temp =  filter(lambda x: (x.isdigit() or x == ' '), widget._geometry_string)
                # remove lead and trailing spaces and then slit on spaces
                temp = temp.strip(' ').split(' ')
                go(px+int(temp[0]), py+int(temp[1]), int(temp[2]), int(temp[3]))
            else:
                temp = widget._geometry_string.split(' ')
                go(int(temp[0]), int(temp[1]), int(temp[2]), int(temp[3]))
        except Exception as e:
            LOG.error('Calculating geometry of {} using natural placement.'.format(widget.HAL_NAME_))
            LOG.debug('Dialog gometry python error: {}'.format(e))
            x = widget.geometry().x()
            y = widget.geometry().y()
            w = widget.geometry().width()
            h = widget.geometry().height()
            go( x,y,w,h)

def record_geometry(widget, prefname):
    if widget.PREFS_ :
        temp = widget._geometry_string.replace(' ','')
        if temp =='' or temp.isdigit():
            LOG.debug('Saving {} data from widget {} to file.'.format( prefname,widget.HAL_NAME_))
            x = widget.geometry().x()
            y = widget.geometry().y()
            w = widget.geometry().width()
            h = widget.geometry().height()
            geo = '%s %s %s %s'% (x,y,w,h)
            widget.PREFS_.putpref(prefname, geo, str, 'DIALOG_OPTIONS')

################################
# for testing without editor:
################################
def main():
    import sys
    from PyQt5.QtWidgets import QApplication

    app = QApplication(sys.argv)
    widget = CalculatorDialog()
    widget.showdialog()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
