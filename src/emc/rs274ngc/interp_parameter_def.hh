/**
 * @file interp_parameter_def.hh
 * Named global parameters used by the interpreter.
 *
 * This file exists mostly to avoid magic numbers in interpreter code.
 *
 * @author Robert W. Ellenberg <rwe24g@gmail.com>
 *
 * Copyright (c) 2019, Robert W. Ellenberg
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License (V2) as published by the Free Software Foundation.
 */

#ifndef INTERP_PARAMETER_DEF_H
#define INTERP_PARAMETER_DEF_H

namespace interp_param_global
{
// 31-5000 - G code user parameters. These parameters are global in the G code file, and available for general use. Volatile.
enum InterpParameterIndex {
// 5061-5069 - Coordinates of a G38 probe result (X, Y, Z, A, B, C, U, V & W). Coordinates are in the coordinate system in which the G38 took place. Volatile.
    G38_X=5061,
    G38_Y,
    G38_Z,
    G38_A,
    G38_B,
    G38_C,
    G38_U,
    G38_V,
    G38_W,
// 5070 - G38 probe result: 1 if success, 0 if probe failed to close. Used with G38.3 and G38.5. Volatile.
    G38_TRIPPED=5070,
// 5161-5169 - "G28" Home for X, Y, Z, A, B, C, U, V & W. Persistent.
    G28_X=5161,
    G28_Y,
    G28_Z,
    G28_A,
    G28_B,
    G28_C,
    G28_U,
    G28_V,
    G28_W,
// 5181-5189 - "G30" Home for X, Y, Z, A, B, C, U, V & W. Persistent.
    G30_X=5181,
    G30_Y,
    G30_Z,
    G30_A,
    G30_B,
    G30_C,
    G30_U,
    G30_V,
    G30_W,
// 5210 - 1 if "G92" offset is currently applied, 0 otherwise. Persistent.
// 5211-5219 - "G92" offset for X, Y, Z, A, B, C, U, V & W. Persistent.
    G92_APPLIED=5210,
    G92_X=5211,
    G92_Y,
    G92_Z,
    G92_A,
    G92_B,
    G92_C,
    G92_U,
    G92_V,
    G92_W,
// 5220 - Coordinate System number 1 - 9 for G54 - G59.3. Persistent.
// 5221-5230 - Coordinate System 1, G54 for X, Y, Z, A, B, C, U, V, W & R. R denotes the XY rotation angle around the Z axis. Persistent.
// 5241-5250 - Coordinate System 2, G55 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
// 5261-5270 - Coordinate System 3, G56 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
// 5281-5290 - Coordinate System 4, G57 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
// 5301-5310 - Coordinate System 5, G58 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
// 5321-5330 - Coordinate System 6, G59 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
// 5341-5350 - Coordinate System 7, G59.1 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
// 5361-5370 - Coordinate System 8, G59.2 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
// 5381-5390 - Coordinate System 9, G59.3 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
    ACTIVE_WORK_CSYS=5220,
    G54_X=5221,
    G54_Y,
    G54_Z,
    G54_A,
    G54_B,
    G54_C,
    G54_U,
    G54_V,
    G54_W,
    G54_R,
    G55_X=5241,
    G55_Y,
    G55_Z,
    G55_A,
    G55_B,
    G55_C,
    G55_U,
    G55_V,
    G55_W,
    G55_R,
    G56_X=5261,
    G56_Y,
    G56_Z,
    G56_A,
    G56_B,
    G56_C,
    G56_U,
    G56_V,
    G56_W,
    G56_R,
    G57_X=5281,
    G57_Y,
    G57_Z,
    G57_A,
    G57_B,
    G57_C,
    G57_U,
    G57_V,
    G57_W,
    G57_R,
    G58_X=5301,
    G58_Y,
    G58_Z,
    G58_A,
    G58_B,
    G58_C,
    G58_U,
    G58_V,
    G58_W,
    G58_R,
    G59_X=5321,
    G59_Y,
    G59_Z,
    G59_A,
    G59_B,
    G59_C,
    G59_U,
    G59_V,
    G59_W,
    G59_R,
    G59_1_X=5341,
    G59_1_Y,
    G59_1_Z,
    G59_1_A,
    G59_1_B,
    G59_1_C,
    G59_1_U,
    G59_1_V,
    G59_1_W,
    G59_1_R,
    G59_2_X=5361,
    G59_2_Y,
    G59_2_Z,
    G59_2_A,
    G59_2_B,
    G59_2_C,
    G59_2_U,
    G59_2_V,
    G59_2_W,
    G59_2_R,
    G59_3_X=5381,
    G59_3_Y,
    G59_3_Z,
    G59_3_A,
    G59_3_B,
    G59_3_C,
    G59_3_U,
    G59_3_V,
    G59_3_W,
    G59_3_R,
// 5399 - Result of M66 - Check or wait for input. Volatile.
    M66_RESULT=5399,
// 5400 - Tool Number. Volatile.
// 5401-5409 - Tool Offsets for X, Y, Z, A, B, C, U, V & W. Volatile.
    TOOL_NUMBER=5400,
    TOOL_OFFSET_X=5401,
    TOOL_OFFSET_Y,
    TOOL_OFFSET_Z,
    TOOL_OFFSET_A,
    TOOL_OFFSET_B,
    TOOL_OFFSET_C,
    TOOL_OFFSET_U,
    TOOL_OFFSET_V,
    TOOL_OFFSET_W,
// 5410 - Tool Diameter. Volatile.
    TOOL_DIAMETER=5410,
// 5411 - Tool Front Angle. Volatile.
    TOOL_FRONT_ANGLE=5411,
// 5412 - Tool Back Angle. Volatile.
    TOOL_BACK_ANGLE=5412,
// 5413 - Tool Orientation. Volatile.
    TOOL_ORIENTATION=5413,
// 5420-5428 - Current relative position in the active coordinate system including all offsets and in the current program units for X, Y, Z, A, B, C, U, V & W, volatile.
    RELATIVE_POSITION_X=5420,
    RELATIVE_POSITION_Y,
    RELATIVE_POSITION_Z,
    RELATIVE_POSITION_A,
    RELATIVE_POSITION_B,
    RELATIVE_POSITION_C,
    RELATIVE_POSITION_U,
    RELATIVE_POSITION_V,
    RELATIVE_POSITION_W,
// 5599 - Flag for controlling the output of (DEBUG,) statements. 1=output, 0=no output; default=1. Volatile.
    DEBUG_LEVEL_FLAG=5599,
// 5600 - Toolchanger fault indicator. Used with the iocontrol-v2 component. 1: toolchanger faulted, 0: normal. Volatile.
    TOOLCHANGER_FAULT=5600,
// 5601 - Toolchanger fault code. Used with the iocontrol-v2 component. Reflects the value of the toolchanger-reason HAL pin if a fault occurred. Volatile.
    TOOLCHANGER_FAULT_CODE=5601,
    RS274NGC_MAX_PARAMETERS=5602
};
}
#endif // INTERP_PARAMETER_DEF_H
