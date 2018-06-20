# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

import os
import time
from PyQt5 import QtCore, QtWidgets, QtGui
import linuxcnc
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.lib.message import Message
from qtvcp.lib.notify import Notify
from qtvcp.lib.audio_player import Player
from qtvcp.lib.preferences import Access
from qtvcp.core import Status, Info

STATUS = Status()
NOTE = Notify()
MSG = Message()
INFO = Info()

try:
    SOUND = Player()
except:
    log.warning('Sound Player did not load')

class Lcnc_ScreenOptions(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QWidget.__init__(self, parent)
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
        self.pref_filename = '~/.qtscreen_preferences'

    # self.QTVCP_INSTANCE_
    # self.HAL_GCOMP_
    # come from base class
    def _hal_init(self):
        # Read user preferences
        if self.PREFS_:
            self.catch_errors = self.PREFS_.getpref('catch_errors', True, bool,'SCREEN_OPTIONS')
            self.desktop_notify = self.PREFS_.getpref('desktop_notify', True, bool,'SCREEN_OPTIONS')
            self.close_event = self.PREFS_.getpref('shutdown_check', True, bool,'SCREEN_OPTIONS')
            self.play_sounds = self.PREFS_.getpref('sound_player_on', True, bool,'SCREEN_OPTIONS')
            self.mchnMsg_play_sound = self.PREFS_.getpref('mchnMsg_play_sound', True, bool,'MCH_MSG_OPTIONS')
            self.mchnMsg_speak_errors = self.PREFS_.getpref('mchnMsg_speak_errors', True, bool,'MCH_MSG_OPTIONS')
            self.mchnMsg_sound_type = self.PREFS_.getpref('mchnMsg_sound_type', 'ERROR', str,'MCH_MSG_OPTIONS')
            self.usrMsg_play_sound = self.PREFS_.getpref('usermsg_play_sound', True, bool,'USR_MSG_OPTIONS')
            self.usrMsg_sound_type = self.PREFS_.getpref('userMsg_sound_type', 'RING', str,'USR_MSG_OPTIONS')
            self.usrMsg_use_FocusOverlay = self.PREFS_.getpref('userMsg_use_focusOverlay', True, bool,'USR_MSG_OPTIONS')
            self.shutdown_play_sound = self.PREFS_.getpref('shutdown_play_sound', True, bool,'SHUTDOWN_OPTIONS')
            self.shutdown_alert_sound_type = self.PREFS_.getpref('shutdown_alert_sound_type', 'ATTENTION', str,'SHUTDOWN_OPTIONS')
            self.shutdown_exit_sound_type = self.PREFS_.getpref('shutdown_exit_sound_type', 'LOGOUT', str,'SHUTDOWN_OPTIONS')
            self.shutdown_msg_title = self.PREFS_.getpref('shutdown_msg_title', 'Do you want to Shutdown now?', str,'SHUTDOWN_OPTIONS')
            self.shutdown_msg_detail = self.PREFS_.getpref('shutdown_msg_detail', 'This option can be changed in the preference file', str,'SHUTDOWN_OPTIONS')
            self.notify_start_title = self.PREFS_.getpref('notify_start_title', 'Welcome', str,'NOTIFY_OPTIONS')
            self.notify_start_detail = self.PREFS_.getpref('notify_start_detail', 'This is a test screen for Qtscreen', str,'NOTIFY_OPTIONS')
            self.notify_start_timeout = self.PREFS_.getpref('notify_start_timeout', 10 , int,'NOTIFY_OPTIONS')
        # connect to STATUS to catch linuxcnc events
        if self.catch_errors:
            STATUS.connect('periodic', self.on_periodic)
        if self.close_event:
            self.QTVCP_INSTANCE_.closeEvent = self.closeEvent
        if self.play_sounds:
            try:
                SOUND._register_messages()
            except:
                self.play_sounds = False
                log.warning('Sound Option turned off')
        if self.user_messages:
            MSG.message_setup(self.HAL_GCOMP_)
            MSG.message_option('NOTIFY',NOTE)
            if self.play_sounds:
                MSG.message_option('play_sounds', self.usrMsg_play_sound)
            else:
                MSG.message_option('play_sounds', False)
            MSG.message_option('alert_sound', self.usrMsg_sound_type)
            MSG.message_option('use_focus_overlay', self.usrMsg_use_FocusOverlay)
        # If there is a widget named statusBar give a reference to desktop notify
        try:
            NOTE.statusbar = self.QTVCP_INSTANCE_.statusBar
        except Exception as e:
           log.info('Exception adding status to notify:', exc_info=e)
        if self.desktop_notify:
            NOTE.notify(self.notify_start_title,self.notify_start_detail,None,self.notify_start_timeout,self.notify_start_timeout)
        try:
            timestamp = time.strftime("%a, %b %d %Y %X ---\n")        
            fp = os.path.expanduser(INFO.MESSAGE_HISTORY_PATH)
            fp = open(fp, 'a')
            fp.write('\n--- Qtscreen loaded on: ' + timestamp)
            fp.close()
        except:
            log.warning('Message history: path valid?')

    # This is called early by qt_makegui.py for access to 
    # be able to pass the preference object to ther widgets
    def _pref_init(self):
        if self.use_pref_file:
            return Access(self.pref_filename)
        return None

    def on_periodic(self,w):
        e = self.error.poll()
        if e:
            kind, text = e
            if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                if self.desktop_notify:
                    NOTE.notify('ERROR',text,None,0,4)
            elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
               if self.desktop_notify:
                    NOTE.notify('OP MESSAGE',text,None,0,4)
            elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
               if self.desktop_notify:
                    NOTE.notify('DISPLAY',text,None,0,4)
            if self.play_sounds and self.mchnMsg_play_sound:
                STATUS.emit('play-alert','%s'% self.mchnMsg_sound_type)
                if self.mchnMsg_speak_errors:
                    STATUS.emit('play-alert','SPEAK %s '% text)
            try:
                timestamp = time.strftime("%a%d %H:%M ")
                fp = os.path.expanduser(INFO.MESSAGE_HISTORY_PATH)
                fp = open(fp, 'a')
                fp.write(timestamp + text + "\n")
                fp.close()
                STATUS.emit('reload-message-history')
            except:
                log.warning('Message history: path valid?: {}'.format(fp))

    def closeEvent(self, event):
        if self.close_event:
            sound = None
            if self.play_sounds and self.play_shutdown_sounds:
                sound = self.shutdown_alert_sound_type
            answer = self.QTVCP_INSTANCE_.lcnc_dialog.showdialog(self.shutdown_msg_title,
                None, details = self.shutdown_msg_detail,
                icon=MSG.CRITICAL, display_type=MSG.YN_TYPE,focus_text='ARE YOU SURE',
                 focus_color=QtGui.QColor(100, 0, 0,150), play_alert = sound )
            if not answer:
                event.ignore()
                return
            if self.play_sounds and self.play_shutdown_sounds:
                STATUS.emit('play-alert',self.shutdown_exit_sound_type)
                import time
                time.sleep(2)
                try:
                    self.QTVCP_INSTANCE_.handler_instance.closing_cleanup__()
                except:
                    pass
            event.accept()

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
        self.pref_filename = '~/.qtscreen_preferences'

    # designer will show these properties in this order:
    notify_option = QtCore.pyqtProperty(bool, get_notify, set_notify, reset_notify)
    catch_close_option = QtCore.pyqtProperty(bool, get_close, set_close, reset_close)
    catch_errors_option = QtCore.pyqtProperty(bool, get_errors, set_errors, reset_errors)
    play_sounds_option = QtCore.pyqtProperty(bool, get_play_sounds, set_play_sounds, reset_play_sounds)
    use_pref_file_option = QtCore.pyqtProperty(bool, get_use_pref_file, set_use_pref_file, reset_use_pref_file)
    pref_filename_string = QtCore.pyqtProperty(str, get_pref_filename, set_pref_filename, reset_pref_filename)
