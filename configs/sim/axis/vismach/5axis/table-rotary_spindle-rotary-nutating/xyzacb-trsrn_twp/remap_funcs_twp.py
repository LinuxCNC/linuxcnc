# This is imported by remap.py and contains twp functionality specific to the
# xyzacb-trsrn config, a machine with primary rotary C and secondary rotary B
#
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
#
import sys
import numpy as np
from math import sin,cos,tan,asin,acos,atan,atan2,sqrt,pi,degrees,radians,fabs
import hal


# set up parsing of the inifile
import os
import linuxcnc
# get the path for the ini file used to start this config
inifile = os.environ.get("INI_FILE_NAME")
# instantiate the LinuxCNC ini-parser
config = linuxcnc.ini(inifile)

## ROTARY JOINT LETTERS
# primary joint
joint_letter_primary = config.getstring('TWP', 'PRIMARY', fallback="").capitalize()
# secondary joint (ie the one closer to the tool)
joint_letter_secondary = config.getstring('TWP', 'SECONDARY', fallback="").capitalize()
# get the MIN/MAX limits of the respective rotary joint letters
category = 'AXIS_' +  joint_letter_primary
primary_min_limit = config.getreal(category, 'MIN_LIMIT', fallback=0.0)
primary_max_limit = config.getreal(category, 'MAX_LIMIT', fallback=0.0)
category = 'AXIS_' +  joint_letter_secondary
secondary_min_limit = config.getreal(category, 'MIN_LIMIT', fallback=0.0)
secondary_max_limit = config.getreal(category, 'MAX_LIMIT', fallback=0.0)

## CONNECTIONS TO THE KINEMATIC COMPONENT
# the module is named for the kinematics, its hal pins carry a "_kins" suffix
kins_comp = config.getstring('KINS', 'KINEMATICS', fallback="") + '_kins'
kins_nutation_angle = kins_comp + '.nut-angle'
kins_virtual_rotation = kins_comp + '.pre-rot'
kins_primary_rotation = kins_comp + '.primary-angle'
kins_secondary_rotation = kins_comp + '.secondary-angle'


# defines the kinematic model for (world <-> tool) coordinates of the machine at hand
# returns 4x4 transformation matrix for given angles and 4x4 input matrix
# NOTE: these matrices must be the same as the ones used to derive the kinematic model
def kins_calc_transformation_matrix(theta_1, theta_2, virtual_rot, matrix_in, direction='fwd'): # expects radians
    global kins_nutation_angle
    T_in = matrix_in
    ## Define 4x4 transformation for virtual rotation around tool-z to orient tool-x and -y
    Stc = sin(virtual_rot)
    Ctc = cos(virtual_rot)
    Rtc=np.matrix([[ Ctc, -Stc, 0, 0],
                   [ Stc,  Ctc, 0, 0],
                   [ 0 ,  0 ,   1, 0],
                   [ 0,   0 ,   0, 1]])

    ## Define 4x4 transformation for the primary joint
    # get the basic 3x3 rotation matrix (returns array)
    Rp = Rz(theta_1)
    # add fourth column on the right
    Rp = np.hstack((Rp, [[0],[0],[0]]))
    # expand to 4x4 array and make into a matrix
    row_4 = [0,0,0,1]
    Rp = np.vstack((Rp, row_4))
    Rp = np.asmatrix(Rp)

    ## Define 4x4 transformation matrix for the secondary joint
    # get the basic 3x3 rotation matrix (returns array)
    Rs = Ry(theta_2)
    # add fourth column on the right
    Rs = np.hstack((Rs, [[0],[0],[0]]))
    # expand to 4x4 array and make into a matrix
    row_4 = [0,0,0,1]
    Rs = np.vstack((Rs, row_4))
    Rs = np.asmatrix(Rs)

    # Additional definitions for nutating joint
    v = radians(hal.get_value(kins_nutation_angle))
    Sv = sin(v)
    Cv = cos(v)
    Ss = sin(theta_2)
    Cs = cos(theta_2)
    r = Cs + Sv*Sv*(1-Cs)
    s = Cs + Cv*Cv*(1-Cs)
    t = Sv*Cv*(1-Cs)
    # define rotation matrix for the secondary joint
    Rs=np.matrix([[     Cs, -Cv*Ss,  Sv*Ss, 0],
                  [  Cv*Ss,      r,      t, 0],
                  [ -Sv*Ss,      t,      s, 0],
                  [      0,      0,      0, 1]])

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


# calculates the primary joint position for a given tool-vector
# Note: this uses functions derived from the custom kinematic
def kins_calc_primary(log, z_vector_req, x_vector_req, theta_2_list=[]):
    global primary_min_limit, primary_max_limit
    global kins_nutation_angle
    epsilon = 0.000001
    theta_1_list=[]
    (Kzx, Kzy, Kzz) = (z_vector_req[0], z_vector_req[1], z_vector_req[2])
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
            # since we are using asin() we really have two solutions theta_1 and pi-theta_2
            for theta in [theta_1, transform_to_pipi(pi - theta_1)]:
                log.debug(f'    Checking possible primary angle {degrees(theta):.4f}° for limit violations.')
                if degrees(theta) > secondary_min_limit and degrees(theta) < secondary_max_limit:
                    theta_1_list.append(theta)
    return theta_1_list # returns radians


# calculates the secondary joint position for a given tool-vector
# secondary being the joint closest to the tool
# Note: this uses functions derived from the custom kinematic
def kins_calc_secondary(log, z_vector_req, x_vector_req):
    global secondary_min_limit, secondary_max_limit
    global kins_nutation_angle
    epsilon = 0.000001
    theta_2_list=[]
    (Kzx, Kzy, Kzz) = (z_vector_req[0], z_vector_req[1], z_vector_req[2])
    v = radians(hal.get_value(kins_nutation_angle))
    Sv = sin(v)
    Cv = cos(v)
    # This kinmatic has infinite results for the vertical tool orientation
    # so we explicitly define the angles for that specific case
    if Kzz > 1 - epsilon:
        theta_2 = 0
    # This kinematics nutation angle restricts the negative range of Kzz
    elif Kzz < 2*Cv*Cv - 1:
        log.error('remap_funcs: Requested orientation not reachable with the current nutation angle.')
        return None
    else:
        theta_2 = acos((Kzz - Cv*Cv)/(1 - Cv*Cv))
    for theta in [theta_2, -theta_2]:
        log.debug(f'    Checking possible secondary angle {degrees(theta):.4f}° for limit violations.')
        if degrees(theta) > secondary_min_limit and degrees(theta) < secondary_max_limit:
            theta_2_list.append(theta)
    return theta_2_list # returns radians


# define the order in which the joint angles need to be calculated
def kins_calc_possible_joint_angles(log, z_vector_req, x_vector_req):
    try:
        theta_2_calcd = kins_calc_secondary(log, z_vector_req, x_vector_req)
    except Exception as error:
        log.error('kins_calc_jnt_angles, kins_calc_secondary, %s', error)
    if theta_2_calcd == None:
        return (None, None)
    try:
        theta_1_calcd = kins_calc_primary(log, z_vector_req, x_vector_req, theta_2_calcd)
    except Exception as error:
        log.error('kins_calc_jnt_angles, kins_calc_primary, %s', error)
    return (theta_1_calcd, theta_2_calcd) # returns radians


# calculate the transformed work offset used after 53.n
def kins_calc_transformed_work_offset(current_offset, twp_offset, theta_1, theta_2, virtual_rot):
    P = matrix_to_point(kins_calc_transformation_matrix(theta_1, theta_2, virtual_rot, point_to_matrix(current_offset)))
    # calculate the twp offset in transformed-coordinates
    Q = matrix_to_point(kins_calc_transformation_matrix(theta_1, theta_2, virtual_rot, point_to_matrix(twp_offset)))
    transformed_offset = (P[0]+Q[0], P[1]+Q[1], P[2]+Q[2])
    return transformed_offset

# pass required values to the kinematics component
# the module takes the virtual rotation in radians and the two joint angles in
# degrees, the same units the joints themselves are in
def kins_set_values(theta_1, theta_2, virtual_rot): # expects radians
    hal.set_p(kins_virtual_rotation, str(virtual_rot))
    hal.set_p(kins_primary_rotation, str(degrees(theta_1)))
    hal.set_p(kins_secondary_rotation, str(degrees(theta_2)))


# returns angle required to orient the x-vector parallel to the machine-xy plane
# for given machine joint position angles.
# For G68.3 this is the default tool-x direction
# NOTE: this uses formulas derived from the transformation matrix in the inverse tool kinematic
# TODO I don't actually know if this is the correct x orientation for G68.3'
def kins_calc_virtual_rot_for_g683(theta_1, theta_2):
    # The idea is that the oriented x-vector is parallel to the machine xy-plane when the
    # z component of the x-direction vector is equal to zero
    # Mathematically we take the symbolic formula found in row 3, column 1 of the transformation
    # matrix from the inverse tool-kinematics, equal that to zero and solve for 'tc'.
    # this makes the x-vector of the oriented coords horizontal and the user can set the
    # rotation from there using g68.3 r
    global kins_nutation_angle
    v = radians(hal.get_value(kins_nutation_angle))
    Cv = cos(v)
    Sv = sin(v)
    Cs = cos(theta_2)
    Ss = sin(theta_2)
    Cp = cos(theta_1)
    Sp = sin(theta_1)
    t = Sv*Cv*(1-Cs)
    tc = atan2((Sv*Ss),t)
    # note: rotation is done using a halpin that feeds into the kinematic component and the
    # vismach model. In contrast to a gcode command where 'c' refers to a physical machine joint)
    return tc # returns radians


# return the start values required to calculate the virtual rotation
def kins_calc_virtual_rot_get_values(x_vector_requested, z_vector_requested, twp_matrix):
    x_vector_requested = [twp_matrix[0,0],twp_matrix[1,0],twp_matrix[2,0]]
    z_vector_requested = [twp_matrix[0,2],twp_matrix[1,2],twp_matrix[2,2]]
    matrix_in = np.asmatrix(np.identity(4))
    direction = 'inv'
    return (x_vector_requested, z_vector_requested, matrix_in, direction)


# If the operator has requested a rotation by passing an R word in the 68.n command we need to
# create a rotation matrix that represents a rotation around the Z-axis of the TWP plane
def kins_calc_twp_origin_rot_matrix(r): # expects radians
    # we use xzx-euler rotation to create the rotation matrix for the requested origin rotation
    twp_origin_rot_matrix = calc_euler_rot_matrix(0, r, 0, '131')

    return twp_origin_rot_matrix


# This returns which transformation to use when checking calculated angles
# and when calculating the twp_matrix for G68.3
def kins_calc_transformation_get_direction():
    return 'inv'


# returns the pin name for the virtual rotation in the kinematics component
def kins_get_current_virtual_rot():
    current_virtual_rot = hal.get_value(kins_virtual_rotation)
    return current_virtual_rot # returns radians







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


# this is from 'mika-s.github.io'
# transforms a given angle to the interval of [-pi,pi]
def transform_to_pipi(input_angle):
    def truncated_remainder(dividend, divisor):
        divided_number = dividend / divisor
        divided_number = -int(-divided_number) if divided_number < 0 else int(divided_number)
        remainder = dividend - divisor * divided_number
        return remainder

    revolutions = int((input_angle + np.sign(input_angle) * pi) / (2 * pi))
    p1 = truncated_remainder(input_angle + np.sign(input_angle) * pi, 2 * pi)
    p2 = (np.sign(np.sign(input_angle)
                  + 2 * (np.sign(fabs((truncated_remainder(input_angle + pi, 2 * pi)) / (2 * pi))) - 1))) * pi
    output_angle = p1 - p2
    return output_angle


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
def calc_euler_rot_matrix(th1, th2, th3, order):
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
    return matrix
