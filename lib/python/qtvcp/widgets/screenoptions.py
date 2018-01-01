# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

from PyQt5 import QtCore, QtWidgets, QtGui
import linuxcnc
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.lib.message import Message
from qtvcp.lib.notify import Notify
from qtvcp.lib.audio_player import Player
from qtvcp.lib.preferences import Access
from qtvcp.qt_glib import GStat

GSTAT = GStat()
NOTE = Notify()
MSG = Message()

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
        self.mchnMsg_sound_doPlay = True
        self.usrMsg_sound_doPlay = True
        self.usrMsg_sound_type = 'READY'
        self.usrMsg_doFocusOverlay = True
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
            self.play_sounds = self.PREFS_.getpref('sound_option_on', True, bool,'SCREEN_OPTIONS')
            self.mchnMsg_sound_doPlay = self.PREFS_.getpref('sound_on_machine_messages', True, bool,'SCREEN_OPTIONS')
            self.usrMsg_sound_doPlay = self.PREFS_.getpref('sound_on_user_messages', True, bool,'SCREEN_OPTIONS')
            self.usrMsg_sound_type = self.PREFS_.getpref('userMsg_alert_sound', 'RING', str,'SCREEN_OPTIONS')
            self.usrMsg_doFocusOverlay = self.PREFS_.getpref('focusOverlay_on_user_messages', True, bool,'SCREEN_OPTIONS')
            self.play_shutdown_sounds = self.PREFS_.getpref('sound_on_shutdown', True, bool,'SCREEN_OPTIONS')

        # connect to GStat to catch linuxcnc events
        if self.catch_errors:
            GSTAT.connect('periodic', self.on_periodic)
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
                MSG.message_option('play_sounds', self.usrMsg_sound_doPlay)
            else:
                MSG.message_option('play_sounds', False)
            MSG.message_option('alert_sound', self.usrMsg_sound_type)
            MSG.message_option('use_focus_overlay', self.usrMsg_doFocusOverlay)

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
                    NOTE.notify('ERROR',text,None,4)
            elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
               if self.desktop_notify:
                    NOTE.notify('OP MESSAGE',text,None,4)
            elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
               if self.desktop_notify:
                    NOTE.notify('DISPLAY',text,None,4)
            if self.play_sounds and self.mchnMsg_sound_doPlay:
                GSTAT.emit('play-alert','ERROR')

    def closeEvent(self, event):
        if self.close_event:
            sound = None
            if self.play_sounds and self.play_shutdown_sounds:
                sound = 'ATTENTION'
            answer = self.QTVCP_INSTANCE_.lcnc_dialog.showdialog('Do you want to shutdown now?',
                None, details='You can set a preference to not see this message',
                icon=MSG.CRITICAL, display_type=MSG.YN_TYPE,focus_text='ARE YOU SURE',
                 focus_color=QtGui.QColor(100, 0, 0,150), play_alert = sound )
            if not answer:
                event.ignore()
                return
            if self.play_sounds and self.play_shutdown_sounds:
                GSTAT.emit('play-alert','LOGOUT')
                import time
                time.sleep(2) 
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
