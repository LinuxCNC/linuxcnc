#!/usr/bin/env python3
import sys
import os
from PyQt5 import QtCore, QtGui, QtWidgets, uic
from PyQt5.QtCore import Qt, QEvent

from qtvcp.core import Info
from qtvcp import logger
LOG = logger.getLogger(__name__)

INFO = Info()

class VirtualKeyboard(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(VirtualKeyboard, self).__init__(parent)
        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'virtual_keyboard.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

        self.NO_ORD_KEY_LIST = list()
        self.NO_ORD_KEY_LIST.append(Qt.Key_Backspace)
        self.NO_ORD_KEY_LIST.append(Qt.Key_Enter)
        self.NO_ORD_KEY_LIST.append(Qt.Key_Escape)
        self.NO_ORD_KEY_LIST.append(Qt.Key_Delete)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F1)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F2)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F3)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F4)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F5)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F6)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F7)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F8)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F9)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F10)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F11)
        self.NO_ORD_KEY_LIST.append(Qt.Key_F12)

        self.letter_list = "abcdefghijklmnopqrstuvwxyz"
        self.number_list = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'minus', 'equal']
        self.shift_number = [')', '!', '@', '#', '$', '%', '^', '&&', '*', '(']
        self.special_list = {
            'less': Qt.Key_Less,
            'greater': Qt.Key_Greater,
            'question': Qt.Key_Question,
            'colon': Qt.Key_Colon,
            'leftbrace': Qt.Key_BraceLeft,
            'rightbrace': Qt.Key_BraceRight,
            'doublequote': Qt.Key_QuoteDbl,
            'pipe': Qt.Key_Bar,
            'underscore': Qt.Key_Underscore,
            'plus': Qt.Key_Plus,
            'exclamation': Qt.Key_Exclam,
            'at': Qt.Key_At,
            'pound': Qt.Key_NumberSign,
            'dollar': Qt.Key_Dollar,
            'percent': Qt.Key_Percent,
            'carat': Qt.Key_AsciiCircum,
            'ampersand': Qt.Key_Ampersand,
            'asterisk': Qt.Key_Asterisk,
            'leftparenth': Qt.Key_ParenLeft,
            'rightparenth': Qt.Key_ParenRight,
            'space': Qt.Key_Space
            }

        self.function_keys = {
            '0': Qt.Key_F10,
            '1': Qt.Key_F1,
            '2': Qt.Key_F2,
            '3': Qt.Key_F3,
            '4': Qt.Key_F4,
            '5': Qt.Key_F5,
            '6': Qt.Key_F6,
            '7': Qt.Key_F7,
            '8': Qt.Key_F8,
            '9': Qt.Key_F9,
            'minus': Qt.Key_F11,
            'equal': Qt.Key_F12
            }

        self.init_letters()
        self.init_numbers()
        self.init_special()

    # control button group
        self.btn_back.KEY_CHAR = Qt.Key_Backspace
        self.btn_delete.KEY_CHAR = Qt.Key_Delete
        self.btn_enter.KEY_CHAR = Qt.Key_Enter

        self.letters_buttonGroup.buttonClicked.connect(self.letter_clicked)
        self.numbers_buttonGroup.buttonClicked.connect(self.special_clicked)
        self.special_buttonGroup.buttonClicked.connect(self.special_clicked)
        self.control_buttonGroup.buttonClicked.connect(self.button_clicked)
        
    def init_letters(self):
        for val in self.letter_list:
            self['btn_' + val].setText(val)
            self['btn_' + val].KEY_CHAR = ord(val)

    def init_numbers(self):
        for i, val in enumerate(self.number_list[:-2]):
            self['btn_' + val].setText(self.shift_number[i] + '\n' + val)
            self['btn_' + val].KEY_CHAR = ord(val)
        self.btn_minus.setText('_\n-')
        self.btn_minus.KEY_CHAR = Qt.Key_Minus
        self.btn_equal.setText('+\n=')
        self.btn_equal.KEY_CHAR = Qt.Key_Equal

    def init_special(self):
        self.btn_comma.KEY_CHAR = Qt.Key_Comma
        self.btn_period.KEY_CHAR = Qt.Key_Period
        self.btn_semicolon.KEY_CHAR = Qt.Key_Semicolon
        self.btn_slash.KEY_CHAR = Qt.Key_Slash
        self.btn_backslash.KEY_CHAR = Qt.Key_Backslash
        self.btn_apostrophe.KEY_CHAR = Qt.Key_Apostrophe
        self.btn_left_bracket.KEY_CHAR = Qt.Key_BracketLeft
        self.btn_right_bracket.KEY_CHAR = Qt.Key_BracketRight
        self.btn_space.KEY_CHAR = Qt.Key_Space

    def letter_clicked(self, btn):
        if self.btn_shift.isChecked():
            btn.KEY_CHAR = ord(btn.text().upper())
        self.button_clicked(btn)
        btn.KEY_CHAR = ord(btn.text())

    def special_clicked(self, btn):
        key_save = btn.KEY_CHAR
        if self.btn_shift.isChecked() and not self.btn_function.isChecked():
            key = btn.property('upper')
            btn.KEY_CHAR = self.special_list.get(key)
        self.button_clicked(btn)
        btn.KEY_CHAR = key_save

    def button_clicked(self, btn):
        fw = QtWidgets.QApplication.focusWidget()
        if fw is None: return
        if btn.KEY_CHAR in self.NO_ORD_KEY_LIST:
            keyPress = QtGui.QKeyEvent(QEvent.KeyPress, btn.KEY_CHAR, Qt.NoModifier, '')
        else:
            if self.btn_shift.isChecked():
                modifier = Qt.ShiftModifier
                self.btn_shift.setChecked(False)
            else:
                modifier = Qt.NoModifier
            keyPress = QtGui.QKeyEvent(QEvent.KeyPress, btn.KEY_CHAR, modifier, chr(btn.KEY_CHAR))
        try:
            QtWidgets.QApplication.sendEvent(fw, keyPress)
        except:
            pass

    def caps_lock_clicked(self, state):
        if state:
            for val in self.letter_list:
                self['btn_' + val].setText(val.upper())
                self['btn_' + val].KEY_CHAR = ord(val.upper())
        else:
            self.init_letters()

    def function_clicked(self, state):
        if state:
            for val in self.number_list:
                alt_txt = self['btn_' + val].property('fcode')
                self['btn_' + val].setText(alt_txt)
            for key, val in list(self.function_keys.items()):
                self['btn_' + key].KEY_CHAR = val
        else:
            self.init_numbers()

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)
        
    #############################
    # Testing                   #
    #############################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    app = QtWidgets.QApplication(sys.argv)
    w = VirtualKeyboard()
    w.show()
    sys.exit( app.exec_() )

