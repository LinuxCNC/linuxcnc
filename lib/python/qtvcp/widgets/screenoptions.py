# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

from PyQt5 import QtCore, QtWidgets, QtGui
import linuxcnc
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.lib.message import Message
from qtvcp.lib.notify import Notify
from qtvcp.lib.audio_player import Player
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

    def _hal_init(self):
        self.w = self.QTVCP_INSTANCE_
        # connect to GStat to catch linuxcnc events
        if self.catch_errors:
            GSTAT.connect('periodic', self.on_periodic)
        if self.close_event:
            self.w.closeEvent = self.closeEvent
        if self.play_sounds:
            try:
                SOUND._register_messages()
            except:
                self.play_sounds = False
                log.warning('Sound Option turned off')

    def on_periodic(self,w):
        e = self.error.poll()
        if e:
            kind, text = e
            if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                if self.desktop_notify:
                    if self.play_sounds:
                        GSTAT.emit('play-alert','ERROR')
                    NOTE.notify('ERROR',text,None,4)
            elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
               if self.desktop_notify:
                    if self.play_sounds:
                        GSTAT.emit('play-alert','LOGIN')
                    NOTE.notify('OP MESSAGE',text,None,4)
            elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
               if self.desktop_notify:
                    if self.play_sounds:
                        GSTAT.emit('play-alert','ERROR')
                    NOTE.notify('DISPLAY',text,None,4)

    def closeEvent(self, event):
        if self.close_event:
            sound = None
            if self.play_sounds:
                sound = 'ATTENTION'
            answer = self.w.lcnc_dialog.showdialog('Do you want to shutdown now?',
                None, details='You can set a preference to not see this message',
                icon=MSG.CRITICAL, display_type=MSG.YN_TYPE,focus_text='ARE YOU SURE',
                 focus_color=QtGui.QColor(100, 0, 0,150), play_alert = sound )
            if not answer:
                event.ignore()
                return
            if self.play_sounds:
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

    # designer will show these properties in this order:
    notify_option = QtCore.pyqtProperty(bool, get_notify, set_notify, reset_notify)
    catch_close_option = QtCore.pyqtProperty(bool, get_close, set_close, reset_close)
    catch_errors_option = QtCore.pyqtProperty(bool, get_errors, set_errors, reset_errors)
    play_sounds_option = QtCore.pyqtProperty(bool, get_play_sounds, set_play_sounds, reset_play_sounds)
