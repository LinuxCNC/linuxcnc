from machinekit import hal
from machinekit import rtapi as rt
from machinekit import config as c


def setup_motion(kinematics='trivkins'):
#     print ("debug: kinematics(%s)" % (kinematics))
    rt.loadrt(kinematics)
    rt.loadrt('tp')

    # motion controller, get name and thread periods from ini file
    rt.loadrt(c.find('EMCMOT', 'EMCMOT'),
              servo_period_nsec=c.find('EMCMOT', 'SERVO_PERIOD'),
              num_joints=c.find('TRAJ', 'AXES'),
              num_aio=51,
              num_dio=21,
              kins=kinematics,
              tp='tp')

