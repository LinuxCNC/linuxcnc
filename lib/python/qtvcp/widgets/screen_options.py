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
from qtvcp.lib.message import Message
from qtvcp.lib.notify import Notify
from qtvcp.lib.audio_player import Player
from qtvcp.lib.preferences import Access
from qtvcp.lib.machine_log import MachineLogger
from qtvcp.core import Status, Info, Tool
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
        self.notify_start_title = 'Welcome'
        self.notify_start_detail = ''
        self.notify_start_timeout = 5
        self.shutdown_msg_title = 'Do you want to Shutdown now?'
        self.shutdown_msg_detail = ''
        self.user_messages = True
        self.use_pref_file = True
        self.process_tabs = True
        self.add_message_dialog = True
        self.add_entry_dialog = False
        self.add_tool_dialog = False
        self.add_file_dialog = False
        self.add_focus_overlay = False
        self.add_versaprobe_dialog = False
        self.add_macrotab_dialog = False
        self.add_camview_dialog = False
        self.add_originoffset_dialog = False
        self.add_tooloffset_dialog = False
        self.add_calculator_dialog = False

        self.pref_filename = '~/.qtvcp_screen_preferences'
        self._default_tab_name = ''
        self._close_color = QtGui.QColor(100, 0, 0, 150)
        self._messageDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._entryDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._toolDialogColor = QtGui.QColor(100, 0, 0, 150)
        self._fileDialogColor = QtGui.QColor(0, 0, 100, 150)
        self._versaProbeDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._macroTabDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._camViewDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._originOffsetDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._toolOffsetDialogColor = QtGui.QColor(0, 0, 0, 150)
        self._calculatorDialogColor = QtGui.QColor(0, 0, 0, 150)

    # self.QTVCP_INSTANCE_
    # self.HAL_GCOMP_
    # self.PREFS_
    # come from base class
    def _hal_init(self):
        if self.add_message_dialog:
            self.init_message_dialog()

        if self.add_entry_dialog:
            self.init_entry_dialog()

        if self.add_tool_dialog:
            self.init_tool_dialog()

        if self.add_file_dialog:
            self.init_file_dialog()

        if self.add_focus_overlay:
            self.init_focus_overlay()

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
        except Exception as e:
            LOG.info('Exception adding status to notify:', exc_info=e)

        if self.desktop_notify:
            NOTICE.notify(self.notify_start_title, self.notify_start_detail, None,
                        self.notify_start_timeout, self. notify_start_timeout)
            self.desktop_dialog = NOTICE.new_critical(None)

        # add any XEmbed tabs
        if self.process_tabs:
            self.add_xembed_tabs()

        # clear and add an intial machine log message
        STATUS.emit('update-machine-log', '', 'DELETE')
        STATUS.emit('update-machine-log', '', 'INITIAL')
        STATUS.connect('tool-info-changed', lambda w, data: self._tool_file_info(data, TOOL.COMMENTS))

    # This is called early by qt_makegui.py for access to
    # be able to pass the preference object to ther widgets
    def _pref_init(self):
        if self.use_pref_file:
            if INFO.PREFERENCE_PATH:
                self.pref_filename = INFO.PREFERENCE_PATH
                LOG.debug('Switching to Preference File Path from INI: {}'.format(INFO.PREFERENCE_PATH))
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
                    NOTICE.update(self.desktop_dialog, title='ERROR:', message=text)
            elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
                if self.desktop_notify:
                    NOTICE.update(self.desktop_dialog, title='OPERATOR TEXT:', message=text)
            elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
                if self.desktop_notify:
                    NOTICE.update(self.desktop_dialog, title='OPERATOR DISPLAY:', message=text)
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
            answer = self.QTVCP_INSTANCE_.messageDialog_.showdialog(self.shutdown_msg_title,
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
        try:
            toolnum = tool_entry[0]
            tool_table_line = TOOL.GET_TOOL_INFO(toolnum)
            text = 'Tool %s: %s'%(str(tool_table_line[0]),str(tool_table_line[index]))
            STATUS.emit('update-machine-log', text, 'TIME')
        except:
            pass

    # XEmbed program into tabs
    def add_xembed_tabs(self):
        if INFO.TAB_CMDS:
            from qtvcp.widgets.xembed import XEmbeddable
            for name, tab, cmd in INFO.ZIPPED_TABS:
                LOG.debug('Processing Embedded tab:{}, {}, {}'.format(name,tab,cmd))
                if tab == 'default':
                    tab = 'rightTab'
                if not isinstance(self.QTVCP_INSTANCE_[tab], QtWidgets.QTabWidget):
                    LOG.warning('tab location {} is not a QTabWidget - skipping'.format(tab))
                    continue
                try:
                    temp = XEmbeddable()
                    temp.embed(cmd)
                except Exception, e:
                    LOG.error('Embedded tab loading failed: {}'.format(name))
                else:
                    try:
                        self.QTVCP_INSTANCE_[tab].addTab(temp, name)
                    except Exception, e:
                        LOG.error('Embedded tab adding widget into {} failed.'.format(tab))

    def init_tool_dialog(self):
        from qtvcp.widgets.dialog_widget import ToolDialog
        w = self.QTVCP_INSTANCE_
        w.toolDialog_ = ToolDialog()
        w.toolDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.toolDialog_, w, w.PATHS, self.PREFS_)
        w.toolDialog_.overlay_color = self._toolDialogColor

    def init_message_dialog(self):
        from qtvcp.widgets.dialog_widget import LcncDialog
        w = self.QTVCP_INSTANCE_
        w.messageDialog_ = LcncDialog()
        w.messageDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.messageDialog_, w, w.PATHS, self.PREFS_)
        w.messageDialog_.overlay_color = self._messageDialogColor

    def init_entry_dialog(self):
        from qtvcp.widgets.dialog_widget import EntryDialog
        w = self.QTVCP_INSTANCE_
        w.entryDialog_ = EntryDialog()
        w.entryDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.entryDialog_, w, w.PATHS, self.PREFS_)
        w.entryDialog_.overlay_color = self._entryDialogColor

    def init_file_dialog(self):
        from qtvcp.widgets.dialog_widget import FileDialog
        w = self.QTVCP_INSTANCE_
        w.fileDialog_ = FileDialog()
        w.fileDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.fileDialog_, w, w.PATHS, self.PREFS_)
        w.fileDialog_.overlay_color = self._fileDialogColor

    def init_focus_overlay(self):
        from qtvcp.widgets.overlay_widget import FocusOverlay
        w = self.QTVCP_INSTANCE_
        w.focusOverlay_ = FocusOverlay(w)
        w.focusOverlay_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.focusOverlay_, w, w.PATHS, self.PREFS_)

    def init_versaprobe_dialog(self):
        from qtvcp.widgets.dialog_widget import VersaProbeDialog
        w = self.QTVCP_INSTANCE_
        w.versaProbeDialog_ = VersaProbeDialog()
        w.versaProbeDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.versaProbeDialog_, w, w.PATHS, self.PREFS_)
        w.versaProbeDialog_.overlay_color = self._versaProbeDialogColor

    def init_macrotab_dialog(self):
        from qtvcp.widgets.dialog_widget import MacroTabDialog
        w = self.QTVCP_INSTANCE_
        w.macroTabDialog_ = MacroTabDialog()
        w.macroTabDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.macroTabDialog_, w, w.PATHS, self.PREFS_)
        w.macroTabDialog_.overlay_color = self._macroTabDialogColor

    def init_camview_dialog(self):
        from qtvcp.widgets.dialog_widget import CamViewDialog
        w = self.QTVCP_INSTANCE_
        w.camViewDialog_ = CamViewDialog()
        w.camViewDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.camViewDialog_, w, w.PATHS, self.PREFS_)
        w.camViewDialog_.overlay_color = self._camViewDialogColor

    def init_tooloffset_dialog(self):
        from qtvcp.widgets.dialog_widget import ToolOffsetDialog
        w = self.QTVCP_INSTANCE_
        w.toolOffsetDialog_ = ToolOffsetDialog()
        w.toolOffsetDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.toolOffsetDialog_, w, w.PATHS, self.PREFS_)
        w.toolOffsetDialog_.overlay_color = self._toolOffsetDialogColor

    def init_originoffset_dialog(self):
        from qtvcp.widgets.dialog_widget import OriginOffsetDialog
        w = self.QTVCP_INSTANCE_
        w.originOffsetDialog_ = OriginOffsetDialog()
        w.originOffsetDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.originOffsetDialog_, w, w.PATHS, self.PREFS_)
        w.originOffsetDialog_.overlay_color = self._originOffsetDialogColor

    def init_calculator_dialog(self):
        from qtvcp.widgets.dialog_widget import CalculatorDialog
        w = self.QTVCP_INSTANCE_
        w.calculatorDialog_ = CalculatorDialog()
        w.calculatorDialog_.hal_init(self.HAL_GCOMP_, self.HAL_NAME_,
             w.calculatorDialog_, w, w.PATHS, self.PREFS_)
        w.calculatorDialog_.overlay_color = self._calculatorDialogColor

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
    close_overlay_color = QtCore.pyqtProperty(QtGui.QColor, getColor, setColor)

    catch_errors_option = QtCore.pyqtProperty(bool, get_errors, set_errors, reset_errors)
    play_sounds_option = QtCore.pyqtProperty(bool, get_play_sounds, set_play_sounds, reset_play_sounds)

    use_pref_file_option = QtCore.pyqtProperty(bool, get_use_pref_file, set_use_pref_file, reset_use_pref_file)
    pref_filename_string = QtCore.pyqtProperty(str, get_pref_filename, set_pref_filename, reset_pref_filename)

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

    def set_entryDialog(self, data):
        self.add_entry_dialog = data
    def get_entryDialog(self):
        return self.add_entry_dialog
    def reset_entryDialog(self):
        self.add_entry_dialog = False
    entryDialog_option = QtCore.pyqtProperty(bool, get_entryDialog, set_entryDialog, reset_entryDialog)
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

    ##############################
    # required boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
