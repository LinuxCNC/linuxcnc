# HAL file for BeagleBone + TCT paralell port cape with 5 steppers and 3D printer board
import os

from machinekit import rtapi as rt
from machinekit import hal
from machinekit import config as c

from config import base
from config import motion

# initialize the RTAPI command client
rt.init_RTAPI()
# loads the ini file passed by linuxcnc
c.load_ini(os.environ['INI_FILE_NAME'])

motion.setup_motion(kinematics=c.find('KINS','KINEMATICS'))

# reading functions
hal.addf('motion-command-handler', 'servo-thread')

# define gantry joints
base.init_gantry(axisIndex=1, joints=2, latching=True)

# Axis-of-motion Specific Configs (not the GUI)
# X [J0] Axis
base.setup_stepper(section='AXIS_0', axisIndex=0, stepgenIndex=0, stepgenType='sim')
# Y [J1] Axis
base.setup_stepper(section='AXIS_1', axisIndex=1, stepgenIndex=1, gantry=True, gantryJoint=0, stepgenType='sim')
# YY[J2] Axis
base.setup_stepper(section='AXIS_1', axisIndex=1, stepgenIndex=2, gantry=True, gantryJoint=1, stepgenType='sim')
# Z [J3] Axis
base.setup_stepper(section='AXIS_2', axisIndex=2, stepgenIndex=3, stepgenType='sim')
# A [J4] Axis
base.setup_stepper(section='AXIS_3', axisIndex=3, stepgenIndex=4, stepgenType='sim')
# B [J5] Axis
base.setup_stepper(section='AXIS_4', axisIndex=4, stepgenIndex=5, stepgenType='sim')

# update gantry position feedback
base.gantry_read(gantryAxis=1, thread='servo-thread')

# Standard I/O - EStop, Enables, Limit Switches, Etc
errorSignals = ['temp-hw-error', 'watchdog-error', 'hbp-error']
base.setup_estop(errorSignals, thread='servo-thread')
base.setup_tool_loopback()

# testing
base.setup_extras()

# write out functions
hal.addf('motion-controller', 'servo-thread')
base.gantry_write(gantryAxis=1, thread='servo-thread')

# start haltalk server after everything is initialized
# else binding the remote components on the UI might fail
hal.loadusr('haltalk', wait=True)
