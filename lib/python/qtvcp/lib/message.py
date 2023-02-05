from PyQt5.QtWidgets import *
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QColor
from qtvcp.core import Status, Info
from qtvcp.widgets.dialog_widget import LcncDialog
import hal

# Set up logging
from qtvcp import logger

log = logger.getLogger(__name__)

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
STATUS = Status()
INFO = Info()


class Message:
    def __init__(self):
        self.QUESTION = QMessageBox.Question
        self.INFO = QMessageBox.Information
        self.WARNING = QMessageBox.Warning
        self.CRITICAL = QMessageBox.Critical
        self._color = QColor(0, 0, 0, 150)
        self.focus_text = ' '
        self.play_sounds = True
        self.alert_sound = 'ATTENTION'
        self.use_focus_overlay = True

    # search for and set up user requested message system.
    # status displays on the statusbar and requires no acknowledge.
    # dialog displays a  Messagebox with yes or no buttons
    # okdialog displays a Messagebox with an ok button
    # dialogs require an answer before focus is sent back to main screen
    def message_setup(self, hal_comp, window=None):
        self.HAL_GCOMP_ = hal_comp
        if INFO.ZIPPED_USRMESS:
            for boldtext, text, pinname, details, name, icon in (INFO.ZIPPED_USRMESS):
                if not ("status" in details)\
                and not ("dialog" in details):
                    log.info('invalid message type {} in INI File [DISPLAY] section'.format(details))
                    continue

                if icon.upper() == 'CRITICAL': icon = QMessageBox.Critical
                elif icon.upper() == 'INFO': icon = QMessageBox.Information
                elif icon.upper() == 'WARNING': icon = QMessageBox.Warning
                else: icon = QMessageBox.Question

                if not name == None:
                    D = self['dialog-{}'.format(name)] = LcncDialog(window)
                    D._request_name = 'INI_MESSAGE_{}'.format(name)
                    D.hal_init(HAL_NAME=name)
                    D.pinname = name

                    # this is how we make a pin that can be connected to a callback
                    if ("none" in details):
                        D._halpin = self.HAL_GCOMP_.newpin(name, hal.HAL_BIT, hal.HAL_IN)
                    else:
                        D._halpin = self.HAL_GCOMP_.newpin(name, hal.HAL_BIT, hal.HAL_IO)

                    D._halpin.value_changed.connect(self.dummy(self['dialog-{}'.format(name)],
                                                D._halpin, name, boldtext, text, pinname, details, icon))

                    if ("dialog" in details) and not ("nonedialog" in details):
                        self.HAL_GCOMP_.newpin(name + "-waiting", hal.HAL_BIT, hal.HAL_OUT)
                        if not ("ok" in details):
                            self.HAL_GCOMP_.newpin(name + "-response", hal.HAL_BIT, hal.HAL_OUT)
                            self.HAL_GCOMP_.newpin(name + "-response-s32", hal.HAL_S32, hal.HAL_OUT)
                            self.HAL_GCOMP_[name + "-response-s32"] = -1 # undetermined

    # This weird code is so we can get access to proper variables.
    # using clicked.connect( self.on_printmessage(pin,name,bt,t,c) ) apparently doesn't easily
    # add user data - it seems you only get the last set added
    # found this closure technique hack on the web
    # truly weird black magic
    def dummy(self, dialog, pin, pinname, boldtext, text, details, type, icon):
        def calluser():
            self.on_printmessage(dialog, pin, pinname, boldtext, text, details, type, icon)
        return calluser

    # This is part of the user message system
    # There is status that prints to the status bar
    # There is Okdialog that prints a dialog that the user must acknowledge
    # there is yes/no dialog where the user must choose between yes or no
    # you can combine status and dialog messages so they print to the status bar
    # and pop a dialog
    # This gets called as the HAL pin changes state
    def on_printmessage(self, dialog, pin, pinname, boldtext, text, details, type, icon):

        # hide a no button dialog if pin goes False
        if not pin.get():
            if dialog.style == LcncDialog.NONE:
                dialog.record_geometry()
                dialog.hide()
            return

        if self.play_sounds:
            STATUS.emit('play-sound', self.alert_sound)
        if boldtext == "NONE": boldtext = ''
        if "status" in type:
            if boldtext:
                statustext = boldtext
            else:
                statustext = text
            if self.NOTIFY:
                self.NOTIFY.notify("INFO:", statustext)
        if "dialog" in type or "okdialog" in type:
            if self.use_focus_overlay:
                STATUS.emit('focus-overlay-changed', True, self.focus_text, self._color)
            if "nonedialog" in type: style = LcncDialog.NONE
            elif "okdialog" in type: style = LcncDialog.OK
            elif "yesnodialog" in type: style = LcncDialog.YESNO
            elif "okcancel" in type: style = LcncDialog.OKCANCEL
            elif "closepromt" in type: style = LcncDialog.CLOSEPROMPT

            dialog.style = style
            if style != LcncDialog.NONE:
                self.HAL_GCOMP_[pinname + "-waiting"] = True
                if style != LcncDialog.OK:
                    if pin.get():
                        self.HAL_GCOMP_[pinname + "-response"] = False
                        self.HAL_GCOMP_[pinname + "-response-s32"] = -1
            self.showDialog(dialog, boldtext, text, details, style, icon, pinname)

    def showDialog(self, dialog, message, more_info=None, details=None, display_type='OK',
                   icon=QMessageBox.Information, pinname=None, focus_text=None,
                           focus_color=None, play_alert=None, nblock= False,
                           return_callback = None, flags = None, setflags = None,
                            title = None):

        if return_callback == None:
            return_callback = self.dialog_return
        flags =  (Qt.Tool | Qt.Dialog | Qt.WindowStaysOnTopHint\
                | Qt.WindowSystemMenuHint | Qt.CustomizeWindowHint)
        title = 'User Message'
        setFlags = {Qt.WindowCloseButtonHint: False}
        dialog.showdialog( message, more_info=more_info, details=details, display_type=display_type,
                           icon=icon, pinname=pinname, focus_text=None,
                           focus_color=None, play_alert=None, nblock= nblock,
                           return_callback = return_callback, flags = flags, setflags = setFlags,
                            title = title)

    # for testing, could be overridden
    def msgbtn(self, i):
        pass
        # print "Button pressed is:",i.text()

    # a hacky way to adjust future options
    # using it to adjust runtime options from a preference file in screenoptions (presently)
    # 'with great power comes ....'
    def message_option(self, option, data):
        try:
            self[option] = data
        except:
            pass

    # message dialog returns a response here
    # update any hand shaking pins
    # this is called by LcncDialog using callback mechanism
    def dialog_return(self, dialog, result):
        if not dialog.style == LcncDialog.NONE:
            if dialog.pinname:
                self.HAL_GCOMP_[dialog.pinname + "-waiting"] = False
            if not dialog.style == LcncDialog.OK:
                self.HAL_GCOMP_[dialog.pinname + "-response"] = result
                if result:
                    num = 1
                else:
                    num = 0
                self.HAL_GCOMP_[dialog.pinname + "-response-s32"] = num
            # reset the HAL IO pin so it can fire again
            self.HAL_GCOMP_[dialog.pinname] = False
        if self.use_focus_overlay:
            STATUS.emit('focus-overlay-changed', False, None, None)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


if __name__ == '__main__':
    import sys
    from PyQt5.QtCore import *

    def callreturn(dialog, btn, pinname):
        result = dialog.qualifiedReturn(btn)
        print(dialog, ' = ', result)

    m = Message()
    app = QApplication(sys.argv)
    w = QWidget()

    dialogb = LcncDialog()
    b = QPushButton(w)
    b.setText("Show Y/N\n message!")
    b.move(10, 0)
    b.clicked.connect(lambda data: m.showDialog(dialogb, 'This is a question message',
                                                more_info='Pick yes or no', icon=m.QUESTION,
                                                 display_type=LcncDialog.YESNO,
                                                return_callback = callreturn))
    dialogc = LcncDialog()
    c = QPushButton(w)
    c.setText("Show OK\n message!")
    c.move(10, 40)
    c.clicked.connect(lambda data: m.showDialog(dialogc, 'This is an OK message',
                             display_type=LcncDialog.OK, return_callback = callreturn))

    dialogd = LcncDialog()
    d = QPushButton(w)
    d.setText("Show warning\n message!")
    d.move(10, 80)
    d.clicked.connect(lambda data: m.showDialog(dialogd,'This is a Warning message',
                      details='There seems to be something wrong\n feel free to ignore', icon=m.WARNING,
                      display_type=LcncDialog.OK, return_callback = callreturn))

    dialoge = LcncDialog()
    e = QPushButton(w)
    e.setText("Show critical\n persistent message!")
    e.move(10, 120)
    e.clicked.connect(lambda data: m.showDialog(dialoge, 'This is a Critical persistent message',
                      details='There seems to be something wrong\n You must fix it to clear message', icon=m.CRITICAL,
                      display_type=LcncDialog.NONE,return_callback = callreturn, nblock=True))

    w.setWindowTitle("PyQt Dialog demo")
    w.setGeometry(300, 300, 300, 150)
    w.show()
    sys.exit(app.exec_())
