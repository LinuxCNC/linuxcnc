#!/usr/bin/env python
# QTVcp Widget - MDI edit line widget
# This widgets displays linuxcnc axis position information.
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
from PyQt4.QtGui import QMessageBox, QFileDialog, QColor, QDesktopWidget, \
        QDialog, QDialogButtonBox, QVBoxLayout, QPushButton, QHBoxLayout
from PyQt4.QtCore import Qt, pyqtSlot, pyqtProperty
from qtvcp.widgets.simple_widgets import _HalWidgetBase, hal
from qtvcp.widgets.origin_offsetview import Lcnc_OriginOffsetView as OFFVIEW_WIDGET
from qtvcp.widgets.camview_widget import CamView
from qtvcp.widgets.macro_widget import macroTab
from qtvcp.qt_glib import GStat, Lcnc_Action
from qtvcp.qt_istat import IStat

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

# Instantiate the libraries with global reference
# GSTAT gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
GSTAT = GStat()
ACTION = Lcnc_Action()
INI = IStat()

class Lcnc_Dialog(QMessageBox):
    def __init__(self, parent = None):
        QMessageBox.__init__(self,parent)
        self.setTextFormat(Qt.RichText)
        self.setText('<b>Sample Text?</b>')
        self.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        self.setIcon(QMessageBox.Critical)
        self.setDetailedText('Sample Detail Text')
        self.OK_TYPE = 1
        self.YN_TYPE = 0
        self._state = False
        self._color = QColor(0, 0, 0, 150)
        self.focus_text =''
        self.hide()

    def showdialog(self, message, more_info=None, details=None, display_type=1,
                     icon=QMessageBox.Information, pinname=None, focus_text=None,
                        focus_color=None):
        if focus_text:
            self.focus_text = focus_text
        if focus_color:
            self._color = focus_color
        self.OK_TYPE = 1
        self.YN_TYPE = 0
        self.QUESTION = QMessageBox.Question
        self.INFO = QMessageBox.Information
        self.WARNING = QMessageBox.Warning
        self.CRITICAL = QMessageBox.Critical
        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags( self.windowFlags() |Qt.Tool |
                 Qt.FramelessWindowHint | Qt.Dialog |
                 Qt.WindowStaysOnTopHint |Qt.WindowSystemMenuHint)
                 #Qt.X11BypassWindowManagerHint
        self.setIcon(icon)
        self.setText('<b>%s</b>'% message)
        if more_info:
            self.setInformativeText(more_info)
        else:
           self.setInformativeText('')
        if details:
            self.setDetailedText(details)
        if display_type == self.OK_TYPE:
            self.setStandardButtons(QMessageBox.Ok)
        else:
            self.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        self.buttonClicked.connect(self.msgbtn)
        GSTAT.emit('focus-overlay-changed',True,self.focus_text,self._color)
        retval = self.exec_()
        GSTAT.emit('focus-overlay-changed',False,None,None)
        log.debug("Value of pressed button: {}".format(retval))
        if retval == QMessageBox.No:
            return False
        else:
            return True

    def showEvent(self, event):
        geom = self.frameGeometry()
        geom.moveCenter(QDesktopWidget().availableGeometry().center())
        self.setGeometry(geom)
        super(Lcnc_Dialog, self).showEvent(event)

    def msgbtn(self, i):
        log.debug("Button pressed is: {}".format(i.text()))
        return

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
        self._color = QColor(0, 0, 0,150)

    overlay_color = pyqtProperty(QColor, getColor, setColor)
    state = pyqtProperty(bool, getState, setState, resetState)

################################################################################
# Tool Change Dialog
################################################################################
class Lcnc_ToolDialog(Lcnc_Dialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_ToolDialog, self).__init__(parent)
        self.setText('<b>Manual Tool Change Request</b>')
        self.setInformativeText('Please Insert Tool 0')
        self.setStandardButtons(QMessageBox.Ok)

    # We want the tool change HAL pins the same as whats used in AXIS so it is
    # easier for users to connect to.
    # So we need to trick the HAL component into doing this for these pins,
    # but not anyother Qt widgets.
    # So we record the original base name of the component, make our pins, then
    # switch it back
    def _hal_init(self):
        self.topParent = self.QTVCP_INSTANCE_
        #_HalWidgetBase._hal_init(self)
        oldname = self.hal.comp.getprefix()
        self.hal.comp.setprefix('hal_manualtoolchange')
        self.hal_pin = self.hal.newpin('change', hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(self.tool_change)
        self.tool_number = self.hal.newpin('number', hal.HAL_S32, hal.HAL_IN)
        self.changed = self.hal.newpin('changed', hal.HAL_BIT, hal.HAL_OUT)
        #self.hal_pin = self.hal.newpin(self.hal_name + 'change_button', hal.HAL_BIT, hal.HAL_IN)
        self.hal.comp.setprefix(oldname)

    def showtooldialog(self, message, more_info=None, details=None, display_type=1,
                     icon=QMessageBox.Information):

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags( self.windowFlags() |Qt.Tool |
                 Qt.FramelessWindowHint | Qt.Dialog |
                    Qt.WindowStaysOnTopHint |Qt.WindowSystemMenuHint)
        self.setIcon(icon)
        self.setTextFormat(Qt.RichText)
        self.setText('<b>%s</b>'% message)
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
        retval = self.exec_()
        if retval == QMessageBox.Cancel:
            return False
        else:
            return True

    def tool_change(self, change):
        if change:
                answer = self.do_message(change)
                if answer:
                    self.changed.set(True)
                else:
                    # TODO add abort command
                    log.debug('cancelled should abort')
        elif not change:
            self.changed.set(False)

    def do_message(self,change):
        if change and not self.changed.get():
            MORE = 'Please Insert Tool %d'% self.tool_number.get()
            MESS = 'Manual Tool Change Request'
            DETAILS = ' Tool Info:'
            GSTAT.emit('focus-overlay-changed',True,MESS, self._color)
            result = self.showtooldialog(MESS,MORE,DETAILS)
            GSTAT.emit('focus-overlay-changed',False,None,None)
            return result

    #**********************
    # Designer properties
    #**********************
    # inherited

################################################################################
# File Open Dialog
################################################################################
class Lcnc_FileDialog(QFileDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_FileDialog, self).__init__(parent)
        self._state = False
        self._color = QColor(0, 0, 0, 150)
        self.setWindowModality(Qt.ApplicationModal)
        self.setFileMode(QFileDialog.ExistingFile)
        self.setDirectory( os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files/examples'))
        exts = INI.get_qt_filter_extensions()
        self.setNameFilter(exts)

    def _hal_init(self):
        GSTAT.connect('load-file-request', lambda w: self.load_dialog())
        GSTAT.connect('dialog-request', self._external_request)

    def _external_request(self, w, cmd):
        if cmd =='FILE':
            self.load_dialog()

    def load_dialog(self):
        GSTAT.emit('focus-overlay-changed',True,'Open Gcode',self._color)
        #self.move( 400, 400 )
        fname = None
        if (self.exec_()):
            fname = self.selectedFiles()[0]
            self.setDirectory(self.directory().absolutePath())
        GSTAT.emit('focus-overlay-changed',False,None,None)
        if fname:
            #NOTE.notify('Error',str(fname),QtGui.QMessageBox.Information,10)
            f = open(fname, 'r')
            ACTION.OPEN_PROGRAM(fname)
        return fname

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
        self._color = QColor(0, 0, 0,150)

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

################################################################################
# origin Offset Dialog
################################################################################
class Lcnc_OriginOffsetDialog(QDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_OriginOffsetDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self._state = False
        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags( self.windowFlags() |Qt.Tool |
                  Qt.Dialog |
                 Qt.WindowStaysOnTopHint |Qt.WindowSystemMenuHint)
        self.setMinimumSize(200,200)
        buttonBox = QDialogButtonBox()
        buttonBox.setEnabled(False)
        GSTAT.connect('not-all-homed', lambda w, axis: buttonBox.setEnabled(False))
        GSTAT.connect('all-homed', lambda w: buttonBox.setEnabled(True))
        GSTAT.connect('state-estop', lambda w: buttonBox.setEnabled(False))
        GSTAT.connect('state-estop-reset', lambda w: buttonBox.setEnabled(GSTAT.machine_is_on() and GSTAT.is_all_homed()))
        for i in('X','Y','Z'):
            b = 'button_%s'%i
            self[b] = QPushButton('Zero %s'%i)
            self[b].clicked.connect(self.zeroPress('%s'%i))
            buttonBox.addButton(self[b],3)
        
        v = QVBoxLayout()
        h = QHBoxLayout()
        self._o = OFFVIEW_WIDGET()
        self._o._hal_init()
        self.setLayout(v)
        v.addWidget(self._o)
        b = QPushButton('OK')
        b.clicked.connect(lambda:self.close())
        h.addWidget(b)
        h.addWidget(buttonBox)
        v.addLayout(h)
        self.setModal(True)

    def _hal_init(self):
        self.topParent = self.QTVCP_INSTANCE_
        GSTAT.connect('dialog-request', self._external_request)

    def _external_request(self, w, cmd):
        if cmd =='ORIGINOFFSET':
            self.load_dialog()

    # This weird code is just so we can get the axis
    # letter
    # using clicked.connect() apparently can't easily
    # add user data 
    def zeroPress(self,data):
        def calluser():
            self.zeroAxis(data)
        return calluser

    def zeroAxis(self, index):
        ACTION.SET_AXIS_ORIGIN(index,0)

    def load_dialog(self):
        GSTAT.emit('focus-overlay-changed',True,'Set Origin Offsets',self._color)
        # move to botton laeft of parent
        ph = self.topParent.geometry().height()
        px = self.topParent.geometry().x()
        py = self.topParent.geometry().y()
        dw = 450#self.width()
        dh = 300#self.height()
        self.setGeometry( px, py+ph-dh, dw, dh )
        self.show()
        self.exec_()
        GSTAT.emit('focus-overlay-changed',False,None,None)

    # usual boiler code
    # (used so we can use code such as self[SomeDataName]
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

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
        self._color = QColor(0, 0, 0,150)

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

################################################################################
# CamView Dialog
################################################################################
class Lcnc_CamViewDialog(QDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_CamViewDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self._state = False

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags( self.windowFlags() |Qt.Tool |
                  Qt.Dialog |
                 Qt.WindowStaysOnTopHint |Qt.WindowSystemMenuHint)
        self.setMinimumSize(400,400)
        buttonBox = QDialogButtonBox(QDialogButtonBox.Ok)
        b = buttonBox.button(QDialogButtonBox.Ok)
        b.clicked.connect(lambda:self.close())
        l = QVBoxLayout()
        o = CamView()
        o._hal_init()
        self.setLayout(l)
        l.addWidget(o)
        l.addWidget(buttonBox)

    def _hal_init(self):
        self.topParent = self.QTVCP_INSTANCE_
        GSTAT.connect('dialog-request', self._external_request)

    def _external_request(self, w, cmd):
        if cmd =='CAMVIEW':
            self.load_dialog()

    def load_dialog(self):
        GSTAT.emit('focus-overlay-changed',True,'Cam View Dialog',self._color)
        # move to botton laeft of parent
        ph = self.topParent.geometry().height()
        px = self.topParent.geometry().x()
        py = self.topParent.geometry().y()
        dw = self.width()
        dh = self.height()
        self.setGeometry( px, py+ph-dh, dw, dh )
        self.show()
        self.exec_()
        GSTAT.emit('focus-overlay-changed',False,None,None)

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
        self._color = QColor(0, 0, 0,150)

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

################################################################################
# macroTab Dialog
################################################################################
class Lcnc_MacroTabDialog(QDialog, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_MacroTabDialog, self).__init__(parent)
        self._color = QColor(0, 0, 0, 150)
        self._state = False

        self.setWindowModality(Qt.ApplicationModal)
        self.setWindowFlags( self.windowFlags() |Qt.Tool |
                  Qt.Dialog |
                 Qt.WindowStaysOnTopHint |Qt.WindowSystemMenuHint)
        self.setMinimumSize(600,400)
        self.resize(600, 400)
        # patch class to call our button methods rather then the
        # original methods (Gotta do before instantiation)
        macroTab.cancelChecked = self._cancel
        macroTab.okChecked = self._ok
        # ok now instantiate patched class
        self.tab = macroTab()
        l = QVBoxLayout()
        self.setLayout(l)
        l.addWidget(self.tab)

    def _hal_init(self):
        # gotta call this since we instantiated this out of qtvcp's knowledge
        self.tab._hal_init()
        self.topParent = self.QTVCP_INSTANCE_
        GSTAT.connect('dialog-request', self._external_request)

    def _external_request(self, w, cmd):
        if cmd =='MACRO':
            self.load_dialog()

    # This method is called instead of MacroTab's cancelChecked method
    # we do this so we can use it's buttons to hide our dialog
    # rather then close the macroTab widget
    def _cancel(self):
        self.close()

    # This method is called instead of MacroTab's okChecked() method
    # we do this so we can use it's buttons to hide our dialog
    # rather then close the macroTab widget
    def _ok(self):
        self.tab.runMacro()
        self.close()

    def load_dialog(self):
        GSTAT.emit('focus-overlay-changed',True,'Lathe Macro Dialog',self._color)
        self.tab.stack.setCurrentIndex(0)
        # move to botton laeft of parent
        ph = self.topParent.geometry().height()
        px = self.topParent.geometry().x()
        py = self.topParent.geometry().y()
        dw = self.width()
        dh = self.height()
        self.setGeometry( px, py+ph-dh, dw, dh )
        self.show()
        self.exec_()
        GSTAT.emit('focus-overlay-changed',False,None,None)

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
        self._color = QColor(0, 0, 0,150)

    state = pyqtProperty(bool, getState, setState, resetState)
    overlay_color = pyqtProperty(QColor, getColor, setColor)

################################
# for testing without editor:
################################
def main():
    import sys
    from PyQt4.QtGui import QApplication

    app = QApplication(sys.argv)
    widget = Lcnc_ToolDialog()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()


