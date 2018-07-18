#!/usr/bin/env python
# QTVcp Widget
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
#
# base code Found on Code Cookbook
#
# This widget pops up an onscreen keyboard for entries
# Used in the Macro and MDI line widget

from PyQt5 import QtWidgets, QtCore, QtGui
from decimal import Decimal

# applicationle widgets
SIP_WIDGETS = [QtWidgets.QLineEdit]


class MyFlatPushButton(QtWidgets.QPushButton):
    def __init__(self, caption, min_size=(50, 50)):
        self.MIN_SIZE = min_size
        super(MyFlatPushButton, self).__init__(caption)
        self.setFocusPolicy(QtCore.Qt.NoFocus)
        self.setStyleSheet('''
MyFlatPushButton {
font-weight: bold;
color: #333;
border: 2px solid #555;
border-radius: 11px;
padding: px;
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #888);
min-width: 30px;
}
        ''')
    def sizeHint(self):
        return QtCore.QSize(self.MIN_SIZE[0], self.MIN_SIZE[1])


class SoftInputWidget(QtWidgets.QDialog):
    def __init__(self, parent_object, keyboard_type='default'):
        super(SoftInputWidget, self).__init__()
        self.setWindowModality(QtCore.Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.FramelessWindowHint |
                            QtCore.Qt.WindowStaysOnTopHint)
        self.INPUT_WIDGET = None
        self.PARENT_OBJECT = parent_object
        self.signalMapper = QtCore.QSignalMapper(self)

        self.NO_ORD_KEY_LIST = list()
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Left)
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Up)
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Right)
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Down)
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Backspace)
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Enter)
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Tab)
        self.NO_ORD_KEY_LIST.append(QtCore.Qt.Key_Escape)

        self.do_layout(keyboard_type)

        self.signalMapper.mapped[int].connect(self.buttonClicked)

        self.setStyleSheet('''
SoftInputWidget {
font-weight: bold;
color: #333;
border: 2px solid #555;
border-radius: 15px;
padding: px;
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #888);
min-width: 30px;
}
''')
    def do_layout(self, keyboard_type='default'):
        """
        @param   keyboard_type:
        @return:
        """
        gl = QtWidgets.QVBoxLayout()
        self.setFont(self.PARENT_OBJECT.font())
        number_widget_list = []
        sym_list = range(0, 6)
        for sym in sym_list:
            button = MyFlatPushButton(str(sym))
            button.KEY_CHAR = ord(str(sym))
            number_widget_list.append(button)

        number_widget_list.append('new_row')
        sym_list = range(6, 10)
        for sym in sym_list:
            button = MyFlatPushButton(str(sym))
            button.KEY_CHAR = ord(str(sym))
            number_widget_list.append(button)

        button = MyFlatPushButton('.')
        button.KEY_CHAR = ord('.')
        number_widget_list.append(button)
        button = MyFlatPushButton('-')
        button.KEY_CHAR = ord('-')
        number_widget_list.append(button)

        # alphabets
        alpha_widget_list = []
        sym_list = ['Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
                    'new_row',
                    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
                    'new_row',
                    'Z', 'X', 'C', 'V', 'B', 'N', 'M']
        for sym in sym_list:
            if sym == 'new_row':
                alpha_widget_list.append('new_row')
            else:
                button = MyFlatPushButton(sym)
                button.KEY_CHAR = ord(sym)
                alpha_widget_list.append(button)

        control_widget_list = []

        button = MyFlatPushButton('Up')
        button.setToolTip('Cursor Up')
        button.KEY_CHAR = QtCore.Qt.Key_Up
        control_widget_list.append(button)
        control_widget_list.append('sep')

        button = MyFlatPushButton('Dwn')
        button.setToolTip('Cursor Down')
        button.KEY_CHAR = QtCore.Qt.Key_Down
        control_widget_list.append(button)
        control_widget_list.append('sep')

        # back space
        button = MyFlatPushButton('<B')
        button.setToolTip('Backspace')
        button.KEY_CHAR = QtCore.Qt.Key_Backspace
        control_widget_list.append(button)
        control_widget_list.append('sep')

        # close
        button = MyFlatPushButton('Close')
        button.setToolTip('Close Keyboard')
        button.KEY_CHAR = QtCore.Qt.Key_Escape
        control_widget_list.append(button)
        control_widget_list.append('sep')

        # enter
        button = MyFlatPushButton('Enter', min_size=(110, 60))
        button.setToolTip('Enter Command')
        button.KEY_CHAR = QtCore.Qt.Key_Enter
        control_widget_list.append(button)
        control_widget_list.append('sep')

        MAX_COL = 10
        col = 0
        tlist = list()
        if keyboard_type == 'numeric':
            widget_list = number_widget_list
        elif keyboard_type == 'alpha':
            widget_list = alpha_widget_list
        else:
            widget_list = list()
            widget_list.extend(number_widget_list)
            widget_list.append('new_row')
            widget_list.extend(alpha_widget_list)

        widget_list.append('new_row')
        widget_list.extend(control_widget_list)

        for widget in widget_list:
            if widget == 'new_row':
                col = MAX_COL
            elif widget == 'sep':
                tlist.append(self.get_vline())
                continue
            else:
                tlist.append(widget)
                widget.clicked.connect(self.signalMapper.map)
                self.signalMapper.setMapping(widget, widget.KEY_CHAR)

            if col == MAX_COL:
                col = 0
                v = QtWidgets.QHBoxLayout()
                v.addStretch()
                v.setSpacing(5)
                map(v.addWidget, tlist)
                v.addStretch()
                gl.addLayout(v)
                tlist = []
            else:
                col += 1

        v = QtWidgets.QHBoxLayout()
        v.setSpacing(5)
        v.addStretch()
        map(v.addWidget, tlist)
        v.addStretch()
        gl.addLayout(v)
        gl.setContentsMargins(0, 0, 0, 0)
        gl.setSpacing(5)
        gl.setSizeConstraint(gl.SetFixedSize)

        self.setLayout(gl)

    def reject(self):
        self.buttonClicked(QtCore.Qt.Key_Escape)

    def buttonClicked(self, char_ord):
        w = self.INPUT_WIDGET
        if char_ord in self.NO_ORD_KEY_LIST:
            keyPress = QtGui.QKeyEvent(QtCore.QEvent.KeyPress, char_ord, QtCore.Qt.NoModifier, '')
        else:
            keyPress = QtGui.QKeyEvent(QtCore.QEvent.KeyPress, char_ord, QtCore.Qt.NoModifier, chr(char_ord))
        # hide on enter or esc button click
        if char_ord == QtCore.Qt.Key_Escape:
            self.hide()
        else:
            # send keypress event to widget
            QtWidgets.QApplication.sendEvent(w, keyPress)
            if char_ord == QtCore.Qt.Key_Enter:
                self.hide()

        # line edit returnPressed event is triggering twise for press and release both
        # that is why do not send release event for special key
        if char_ord not in self.NO_ORD_KEY_LIST:
            keyRelease = QtGui.QKeyEvent(QtCore.QEvent.KeyPress, char_ord, QtCore.Qt.NoModifier, '')
            QtWidgets.QApplication.sendEvent(w, keyRelease)

    def show_input_panel(self, widget):
        self.INPUT_WIDGET = widget
        self.show()
        self.update_panel_position()
        self.setFocus()
        self.raise_()

    def update_panel_position(self):
        widget = self.INPUT_WIDGET
        if not widget: return

        widget_rect = widget.rect()
        widget_bottom = widget.mapToGlobal(QtCore.QPoint(widget.frameGeometry().x(),
                                                         widget.frameGeometry().y())).y()
        screen_height = QtWidgets.qApp.desktop().availableGeometry().height()
        input_panel_height = self.geometry().height() + 5

        if (screen_height - widget_bottom) > input_panel_height:
            # display input panel at bottom of widget
            panelPos = QtCore.QPoint(widget_rect.left(), widget_rect.bottom() + 2)
        else:
            # display input panel at top of widget
            panelPos = QtCore.QPoint(widget_rect.left(), widget_rect.top() - input_panel_height)

        panelPos = widget.mapToGlobal(panelPos)
        self.move(panelPos)

    def _get_line(self, vertical=True):
        line = QtWidgets.QFrame()
        line.setContentsMargins(0, 0, 0, 0)
        if vertical is True:
            line.setFrameShape(line.VLine)
        else:
            line.setFrameShape(line.HLine)
        line.setFrameShadow(line.Sunken)
        return line

    def get_hline(self):
        return self._get_line(vertical=False)

    def get_vline(self):
        return self._get_line()


class TouchInterface(QtCore.QObject):
    def __init__(self, PARENT_WIDGET):
        super(TouchInterface, self).__init__()
        self._PARENT_WIDGET = PARENT_WIDGET
        self._input_panel_alpha = SoftInputWidget(PARENT_WIDGET, 'alpha')
        self._input_panel_numeric = SoftInputWidget(PARENT_WIDGET, 'numeric')
        self._input_panel_full = SoftInputWidget(PARENT_WIDGET, 'default')

    def childEvent(self, event):
        if event.type() == QtCore.QEvent.ChildAdded:
            if isinstance(event.child(), *SIP_WIDGETS):
                event.child().installEventFilter(self)

    def eventFilter(self, widget, event):
      try:
        if self._PARENT_WIDGET.focusWidget() == widget and event.type() == QtCore.QEvent.MouseButtonPress:
            if hasattr(widget, 'keyboard_type'):
                if widget.keyboard_type.lower() == 'alpha':
                    self._input_panel_alpha.show_input_panel(widget)
                elif widget.keyboard_type.lower() == 'numeric':
                    self._input_panel_numeric.show_input_panel(widget)
                else:
                    self._input_panel_full.show_input_panel(widget)
        return False
      except:
        return False

class TouchInputWidget(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(TouchInputWidget, self).__init__(parent)
        self.touch_interface = TouchInterface(self)

    def childEvent(self, event):
        self.touch_interface.childEvent(event)

    def eventFilter(self, widget, event):
        return self.touch_interface.eventFilter(widget, event)


class ExampleWidget(TouchInputWidget):
    def __init__(self):
        super(ExampleWidget, self).__init__()

        self.txtNumeric = QtWidgets.QLineEdit()
        # actiate touch input
        self.txtNumeric.keyboard_type = 'numeric'

        self.txtText = QtWidgets.QLineEdit()
        # activate touch input
        self.txtText.keyboard_type = 'default'

        gl = QtWidgets.QVBoxLayout()
        gl.addWidget(self.txtNumeric)
        gl.addWidget(self.txtText)

        self.setWindowTitle('Touch Input Demo')
        self.setLayout(gl)


class ExampleDialog(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super(ExampleDialog, self).__init__(parent)
        #TouchInputWidget.__init__(self)
        #self =QtWidgets.QDialog()
        #self.setWindowModality(QtCore.Qt.ApplicationModal)
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.Tool |
                            QtCore.Qt.Dialog | QtCore.Qt.WindowStaysOnTopHint |
                            QtCore.Qt.WindowSystemMenuHint)
        buttonBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok)
        b = buttonBox.button(QtWidgets.QDialogButtonBox.Ok)
        b.clicked.connect(lambda: self.end())
        l = QtWidgets.QVBoxLayout()
        self.setLayout(l)

        o = TouchInputWidget()
        txt = QtWidgets.QLineEdit()
        txt.keyboard_type = 'alpha'

        Num = QtWidgets.QLineEdit()
        # actiate touch input
        Num.keyboard_type = 'numeric'

        All = QtWidgets.QLineEdit()
        # actiate touch input
        All.keyboard_type = 'all'

        gl = QtWidgets.QVBoxLayout()
        gl.addWidget(txt)
        gl.addWidget(Num)
        gl.addWidget(All)
        o.setLayout(gl)
        l.addWidget(o)

        l.addWidget(buttonBox)

    def end(self):
        sys.exit()

if __name__ == '__main__':
    import sys
    app = QtWidgets.QApplication([])
    #ExampleWidget().show()
    test = ExampleDialog()
    test.show()
    #numEdit().show()
    sys.exit(app.exec_())
