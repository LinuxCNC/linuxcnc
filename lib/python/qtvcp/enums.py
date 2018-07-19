#!/usr/bin/env python
from PyQt5.QtCore import Q_ENUMS
class AXIS(object):
    X, Y, Z, A, B, C, U, V, W = range(9)

class JOINT(object):
    J0, J1, J2, J3, J4, J5, J6, J7, J8, J9, J10, J11 = range(12)

class REFERENCE(object):
    Absolute = 0
    Relative = 1
    DistanceToGo = 2

class UNITS(object):
    ProgrammedMode = 0 # Use program units g20/21
    Inch = 1    # CANON_UNITS_INCHES=1
    Metric = 2  # CANON_UNITS_MM=2

class PERMISSION(object):
    Always = 0
    WhenRunning = 1
    WhenMoving = 2
    WhenHoming = 3
    WhenIdle = 4
