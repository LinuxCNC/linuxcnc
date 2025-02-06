# This is a python remap for LinuxCNC implementing 'Tilted Work Plane'
# G68.2, G68.3, G68.4 and related Gcodes G53.1, G53.3, G53.6, G69
#
# Copyright ()c) 2023 David Mueller <mueller_david@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
#
import sys
import traceback
import numpy as np
from math import sin,cos,tan,asin,acos,atan,atan2,sqrt,pi,degrees,radians,fabs
from interpreter import *
import emccanon
from util import lineno, call_pydevd
import hal


# logging
import logging
# this name will be printed first on each log message
log = logging.getLogger('remap.py; TWP')
# we have to setup a handler to be able to set the log level for this module
handler = logging.StreamHandler()
formatter = logging.Formatter('%(name)s %(levelname)s: %(message)s')
handler.setFormatter(formatter)
log.addHandler(handler)
# Manually force the log level for this module
log.setLevel(logging.ERROR) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


# set up parsing of the inifile
import os
import configparser
# get the path for the ini file used to start this config
inifile = os.environ.get("INI_FILE_NAME")
# instantiate a parser in non-strict mode because we have multiple entries for
# some sections in the ini
config = configparser.ConfigParser(strict=False)
# ingest the ini file
config.read(inifile)

## SPINDLE ROTARY JOINT LETTERS
# spindle primary joint
joint_letter_primary = (config['TWP']['PRIMARY']).capitalize()
# spindle secondary joint (ie the one closer to the tool)
joint_letter_secondary = (config['TWP']['SECONDARY']).capitalize()

if not joint_letter_primary in ('A','B','C') or not joint_letter_secondary in ('A','B','C'):
    log.error("Unable to parse joint letters given in INI [TWP].")
elif joint_letter_primary == joint_letter_secondary:
    log.error("Letters for primary and secondary joints in INI [TWP] must not be the same.")
else:
    # get the MIN/MAX limits of the respective rotary joint letters
    category = 'AXIS_' +  joint_letter_primary
    primary_min_limit = float(config[category]['MIN_LIMIT'])
    primary_max_limit = float(config[category]['MAX_LIMIT'])
    log.info('Joint letter for primary   is %s with MIN/MAX limits: %s,%s', joint_letter_primary, primary_min_limit, primary_max_limit)
    category = 'AXIS_' +  joint_letter_secondary
    secondary_min_limit = float(config[category]['MIN_LIMIT'])
    secondary_max_limit = float(config[category]['MAX_LIMIT'])
    log.info('Joint letter for secondary is %s with MIN/MAX Limits: %s,%s', joint_letter_secondary, secondary_min_limit, secondary_max_limit)


## CONNECTIONS TO THE KINEMATIC COMPONENT
# get the name of the kinematic component (this seems to ingest also the next line)
kins_comp = (config['KINS']['KINEMATICS']).partition('\n')[0]
# name of the hal pin that represents the nutation-angle
kins_nutation_angle = kins_comp + '_kins.nut-angle'
# name of the hal pin that represents the pre-rotation
kins_pre_rotation = kins_comp + '_kins.pre-rot'
# name of the hal pin that represents the primary joint orientation angle
kins_primary_rotation = kins_comp + '_kins.primary-angle'
# name of the hal pin that represents the secondary joint orientation angle
kins_secondary_rotation = kins_comp + '_kins.secondary-angle'

## CONNECTIONS TO THE HELPER COMPONENT
twp_comp = 'twp-helper-comp.'
twp_is_defined = twp_comp + 'twp-is-defined'
twp_is_active = twp_comp + 'twp-is-active'


# raise InterpreterException if execute() or read() fail
throw_exceptions = 1

## VALUE INITIALIZATION
# we start with the identity matrix (ie the twp is equal to the world coordinates)
twp_matrix = np.asmatrix(np.identity(4))

# some g68.2 p-word modes require several calls to enter all the required parameters so we
# need a flag that indicates when the twp has been defined and is ready for g53.x
# [current p-word, number of calls required, (state of calls required for that p mode added by g68.2)]
# note that we use string since boolean True == 1, which gives wrong results if we want
# to count the elements that are True because it is counted as integer '1'
# eg: twp_flag = [0, 1, 'empty']
twp_flag = []
# we need a place to store the twp-build-parameters if the mode needs more than one call
twp_build_params = {}
# container to store the current work offset during twp operations
current_work_offset_number = 1
saved_work_offset = [0,0,0]
# orientation mode refers to the strategy used to choose from the different rotary angles for a given
# tool-z vector. The optimization is applied to the primary axis only with mode 0 (shortest path) being
# the default. (0=shortest_path , 1=positive_rotation only, 2=negative_rotation only, )
orient_mode = 0


# defines the kinematic model for (world <-> tool) coordinates of the machine at hand
# returns 4x4 transformation matrix for given angles and 4x4 input matrix
# NOTE: these matrices must be the same as the ones used to derive the kinematic model
def kins_tool_transformation(theta_1, theta_2, pre_rot, matrix_in, direction='fwd'):
    global joint_letter_primary, joint_letter_secondary
    global kins_nutation_angle
    T_in = matrix_in

    ## Define 4x4 transformation for virtual rotation around tool-z to orient tool-x and -y
    Stc = sin(pre_rot)
    Ctc = cos(pre_rot)
    Rtc=np.matrix([[ Ctc, -Stc, 0, 0],
                   [ Stc,  Ctc, 0, 0],
                   [ 0 ,  0 ,   1, 0],
                   [ 0,   0 ,   0, 1]])

    ## Define 4x4 transformation for the primary joint
    # get the basic 3x3 rotation matrix (returns array)
    if joint_letter_primary == 'A':
        Rp = Rx(theta_1)
    elif joint_letter_primary == 'B':
        Rp = Ry(theta_1)
    elif joint_letter_primary == 'C':
        Rp = Rz(theta_1)
    # add fourth column on the right
    Rp = np.hstack((Rp, [[0],[0],[0]]))
    # expand to 4x4 array and make into a matrix
    row_4 = [0,0,0,1]
    Rp = np.vstack((Rp, row_4))
    Rp = np.asmatrix(Rp)

    ## Define 4x4 transformation matrix for the secondary joint
    # get the basic 3x3 rotation matrix (returns array)
    if joint_letter_secondary == 'A':
        Rs = Rx(theta_2)
    elif joint_letter_secondary == 'B':
        Rs = Ry(theta_2)
    elif joint_letter_secondary == 'C':
        Rs = Rz(theta_2)
    # add fourth column on the right
    Rs = np.hstack((Rs, [[0],[0],[0]]))
    # expand to 4x4 array and make into a matrix
    row_4 = [0,0,0,1]
    Rs = np.vstack((Rs, row_4))
    Rs = np.asmatrix(Rs)

    if (joint_letter_primary, joint_letter_secondary)== ('C', 'B'):
        # Additional definitions for nutating joint
        v = radians(hal.get_value(kins_nutation_angle))
        Sv = sin(v)
        Cv = cos(v)
        Ss = sin(theta_2)
        Cs = cos(theta_2)
        r = Cs + Sv*Sv*(1-Cs)
        s = Cs + Cv*Cv*(1-Cs)
        t = Sv*Cv*(1-Cs) 
        # define rotation matrix for the secondary spindle joint
        Rs=np.matrix([[     Cs, -Cv*Ss,  Sv*Ss, 0],
                      [  Cv*Ss,      r,      t, 0],
                      [ -Sv*Ss,      t,      s, 0],
                      [      0,      0,      0, 1]])

    elif (joint_letter_primary, joint_letter_secondary)== ('C', 'A'):
        # Additional definitions for nutating joint
        v = radians(hal.get_value(kins_nutation_angle))
        Sv = sin(v)
        Cv = cos(v)
        Ss = sin(theta_2)
        Cs = cos(theta_2)
        r = Cs + Sv*Sv*(1-Cs)
        s = Cs + Cv*Cv*(1-Cs)
        t = Sv*Cv*(1-Cs)
        # define rotation matrix for the secondary spindle joint
        Rs=np.matrix([[      r, -Cv*Ss,      t, 0],
                      [  Cv*Ss,     Cs, -Sv*Ss, 0],
                      [      t,  Sv*Ss,      s, 0],
                      [      0,      0,      0, 1]])

    else:
        log.error('No formula for this spindle kinematic (primary, secondary) %s, %s', joint_letter_primary, joint_letter_secondary)

    # calculate the transformation matrix for the forward tool kinematic
    matrix_tool_fwd = np.transpose(Rtc)*np.transpose(Rs)*np.transpose(Rp)*T_in
    # calculate the transformation matrix for the inverse tool kinematic
    matrix_tool_inv = Rp*Rs*Rtc*T_in
    if direction == 'fwd':
        #log.debug("matrix tool fwd: \n", matrix_tool_fwd)
        #log.debug("inv would have been: \n", matrix_tool_inv)
        return matrix_tool_fwd
    elif direction == 'inv':
        #log.debug("matrix tool inv: \n", matrix_tool_inv)
        #log.debug("fwd would have been: \n", matrix_tool_fwd)
        return matrix_tool_inv
    else:
        return 0


# returns angle 'tc' required to rotate the x-axis of the tool-coords parallelto the machine-xy plane
# for given machine joint position angles.
# For G68.3 this is the default tool-x direction
# NOTE: this uses formulas derived from the transformation matrix in the inverse tool kinematic
def kins_calc_tool_rot_c_for_horizontal_x(self, theta_1, theta_2):
    global joint_letter_primary, joint_letter_secondary
    # The idea is that the tool-x vector is parallel to the machine xy-plane when the
    # z component of the x-direction vector is equal to zero
    # Mathematically we take the symbolic formula found in row 3, column 1 of the transformation
    # matrix from the inverse tool-kinematics, equal that to zero and solve for 'tc'.
    # this makes the x orientation of the tool coords horizontal and the user can set the
    # rotation from there using g68.3 r
    global kins_nutation_angle
    v = radians(hal.get_value(kins_nutation_angle))
    Cv = cos(v)
    Sv = sin(v)
    Cs = cos(theta_2)
    Ss = sin(theta_2)
    Cp = cos(theta_1)
    Sp = sin(theta_1)
    if (joint_letter_primary, joint_letter_secondary)== ('C', 'B'):
        t = Sv*Cv*(1-Cs)
        tc = atan2((Sv*Ss),t)
    elif (joint_letter_primary, joint_letter_secondary)== ('C', 'A'):
        t = Sv*Cv*(1-Cs)
        tc = atan2(-t,(Sv*Ss))
    else:
        log.error('No formula for this spindle kinematic (primary, secondary) %s, %s', joint_letter_primary, joint_letter_secondary)
    # note: tool-c rotation is done using a halpin that feeds into the kinematic component and the
    # vismach model. In contrast to a gcode command where 'c' refers to a physical machine joint)
    return tc


# calculates the secondary joint position for a given tool-vector
# secondary being the joint closest to the tool
# Note: this uses functions derived from the custom kinematic
def kins_calc_secondary(self, tool_z_req):
    global joint_letter_primary, joint_letter_secondary
    global secondary_min_limit, secondary_max_limit
    global kins_nutation_angle
    epsilon = 0.000001
    theta_2_list=[]
    (Kzx, Kzy, Kzz) = (tool_z_req[0], tool_z_req[1], tool_z_req[2])

    if (joint_letter_primary, joint_letter_secondary)== ('C', 'B'):
        # This kinmatic has infinite results for the vertical tool orientation
        # so we explicitly define the angles for that specific case
        if Kzz > 1 - epsilon:
            return [0]
        else:
            v = radians(hal.get_value(kins_nutation_angle))
            Sv = sin(v)
            Cv = cos(v)
            theta_2 = acos((Kzz - Cv*Cv)/(1 - Cv*Cv))
    elif (joint_letter_primary, joint_letter_secondary)== ('C', 'A'):
        # This kinmatic has infinite results for the vertical tool orientation
        # so we explicitly define the angles for that specific case
        if Kzz > 1 - epsilon:
            return [0]
        else:
            v = radians(hal.get_value(kins_nutation_angle))
            Sv = sin(v)
            Cv = cos(v)
            theta_2 = acos((Kzz - Cv*Cv)/(1 - Cv*Cv))
    else:
        log.error('No formula for this spindle kinematic (primary, secondary) %s', (joint_letter_primary, joint_letter_secondary))
    # since we are using acos() we really have two solutions theta_2 and -theta_2
    for theta in [theta_2, -theta_2]:
        log.debug('Checking if result %s is within secondary joint limits of %s and %s.',
                    degrees(theta), secondary_min_limit, secondary_max_limit)
        if theta > secondary_min_limit and theta < secondary_max_limit:
            log.debug('Adding %s to valid angles list.', degrees(theta))
            theta_2_list.append(theta)
    log.debug('List of possible secondary angles: %s\n',  theta_2_list)
    return theta_2_list


# calculates the primary joint position for a given tool-vector
# Note: this uses functions derived from the custom kinematic
def kins_calc_primary(self, tool_z_req, theta_2_list):
    global joint_letter_primary, joint_letter_secondary
    global primary_min_limit, primary_max_limit
    global kins_nutation_angle
    epsilon = 0.000001
    theta_1_list=[]
    (Kzx, Kzy, Kzz) = (tool_z_req[0], tool_z_req[1], tool_z_req[2])
    if (joint_letter_primary, joint_letter_secondary)== ('C', 'B'):
        # This kinmatic has infinite results for the vertical tool orientation
        # so we explicitly define the angles for that specific case
        if Kzz > 1 - epsilon:
            return [0]
        else:
            v = radians(hal.get_value(kins_nutation_angle))
            Sv = sin(v)
            Cv = cos(v)
            for i in range(len(theta_2_list)):
                theta_2 = theta_2_list[i]
                Ss  = sin(theta_2)
                Cs  = cos(theta_2)
                t = Sv*Cv*(1-Cs)
                p = Sv * Ss

                theta_1 = asin((p*Kzy - t*Kzx)/(t*t + p*p))
    elif (joint_letter_primary, joint_letter_secondary)== ('C', 'A'):
        # This kinmatic has infinite results for the vertical tool orientation
        # so we explicitly define the angles for that specific case
        if Kzz > 1 - epsilon:
            return [0]
        else:
            v = radians(hal.get_value(kins_nutation_angle))
            Sv = sin(v)
            Cv = cos(v)
            for i in range(len(theta_2_list)):
                theta_2 = theta_2_list[i]
                Ss  = sin(theta_2)
                Cs  = cos(theta_2)
                t = Sv*Cv*(1-Cs)
                p = Sv * Ss
                q = (t*Kzy - p*Kzx)/(t*t + p*p)
                theta_1 = asin(q)
    else:
        log.error('No formula for this spindle kinematic (primary, secondary) %s', (joint_letter_primary, joint_letter_secondary))
    # since we are using asin() we really have two solutions theta_1 and pi-theta_2
    for theta in [theta_1, transform_to_pipi(pi - theta_1)]:
        log.debug('Checking if result %s is within secondary joint limits of %s and %s.',
                    degrees(theta), secondary_min_limit, secondary_max_limit)
        if theta > secondary_min_limit and theta < secondary_max_limit:
            log.debug('Adding %s to valid angles list.', degrees(theta))
            theta_1_list.append(theta)
    log.debug('List of possible secondary angles: %s\n',  theta_2_list)
    return theta_1_list


# this is from 'mika-s.github.io'
# transforms a given angle to the interval of [-pi,pi]
def transform_to_pipi(input_angle):
    revolutions = int((input_angle + np.sign(input_angle) * pi) / (2 * pi))
    p1 = truncated_remainder(input_angle + np.sign(input_angle) * pi, 2 * pi)
    p2 = (np.sign(np.sign(input_angle)
                  + 2 * (np.sign(fabs((truncated_remainder(input_angle + pi, 2 * pi)) / (2 * pi))) - 1))) * pi
    output_angle = p1 - p2
    return output_angle


# this is from 'mika-s.github.io'
# used by 'transform_to_pipi()'
def truncated_remainder(dividend, divisor):
    divided_number = dividend / divisor
    divided_number = -int(-divided_number) if divided_number < 0 else int(divided_number)
    remainder = dividend - divisor * divided_number
    return remainder


# returns a list of valid primary/secondary spindle joint positions for a given tool-orientation vector
# or 'None','None' if no valid position could be found
def kins_calc_jnt_angles(self, tool_z_req):
    log.debug('tool_z_requested: %s', tool_z_req)
    # set the tolerance value
    epsilon = 0.0001
    # create np.array so we can easily calculate differences and check elements
    tool_z_req = np.array([tool_z_req[0], tool_z_req[1], tool_z_req[2]])
    # calculate secondary joint values using kinematic specific formula
    theta_2_pair = kins_calc_secondary(self, tool_z_req)
    # calculate primary joint values using kinematic specific formula
    theta_1_pair = kins_calc_primary(self, tool_z_req, theta_2_pair)
    joint_angles_list = []
    # iterate through all the possible combinations of (theta_1 , theta_2)
    for i in range(len(theta_1_pair)):
        for j in range(len(theta_2_pair)):
            # rotate an identity matrix using the custom tool kinematic model and the (theta_1, theta_2)
            matrix_in = np.asmatrix(np.identity(4))
            t_out = kins_tool_transformation(theta_1_pair[i], theta_2_pair[j], 0, matrix_in,'inv')
            # the resulting tool-z vector for this pair of (theta_1, theta_2) is found in the third column
            tool_z_would_be = np.array([t_out[0,2], t_out[1,2], t_out[2,2]])
            log.debug('tool_z_would_be: %s', tool_z_would_be)
            # calculate the difference of the respective elements
            tool_z_diff = tool_z_req - tool_z_would_be
            # and check if all elements are within [-epsilon,epsilon]
            match = np.all((tool_z_diff > -epsilon) & (tool_z_diff < epsilon))
            log.debug('Is the tool-Z-vector close enough ? %s', match)
            if match:
                # check if we already have this particular pair in the list
                if not (theta_1_pair[i], theta_2_pair[j]) in joint_angles_list:
                    log.debug('Appending (theta_1_pair, theta_2_pair) %s', (degrees(theta_1_pair[i]), degrees(theta_2_pair[j])))
                    joint_angles_list.append((theta_1_pair[i], theta_2_pair[j]))
    log.info('Found valid joint angles: %s', joint_angles_list)
    if joint_angles_list:
        return joint_angles_list
        #return joint_angles_list[-1]
    else:
        return None, None

def calc_shortest_distance(pos, trgt, mode):
    # calculate the shortest distance in [-180°, 180°]
    # eg if pos=170° and trgt=-170° then dist will be 20°
    # If the operator requests positive or negative rotation
    # we may need to return the long distance instead
    log.debug('Got (pos, trgt): %s', (pos, trgt))
    dist_short = (trgt - pos + 180) % 360 - 180
    # calculate short and long distance
    if dist_short >= 0: # ie dist_long should be negative
        dist_long = -(360 - dist_short)
    else:
        dist_long =  360 + dist_short
    log.debug('Calculated (dist_short, dist_long):  %s', (dist_short, dist_long))
    if mode == 1: # positive rotation only, ie we want a positive distance
        if dist_short >= 0: # ie we want this one
            dist = dist_short
        else:  # ie we need to go the other way
            dist = dist_long
    if mode == 2: # negative rotation only ie we want a positive distance
        if dist_short >= 0: # ie we need to go the other way
            dist = dist_long
        else: # ie we want this one
            dist = dist_short
    else: # mode = 0 ie we want the shortest distance either way
        dist = dist_short
    log.debug('Distance returned:  %s', dist)
    return dist


# this takes a target angle in [-pi,pi] and finds the closest move within [min_limit, max_limit]
# from a given position in [min_limit, max_limit], returns the optimized target angle and the distance
# from the given position to that target angle
def calc_rotary_move_with_joint_limits(position, target, max_limit, min_limit, mode):
    pos = degrees(position)
    trgt = degrees(target)
    log.debug('(Current_pos, target):  %s', (pos, trgt))
    # calculate the shortest distance from position to target for the strategy given by
    # the operator (ie shortest (= default), positive rotation only, negative rotation only )
    dist = calc_shortest_distance(pos, trgt, mode)
    # check that the result is within the rotary axis limits defined in the ini file
    if dist >= 0: # shortest way is in the positive direction
        if (pos + dist) <=  max_limit: # if the limits allow we rotate the joint in the positive sense
            log.debug('Max_limit OK, target changed to: %s', (pos + dist))
            theta = pos + dist
        else: # if positive limits would be exceeded we need to go the longer wey in the other direction
            if mode == 0:
                log.debug('Max_limit reached, target remains: %s', trgt)
                theta = trgt
            else: # if the rotation direction was set by the operator then we can not change direction
                theta = None
                dist = None
    else:  # shortest way is in the negative direction
        if (pos + dist) >=  min_limit: # if the limits allow we rotate the joint in the negative sense
            log.debug('Min_limit OK, target changed to:  %s', (pos + dist))
            theta = pos + dist
        else: # if negative limits would be exceeded we need to go the longer way int the other direction
            if mode == 0:
                log.debug('Min_limit reached, target remains:  %s', trgt)
                theta = trgt
            else: # if the rotation direction was set by the operator then we can not change direction
                theta = None
                dist = None
    # we also attach the distance for this particular move and mode
    log.debug('Angle and distance returned:  %s, %s', theta, dist)
    return theta, dist


# this takes a list of joint angle pairs in [-pi,pi] and optimizes them for shortest moves
# in (min_limit, max_linit) from the current joint positions using the orient_mode set by
# the operator: 0=shortest (default), 1=positive rotation only, 2=negative rotation only
def calc_angle_pairs_and_distances(self, possible_prim_sec_angle_pairs):
    global primary_min_limit, primary_max_limit, secondary_min_limit, secondary_max_limit
    global orient_mode
    # get the current joint positions
    prim_pos, sec_pos = get_current_rotary_positions(self)
    # we want to return a list of angles that are optimized for the orient_mode and the
    # rotary axes limits as set in the ini file
    target_dist_list= []
    for prim_trgt, sec_trgt in possible_prim_sec_angle_pairs:
        # primary joint, here we apply the orient mode requested by the operator
        prim_move, prim_dist = calc_rotary_move_with_joint_limits(prim_pos, prim_trgt,
                                                                  primary_max_limit, primary_min_limit,
                                                                  orient_mode)
        # secondary joint, here we want the shortest move (although we could also apply a strategy here)
        sec_move, sec_dist = calc_rotary_move_with_joint_limits(sec_pos, sec_trgt,
                                                                secondary_max_limit, secondary_min_limit,
                                                                0)
        # if a solution has been found for this particular pair then we add it to the list
        if not (prim_move == None) and not (sec_move == None):
            target_dist_list.append(((prim_move, sec_move),(prim_dist, sec_dist)))
    log.debug('Assembled target_dist_list:  %s',target_dist_list)
    return target_dist_list


# find the optimal joint move from current to target positions in the list
# for this we look at the primary joint move only
# orient_mode is 0=shortest, 1=positive rotation only, 2=negative rotation only
# For orient_mode=(1,2): If no move can be found within joint limits we return None
def calc_optimal_joint_move(self, possible_prim_sec_angle_pairs):
    global orient_mode
    # this returns a list with all moves ((prim_move, sec_move),(prim_dist, sec_dist)) that
    # will result in correct tool orientation, stay within the rotary axis limits and respect the
    # orient_mode if set by the operator
    valid_joint_moves_and_distances = calc_angle_pairs_and_distances(self, possible_prim_sec_angle_pairs)
    # now we need to pick and return the (primary angle, secondary angle) that results in the
    # shortest move of the primary joint
    (theta_1, theta_2) = (None, None)
    dist = 3600
    for trgt_angles, dists in valid_joint_moves_and_distances:
        if orient_mode == 0 and fabs(dists[0]) < fabs(dist): # shortest move requested
            (theta_1, theta_2) = trgt_angles
            dist = dists[0]
        elif orient_mode == 1 and fabs(dists[0]) < fabs(dist) and dists[0] >= 0: # positive primary rotation only
            (theta_1, theta_2) = trgt_angles
            dist = dists[0]
        elif orient_mode == 2 and fabs(dists[0]) < fabs(dist) and dists[0] <= 0: # negative primary rotation only
            (theta_1, theta_2) = trgt_angles
            dist = dists[0]
    log.debug('Shortest move selected for (orient_mode, theta_1, theta_2):  %s', (orient_mode, theta_1, theta_2))
    return theta_1, theta_2


# calculates the required pre-rotation around tool-z so the tool-x matches the requested
# orientation after rotation of the spindle joints
def kins_calc_pre_rot(self, theta_1, theta_2, tool_x_req, tool_z_requested):
    # tolerance setting for check if tool-x-vector needs to be rotated at all
    epsilon = 0.00000001
    log.info("Tool-x-requested: %s", tool_x_req)
    # we need to calculate the current tool-x vector with the given rotations using
    # the transformation matrix from our custom tool kinematic
    log.debug("joint angles (secondary, primary) in radians given: %s", (theta_2, theta_1))
    log.debug("joint angles (secondary, primary) in degrees given: %s", (theta_2*180/pi, theta_1*180/pi))
    # run the identity matrix through the tool kinematic transformation in the requested direction
    # using the given joint angles and pre-rotation zero
    matrix_in = np.asmatrix(np.identity(4))
    t_out = kins_tool_transformation(theta_1, theta_2, 0, matrix_in,'inv')
    # the tool-x vector for the given machine joint rotations is found directly in the first column
    tool_x_is = [t_out[0,0], t_out[1,0], t_out[2,0]]
    log.debug("tool-x after machine rotation would be: %s", tool_x_is)
    # we calculate the angular difference between the two vectors so we can 'pre-rotate'
    # around tool-z to get the requested tool-x vector after machine rotation
    # just to be sure we normalize the two vectors
    tool_x_is = tool_x_is / np.linalg.norm(tool_x_is)
    tool_x_req = tool_x_req / np.linalg.norm(tool_x_req)
    # check if the x-vector is already in the required orientation (ie parallel)
    log.debug("check if vectors are parallel: %s",  np.dot(tool_x_is,tool_x_req))
    if np.dot(tool_x_is,tool_x_req) >  1 - epsilon:
        log.info("Tool x-vector already oriented, setting pre-rotation = 0")
        # if we are already parallel then we don't need to pre-rotate
        pre_rot = 0
    else:
        # we can use the cross product to determine the direction we need to rotate
        cross = np.cross(tool_x_req, tool_x_is)
        log.debug("cross product (tool_x_req, tool_x_is): %s", cross)
        log.info("Tool_z_requested: %s", tool_z_requested)
        pre_rot =  np.arccos(np.dot(tool_x_req, tool_x_is))
        log.debug('base pre_rot: %s', pre_rot)
        # To find out which quadrant we need the angle to be in we create a list of them all
        pre_rot_list = [pre_rot, -pre_rot, 2*pi-pre_rot, -(2*pi-pre_rot)]
        log.debug('pre_rot_list: %s',pre_rot_list)
        # then we run all of them through the kinematic model and see which gives us
        # the requested tool-x-vector
        for pre_rot in pre_rot_list:
            zeta = 0.0001
            # run the identity matrix through the tool kinematic transformation in the requested direction
            # using the given joint angles and pre-rotation angle in the list
            matrix_in = np.asmatrix(np.identity(4))
            t_out = kins_tool_transformation(theta_1, theta_2, pre_rot, matrix_in,'inv')
            # the tool-x vector for the given primary and secondary rotations is found directly in the first column
            tool_x_would_be = [t_out[0,0], t_out[1,0], t_out[2,0]]
            log.debug('tool_x_would_be: %s', tool_x_would_be)
            # calculate the difference of the respective elements
            tool_x_diff = tool_x_req - tool_x_would_be
            # and check if all elements are within [-epsilon,epsilon]
            match = np.all((tool_x_diff > -zeta) & (tool_x_diff < zeta))
            log.debug('Is the tool-X-vector close enough ? %s', match)
            if match:
                # if we have a match we leave the loop and use this angle
                break
        log.info("Pre-rotation calculated [deg]: %s", degrees(pre_rot))
    # return pre_rot in radians
    return pre_rot


# transforms a 4x4 input matrix using the current tool transformation matrix
# (forward or inverse) using the kinematic model of the machine
def kins_calc_tool_transformation(self, matrix_in, theta_1=None, theta_2=None, pre_rot=None, direction='fwd'):
    global kins_pre_rotation
    # if no angle values have been passed we get the current joint positions
    if theta_2 == None or theta_1 == None:
        # read current spindle rotary angles and convert to radians
        theta_1, theta_2 = get_current_rotary_positions(self)
    else:
        log.debug("got for secondary joint: %s", theta_2)
        log.debug("got for primary joint: %s", theta_1)
    # pre-rot is the virtual rotary axis around the tool-z axis to align the tool-x axis
    # if no pre-rot angle is passed then we use the currently active value
    if pre_rot == None:
        pre_rot =  hal.get_value(kins_pre_rotation )
        log.debug("current pre-rot: %s", pre_rot)
    else:
        log.debug("requested pre-rot value [DEG]): %s", degrees(pre_rot))
    # run the input matrix through the tool kinematic transformation in the requested direction
    # using the current joint angles and pre-rotation as requested
    matrix_out = kins_tool_transformation(theta_1, theta_2, pre_rot, matrix_in, direction)
    return matrix_out


# define the basic rotation matrices, used for euler twp modes
def Rx(th):
   return np.array([[1, 0      ,  0      ],
                    [0, cos(th), -sin(th)],
                    [0, sin(th),  cos(th)]])

def Ry(th):
   return np.array([[ cos(th), 0, sin(th)],
                    [ 0      , 1, 0      ],
                    [-sin(th), 0, cos(th)]])

def Rz(th):
   return np.array([[cos(th), -sin(th), 0],
                    [sin(th),  cos(th), 0],
                    [0      ,  0      , 1]])


# returns the rotation matrices for given order and angles
def twp_calc_euler_rot_matrix(th1, th2, th3, order):
    log.debug("euler order requested: %s", order)
    log.debug("angles given (th1, th2 , th3): %s", (th1, th2, th3))
    th1 = radians(th1)
    th2 = radians(th2)
    th3 = radians(th3)
    if order == '131':
        matrix = np.dot(np.dot(Rx(th1), Rz(th2)), Rx(th3))
    elif order=='121':
        matrix = np.dot(np.dot(Rx(th1), Ry(th2)), Rx(th3))
    elif order=='212':
        matrix = np.dot(np.dot(Ry(th1), Rx(th2)), Ry(th3))
    elif order=='232':
        matrix = np.dot(np.dot(Ry(th1), Rz(th2)), Ry(th3))
    elif order=='323':
        matrix = np.dot(np.dot(Rz(th1), Ry(th2)), Rz(th3))
    elif order=='313':
        matrix = np.dot(np.dot(Rz(th1), Rx(th2)), Rz(th3))
    elif order=='123':
        matrix = np.dot(np.dot(Rx(th1), Ry(th2)), Rz(th3))
    elif order=='132':
        matrix = np.dot(np.dot(Rx(th1), Rz(th2)), Ry(th3))
    elif order=='213':
        matrix = np.dot(np.dot(Ry(th1), Rx(th2)), Rz(th3))
    elif order=='231':
        matrix = np.dot(np.dot(Ry(th1), Rz(th2)), Rx(th3))
    elif order=='321':
        matrix = np.dot(np.dot(Rz(th1), Ry(th2)), Rx(th3))
    elif order=='312':
        matrix = np.dot(np.dot(Rz(th1), Rx(th2)), Ry(th3))
    log.debug('euler rotation as matrix: \n %s', matrix)
    return matrix


# The tilted-work-plane is created in identity mode and must NOT be updated after a switch
def gui_update_twp(self):
    global twp_matrix, saved_work_offset
    # twp origin as vector (in world coords) from current work-offset to the origin of the twp
    hal.set_p("twp-helper-comp.twp-ox-in",str(twp_matrix[0,3]))
    hal.set_p("twp-helper-comp.twp-oy-in",str(twp_matrix[1,3]))
    hal.set_p("twp-helper-comp.twp-oz-in",str(twp_matrix[2,3]))
    # twp x-vector
    hal.set_p("twp-helper-comp.twp-xx-in",str(twp_matrix[0,0]))
    hal.set_p("twp-helper-comp.twp-xy-in",str(twp_matrix[1,0]))
    hal.set_p("twp-helper-comp.twp-xz-in",str(twp_matrix[2,0]))
    # twp z-vector
    hal.set_p("twp-helper-comp.twp-zx-in",str(twp_matrix[0,2]))
    hal.set_p("twp-helper-comp.twp-zy-in",str(twp_matrix[1,2]))
    hal.set_p("twp-helper-comp.twp-zz-in",str(twp_matrix[2,2]))
    # publish the twp offset coordinates in world coordinates (ie identity)
    [work_offset_x, work_offset_y, work_offset_z] = saved_work_offset
    log.debug("Setting work_offsets in the simulation: %s", (work_offset_x, work_offset_y, work_offset_z))
    # this is used to translate the rotated twp to the correct position
    # care must be taken that only the work_offsets in identity mode are sent as that is
    # what the model uses. The visuals for the offsets are created then rotated according to
    # the rotary joint position and then translated.
    # The twp has to be rotated out of the machine xy plane using the g68.2 parameters and is then
    # translated by the offset values of the identity mode.
    hal.set_p("twp-helper-comp.twp-ox-world-in",str(work_offset_x))
    hal.set_p("twp-helper-comp.twp-oy-world-in",str(work_offset_y))
    hal.set_p("twp-helper-comp.twp-oz-world-in",str(work_offset_z))


# NOTE: Due to easier abort handling we currently restrict the use of twp to G54
# as LinuxCNC seems to revert to G54 as the default system
def get_current_work_offset(self):
    # get which offset is active (g54=1 .. g59.3=9)
    active_offset = int(self.params[5220])
    current_work_offset_number = active_offset
    # set the relevant parameter numbers that hold the active offset values
    # (G54_x: #5221, G55_x:#[5221+20], G56_x:#[5221+40] ....)
    work_offset_x = (active_offset-1)*20 + 5221
    work_offset_y = work_offset_x + 1
    work_offset_z = work_offset_x + 2
    co_x = self.params[work_offset_x]
    co_y = self.params[work_offset_y]
    co_z = self.params[work_offset_z]
    current_work_offset = [co_x, co_y, co_z]
    return [current_work_offset_number, current_work_offset]


def get_current_rotary_positions(self):
    global joint_letter_primary, joint_letter_secondary
    if joint_letter_primary == 'A':
        theta_1 = radians(self.AA_current)
    elif joint_letter_primary == 'B':
        theta_1 = radians(self.BB_current)
    elif joint_letter_primary == 'C':
        theta_1 = radians(self.CC_current)
    log.debug('Current position Primary joint: %s', degrees(theta_1))
    # read current spindle rotary angles and convert to radians
    if joint_letter_secondary == 'A':
        theta_2 = radians(self.AA_current)
    elif joint_letter_secondary == 'B':
        theta_2 = radians(self.BB_current)
    elif joint_letter_secondary == 'C':
        theta_2 = radians(self.CC_current)
    log.debug('Current position Secondary joint: %s', degrees(theta_2))
    return theta_1, theta_2


# forms a 4x4 transformation matrix from a given 1x3 point vector [x,y,z]
def point_to_matrix(point):
    # start with a 4x4 identity matrix and add the point vector to the 4th column
    matrix = np.identity(4)
    [matrix[0,3], matrix[1,3], matrix[2,3]] = point
    matrix = np.asmatrix(matrix)
    return matrix


# extracts the point vector form a given 4x4 transformation matrix
def matrix_to_point(matrix):
    point = (matrix[0,3],matrix[1,3],matrix[2,3])
    return point


def reset_twp_params(self):
    global pre_rot, twp_matrix, twp_flag, twp_build_params
    pre_rot = 0
    # we must not change tool kins parameters when TOOL kins are active or we get sudden joint position changes
    # ie don't do this: kins_comp_set_pre_rot(self,0)!
    twp_flag = []
    twp_build_params = {}
    log.info("Resetting TWP-matrix")
    twp_matrix = np.asmatrix(np.identity(4))

# Orient the tool to the current twp (with TCP for G53.1 or IDENTITY for G53.6)
# (some controllers offer an optional P-word to give preferred rotation directions this is not implemented yet)
# Note: To avoid that this python code is run prematurely by the read ahead we need a quebuster at the beginning but
# because we need self.execute() to switch the WCS properly this remap needs to be called from
# an ngc reamp that contains a quebuster before calling this code
# IMPORTANT:
# The correct kinematic mode (ie TCP for 53.1 / IDENTITY for G53.6) must be active when this code is called
# (ie do it in the ngc remap mentioned above!)
def g53x_core(self):
    global saved_work_offset, twp_matrix, twp_flag, pre_rot
    global joint_letter_primary, joint_letter_secondary, twp_error_status
    global orient_mode
    if self.task == 0: # ignore the preview interpreter
        yield INTERP_EXECUTE_FINISH
        return INTERP_OK

    if not  hal.get_value(twp_is_defined):
         # reset the twp parameters
        reset_twp_params(self)
        msg = "G53.x: No TWP defined."
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    elif hal.get_value(twp_is_active):
         # reset the twp parameters
        reset_twp_params(self)
        msg = "G53.x: TWP already active"
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # Check if any words have been passed with the respective G53.x command
    c = self.blocks[self.remap_level]
    p = c.p_number if c.p_flag else 0
    x = c.i_number if c.i_flag else None
    y = c.j_number if c.j_flag else None
    z = c.k_number if c.k_flag else None
    log.debug('G53.x Words passed: (P, X,Y,Z): %s', (p,x,y,z))
    if p not in [0,1,2]:
         # reset the twp parameters
        reset_twp_params(self)
        msg = "G53.x : unrecognised P-Word found."
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    orient_mode = p
    # calculate the required rotary joint positions and pre_rotation for the requested tool-orientation
    try:
        tool_z_requested = [twp_matrix[0,2],twp_matrix[1,2],twp_matrix[2,2]]
        # calculate all possible pairs of (primary, secondary) angles so our tool-z vector matches the requested tool-z
        # angles are returned in [-pi,pi]
        possible_prim_sec_angle_pairs = kins_calc_jnt_angles(self, tool_z_requested)
    # An excepton will occur if the requested tool orientation cannot be achieved with the kinematic at hand
    except Exception as error:
        log.error('G53.x: Calculation failed, %s', error)
        possible_prim_sec_angle_pairs = []
    if not possible_prim_sec_angle_pairs:
         # reset the twp parameters
        reset_twp_params(self)
        msg = "G53.x ERROR: Requested tool orientation not reachable -> aborting G53.x"
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # this returns one pair of optimized angles in degrees, or (None, None) if no solution could be found
    theta_1, theta_2 = calc_optimal_joint_move(self, possible_prim_sec_angle_pairs)
    if theta_1 == None:
         # reset the twp parameters
        reset_twp_params(self)
        msg = ("G53.x ERROR: Requested tool orientation not reachable -> aborting G53.x")
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    theta_1 = radians(theta_1)
    theta_2 = radians(theta_2)
    # calculate the pre-rotation needed so our tool-x vector matches the requested tool-x vector
    tool_x_requested = [twp_matrix[0,0],twp_matrix[1,0],twp_matrix[2,0]]
    pre_rot = kins_calc_pre_rot(self,theta_1, theta_2, tool_x_requested, tool_z_requested)
    log.debug("Calculated pre-rotation (pre_rot) to match requested tool-x): %s", pre_rot)
    # mark twp-flag as active
    twp_flag = [0, 'active']
    gui_update_twp(self)
    # set the pre-rotation value in the kinematic component
    log.debug("G53.x: setting primary, secondary and pre_rotation angles in kinematic component: %s", (degrees(theta_1), degrees(theta_2), degrees(pre_rot)))
    hal.set_p(kins_pre_rotation, str(pre_rot))
    hal.set_p(kins_primary_rotation, str(degrees(theta_1)))
    hal.set_p(kins_secondary_rotation, str(degrees(theta_2)))

    # calculate the work offset in tool-coords
    P = matrix_to_point(kins_calc_tool_transformation(self, point_to_matrix(saved_work_offset), theta_1, theta_2, pre_rot))
    # get the current twp_origin
    twp_offset = (twp_matrix[0,3],twp_matrix[1,3],twp_matrix[2,3])
    # calculate the twp offset in tool-coords
    Q = matrix_to_point(kins_calc_tool_transformation(self, point_to_matrix(twp_offset), theta_1, theta_2, pre_rot))
    log.debug("G53.x: Setting transformed work-offsets for tool-kins in G59, G59.1, G59.2 and G59.3 to: %s ", P)
    # set the dedicated TWP work offset values (G53, G53.1, G53.2, G53.3)
    self.execute("G10 L2 P6 X%f Y%f Z%f " % (P[0]+Q[0], P[1]+Q[1], P[2]+Q[2]), lineno())
    self.execute("G10 L2 P7 X%f Y%f Z%f " % (P[0]+Q[0], P[1]+Q[1], P[2]+Q[2]), lineno())
    self.execute("G10 L2 P8 X%f Y%f Z%f " % (P[0]+Q[0], P[1]+Q[1], P[2]+Q[2]), lineno())
    self.execute("G10 L2 P9 X%f Y%f Z%f " % (P[0]+Q[0], P[1]+Q[1], P[2]+Q[2]), lineno())
    log.debug("G53.x: Moving (secondary and primary) joints to: %s", (degrees(theta_2), degrees(theta_1)))
    if (x,y,z) == (None,None,None):
        # Move rotary joints to align the tool with the requested twp
        self.execute("G0 %s%f %s%f" % (joint_letter_secondary, degrees(theta_2), joint_letter_primary, degrees(theta_1)), lineno())
    # switch to the dedicated TWP work offsets
    self.execute("G59", lineno())
    # activate TOOL kinematics
    self.execute("M68 E3 Q2")
    if (x,y,z) != (None,None,None):
        log.debug('G53.3 called')
        self.execute("G0 X%s Y%s Z%s %s%f %s%f" % (x, y, z, joint_letter_secondary, degrees(theta_2), joint_letter_primary, degrees(theta_1)), lineno())
    # set twp-state to 'active' (2)
    self.execute("M68 E2 Q2")
    yield INTERP_EXECUTE_FINISH
    return INTERP_OK


# Cancel an active TWP definition and reset the parameters to zero
# Note: To avoid that this python code is run prematurely by the read ahead we need a quebuster at the beginning but
# because we need self.execute() to switch the WCS properly this remap needs to be called from
# an ngc that contains a quebuster before calling this code
def g69_core(self):
    global twp_flag, saved_work_offset_number, saved_work_offset
    if self.task == 0: # ignore the preview interpreter
        yield INTERP_EXECUTE_FINISH
        return INTERP_OK
    log.info('G69 called')
    # reset the twp parameters
    reset_twp_params(self)
    gui_update_twp(self)
    # set twp-state to 'undefined' (0)
    self.execute("M68 E2 Q0")
    yield INTERP_EXECUTE_FINISH
    return INTERP_OK


# define a virtual tilted-work-plane (twp) that is perpendicular to the current
# tool-orientation
def g683(self, **words):
    global twp_matrix, pre_rot, twp_flag, saved_work_offset_number, saved_work_offset

    if self.task == 0: # ignore the preview interpreter
        yield INTERP_EXECUTE_FINISH
        return INTERP_OK

    # ! IMPORTANT !
    #  We need to use 'yield INTERP_EXECUTE_FINISH' here to stop the read ahead
    # and avoid it executing the rest of the remap ahead of time
    ## NOTE: No 'self.execute(..)' command can be used after 'yield INTERP_EXECUTE_FINISH'
    yield INTERP_EXECUTE_FINISH

    if  hal.get_value(twp_is_defined):
         # reset the twp parameters
        reset_twp_params(self)
        msg =("G68.3 ERROR: TWP already defined.")
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # NOTE: Due to easier abort handling we currently restrict the use of twp to G54
    # as LinuxCNC seems to revert to G54 as the default system
    # get which offset is active (g54=1 .. g59.3=9)
    (n, offsets) = get_current_work_offset(self)
    if n != 1:
         # reset the twp parameters
        reset_twp_params(self)
        msg = "G68.3 ERROR: Must be in G54 to define TWP."
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    c = self.blocks[self.remap_level]
    # parse the requested origin
    x = c.x_number if c.x_flag else 0
    y = c.y_number if c.y_flag else 0
    z = c.z_number if c.z_flag else 0
    # parse the requested rotation of tool-x around the origin
    r = c.r_number if c.r_flag else 0

    twp_flag = [0, 1, 'empty'] # one call to define the twp in this mode
    theta_1, theta_2 = get_current_rotary_positions(self)
    # calculate tool-prerotation necessary to have tool-x vector in machine xy-plane
    pre_rot = kins_calc_tool_rot_c_for_horizontal_x(self, theta_1, theta_2 )
    log.info("G68.3: Pre-Rotation calculated for x-vector in machine-xy plane [deg]:  %s", pre_rot*180/pi)
    # then we need the tool transformation matrix of the current tool orientation with the
    # calculated pre-rotation to get the tool-x vector in the machine xy-plane
    # for this we take the 4x4 identity matrix and pass it through the inverse tool kinematic
    # transformation using the current rotary joint positions and calculated pre-rotation angle
    # plus the requested angle of rotation for tool-x from the machine-xy plane
    start_matrix = np.asmatrix(np.identity(4))
    log.info('G68.3: Requested origin rotation [deg]: %s', r)
    twp_matrix = kins_calc_tool_transformation(self, start_matrix, None, None, pre_rot +  radians(r), 'inv')
    log.debug("G68.3: Tool matrix with x-vector in machine xy-plane: \n%s", twp_matrix)
    # put the requested origin into the twp_matrix
    (twp_matrix[0,3], twp_matrix[1,3], twp_matrix[2,3]) = (x, y, z)
    # update the build state of the twp call
    twp_flag[2] = 'done'
    log.info("G68.3: Built twp-transformation-matrix: \n%s", twp_matrix)
    # collect the currently active work offset values (ie g54, g55 or other)
    saved_work_offset = offsets
    saved_work_offset_number = n
    log.debug("G68.3: Saved work offsets: %s", (n, saved_work_offset))
    # set twp-state to 'defined' (1)
    self.execute("M68 E2 Q1")
    yield INTERP_EXECUTE_FINISH

    gui_update_twp(self)
    return INTERP_OK


# definition of a virtual work-plane (twp) using different methods set by the 'p'-word
def g682(self, **words):
    global twp_matrix, pre_rot, twp_flag, twp_build_params, saved_work_offset_number, saved_work_offset

    if self.task == 0: # ignore the preview interpreter
        yield INTERP_EXECUTE_FINISH
        return INTERP_OK

    # ! IMPORTANT !
    #  We need to use 'yield INTERP_EXECUTE_FINISH' here to stop the read ahead
    # and avoid it executing the rest of the remap ahead of time
    ## NOTE: No 'self.execute(..)' command can be used after 'yield INTERP_EXECUTE_FINISH'
    yield INTERP_EXECUTE_FINISH

    if  hal.get_value(twp_is_defined): # ie TWP has already been defined
         # reset the twp parameters
        reset_twp_params(self)
        msg = ("G68.2: TWP already defined.")
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # NOTE: Due to easier abort handling we currently restrict the use of twp to G54
    # as LinuxCNC seems to revert to G54 as the default system
    (n, offsets) = get_current_work_offset(self)
    if n != 1:
         # reset the twp parameters
        reset_twp_params(self)
        msg = "G68.2 ERROR: Must be in G54 to define TWP."
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # collect the currently active work offset values (ie g54, g55 or other)
    saved_work_offset_number = n
    saved_work_offset = offsets
    log.debug("G68.2: Saved work offsets %s", (n, saved_work_offset))

    c = self.blocks[self.remap_level]
    p = c.p_number if c.p_flag else 0
    if p == 0: # true euler angles (this is the default mode)
        twp_flag = [int(p), 1, 'empty'] # one call to define the twp in this mode
        # parse requested order of rotations (default is '313' ie: ZXZ)
        q = str(int(c.q_number if c.q_flag else 313))
        if q not in ['121','131','212','232','313','323']:
             # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.2 (P0): No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested rotation of tool-x around the origin
        r = c.r_number if c.r_flag else 0
        # parse the requested euler rotation angles
        th1 = c.i_number if c.i_flag else 0
        th2 = c.j_number if c.j_flag else 0
        th3 = c.k_number if c.k_flag else 0

        # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
        twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
        log.debug('G68.2 (P0): Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = twp_calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('G68.2 (P0): Twp_euler_rotation \n%s',twp_euler_rotation)
        # calculate the total twp_rotation using matrix multiplication
        twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_euler_rotation)
        # combine rotation and translation and form the 4x4 twp-transformation matrix
        twp_matrix = np.hstack((twp_rotation, twp_origin))
        twp_row_4 = [0,0,0,1]
        twp_matrix = np.vstack((twp_matrix, twp_row_4))
        twp_matrix = np.asmatrix(twp_matrix)
        # update the build state of the twp call
        twp_flag[2] = 'done'

    elif p == 1: # non-true euler angles, eg: 'pitch,roll,yaw'
        twp_flag = [int(p), 1, 'empty'] # one call to define the twp in this mode
        # parse requested order of rotations (default is '123' ie: XYZ)
        q = str(int(c.q_number if c.q_flag else 123))

        if q not in ['123','132','213','231','312','321']:
            # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.2 P1: No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR


        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested rotation of tool-x around the origin
        r = c.r_number if c.r_flag else 0
        # parse the requested euler rotation angles
        th1 = c.i_number if c.i_flag else 0
        th2 = c.j_number if c.j_flag else 0
        th3 = c.k_number if c.k_flag else 0

         # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
        twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
        log.debug('G68.2 P1: Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = twp_calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('G68.2 P1: Twp_euler_rotation \n%s',twp_euler_rotation)
        # calculate the total twp_rotation using matrix multiplication
        twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_euler_rotation)
        # combine rotation and translation and form the 4x4 twp-transformation matrix
        twp_matrix = np.hstack((twp_rotation, twp_origin))
        twp_row_4 = [0,0,0,1]
        twp_matrix = np.vstack((twp_matrix, twp_row_4))
        twp_matrix = np.asmatrix(twp_matrix)
        # update the build state of the twp call
        twp_flag[2] = 'done'

    elif p == 2: # twp defined py 3 points on the plane
        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            twp_flag = [int(p), 4 , 'empty', 'empty', 'empty', 'empty'] # four calls needed
            twp_build_params = {'q0':[], 'q1':[], 'q2':[], 'q3':[]}
        # Point 1: defines the origin of the twp
        # Point 2: direction from P1 to P2 defines the positive x direction on the twp (tool-x)
        # Point 3: defines the positive y side and with P1 and P2 defines the xy work plane (tool-z)
        q = int(c.q_number if c.q_flag else 0)
        # this mode needs four calls to fill all required parameters
        if q == 0: # define new origin and rotation
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested rotation of tool-x around the origin
            r = c.r_number if c.r_flag else 0
            twp_build_params['q0'] = [x,y,z,r]
            twp_flag[2] = 'done'
        elif q == 1: # define point 1
            x1 = c.x_number if c.x_flag else 0
            y1 = c.y_number if c.y_flag else 0
            z1 = c.z_number if c.z_flag else 0
            twp_build_params['q1'] = [x1,y1,z1]
            twp_flag[3] = 'done'
        elif q == 2: # define point 2
            x2 = c.x_number if c.x_flag else 0
            y2 = c.y_number if c.y_flag else 0
            z2 = c.z_number if c.z_flag else 0
            twp_build_params['q2'] = [x2,y2,z2]
            twp_flag[4] = 'done'
        elif q == 3: # define point 3
            x3 = c.x_number if c.x_flag else 0
            y3 = c.y_number if c.y_flag else 0
            z3 = c.z_number if c.z_flag else 0
            twp_build_params['q3'] = [x3,y3,z3]
            twp_flag[5] = 'done'
        else:
             # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.2 P2: No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # only start calculations once all the parameters have been passed
        if twp_flag.count('done') == twp_flag[1]:
            [x, y, z, r] = twp_build_params['q0'][0:4]
            # build the translation vector of the twp_matrix
            twp_origin = [[x], [y], [z]]
            p1 = twp_build_params['q1'][0:3]
            p2 = twp_build_params['q2']
            p3 = twp_build_params['q3']
            log.debug("G68.2 P2: Point 1: %s",p1)
            log.debug("G68.2 P2: Point 2: %s",p2)
            log.debug("G68.2 P2: Point 3: %s",p3)
            # build vectors x:P1->P2 and v2:P1->P3
            twp_vect_x = [p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]]
            log.debug("G68.2 P2: Twp_vect_x: \n%s",twp_vect_x)
            v2 = [p3[0]-p1[0], p3[1]-p1[1], p3[2]-p1[2]]
            log.debug("G68.2 P2 (v2): %s",v2)
            # normalize the two vectors
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            v2 = v2 / np.linalg.norm(v2)
            # we can use the cross product to calculate the tool-z vector
            # note: if P3 is on the right side of the vector P1->P2
            # then the tool-z will be below the twp (ie tool-z will be downwards)
            twp_vect_z = np.cross(twp_vect_x , v2)
            log.debug("G68.2 P2: Twp_vect_z %s",twp_vect_z)
            # we can use the cross product to calculate the tool-y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("G68.2 P2: Twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated tool-vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("G68.2 P2: Built the twp-rotation-matrix: \n%s", twp_vect_rotation)
            # convert requested origin rotation to radians
            # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
            twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
            log.debug('G68.2 P2: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("G68.2 P2: Built twp-transformation-matrix: \n%s", twp_matrix)

    elif p == 3: # two vectors (vector 1 defines the tool-x and vector 2 defines the tool-z)
        q = int(c.q_number if c.q_flag else 0)
        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            log.info('first call')
            twp_flag = [int(p), 2 , 'empty', 'empty'] # two calls needed
            twp_build_params = {'q0':[], 'q1':[]}
        log.debug('twp_build_params: %s', twp_build_params)
        if q == 0: # define new origin of the twp
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested rotation of tool-x around the origin
            r = c.r_number if c.r_flag else 0
            # first vector (direction of x in the twp)
            i = c.i_number if c.i_flag else 0
            j = c.j_number if c.j_flag else 0
            k = c.k_number if c.k_flag else 0
            twp_build_params['q0'] = [x,y,z,i,j,k,r]
            twp_flag[2] = 'done'
        elif q == 1: # define second vector (the normal vector of the twp
            i1 = c.i_number if c.i_flag else 0
            j1 = c.j_number if c.j_flag else 0
            k1 = c.k_number if c.k_flag else 0
            twp_build_params['q1'] = [i1,j1,k1]
            twp_flag[3] = 'done'
        else:
             # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.2 P3: No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # only start calculations once all the parameters have been passed
        if twp_flag.count('done') == twp_flag[1]:
            twp_origin = (x ,y, z) = twp_build_params['q0'][0:3]
            r = twp_build_params['q0'][6]
            (i, j, k) = twp_build_params['q0'][3:6]
            (i1, j1, k1) = twp_build_params['q1']
            log.debug("(x, y, z): %s", (x, y, z))
            log.debug("(i, j, k): %s", (i, j, k))
            log.debug("(i1, j1, k1): %s", (i1, j1, k1))
            # build unit vector defining tool-x direction
            twp_vect_x = [i-x, j-y, k-z]
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            twp_vect_z = [i1, j1, k1]
            twp_vect_z = twp_vect_z / np.linalg.norm(twp_vect_z)
            orth = np.dot(twp_vect_x, twp_vect_z)
            log.debug("orth check: %s", orth)
            # the two vectors must be orthogonal
            if orth != 0:
                reset_twp_params(self)
                msg = ("G68.2 P3: Vectors are not orthogonal.")
                log.debug(msg)
                emccanon.CANON_ERROR(msg)
                yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
                yield INTERP_EXIT # w/o this the error does not abort a running gcode program
                return INTERP_ERROR

            # we can use the cross product to calculate the tool-y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("G68.2 P3: twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated tool-vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("G68.2 P3: Built twp-rotation-matrix: \n%s", twp_vect_rotation)
            # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
            try:
                twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
            except Exception as e:
                log.info('G68.2 P3: twp_origin_rotation failed, %s', e)
            log.debug('G68.2 P3: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_origin = [[x], [y], [z]]
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("G68.2 P3: Built twp-transformation-matrix: \n%s", twp_matrix)

    else:
         # reset the twp parameters
        reset_twp_params(self)
        msg = ("G68.2: No recognised P-Word found.")
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    log.debug("G68.2: twp_flag: %s", twp_flag)
    log.debug("G68.2: calls required: %s", twp_flag.count('done'))
    log.debug("G68.2: number of calls made: %s", twp_flag.count('done'))

    if twp_flag.count('done') == twp_flag[1]:
        log.info('G68.2: requested rotation: %s', radians(r))
        log.info("G68.2: twp-tranformation-matrix: \n%s",twp_matrix)
        twp_origin = [twp_matrix[0,3],twp_matrix[1,3],twp_matrix[2,3]]
        log.info("G68.2: twp origin: %s", twp_origin)
        twp_vect_x = [twp_matrix[0,0],twp_matrix[1,0],twp_matrix[2,0]]
        log.info("G68.2: twp vector-x: %s", twp_vect_x)
        twp_vect_z = [twp_matrix[0,2],twp_matrix[1,2],twp_matrix[2,2]]
        log.info("G68.2: twp vector-z: %s", twp_vect_z)
        # set twp-state to 'defined' (1)
        self.execute("M68 E2 Q1")
        yield INTERP_EXECUTE_FINISH

        gui_update_twp(self)
    return INTERP_OK

# incremental definition of  a virtual work-plane (twp) using different methods set by the 'p'-word
def g684(self, **words):
    global twp_matrix, pre_rot, twp_flag, twp_build_params, saved_work_offset_number, saved_work_offset

    if self.task == 0: # ignore the preview interpreter
        yield INTERP_EXECUTE_FINISH
        return INTERP_OK

    # ! IMPORTANT !
    #  We need to use 'yield INTERP_EXECUTE_FINISH' here to stop the read ahead
    # and avoid it executing the rest of the remap ahead of time
    ## NOTE: No 'self.execute(..)' command can be used after 'yield INTERP_EXECUTE_FINISH'
    yield INTERP_EXECUTE_FINISH

    if not hal.get_value(twp_is_active): # ie there is currently no TWP defined
         # reset the twp parameters
        reset_twp_params(self)
        msg = ("G68.4: No TWP active to increment from. Run G68.2 or G68.3 first.")
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # collect the currently active work offset values (ie g54, g55 or other)
    n = get_current_work_offset(self)[0]
    # Must be in one of the dedicated offset systems for TWP
    if False: #n < 6:
         # reset the twp parameters
        reset_twp_params(self)
        msg = ("G68.4 ERROR: Must be in G59, G59.x to increment TWP.")
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR


    # store the current TWP to
    twp_matrix_current = np.matrix.copy(twp_matrix)

    c = self.blocks[self.remap_level]
    p = c.p_number if c.p_flag else 0

    if p == 0: # true euler angles (this is the default mode)
        twp_flag = [int(p), 1, 'empty'] # one call to define the twp in this mode
        # parse requested order of rotations (default is '313' ie: ZXZ)
        q = str(int(c.q_number if c.q_flag else 313))

        if q not in ['121','131','212','232','313','323']:
             # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.4 (P0): No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested rotation of tool-x around the origin
        r = c.r_number if c.r_flag else 0
        # parse requested euler angles
        th1 = c.i_number if c.i_flag else 0
        th2 = c.j_number if c.j_flag else 0
        th3 = c.k_number if c.k_flag else 0

        # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
        twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
        log.debug('G68.4 (P0): Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = twp_calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('G68.4 (P0): Twp_euler_rotation \n%s',twp_euler_rotation)
        # calculate the total twp_rotation using matrix multiplication
        twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_euler_rotation)
        # combine rotation and translation and form the 4x4 twp-transformation matrix
        twp_matrix = np.hstack((twp_rotation, twp_origin))
        twp_row_4 = [0,0,0,1]
        twp_matrix = np.vstack((twp_matrix, twp_row_4))
        twp_matrix = np.asmatrix(twp_matrix)
        # update the build state of the twp call
        twp_flag[2] = 'done'

    elif p == 1: # non-true euler angles, eg: 'pitch,roll,yaw'
        twp_flag = [int(p), 1, 'empty'] # one call to define the twp in this mode
        # parse requested order of rotations (default is '123' ie: XYZ)
        q = str(int(c.q_number if c.q_flag else 123))

        if q not in ['123','132','213','231','312','321']:
             # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.4 P1: No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested rotation of tool-x around the origin
        r = c.r_number if c.r_flag else 0
        # parse the requested euler rotation angles
        th1 = c.i_number if c.i_flag else 0
        th2 = c.j_number if c.j_flag else 0
        th3 = c.k_number if c.k_flag else 0

         # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
        twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
        log.debug('G68.4 P1: Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = twp_calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('G68.4 P1: Twp_euler_rotation \n%s',twp_euler_rotation)
        # calculate the total twp_rotation using matrix multiplication
        twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_euler_rotation)
        # combine rotation and translation and form the 4x4 twp-transformation matrix
        twp_matrix = np.hstack((twp_rotation, twp_origin))
        twp_row_4 = [0,0,0,1]
        twp_matrix = np.vstack((twp_matrix, twp_row_4))
        twp_matrix = np.asmatrix(twp_matrix)
        # update the build state of the twp call
        twp_flag[2] = 'done'

    elif p == 2: # twp defined py 3 points on the plane
        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            twp_flag = [int(p), 4 , 'empty', 'empty', 'empty', 'empty'] # four calls needed
            twp_build_params = {'q0':[], 'q1':[], 'q2':[], 'q3':[]}
        # Point 1: defines the origin of the twp
        # Point 2: direction from P1 to P2 defines the positive x direction on the twp (tool-x)
        # Point 3: defines the positive y side and with P1 and P2 defines the xy work plane (tool-z)
        q = int(c.q_number if c.q_flag else 0)
        # this mode needs four calls to fill all required parameters
        if q == 0: # define new origin and rotation
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested rotation of tool-x around the origin
            r = c.r_number if c.r_flag else 0
            twp_build_params['q0'] = [x,y,z,r]
            twp_flag[2] = 'done'
        elif q == 1: # define point 1
            x1 = c.x_number if c.x_flag else 0
            y1 = c.y_number if c.y_flag else 0
            z1 = c.z_number if c.z_flag else 0
            twp_build_params['q1'] = [x1,y1,z1]
            twp_flag[3] = 'done'
        elif q == 2: # define point 2
            x2 = c.x_number if c.x_flag else 0
            y2 = c.y_number if c.y_flag else 0
            z2 = c.z_number if c.z_flag else 0
            twp_build_params['q2'] = [x2,y2,z2]
            twp_flag[4] = 'done'
        elif q == 3: # define point 3
            x3 = c.x_number if c.x_flag else 0
            y3 = c.y_number if c.y_flag else 0
            z3 = c.z_number if c.z_flag else 0
            twp_build_params['q3'] = [x3,y3,z3]
            twp_flag[5] = 'done'
        else:
             # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.4 P2: No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # only start calculations once all the parameters have been passed
        if twp_flag.count('done') == twp_flag[1]:
            [x, y, z, r] = twp_build_params['q0'][0:4]
            # build the translation vector of the twp_matrix
            twp_origin = [[x], [y], [z]]
            p1 = twp_build_params['q1'][0:3]
            p2 = twp_build_params['q2']
            p3 = twp_build_params['q3']
            log.debug("G68.4 P2: Point 1: %s",p1)
            log.debug("G68.4 P2: Point 2: %s",p2)
            log.debug("G68.4 P2: Point 3: %s",p3)
            # build vectors x:P1->P2 and v2:P1->P3
            twp_vect_x = [p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]]
            log.debug("G68.4 P2: Twp_vect_x: \n%s",twp_vect_x)
            v2 = [p3[0]-p1[0], p3[1]-p1[1], p3[2]-p1[2]]
            log.debug("G68.4 P2: (v2) %s", v2)
            # normalize the two vectors
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            v2 = v2 / np.linalg.norm(v2)
            # we can use the cross product to calculate the tool-z vector
            # note: if P3 is on the right side of the vector P1->P2
            # then the tool-z will be below the twp (ie tool-z will be downwards)
            twp_vect_z = np.cross(twp_vect_x , v2)
            log.debug("G68.4 P2: Twp_vect_z %s",twp_vect_z)
            # we can use the cross product to calculate the tool-y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("G68.4 P2: Twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated tool-vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("G68.4 P2: Built the twp-rotation-matrix: \n%s", twp_vect_rotation)
            # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
            try:
                twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
            except Exception as e:
                log.debug('G68.4 P2: twp_origin_rotation failed ', e)
            log.debug('G68.4 P2: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("G68.4 P2: Built twp-transformation-matrix: \n%s", twp_matrix)

    elif p == 3: # two vectors (vector 1 defines the tool-x and vector 2 defines the tool-z)
        q = int(c.q_number if c.q_flag else 0)
        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            twp_flag = [int(p), 2 , 'empty', 'empty'] # two calls needed
            twp_build_params = {'q0':[], 'q1':[]}
        if q == 0: # define new origin and first vector (direction of x in the twp)
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested rotation of tool-x around the origin
            r = c.r_number if c.r_flag else 0
            # first vector (direction of x in the twp)
            i = c.i_number if c.i_flag else 0
            j = c.j_number if c.j_flag else 0
            k = c.k_number if c.k_flag else 0
            twp_build_params['q0'] = [x,y,z,i,j,k,r]
            twp_flag[2] = 'done'
        elif q == 1: # define second vector (the normal vector of the twp
            i1 = c.i_number if c.i_flag else 0
            j1 = c.j_number if c.j_flag else 0
            k1 = c.k_number if c.k_flag else 0
            twp_build_params['q1'] = [i1,j1,k1]
            twp_flag[3] = 'done'
        else:
             # reset the twp parameters
            reset_twp_params(self)
            msg = ("G68.4 P3: No recognised Q-Word found.")
            log.debug(msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # only start calculations once all the parameters have been passed
        if twp_flag.count('done') == twp_flag[1]:
            twp_origin = (x ,y, z) = twp_build_params['q0'][0:3]
            r = twp_build_params['q0'][6]
            (i, j, k) = twp_build_params['q0'][3:6]
            (i1, j1, k1) = twp_build_params['q1']
            log.debug("(x, y, z) %s", (x, y, z))
            log.debug("(i, j, k) %s", (i, j, k))
            log.debug("(i1, j1, k1) %s", (i1, j1, k1))
            # build unit vector defining tool-x direction
            twp_vect_x = [i-x, j-y, k-z]
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            twp_vect_z = [i1, j1, k1]
            twp_vect_z = twp_vect_z / np.linalg.norm(twp_vect_z)
            orth = np.dot(twp_vect_x, twp_vect_z)
            log.debug("orth check: %s", orth)
            # the two vectors must be orthogonal
            if orth != 0:
                 # reset the twp parameters
                reset_twp_params(self)
                ## reset the parameter values
                #twp_flag = [int(p), 2 , 'empty', 'empty'] # two calls needed
                #twp_build_params = {'q0':[], 'q1':[]}
                msg = ("G68.4 P3: Vectors are not orthogonal.")
                log.debug(msg)
                emccanon.CANON_ERROR(msg)
                yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
                yield INTERP_EXIT # w/o this the error does not abort a running gcode program
                return INTERP_ERROR

            # we can use the cross product to calculate the tool-y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("G68.4 P3: twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated tool-vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("G68.4 P3: Built twp-rotation-matrix: \n%s", twp_vect_rotation)
            # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
            twp_origin_rotation = twp_calc_euler_rot_matrix(0, r, 0, '131')
            log.debug('G68.4 P3: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_origin = [[x], [y], [z]]
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("G68.4 P3: Built twp-transformation-matrix: \n%s", twp_matrix)

    else:
         # reset the twp parameters
        reset_twp_params(self)
        msg = ("G68.4: No recognised P-Word found.")
        log.debug(msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    log.debug("G68.4: twp_flag: %s", twp_flag)
    log.debug("G68.4: calls required: %s", twp_flag.count('done'))
    log.debug("G68.4: number of calls made: %s", twp_flag.count('done'))

    if twp_flag.count('done') == twp_flag[1]:
        log.info('G68.4: requested rotation %s', radians(r))
        log.info("G68.4: twp_matrix_current: \n%s", twp_matrix_current)
        log.info("G68.4: incremental twp_matrix requested: \n%s",twp_matrix)
        log.info("G68.4: calculating new twp_matrix...")
        twp_matrix_new = twp_matrix_current * twp_matrix
        log.info("G68.4: twp_matrix_new: \n%s",twp_matrix_new)
        twp_origin = [twp_matrix[0,3],twp_matrix[1,3],twp_matrix[2,3]]
        log.info("G68.4: twp origin: %s", twp_origin)
        twp_vect_x = [twp_matrix[0,0],twp_matrix[1,0],twp_matrix[2,0]]
        log.info("G68.4: twp vector-x: %s", twp_vect_x)
        twp_vect_z = [twp_matrix[0,2],twp_matrix[1,2],twp_matrix[2,2]]
        log.info("G68.4: twp vector-z: %s", twp_vect_z)
        log.info("G68.4: incremented twp_matrix: \n%s", twp_matrix_new)
        twp_matrix = twp_matrix_new
        # set twp-state to 'defined' (1)
        self.execute("M68 E2 Q1")
        yield INTERP_EXECUTE_FINISH

        gui_update_twp(self)
    return INTERP_OK
