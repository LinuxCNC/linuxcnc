from PyQt4.QtGui import *
from PyQt4.QtCore import Qt

class Message:
    def __init__(self):
        self.OK_TYPE = 1
        self.YN_TYPE = 0
        self.QUESTION = QMessageBox.Question
        self.INFO = QMessageBox.Information
        self.WARNING = QMessageBox.Warning
        self.CRITICAL = QMessageBox.Critical

    def showdialog(self, message, more_info=None, details=None, display_type=1,
                     icon=QMessageBox.Information, pinname=None):
        msg = QMessageBox()
        msg.setWindowModality(Qt.ApplicationModal)
        msg.setIcon(icon)
        #msg.setWindowTitle("MessageBox demo")
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

        retval = msg.exec_()
        print "value of pressed message box button:", retval
        if retval == QMessageBox.No:
            return False
        else:
            return True

    def msgbtn(self, i):
        print "Button pressed is:",i.text()




    # This is part of the user message system
    # There is status that prints to the status bar
    # There is Okdialog that prints a dialog that the user must acknoledge
    # there is yes/no dialog where the user must choose between yes or no
    # you can combine status and dialog messages so they print to the status bar 
    # and pop a dialog
    def on_printmessage(self, pin, pinname,boldtext,text,type):
        if not pin.get(): return
        if boldtext == "NONE": boldtext = None
        if "status" in type:
            if boldtext:
                statustext = boldtext
            else:
                statustext = text
            self.notify(_("INFO:"),statustext,INFO_ICON)
        if "dialog" in type or "okdialog" in type:
            if pin.get():
                self.halcomp[pinname + "-waiting"] = True
            if "okdialog" in type:
                self.warning_dialog(boldtext,True,text,pinname)
            else:
                if pin.get():
                    self.halcomp[pinname + "-response"] = 0
                self.warning_dialog(boldtext,False,text,pinname)

    # search for and set up user requested message system.
    # status displays on the statusbat and requires no acknowledge.
    # dialog displays a GTK dialog box with yes or no buttons
    # okdialog displays a GTK dialog box with an ok button
    # dialogs require an answer before focus is sent back to main screen
    def message_setup(self):
        if not self.inifile:
            return
        m_boldtext = self.inifile.findall("DISPLAY", "MESSAGE_BOLDTEXT")
        m_text = self.inifile.findall("DISPLAY", "MESSAGE_TEXT")
        m_type = self.inifile.findall("DISPLAY", "MESSAGE_TYPE")
        m_pinname = self.inifile.findall("DISPLAY", "MESSAGE_PINNAME")
        if len(m_text) != len(m_type):
            print _("**** Gscreen ERROR:    Invalid message configuration (missing text or type) in INI File [DISPLAY] section")
        if len(m_text) != len(m_pinname):
            print _("**** Gscreen ERROR:    Invalid message configuration (missing pinname) in INI File [DISPLAY] section")
        if len(m_text) != len(m_boldtext):
            print _("**** Gscreen ERROR:    Invalid message configuration (missing boldtext) in INI File [DISPLAY] section")
        for bt,t,c ,name in zip(m_boldtext,m_text, m_type,m_pinname):
            #print bt,t,c,name
            if not ("status" in c) and not ("dialog" in c) and not ("okdialog" in c):
                print _("**** Gscreen ERROR:    invalid message type (%s)in INI File [DISPLAY] section"% c)
                continue
            if not name == None:
                # this is how we make a pin that can be connected to a callback 
                self.data['name'] = hal_glib.GPin(self.halcomp.newpin(name, hal.HAL_BIT, hal.HAL_IN))
                self.data['name'].connect('value-changed', self.on_printmessage,name,bt,t,c)
                if ("dialog" in c):
                    self.halcomp.newpin(name+"-waiting", hal.HAL_BIT, hal.HAL_OUT)
                    if not ("ok" in c):
                        self.halcomp.newpin(name+"-response", hal.HAL_BIT, hal.HAL_OUT)

    # display dialog
    def warning_dialog(self,message, displaytype, secondary=None,pinname=None):
        if displaytype:
            dialog = gtk.MessageDialog(self.widgets.window1,
                gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_INFO, gtk.BUTTONS_OK,message)
        else:   
            dialog = gtk.MessageDialog(self.widgets.window1,
               gtk.DIALOG_DESTROY_WITH_PARENT,
               gtk.MESSAGE_QUESTION, gtk.BUTTONS_YES_NO,message)
        # if there is a secondary message then the first message text is bold
        if secondary:
            dialog.format_secondary_text(secondary)
        dialog.show_all()
        try:
            if "dialog_return" in dir(self.handler_instance):
                dialog.connect("response", self.handler_instance.dialog_return,self,displaytype,pinname)
            else:
                dialog.connect("response", self.dialog_return,displaytype,pinname)
        except:
            dialog.destroy()
            raise NameError (_('Dialog error - Is the dialog handler missing from the handler file?'))
        if pinname == "TOOLCHANGE":
            dialog.set_title(_("Manual Toolchange"))
        else:
            dialog.set_title(_("Operator Message"))

    # message dialog returns a response here
    # This includes the manual tool change dialog
    # We know this by the pinname being called 'TOOLCHANGE' 
    def dialog_return(self,widget,result,dialogtype,pinname):
        if pinname == "TOOLCHANGE":
            self.halcomp["tool-changed"] = True
            widget.destroy()
            try:
                self.widgets.statusbar1.remove_message(self.statusbar_id,self.data.tool_message)
            except:
                self.show_try_errors()
            return
        if not dialogtype: # yes/no dialog
            if result == gtk.RESPONSE_YES:result = True
            else: result = False
            if pinname:
                self.halcomp[pinname + "-response"] = result
        if pinname:
            self.halcomp[pinname + "-waiting"] = False
        widget.destroy()

if __name__ == '__main__':
    import sys
    from PyQt4.QtCore import *
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
