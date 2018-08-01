import os

from machinekit import hal
from machinekit import config as c

from fdm.config import base


def setup_linearDeltaKins(c):
#    c.load_ini(os.environ['INI_FILE_NAME'])

    deltaCfRod = c.find('MACHINE', 'CF_ROD')
    deltaRadius = c.find('MACHINE', 'DELTA_R')
    deltaJoin0Offset = c.find('MACHINE', 'JOINT_0_OFFSET',0)
    deltaJoin1Offset = c.find('MACHINE', 'JOINT_1_OFFSET',0)
    deltaJoin2Offset = c.find('MACHINE', 'JOINT_2_OFFSET',0)
    deltaJoin1AngleOffset = c.find('MACHINE', 'JOINT_1_ANGLE_OFFSET',0)
    deltaJoin2AngleOffset = c.find('MACHINE', 'JOINT_2_ANGLE_OFFSET',0)
    deltaJoin1RadiusOffset = c.find('MACHINE', 'JOINT_1_RADIUS_OFFSET',0)
    deltaJoin2RadiusOffset = c.find('MACHINE', 'JOINT_2_RADIUS_OFFSET',0)

    c.load_ini(os.environ['INI_FILE_NAME'])

    hal.Pin('lineardeltakins.L').set(deltaCfRod)
    hal.Pin('lineardeltakins.R').set(deltaRadius)
    hal.Pin('lineardeltakins.J0off').set(deltaJoin0Offset)
    hal.Pin('lineardeltakins.J1off').set(deltaJoin1Offset)
    hal.Pin('lineardeltakins.J2off').set(deltaJoin2Offset)
    hal.Pin('lineardeltakins.A1off').set(deltaJoin1AngleOffset)
    hal.Pin('lineardeltakins.A2off').set(deltaJoin2AngleOffset)
    hal.Pin('lineardeltakins.R1off').set(deltaJoin1RadiusOffset)
    hal.Pin('lineardeltakins.R2off').set(deltaJoin2RadiusOffset)
