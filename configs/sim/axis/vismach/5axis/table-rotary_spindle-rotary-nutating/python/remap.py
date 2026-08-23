# This is a python remap for LinuxCNC implementing 'Tilted Work Plane'
# G68.2, G68.3, G68.4 and related Gcodes G53.1, G53.3, G53.6, G69
#
# Copyright ()c) 2025 David Mueller <mueller_david@hotmail.com>
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
'''
The remap does the following:

- Parses the G68.[2,4] gcodes and constructs the requested tool orientation vectors (x,z).
- Writes and reads hal pins created and updated by'twp-helper-comp.py' (mostly for updating the gui).
- Parses the G53.[1,3,6] and uses the functions in 'remap_funcs_twp.py' to calculate all rotary joint position that result in the correct tool orientation (there may be more than just one).
- Selects the appropriate rotary angles that will respect rotary limits set in the ini file and also follow any orientation strategy requested by the operator using the 'P' word.
- Sets the kinematic modes
- Calculates new work offset values so the WCS origin after switching to TWP mode is in the requested physical position.
- Used MDI commands to:
        - Move the rotary joints to the calculated positions
        - Switch the WCS system to 'G59' and set the values of G59, G59.[1,2.3] to the calculated coordinates
- Parses the G69 gcodes, resets the relevant parameters and switches back to Identity kinematic mode
'''


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

# set up parsing of the inifile
import os
import linuxcnc
# get the path for the ini file used to start this config
inifile = os.environ.get("INI_FILE_NAME")

# adding the remap_funcs folder to the system path.  The machine specific
# functions live beside the ini file, which is the working directory, and the
# parent is searched too so a config may keep them one level up and share them
# between variants.
cwd = os.getcwd()
parent = os.path.abspath(os.path.join(cwd, os.pardir))
sys.path.insert(0, parent)
sys.path.insert(0, cwd)
from remap_funcs_twp import *

# instantiate the LinuxCNC ini-parser
config = linuxcnc.ini(inifile)

# debug setting
try:
    debug_setting = config.getint('TWP', 'LOG_LEVEL', fallback=1)
    if debug_setting > 4: debug_setting = 4
    if debug_setting < 0: debug_setting = 0
except Exception as error:
    debug_setting = 1
    log.warning("Unable to parse debug setting given in INI. Setting it to 1.")
debug_levels = (logging.CRITICAL, logging.ERROR, logging.WARNING, logging.INFO,  logging.DEBUG)
log.setLevel(debug_levels[debug_setting])

## ROTARY JOINT LETTERS
# primary rotary joint (independent of the secondary joint)
joint_letter_primary = config.getstring('TWP', 'PRIMARY', fallback="").capitalize()
# secondary rotary joint (dependent on the primary joint)
joint_letter_secondary = config.getstring('TWP', 'SECONDARY', fallback="").capitalize()

if not joint_letter_primary in ('A','B','C') or not joint_letter_secondary in ('A','B','C'):
    log.error("Unable to parse joint letters given in INI [TWP].")
elif joint_letter_primary == joint_letter_secondary:
    log.error("Letters for primary and secondary joints in INI [TWP] must not be the same.")
else:
    # get the MIN/MAX limits of the respective rotary joint letters
    category = 'AXIS_' +  joint_letter_primary
    primary_min_limit = radians(config.getreal(category, 'MIN_LIMIT', fallback=0.0))
    primary_max_limit = radians(config.getreal(category, 'MAX_LIMIT', fallback=0.0))
    log.info('Joint letter for primary   is %s with MIN/MAX limits: %s,%s',
                         joint_letter_primary, degrees(primary_min_limit), degrees(primary_max_limit))
    category = 'AXIS_' +  joint_letter_secondary
    secondary_min_limit = radians(config.getreal(category, 'MIN_LIMIT', fallback=0.0))
    secondary_max_limit = radians(config.getreal(category, 'MAX_LIMIT', fallback=0.0))
    log.info('Joint letter for secondary is %s with MIN/MAX Limits: %s,%s',
                     joint_letter_secondary, degrees(secondary_min_limit), degrees(secondary_max_limit))


## CONNECTIONS TO THE HELPER COMPONENT
twp_comp = 'twp-helper-comp.'
twp_is_defined = twp_comp + 'twp-is-defined'
twp_is_active = twp_comp + 'twp-is-active'

# Which rotary joint should be prioritized when calculating optimal joint rotation angles
try:
    optimization_priority = config.getint('TWP', 'PRIORITY', fallback=1)
except Exception as error:
    log.warning("Unable to parse orientation priority given in INI. Setting it to 1.")
    optimization_priority = 1

# raise InterpreterException if execute() or read() fail
throw_exceptions = 1

## VALUE INITIALIZATION
# we start with the identity matrix (ie the twp is equal to the world coordinates)
twp_matrix = np.asmatrix(np.identity(4))

# some g68.2 p-word modes require several calls to enter all the required parameters so we
# need a flag that indicates when the twp has been defined and is ready for G53.n
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
# z-vector vector. The optimization is applied to the primary axis only with mode 0 (shortest path) being
# the default. (0=shortest_path , 1=positive_rotation only, 2=negative_rotation only, )
orient_mode = 0



# define the basic rotation matrices
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



def calc_euler_rot_matrix(th1, th2, th3, order): # expects radians
    # returns the rotation matrices for given order and angles
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    debug_msg = (f'   Euler order {order} requested with angles: '
                 f'{degrees(th1):.4f}, {degrees(th2):.4f}, {degrees(th3):.4f}')
    log.debug(debug_msg)
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
    #log.debug('   Returning euler rotation as matrix: \n %s', matrix)
    return matrix


def calc_joint_angles(z_vector_req, x_vector_req):
    # returns a list of valid primary/secondary rotary joint positions in radians for a given orientation vector
    # returns an empty list if no valid position could be found
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    log.debug('   z_vector_requested: %s', z_vector_req)
    log.debug('   x_vector_requested: %s', x_vector_req)
    # set the tolerance value
    epsilon = 0.0001
    # create np.array so we can easily calculate differences and check elements
    z_vector_req = np.array([z_vector_req[0], z_vector_req[1], z_vector_req[2]])
    # calculate joint values using kinematic specific formula
    try:
        (theta_1_calcd, theta_2_calcd) = kins_calc_possible_joint_angles(log, z_vector_req, x_vector_req)
    except Exception as error:
        log.error('Remap_funcs: kins_calc_possible_joint_angles failure, %s', error)

    # remove any duplicate values from the results
    theta_1_calcd = tuple(set(theta_1_calcd))
    theta_2_calcd = tuple(set(theta_2_calcd))
    log.debug('   Got possible angles theta_1: ' + ' '.join("{:.4f}°".format(degrees(theta)) for theta in theta_1_calcd))
    log.debug('   Got possible angles theta_2: ' + ' '.join("{:.4f}°".format(degrees(theta)) for theta in theta_2_calcd))
    if theta_1_calcd == None or theta_2_calcd == None:
        return []
    angle_pairs_list = []
    # create a list of paired combinations of returned angles (theta_1 , theta_2)
    for i in range(len(theta_1_calcd)):
        for j in range(len(theta_2_calcd)):
            angle_pairs_list.append((theta_1_calcd[i], theta_2_calcd[j]))
    angle_pairs_list = list(set(angle_pairs_list))
    # iterate through the list and check if a particular pair actually produces the requested z-vector orientation
    joint_angles_list = []
    for i in range(len(angle_pairs_list)):
        debug_msg = (f'   Checking angle pair {i}: ({angle_pairs_list[i][0]:.4f}, {angle_pairs_list[i][1]:.4f}) '
                     f'({degrees(angle_pairs_list[i][0]):.4f}°, {degrees(angle_pairs_list[i][1]):.4f}°)')
        log.debug(debug_msg)
        # we start with an identity matrix (ie oriented to world)
        matrix_in = np.asmatrix(np.identity(4))
        try:
            direction = kins_calc_transformation_get_direction()
        except Exception as error:
            log.error('kins_calc_transformation_get_direction, %s', error)
        try:
            matrix_out = kins_calc_transformation_matrix(angle_pairs_list[i][0], angle_pairs_list[i][1], 0, matrix_in, direction)
        except Exception as error:
            log.error('kins_calc_transformation_matrix, %s', error)
        # the resulting z-vector for this pair of (theta_1, theta_2) is found in the third column
        z_vector_would_be = np.array([matrix_out[0,2], matrix_out[1,2], matrix_out[2,2]])
        # calculate the difference of the respective elements
        z_vector_diff = z_vector_req - z_vector_would_be
        log.debug('   z_vector_diff: %s', z_vector_diff)
        # and check if all elements are within [-epsilon,epsilon]
        match_z = np.all((z_vector_diff > -epsilon) & (z_vector_diff < epsilon))
        log.debug('   Is the z-vector-vector close enough ? %s', match_z)
        if match_z:
            joint_angles_list.append((angle_pairs_list[i][0], angle_pairs_list[i][1]))
    for (theta_1, theta_2) in joint_angles_list:
        log.debug(f'Returning valid joint angles found: {degrees(theta_1):.4f}°, {degrees(theta_2):.4f}°')
    return joint_angles_list # returns radians


def calc_shortest_distance(pos, trgt, mode):
    pos = degrees(pos)
    trgt = degrees(trgt)
    # calculate the shortest distance in [-180°, 180°] eg if pos=170° and trgt=-170° then dist will be 20°
    # If the operator requests positive or negative rotation we may need to return the long distance instead
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    dist_short = (trgt - pos + 180) % 360 - 180
    # calculate short and long distance
    if dist_short >= 0: # ie dist_long should be negative
        dist_long = -(360 - dist_short)
    else:
        dist_long =  360 + dist_short
    log.debug(f'   Calculated dist_short: {dist_short:.4f}°, dist_long: {dist_long:.4f}°')
    if mode == 1: # positive rotation only, ie we want a positive distance
        if dist_short >= 0: # ie we want this one
            dist = dist_short
        else:  # ie we need to go the other way
            dist = dist_long
    elif mode == 2: # negative rotation only ie we want a positive distance
        if dist_short >  0: # ie we need to go the other way
            dist = dist_long
        else: # ie we want this one
            dist = dist_short
    else: # mode = 0 ie we want the shortest distance either way
        dist = dist_short
    log.debug(f'Returning distance: {dist:.4f}°')
    return radians(dist)


def calc_rotary_move_with_joint_limits(pos, trgt, max_limit, min_limit, mode): # expects radians
    # this takes a target angle in [-pi,pi] and finds the closest move within [min_limit, max_limit]
    # from a given position in [min_limit, max_limit], returns the optimized target angle and the distance
    # from the given position to that target angle
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    log.debug(f'   Current position: {degrees(pos):.4f}°, target position: {degrees(trgt):.4f}°')
    # calculate the shortest distance from position to target for the strategy given by
    # the operator (ie shortest (= default), positive rotation only, negative rotation only )
    dist = calc_shortest_distance(pos, trgt, mode)
    # check that the result is within the rotary axis limits defined in the ini file
    if dist >= 0: # shortest way is in the positive direction
        if (pos + dist) <=  max_limit: # if the limits allow we rotate the joint in the positive sense
            log.debug(f'   Max_limit OK, setting target to: {degrees(pos + dist):.4f}°')
            theta = pos + dist
        else: # if positive limits would be exceeded we need to go the longer way in the other direction
            log.debug(f'   Maximum axis limit of {degrees(max_limit):.4f} would be violated.')
            if mode == 0:
                dist = dist - 2*pi
                log.debug(f'   Changing target to: {degrees(trgt):.4f}°, distance to: {degrees(dist):.4f}°')
                theta = trgt
            else: # if the rotation direction was set by the operator then we can not change direction
                log.debug(f'   Unable to change direction because orient mode is set to {mode:.0f}.\n')
                theta = None
    else:  # shortest way is in the negative direction
        if (pos + dist) >=  min_limit: # if the limits allow we rotate the joint in the negative sense
            log.debug(f'   Min_limit OK, setting target to: {degrees(pos + dist):.4f}°')
            theta = pos + dist
        else: # if negative limits would be exceeded we need to go the longer way int the other direction
            log.debug(f'   Minimum axis limit of {degrees(min_limit):.4f} would be violated.')
            if mode == 0:
                dist = dist + 2*pi
                log.debug(f'   Changing target to: {degrees(trgt):.4f}°, distance to: {degrees(dist):.4f}°')
                theta = trgt
            else: # if the rotation direction was set by the operator then we can not change direction
                log.debug(f'   Unable to change direction because orient mode is set to {mode:.0f}.\n')
                theta = None
    if theta is not None:
        log.debug(f'Returning: angle {degrees(theta):.4f}° with distance {degrees(dist):.4f}° for requested mode {mode:.0f}\n')
    # we also attach the distance for this particular move and mode
    return theta, dist # returns radians


def calc_angle_pairs_and_distances(self, possible_prim_sec_angle_pairs): # expects radians
    # this takes a list of joint angle pairs in [-pi,pi] and optimizes them for shortest moves
    # in (min_limit, max_linit) from the current joint positions using the orient_mode set by
    # the operator: 0=shortest (default), 1=positive rotation only, 2=negative rotation only
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global primary_min_limit, primary_max_limit, secondary_min_limit, secondary_max_limit
    global orient_mode
    # get the current joint positions
    prim_pos, sec_pos = get_current_rotary_positions(self) # returns radians
    # we want to return a list of angles that are optimized for the orient_mode and the
    # rotary axes limits as set in the ini file
    target_dist_list= []
    for prim_trgt, sec_trgt in possible_prim_sec_angle_pairs:
        # For the priortized joint we apply the orient mode requested by the operator
        # the other we optimize for shortest move
        if optimization_priority == 2:
            primary_strategy = 0
            secondary_strategy = orient_mode
        else:
            primary_strategy = orient_mode
            secondary_strategy = 0
        # primary joint
        prim_move, prim_dist = calc_rotary_move_with_joint_limits(prim_pos, prim_trgt,
                                                                  primary_max_limit, primary_min_limit,
                                                                  primary_strategy)
        # secondary joint
        sec_move, sec_dist = calc_rotary_move_with_joint_limits(sec_pos, sec_trgt,
                                                                secondary_max_limit, secondary_min_limit,
                                                                secondary_strategy)
        # if a solution has been found for this particular pair then we add it to the list
        if not (prim_move == None) and not (sec_move == None):
            target_dist_list.append(((prim_move, sec_move),(prim_dist, sec_dist)))
    for ((prim_move, sec_move),(prim_dist, sec_dist)) in target_dist_list:
        debug_msg = (f'Returning prim_move: {degrees(prim_move):.4f}°, sec_move: {degrees(sec_move):.4f}°, '
                     f'prim_dist: {degrees(prim_dist):.4f}°, sec_dist: {degrees(sec_dist):.4f}°')
        log.debug(debug_msg)
    return target_dist_list # returns radians


def calc_optimal_joint_move(self, possible_prim_sec_angle_pairs):
    # find the optimal joint move from current to target positions in the list
    # orient_mode is 0=shortest, 1=positive rotation only, 2=negative rotation only
    # For orient_mode=(1,2): If no move can be found within joint limits we return None
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global orient_mode
    # this returns a list with all moves ((prim_move, sec_move),(prim_dist, sec_dist)) that
    # will result in correct tool orientation, stay within the rotary axis limits and respect the
    # orient_mode if set by the operator
    valid_joint_moves_and_distances = calc_angle_pairs_and_distances(self, possible_prim_sec_angle_pairs)
    if len(valid_joint_moves_and_distances) < 1:
            log.error(f'   No valid joint moves found.')
            return (None, None)
    # now we need to pick and return the (primary angle, secondary angle) that results in the
    # shortest move of the prioritized joint
    (theta_1, theta_2) = (None, None)
    joint = optimization_priority - 1
    dist = 10 # some large initial value
    for trgt_angles, dists in valid_joint_moves_and_distances:
        if orient_mode == 0 and fabs(dists[joint]) < fabs(dist): # shortest move requested
            (theta_1, theta_2) = trgt_angles
            dist = dists[0]
        elif orient_mode == 1 and fabs(dists[joint]) < fabs(dist) and dists[joint] >= 0: # positive primary rotation only
            (theta_1, theta_2) = trgt_angles
            dist = dists[0]
        elif orient_mode == 2 and fabs(dists[joint]) < fabs(dist) and dists[joint] <= 0: # negative primary rotation only
            (theta_1, theta_2) = trgt_angles
            dist = dists[0]
    if theta_1 is not None:
        debug_msg = (f'Returning shortest move selected for orient_mode {orient_mode:.0f}: '
                     f'primary: {degrees(theta_1):.4f}°, secondary: {degrees(theta_2):.4f}°\n')
        log.debug(debug_msg)
    return theta_1, theta_2 # returns radians


def calc_virtual_rotation(theta_1, theta_2, x_vector_req, z_vector_req, matrix_in, direction): # expects radians
    # calculates a required virtual-rotation around tool- or work-z so the x-vector matches the requested
    # orientation after rotation
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    # tolerance setting for check if x-vector-vector needs to be rotated at all
    epsilon = 0.00000001
    log.info("   x-vector-requested: %s", x_vector_req)
    debug_msg = (f'   got joint angles: primary {theta_1:.4f} {degrees(theta_1):.4f}°, '
                 f'secondary {theta_2:.4f}° {degrees(theta_2):.4f}°')
    log.debug(debug_msg)
    # run matrix_in through the kinematic transformation in the requested direction
    # using the given joint angles and zero virtual-rotation
    try:
        matrix_out = kins_calc_transformation_matrix(theta_1, theta_2, 0, matrix_in, direction)
    except Exception as error:
        log.error('calc_virtual_rotation, %s', error)
    # the x-vector for the given machine joint rotations is found directly in the first column
    x_vector_is = [matrix_out[0,0], matrix_out[1,0], matrix_out[2,0]]
    log.debug("   X-vector after machine rotation would be: %s", x_vector_is)
    # we calculate the angular difference between the two vectors so we can add a virtual rotation
    # around z-vector or work-z to match the requested x orientation after machine rotation
    # just to be sure we normalize the two vectors
    x_vector_is = x_vector_is / np.linalg.norm(x_vector_is)
    x_vector_req = x_vector_req / np.linalg.norm(x_vector_req)
    # check if the x-vector is already in the required orientation (ie parallel)
    log.debug("   checking if vectors are parallel: %s",  np.dot(x_vector_is,x_vector_req))
    if np.dot(x_vector_is, x_vector_req) >  1 - epsilon:
        log.info("   X-vector already oriented, setting virtual-rotation = 0")
        # if we are already parallel then we don't need to add a virtual rotation
        virtual_rot = 0
    else:
        # we can use the cross product to determine the direction we need to rotate
        cross = np.cross(x_vector_req, x_vector_is)
        log.debug("   cross product (x_vector_req, x_vector_is): %s", cross)
        virtual_rot = np.arccos(np.dot(x_vector_req, x_vector_is))
        log.debug(f'   raw virtual_rot: {virtual_rot:.4f} {degrees(virtual_rot):.4f}°')
        # To find out which quadrant we need the angle to be in we create a list of them all
        virtual_rot_list = [virtual_rot, -virtual_rot, 2*pi-virtual_rot, -(2*pi-virtual_rot)]
        log.debug('   Got possible virtual_rot angles: ' + ' '.join("{:.4f}°".format(degrees(angle)) for angle in virtual_rot_list))
        # then we run all of them through the kinematic model and see which gives us the requested x-vector-vector
        for virtual_rot in virtual_rot_list:
            log.debug(f'   Checking virtual_rot = {degrees(virtual_rot):.4f}°')
            zeta = 0.0001
            # run the identity matrix through the kinematic transformation in the requested direction
            # using the given joint angles and virtual-rotation angle in the list
            try:
                matrix_out = kins_calc_transformation_matrix(theta_1, theta_2, virtual_rot, matrix_in, direction)
            except Exception as error:
                log.error('calc_virtual_rotation, %s', error)
            # the oriented x-vector is found directly in the first column
            x_vector_would_be = [matrix_out[0,0], matrix_out[1,0], matrix_out[2,0]]
            log.debug('   x_vector_would_be: %s', x_vector_would_be)
            # calculate the difference of the respective elements
            x_vector_diff = x_vector_req - x_vector_would_be
            # and check if all elements are within [-epsilon,epsilon]
            match = np.all((x_vector_diff > -zeta) & (x_vector_diff < zeta))
            log.debug('   Is the X-vector close enough ? %s', match)
            if match:
                # if we have a match we leave the loop and use this angle
                break
        log.info(f'Returning virtual-rotation calculated {degrees(virtual_rot):.4f}°')
    return virtual_rot # returns radians


def calc_twp_matrix_from_joint_position(self, matrix_in, virtual_rot, direction): # expects radians
    # transforms a 4x4 input matrix using the current transformation matrix
    # (forward or inverse) using the kinematic model of the machine
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global kins_virtual_rotation
    # read current spindle rotary angles (radians)
    theta_1, theta_2 = get_current_rotary_positions(self)
    # virtual-rot is the virtual rotary axis around the z-vector or work-z axis to align the x-vector
    log.debug(f"    requested virtual-rot value {degrees(virtual_rot):.4f}°")
    # run matrix_in through the kinematic transformation in the requested direction
    # using the current joint angles and virtual-rotation as requested
    try:
        twp_matrix = kins_calc_transformation_matrix(theta_1, theta_2, virtual_rot, matrix_in, direction)
    except Exception as error:
        log.error('calc_twp_matrix_from_joint_position, %s', error)
    return twp_matrix


def gui_update_twp():
    # The tilted-work-plane is created in identity mode and must NOT be updated after a switch
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global twp_matrix, saved_work_offset
    # twp origin as vector (in world coords) from current work-offset to the origin of the twp
    try:
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
    except Exception as error:
        log.error('gui_update_twp failed, %s', error)
    # publish the twp offset coordinates in world coordinates (ie identity)
    [work_offset_x, work_offset_y, work_offset_z] = saved_work_offset
    log.debug("   Setting work_offsets in the simulation: %s", (work_offset_x, work_offset_y, work_offset_z))
    # this is used to translate the rotated twp to the correct position
    # care must be taken that only the work_offsets in identity mode are sent as that is
    # what the model uses. The visuals for the offsets are created in the origin,
    # then rotated according to the rotary joint position and then translated.
    # The twp has to be rotated out of the machine xy plane using the g68.2 parameters and is then
    # translated by the offset values of the identity mode.
    try:
        hal.set_p("twp-helper-comp.twp-ox-world-in",str(work_offset_x))
        hal.set_p("twp-helper-comp.twp-oy-world-in",str(work_offset_y))
        hal.set_p("twp-helper-comp.twp-oz-world-in",str(work_offset_z))
    except Exception as error:
        log.error('gui_update_twp failed, %s', error)


# NOTE: Due to easier abort handling we currently restrict the use of twp to G54
# as LinuxCNC seems to revert to G54 as the default system
def get_current_work_offset(self):
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
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
    current_work_offset = (co_x, co_y, co_z)
    return [current_work_offset_number, current_work_offset]


def get_current_rotary_positions(self):
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global joint_letter_primary, joint_letter_secondary
    if joint_letter_primary == 'A':
        theta_1 = radians(self.AA_current)
    elif joint_letter_primary == 'B':
        theta_1 = radians(self.BB_current)
    elif joint_letter_primary == 'C':
        theta_1 = radians(self.CC_current)
    log.debug(f'   Current position Primary joint: {degrees(theta_1):.4f}°')
    # read current spindle rotary angles and convert to radians
    if joint_letter_secondary == 'A':
        theta_2 = radians(self.AA_current)
    elif joint_letter_secondary == 'B':
        theta_2 = radians(self.BB_current)
    elif joint_letter_secondary == 'C':
        theta_2 = radians(self.CC_current)
    log.debug(f'   Current position Secondary joint: {degrees(theta_2):.4f}°')
    return theta_1, theta_2


def reset_twp_params():
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global virtual_rot, twp_matrix, twp_flag, twp_build_params
    virtual_rot = 0
    # we must not change tool kins parameters when TOOL kins are active or we get sudden joint position changes
    # ie don't do this: kins_comp_set_virtual_rot(0)!
    twp_flag = []
    twp_build_params = {}
    log.info("   Resetting TWP-matrix")
    twp_matrix = np.asmatrix(np.identity(4))


def g53n_core(self):
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    # Orient the tool to the current twp (with TCP for G53.1 or IDENTITY for G53.6)
    # Note: To avoid that this python code is run prematurely by the read ahead we need a quebuster at the
    # beginning but because we need self.execute() to switch the WCS properly this remap needs to be called from
    # an ngc reamp that contains a quebuster before calling this code.
    # IMPORTANT:
    # The correct kinematic mode (ie TCP for 53.1 / IDENTITY for G53.6) must be active when this code is called
    # (ie do it in the ngc remap mentioned above!)
    global saved_work_offset, twp_matrix, twp_flag, virtual_rot
    global joint_letter_primary, joint_letter_secondary, twp_error_status
    global orient_mode
    if self.task == 0: # ignore the preview interpreter
        yield INTERP_EXECUTE_FINISH
        return INTERP_OK

    if not  hal.get_value(twp_is_defined):
         # reset the twp parameters
        reset_twp_params()
        msg = "G53.n: No TWP defined."
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR
    elif hal.get_value(twp_is_active):
         # reset the twp parameters
        reset_twp_params()
        msg = "G53.n: TWP already active"
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # Check if any words have been passed with the respective G53.n command
    c = self.blocks[self.remap_level]
    p = c.p_number if c.p_flag else 0
    x = c.i_number if c.i_flag else None
    y = c.j_number if c.j_flag else None
    z = c.k_number if c.k_flag else None
    log.debug('    G53.n Words passed: (P, X,Y,Z): %s', (p,x,y,z))

    if p not in [0,1,2]:
        # reset the twp parameters
        reset_twp_params()
        msg = "G53.n : unrecognised P-Word found."
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    orient_mode = p
    z_vector_requested = [twp_matrix[0,2],twp_matrix[1,2],twp_matrix[2,2]]
    x_vector_requested = [twp_matrix[0,0],twp_matrix[1,0],twp_matrix[2,0]]
    # calculate all possible pairs of (primary, secondary) angles to matches the requested orientation
    try:
        # angles are returned in [-pi,pi]
        possible_prim_sec_angle_pairs = calc_joint_angles(z_vector_requested, x_vector_requested) # returns radians
    except Exception as error:
        log.error('calc_joint_angles, %s', error)
        # reset the twp parameters
        reset_twp_params()
        msg = ("G53.n ERROR: Calculation of joint angles has failed. -> aborting G53.n")
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    if possible_prim_sec_angle_pairs == []:
        # reset the twp parameters
        log.error('G53.n: No possible primary/secondary angle pairs found.')
        reset_twp_params()
        msg = "G53.n ERROR: Requested tool orientation not reachable -> aborting G53.n"
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # this returns one pair of optimized angles in degrees, or (None, None) if no solution could be found
    try:
        theta_1, theta_2 = calc_optimal_joint_move(self, possible_prim_sec_angle_pairs) # returns radians
    except Exception as error:
        log.error('G53.n: Calculation of optimal joint move failed, %s', error)
    if theta_1 == None or theta_2 == None:
         # reset the twp parameters
        reset_twp_params()
        msg = ("G53.n ERROR: Requested tool orientation not reachable -> aborting G53.n")
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # get the particular conditions to be met for the kinematic at hand
    try:
        (x_vector_requested, z_vector_requested, matrix_in, direction) = kins_calc_virtual_rot_get_values(x_vector_requested,
                                                                                               z_vector_requested,
                                                                                               twp_matrix)
    except Exception as error:
        log.error('G53.n: kins_calc_virtual_rot_get_values failed, %s', error)
    # calculate the virtual-rotation needed
    virtual_rot = calc_virtual_rotation(theta_1,
                                        theta_2,
                                        x_vector_requested,
                                        z_vector_requested,
                                        matrix_in,
                                        direction) # returns radians
    log.debug(f"   Calculated virtual-rotation to match requested x-vector: {degrees(virtual_rot):.4f}°")

    # mark twp-flag as active
    twp_flag = [0, 'active']
    gui_update_twp()

    # set the virtual-rotation value in the kinematic component
    debug_msg = (f'   G53.n: Setting angle values in kins comp to theta1: {degrees(theta_1):.4f}°, '
                 f'theta2: {degrees(theta_2):.4f}°, virtual_rot: {degrees(virtual_rot):.4f}°')
    log.debug(debug_msg)
    try:
        kins_set_values(theta_1, theta_2, virtual_rot)
    except Exception as error:
        log.error('G53.n: kins_set_values failed, %s', error)

    # calculate the work offset in transformed-coordinatess
    log.debug("   G53.n: Saved work offset: %s", saved_work_offset)
    twp_offset = (twp_matrix[0,3],twp_matrix[1,3],twp_matrix[2,3])
    try:
        new_offset = kins_calc_transformed_work_offset(saved_work_offset, twp_offset, theta_1, theta_2, virtual_rot)
    except Exception as error:
        log.error('G53.n: Calculation of kins_calc_transformed_work_offset failed, %s', error)
    debug_msg = (f'   G53.n: Setting transformed work-offsets for twp-kins in G59, G59.1, '
                 f'G59.2 and G59.3 to: {new_offset[0]:.4f}, {new_offset[1]:.4f}, {new_offset[2]:.4f}')
    log.debug(debug_msg)
    # set the dedicated TWP work offset values (G53, G53.1, G53.2, G53.3)
    self.execute("G10 L2 P6 X%f Y%f Z%f" % (new_offset[0], new_offset[1], new_offset[2]), lineno())
    self.execute("G10 L2 P7 X%f Y%f Z%f" % (new_offset[0], new_offset[1], new_offset[2]), lineno())
    self.execute("G10 L2 P8 X%f Y%f Z%f" % (new_offset[0], new_offset[1], new_offset[2]), lineno())
    self.execute("G10 L2 P9 X%f Y%f Z%f" % (new_offset[0], new_offset[1], new_offset[2]), lineno())

    log.debug(f"   G53.n: Moving primary joint to {degrees(theta_1):.4f}° and secondary joint to {degrees(theta_2):.4f}° ")
    if (x,y,z) == (None,None,None):
        # Move rotary joints to align the tool and the requested work plane
        self.execute("G0 %s%f %s%f" % (joint_letter_primary, degrees(theta_1), joint_letter_secondary, degrees(theta_2)), lineno())
    # switch to the dedicated TWP work offsets
    self.execute("G59", lineno())
    # activate TWP kinematics
    self.execute("G12.1 P2")
    if (x,y,z) != (None,None,None):
        log.debug('   G53.3 called')
        self.execute("G0 X%s Y%s Z%s %s%f %s%f" %
            (x, y, z, joint_letter_primary, degrees(theta_1), joint_letter_secondary, degrees(theta_2)), lineno())
    # set twp-state to 'active' (2)
    self.execute("M68 E2 Q2")
    yield INTERP_EXECUTE_FINISH
    return INTERP_OK


# Cancel an active TWP definition and reset the parameters to zero
# Note: To avoid that this python code is run prematurely by the read ahead we need a quebuster at the beginning but
# because we need self.execute() to switch the WCS properly this remap needs to be called from
# an ngc that contains a quebuster before calling this code
def g69_core(self):
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global twp_flag, saved_work_offset_number, saved_work_offset
    if self.task == 0: # ignore the preview interpreter
        yield INTERP_EXECUTE_FINISH
        return INTERP_OK
    log.info('G69 called')
    # reset the twp parameters
    reset_twp_params()
    gui_update_twp()
    # set twp-state to 'undefined' (0)
    self.execute("M68 E2 Q0")
    yield INTERP_EXECUTE_FINISH
    return INTERP_OK


# define a virtual tilted-work-plane (twp) that is perpendicular to the current tool-orientation
def g683(self, **words):
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global twp_matrix, virtual_rot, twp_flag, saved_work_offset_number, saved_work_offset

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
        reset_twp_params()
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
        reset_twp_params()
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
    # parse the requested rotation of x-vector around the origin
    r = radians(c.r_number) if c.r_flag else 0

    twp_flag = [0, 1, 'empty'] # one call to define the twp in this mode
    theta_1, theta_2 = get_current_rotary_positions(self) # radians
    # calculate virtual rotation to have the oriented x-vector in the direction required for the kinematic at hand
    try:
        virtual_rot = kins_calc_virtual_rot_for_g683(theta_1, theta_2 )
    except Exception as error:
        log.error('remap_func: kins_calc_virtual_rot_for_g683 failed, %s', error)
    log.info("G68.3: virtual-Rotation calculated for x-vector in machine-xy plane [deg]:  %s", degrees(virtual_rot))
    # then we need to calculate the transformation matrix of the current orientation with the including the
    # calculated virtual-rotation.
    # for this we take the 4x4 identity matrix and pass it through the kinematic transformation using the
    # current rotary joint positions and the calculated virtual-rotation angle plus any additional angle
    # passed in the R word of the G68.3 command
    start_matrix = np.asmatrix(np.identity(4))
    log.info('G68.3: Requested R-word rotation [deg]: %s', degrees(r))
    # the required transformation direction may depend on the kinematic at hand
    try:
        direction = kins_calc_transformation_get_direction()
    except Exception as error:
        log.error('kins_calc_transformation_get_direction, %s', error)
    twp_matrix = calc_twp_matrix_from_joint_position(self, start_matrix, virtual_rot + r, direction)
    log.debug("G68.3: TWP matrix with oriented x-vector: \n%s", twp_matrix)
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

    gui_update_twp()
    return INTERP_OK


# definition of a virtual work-plane (twp) using different methods set by the 'p'-word
def g682(self, **words):
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global twp_matrix, virtual_rot, twp_flag, twp_build_params, saved_work_offset_number, saved_work_offset

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
        reset_twp_params()
        msg = ("G68.2: TWP already defined.")
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # NOTE: Due to easier abort handling we currently restrict the use of twp to G54
    # as LinuxCNC seems to revert to G54 as the default system
    (n, offsets) = get_current_work_offset(self)
    if n != 1:
         # reset the twp parameters
        reset_twp_params()
        msg = "G68.2 ERROR: Must be in G54 to define TWP."
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # collect the currently active work offset values (ie g54, g55 or other)
    saved_work_offset_number = n
    saved_work_offset = offsets
    log.debug("   G68.2: Saved work offsets %s", (n, saved_work_offset))

    c = self.blocks[self.remap_level]
    p = c.p_number if c.p_flag else 0
    if p == 0: # true euler angles (this is the default mode)
        twp_flag = [int(p), 1, 'empty'] # one call to define the twp in this mode
        # parse requested order of rotations (default is '313' ie: ZXZ)
        q = str(int(c.q_number if c.q_flag else 313))
        if q not in ['121','131','212','232','313','323']:
             # reset the twp parameters
            reset_twp_params()
            msg = ("G68.2 (P0): No recognised Q-Word found.")
            log.debug('   ' + msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested xy-rotation around the origin
        r = radians(c.r_number) if c.r_flag else 0
        # parse the requested euler rotation angles
        th1 = radians(c.i_number) if c.i_flag else 0
        th2 = radians(c.j_number) if c.j_flag else 0
        th3 = radians(c.k_number) if c.k_flag else 0

        # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # create the rotation matrix for the requested origin rotation
        try:
            twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
        except Exception as error:
            log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
        log.debug('   G68.2 (P0): Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('   G68.2 (P0): Twp_euler_rotation \n%s',twp_euler_rotation)
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
            reset_twp_params()
            msg = ("G68.2 P1: No recognised Q-Word found.")
            log.debug('   ' + msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested xy-rotation around the origin
        r = radians(c.r_number) if c.r_flag else 0
        # parse the requested euler rotation angles
        th1 = radians(c.i_number) if c.i_flag else 0
        th2 = radians(c.j_number) if c.j_flag else 0
        th3 = radians(c.k_number) if c.k_flag else 0

         # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # create the rotation matrix for the requested origin rotation
        try:
            twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
        except Exception as error:
            log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
        log.debug('   G68.2 P1: Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('   G68.2 P1: Twp_euler_rotation \n%s',twp_euler_rotation)
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
        # TODO implement operator errors as outlined in the twp README
        #- G68.2 P2 (Q0),Q1,Q2,Q3 commands are not entered consecutively
        #- two to the points entered in Q1,Q2,Q3 are identical
        #- all three points entered in Q1,Q2,Q3 are on a line
        #- the distance between a line defined by any two points entered in (Q1,Q2,Q3) and
        #the remaining point  is less than 10mm or 0.5inch (just some arbitrary values for now)

        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            twp_flag = [int(p), 4 , 'empty', 'empty', 'empty', 'empty'] # four calls needed
            twp_build_params = {'q0':[], 'q1':[], 'q2':[], 'q3':[]}
        # Point 1: defines the origin of the twp
        # Point 2: direction from P1 to P2 defines the positive x direction on the twp (x-vector)
        # Point 3: defines the positive y side and with P1 and P2 defines the xy work plane (z-vector)
        q = int(c.q_number if c.q_flag else 0)
        # this mode needs four calls to fill all required parameters
        if q == 0: # define new origin and rotation
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested xy-rotation around the origin
            r = radians(c.r_number) if c.r_flag else 0
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
            reset_twp_params()
            msg = ("G68.2 P2: No recognised Q-Word found.")
            log.debug('   ' + msg)
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
            log.debug("   G68.2 P2: Point 1: %s",p1)
            log.debug("   G68.2 P2: Point 2: %s",p2)
            log.debug("   G68.2 P2: Point 3: %s",p3)
            # build vectors x:P1->P2 and v2:P1->P3
            twp_vect_x = [p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]]
            log.debug("   G68.2 P2: Twp_vect_x: \n%s",twp_vect_x)
            v2 = [p3[0]-p1[0], p3[1]-p1[1], p3[2]-p1[2]]
            log.debug("   G68.2 P2 (v2): %s",v2)
            # normalize the two vectors
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            v2 = v2 / np.linalg.norm(v2)
            # we can use the cross product to calculate the z-vector vector
            # note: if P3 is on the right side of the vector P1->P2
            # then the z-vector will be below the twp (ie z-vector will be downwards)
            twp_vect_z = np.cross(twp_vect_x , v2)
            log.debug("   G68.2 P2: Twp_vect_z %s",twp_vect_z)
            # we can use the cross product to calculate the y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("   G68.2 P2: Twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("   G68.2 P2: Built the twp-rotation-matrix: \n%s", twp_vect_rotation)
            # create the rotation matrix for the requested origin rotation
            try:
                twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
            except Exception as error:
                log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
            log.debug('   G68.2 P2: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("   G68.2 P2: Built twp-transformation-matrix: \n%s", twp_matrix)

    elif p == 3: # two vectors (vector 1 defines the x-vector and vector 2 defines the z-vector)
        # TODO implement operator errors as outlined in the twp README
        #- G68.2 P3 Q1 and Q2 commands are not entered consecutively
        #- one of the vectors is the zero vector
        #- the enclosed angle between the 1. and 2. vector is <85° or >95° (re fanuc twp pdf)
        q = int(c.q_number if c.q_flag else 0)
        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            log.info('   first call')
            twp_flag = [int(p), 2 , 'empty', 'empty'] # two calls needed
            twp_build_params = {'q0':[], 'q1':[]}
        log.debug('   twp_build_params: %s', twp_build_params)
        if q == 0: # define new origin of the twp
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested xy-rotation around the origin
            r = radians(c.r_number) if c.r_flag else 0
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
            reset_twp_params()
            msg = ("G68.2 P3: No recognised Q-Word found.")
            log.debug('   ' + msg)
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
            # build unit vector defining x-vector direction
            twp_vect_x = [i-x, j-y, k-z]
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            twp_vect_z = [i1, j1, k1]
            twp_vect_z = twp_vect_z / np.linalg.norm(twp_vect_z)
            orth = np.dot(twp_vect_x, twp_vect_z)
            log.debug("   orth check: %s", orth)
            # the two vectors must be orthogonal
            if orth > 0.001:
                reset_twp_params()
                msg = ("G68.2 P3: Vectors are not orthogonal.")
                log.debug('   ' + msg)
                emccanon.CANON_ERROR(msg)
                yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
                yield INTERP_EXIT # w/o this the error does not abort a running gcode program
                return INTERP_ERROR

            # we can use the cross product to calculate the y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("   G68.2 P3: twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("   G68.2 P3: Built twp-rotation-matrix: \n%s", twp_vect_rotation)
            # create the rotation matrix for the requested origin rotation
            try:
                twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
            except Exception as error:
                log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
            log.debug('   G68.2 P3: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_origin = [[x], [y], [z]]
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("   G68.2 P3: Built twp-transformation-matrix: \n%s", twp_matrix)

    # TODO implement G68.2 P4 as outlined in the fanuc twp pdf (the exact meaning of which is unclear to me)

    else:
         # reset the twp parameters
        reset_twp_params()
        msg = ("G68.2: No recognised P-Word found.")
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    log.debug("   G68.2: twp_flag: %s", twp_flag)
    log.debug("   G68.2: calls required: %s", twp_flag.count('done'))
    log.debug("   G68.2: number of calls made: %s", twp_flag.count('done'))

    if twp_flag.count('done') == twp_flag[1]:
        log.info('   G68.2: requested rotation (degrees): %s', degrees(r))
        log.info("   G68.2: twp-tranformation-matrix: \n%s",twp_matrix)
        twp_origin = [twp_matrix[0,3],twp_matrix[1,3],twp_matrix[2,3]]
        log.info("   G68.2: twp origin: %s", twp_origin)
        twp_vect_x = [twp_matrix[0,0],twp_matrix[1,0],twp_matrix[2,0]]
        log.info("   G68.2: twp vector-x: %s", twp_vect_x)
        twp_vect_z = [twp_matrix[0,2],twp_matrix[1,2],twp_matrix[2,2]]
        log.info("   G68.2: twp vector-z: %s", twp_vect_z)
        # set twp-state to 'defined' (1)
        self.execute("M68 E2 Q1")
        yield INTERP_EXECUTE_FINISH

        gui_update_twp()
    return INTERP_OK


# incremental definition of  a virtual work-plane (twp) using different methods set by the 'p'-word
def g684(self, **words):
    log.debug('Entering: %s', sys._getframe(  ).f_code.co_name)
    global twp_matrix, virtual_rot, twp_flag, twp_build_params, saved_work_offset_number, saved_work_offset

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
        reset_twp_params()
        msg = ("G68.4: No TWP active to increment from. Run G68.2 or G68.3 first.")
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    # collect the currently active work offset values (ie g54, g55 or other)
    n = get_current_work_offset(self)[0]
    # Must be in one of the dedicated offset systems for TWP
    if False: #n < 6:
         # reset the twp parameters
        reset_twp_params()
        msg = ("G68.4 ERROR: Must be in G59, G59.x to increment TWP.")
        log.debug('   ' + msg)
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
            reset_twp_params()
            msg = ("G68.4 (P0): No recognised Q-Word found.")
            log.debug('   ' + msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested xy-rotation around the origin
        r = radians(c.r_number) if c.r_flag else 0
        # parse the requested euler rotation angles
        th1 = radians(c.i_number) if c.i_flag else 0
        th2 = radians(c.j_number) if c.j_flag else 0
        th3 = radians(c.k_number) if c.k_flag else 0

        # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # create the rotation matrix for the requested origin rotation
        try:
            twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
        except Exception as error:
            log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
        log.debug('   G68.4 (P0): Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('   G68.4 (P0): Twp_euler_rotation \n%s',twp_euler_rotation)
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
            reset_twp_params()
            msg = ("G68.4 P1: No recognised Q-Word found.")
            log.debug('   ' + msg)
            emccanon.CANON_ERROR(msg)
            yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
            yield INTERP_EXIT # w/o this the error does not abort a running gcode program
            return INTERP_ERROR

        # parse the requested origin
        x = c.x_number if c.x_flag else 0
        y = c.y_number if c.y_flag else 0
        z = c.z_number if c.z_flag else 0
        # parse the requested xy-rotation around the origin
        r = radians(c.r_number) if c.r_flag else 0
        # parse the requested euler rotation angles
        th1 = radians(c.i_number) if c.i_flag else 0
        th2 = radians(c.j_number) if c.j_flag else 0
        th3 = radians(c.k_number) if c.k_flag else 0

         # build the translation vector of the twp_matrix
        twp_origin = [[x], [y], [z]]
        # create the rotation matrix for the requested origin rotation
        try:
            twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
        except Exception as error:
            log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
        log.debug('   G68.4 P1: Twp_origin_rotation \n%s',twp_origin_rotation)
        # build the rotation matrix for the requested euler rotation
        twp_euler_rotation = calc_euler_rot_matrix(th1, th2, th3, q)
        log.debug('   G68.4 P1: Twp_euler_rotation \n%s',twp_euler_rotation)
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
        # TODO implement operator errors as outlined in the twp README
        #- G68.2 P2 (Q0),Q1,Q2,Q3 commands are not entered consecutively
        #- two to the points entered in Q1,Q2,Q3 are identical
        #- all three points entered in Q1,Q2,Q3 are on a line
        #- the distance between a line defined by any two points entered in (Q1,Q2,Q3) and
        #the remaining point  is less than 10mm or 0.5inch (just some arbitrary values for now)

        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            twp_flag = [int(p), 4 , 'empty', 'empty', 'empty', 'empty'] # four calls needed
            twp_build_params = {'q0':[], 'q1':[], 'q2':[], 'q3':[]}
        # Point 1: defines the origin of the twp
        # Point 2: direction from P1 to P2 defines the positive x direction on the twp (x-vector)
        # Point 3: defines the positive y side and with P1 and P2 defines the xy work plane (z-vector)
        q = int(c.q_number if c.q_flag else 0)
        # this mode needs four calls to fill all required parameters
        if q == 0: # define new origin and rotation
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested xy-rotation around the origin
            r = radians(c.r_number) if c.r_flag else 0
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
            reset_twp_params()
            msg = ("G68.4 P2: No recognised Q-Word found.")
            log.debug('   ' + msg)
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
            log.debug("   G68.4 P2: Point 1: %s",p1)
            log.debug("   G68.4 P2: Point 2: %s",p2)
            log.debug("   G68.4 P2: Point 3: %s",p3)
            # build vectors x:P1->P2 and v2:P1->P3
            twp_vect_x = [p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]]
            log.debug("   G68.4 P2: Twp_vect_x: \n%s",twp_vect_x)
            v2 = [p3[0]-p1[0], p3[1]-p1[1], p3[2]-p1[2]]
            log.debug("   G68.4 P2: (v2) %s", v2)
            # normalize the two vectors
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            v2 = v2 / np.linalg.norm(v2)
            # we can use the cross product to calculate the z-vector vector
            # note: if P3 is on the right side of the vector P1->P2
            # then the z-vector will be below the twp (ie z-vector will be downwards)
            twp_vect_z = np.cross(twp_vect_x , v2)
            log.debug("   G68.4 P2: Twp_vect_z %s",twp_vect_z)
            # we can use the cross product to calculate the y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("   G68.4 P2: Twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("   G68.4 P2: Built the twp-rotation-matrix: \n%s", twp_vect_rotation)
            # create the rotation matrix for the requested origin rotation
            try:
                twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
            except Exception as error:
                log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
            log.debug('   G68.4 P2: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("   G68.4 P2: Built twp-transformation-matrix: \n%s", twp_matrix)

    elif p == 3: # two vectors (vector 1 defines the x-vector and vector 2 defines the z-vector)
        # TODO implement operator errors as outlined in the twp README
        #- G68.2 P3 Q1 and Q2 commands are not entered consecutively
        #- one of the vectors is the zero vector
        #- the enclosed angle between the 1. and 2. vector is <85° or >95° (re fanuc twp pdf)
        q = int(c.q_number if c.q_flag else 0)
        # if this is the first call for this mode reset the twp_flag flag
        if not twp_flag:
            twp_flag = [int(p), 2 , 'empty', 'empty'] # two calls needed
            twp_build_params = {'q0':[], 'q1':[]}
        if q == 0: # define new origin and first vector (direction of x in the twp)
            x = c.x_number if c.x_flag else 0
            y = c.y_number if c.y_flag else 0
            z = c.z_number if c.z_flag else 0
            # parse the requested xy-rotation around the origin
            r = radians(c.r_number) if c.r_flag else 0
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
            reset_twp_params()
            msg = ("G68.4 P3: No recognised Q-Word found.")
            log.debug('   ' + msg)
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
            # build unit vector defining x-vector direction
            twp_vect_x = [i-x, j-y, k-z]
            twp_vect_x = twp_vect_x / np.linalg.norm(twp_vect_x)
            twp_vect_z = [i1, j1, k1]
            twp_vect_z = twp_vect_z / np.linalg.norm(twp_vect_z)
            orth = np.dot(twp_vect_x, twp_vect_z)
            log.debug("   orth check: %s", orth)
            # the two vectors must be orthogonal
            if orth != 0:
                 # reset the twp parameters
                reset_twp_params()
                ## reset the parameter values
                #twp_flag = [int(p), 2 , 'empty', 'empty'] # two calls needed
                #twp_build_params = {'q0':[], 'q1':[]}
                msg = ("G68.4 P3: Vectors are not orthogonal.")
                log.debug('   ' + msg)
                emccanon.CANON_ERROR(msg)
                yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
                yield INTERP_EXIT # w/o this the error does not abort a running gcode program
                return INTERP_ERROR

            # we can use the cross product to calculate the y vector
            twp_vect_y = np.cross(twp_vect_z, twp_vect_x)
            log.debug("   G68.4 P3: twp_vect_y %s",twp_vect_y)
            # build the rotation matrix of the twp_matrix from the calculated vectors
            # first stack the vectors (lists) and then flip diagonally (transpose)
            # so the vectors are now vertical
            twp_vect_rotation_t = np.vstack((twp_vect_x, twp_vect_y))
            twp_vect_rotation_t = np.vstack((twp_vect_rotation_t, twp_vect_z))
            twp_vect_rotation = np.transpose(twp_vect_rotation_t)
            log.debug("   G68.4 P3: Built twp-rotation-matrix: \n%s", twp_vect_rotation)
            # create the rotation matrix for the requested origin rotation
            try:
                twp_origin_rotation = kins_calc_twp_origin_rot_matrix(r)
            except Exception as error:
                log.error('remap_func: kins_calc_twp_origin_rot_matrix failed, %s', error)
            log.debug('   G68.4 P3: Twp-origin-rotation-matrix \n%s',twp_origin_rotation)
            # calculate the total twp_rotation using matrix multiplication
            twp_rotation = np.asmatrix(twp_origin_rotation) * np.asmatrix(twp_vect_rotation)
            # add the origin translation on the right
            twp_origin = [[x], [y], [z]]
            twp_matrix = np.hstack((twp_rotation, twp_origin))
            # expand to 4x4 array and make into a matrix
            twp_row_4 = [0,0,0,1]
            twp_matrix = np.vstack((twp_matrix, twp_row_4))
            twp_matrix = np.asmatrix(twp_matrix)
            log.debug("   G68.4 P3: Built twp-transformation-matrix: \n%s", twp_matrix)

    # TODO implement G68.4 P4 as outlined in the fanuc twp pdf (the exact meaning of which is unclear to me)

    else:
         # reset the twp parameters
        reset_twp_params()
        msg = ("G68.4: No recognised P-Word found.")
        log.debug('   ' + msg)
        emccanon.CANON_ERROR(msg)
        yield INTERP_EXECUTE_FINISH # w/o this the error message is not displayed
        yield INTERP_EXIT # w/o this the error does not abort a running gcode program
        return INTERP_ERROR

    log.debug("   G68.4: twp_flag: %s", twp_flag)
    log.debug("   G68.4: calls required: %s", twp_flag.count('done'))
    log.debug("   G68.4: number of calls made: %s", twp_flag.count('done'))

    if twp_flag.count('done') == twp_flag[1]:
        log.info('   G68.4: requested rotation (degrees) %s', degrees(r))
        log.info("   G68.4: twp_matrix_current: \n%s", twp_matrix_current)
        log.info("   G68.4: incremental twp_matrix requested: \n%s",twp_matrix)
        log.info("   G68.4: calculating new twp_matrix...")
        twp_matrix_new = twp_matrix_current * twp_matrix
        log.info("   G68.4: twp_matrix_new: \n%s",twp_matrix_new)
        twp_origin = [twp_matrix[0,3],twp_matrix[1,3],twp_matrix[2,3]]
        log.info("   G68.4: twp origin: %s", twp_origin)
        twp_vect_x = [twp_matrix[0,0],twp_matrix[1,0],twp_matrix[2,0]]
        log.info("   G68.4: twp vector-x: %s", twp_vect_x)
        twp_vect_z = [twp_matrix[0,2],twp_matrix[1,2],twp_matrix[2,2]]
        log.info("   G68.4: twp vector-z: %s", twp_vect_z)
        log.info("   G68.4: incremented twp_matrix: \n%s", twp_matrix_new)
        twp_matrix = twp_matrix_new
        # set twp-state to 'defined' (1)
        self.execute("M68 E2 Q1")
        yield INTERP_EXECUTE_FINISH

        gui_update_twp()
    return INTERP_OK
