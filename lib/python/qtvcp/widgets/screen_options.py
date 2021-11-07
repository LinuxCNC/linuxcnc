#!/usr/bin/env python3
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
from qtvcp.widgets.xembed import XEmbeddable
from qtvcp.lib.message import Message
from qtvcp.lib.notify import Notify
from qtvcp.lib.audio_player import Player
from qtvcp.lib.preferences import Access
from qtvcp.lib.machine_log import MachineLogger
from qtvcp.core import Status, Info, Tool, Path, Action
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# NOTICE is for desktop popup notification
# MSG is for user-defined dialog popup messages
# INFO is INI file details
# MLOG is for machine error/message logging to file
# LOG is for running code logging
# SOUND is for playing alert sounds
STATUS = Status()
NOTICE = Notify()
MSG = Message()
INFO = Info()
TOOL = Tool()
PATH = Path()
MLOG = MachineLogger()
LOG = logger.getLogger(__name__)
ACTION = Action()

try:
    SOUND = Player()
except Exception as e:
    LOG.warning('Sound Player did not load: {}'.format(e))

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

# only import zmq if needed and present
def import_ZMQ():
    try:
        import zmq
    except:
        LOG.warning('Problem importing zmq - Is python-zmg installed?')
        # nope no messaging for you
        return False
    else:
        import json
        # since we imported in a function we need to globalize
        # the libraries so screenoptions can reference them.
        global zmq
        global json
        # imports are good - go ahead and setup messaging
        return True

class ScreenOptions(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(ScreenOptions, self).__init__(parent)
        self.error = linuxcnc.error_channel()
        self.catch_errors = True
        self.desktop_notify = True
        self.notify_max_msgs = 10
        self.close_event = True
        self.play_sounds = True
        self.mchnMsg_play_sound = True
        self.mchnMsg_speak_errors = False
        self.mchnMsg_speak_text = True
        self.mchnMsg_sound_type  = 'ERROR'
        self.usrMsg_play_sound = True
        self.usrMsg_sound_type = 'BELL'
        self.usrMsg_use_FocusOverlay = True
        self.shutdown_play_sound = True
        self.shutdown_alert_sound_type = 'READY'
        self.shutdown_exit_sound_type = 'LOGOUT'
        self.notify_start_greeting = False
        self.notify_start_title = 'Welcome'
        self.notify_start_detail = 'This option can be changed in the preference file'
        self.notify_start_timeout = 5
        self.shutdown_msg_title = 'Do you want to Shutdown now?'
        self.shutdown_msg_focus_text = ''
        self.shutdown_msg_detail = ''
        self.user_messages = True
        self.use_pref_file = True
        self.process_tabs = True
        self.add_message_dialog = True
        self.add_close_dialog = True
        self.add_entry_dialog = False
        self.add_tool_dialog = False
        self.add_file_dialog = False
        self.add_focus_overlay = False
        self.add_keyboard_dialog = False
        self.add_versaprobe_dialog = False
        self.add_macrotab_dialog = False
        self.add_camview_dialog = False
        self.add_originoffset_dialog = False
        self.add_tooloffset_dialog = False
        self.add_calculator_dialog = False
        self.add_machinelog_dialog = False
        self.add_runFromLine_dialog = False
        self.add_send_zmq = False
        self.add_receive_zmq = False

        self.pref_filename = '~/.qtvcp_screen_preferences'
        self._default_tab_name = ''
        self._close_color = QtGui.QColor(100, 0, 0, 150)
        self._messageDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._entryDialogSoftkey = True
        self._entryDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._toolDialogColor = QtGui.QColor(100, 0, 0, 150)
        self._fileDialogColor = QtGui.QColor(0, 0, 100, 150)
        self._keyboardDialogColor = QtGui.QColor(0, 0, 100, 150)
        self._versaProbeDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._macroTabDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._camViewDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._originOffsetDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._toolOffsetDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._toolUseDesktopNotify = False
        self._toolFrameless = False
        self._calculatorDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._machineLogDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._runFromLineDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._zmq_sub_subscribe_name = b""
        self._zmq_sub_socket_address = "tcp://127.0.0.1:5690"
        self._zmq_pub_socket_address = "tcp://127.0.0.1:5690"
        self._halBaseName = ''

    # self.QTVCP_INSTANCE_
    # self.HAL_GCOMP_
    # self.PREFS_
    # come from base class
    def _hal_init(self):
        if self.add_message_dialog:
            self.init_message_dialog()

        if self.add_close_dialog:
            self.init_close_dialog()

        if self.add_entry_dialog:
            self.init_entry_dialog()

        if self.add_tool_dialog:
            self.init_tool_dialog()

        if self.add_file_dialog:
            self.init_file_dialog()

        if self.add_focus_overlay:
            self.init_focus_overlay()

        if self.add_keyboard_dialog:
            self.init_keyboard_dialog()

        if self.add_versaprobe_dialog:
            self.init_versaprobe_dialog()

        if self.add_macrotab_dialog:
            self.init_macrotab_dialog()

        if self.add_camview_dialog:
            self.init_camview_dialog()

        if self.add_tooloffset_dialog:
            self.init_tooloffset_dialog()

        if self.add_originoffset_dialog:
            self.init_originoffset_dialog()

        if self.add_calculator_dialog:
            self.init_calculator_dialog()

        if self.add_machinelog_dialog:
            self.init_machinelog_dialog()

        if self.add_runFromLine_dialog:
            self.init_runfromline_dialog()

        # Read user preferences
        if self.PREFS_:
            self.catch_errors = self.PREFS_.getpref('catch_errors', self.catch_errors, bool, 'SCREEN_OPTIONS')
            self.desktop_notify = self.PREFS_.getpref('desktop_notify', self.desktop_notify, bool, 'SCREEN_OPTIONS')
            self.notify_max_msgs = self.PREFS_.getpref('notify_max_msgs', self.notify_max_msgs, int, 'SCREEN_OPTIONS')
            self.close_event = self.PREFS_.getpref('shutdown_check', self.close_event, bool, 'SCREEN_OPTIONS')
            self.play_sounds = self.PREFS_.getpref('sound_player_on', self.play_sounds, bool, 'SCREEN_OPTIONS')
            self.mchnMsg_play_sound = self.PREFS_.getpref('mchnMsg_play_sound', self.mchnMsg_play_sound, bool, 'MCH_MSG_OPTIONS')
            self.mchnMsg_speak_errors = self.PREFS_.getpref('mchnMsg_speak_errors', self.mchnMsg_speak_errors, bool, 'MCH_MSG_OPTIONS')
            self.mchnMsg_speak_text = self.PREFS_.getpref('mchnMsg_speak_text', self.mchnMsg_speak_text, bool, 'MCH_MSG_OPTIONS')
            self.mchnMsg_sound_type = self.PREFS_.getpref('mchnMsg_sound_type', self.usrMsg_sound_type, str, 'MCH_MSG_OPTIONS')
            self.usrMsg_play_sound = self.PREFS_.getpref('usermsg_play_sound', self.usrMsg_play_sound, bool, 'USR_MSG_OPTIONS')
            self.usrMsg_sound_type = self.PREFS_.getpref('userMsg_sound_type', self.usrMsg_sound_type, str, 'USR_MSG_OPTIONS')
            self.usrMsg_use_FocusOverlay = self.PREFS_.getpref('userMsg_use_focusOverlay',
                                                               self.usrMsg_use_FocusOverlay, bool, 'USR_MSG_OPTIONS')
            self.shutdown_play_sound = self.PREFS_.getpref('shutdown_play_sound', self.shutdown_play_sound, bool, 'SHUTDOWN_OPTIONS')
            self.shutdown_alert_sound_type = self.PREFS_.getpref('shutdown_alert_sound_type', self.shutdown_alert_sound_type,
                                                                 str, 'SHUTDOWN_OPTIONS')
            self.shutdown_exit_sound_type = self.PREFS_.getpref('shutdown_exit_sound_type', self.shutdown_exit_sound_type,
                                                                str, 'SHUTDOWN_OPTIONS')
            self.shutdown_msg_title = self.PREFS_.getpref('shutdown_msg_title', self.shutdown_msg_title,
                                                          str, 'SHUTDOWN_OPTIONS')
            self.shutdown_msg_focus_text = self.PREFS_.getpref('shutdown_msg_focus_text', self.shutdown_msg_focus_text,
                                                          str, 'SHUTDOWN_OPTIONS')
            self.shutdown_msg_detail = self.PREFS_.getpref('shutdown_msg_detail',
                                                            self.shutdown_msg_detail,
                                                           str, 'SHUTDOWN_OPTIONS')
            self.notify_start_greeting = self.PREFS_.getpref('notify_start_greeting', self.notify_start_greeting, bool, 'NOTIFY_OPTIONS')
            self.notify_start_title = self.PREFS_.getpref('notify_start_title', self.notify_start_title, str, 'NOTIFY_OPTIONS')
            self.notify_start_detail = self.PREFS_.getpref('notify_start_detail', self.notify_start_detail,
                                                           str, 'NOTIFY_OPTIONS')
            self.notify_start_timeout = self.PREFS_.getpref('notify_start_timeout', self.notify_start_timeout, int, 'NOTIFY_OPTIONS')

        # connect to STATUS to catch linuxcnc events
        if self.catch_errors:
            STATUS.connect('periodic', self.on_periodic)
            STATUS.connect('error', self.process_error)
            STATUS.connect('graphics-gcode-error', lambda w, data: self.process_error(None, linuxcnc.OPERATOR_ERROR, data))

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
            MSG.message_option('NOTIFY', NOTICE)
            if self.play_sounds:
                MSG.message_option('play_sounds', self.usrMsg_play_sound)
            else:
                MSG.message_option('play_sounds', False)
            MSG.message_option('alert_sound', self.usrMsg_sound_type)
            MSG.message_option('use_focus_overlay', self.usrMsg_use_FocusOverlay)

        # If there is a widget named statusBar give a reference to desktop notify
        try:
            NOTICE.statusbar = self.QTVCP_INSTANCE_.statusbar
        except:
            LOG.debug('cannot add notifications to statusbar - no statusbar?:')

        # critical messages don't timeout, the greeting does
        if self.desktop_notify:
            self.notify_critical = NOTICE.new_critical()
            self.notify_normal = NOTICE.new_normal()
            self.notify_hard_limits = NOTICE.new_hard_limits(callback = self._override_limits)
            if self.notify_start_greeting:
                NOTICE.notify(self.notify_start_title, self.notify_start_detail, None,
                        self.notify_start_timeout, self. notify_start_timeout)

        # add any XEmbed tabs
        if self.process_tabs:
            self.add_xembed_tabs()

        # clear and add an initial machine log message
        STATUS.emit('update-machine-log', '', 'DELETE')
        STATUS.emit('update-machine-log', '', 'INITIAL')
        STATUS.connect('tool-info-changed', lambda w, data: self._tool_file_info(data, TOOL.COMMENTS))

        # install remote control
        if self.add_send_zmq:
            self.init_zmq_publish()
        if self.add_receive_zmq:
            self.init_zmg_subscribe()
            

    # This is called early by qt_makegui.py for access to
    # be able to pass the preference object to the widgets
    def _pref_init(self):
        if self.use_pref_file:
            # we prefer INI settings
            if INFO.PREFERENCE_PATH:
                self.pref_filename = INFO.PREFERENCE_PATH
                LOG.debug('Switching to Preference File Path from INI: {}'.format(INFO.PREFERENCE_PATH))
            # substitute for keywords
            self.pref_filename = self.pref_filename.replace('CONFIGFOLDER',PATH.CONFIGPATH)
            self.pref_filename = self.pref_filename.replace('WORKINGFOLDER',PATH.WORKINGDIR)
            # check that there is a directory present
            dir = os.path.split(str(self.pref_filename))
            dir = os.path.expanduser(dir[0])
            if os.path.exists(dir):
                return Access(self.pref_filename), self.pref_filename
            else:
                raise Exception('Cannot find directory: {} for preference file.'.format(dir))
        return None,None

    # allow screen option to inject data to the main VCP object (basically the window)
    def _VCPObject_injection(self, vcpObject):
        if self.desktop_notify:
            vcpObject._NOTICE = NOTICE # Guve reference

    def on_periodic(self, w):
        try:
            e = self.error.poll()
            if e:
                kind, text = e
                STATUS.emit('error',kind,text)
        except Exception as e:
                LOG.error('Error channel reading error: {}'.format(e))

    def process_error(self, w, kind, text):
            if 'on limit switch error' in text:
                if self.desktop_notify:
                    NOTICE.update(self.notify_hard_limits, title='Machine Error:', message=text, msgs=self.notify_max_msgs)
            elif kind == linuxcnc.OPERATOR_ERROR:
                if self.desktop_notify:
                    NOTICE.update(self.notify_critical, title='Operator Error:', message=text, msgs=self.notify_max_msgs)
            elif kind == linuxcnc.OPERATOR_TEXT:
                if self.desktop_notify:
                    NOTICE.update(self.notify_critical, title='Operator Text:', message=text, msgs=self.notify_max_msgs)
            elif kind == linuxcnc.OPERATOR_DISPLAY:
                if self.desktop_notify:
                    NOTICE.update(self.notify_critical, title='Operator Display:', message=text, msgs=self.notify_max_msgs)

            elif kind == linuxcnc.NML_ERROR:
                if self.desktop_notify:
                    NOTICE.update(self.notify_critical, title='Internal NML Error:', message=text, msgs=self.notify_max_msgs)
            elif kind == linuxcnc.NML_TEXT:
                if self.desktop_notify:
                    NOTICE.update(self.notify_critical, title='Internal NML Text:', message=text, msgs=self.notify_max_msgs)
            elif kind == linuxcnc.NML_DISPLAY:
                if self.desktop_notify:
                    NOTICE.update(self.notify_critical, title='Internal NML Display:', message=text, msgs=self.notify_max_msgs)

            elif kind == STATUS.TEMPARARY_MESSAGE: # temporary info
                if self.desktop_notify:
                    NOTICE.update(self.notify_normal,
                                    title='Operator Info:',
                                    message=text,
                                    status_timeout=0,
                                    timeout=2,
                                    msgs=self.notify_max_msgs)

            if self.play_sounds and self.mchnMsg_play_sound:
                STATUS.emit('play-sound', '%s' % self.mchnMsg_sound_type)
                if self.mchnMsg_speak_errors:
                    if kind in (linuxcnc.OPERATOR_ERROR, linuxcnc.NML_ERROR):
                        STATUS.emit('play-sound', 'SPEAK %s ' % text)
                if self.mchnMsg_speak_text:
                    if kind in (linuxcnc.OPERATOR_TEXT, linuxcnc.NML_TEXT,
                                linuxcnc.OPERATOR_DISPLAY, STATUS.TEMPARARY_MESSAGE):
                        STATUS.emit('play-sound', 'SPEAK %s ' % text)

            STATUS.emit('update-machine-log', text, 'TIME')


    def closeEvent(self, event):
        if self.close_event:
            sound = None
            if self.PREFS_ and self.play_sounds and self.shutdown_play_sound:
                sound = self.shutdown_alert_sound_type
            try:
                if self.shutdown_msg_detail == '': details = None
                else: details = self.shutdown_msg_detail
                answer = self.QTVCP_INSTANCE_.closeDialog_.showdialog(self.shutdown_msg_title,
                                                                 None,
                                                                 details=details,
                                                                 icon=MSG.CRITICAL,
                                                                 display_type='YESNO',
                                                                 focus_text=self.shutdown_msg_focus_text,
                                                                 focus_color=self._close_color,
                                                                 play_alert=sound)
            except:
                answer = True
            # system shutdown
            HANDLER = self.QTVCP_INSTANCE_.handler_instance
            if answer == QtWidgets.QMessageBox.DestructiveRole:
                if 'system_shutdown_request__' in dir(HANDLER):
                    HANDLER.system_shutdown_request__()
                else:
                    from qtvcp.core import Action
                    ACTION = Action()
                    ACTION.SHUT_SYSTEM_DOWN_PROMPT()
                if '_hal_cleanup' in dir(HANDLER):
                    HANDLER._hal_cleanup()
                event.accept()
            # close linuxcnc
            elif answer:
                if self.PREFS_ and self.play_sounds and self.shutdown_play_sound:
                    STATUS.emit('play-sound', self.shutdown_exit_sound_type)
                if '_hal_cleanup' in dir(HANDLER):
                    HANDLER._hal_cleanup()
                event.accept()
            # cancel
            elif answer == False:
                event.ignore()
                return

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
        try:
            toolnum = tool_entry[0]
            tool_table_line = TOOL.GET_TOOL_INFO(toolnum)
            text = 'Tool %s: %s'%(str(tool_table_line[0]),str(tool_table_line[index]))
            STATUS.emit('update-machine-log', text, 'TIME')
        except:
            pass

    # XEmbed program into tabs
    def add_xembed_tabs(self):
        if INFO.GLADEVCP:
            cmd = 'halcmd loadusr -Wn gladevcp gladevcp -c gladevcp -x {XID} %s' %(INFO.GLADEVCP)
            LOG.debug('AXIS style side panel vcp: {} '.format(cmd))
            loc = 'allLayout'
            # TODO this needs work panel doesn't embed
            #self._embed(cmd,loc,None)

        if INFO.TAB_CMDS:
            wid = int(self.QTVCP_INSTANCE_.winId())
            os.environ['QTVCP_FORWARD_EVENTS_TO'] = str(wid)

            for name, loc, cmd in INFO.ZIPPED_TABS:
                LOG.debug('Processing Embedded tab:{}, {}, {}'.format(name,loc,cmd))
                if loc == 'default':
                    loc = 'rightTab'
                try:
                    if isinstance(self.QTVCP_INSTANCE_[loc], QtWidgets.QTabWidget):
                        tw = QtWidgets.QWidget()
                        self.QTVCP_INSTANCE_[loc].addTab(tw, name)
                    elif isinstance(self.QTVCP_INSTANCE_[loc], QtWidgets.QStackedWidget):
                        tw = QtWidgets.QWidget()
                        self.QTVCP_INSTANCE_[loc].addWidget(tw)
                    else:
                        LOG.warning('tab location {} is not a Tab or stacked Widget - skipping'.format(loc))
                        continue
                except Exception as e:
                    LOG.warning("problem inserting VCP '{}' to location: {} :\n {}".format(name, loc, e))
                    return
                self._embed(cmd,loc,tw)

    def _embed(self, cmd,loc,twidget):
            if twidget is None:
               tw = QtWidgets.QWidget()
               tw.setMinimumSize(400,200)
               self.QTVCP_INSTANCE_[loc].addWidget(tw)
            try:
                temp = XEmbeddable()
                temp.embed(cmd)
            except Exception as e:
                LOG.error('Embedded tab loading failed: {} {}'.format(cmd,e))
            else:
                if twidget is not None:
                    try:
                        if loc == 'rightPanel':
                            self.QTVCP_INSTANCE_[loc].setMaximumSize(400,30000)
                            self.QTVCP_INSTANCE_[loc].setMinimumSize(400,0)
                        layout = QtWidgets.QGridLayout(twidget)
                        layout.addWidget(temp, 0, 0)
                    except Exception as e:
                        LOG.error('Embedded tab adding widget into {} failed.'.format(loc))
                else:
                    layout = QtWidgets.QGridLayout(tw)
                    layout.addWidget(temp, 0, 0)

    def init_tool_dialog(self):
        from qtvcp.widgets.dialog_widget import ToolDialog
        w = self.QTVCP_INSTANCE_
        w.toolDialog_ = ToolDialog(w)
        w.toolDialog_.setObjectName('toolDialog_')
        w.toolDialog_.hal_init(HAL_NAME='')
        w.toolDialog_.overlay_color = self._toolDialogColor
        w.toolDialog_.setProperty('useDesktopNotify', self._toolUseDesktopNotify)
        w.toolDialog_.setProperty('frameless', self._toolFrameless)

    def init_message_dialog(self):
        from qtvcp.widgets.dialog_widget import LcncDialog
        w = self.QTVCP_INSTANCE_
        w.messageDialog_ = LcncDialog(w)
        w.messageDialog_.setObjectName('messageDialog_')
        w.messageDialog_.hal_init(HAL_NAME='')
        w.messageDialog_.overlay_color = self._messageDialogColor

    def init_close_dialog(self):
        from qtvcp.widgets.dialog_widget import CloseDialog
        w = self.QTVCP_INSTANCE_
        w.closeDialog_ = CloseDialog(w)
        w.closeDialog_.setObjectName('closeDialog_')
        w.closeDialog_.hal_init(HAL_NAME='')
        w.closeDialog_.overlay_color = self._messageDialogColor

    def init_entry_dialog(self):
        from qtvcp.widgets.dialog_widget import EntryDialog
        w = self.QTVCP_INSTANCE_
        w.entryDialog_ = EntryDialog(w)
        w.entryDialog_.setObjectName('entryDialog_')
        w.entryDialog_.hal_init(HAL_NAME='')
        w.entryDialog_.soft_keyboard_option = self._entryDialogSoftkey
        w.entryDialog_.overlay_color = self._entryDialogColor

    def init_file_dialog(self):
        from qtvcp.widgets.dialog_widget import FileDialog
        w = self.QTVCP_INSTANCE_
        w.fileDialog_ = FileDialog(w)
        w.fileDialog_.setObjectName('fileDialog_')
        w.fileDialog_.hal_init(HAL_NAME='')
        w.fileDialog_.overlay_color = self._fileDialogColor

    def init_focus_overlay(self):
        from qtvcp.widgets.overlay_widget import FocusOverlay
        w = self.QTVCP_INSTANCE_
        w.focusOverlay_ = FocusOverlay(w)
        w.focusOverlay_.setObjectName('focusOverlay_')
        w.focusOverlay_.hal_init(HAL_NAME='')

    def init_keyboard_dialog(self):
        from qtvcp.widgets.dialog_widget import KeyboardDialog
        w = self.QTVCP_INSTANCE_
        w.keyboardDialog_ = KeyboardDialog(w)
        w.keyboardDialog_.setObjectName('keyboardDialog_')
        w.keyboardDialog_.hal_init(HAL_NAME='')
        w.keyboardDialog_.overlay_color = self._keyboardDialogColor

    def init_versaprobe_dialog(self):
        from qtvcp.widgets.dialog_widget import VersaProbeDialog
        w = self.QTVCP_INSTANCE_
        w.versaProbeDialog_ = VersaProbeDialog(w)
        w.versaProbeDialog_.setObjectName('versaProbeDialog_')
        w.registerHalWidget(w.versaProbeDialog_)
        w.versaProbeDialog_.hal_init(HAL_NAME='')
        w.versaProbeDialog_.overlay_color = self._versaProbeDialogColor

    def init_macrotab_dialog(self):
        from qtvcp.widgets.dialog_widget import MacroTabDialog
        w = self.QTVCP_INSTANCE_
        w.macroTabDialog_ = MacroTabDialog(w)
        w.macroTabDialog_.setObjectName('macroTabDialog_')
        w.macroTabDialog_.hal_init(HAL_NAME='')
        w.macroTabDialog_.overlay_color = self._macroTabDialogColor

    def init_camview_dialog(self):
        from qtvcp.widgets.dialog_widget import CamViewDialog
        w = self.QTVCP_INSTANCE_
        w.camViewDialog_ = CamViewDialog(w)
        w.camViewDialog_.setObjectName('camViewDialog_')
        w.camViewDialog_.hal_init(HAL_NAME='')
        w.camViewDialog_.overlay_color = self._camViewDialogColor

    def init_tooloffset_dialog(self):
        from qtvcp.widgets.dialog_widget import ToolOffsetDialog
        w = self.QTVCP_INSTANCE_
        w.toolOffsetDialog_ = ToolOffsetDialog(w)
        w.toolOffsetDialog_.setObjectName('toolOffsetDialog_')
        w.toolOffsetDialog_.hal_init(HAL_NAME='')
        w.toolOffsetDialog_.overlay_color = self._toolOffsetDialogColor

    def init_originoffset_dialog(self):
        from qtvcp.widgets.dialog_widget import OriginOffsetDialog
        w = self.QTVCP_INSTANCE_
        w.originOffsetDialog_ = OriginOffsetDialog(w)
        w.registerHalWidget(w.originOffsetDialog_)
        w.originOffsetDialog_.setObjectName('originOffsetDialog_')
        w.originOffsetDialog_.hal_init(HAL_NAME='')
        w.originOffsetDialog_.overlay_color = self._originOffsetDialogColor

    def init_calculator_dialog(self):
        from qtvcp.widgets.dialog_widget import CalculatorDialog
        w = self.QTVCP_INSTANCE_
        w.calculatorDialog_ = CalculatorDialog(w)
        w.calculatorDialog_.setObjectName('calculatorDialog_')
        w.calculatorDialog_.hal_init(HAL_NAME='')
        w.calculatorDialog_.overlay_color = self._calculatorDialogColor

    def init_machinelog_dialog(self):
        from qtvcp.widgets.dialog_widget import MachineLogDialog
        w = self.QTVCP_INSTANCE_
        w.machineLogDialog_ = MachineLogDialog(w)
        w.machineLogDialog_.setObjectName('machineLogDialog_')
        w.machineLogDialog_.hal_init(HAL_NAME='')
        w.machineLogDialog_.overlay_color = self._machineLogDialogColor

    def init_runfromline_dialog(self):
        from qtvcp.widgets.dialog_widget import RunFromLineDialog
        w = self.QTVCP_INSTANCE_
        w.runFromLineDialog_ = RunFromLineDialog(w)
        w.runFromLineDialog_.setObjectName('runFromLineDialog_')
        w.runFromLineDialog_.hal_init(HAL_NAME='')
        w.runFromLineDialog_.overlay_color = self._runFromLineDialogColor

    def init_zmg_subscribe(self):
        if import_ZMQ():
            try:
                self._zmq_sub_context = zmq.Context()
                self._zmq_sub_sock = self._zmq_sub_context.socket(zmq.SUB)
                self._zmq_sub_sock.connect(self._zmq_sub_socket_address)
                self._zmq_sub_sock.setsockopt(zmq.SUBSCRIBE, self._zmq_sub_subscribe_name)

                self.read_noti = QtCore.QSocketNotifier(
                                    self._zmq_sub_sock.getsockopt(zmq.FD),
                                    QtCore.QSocketNotifier.Read, None)
                self.read_noti.activated.connect(self.on_read_msg)
            except Exception as e:
                LOG.error('zmq subscribe to message setup error: {}'.format(e))

    def on_read_msg(self):
        self.read_noti.setEnabled(False)
        if self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLIN:
            while self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLIN:
                # get raw message
                topic, data = self._zmq_sub_sock.recv_multipart()
                # convert from json object to python object
                y = json.loads(data)
                # get the function name
                function = y.get('FUNCTION')
                # get the arguments
                arguments = y.get('ARGS')
                LOG.debug('{} Sent ZMQ Message:{} {}'.format(topic,function,arguments))

                # call handler function with arguments
                try:
                     self.QTVCP_INSTANCE_[function](arguments)
                except Exception as e:
                    LOG.error('zmq message parcing error: {}'.format(e))
                    LOG.error('{} {}'.format(function, arguments))
        elif self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLOUT:
            print("[Socket] zmq.POLLOUT")
        elif self._zmq_sub_sock.getsockopt(zmq.EVENTS) & zmq.POLLERR:
            print("[Socket] zmq.POLLERR")
        self.read_noti.setEnabled(True)

    def init_zmq_publish(self):
        if import_ZMQ():
            try:
                self._zmq_pub_context = zmq.Context()
                self._zmq_pub_socket = self._zmq_pub_context.socket(zmq.PUB)
                self._zmq_pub_socket.bind(self._zmq_pub_socket_address)
            except Exception as e:
                LOG.error('zmq publish message setup error: {}'.format(e))

    def zmq_write_message(self, args,topic = 'QtVCP'):
        if self.add_send_zmq:
            try:
                message = json.dumps(args)
                LOG.debug('Sending ZMQ Message:{} {}'.format(topic,message))
                self._zmq_pub_socket.send_multipart(
                    [bytes(topic.encode('utf-8')),
                        bytes((message).encode('utf-8'))])
            except Exception as e:
                LOG.error('zmq message sending error: {}'.format(e))
        else:
            LOG.info('ZMQ Message not enabled. message:{} {}'.format(topic,args))

    def _override_limits(self, n, signal_text):
        if not STATUS.is_limits_override_set():
            ACTION.TOGGLE_LIMITS_OVERRIDE()
        ACTION.SET_MACHINE_STATE(True)

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

    def set_max_messages(self, data):
        self.notify_max_msgs = data
    def get_max_messages(self):
        return self.notify_max_msgs
    def reset_max_messages(self):
        self.notify_max_msgs = 10

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
    def resetColor(self):
        self._close_color = QtGui.QColor(100, 0, 0, 150)

    def set_send_zmg(self, data):
        self.add_send_zmq = data
    def get_send_zmg(self):
        return self.add_send_zmq

    def set_receive_zmg(self, data):
        self.add_receive_zmq = data
    def get_receive_zmg(self):
        return self.add_receive_zmq

    # designer will show these properties in this order:
    notify_option = QtCore.pyqtProperty(bool, get_notify, set_notify, reset_notify)
    notify_max_messages = QtCore.pyqtProperty(int, get_max_messages, set_max_messages, reset_max_messages)

    catch_close_option = QtCore.pyqtProperty(bool, get_close, set_close, reset_close)
    close_overlay_color = QtCore.pyqtProperty(QtGui.QColor, getColor, setColor, resetColor)

    catch_errors_option = QtCore.pyqtProperty(bool, get_errors, set_errors, reset_errors)
    play_sounds_option = QtCore.pyqtProperty(bool, get_play_sounds, set_play_sounds, reset_play_sounds)

    use_pref_file_option = QtCore.pyqtProperty(bool, get_use_pref_file, set_use_pref_file, reset_use_pref_file)
    pref_filename_string = QtCore.pyqtProperty(str, get_pref_filename, set_pref_filename, reset_pref_filename)

    use_send_zmq_option = QtCore.pyqtProperty(bool, get_send_zmg, set_send_zmg)
    use_receive_zmq_option = QtCore.pyqtProperty(bool, get_receive_zmg, set_receive_zmg)
    # Embeddable program info ##########################

    def set_embed_prgm(self, data):
        self.process_tabs = data
    def get_embed_prgm(self):
        return self.process_tabs
    def reset_embed_prgm(self):
        self.process_tabs = True
    embedded_program_option = QtCore.pyqtProperty(bool, get_embed_prgm, set_embed_prgm, reset_embed_prgm)
    def set_default_tab(self, data):
        self._default_tab_name = data
    def get_default_tab(self):
        return self._default_tab_name
    default_embed_tab = QtCore.pyqtProperty(str, get_default_tab, set_default_tab)

    # Focus overlay ####################################

    def set_focusOverlay(self, data):
        self.add_focus_overlay = data
    def get_focusOverlay(self):
        return self.add_focus_overlay
    def reset_focusOverlay(self):
        self.add_focus_overlay = False
    focusOverlay_option = QtCore.pyqtProperty(bool, get_focusOverlay, set_focusOverlay, reset_focusOverlay)

    # Dialogs ##########################################

    def set_messageDialog(self, data):
        self.add_message_dialog = data
    def get_messageDialog(self):
        return self.add_message_dialog
    def reset_messageDialog(self):
        self.add_message_dialog = False
    messageDialog_option = QtCore.pyqtProperty(bool, get_messageDialog, set_messageDialog, reset_messageDialog)
    def get_messageDialogColor(self):
        return self._messageDialogColor
    def set_messageDialogColor(self, value):
        self._messageDialogColor = value
    message_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_messageDialogColor, set_messageDialogColor)

    def set_closeDialog(self, data):
        self.add_close_dialog = data
    def get_closeDialog(self):
        return self.add_close_dialog
    def reset_closeDialog(self):
        self.add_close_dialog = False
    closeDialog_option = QtCore.pyqtProperty(bool, get_closeDialog, set_closeDialog, reset_closeDialog)

    def set_entryDialog(self, data):
        self.add_entry_dialog = data
    def get_entryDialog(self):
        return self.add_entry_dialog
    def reset_entryDialog(self):
        self.add_entry_dialog = False
    entryDialog_option = QtCore.pyqtProperty(bool, get_entryDialog, set_entryDialog, reset_entryDialog)
    def set_entryDialogSoftkey(self, data):
        self._entryDialogSoftkey = data
    def get_entryDialogSoftkey(self):
        return self._entryDialogSoftkey
    def reset_entryDialogSoftkey(self):
        self._entryDialogSoftkey = False
    entryDialogSoftkey_option = QtCore.pyqtProperty(bool, get_entryDialogSoftkey, set_entryDialogSoftkey, reset_entryDialogSoftkey)
    def get_entryDialogColor(self):
        return self._entryDialogColor
    def set_entryDialogColor(self, value):
        self._entryDialogColor = value
    entry_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_entryDialogColor, set_entryDialogColor)

    def set_toolDialog(self, data):
        self.add_tool_dialog = data
    def get_toolDialog(self):
        return self.add_tool_dialog
    def reset_toolDialog(self):
        self.add_tool_dialog = False
    toolDialog_option = QtCore.pyqtProperty(bool, get_toolDialog, set_toolDialog, reset_toolDialog)
    def get_toolDialogColor(self):
        return self._toolDialogColor
    def set_toolDialogColor(self, value):
        self._toolDialogColor = value
    tool_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_toolDialogColor, set_toolDialogColor)
    def setUseDesktopNotify(self, value):
        self._toolUseDesktopNotify = value
    def getUseDesktopNotify(self):
        return self._toolUseDesktopNotify
    def resetUseDesktopNotify(self):
        self._toolUseDesktopNotify = False
    ToolUseDesktopNotify = QtCore.pyqtProperty(bool, getUseDesktopNotify, setUseDesktopNotify, resetUseDesktopNotify)
    def setFrameless(self, value):
        self._toolFrameless = value
    def getFrameless(self):
        return self._toolFrameless
    def resetFrameless(self):
        self._toolFrameless = False
    ToolFrameless = QtCore.pyqtProperty(bool, getFrameless, setFrameless, resetFrameless)

    def set_fileDialog(self, data):
        self.add_file_dialog = data
    def get_fileDialog(self):
        return self.add_file_dialog
    def reset_fileDialog(self):
        self.add_file_dialog = False
    fileDialog_option = QtCore.pyqtProperty(bool, get_fileDialog, set_fileDialog, reset_fileDialog)
    def get_fileDialogColor(self):
        return self._fileDialogColor
    def set_fileDialogColor(self, value):
        self._fileDialogColor = value
    file_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_fileDialogColor, set_fileDialogColor)

    def set_keyboardDialog(self, data):
        self.add_keyboard_dialog = data
    def get_keyboardDialog(self):
        return self.add_keyboard_dialog
    def reset_keyboardDialog(self):
        self.add_keyboard_dialog = False
    keyboardDialog_option = QtCore.pyqtProperty(bool, get_keyboardDialog, set_keyboardDialog, reset_keyboardDialog)
    def get_keyboardDialogColor(self):
        return self._keyboardDialogColor
    def set_keyboardDialogColor(self, value):
        self._keyboardDialogColor = value
    keyboard_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_keyboardDialogColor, set_keyboardDialogColor)

    def set_versaProbeDialog(self, data):
        self.add_versaprobe_dialog = data
    def get_versaProbeDialog(self):
        return self.add_versaprobe_dialog
    def reset_versaProbeDialog(self):
        self.add_versaprobe_dialog = False
    versaProbeDialog_option = QtCore.pyqtProperty(bool, get_versaProbeDialog, set_versaProbeDialog, reset_versaProbeDialog)
    def get_versaProbeDialogColor(self):
        return self._versaProbeDialogColor
    def set_versaProbeDialogColor(self, value):
        self._versaProbeDialogColor = value
    versaProbe_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_versaProbeDialogColor, set_versaProbeDialogColor)

    def set_macroTabDialog(self, data):
        self.add_macrotab_dialog = data
    def get_macroTabDialog(self):
        return self.add_macrotab_dialog
    def reset_macroTabDialog(self):
        self.add_macrotab_dialog = False
    macroTabDialog_option = QtCore.pyqtProperty(bool, get_macroTabDialog, set_macroTabDialog, reset_macroTabDialog)
    def get_macroTabDialogColor(self):
        return self._macroTabDialogColor
    def set_macroTabDialogColor(self, value):
        self._macroTabDialogColor = value
    macroTab_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_macroTabDialogColor, set_macroTabDialogColor)

    def set_camViewDialog(self, data):
        self.add_camview_dialog = data
    def get_camViewDialog(self):
        return self.add_camview_dialog
    def reset_camViewDialog(self):
        self.add_camview_dialog = False
    camViewDialog_option = QtCore.pyqtProperty(bool, get_camViewDialog, set_camViewDialog, reset_camViewDialog)
    def get_camViewDialogColor(self):
        return self._camViewDialogColor
    def set_camViewDialogColor(self, value):
        self._camViewDialogColor = value
    camView_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_camViewDialogColor, set_camViewDialogColor)

    def set_toolOffsetDialog(self, data):
        self.add_tooloffset_dialog = data
    def get_toolOffsetDialog(self):
        return self.add_tooloffset_dialog
    def reset_toolOffsetDialog(self):
        self.add_tooloffset_dialog = False
    toolOffsetDialog_option = QtCore.pyqtProperty(bool, get_toolOffsetDialog, set_toolOffsetDialog, reset_toolOffsetDialog)
    def get_toolOffsetDialogColor(self):
        return self._toolOffsetDialogColor
    def set_toolOffsetDialogColor(self, value):
        self._toolOffsetDialogColor = value
    toolOffset_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_toolOffsetDialogColor, set_toolOffsetDialogColor)

    def set_originOffsetDialog(self, data):
        self.add_originoffset_dialog = data
    def get_originOffsetDialog(self):
        return self.add_originoffset_dialog
    def reset_originOffsetDialog(self):
        self.add_originoffset_dialog = False
    originOffsetDialog_option = QtCore.pyqtProperty(bool, get_originOffsetDialog, set_originOffsetDialog, reset_originOffsetDialog)
    def get_originOffsetDialogColor(self):
        return self._originOffsetDialogColor
    def set_originOffsetDialogColor(self, value):
        self._originOffsetDialogColor = value
    originOffset_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_originOffsetDialogColor, set_originOffsetDialogColor)

    def set_calculatorDialog(self, data):
        self.add_calculator_dialog = data
    def get_calculatorDialog(self):
        return self.add_calculator_dialog
    def reset_calculatorDialog(self):
        self.add_calculator_dialog = False
    calculatorDialog_option = QtCore.pyqtProperty(bool, get_calculatorDialog, set_calculatorDialog, reset_calculatorDialog)
    def get_calculatorDialogColor(self):
        return self._calculatorDialogColor
    def set_calculatorDialogColor(self, value):
        self._calculatorDialogColor = value
    calculator_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_calculatorDialogColor, set_calculatorDialogColor)

    def set_machineLogDialog(self, data):
        self.add_machinelog_dialog = data
    def get_machineLogDialog(self):
        return self.add_machinelog_dialog
    def reset_machineLogDialog(self):
        self.add_machinelog_dialog = False
    machineLogDialog_option = QtCore.pyqtProperty(bool, get_machineLogDialog, set_machineLogDialog, reset_machineLogDialog)
    def get_machineLogDialogColor(self):
        return self._machineLogDialogColor
    def set_machineLogDialogColor(self, value):
        self._machineLogDialogColor = value
    machineLog_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_machineLogDialogColor, set_machineLogDialogColor)

    def set_runFromLineDialog(self, data):
        self.add_runFromLine_dialog = data
    def get_runFromLineDialog(self):
        return self.add_runFromLine_dialog
    def reset_runFromLineDialog(self):
        self.add_runFromLine_dialog = False
    runFromLineDialog_option = QtCore.pyqtProperty(bool, get_runFromLineDialog, set_runFromLineDialog, reset_runFromLineDialog)
    def get_runFromLineDialogColor(self):
        return self._runFromLineDialogColor
    def set_runFromLineDialogColor(self, value):
        self._runFromLineDialogColor = value
    runFromLine_overlay_color = QtCore.pyqtProperty(QtGui.QColor, get_runFromLineDialogColor, set_runFromLineDialogColor)

    def getHalCompName(self):
        return self._halBaseName
    def setHalCompName(self, value):
        self._halBaseName = value
    def resetHalCompName(self):
        self._halBaseName = ''
    halCompBaseName = QtCore.pyqtProperty(str, getHalCompName, setHalCompName, resetHalCompName)

    ##############################
    # required boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
