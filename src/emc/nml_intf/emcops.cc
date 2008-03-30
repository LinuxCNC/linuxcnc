/********************************************************************
* Description: emcops.cc
*   Initialization and other ad hoc functions for NML. This complements
*   the auto-generated emc.cc, which contains all the rote update
*   methods for the message classes.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

#include "emc.hh"
#include "emc_nml.hh"

EMC_JOINT_STAT::EMC_JOINT_STAT():
EMC_JOINT_STAT_MSG(EMC_JOINT_STAT_TYPE, sizeof(EMC_JOINT_STAT))
{
    jointType = EMC_JOINT_LINEAR;
    units = 1.0;
    backlash = 0.0;
    maxError = 0.0;
    minPositionLimit = -1.0;
    maxPositionLimit = 1.0;
    minFerror = 1.0;
    maxFerror = 1.0;
    ferrorCurrent = 0.0;
    ferrorHighMark = 0.0;
    output = 0.0;
    input = 0.0;
    inpos = 1;
    homing = 0;
    homed = 0;
    enabled = 0;
    fault = 0;
    minSoftLimit = 0;
    maxSoftLimit = 0;
    minHardLimit = 0;
    maxHardLimit = 0;
    overrideLimits = 0;
}

EMC_TRAJ_STAT::EMC_TRAJ_STAT():
EMC_TRAJ_STAT_MSG(EMC_TRAJ_STAT_TYPE, sizeof(EMC_TRAJ_STAT))
{
    linearUnits = 1.0;
    angularUnits = 1.0;
    cycleTime = 0.0;
    axes = 1;
    axis_mask = 1;
    mode = EMC_TRAJ_MODE_FREE;
    enabled = 0;
    inpos = 1;
    queue = 0;
    activeQueue = 0;
    queueFull = 0;
    id = 0;
    paused = 0;
    scale = 0.0;
    spindle_scale = 0.0;

    ZERO_EMC_POSE(position);
    ZERO_EMC_POSE(actualPosition);

    velocity = 1.0;
    acceleration = 1.0;
    maxVelocity = 1.0;
    maxAcceleration = 1.0;

    ZERO_EMC_POSE(probedPosition);
    probe_tripped = 0;
    probing = 0;
    probeval = 0;
    
    ZERO_EMC_POSE(dtg);
    distance_to_go = 0.0;
    kinematics_type = 0;
    motion_type = 0;
    current_vel = 0.0;
    feed_override_enabled = 0;
    spindle_override_enabled = 0;
    adaptive_feed_enabled = 0;
    feed_hold_enabled = 0;
    delayLeft = 0.0;
}

EMC_MOTION_STAT::EMC_MOTION_STAT():
EMC_MOTION_STAT_MSG(EMC_MOTION_STAT_TYPE, sizeof(EMC_MOTION_STAT))
{
    int i;

    for (i = 0; i < EMC_MAX_DIO; i++)
	synch_di[i] = 0;

    for (i = 0; i < EMC_MAX_AIO; i++)
	analog_input[i] = 0;
    
    debug = 0;
};

EMC_TASK_STAT::EMC_TASK_STAT():
EMC_TASK_STAT_MSG(EMC_TASK_STAT_TYPE, sizeof(EMC_TASK_STAT))
{
    int t;

    mode = EMC_TASK_MODE_MANUAL;
    state = EMC_TASK_STATE_ESTOP;
    execState = EMC_TASK_EXEC_DONE;
    interpState = EMC_TASK_INTERP_IDLE;
    motionLine = 0;
    currentLine = 0;
    readLine = 0;
    optional_stop_state = OFF;
    block_delete_state = OFF;
    input_timeout = OFF;
    file[0] = 0;
    command[0] = 0;

    ZERO_EMC_POSE(origin);
    ZERO_EMC_POSE(toolOffset);

    rotation_xy = 0.0;
    tloIsAlongW = OFF;

    for (t = 0; t < ACTIVE_G_CODES; t++)
	activeGCodes[t] = -1;
    for (t = 0; t < ACTIVE_M_CODES; t++)
	activeMCodes[t] = -1;
    for (t = 0; t < ACTIVE_SETTINGS; t++)
	activeSettings[t] = 0.0;

    programUnits = CANON_UNITS_MM;
    interpreter_errcode = 0;
    task_paused = 0;
}

EMC_TOOL_STAT::EMC_TOOL_STAT():
EMC_TOOL_STAT_MSG(EMC_TOOL_STAT_TYPE, sizeof(EMC_TOOL_STAT))
{
    int t;

    toolPrepped = 0;
    toolInSpindle = 0;

    for (t = 0; t <= CANON_TOOL_MAX; t++) {
	toolTable[t].id = 0;
	toolTable[t].xoffset = 0.0;
	toolTable[t].zoffset = 0.0;
	toolTable[t].diameter = 0.0;
	toolTable[t].orientation = 0;
	toolTable[t].frontangle = 0.0;
	toolTable[t].backangle = 0.0;
    }
}

EMC_AUX_STAT::EMC_AUX_STAT():
EMC_AUX_STAT_MSG(EMC_AUX_STAT_TYPE, sizeof(EMC_AUX_STAT))
{
    int t;

    estop = 1;

    for (t = 0; t < EMC_AUX_MAX_DOUT; t++) {
	dout[t] = 0;
    }

    for (t = 0; t < EMC_AUX_MAX_DIN; t++) {
	din[t] = 0;
    }

    for (t = 0; t < EMC_AUX_MAX_AOUT; t++) {
	aout[t] = 0;
    }

    for (t = 0; t < EMC_AUX_MAX_AIN; t++) {
	ain[t] = 0;
    }
}

EMC_SPINDLE_STAT::EMC_SPINDLE_STAT():
EMC_SPINDLE_STAT_MSG(EMC_SPINDLE_STAT_TYPE, sizeof(EMC_SPINDLE_STAT))
{
    speed = 0.0;
    direction = 0;
    brake = 1;
    increasing = 0;
    enabled = 0;
}

EMC_COOLANT_STAT::EMC_COOLANT_STAT():EMC_COOLANT_STAT_MSG(EMC_COOLANT_STAT_TYPE,
		     sizeof
		     (EMC_COOLANT_STAT))
{
    mist = 0;
    flood = 0;
}

EMC_LUBE_STAT::EMC_LUBE_STAT():
EMC_LUBE_STAT_MSG(EMC_LUBE_STAT_TYPE, sizeof(EMC_LUBE_STAT))
{
    on = 0;
    level = 1;
}

// overload = , since class has array elements
EMC_TOOL_STAT EMC_TOOL_STAT::operator =(EMC_TOOL_STAT s)
{
    int t;

    toolPrepped = s.toolPrepped;
    toolInSpindle = s.toolInSpindle;

    for (t = 0; t <= CANON_TOOL_MAX; t++) {
	toolTable[t].id = s.toolTable[t].id;
	toolTable[t].xoffset = s.toolTable[t].xoffset;
	toolTable[t].zoffset = s.toolTable[t].zoffset;
	toolTable[t].diameter = s.toolTable[t].diameter;
	toolTable[t].frontangle = s.toolTable[t].frontangle;
	toolTable[t].backangle = s.toolTable[t].backangle;
	toolTable[t].orientation = s.toolTable[t].orientation;
    }

    return s;
}

// overload = , since class has array elements
EMC_AUX_STAT EMC_AUX_STAT::operator =(EMC_AUX_STAT s)
{
    int t;

    estop = s.estop;

    for (t = 0; t < EMC_AUX_MAX_DOUT; t++) {
	dout[t] = s.dout[t];
    }

    for (t = 0; t < EMC_AUX_MAX_DIN; t++) {
	din[t] = s.din[t];
    }

    for (t = 0; t < EMC_AUX_MAX_AOUT; t++) {
	aout[t] = s.aout[t];
    }

    for (t = 0; t < EMC_AUX_MAX_AIN; t++) {
	ain[t] = s.ain[t];
    }

    return s;
}

EMC_STAT::EMC_STAT():EMC_STAT_MSG(EMC_STAT_TYPE, sizeof(EMC_STAT))
{
}
