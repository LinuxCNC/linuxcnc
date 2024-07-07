#!/usr/bin/env python3
#
# Qtvcp Widgets
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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
#
#################################################################################

import os

from PyQt5 import QtWidgets, QtCore, QtGui
from PyQt5.QtWidgets import qApp
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.widgets.entry_widget import TouchInterface
from qtvcp.core import Status, Action, Info
from qtvcp import logger
from qtvcp.widgets.touchEntry import TouchDoubleSpinBox,TouchSpinBox

# Instantiate the libraries with global reference
# INFO holds INI file details
# STATUS gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
# LOG is for running code logging
INFO = Info()
STATUS = Status()
ACTION = Action()
LOG = logger.getLogger(__name__)

# Set the log level for this module
if not INFO.LINUXCNC_IS_RUNNING:
    LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL
LOG.setLevel(logger.DEBUG)
try:
    from PyQt5 import QtSvg
except:
    LOG.critical("Qtvcp error with macro_widget - is package python3-pyqt5.qtsvg installed?")

###############################################################
# helper widget for SVG display on Button
#
# We can add a svg image from a specific layer to QPushButton
###############################################################
class CustomButton(QtWidgets.QPushButton):
    def __init__(self, parent=None, path=None, layer=0):
        super(CustomButton, self).__init__(parent)
        if path is None:
            tpath = os.path.expanduser(INFO.MACRO_PATH)
            path = os.path.join(tpath, 'LatheMacro.svg')
        self.r = QtSvg.QSvgRenderer(path)
        self.basename = 'layer'
        self.setLayerNumber(layer)

    def setLayerNumber(self, num):
        temp = '%s%d' % (self.basename, int(num))
        if self.r.elementExists(temp):
            self.Num = int(num)
            self.layer = temp
        else:
            self.Num = 0
            self.layer = 'layer0'
            LOG.error("MacrosTab SVG Button-No Layer Found: {}".format(temp))

    def paintEvent(self, event):
        super(CustomButton, self).paintEvent(event)
        qp = QtGui.QPainter()
        qp.begin(self)
        self.r.render(qp, self.layer)
        qp.end()


####################################################
# helper widget for SVG display
#
# class to display certain layers of an svg file
# instantiate it with layer number or
# set layer number after with setLayerNumbet(int)
####################################################
class CustomSVG(QtSvg.QSvgWidget):
    def __init__(self, parent=None, layer=0):
        super(CustomSVG, self).__init__(parent)
        self.basename = 'layer'
        self.layer = 'layer%d' % layer
        self.num = layer

    def setLayerNumber(self, num):
        temp = '%s%d' % (self.basename, int(num))
        if self.renderer().elementExists(temp):
            self.Num = int(num)
            self.layer = temp
        else:
            LOG.error("MacrosTab SVG-No Layer Found: {}".format(temp))

    def sizeHint(self):
        return QtCore.QSize(200, 200)

    def paintEvent(self, event):
        qp = QtGui.QPainter()
        qp.begin(self)
        s = self.renderer()
        s.render(qp, self.layer)
        qp.end()


###############################################################################
# Macro tab widget
#
# macro tab widget parses the subroutine path for /lathe
# It then opens the .ngc files there and searches for keynames
# using these key names it puts together a tab widget with svg file pics
# the svg file should be in the same folder
###############################################################################

DEFAULT = 0
WARNING = 1
CRITICAL = 2

class MacroTab(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(MacroTab, self).__init__(parent)

        self.hilightStyle = "QLineEdit{border: 2px solid red;}"

        # id names for what dialog we want launched
        self.load_dialog_code = 'LOAD'
        self.save_dialog_code = 'SAVE'
        self._request_name = 'CALCULATOR'

        self.stack = QtWidgets.QStackedWidget()

        # add some buttons to run,close and menu
        hbox = QtWidgets.QHBoxLayout()
        self.runButton = QtWidgets.QPushButton("Run")
        self.runButton.pressed.connect(self.runChecked)
        self.saveButton = QtWidgets.QPushButton("save")
        self.saveButton.pressed.connect(self.saveChecked)
        self.saveButton.setVisible(False)

        self.loadButton = QtWidgets.QPushButton("load")
        self.loadButton.pressed.connect(self.loadChecked)
        self.loadButton.setVisible(False)
        self.closeButton = QtWidgets.QPushButton("Close")
        self.closeButton.pressed.connect(self.closeChecked)
        self.closeButton.setVisible(False)

        menuButton = QtWidgets.QPushButton("Menu")
        menuButton.pressed.connect(self.menuChecked)
        hbox.addWidget(self.runButton)
        hbox.addWidget(self.loadButton)
        hbox.addWidget(self.saveButton)
        hbox.addWidget(self.closeButton)
        hbox.insertSpacing(1, 20)
        hbox.addWidget(menuButton)
        hbox.addStretch(0)
        vbox = QtWidgets.QVBoxLayout()
        vbox.addWidget(self.stack)
        vbox.addStretch(1)
        vbox.addLayout(hbox)
        # add all that stuff above to me
        self.setLayout(vbox)

    def sizeHint(self):
        return QtCore.QSize(200, 200)

    def _hal_init(self):
        # add The macros to the stacked wodget
        self.buildStack(INFO.SUB_PATH_LIST)

        # get current style of the line input
        self._oldstyle = self.runButton.styleSheet()

        self.runButton.setEnabled(False)
        STATUS.connect('state-off', lambda w: self.runButton.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.runButton.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.runButton.setEnabled(STATUS.machine_is_on()
                                                                and (STATUS.is_all_homed()
                                                                or INFO.NO_HOME_REQUIRED)))
        STATUS.connect('interp-run', lambda w: self.runButton.setEnabled(False))
        STATUS.connect('not-all-homed', lambda w, axis: self.runButton.setEnabled(False or INFO.NO_HOME_REQUIRED))
        STATUS.connect('all-homed', lambda w: self.runButton.setEnabled(True))
        STATUS.connect('general',self.returnFromDialog)

    # Build a stack per macro found
    # it finds the icon info from the macro file
    # using the magic comments parsed before this
    # first find macros
    # then build a menu page
    # then build the stack
    # anything goes wrong display an error page
    def buildStack(self, pathlist):
        def checkIfFloat(val):
            return all([ [any([i.isnumeric(), i in ['.','e']]) for i in val],  len(val.split('.')) == 2] )

        macroFlag = False
        for path in pathlist:
            if 'macro' in path:
                path = os.path.expanduser(path)
                tabName = self._findMacros(path)
                LOG.debug("Macros Found: {}".format(tabName))
                if tabName is None:
                    continue
                macroFlag = True
                self._buildMenuPage(tabName,path)
                # Add pages
                # tabname is a list of found macros
                # these macro names are also used as the base name
                # of a list of required inputs.
                # we add a label and lineedit/radiobutton for each string in each
                # of these arrays
                for i, tName in enumerate(tabName):
                    # make a widget that is added to the stack
                    # TouchInterface can pop an entry dialog on focus
                    w = TouchInterface(self)
                    w.setObjectName(tName)
                    w.keyboard_enable = True
                    # redirect Touchinterface call to our function
                    w.callDialog = self.getNumbers

                    # main layout of this tab
                    hbox = QtWidgets.QHBoxLayout(w)
                    #hbox.addStretch(1)

                    # vertical layout for labels and entries
                    vbox = QtWidgets.QVBoxLayout()

                    widgetList = []

                    # add labels and edits
                    # self[tName][0] is the list of name text and defaults pairs
                    for n, name in enumerate(self[tName][0]):
                        #print('int------>',self[tName])
                        # if no list of names then continue looking
                        if name[0]=='':continue

                        # layout holds label and entry for one line
                        hbox2 = QtWidgets.QHBoxLayout()
                        # make a label 
                        l = QtWidgets.QLabel(name[0])

                        # make appropriate entries:
                        # radio buttons?
                        if name[1].lower() in('false', 'true'):
                            self['%s%d' % (tName, n)] = QtWidgets.QRadioButton()
                            if name[1].lower() == 'true':
                                self['%s%d' % (tName, n)].setChecked(True)
                        # line edits that will pop an entry dialog:
                        else:
                            if checkIfFloat(name[1]):
                                #self['%s%d' % (tName, n)] = QtWidgets.QLineEdit()
                                #self['%s%d' % (tName, n)].setText(name[1])
                                self['%s%d' % (tName, n)] = TouchDoubleSpinBox()
                                self['%s%d' % (tName, n)].callDialog = self.getNumbers
                                self['%s%d' % (tName, n)].setValue(float(name[1]))
                                wName = "{}".format(name[0])
                                self['%s%d' % (tName, n)].setObjectName(wName)
                                widgetList.append([wName,'%s%d' % (tName, n)])
                                #print('float-->',wName)
                            else:
                                self['%s%d' % (tName, n)] =  TouchSpinBox()
                                self['%s%d' % (tName, n)].callDialog = self.getNumbers
                                self['%s%d' % (tName, n)].setValue(int(name[1]))
                                wName = "{}".format(name[0])
                                self['%s%d' % (tName, n)].setObjectName(wName)
                                widgetList.append([wName,'%s%d' % (tName, n)])
                                #print('int-->',wName)

                            self['%s%d' % (tName, n)]._label = name[0]
                            self['%s%d' % (tName, n)]._tabName = tName
                            #self.set_style(self['%s%d' % (tName, n)])
                            self['%s%d' % (tName, n)].keyboard_type = 'numeric'

                        hbox2.addWidget(l)
                        hbox2.addWidget(self['%s%d' % (tName, n)])

                        # add label/entry layout to vertical layout fr this tab 
                        vbox.addLayout(hbox2)

                    #add the SVG/image pic layer
                    img_info = self[tName][1]
                    #print path+svg_info[0], svg_info[1]
                    # SVG?
                    if img_info[0].endswith('.svg'):
                        svgpath = os.path.join(path, img_info[0])
                        self['sw%d' % i] = CustomSVG(svgpath,  int(img_info[1]))
                        self['sw%d' % i].setSizePolicy(QtWidgets.QSizePolicy.MinimumExpanding,
                                    QtWidgets.QSizePolicy.MinimumExpanding)
                    # some other supported image file?
                    else:
                        # get image path
                        try:
                            #print(self[tName][1][1])
                            imgpath = os.path.join(path, self[tName][1][1])
                        except:
                            imgpath = os.path.join(path, img_info[0])

                        # use a qlabel to display image
                        self['sw%d' % i] = QtWidgets.QLabel()
                        self['sw%d' % i].setPixmap(QtGui.QPixmap(imgpath))
                        self['sw%d' % i].setScaledContents(True)
                        self['sw%d' % i].setSizePolicy(QtWidgets.QSizePolicy.MinimumExpanding,
                                    QtWidgets.QSizePolicy.MinimumExpanding)

                    # add image to the main layout for this tab
                    hbox.addWidget(self['sw%d' % i])
                    # add label/entry stack to the main layout for this tab
                    hbox.addLayout(vbox)
                    # add this tab to the stack
                    self.stack.addWidget(w)

                    # add the widget list used by the dialog
                    # to cucle through entries
                    self[tName][3] = widgetList

        # No macros found in any path
        # show a message
        if macroFlag == False:
            self._buildErrorTab()

    # Menu page has icon buttons to select the macro
    # it finds the icon info from the macro file
    # using the magic comments parsed before this
    def _buildMenuPage(self, tabNames,path):
        col = row = 0
        w = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout(w)
        vbox = QtWidgets.QVBoxLayout()
        grid = QtWidgets.QGridLayout()
        grid.setSpacing(10)
        # we grid them in columns of (arbitrarily) 5
        # hopefully we don't have too many macros...
        for i, tName in enumerate(tabNames):
            svg_name = self[tName][1][0]
            if svg_name.endswith('.svg'):
                try:
                    svg_num = self[tName][1][2]
                except:
                    svg_num = self[tName][1][1]
                svgpath = os.path.join(path, svg_name)
                # label is the only way I have found to make the buttons
                # larger - the label is under the pic - if no errors
                btn = CustomButton('Oops\n', path=svgpath, layer=svg_num)
            else:
                imgpath = os.path.join(path, svg_name)
                btn = QtWidgets.QPushButton()
                btn.setStyleSheet("border-image: url(" + imgpath + ");")
            btn.setToolTip('Macro: {}'.format(tName))
            btn.setWhatsThis('This button will select The entry page for the {} macro'.format(tName))
            btn.clicked.connect(self.menuButtonPress(i))
            btn.setSizePolicy(QtWidgets.QSizePolicy.Preferred,
                              QtWidgets.QSizePolicy.Preferred)
            grid.addWidget(btn, row, col, 1, 1)
            row += 1
            if row > 4:
                row = 0
                col += 1
        vbox.addLayout(grid)
        vbox.addStretch(1)
        hbox.addLayout(vbox)
        hbox.addStretch(1)
        # add the widget to our stack
        self.stack.addWidget(w)

    # make something so the user may have some small clue.
    # probably should do more - subroutines/macros are not user friendly
    def _buildErrorTab(self):
        w = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout(w)
        vbox.addStretch(1)
        mess = QtWidgets.QLabel('No Usable Macros Found.\nLooked in path(s) specified in INI file under heading:\n[RS274NGC],\nSUBROUTINE_PATH=')
        vbox.addWidget(mess)
        mess = QtWidgets.QLabel( '\n'.join(map(str, INFO.SUB_PATH_LIST)))
        vbox.addWidget(mess)
        # add the widget to the stack
        self.stack.addWidget(w)

    # search for special macros that have the magic comments
    # compiles them into a complicated list for each macro
    # self['macroname'] = [ [DEFAULT DATA],[SVG FILE,LAYER,ICON LAYER],{OPTION DICT NAME:OPTION DICT DATA,}]
    # returns a list on the macro names that it finds valid

    def _findMacros(self, path):
        tName = []
        macros = []
        defaults = []
        svg_data = []
        try:
            # look for NGC macros in path
            for f in os.listdir(path):
                if f.endswith('.ngc'):
                    # open and look at the first three lines
                    # these will gives us the info we need
                    # to create the macros page
                    with open(os.path.join(path, f), 'r') as temp:
                        first_line = temp.readline().strip()
                        second_line = temp.readline().strip()
                        third_line = temp.readline().strip()
                        fourth_line = temp.readline().strip()
                        # check if they have the magic comments
                        if 'MACROCOMMAND' in first_line and \
                                'MACRODEFAULT' in second_line and \
                                ('MACROSVG' in third_line or 
                                    'MACROIMAGE' in third_line):
                            name = os.path.splitext(f)[0]
                            # yes, now keep everything after '='
                            macros = first_line.split('=')[1]
                            defaults = second_line.split('=')[1]
                            svg_data = third_line.split('=')[1]
                            # we use a comma to break up titles
                            m = macros.split(',')
                            d = defaults.split(',')
                            s = svg_data.split(',')
                            # combine titles with defaults in to a list
                            for num, (m_item, d_item) in enumerate(zip(m, d)):
                                #print num,m_item,d_item
                                if num == 0:
                                    temp = []
                                    temp.append((m_item, d_item))
                                    continue
                                temp.append((m_item, d_item))

                            # look for options like save/load buttons
                            # it should be the fourth line in sub program
                            # it's put in a dict to future proof it
                            # and make it easier to parse
                            option_dict={}
                            if 'MACROOPTIONS' in fourth_line:
                                options = fourth_line.split('=')[1]
                                o = options.split(',')
                                for i in(o):
                                    h,g = i.split(':')
                                    option_dict['%s'%h.upper()]=g
                                #print option_dict

                            # add the list then add svg info, then options
                            self[name] = [temp]
                            self[name].append(s)
                            self[name].append(option_dict)
                            self[name].append([])
                            #print'group:',name, self[name]
                            # make a list of pages, which is also the macro program name
                            tName.append(name)
        except (TypeError):
            return None
        except OSError as e:
            LOG.debug('Path: {} is not valid.'.format(path))
            return None
        except Exception as e:
            LOG.debug('Exception loading Macros:', exc_info=e)
            return None
        if tName == []:
            return None
        return tName

    # This figures out what macro is showing
    # and builds the macro command from the data
    # then sends it to the controller
    def runMacro(self):
        cmd = ''
        name = str(self.stack.currentWidget().objectName())
        if name == '': return
        macro = name
        #print 'macro', macro
        for num, i in enumerate(self[name][0]):
            # check for macro that needs no data
            if i == ('', ''):break
            # Look for a radio button instance so we can convert to integers
            # other wise we assume text
            if isinstance(self['%s%d' % (name, num)], QtWidgets.QRadioButton):
                data = str(1 * int(self['%s%d' % (name, num)].isChecked()))
            else:
                data = str(self['%s%d' % (name, num)].text())
            if data != '':
                cmd = cmd + '[' + data + '] '
        command = str("O<" + macro + "> call " + cmd)
        LOG.debug("Macro command: {}".format(command))
        ACTION.CALL_MDI(command)

    # This could be 'class patched' to do something else
    # the macro dialog does this
    def runChecked(self):
        self.runMacro()

    # This could be 'class patched' to do something else
    def saveChecked(self):
        self.savePressed()

    # This could be 'class patched' to do something else
    def loadChecked(self):
        self.loadPressed()

    # This could be 'class patched' to do something else
    def closeChecked(self):
        self.stack.setCurrentIndex(0)

    # brings the menu selection page to the front
    def menuChecked(self):
        self.stack.setCurrentIndex(0)
        self.setTitle('Qtvcp Macro Menu')
        self.loadButton.setVisible(False)
        self.saveButton.setVisible(False)

    # This weird code is just so we can get the index
    # number from a menu button press
    # using clicked.connect() apparently doesn't easily
    # add user data
    def menuButtonPress(self, data):
        def calluser():
            self.stack.setCurrentIndex(data+1)
            try:
                name = self.stack.currentWidget().objectName()
                self.setTitle(name)
                self.set_statusbar('Macro Tab:{}'.format(name.upper()))
                # show these buttons if the macro specifies it
                for name in (self[name][2]):
                    if name == 'LOAD':
                        self.loadButton.setVisible(True)
                    if name == 'SAVE':
                        self.saveButton.setVisible(True)
            except: pass

        return calluser

    def loadPressed(self):
        self.getFileName()

    def savePressed(self):
        self.getSaveFileName()

    def openReturn(self, path):
        LOG.debug("Open return filename chosen: {}".format(path))
        file = QtCore.QFile(path)
        file.open(QtCore.QFile.ReadOnly)
        name = str(self.stack.currentWidget().objectName())
        while not file.atEnd():
            readLine = file.readLine()
            try:
                # Python v2.
                readLine = str(readLine, encoding='utf8')
            except NameError:
                # Python v3.
                readLine = str(readLine, encoding='utf8')
            widget,data,title = readLine.split(',')
            #print widget,data,title,name
            if name in widget:
                # set widgets to data:
                # Look for a radio button instance so we can convert to integers
                # other wise we assume text
                if isinstance(self[widget], QtWidgets.QRadioButton):
                    self[widget].setChecked(bool(data))
                else:
                    self[widget].setText(str(data))

    # save the current screen data to file picked by the user.
    # it's a plain text file
    def saveReturn(self, path):
        LOG.debug("Save return filename chosen: {}".format(path))
        name = str(self.stack.currentWidget().objectName())
        if name == '': return
        file = QtCore.QFile(path)
        if file.open(QtCore.QFile.WriteOnly):
            for num, i in enumerate(self[name][0]):
                widgetname = '%s%d' % (name, num)
                # Look for a radio button instance so we can convert to bool
                # other wise we assume text
                if isinstance(self[widgetname], QtWidgets.QRadioButton):
                    data = str(1 * int(self[widgetname].isChecked()))
                else:
                    data = str(self[widgetname].text())
                line =  '%s,%s,    %s\n'%( widgetname, str(data), i[0])
                QtCore.QTextStream(file) << line
        else:
            QMessageBox.information(self, "Unable to open file",
                    file.errorString())

    # we do this instead of directly so the dialog version's title changes
    # when it's overridden
    def setTitle(self, string):
        self.setWindowTitle(string)

    # get numeric data
    def getNumbers(self, widget, ktype=None, next=False):
        # remove overlay so style can be changed
        STATUS.emit('focus-overlay-changed', False, None, None)
        # record the original style
        self._oldstyle = widget.styleSheet()
        # change to highlight style
        widget.setStyleSheet(self.hilightStyle) 
        qApp.processEvents()

        # dialog call
        mess = {'NAME':self._request_name,
                'ID':'%s__macro' % self.objectName(),
                'PRELOAD':float(widget.text()),
                'TITLE':'{} Macro Entry For {}'.format(widget._tabName,widget._label),
                'WIDGET':widget,
                'NEXT':next,
                'WIDGETCYCLE': True
               }
        LOG.debug("get numbers {}".format(mess))
        STATUS.emit('dialog-request', mess)

    # request the system to pop a load path picker dialog
    # do this so the system is consistent and things like dialog
    # placement are done.
    def getFileName(self):
        mess = {'NAME':self.load_dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'Load Macro',
            'FILENAME':'%s_data.txt' % str(self.stack.currentWidget().objectName()),
            'EXTENSIONS':'Text Files (*.txt);;ALL Files (*.*)'
            }
        STATUS.emit('dialog-request', mess)

    # request the system to pop a save path picker dialog
    # do this so the system is consistent and things like dialog
    # placement are done.
    def getSaveFileName(self):
        mess = {'NAME':self.save_dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'Save Macro', 'FILENAME':'%s_data.txt' % str(self.stack.currentWidget().objectName()),
            'EXTENSIONS':'Text Files (*.txt);;ALL Files (*.*)'}
        STATUS.emit('dialog-request', mess)

    # process the STATUS return message from dialogs
    def returnFromDialog(self, w, message):
        if message.get('NAME') == self.load_dialog_code:
            path = message.get('RETURN')
            code = bool(message.get('ID') == '%s__'% self.objectName())
            if path and code:
                self.openReturn(path)
        elif message.get('NAME') == self.save_dialog_code:
            path = message.get('RETURN')
            code = bool(message.get('ID') == '%s__'% self.objectName())
            if path and code:
                self.saveReturn(path)
        elif message.get('NAME') == self._request_name:
            num = message.get('RETURN')
            code = bool(message.get('ID') == '%s__macro'% self.objectName())
            widget = message.get('WIDGET')
            next = message.get('NEXT', False)
            back = message.get('BACK', False)
            if code and widget is not None:
                widget.setStyleSheet(self._oldstyle)
                if num is not None:
                    widget.setValue(num)
                    # clear high lighting

                # request for next input widget from parm_list
                if next or back:
                    name = self.stack.currentWidget().objectName()
                    # find current widget in list
                    for num,data in enumerate(self[name][3]):
                        txt,widname = data
                        if txt == widget.objectName():
                            if next:
                                num +=1
                                if num == len(self[name][3]):
                                    num = 0
                            else:
                                num -=1
                                if num == -1:
                                    num = len(self[name][3])-1
                            # build reference to the next widget
                            newobj = self[self[name][3][num][1]]
                            break

                    # update the dialog
                    self.getNumbers(newobj,None, next=True)

#####################################################
# Helper functions
#####################################################

    # return false if failed so other ways of reporting can be used.
    # there might not be a statusbar in main screen.
    def set_statusbar(self, msg, priority = DEFAULT, noLog = False):
        try:
            self.QTVCP_INSTANCE_.add_status(msg, priority, noLog)
        except:
            return False
        return True

    def set_style(self, widget):
        widget.setStyleSheet(
"""
QDoubleSpinBox,
QSpinBox {
    	padding-right: 15px; /* make room for the arrows */
    	border-image: url(:/images/frame.png) 4;
    	border-width: 3;
	padding-left: 5px;
	padding-right: 5px;
	padding-top: 0px;
	padding-bottom: 0px;
	font-size:20px;
	border: 1px solid black;
	min-width: 100px;
	max-height: 30;
}

QDoubleSpinBox::up-button,
QSpinBox::up-button {
    	subcontrol-origin: border;
 	subcontrol-origin: padding;
          	subcontrol-position: right; width: 30px; height: 28px;
	image: url(:/widget/images/arrow_up1.png);
           	border: 1px solid black;
}
QDoubleSpinBox::up-button:pressed,
QSpinBox::up-button:pressed {
    	image: url(:/widget/images/arrow_up2.png);
}

QDoubleSpinBox::down-button,
QSpinBox::down-button {
    	subcontrol-origin: border;
 	subcontrol-origin: padding;
	subcontrol-position: left; width: 30px; height: 28px;
	image: url(:/widget/images/arrow_down1.png);
	border: 1px solid black;
}
QDoubleSpinBox::down-button:pressed,
QSpinBox::down-button:pressed {
   	 image: url(:/widget/images/arrow_down2.png) 1;
}

""")

    # usual boiler code
    # (used so we can use code such as self[SomeDataName]
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    # STATUS may cause seg fault testing here
    import sys
    app = QtWidgets.QApplication(sys.argv)
    #sw = QtSvg.QSvgWidget('LatheMacro.svg')
    sw = MacroTab()
    sw.buildStack(['~/linuxcnc/nc_files/examples/macros/lathe'])
    sw.setGeometry(50, 50, 759, 668)
    sw.show()
    sys.exit(app.exec_())
