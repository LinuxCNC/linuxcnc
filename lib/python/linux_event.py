#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import struct, fcntl, array, os, select, glob, fnmatch, time, re

size_shift = 16
def SZ(a,b): return a | (b<<size_shift)

def get_name(f):
    return fcntl.ioctl(f, SZ(EVIOCGNAME, 1024), '\0' * 1024)

def get_phys(f):
    return fcntl.ioctl(f, SZ(EVIOCGPHYS, 1024), '\0' * 1024)

# get_uniq seems to return -ENOENT on all my devices
def get_uniq(f):
    return fcntl.ioctl(f, SZ(EVIOCGUNIQ, 1024), '\0' * 1024)

def get_bits(f, o=0):
    o = EV_invert.get(o, o)
    if o == 'EV_SYN': fmap, rmap, prefix = EV_invert, EV, "SYN"
    elif o == 'EV_KEY': fmap, rmap, prefix = KEYBTN_invert, KEY, "KEY"
    elif o == 'EV_LED': fmap, rmap, prefix = LED_invert, LED, "LED"
    elif o == 'EV_ABS': fmap, rmap, prefix = ABS_invert, ABS, "ABS"
    elif o == 'EV_REL': fmap, rmap, prefix = REL_invert, REL, "REL"
    else: raise ValueError, "get_bits: unexpected map %s" % o

    sz = max(fmap) + 1
    a = fcntl.ioctl(f, SZ(EVIOCGBIT+EV[o], sz), '\0' * ((sz+7)/8))
    ret = set()
    for j, ch in enumerate(a):
	ch = ord(ch)
	for i in range(8):
	    b = 1<<i
	    if ch & b:
		k = j*8+i
		if k not in fmap:
		    name = "%s_%d" % (prefix, k)
		    fmap[k] = name
		    rmap[name] = k
		ret.add(fmap[k])
    return ret

def get_keys(f):
    ret = set()

class InputId:
    format = "HHHH"
    size = struct.calcsize(format)
    def __init__(self, buf):
	self.bustype, self.vendor, self.product, self.version = \
	    struct.unpack(self.format, buf)
	self.bustype = BUS_invert.get(self.bustype, self.bustype)
    @classmethod
    def get(cls, fd):
	buf = fcntl.ioctl(fd, EVIOCGID, '\0' * cls.size)
	return cls(buf)

    def __repr__(self):
	return "<InputId: %s %04x:%04x version=%d>" % (
	    self.bustype, self.vendor, self.product, self.version)
class AbsInfo:
    format = "iiiii"
    size = struct.calcsize(format)
    def __init__(self, *args):
	if len(args) == 1:
	    self.value, self.minimum, self.maximum, self.fuzz, self.flat = \
		struct.unpack(self.format, args[0])
	else:
	    self.value, self.minimum, self.maximum, self.fuzz, self.flat = args
    def __repr__(self):
	return "<AbsInfo: %8d %8d %8d %8d %8d>" % (
	    self.value, self.minimum, self.maximum, self.fuzz, self.flat)
	
    @classmethod
    def get(cls, fd, idx):
	idx = ABS.get(idx, idx)
	buf = fcntl.ioctl(fd, EVIOCGABS + idx, '\0' * cls.size)
	return cls(buf)

    def set(self, fd, idx):
	idx = ABS.get(idx, idx)
	buf = struct.pack(format, self.value, self.minimum, self.maximum, 
			    self.fuzz, self.flat)
	fcntl.ioctl(fd, EVIOCSABS + idx, buf)

def mapcode(type, code):
    if type == EV_REL: return REL_invert.get(code, code)
    if type == EV_ABS: return ABS_invert.get(code, code)
    if type == EV_KEY:
	if code in KEY_invert:
	    return KEY_invert[code]
	return BTN_invert.get(code, code)
    if type == EV_REL: return REL_invert.get(code, code)

class Event:
    format = "llHHi"
    size = struct.calcsize(format)

    def __init__(self, buf):
	data = struct.unpack(self.format, buf)
	self.time = data[0] + data[1] * 1e-9
	self.type = EV_invert[data[2]]
	self.code = data[3]
	self.value = data[4]

    @classmethod
    def read(cls, f):
	if isinstance(f, int):
	    buf = os.read(f, cls.size)
	else:
	    buf = f.read(size)
	return cls(buf)

    @classmethod
    def write(cls, f, *args):
	if len(args) == 3:
	    event_time = time.time()
	    time_seconds = int(event_time)
	    time_ns = int((event_time - time_seconds) * 1e9)
	    type, code, value = args
	elif len(args) == 4:
	    event_time, type, code, value = args
	    time_seconds = int(event_time)
	    time_ns = int((event_time - time_seconds) * 1e9)
	else:
	    time_seconds, time_ns, type, code, value = args
	if type == 'EV_LED' and isinstance(code, str): code = LED[code]
	type = EV.get(type, type)
	buf = struct.pack(cls.format, time_seconds, time_ns, type, code, value)
	os.write(f, buf)

    def __repr__(self):
	code = self.code
	if isinstance(code, int): code = "%04x" % code
	return "<Event %s %s %8d>" % (self.type, code, self.value)

EV = {
	'EV_SYN': 0x00,
	'EV_KEY': 0x01,
	'EV_REL': 0x02,
	'EV_ABS': 0x03,
	'EV_MSC': 0x04,
	'EV_SW': 0x05,
	'EV_LED': 0x11,
	'EV_SND': 0x12,
	'EV_REP': 0x14,
	'EV_FF': 0x15,
	'EV_PWR': 0x16,
	'EV_FF_STATUS': 0x17,
}
REL = {
	'REL_X': 0x00,
	'REL_Y': 0x01,
	'REL_Z': 0x02,
	'REL_RX': 0x03,
	'REL_RY': 0x04,
	'REL_RZ': 0x05,
	'REL_HWHEEL': 0x06,
	'REL_DIAL': 0x07,
	'REL_WHEEL': 0x08,
	'REL_MISC': 0x09,
}
KEY = {
	'KEY_RESERVED': 0,
	'KEY_ESC': 1,
	'KEY_1': 2,
	'KEY_2': 3,
	'KEY_3': 4,
	'KEY_4': 5,
	'KEY_5': 6,
	'KEY_6': 7,
	'KEY_7': 8,
	'KEY_8': 9,
	'KEY_9': 10,
	'KEY_0': 11,
	'KEY_MINUS': 12,
	'KEY_EQUAL': 13,
	'KEY_BACKSPACE': 14,
	'KEY_TAB': 15,
	'KEY_Q': 16,
	'KEY_W': 17,
	'KEY_E': 18,
	'KEY_R': 19,
	'KEY_T': 20,
	'KEY_Y': 21,
	'KEY_U': 22,
	'KEY_I': 23,
	'KEY_O': 24,
	'KEY_P': 25,
	'KEY_LEFTBRACE': 26,
	'KEY_RIGHTBRACE': 27,
	'KEY_ENTER': 28,
	'KEY_LEFTCTRL': 29,
	'KEY_A': 30,
	'KEY_S': 31,
	'KEY_D': 32,
	'KEY_F': 33,
	'KEY_G': 34,
	'KEY_H': 35,
	'KEY_J': 36,
	'KEY_K': 37,
	'KEY_L': 38,
	'KEY_SEMICOLON': 39,
	'KEY_APOSTROPHE': 40,
	'KEY_GRAVE': 41,
	'KEY_LEFTSHIFT': 42,
	'KEY_BACKSLASH': 43,
	'KEY_Z': 44,
	'KEY_X': 45,
	'KEY_C': 46,
	'KEY_V': 47,
	'KEY_B': 48,
	'KEY_N': 49,
	'KEY_M': 50,
	'KEY_COMMA': 51,
	'KEY_DOT': 52,
	'KEY_SLASH': 53,
	'KEY_RIGHTSHIFT': 54,
	'KEY_KPASTERISK': 55,
	'KEY_LEFTALT': 56,
	'KEY_SPACE': 57,
	'KEY_CAPSLOCK': 58,
	'KEY_F1': 59,
	'KEY_F2': 60,
	'KEY_F3': 61,
	'KEY_F4': 62,
	'KEY_F5': 63,
	'KEY_F6': 64,
	'KEY_F7': 65,
	'KEY_F8': 66,
	'KEY_F9': 67,
	'KEY_F10': 68,
	'KEY_NUMLOCK': 69,
	'KEY_SCROLLLOCK': 70,
	'KEY_KP7': 71,
	'KEY_KP8': 72,
	'KEY_KP9': 73,
	'KEY_KPMINUS': 74,
	'KEY_KP4': 75,
	'KEY_KP5': 76,
	'KEY_KP6': 77,
	'KEY_KPPLUS': 78,
	'KEY_KP1': 79,
	'KEY_KP2': 80,
	'KEY_KP3': 81,
	'KEY_KP0': 82,
	'KEY_KPDOT': 83,
	'KEY_ZENKAKUHANKAKU': 85,
	'KEY_102ND': 86,
	'KEY_F11': 87,
	'KEY_F12': 88,
	'KEY_RO': 89,
	'KEY_KATAKANA': 90,
	'KEY_HIRAGANA': 91,
	'KEY_HENKAN': 92,
	'KEY_KATAKANAHIRAGANA': 93,
	'KEY_MUHENKAN': 94,
	'KEY_KPJPCOMMA': 95,
	'KEY_KPENTER': 96,
	'KEY_RIGHTCTRL': 97,
	'KEY_KPSLASH': 98,
	'KEY_SYSRQ': 99,
	'KEY_RIGHTALT': 100,
	'KEY_LINEFEED': 101,
	'KEY_HOME': 102,
	'KEY_UP': 103,
	'KEY_PAGEUP': 104,
	'KEY_LEFT': 105,
	'KEY_RIGHT': 106,
	'KEY_END': 107,
	'KEY_DOWN': 108,
	'KEY_PAGEDOWN': 109,
	'KEY_INSERT': 110,
	'KEY_DELETE': 111,
	'KEY_MACRO': 112,
	'KEY_MUTE': 113,
	'KEY_VOLUMEDOWN': 114,
	'KEY_VOLUMEUP': 115,
	'KEY_POWER': 116,
	'KEY_KPEQUAL': 117,
	'KEY_KPPLUSMINUS': 118,
	'KEY_PAUSE': 119,
	'KEY_KPCOMMA': 121,
	'KEY_HANGUEL': 122,
	'KEY_HANJA': 123,
	'KEY_YEN': 124,
	'KEY_LEFTMETA': 125,
	'KEY_RIGHTMETA': 126,
	'KEY_COMPOSE': 127,
	'KEY_STOP': 128,
	'KEY_AGAIN': 129,
	'KEY_PROPS': 130,
	'KEY_UNDO': 131,
	'KEY_FRONT': 132,
	'KEY_COPY': 133,
	'KEY_OPEN': 134,
	'KEY_PASTE': 135,
	'KEY_FIND': 136,
	'KEY_CUT': 137,
	'KEY_HELP': 138,
	'KEY_MENU': 139,
	'KEY_CALC': 140,
	'KEY_SETUP': 141,
	'KEY_SLEEP': 142,
	'KEY_WAKEUP': 143,
	'KEY_FILE': 144,
	'KEY_SENDFILE': 145,
	'KEY_DELETEFILE': 146,
	'KEY_XFER': 147,
	'KEY_PROG1': 148,
	'KEY_PROG2': 149,
	'KEY_WWW': 150,
	'KEY_MSDOS': 151,
	'KEY_COFFEE': 152,
	'KEY_DIRECTION': 153,
	'KEY_CYCLEWINDOWS': 154,
	'KEY_MAIL': 155,
	'KEY_BOOKMARKS': 156,
	'KEY_COMPUTER': 157,
	'KEY_BACK': 158,
	'KEY_FORWARD': 159,
	'KEY_CLOSECD': 160,
	'KEY_EJECTCD': 161,
	'KEY_EJECTCLOSECD': 162,
	'KEY_NEXTSONG': 163,
	'KEY_PLAYPAUSE': 164,
	'KEY_PREVIOUSSONG': 165,
	'KEY_STOPCD': 166,
	'KEY_RECORD': 167,
	'KEY_REWIND': 168,
	'KEY_PHONE': 169,
	'KEY_ISO': 170,
	'KEY_CONFIG': 171,
	'KEY_HOMEPAGE': 172,
	'KEY_REFRESH': 173,
	'KEY_EXIT': 174,
	'KEY_MOVE': 175,
	'KEY_EDIT': 176,
	'KEY_SCROLLUP': 177,
	'KEY_SCROLLDOWN': 178,
	'KEY_KPLEFTPAREN': 179,
	'KEY_KPRIGHTPAREN': 180,
	'KEY_NEW': 181,
	'KEY_REDO': 182,
	'KEY_F13': 183,
	'KEY_F14': 184,
	'KEY_F15': 185,
	'KEY_F16': 186,
	'KEY_F17': 187,
	'KEY_F18': 188,
	'KEY_F19': 189,
	'KEY_F20': 190,
	'KEY_F21': 191,
	'KEY_F22': 192,
	'KEY_F23': 193,
	'KEY_F24': 194,
	'KEY_PLAYCD': 200,
	'KEY_PAUSECD': 201,
	'KEY_PROG3': 202,
	'KEY_PROG4': 203,
	'KEY_SUSPEND': 205,
	'KEY_CLOSE': 206,
	'KEY_PLAY': 207,
	'KEY_FASTFORWARD': 208,
	'KEY_BASSBOOST': 209,
	'KEY_PRINT': 210,
	'KEY_HP': 211,
	'KEY_CAMERA': 212,
	'KEY_SOUND': 213,
	'KEY_QUESTION': 214,
	'KEY_EMAIL': 215,
	'KEY_CHAT': 216,
	'KEY_SEARCH': 217,
	'KEY_CONNECT': 218,
	'KEY_FINANCE': 219,
	'KEY_SPORT': 220,
	'KEY_SHOP': 221,
	'KEY_ALTERASE': 222,
	'KEY_CANCEL': 223,
	'KEY_BRIGHTNESSDOWN': 224,
	'KEY_BRIGHTNESSUP': 225,
	'KEY_MEDIA': 226,
	'KEY_SWITCHVIDEOMODE': 227,
	'KEY_KBDILLUMTOGGLE': 228,
	'KEY_KBDILLUMDOWN': 229,
	'KEY_KBDILLUMUP': 230,
	'KEY_SEND': 231,
	'KEY_REPLY': 232,
	'KEY_FORWARDMAIL': 233,
	'KEY_SAVE': 234,
	'KEY_DOCUMENTS': 235,
	'KEY_UNKNOWN': 240,
        'KEY_VIDEO_NEXT':	241,
        'KEY_VIDEO_PREV':	242,
        'KEY_BRIGHTNESS_CYCLE':	243,
        'KEY_BRIGHTNESS_ZERO':	244,
        'KEY_DISPLAY_OFF':	245,
        'KEY_WIMAX':		246,
        'KEY_RFKILL':		247,

	'KEY_OK': 0x160,
	'KEY_SELECT': 0x161,
	'KEY_GOTO': 0x162,
	'KEY_CLEAR': 0x163,
	'KEY_POWER2': 0x164,
	'KEY_OPTION': 0x165,
	'KEY_INFO': 0x166,
	'KEY_TIME': 0x167,
	'KEY_VENDOR': 0x168,
	'KEY_ARCHIVE': 0x169,
	'KEY_PROGRAM': 0x16a,
	'KEY_CHANNEL': 0x16b,
	'KEY_FAVORITES': 0x16c,
	'KEY_EPG': 0x16d,
	'KEY_PVR': 0x16e,
	'KEY_MHP': 0x16f,
	'KEY_LANGUAGE': 0x170,
	'KEY_TITLE': 0x171,
	'KEY_SUBTITLE': 0x172,
	'KEY_ANGLE': 0x173,
	'KEY_ZOOM': 0x174,
	'KEY_MODE': 0x175,
	'KEY_KEYBOARD': 0x176,
	'KEY_SCREEN': 0x177,
	'KEY_PC': 0x178,
	'KEY_TV': 0x179,
	'KEY_TV2': 0x17a,
	'KEY_VCR': 0x17b,
	'KEY_VCR2': 0x17c,
	'KEY_SAT': 0x17d,
	'KEY_SAT2': 0x17e,
	'KEY_CD': 0x17f,
	'KEY_TAPE': 0x180,
	'KEY_RADIO': 0x181,
	'KEY_TUNER': 0x182,
	'KEY_PLAYER': 0x183,
	'KEY_TEXT': 0x184,
	'KEY_DVD': 0x185,
	'KEY_AUX': 0x186,
	'KEY_MP3': 0x187,
	'KEY_AUDIO': 0x188,
	'KEY_VIDEO': 0x189,
	'KEY_DIRECTORY': 0x18a,
	'KEY_LIST': 0x18b,
	'KEY_MEMO': 0x18c,
	'KEY_CALENDAR': 0x18d,
	'KEY_RED': 0x18e,
	'KEY_GREEN': 0x18f,
	'KEY_YELLOW': 0x190,
	'KEY_BLUE': 0x191,
	'KEY_CHANNELUP': 0x192,
	'KEY_CHANNELDOWN': 0x193,
	'KEY_FIRST': 0x194,
	'KEY_LAST': 0x195,
	'KEY_AB': 0x196,
	'KEY_NEXT': 0x197,
	'KEY_RESTART': 0x198,
	'KEY_SLOW': 0x199,
	'KEY_SHUFFLE': 0x19a,
	'KEY_BREAK': 0x19b,
	'KEY_PREVIOUS': 0x19c,
	'KEY_DIGITS': 0x19d,
	'KEY_TEEN': 0x19e,
	'KEY_TWEN': 0x19f,
	'KEY_DEL_EOL': 0x1c0,
	'KEY_DEL_EOS': 0x1c1,
	'KEY_INS_LINE': 0x1c2,
	'KEY_DEL_LINE': 0x1c3,
	'KEY_FN': 0x1d0,
	'KEY_FN_ESC': 0x1d1,
	'KEY_FN_F1': 0x1d2,
	'KEY_FN_F2': 0x1d3,
	'KEY_FN_F3': 0x1d4,
	'KEY_FN_F4': 0x1d5,
	'KEY_FN_F5': 0x1d6,
	'KEY_FN_F6': 0x1d7,
	'KEY_FN_F7': 0x1d8,
	'KEY_FN_F8': 0x1d9,
	'KEY_FN_F9': 0x1da,
	'KEY_FN_F10': 0x1db,
	'KEY_FN_F11': 0x1dc,
	'KEY_FN_F12': 0x1dd,
	'KEY_FN_1': 0x1de,
	'KEY_FN_2': 0x1df,
	'KEY_FN_D': 0x1e0,
	'KEY_FN_E': 0x1e1,
	'KEY_FN_F': 0x1e2,
	'KEY_FN_S': 0x1e3,
	'KEY_FN_B': 0x1e4,
        'KEY_BRL_DOT1': 		0x1f1,
        'KEY_BRL_DOT2': 		0x1f2,
        'KEY_BRL_DOT3': 		0x1f3,
        'KEY_BRL_DOT4': 		0x1f4,
        'KEY_BRL_DOT5': 		0x1f5,
        'KEY_BRL_DOT6': 		0x1f6,
        'KEY_BRL_DOT7': 		0x1f7,
        'KEY_BRL_DOT8': 		0x1f8,
        'KEY_BRL_DOT9': 		0x1f9,
        'KEY_BRL_DOT10': 		0x1fa,
        'KEY_NUMERIC_0': 		0x200,
        'KEY_NUMERIC_1': 		0x201,
        'KEY_NUMERIC_2': 		0x202,
        'KEY_NUMERIC_3': 		0x203,
        'KEY_NUMERIC_4': 		0x204,
        'KEY_NUMERIC_5': 		0x205,
        'KEY_NUMERIC_6': 		0x206,
        'KEY_NUMERIC_7': 		0x207,
        'KEY_NUMERIC_8': 		0x208,
        'KEY_NUMERIC_9': 		0x209,
        'KEY_NUMERIC_STAR': 	0x20a,
        'KEY_NUMERIC_POUND': 	0x20b,
        'KEY_CAMERA_FOCUS': 	0x210,
        'KEY_WPS_BUTTON': 	0x211,
        'KEY_TOUCHPAD_TOGGLE': 	0x212,
        'KEY_TOUCHPAD_ON': 	0x213,
        'KEY_TOUCHPAD_OFF': 	0x214,
        'KEY_CAMERA_ZOOMIN': 	0x215,
        'KEY_CAMERA_ZOOMOUT': 	0x216,
        'KEY_CAMERA_UP': 	0x217,
        'KEY_CAMERA_DOWN': 	0x218,
        'KEY_CAMERA_LEFT': 	0x219,
        'KEY_CAMERA_RIGHT': 	0x21a,
}
BTN = {
	'BTN_MISC': 0x100,
	'BTN_0': 0x100,
	'BTN_1': 0x101,
	'BTN_2': 0x102,
	'BTN_3': 0x103,
	'BTN_4': 0x104,
	'BTN_5': 0x105,
	'BTN_6': 0x106,
	'BTN_7': 0x107,
	'BTN_8': 0x108,
	'BTN_9': 0x109,
	'BTN_10': 0x10a,
	'BTN_11': 0x10b,
	'BTN_12': 0x10c,
	'BTN_13': 0x10d,
	'BTN_14': 0x10e,
	'BTN_15': 0x10f,
	'BTN_MOUSE': 0x110,
	'BTN_LEFT': 0x110,
	'BTN_RIGHT': 0x111,
	'BTN_MIDDLE': 0x112,
	'BTN_SIDE': 0x113,
	'BTN_EXTRA': 0x114,
	'BTN_FORWARD': 0x115,
	'BTN_BACK': 0x116,
	'BTN_TASK': 0x117,
	'BTN_JOYSTICK': 0x120,
	'BTN_TRIGGER': 0x120,
	'BTN_THUMB': 0x121,
	'BTN_THUMB2': 0x122,
	'BTN_TOP': 0x123,
	'BTN_TOP2': 0x124,
	'BTN_PINKIE': 0x125,
	'BTN_BASE': 0x126,
	'BTN_BASE2': 0x127,
	'BTN_BASE3': 0x128,
	'BTN_BASE4': 0x129,
	'BTN_BASE5': 0x12a,
	'BTN_BASE6': 0x12b,
	'BTN_DEAD': 0x12f,
	'BTN_GAMEPAD': 0x130,
	'BTN_A': 0x130,
	'BTN_B': 0x131,
	'BTN_C': 0x132,
	'BTN_X': 0x133,
	'BTN_Y': 0x134,
	'BTN_Z': 0x135,
	'BTN_TL': 0x136,
	'BTN_TR': 0x137,
	'BTN_TL2': 0x138,
	'BTN_TR2': 0x139,
	'BTN_SELECT': 0x13a,
	'BTN_START': 0x13b,
	'BTN_MODE': 0x13c,
	'BTN_THUMBL': 0x13d,
	'BTN_THUMBR': 0x13e,
	'BTN_DIGI': 0x140,
	'BTN_TOOL_PEN': 0x140,
	'BTN_TOOL_RUBBER': 0x141,
	'BTN_TOOL_BRUSH': 0x142,
	'BTN_TOOL_PENCIL': 0x143,
	'BTN_TOOL_AIRBRUSH': 0x144,
	'BTN_TOOL_FINGER': 0x145,
	'BTN_TOOL_MOUSE': 0x146,
	'BTN_TOOL_LENS': 0x147,
	'BTN_TOUCH': 0x14a,
	'BTN_STYLUS': 0x14b,
	'BTN_STYLUS2': 0x14c,
	'BTN_TOOL_DOUBLETAP': 0x14d,
	'BTN_TOOL_TRIPLETAP': 0x14e,
        'BTN_TOOL_QUADTAP':     0x14f,
	'BTN_WHEEL': 0x150,
	'BTN_GEAR_DOWN': 0x150,
	'BTN_GEAR_UP': 0x151,

        'BTN_TRIGGER_HAPPY': 		0x2c0,
        'BTN_TRIGGER_HAPPY1': 		0x2c0,
        'BTN_TRIGGER_HAPPY2': 		0x2c1,
        'BTN_TRIGGER_HAPPY3': 		0x2c2,
        'BTN_TRIGGER_HAPPY4': 		0x2c3,
        'BTN_TRIGGER_HAPPY5': 		0x2c4,
        'BTN_TRIGGER_HAPPY6': 		0x2c5,
        'BTN_TRIGGER_HAPPY7': 		0x2c6,
        'BTN_TRIGGER_HAPPY8': 		0x2c7,
        'BTN_TRIGGER_HAPPY9': 		0x2c8,
        'BTN_TRIGGER_HAPPY10': 		0x2c9,
        'BTN_TRIGGER_HAPPY11': 		0x2ca,
        'BTN_TRIGGER_HAPPY12': 		0x2cb,
        'BTN_TRIGGER_HAPPY13': 		0x2cc,
        'BTN_TRIGGER_HAPPY14': 		0x2cd,
        'BTN_TRIGGER_HAPPY15': 		0x2ce,
        'BTN_TRIGGER_HAPPY16': 		0x2cf,
        'BTN_TRIGGER_HAPPY17': 		0x2d0,
        'BTN_TRIGGER_HAPPY18': 		0x2d1,
        'BTN_TRIGGER_HAPPY19': 		0x2d2,
        'BTN_TRIGGER_HAPPY20': 		0x2d3,
        'BTN_TRIGGER_HAPPY21': 		0x2d4,
        'BTN_TRIGGER_HAPPY22': 		0x2d5,
        'BTN_TRIGGER_HAPPY23': 		0x2d6,
        'BTN_TRIGGER_HAPPY24': 		0x2d7,
        'BTN_TRIGGER_HAPPY25': 		0x2d8,
        'BTN_TRIGGER_HAPPY26': 		0x2d9,
        'BTN_TRIGGER_HAPPY27': 		0x2da,
        'BTN_TRIGGER_HAPPY28': 		0x2db,
        'BTN_TRIGGER_HAPPY29': 		0x2dc,
        'BTN_TRIGGER_HAPPY30': 		0x2dd,
        'BTN_TRIGGER_HAPPY31': 		0x2de,
        'BTN_TRIGGER_HAPPY32': 		0x2df,
        'BTN_TRIGGER_HAPPY33': 		0x2e0,
        'BTN_TRIGGER_HAPPY34': 		0x2e1,
        'BTN_TRIGGER_HAPPY35': 		0x2e2,
        'BTN_TRIGGER_HAPPY36': 		0x2e3,
        'BTN_TRIGGER_HAPPY37': 		0x2e4,
        'BTN_TRIGGER_HAPPY38': 		0x2e5,
        'BTN_TRIGGER_HAPPY39': 		0x2e6,
        'BTN_TRIGGER_HAPPY40': 		0x2e7,
}
BUS = {
	'BUS_PCI': 0x01,
	'BUS_ISAPNP': 0x02,
	'BUS_USB': 0x03,
	'BUS_HIL': 0x04,
	'BUS_BLUETOOTH': 0x05,
	'BUS_ISA': 0x10,
	'BUS_I8042': 0x11,
	'BUS_XTKBD': 0x12,
	'BUS_RS232': 0x13,
	'BUS_GAMEPORT': 0x14,
	'BUS_PARPORT': 0x15,
	'BUS_AMIGA': 0x16,
	'BUS_ADB': 0x17,
	'BUS_I2C': 0x18,
	'BUS_HOST': 0x19,
	'BUS_GSC': 0x1A,
        'BUS_ATARI': 0x1B,
        'BUS_SPI':   0x1C,
}
ABS = {
	'ABS_X': 0x00,
	'ABS_Y': 0x01,
	'ABS_Z': 0x02,
	'ABS_RX': 0x03,
	'ABS_RY': 0x04,
	'ABS_RZ': 0x05,
	'ABS_THROTTLE': 0x06,
	'ABS_RUDDER': 0x07,
	'ABS_WHEEL': 0x08,
	'ABS_GAS': 0x09,
	'ABS_BRAKE': 0x0a,
	'ABS_HAT0X': 0x10,
	'ABS_HAT0Y': 0x11,
	'ABS_HAT1X': 0x12,
	'ABS_HAT1Y': 0x13,
	'ABS_HAT2X': 0x14,
	'ABS_HAT2Y': 0x15,
	'ABS_HAT3X': 0x16,
	'ABS_HAT3Y': 0x17,
	'ABS_PRESSURE': 0x18,
	'ABS_DISTANCE': 0x19,
	'ABS_TILT_X': 0x1a,
	'ABS_TILT_Y': 0x1b,
	'ABS_TOOL_WIDTH': 0x1c,
	'ABS_VOLUME': 0x20,
	'ABS_MISC': 0x28,
        'ABS_MT_SLOT':	0x2f,
        'ABS_MT_TOUCH_MAJOR': 0x30,
        'ABS_MT_TOUCH_MINOR': 0x31,
        'ABS_MT_WIDTH_MAJOR': 0x32,
        'ABS_MT_WIDTH_MINOR': 0x33,
        'ABS_MT_ORIENTATION': 0x34,
        'ABS_MT_POSITION_X':  0x35,
        'ABS_MT_POSITION_Y':  0x36,
        'ABS_MT_TOOL_TYPE':   0x37,
        'ABS_MT_BLOB_ID':     0x38,
        'ABS_MT_TRACKING_ID': 0x39,
        'ABS_MT_PRESSURE':    0x3a,
        'ABS_MT_DISTANCE':    0x3b,
}
LED = {
	'LED_NUML': 0x00,
	'LED_CAPSL': 0x01,
	'LED_SCROLLL': 0x02,
	'LED_COMPOSE': 0x03,
	'LED_KANA': 0x04,
	'LED_SLEEP': 0x05,
	'LED_SUSPEND': 0x06,
	'LED_MUTE': 0x07,
	'LED_MISC': 0x08,
	'LED_MAIL': 0x09,
	'LED_CHARGING': 0x0a,
}

def i(x):
    return x - 2**32
EVIOCGVERSION        = i(0x80044501)
EVIOCGID             = i(0x80084502)
EVIOCGREP            = i(0x80084503)
EVIOCSREP            = 0x40084503
EVIOCGKEYCODE        = i(0x80084504)
EVIOCSKEYCODE        = 0x40084504
EVIOCGNAME           = i(0x80004506)
EVIOCGPHYS           = i(0x80004507)
EVIOCGUNIQ           = i(0x80004508)
EVIOCGKEY            = i(0x80004518)
EVIOCGLED            = i(0x80004519)
EVIOCGSND            = i(0x8000451a)
EVIOCGBIT            = i(0x80004520)
EVIOCGABS            = i(0x80144540)
EVIOCSABS            = 0x401445c0
EVIOCSFF             = 0x402c4580
EVIOCRMFF            = 0x40044581
EVIOCGEFFECTS        = i(0x80044584)
EVIOCGRAB            = 0x40044590

def invert(d):
    return dict((v,k) for k,v in d.iteritems())
ABS_invert = invert(ABS)
BTN_invert = invert(BTN)
BUS_invert = invert(BUS)
EV_invert = invert(EV)
KEY_invert = invert(KEY)
LED_invert = invert(LED)
REL_invert = invert(REL)
del invert
KEYBTN_invert = KEY_invert.copy()
KEYBTN_invert.update(BTN_invert)

def humanize(s):
    s = re.split("(\d+)", s)
    def maybe_int(ss):
        if ss and ss[0] in "0123456789": return int(ss)
        return ss
    return map(maybe_int, s)

def find(pattern):
    if ":" in pattern:
        pattern, idx = pattern.rsplit(":", 1)
        idx = int(idx)
    else:
        idx = 0
    if os.path.exists("/dev/input/event%s" % pattern):
	return os.open("/dev/input/event%s" % pattern, os.O_RDWR)

    candidates = glob.glob("/dev/input/event*")
    candidates.sort(key=humanize)
    successful_opens = 0
    for c in candidates:
	try:
	    f = os.open(c, os.O_RDWR)
	except os.error:
	    continue
        successful_opens += 1

	name = get_name(f)
	if name.find(pattern) != -1 or fnmatch.fnmatch(name, pattern):
            if idx == 0:
                return f
            else:
                idx -= 1
                continue

        try:
            phys = get_phys(f)
        except IOError:
            pass
        else:
            if phys.find(pattern) != -1 or fnmatch.fnmatch(phys, pattern):
                if idx == 0:
                    return f
                else:
                    idx -= 1
                    continue

	id = InputId.get(f)
	sid = "Bus=%s Vendor=%04x Product=%04x Version=%04x" % (\
	    id.bustype, id.vendor, id.product, id.version)
	if sid.find(pattern) != -1 or fnmatch.fnmatch(sid, pattern):
            if idx == 0:
                return f
            else:
                idx -= 1
                continue

	sid = "%04x:%04x" % (id.vendor, id.product)
	if sid.find(pattern) != -1 or fnmatch.fnmatch(sid, pattern):
            if idx == 0:
                return f
            else:
                idx -= 1
                continue

	os.close(f)
    if not successful_opens:
        raise LookupError, """\
No input devices could be opened.  This usually indicates a misconfigured
system.  Please read the section 'PERMISSIONS AND UDEV' in the hal_input
manpage"""
    raise LookupError, (
        "No input device matching %r was found (%d devices checked)" 
            % (pattern, successful_opens))

def decode(map, mapname, code):
    if isinstance(code, str): return code
    if code in map: return map[code]
    return "%s_%s" % (mapname, code)

class InputDevice:
    def __init__(self, pattern):
	if pattern.startswith("+"):
	    exclusive = 1
	    pattern = pattern[1:]
	else:
	    exclusive = 0

	self.f = find(pattern)

	if exclusive:
	    fcntl.ioctl(self.f, EVIOCGRAB, 1)
    	
    def fileno(self): return self.f
    def readable(self):
	r, w, x = select.select([self.f], [], [], 0)
	return self.f in r

    def get_bits(self, arg): return get_bits(self.f, arg)
    def get_absinfo(self, arg): return AbsInfo.get(self.f, arg)
    def read_event(self):
	e = Event.read(self.f)
	if e.type == 'EV_KEY': e.code = decode(KEYBTN_invert, 'KEY', e.code)
	elif e.type == 'EV_ABS': e.code = decode(ABS_invert, 'ABS', e.code)
	elif e.type == 'EV_REL': e.code = decode(REL_invert, 'REL', e.code)
	elif e.type == 'EV_LED': e.code = decode(LED_invert, 'LED', e.code)
	return e

    def write_event(self, *args):
	Event.write(self.f, *args)
