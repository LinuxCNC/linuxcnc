#!/usr/bin/python2.7
#
# Qtvcp widget
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
###############################################################################

import os
import time

from PyQt5 import QtCore, QtWidgets, QtGui
import linuxcnc

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.widgets.dialog_widget import LcncDialog as CloseDialog
from qtvcp.lib.message import Message
from qtvcp.lib.notify import Notify
from qtvcp.lib.audio_player import Player
from qtvcp.lib.preferences import Access
from qtvcp.lib.machine_log import MachineLogger
from qtvcp.core import Status, Info, Tool
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# NOTE is for desktop popup notification
# MSG is for user-defined dialog popup messages
# INFO is INI file details
# MLOG is for machine error/message logging to file
# LOG is for running code logging
# SOUND is for playing alert sounds
STATUS = Status()
NOTE = Notify()
MSG = Message()
INFO = Info()
TOOL = Tool()
MLOG = MachineLogger()
LOG = logger.getLogger(__name__)

try:
    SOUND = Player()
except:
    LOG.warning('Sound Player did not load')

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class ScreenOptions(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(ScreenOptions, self).__init__(parent)
        self.error = linuxcnc.error_channel()
        self.catch_errors = True
        self.desktop_notify = True
        self.close_event = True
        self.play_sounds = True
        self.mchnMsg_play_sound = True
        self.usrMsg_play_sound = True
        self.usrMsg_sound_type = 'READY'
        self.usrMsg_use_FocusOverlay = True
        self.play_shutdown_sounds = True
        self.user_messages = True
        self.use_pref_file = True
        self.pref_filename = '~/.qtvcp_screen_preferences'
        self._close_color = QtGui.QColor(100, 0, 0, 150)

    # self.QTVCP_INSTANCE_
    # self.HAL_GCOMP_
    # come from base class
    def _hal_init(self):
        # Read user preferences
        if self.PREFS_:
            self.catch_errors = self.PREFS_.getpref('catch_errors', True, bool, 'SCREEN_OPTIONS')
            self.desktop_notify = self.PREFS_.getpref('desktop_notify', True, bool, 'SCREEN_OPTIONS')
            self.close_event = self.PREFS_.getpref('shutdown_check', True, bool, 'SCREEN_OPTIONS')
            self.play_sounds = self.PREFS_.getpref('sound_player_on', True, bool, 'SCREEN_OPTIONS')
            self.mchnMsg_play_sound = self.PREFS_.getpref('mchnMsg_play_sound', True, bool, 'MCH_MSG_OPTIONS')
            self.mchnMsg_speak_errors = self.PREFS_.getpref('mchnMsg_speak_errors', True, bool, 'MCH_MSG_OPTIONS')
            self.mchnMsg_sound_type = self.PREFS_.getpref('mchnMsg_sound_type', 'ERROR', str, 'MCH_MSG_OPTIONS')
            self.usrMsg_play_sound = self.PREFS_.getpref('usermsg_play_sound', True, bool, 'USR_MSG_OPTIONS')
            self.usrMsg_sound_type = self.PREFS_.getpref('userMsg_sound_type', 'RING', str, 'USR_MSG_OPTIONS')
            self.usrMsg_use_FocusOverlay = self.PREFS_.getpref('userMsg_use_focusOverlay',
                                                               True, bool, 'USR_MSG_OPTIONS')
            self.shutdown_play_sound = self.PREFS_.getpref('shutdown_play_sound', True, bool, 'SHUTDOWN_OPTIONS')
            self.shutdown_alert_sound_type = self.PREFS_.getpref('shutdown_alert_sound_type', 'ATTENTION',
                                                                 str, 'SHUTDOWN_OPTIONS')
            self.shutdown_exit_sound_type = self.PREFS_.getpref('shutdown_exit_sound_type', 'LOGOUT',
                                                                str, 'SHUTDOWN_OPTIONS')
            self.shutdown_msg_title = self.PREFS_.getpref('shutdown_msg_title', 'Do you want to Shutdown now?',
                                                          str, 'SHUTDOWN_OPTIONS')
            self.shutdown_msg_detail = self.PREFS_.getpref('shutdown_msg_detail',
                                                           'This option can be changed in the preference file',
                                                           str, 'SHUTDOWN_OPTIONS')
            self.notify_start_title = self.PREFS_.getpref('notify_start_title', 'Welcome', str, 'NOTIFY_OPTIONS')
            self.notify_start_detail = self.PREFS_.getpref('notify_start_detail', 'This is a test screen for QtVCP',
                                                           str, 'NOTIFY_OPTIONS')
            self.notify_start_timeout = self.PREFS_.getpref('notify_start_timeout', 10, int, 'NOTIFY_OPTIONS')
        # connect to STATUS to catch linuxcnc events
        if self.catch_errors:
            STATUS.connect('periodic', self.on_periodic)
            STATUS.connect('error', self.process_error)
        if self.close_event:
            self.QTVCP_INSTANCE_.closeEvent = self.closeEvent
        if self.play_sounds:
            try:
                SOUND._register_messages()
            except:
                self.play_sounds = False
                LOG.warning('Sound Option turned off due to error registering')
        if self.user_messages:
            MSG.message_setup(self.HAL_GCOMP_)
            MSG.message_option('NOTIFY', NOTE)
            if self.play_sounds:
                MSG.message_option('play_sounds', self.usrMsg_play_sound)
            else:
                MSG.message_option('play_sounds', False)
            MSG.message_option('alert_sound', self.usrMsg_sound_type)
            MSG.message_option('use_focus_overlay', self.usrMsg_use_FocusOverlay)
        # If there is a widget named statusBar give a reference to desktop notify
        try:
            NOTE.statusbar = self.QTVCP_INSTANCE_.statusbar
        except Exception as e:
            LOG.info('Exception adding status to notify:', exc_info=e)
        if self.desktop_notify:
            NOTE.notify(self.notify_start_title, self.notify_start_detail, None,
                        self.notify_start_timeout, self. notify_start_timeout)
        # clear and add an intial machine log message
        STATUS.emit('update-machine-log', '', 'DELETE')
        STATUS.emit('update-machine-log', '', 'INITIAL')
        STATUS.connect('tool-info-changed', lambda w, data: self._tool_file_info(data, TOOL.COMMENTS))
        # We supply our own dialog for closing
        self.CLOSE_DIALOG = CloseDialog()

    # This is called early by qt_makegui.py for access to
    # be able to pass the preference object to ther widgets
    def _pref_init(self):
        if self.use_pref_file:
            return Access(self.pref_filename), self.pref_filename
        return None

    def on_periodic(self, w):
        e = self.error.poll()
        if e:
            kind, text = e
            STATUS.emit('error',kind,text)

    def process_error(self, w, kind, text):
            if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                if self.desktop_notify:
                    NOTE.notify('ERROR', text, None, 0, 4)
            elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
                if self.desktop_notify:
                    NOTE.notify('OP MESSAGE', text, None, 0, 4)
            elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
                if self.desktop_notify:
                    NOTE.notify('DISPLAY', text, None, 0, 4)
            if self.play_sounds and self.mchnMsg_play_sound:
                STATUS.emit('play-alert', '%s' % self.mchnMsg_sound_type)
                if self.mchnMsg_speak_errors:
                    STATUS.emit('play-alert', 'SPEAK %s ' % text)
            STATUS.emit('update-machine-log', text, 'TIME')


    def closeEvent(self, event):
        if self.close_event:
            sound = None
            if self.play_sounds and self.play_shutdown_sounds:
                sound = self.shutdown_alert_sound_type
            answer = self.CLOSE_DIALOG.showdialog(self.shutdown_msg_title,
                                                                 None,
                                                                 details=self.shutdown_msg_detail,
                                                                 icon=MSG.CRITICAL,
                                                                 display_type=MSG.YN_TYPE,
                                                                 focus_text='Shutdown Requested!',
                                                                 focus_color=self._close_color,
                                                                 play_alert=sound)
            if not answer:
                event.ignore()
                return
            if self.play_sounds and self.play_shutdown_sounds:
                STATUS.emit('play-alert', self.shutdown_exit_sound_type)
                try:
                    self.QTVCP_INSTANCE_.handler_instance.closing_cleanup__()
                except:
                    pass
            event.accept()

        # [0] = tool number
        # [1] = pocket number
        # [2] = X offset
        # [3] = Y offset
        # [4] = Z offset
        # [5] = A offset
        # [6] = B offset
        # [7] = C offset
        # [8] = U offset
        # [9] = V offset
        # [10] = W offset
        # [11] = tool diameter
        # [12] = frontangle
        # [13] = backangle
        # [14] = tool orientation
        # [15] = tool comment
    def _tool_file_info(self, tool_entry, index):
        toolnum = tool_entry[0]
        tool_table_line = TOOL.GET_TOOL_INFO(toolnum)
        text = 'Tool %s: %s'%(str(tool_table_line[0]),str(tool_table_line[index]))
        STATUS.emit('update-machine-log', text, 'TIME')

    ########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def set_notify(self, data):
        self.desktop_notify = data
    def get_notify(self):
        return self.desktop_notify
    def reset_notify(self):
        self.desktop_notify = True

    def set_close(self, data):
        self.close_event = data
    def get_close(self):
        return self.close_event
    def reset_close(self):
        self.close_event = True

    def set_errors(self, data):
        self.catch_errors = data
    def get_errors(self):
        return self.catch_errors
    def reset_errors(self):
        self.catch_errors = True

    def set_play_sounds(self, data):
        self.play_sounds = data
    def get_play_sounds(self):
        return self.play_sounds
    def reset_play_sounds(self):
        self.play_sounds = True

    def set_use_pref_file(self, data):
        self.use_pref_file = data
    def get_use_pref_file(self):
        return self.use_pref_file
    def reset_use_pref_file(self):
        self.use_pref_file = True

    def set_pref_filename(self, data):
        self.pref_filename = data
    def get_pref_filename(self):
        return self.pref_filename
    def reset_pref_filename(self):
        self.pref_filename = '~/.qtvcp_screen_preferences'

    def getColor(self):
        return self._close_color
    def setColor(self, value):
        self._close_color = value
    def resetState(self):
        self._close_color = QtGui.QColor(100, 0, 0, 150)

    # designer will show these properties in this order:
    notify_option = QtCore.pyqtProperty(bool, get_notify, set_notify, reset_notify)
    catch_close_option = QtCore.pyqtProperty(bool, get_close, set_close, reset_close)
    catch_errors_option = QtCore.pyqtProperty(bool, get_errors, set_errors, reset_errors)
    play_sounds_option = QtCore.pyqtProperty(bool, get_play_sounds, set_play_sounds, reset_play_sounds)
    use_pref_file_option = QtCore.pyqtProperty(bool, get_use_pref_file, set_use_pref_file, reset_use_pref_file)
    pref_filename_string = QtCore.pyqtProperty(str, get_pref_filename, set_pref_filename, reset_pref_filename)
    close_overlay_color = QtCore.pyqtProperty(QtGui.QColor, getColor, setColor)
