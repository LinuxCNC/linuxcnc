[AXIS_3]
#JOIN 4
#RotationMotor
#CRAMPS E1


TYPE = ANGULAR
MAX_VELOCITY = 360
MAX_ACCELERATION = 3000.0
# Set Stepgen max 20% higher than the axis
STEPGEN_MAX_VEL = 380.0
STEPGEN_MAX_ACC = 3750.0

BACKLASH =           0.000

# BDH: set for the RichRap Rostock extruder,
# a modified Greg's Wade.  Uses Printrbot injection-molded gears, 16x MS.
#SCALE = 8.8888
SCALE = 8.8888
WRAPPED_ROTARY=1
MIN_LIMIT = -999999999.0
MAX_LIMIT = 999999999.0

FERROR = 1.0
MIN_FERROR = .25

HOME =                  0.000
HOME_OFFSET =           0.00
#HOME_IGNORE_LIMITS =    YES
#HOME_USE_INDEX =        YES
HOME_SEQUENCE =         0

# Set to zero if you don't have physical home/limit switches
# Set to the desired homing and latch velocity if you have switches
# See: http://www.linuxcnc.org/docs/2.5/html/config/ini_homing.html
#HOME_SEARCH_VEL =       0.0
#HOME_LATCH_VEL =        0.0

# these are in nanoseconds for Allegro A4988
DIRSETUP   =              200
DIRHOLD    =              200
STEPLEN    =              1000
STEPSPACE  =              1000


