[AXIS_0]

# 
# Step timing is 40 us steplen + 40 us stepspace
# That gives 80 us step period = 12.5 KHz step freq
#
# Bah, even software stepping can handle that, hm2 doesnt buy you much with
# such slow steppers.
#
# Scale is 200 steps/rev * 5 revs/inch = 1000 steps/inch
#
# This gives a maxvel of 12.5/1 = 12.5 ips
#


TYPE =              LINEAR
MAX_VELOCITY =       150.0
MAX_ACCELERATION =   1500.0
# Set Stepgen max 20% higher than the axis
STEPGEN_MAX_VEL =    200.0
#STEPGEN_MIN_VEL =    0.001
STEPGEN_MAX_ACC =    2000.0

BACKLASH =           0.000

# scale is 200 steps/rev * 5 revs/inch
# SCALE =  80.4 -- Artifex
SCALE = -100.0

MIN_LIMIT =             -100.0
MAX_LIMIT =             700.0

FERROR =     0.1
#MIN_FERROR = 0.25
MIN_FERROR = 0.1

HOME =                  600.000
HOME_OFFSET =           633.70
#HOME_IGNORE_LIMITS =    YES
HOME_USE_INDEX =        NO
HOME_SEQUENCE =         0

# Set to zero if you don't have physical home/limit switches
# Set to the desired homing and latch velocity if you have switches
# See: http://www.linuxcnc.org/docs/2.5/html/config/ini_homing.html
HOME_SEARCH_VEL =       50.0
HOME_LATCH_VEL =       20.0

# these are in nanoseconds
DIRSETUP   =              200
DIRHOLD    =              200
STEPLEN    =              1000
STEPSPACE  =              1000

# Jog increments
#INCREMENTS = 1 10


[AXIS_1]

TYPE =              LINEAR
MAX_VELOCITY =       150.0
MAX_ACCELERATION =   1500.0
# Set Stepgen max 20% higher than the axis
STEPGEN_MAX_VEL =    200.0
#STEPGEN_MIN_VEL =    0.001
STEPGEN_MAX_ACC =    2000.0

BACKLASH =           0.000

SCALE = -100

MIN_LIMIT =             -100.0
MAX_LIMIT =             700.0

FERROR =     0.1
#MIN_FERROR = 0.25
MIN_FERROR = 0.1

HOME =                  600.000
HOME_OFFSET =           633.70
#HOME_IGNORE_LIMITS =    YES
HOME_USE_INDEX =        NO
HOME_SEQUENCE =         0

# Set to zero if you don't have physical home/limit switches
# Set to the desired homing and latch velocity if you have switches
# See: http://www.linuxcnc.org/docs/2.5/html/config/ini_homing.html
HOME_SEARCH_VEL =       50.0
HOME_LATCH_VEL =        20.0

# these are in nanoseconds
DIRSETUP   =              200
DIRHOLD    =              200
STEPLEN    =              1000
STEPSPACE  =              1000

# Jog increments
#INCREMENTS = 1 10


[AXIS_2]

TYPE =              LINEAR
MAX_VELOCITY =      150.0
MAX_ACCELERATION =  1500.0
# Set Stepgen max 20% higher than the axis
STEPGEN_MAX_VEL =    200
#STEPGEN_MIN_VEL =    0.001
STEPGEN_MAX_ACC =    2000.0

BACKLASH =           0.000

#SCALE = -1542.036 -- Artifex
SCALE = -100.0

MIN_LIMIT =             -100.0
MAX_LIMIT =             700.0

FERROR =     0.1
MIN_FERROR = 0.1

HOME =                  600.000
HOME_OFFSET =           633.70
#HOME_IGNORE_LIMITS =    YES
HOME_USE_INDEX =        NO
HOME_SEQUENCE =         0

# Set to zero if you don't have physical home/limit switches
# Set to the desired homing and latch velocity if you have switches
# See: http://www.linuxcnc.org/docs/2.5/html/config/ini_homing.html
HOME_SEARCH_VEL =       50.0
HOME_LATCH_VEL  =       20.0

# these are in nanoseconds
DIRSETUP   =              200
DIRHOLD    =              200
STEPLEN    =              1000
STEPSPACE  =              1000

# Jog increments
#INCREMENTS = 0.01 0.1 1


