from PyQt5.QtWidgets import *
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QColor
from qtvcp.core import Status, Info
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
        self.OK_TYPE = 1
        self.YN_TYPE = 0
        self.QUESTION = QMessageBox.Question
        self.INFO = QMessageBox.Information
        self.WARNING = QMessageBox.Warning
        self.CRITICAL = QMessageBox.Critical
        self._color = QColor(0, 0, 0, 150)
        self.focus_text =' '
        self.play_sounds = True
        self.alert_sound = 'READY'
        self.use_focus_overlay = True

    def showDialog(self, message, more_info=None, details=None, display_type=1,
                     icon=QMessageBox.Information, pinname=None):
        msg = QMessageBox()
        msg.setWindowModality(Qt.ApplicationModal)
        msg.setIcon(icon)
        msg.setWindowTitle("User MessageBox")
        msg.setTextFormat(Qt.RichText)
        msg.setText('<b>%s</b>'% message)
        if more_info:
            msg.setInformativeText(more_info)
        if details:
            msg.setDetailedText(details)
        if display_type == self.OK_TYPE:
            msg.setStandardButtons(QMessageBox.Ok)
        else:
            msg.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        msg.buttonClicked.connect(self.msgbtn)
        # block on answer
        retval = msg.exec_()
        log.debug('value of pressed message box button: {}'.format(retval))
        self.dialog_return(None,retval==QMessageBox.Yes,display_type,pinname)

    def msgbtn(self, i):
        pass
        #print "Button pressed is:",i.text()

    # This is part of the user message system
    # There is status that prints to the status bar
    # There is Okdialog that prints a dialog that the user must acknoledge
    # there is yes/no dialog where the user must choose between yes or no
    # you can combine status and dialog messages so they print to the status bar 
    # and pop a dialog
    def on_printmessage(self, pin, pinname, boldtext, text, details, type, icon):
        if not pin.get(): return
        if self.play_sounds:
            STATUS.emit('play-alert',self.alert_sound)
        if boldtext == "NONE": boldtext = ''
        if "status" in type:
            if boldtext:
                statustext = boldtext
            else:
                statustext = text
            if self.NOTIFY:
                self.NOTIFY.notify(_("INFO:"),statustext)
        if "dialog" in type or "okdialog" in type:
            if self.use_focus_overlay:
                STATUS.emit('focus-overlay-changed', True, self.focus_text, self._color)
            if pin.get():
                self.HAL_GCOMP_[pinname + "-waiting"] = True
            if "okdialog" in type:
                self.showDialog(boldtext,text,details,self.OK_TYPE,icon,pinname)
            else:
                if pin.get():
                    self.HAL_GCOMP_[pinname + "-response"] = 0
                self.showDialog(boldtext,text,details,self.YN_TYPE,icon,pinname)

    # search for and set up user requested message system.
    # status displays on the statusbat and requires no acknowledge.
    # dialog displays a  Messagebox with yes or no buttons
    # okdialog displays a Messagebox with an ok button
    # dialogs require an answer before focus is sent back to main screen
    def message_setup(self, hal_comp):
        self.HAL_GCOMP_ = hal_comp
        icon = QMessageBox.Question
        if INFO.ZIPPED_USRMESS:
            for bt,t,d,style,name in (INFO.ZIPPED_USRMESS):
                if not ("status" in style) and not ("dialog" in style) and not ("okdialog" in style):
                    log.debug('invalid message type {} in INI File [DISPLAY] section'.format(C))
                    continue
                if not name == None:
                    # this is how we make a pin that can be connected to a callback 
                    self[name] = self.HAL_GCOMP_.newpin(name, hal.HAL_BIT, hal.HAL_IO)
                    self[name].value_changed.connect(self.dummy(self[name],name,bt,t,d,style,icon))
                    if ("dialog" in style):
                        self.HAL_GCOMP_.newpin(name+"-waiting", hal.HAL_BIT, hal.HAL_OUT)
                        if not ("ok" in style):
                            self.HAL_GCOMP_.newpin(name+"-response", hal.HAL_BIT, hal.HAL_OUT)

    # a hacky way to adjust future options
    # using it to adjust runtime options from a preference file in screenoptions (presently)
    # 'with great power comes ....'
    def message_option(self, option, data):
        try:
            self[option] = data
        except:
            pass

    # This weird code is so we can get access to proper variables.
    # using clicked.connect( self.on_printmessage(pin,name,bt,t,c) ) apparently doesn't easily
    # add user data - it seems you only get the last set added
    # found this closure technique hack on the web
    # truely weird black magic
    def dummy(self,pin,name,bt,t,d,c,i):
        def calluser():
            self.on_printmessage(pin,name,bt,t,d,c,i)
        return calluser

    # message dialog returns a response here
    # update any hand shaking pins
    def dialog_return(self,widget,result,dialogtype,pinname):
        if not dialogtype: # yes/no dialog
            if pinname:
                self.HAL_GCOMP_[pinname + "-response"] = result
        if pinname:
            self.HAL_GCOMP_[pinname + "-waiting"] = False
        # reset the HAL IO pin so it can fire again
        self.HAL_GCOMP_[pinname] = False
        if self.use_focus_overlay:
            STATUS.emit('focus-overlay-changed',False,None,None)

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
    m = Message()
    app = QApplication(sys.argv)
    w = QWidget()
    b = QPushButton(w)
    b.setText("Show Y/N\n message!")
    b.move(10,0)
    b.clicked.connect(lambda data: m.showdialog('This is a question message',
         more_info='Pick yes or no', icon=m.QUESTION, display_type=m.YN_TYPE))

    c = QPushButton(w)
    c.setText("Show OK\n message!")
    c.move(10,40)
    c.clicked.connect(lambda data: m.showdialog('This is an OK message', display_type=1))

    d = QPushButton(w)
    d.setText("Show critical\n message!")
    d.move(10,80)
    d.clicked.connect(lambda data: m.showdialog('This is an Critical message',
            details='There seems to be something wrong', icon=m.CRITICAL, display_type=1))

    w.setWindowTitle("PyQt Dialog demo")
    w.setGeometry(300, 300, 300, 150)
    w.show()
    sys.exit(app.exec_())
