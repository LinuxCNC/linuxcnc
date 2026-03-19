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
from PyQt6.QtCore import Qt

# Set up logging
from qtvcp import logger

LOG = logger.getLogger(__name__)


# Set the log level for this module
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

def key_pressed(event):
    """ Handle key presses (on any window) """

    keynum = int(event.key())
    keys = {
        Qt.Key.Key_Escape: "Key_Escape",
        Qt.Key.Key_Tab: "Key_Tab",
        Qt.Key.Key_Backtab: "Key_Backtab",
        Qt.Key.Key_Backspace: "Key_Backspace",
        Qt.Key.Key_Return: "Key_Return",
        Qt.Key.Key_Enter: "Key_Enter",
        Qt.Key.Key_Insert: "Key_Insert",
        Qt.Key.Key_Delete: "Key_Delete",
        Qt.Key.Key_Pause: "Key_Pause",
        Qt.Key.Key_Print: "Key_Print",
        Qt.Key.Key_SysReq: "Key_SysReq",
        Qt.Key.Key_Clear: "Key_Clear",
        Qt.Key.Key_Home: "Key_Home",
        Qt.Key.Key_End: "Key_End",
        Qt.Key.Key_Left: "Key_Left",
        Qt.Key.Key_Up: "Key_Up",
        Qt.Key.Key_Right: "Key_Right",
        Qt.Key.Key_Down: "Key_Down",
        Qt.Key.Key_PageUp: "Key_PageUp",
        Qt.Key.Key_PageDown: "Key_PageDown",
        Qt.Key.Key_Shift: "Key_Shift",
        Qt.Key.Key_Control: "Key_Control",
        Qt.Key.Key_Meta: "Key_Meta",
        # Qt.Key.Key_Alt: "Key_Alt",
        Qt.Key.Key_AltGr: "Key_AltGr",
        Qt.Key.Key_CapsLock: "Key_CapsLock",
        Qt.Key.Key_NumLock: "Key_NumLock",
        Qt.Key.Key_ScrollLock: "Key_ScrollLock",
        Qt.Key.Key_F1: "Key_F1",
        Qt.Key.Key_F2: "Key_F2",
        Qt.Key.Key_F3: "Key_F3",
        Qt.Key.Key_F4: "Key_F4",
        Qt.Key.Key_F5: "Key_F5",
        Qt.Key.Key_F6: "Key_F6",
        Qt.Key.Key_F7: "Key_F7",
        Qt.Key.Key_F8: "Key_F8",
        Qt.Key.Key_F9: "Key_F9",
        Qt.Key.Key_F10: "Key_F10",
        Qt.Key.Key_F11: "Key_F11",
        Qt.Key.Key_F12: "Key_F12",
        Qt.Key.Key_F13: "Key_F13",
        Qt.Key.Key_F14: "Key_F14",
        Qt.Key.Key_F15: "Key_F15",
        Qt.Key.Key_F16: "Key_F16",
        Qt.Key.Key_F17: "Key_F17",
        Qt.Key.Key_F18: "Key_F18",
        Qt.Key.Key_F19: "Key_F19",
        Qt.Key.Key_F20: "Key_F20",
        Qt.Key.Key_F21: "Key_F21",
        Qt.Key.Key_F22: "Key_F22",
        Qt.Key.Key_F23: "Key_F23",
        Qt.Key.Key_F24: "Key_F24",
        Qt.Key.Key_F25: "Key_F25",
        Qt.Key.Key_F26: "Key_F26",
        Qt.Key.Key_F27: "Key_F27",
        Qt.Key.Key_F28: "Key_F28",
        Qt.Key.Key_F29: "Key_F29",
        Qt.Key.Key_F30: "Key_F30",
        Qt.Key.Key_F31: "Key_F31",
        Qt.Key.Key_F32: "Key_F32",
        Qt.Key.Key_F33: "Key_F33",
        Qt.Key.Key_F34: "Key_F34",
        Qt.Key.Key_F35: "Key_F35",
        Qt.Key.Key_Super_L: "Key_Super_L",
        Qt.Key.Key_Super_R: "Key_Super_R",
        Qt.Key.Key_Menu: "Key_Menu",
        Qt.Key.Key_Hyper_L: "Key_HYPER_L",
        Qt.Key.Key_Hyper_R: "Key_Hyper_R",
        Qt.Key.Key_Help: "Key_Help",
        Qt.Key.Key_Direction_L: "Key_Direction_L",
        Qt.Key.Key_Direction_R: "Key_Direction_R",
        Qt.Key.Key_Space: "Key_Space",
        Qt.Key.Key_Any: "Key_Any",
        Qt.Key.Key_Exclam: "Key_Exclam",
        Qt.Key.Key_QuoteDbl: "Key_QuoteDdl",
        Qt.Key.Key_NumberSign: "Key_NumberSign",
        Qt.Key.Key_Dollar: "Key_Dollar",
        Qt.Key.Key_Percent: "Key_Percent",
        Qt.Key.Key_Ampersand: "Key_Ampersand",
        Qt.Key.Key_Apostrophe: "Key_Apostrophe",
        Qt.Key.Key_ParenLeft: "Key_ParenLeft",
        Qt.Key.Key_ParenRight: "Key_ParenRight",
        Qt.Key.Key_Asterisk: "Key_Asterisk",
        Qt.Key.Key_Plus: "Key_Plus",
        Qt.Key.Key_Comma: "Key_Comma",
        Qt.Key.Key_Minus: "Key_Minus",
        Qt.Key.Key_Period: "Key_Period",
        Qt.Key.Key_Slash: "Key_Slash",
        Qt.Key.Key_0: "Key_0",
        Qt.Key.Key_1: "Key_1",
        Qt.Key.Key_2: "Key_2",
        Qt.Key.Key_3: "Key_3",
        Qt.Key.Key_4: "Key_4",
        Qt.Key.Key_5: "Key_5",
        Qt.Key.Key_6: "Key_6",
        Qt.Key.Key_7: "Key_7",
        Qt.Key.Key_8: "Key_8",
        Qt.Key.Key_9: "Key_9",
        Qt.Key.Key_Colon: "Key_Colon",
        Qt.Key.Key_Semicolon: "Key_Semicolon",
        Qt.Key.Key_Less: "Key_Less",
        Qt.Key.Key_Equal: "Key_Equal",
        Qt.Key.Key_Greater: "Key_Greater",
        Qt.Key.Key_Question: "Key_Question",
        Qt.Key.Key_At: "Key_At",
        Qt.Key.Key_A: "Key_",
        Qt.Key.Key_B: "Key_",
        Qt.Key.Key_C: "Key_",
        Qt.Key.Key_D: "Key_",
        Qt.Key.Key_E: "Key_",
        Qt.Key.Key_F: "Key_",
        Qt.Key.Key_G: "Key_",
        Qt.Key.Key_H: "Key_",
        Qt.Key.Key_I: "Key_",
        Qt.Key.Key_J: "Key_",
        Qt.Key.Key_K: "Key_",
        Qt.Key.Key_L: "Key_",
        Qt.Key.Key_M: "Key_",
        Qt.Key.Key_N: "Key_",
        Qt.Key.Key_O: "Key_",
        Qt.Key.Key_P: "Key_",
        Qt.Key.Key_Q: "Key_",
        Qt.Key.Key_R: "Key_",
        Qt.Key.Key_S: "Key_",
        Qt.Key.Key_T: "Key_",
        Qt.Key.Key_U: "Key_",
        Qt.Key.Key_V: "Key_",
        Qt.Key.Key_W: "Key_",
        Qt.Key.Key_X: "Key_",
        Qt.Key.Key_Y: "Key_",
        Qt.Key.Key_Z: "Key_",
        Qt.Key.Key_BracketLeft: "Key_BracketLeft",
        Qt.Key.Key_Backslash: "Key_Backslash",
        Qt.Key.Key_BracketRight: "Key_BracketRight",
        Qt.Key.Key_AsciiCircum: "Key_AsciiCircum",
        Qt.Key.Key_Underscore: "Key_Underscore",
        Qt.Key.Key_QuoteLeft: "Key_QuoteLeft",
        Qt.Key.Key_BraceLeft: "Key_BraceLeft",
        Qt.Key.Key_Bar: "Key_Bar",
        Qt.Key.Key_BraceRight: "Key_BraceRight",
        Qt.Key.Key_AsciiTilde: "Key_AsciiTilde",

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
                if LOG.getEffectiveLevel() == LOG.VERBOSE:
                    formatted_lines = traceback.format_exc().splitlines()
                    for i in range(5, len(formatted_lines)):
                        print(formatted_lines[i])
        except Exception as e:
            if is_pressed:
                formatted_lines = traceback.format_exc().splitlines()
                # LOG.debug('Exception in KEYBINDING:', exc_info=e)
                print("""Key Binding Error for key '%s' calling function: %s in handler file:""" % (
                key, self.convert(event)))
                if LOG.getEffectiveLevel() == LOG.VERBOSE:
                    for i in range(5, len(formatted_lines)):
                        print(formatted_lines[i])
        event.accept()
        return True
