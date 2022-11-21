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
#include "tooldata.hh"

EMC_AXIS_STAT::EMC_AXIS_STAT():
EMC_AXIS_STAT_MSG(EMC_AXIS_STAT_TYPE, sizeof(EMC_AXIS_STAT))
{
}

EMC_JOINT_STAT::EMC_JOINT_STAT():
EMC_JOINT_STAT_MSG(EMC_JOINT_STAT_TYPE, sizeof(EMC_JOINT_STAT))
{
    jointType = EMC_LINEAR;
    units = 1.0;
    backlash = 0.0;
    minPositionLimit = -1.0;
    maxPositionLimit = 1.0;
    minFerror = 1.0;
    maxFerror = 1.0;
    ferrorCurrent = 0.0;
    ferrorHighMark = 0.0;
    output = 0.0;
    input = 0.0;
    velocity = 0.0;
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
EMC_TRAJ_STAT_MSG(EMC_TRAJ_STAT_TYPE, sizeof(EMC_TRAJ_STAT)),
    tag()
{
    linearUnits = 1.0;
    angularUnits = 1.0;
    cycleTime = 0.0;
    joints = 1;
    axis_mask = 1;
    mode = EMC_TRAJ_MODE_FREE;
    enabled = OFF;
    inpos = ON;
    queue = 0;
    activeQueue = 0;
    queueFull = OFF;
    id = 0;
    paused = OFF;
    scale = 0.0;
    rapid_scale = 0.0;

    ZERO_EMC_POSE(position);
    ZERO_EMC_POSE(actualPosition);

    velocity = 1.0;
    acceleration = 1.0;
    maxVelocity = 1.0;
    maxAcceleration = 1.0;

    ZERO_EMC_POSE(probedPosition);
    probe_tripped = OFF;
    probing = OFF;
    probeval = 0;
    
    ZERO_EMC_POSE(dtg);
    distance_to_go = 0.0;
    kinematics_type = 0;
    motion_type = 0;
    current_vel = 0.0;
    feed_override_enabled = OFF;
    adaptive_feed_enabled = OFF;
    feed_hold_enabled = OFF;
}

EMC_MOTION_STAT::EMC_MOTION_STAT():
EMC_MOTION_STAT_MSG(EMC_MOTION_STAT_TYPE, sizeof(EMC_MOTION_STAT))
{
    int i;

    for (i = 0; i < EMCMOT_MAX_DIO; i++) {
	synch_di[i] = 0;
	synch_do[i] = 0;
    }

    for (i = 0; i < EMCMOT_MAX_AIO; i++) {
	analog_input[i] = 0.0;
	analog_output[i] = 0.0;
    }

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
    callLevel = 0;
    motionLine = 0;
    currentLine = 0;
    readLine = 0;
    optional_stop_state = OFF;
    block_delete_state = OFF;
    input_timeout = OFF;
    file[0] = 0;
    command[0] = 0;

    ZERO_EMC_POSE(g5x_offset);
    g5x_index = 0;
    ZERO_EMC_POSE(g92_offset);
    ZERO_EMC_POSE(toolOffset);

    rotation_xy = 0.0;

    for (t = 0; t < ACTIVE_G_CODES; t++)
	activeGCodes[t] = -1;
    for (t = 0; t < ACTIVE_M_CODES; t++)
	activeMCodes[t] = -1;
    for (t = 0; t < ACTIVE_SETTINGS; t++)
	activeSettings[t] = 0.0;

    programUnits = CANON_UNITS_MM;
    interpreter_errcode = 0;
    task_paused = 0;
    delayLeft = 0.0;
    queuedMDIcommands = 0;
}

EMC_TOOL_STAT::EMC_TOOL_STAT():
EMC_TOOL_STAT_MSG(EMC_TOOL_STAT_TYPE, sizeof(EMC_TOOL_STAT))
{
    pocketPrepped = 0; // idx
    toolInSpindle = 0; // toolno
    toolFromPocket = 0; // tool_from_pocket
#ifdef TOOL_NML //{
    int idx;
    for (idx = 0; idx < CANON_POCKETS_MAX; idx++) {
        toolTable[idx] = tooldata_entry_init();
    }
#else //}{
    toolTableCurrent = tooldata_entry_init();
#endif //}
}

EMC_AUX_STAT::EMC_AUX_STAT():
EMC_AUX_STAT_MSG(EMC_AUX_STAT_TYPE, sizeof(EMC_AUX_STAT))
{
    estop = 1;
}

EMC_SPINDLE_STAT::EMC_SPINDLE_STAT():
EMC_SPINDLE_STAT_MSG(EMC_SPINDLE_STAT_TYPE, sizeof(EMC_SPINDLE_STAT))
{
    speed = 0.0;
    direction = 0;
    brake = 1;
    increasing = 0;
    enabled = 0;
    spindle_scale = 1.0;
    spindle_override_enabled = 0;

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
    pocketPrepped = s.pocketPrepped; // idx
    toolInSpindle = s.toolInSpindle; // toolno
    toolFromPocket = s.toolFromPocket; // tool_from_pocket

#ifdef TOOL_NML //{
    int idx;
    for (idx = 0; idx < CANON_POCKETS_MAX; idx++) {
        toolTable[idx].toolno = s.toolTable[idx].toolno;
        toolTable[idx].pocketno = s.toolTable[idx].pocketno;
        toolTable[idx].offset = s.toolTable[idx].offset;
        toolTable[idx].diameter = s.toolTable[idx].diameter;
        toolTable[idx].frontangle = s.toolTable[idx].frontangle;
        toolTable[idx].backangle = s.toolTable[idx].backangle;
        toolTable[idx].orientation = s.toolTable[idx].orientation;
    }
#else //}{
    struct CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata,0) != IDX_OK) {
        fprintf(stderr,"UNEXPECTED idx %s %d\n",__FILE__,__LINE__);
    }
    toolTableCurrent = tdata;
#endif //}

    return s;
}

EMC_STAT::EMC_STAT():EMC_STAT_MSG(EMC_STAT_TYPE, sizeof(EMC_STAT))
{
}
