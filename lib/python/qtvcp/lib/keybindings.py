# Qtscreen is Copyright (c) 2017  Chris Morley
#
# QTscreen is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Gscreen is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# This holds/converts the generic function keyword to the actual function call name
# it returns this name so Qtscreen can call the function to actually do something.
# you can add or change these

import traceback
from PyQt5.QtCore import Qt

# Set up logging
from qtvcp import logger

LOG = logger.getLogger(__name__)


# Set the log level for this module
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

def key_pressed(event):
    """ Handle key presses (on any window) """

    keynum = int(event.key())
    keys = {
        Qt.Key_Escape: "Key_Escape",
        Qt.Key_Tab: "Key_Tab",
        Qt.Key_Backtab: "Key_Backtab",
        Qt.Key_Backspace: "Key_Backspace",
        Qt.Key_Return: "Key_Return",
        Qt.Key_Enter: "Key_Enter",
        Qt.Key_Insert: "Key_Insert",
        Qt.Key_Delete: "Key_Delete",
        Qt.Key_Pause: "Key_Pause",
        Qt.Key_Print: "Key_Print",
        Qt.Key_SysReq: "Key_SysReq",
        Qt.Key_Clear: "Key_Clear",
        Qt.Key_Home: "Key_Home",
        Qt.Key_End: "Key_End",
        Qt.Key_Left: "Key_Left",
        Qt.Key_Up: "Key_Up",
        Qt.Key_Right: "Key_Right",
        Qt.Key_Down: "Key_Down",
        Qt.Key_PageUp: "Key_PageUp",
        Qt.Key_PageDown: "Key_PageDown",
        Qt.Key_Shift: "Key_Shift",
        Qt.Key_Control: "Key_Control",
        Qt.Key_Meta: "Key_Meta",
        # Qt.Key_Alt: "Key_Alt",
        Qt.Key_AltGr: "Key_AltGr",
        Qt.Key_CapsLock: "Key_CapsLock",
        Qt.Key_NumLock: "Key_NumLock",
        Qt.Key_ScrollLock: "Key_ScrollLock",
        Qt.Key_F1: "Key_F1",
        Qt.Key_F2: "Key_F2",
        Qt.Key_F3: "Key_F3",
        Qt.Key_F4: "Key_F4",
        Qt.Key_F5: "Key_F5",
        Qt.Key_F6: "Key_F6",
        Qt.Key_F7: "Key_F7",
        Qt.Key_F8: "Key_F8",
        Qt.Key_F9: "Key_F9",
        Qt.Key_F10: "Key_F10",
        Qt.Key_F11: "Key_F11",
        Qt.Key_F12: "Key_F12",
        Qt.Key_F13: "Key_F13",
        Qt.Key_F14: "Key_F14",
        Qt.Key_F15: "Key_F15",
        Qt.Key_F16: "Key_F16",
        Qt.Key_F17: "Key_F17",
        Qt.Key_F18: "Key_F18",
        Qt.Key_F19: "Key_F19",
        Qt.Key_F20: "Key_F20",
        Qt.Key_F21: "Key_F21",
        Qt.Key_F22: "Key_F22",
        Qt.Key_F23: "Key_F23",
        Qt.Key_F24: "Key_F24",
        Qt.Key_F25: "Key_F25",
        Qt.Key_F26: "Key_F26",
        Qt.Key_F27: "Key_F27",
        Qt.Key_F28: "Key_F28",
        Qt.Key_F29: "Key_F29",
        Qt.Key_F30: "Key_F30",
        Qt.Key_F31: "Key_F31",
        Qt.Key_F32: "Key_F32",
        Qt.Key_F33: "Key_F33",
        Qt.Key_F34: "Key_F34",
        Qt.Key_F35: "Key_F35",
        Qt.Key_Super_L: "Key_Super_L",
        Qt.Key_Super_R: "Key_Super_R",
        Qt.Key_Menu: "Key_Menu",
        Qt.Key_Hyper_L: "Key_HYPER_L",
        Qt.Key_Hyper_R: "Key_Hyper_R",
        Qt.Key_Help: "Key_Help",
        Qt.Key_Direction_L: "Key_Direction_L",
        Qt.Key_Direction_R: "Key_Direction_R",
        Qt.Key_Space: "Key_Space",
        Qt.Key_Any: "Key_Any",
        Qt.Key_Exclam: "Key_Exclam",
        Qt.Key_QuoteDbl: "Key_QuoteDdl",
        Qt.Key_NumberSign: "Key_NumberSign",
        Qt.Key_Dollar: "Key_Dollar",
        Qt.Key_Percent: "Key_Percent",
        Qt.Key_Ampersand: "Key_Ampersand",
        Qt.Key_Apostrophe: "Key_Apostrophe",
        Qt.Key_ParenLeft: "Key_ParenLeft",
        Qt.Key_ParenRight: "Key_ParenRight",
        Qt.Key_Asterisk: "Key_Asterisk",
        Qt.Key_Plus: "Key_Plus",
        Qt.Key_Comma: "Key_Comma",
        Qt.Key_Minus: "Key_Minus",
        Qt.Key_Period: "Key_Period",
        Qt.Key_Slash: "Key_Slash",
        Qt.Key_0: "Key_0",
        Qt.Key_1: "Key_1",
        Qt.Key_2: "Key_2",
        Qt.Key_3: "Key_3",
        Qt.Key_4: "Key_4",
        Qt.Key_5: "Key_5",
        Qt.Key_6: "Key_6",
        Qt.Key_7: "Key_7",
        Qt.Key_8: "Key_8",
        Qt.Key_9: "Key_9",
        Qt.Key_Colon: "Key_Colon",
        Qt.Key_Semicolon: "Key_Semicolon",
        Qt.Key_Less: "Key_Less",
        Qt.Key_Equal: "Key_Equal",
        Qt.Key_Greater: "Key_Greater",
        Qt.Key_Question: "Key_Question",
        Qt.Key_At: "Key_At",
        Qt.Key_A: "Key_",
        Qt.Key_B: "Key_",
        Qt.Key_C: "Key_",
        Qt.Key_D: "Key_",
        Qt.Key_E: "Key_",
        Qt.Key_F: "Key_",
        Qt.Key_G: "Key_",
        Qt.Key_H: "Key_",
        Qt.Key_I: "Key_",
        Qt.Key_J: "Key_",
        Qt.Key_K: "Key_",
        Qt.Key_L: "Key_",
        Qt.Key_M: "Key_",
        Qt.Key_N: "Key_",
        Qt.Key_O: "Key_",
        Qt.Key_P: "Key_",
        Qt.Key_Q: "Key_",
        Qt.Key_R: "Key_",
        Qt.Key_S: "Key_",
        Qt.Key_T: "Key_",
        Qt.Key_U: "Key_",
        Qt.Key_V: "Key_",
        Qt.Key_W: "Key_",
        Qt.Key_X: "Key_",
        Qt.Key_Y: "Key_",
        Qt.Key_Z: "Key_",
        Qt.Key_BracketLeft: "Key_BracketLeft",
        Qt.Key_Backslash: "Key_Backslash",
        Qt.Key_BracketRight: "Key_BracketRight",
        Qt.Key_AsciiCircum: "Key_AsciiCircum",
        Qt.Key_Underscore: "Key_Underscore",
        Qt.Key_QuoteLeft: "Key_QuoteLeft",
        Qt.Key_BraceLeft: "Key_BraceLeft",
        Qt.Key_Bar: "Key_Bar",
        Qt.Key_BraceRight: "Key_BraceRight",
        Qt.Key_AsciiTilde: "Key_AsciiTilde",

    }

    char = keys.get(keynum, '<unknown - {}>'.format(keynum))

    mods = []
    if event.modifiers() & Qt.AltModifier:
        mods.append("Alt")
    # For letters we want upper and lower in the keyname
    # if control was used the keyname uses an upper letter
    # we will also not add +shift to the keyname
    if (keynum >= 65) and (keynum <= 90):
        if event.modifiers() & Qt.ControlModifier:
            char = char + chr(event.key())
        else:
            char = char + event.text()

    txt = "+".join(mods) + (mods and "+" or "") + char
    return txt


# list of keyname, function name, optional value
# value can be used when calling one function with multiple keys
class _Keycalls:
    def __init__(self):
        self.Key_F1 = ['on_keycall_ESTOP', None]
        self.Key_F2 = ['on_keycall_POWER', None]
        self.Key_Home = ['on_keycall_HOME', None]
        self.Key_Escape = ['on_keycall_ABORT', None]
        self.Key_Left = ['on_keycall_XNEG', None]
        self.Key_Right = ['on_keycall_XPOS', None]
        self.Key_Up = ['on_keycall_YPOS', None]
        self.Key_Down = ['on_keycall_YNEG', None]
        self.Key_PageUp = ['on_keycall_ZPOS', None]
        self.Key_PageDown = ['on_keycall_ZNEG', None]
        self.Key_BracketLeft = ['on_keycall_APOS', None]
        self.Key_BracketRight = ['on_keycall_ANEG', None]
        self.Key_BraceLeft = ['on_keycall_APOS', None]
        self.Key_BraceRight = ['on_keycall_ANEG', None]

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


# These is the public methods for key conversion to function call name.
class Keylookup:
    def __init__(self):
        self.keycall = _Keycalls()

    # This looks up the function named based from the keynumber and then calls
    # the function in the handler file
    def call(self, handler_instance, event, state, shift, cntrl):
        try:
            function_name, value = self.convert(event)
        except TypeError:
            raise NameError('No KeyCall binding defiimport tracebackned for %s' % key_pressed(event))
        if value is None:
            handler_instance[function_name](event, state, shift, cntrl)
        else:
            # has option value so function signature is different
            handler_instance[function_name](event, state, shift, cntrl, value)
        return True

    # convert a Qt event to a function name
    def convert(self, event):
        try:
            b = key_pressed(event)
            f, v = self.keycall[b]
            return f, v
        except:
            raise NameError("no function name conversion for QT Event: '{}'".format(b))

    # get a function name from a keyname
    def get_call(self, binding):
        try:
            a, v = self.keycall[binding]
            return a, v
        except:
            raise NameError("No key function call")

    # add a keyname and function name
    def add_call(self, binding, function, value=None):
        try:
            self.keycall[binding] = [function, value]
        except:
            raise NameError("Binding %s could not be added" % binding)

    # remove a keyname binding
    def del_call(self, binding):
        try:
            delattr(self.keycall, binding)
        except AttributeError:
            raise NameError("Binding %s could not be found" % binding)
        except Exception as e:
            raise NameError("Binding %s could not be removed: %s" % (binding, e))

    def manage_function_calls(self, handler, event, is_pressed, key, shift, cntrl):
        try:
            b = self.call(handler, event, is_pressed, shift, cntrl)
        except NameError as e:
            if is_pressed:
                LOG.debug('Exception in KEYBINDING: {}'.format(e))
        except Exception as e:
            if is_pressed:
                formatted_lines = traceback.format_exc().splitlines()
                # LOG.debug('Exception in KEYBINDING:', exc_info=e)
                print("""Key Binding Error for key '%s' calling function: %s in handler file:""" % (
                key, self.convert(event)))
                for i in range(5, len(formatted_lines)):
                    print(formatted_lines[i])
        event.accept()
        return True
