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
* $Revision$
* $Author$
* $Date$
********************************************************************/

#include "emc.hh"

EMC_AXIS_STAT::EMC_AXIS_STAT():
EMC_AXIS_STAT_MSG(EMC_AXIS_STAT_TYPE, sizeof(EMC_AXIS_STAT))
{
    axisType = EMC_AXIS_LINEAR;
    units = 1.0;
    p = 0.0;
    i = 0.0;
    d = 0.0;
    ff0 = 0.0;
    ff1 = 0.0;
    ff2 = 0.0;
    backlash = 0.0;
    bias = 0.0;
    maxError = 0.0;
    cycleTime = 1.0;
    inputScale = 1.0;
    inputOffset = 0.0;
    outputScale = 1.0;
    outputOffset = 0.0;
    minPositionLimit = -1.0;
    maxPositionLimit = 1.0;
    minOutputLimit = -1.0;
    maxOutputLimit = 1.0;
    maxFerror = 1.0;
    homingVel = 1.0;
    setup_time = 1;
    hold_time = 2;
    enablePolarity = 1;
    minLimitSwitchPolarity = 1;
    maxLimitSwitchPolarity = 1;
    homeSwitchPolarity = 1;
    homingPolarity = 1;
    faultPolarity = 1;
    ferrorCurrent = 0.0;
    ferrorHighMark = 0.0;
    output = 0.0;
    input = 0.0;
    inpos = 1;
    homing = 0;
    homed = 0;
    enabled = 0;
    minSoftLimit = 0;
    maxSoftLimit = 0;
    minHardLimit = 0;
    maxHardLimit = 0;
    scale = 0.0;
}

EMC_TRAJ_STAT::EMC_TRAJ_STAT():
EMC_TRAJ_STAT_MSG(EMC_TRAJ_STAT_TYPE, sizeof(EMC_TRAJ_STAT))
{
    linearUnits = 1.0;
    angularUnits = 1.0;
    axes = 1;
    mode = EMC_TRAJ_MODE_FREE;
    enabled = 0;
    inpos = 1;
    queue = 0;
    activeQueue = 0;
    queueFull = 0;
    id = 0;
    paused = 0;

    position.tran.x = 0.0;
    position.tran.y = 0.0;
    position.tran.z = 0.0;
    position.a = 0.0;
    position.b = 0.0;
    position.c = 0.0;

    actualPosition.tran.x = 0.0;
    actualPosition.tran.y = 0.0;
    actualPosition.tran.z = 0.0;
    actualPosition.a = 0.0;
    actualPosition.b = 0.0;
    actualPosition.c = 0.0;

    velocity = 1.0;
    acceleration = 1.0;
    maxVelocity = 1.0;
    maxAcceleration = 1.0;
}

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
    file[0] = 0;
    command[0] = 0;

    origin.tran.x = 0.0;
    origin.tran.y = 0.0;
    origin.tran.z = 0.0;
    origin.a = 0.0;
    origin.b = 0.0;
    origin.c = 0.0;

    toolOffset.tran.x = 0.0;
    toolOffset.tran.y = 0.0;
    toolOffset.tran.z = 0.0;
    toolOffset.a = 0.0;
    toolOffset.b = 0.0;
    toolOffset.c = 0.0;

    for (t = 0; t < EMC_TASK_ACTIVE_G_CODES; t++)
	activeGCodes[t] = -1;
    for (t = 0; t < EMC_TASK_ACTIVE_M_CODES; t++)
	activeMCodes[t] = -1;
    for (t = 0; t < EMC_TASK_ACTIVE_SETTINGS; t++)
	activeSettings[t] = 0.0;
}

EMC_TOOL_STAT::EMC_TOOL_STAT():
EMC_TOOL_STAT_MSG(EMC_TOOL_STAT_TYPE, sizeof(EMC_TOOL_STAT))
{
    int t;

    toolPrepped = 0;
    toolInSpindle = 0;

    for (t = 0; t <= CANON_TOOL_MAX; t++) {
	toolTable[t].id = 0;
	toolTable[t].length = 0.0;
	toolTable[t].diameter = 0.0;
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
    sizeof(EMC_COOLANT_STAT))
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
	toolTable[t].length = s.toolTable[t].length;
	toolTable[t].diameter = s.toolTable[t].diameter;
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
    logFile[0] = 0;
    logType = 0;
    logSize = 0;
    logSkip = 0;
    logOpen = 0;
    logStarted = 0;
    logPoints = 0;
}
