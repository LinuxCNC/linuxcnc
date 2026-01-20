/********************************************************************
* Description: emccfg.h
*   Compile-time defaults for EMC application. Defaults are used to
*   initialize globals in emcglb.c. Include emcglb.h to access these
*   globals.
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
#ifndef EMCCFG_H
#define EMCCFG_H

#ifdef __cplusplus
extern "C" {
#endif

/* default name of EMC INI file */
#define DEFAULT_EMC_INIFILE "emc.ini"

/* default name of EMC NML file */
extern const char * DEFAULT_EMC_NMLFILE;

/* cycle time for emctask, in seconds */
#define DEFAULT_EMC_TASK_CYCLE_TIME 0.100

/* default interp len */
#define DEFAULT_EMC_TASK_INTERP_MAX_LEN 1000

/* default feed rate, in user units per second */
#define DEFAULT_TRAJ_DEFAULT_VELOCITY 1.0

/* default traverse rate, in user units per second */
#define DEFAULT_TRAJ_MAX_VELOCITY 10.0

/* default trajectory jerk for feed moves, in user units per second^3
 * Set to 0.0 to disable S-curve acceleration (use trapezoidal profile)
 * When enabled (>0), produces smoother motion with reduced vibration
 */
#define DEFAULT_TRAJ_DEFAULT_JERK 0.0

/* default maximum trajectory jerk, in user units per second^3
 * Set to 0.0 to disable S-curve acceleration (use trapezoidal profile)
 */
#define DEFAULT_TRAJ_MAX_JERK 0.0

/* default trajectory planner type
 * 0 = Trapezoidal acceleration profile (traditional)
 * 1 = S-curve acceleration profile (smoother, requires MAX_JERK > 0)
 */
#define DEFAULT_TRAJ_PLANNER_TYPE 0

/* default joint velocity, in user units per second */
#define DEFAULT_JOINT_MAX_VELOCITY 1.0

/* default joint acceleration, in user units per second per second */
#define DEFAULT_JOINT_MAX_ACCELERATION 1.0

/* default joint jerk, in user units per second per second per second
 * Set to 0.0 to disable S-curve acceleration (use trapezoidal profile)
 * When enabled (>0), produces smoother motion with reduced vibration
 * Typical values: 100-10000 depending on machine rigidity and axis inertia
 */
#define DEFAULT_JOINT_MAX_JERK 0.0

/* default axis velocity, in user units per second */
#define DEFAULT_AXIS_MAX_VELOCITY 1.0

/* default axis acceleration, in user units per second per second */
#define DEFAULT_AXIS_MAX_ACCELERATION 1.0

/* default axis jerk, in user units per second per second per second
 * Set to 0.0 to disable S-curve acceleration (use trapezoidal profile)
 * When enabled (>0), produces smoother motion with reduced vibration
 * Typical values: 100-10000 depending on machine rigidity and axis inertia
 */
#define DEFAULT_AXIS_MAX_JERK 0.0

#ifdef __cplusplus
}				/* matches extern "C" at top */
#endif
#endif
