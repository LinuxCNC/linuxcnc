/********************************************************************
* Description: emccanon.cc
*   Canonical definitions for 3-axis NC application
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
/*

  Notes:

  Units
  -----
  Values are stored internally as mm and degree units, e.g, program
  offsets, end point, tool length offset.  These are "internal
  units". "External units" are the units used by the EMC motion planner.
  All lengths and units output by the interpreter are converted to
  internal units here, using FROM_PROG_LEN,ANG, and then
  TO_EXT_LEN(),ANG are called to convert these to external units.

  Tool Length Offsets
  -------------------
  The interpreter does not subtract off tool length offsets. It calls
  USE_TOOL_LENGTH_OFFSETS(length), which we record here and apply to
  all subsequent Z values.
  */

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>		// strncpy()
#include <ctype.h>		// isspace()
#include "emc.hh"		// EMC NML
#include "canon.hh"		// these decls
#include "interpl.hh"		// interp_list
#include "emcglb.h"		// TRAJ_MAX_VELOCITY
#include "emcpos.h"

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MIN3
#define MIN3(a,b,c) (MIN(MIN((a),(b)),(c)))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MAX4
#define MAX4(a,b,c,d) (MAX(MAX((a),(b)),MAX((c),(d))))
#endif

static PM_QUATERNION quat(1, 0, 0, 0);

/*
  These decls were from the old 3-axis canon.hh, and refer functions
  defined here that are used for convenience but no longer have decls
  in the 6-axis canon.hh. So, we declare them here now.
*/
extern void CANON_ERROR(const char *fmt, ...);

/*
  Origin offsets, length units, and active plane are all maintained
  here in this file. Controller runs in absolute mode, and does not
  have plane select concept.

  programOrigin is stored in mm always, and converted when set or read.
  When it's applied to positions, convert positions to mm units first
  and then add programOrigin.

  Units are then converted from mm to external units, as reported by
  the GET_EXTERNAL_LENGTH_UNITS() function.
  */
static CANON_POSITION programOrigin(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
static CANON_UNITS lengthUnits = CANON_UNITS_MM;
static CANON_PLANE activePlane = CANON_PLANE_XY;

/*
  canonEndPoint is the last programmed end point, stored in case it's
  needed for subsequent calculations. It's in absolute frame, mm units.
  */
static CANON_POSITION canonEndPoint;
static void canonUpdateEndPoint(double x, double y, double z, double a,
				double b, double c)
{
    canonEndPoint.x = x;
    canonEndPoint.y = y;
    canonEndPoint.z = z;
    canonEndPoint.a = a;
    canonEndPoint.b = b;
    canonEndPoint.c = c;
}

/* motion control mode is used to signify blended v. stop-at-end moves.
   Set to 0 (invalid) at start, so first call will send command out */
static CANON_MOTION_MODE canonMotionMode = 0;

/* motion path-following tolerance is used to set the max path-following
   deviation during CANON_CONTINUOUS.
   If this param is 0, then it will behave as emc always did, allowing
   almost any deviation trying to keep speed up. */
static double canonMotionTolerance = 0.0;

/* macros for converting internal (mm/deg) units to external units */
#define TO_EXT_LEN(mm) ((mm) * GET_EXTERNAL_LENGTH_UNITS())
#define TO_EXT_ANG(deg) ((deg) * GET_EXTERNAL_ANGLE_UNITS())

/* macros for converting external units to internal (mm/deg) units */
#define FROM_EXT_LEN(ext) ((ext) / GET_EXTERNAL_LENGTH_UNITS())
#define FROM_EXT_ANG(ext) ((ext) / GET_EXTERNAL_ANGLE_UNITS())

/* macros for converting internal (mm/deg) units to program units */
#define TO_PROG_LEN(mm) ((mm) / (lengthUnits == CANON_UNITS_INCHES ? 25.4 : lengthUnits == CANON_UNITS_CM ? 10.0 : 1.0))
#define TO_PROG_ANG(deg) (deg)

/* macros for converting program units to internal (mm/deg) units */
#define FROM_PROG_LEN(prog) ((prog) * (lengthUnits == CANON_UNITS_INCHES ? 25.4 : lengthUnits == CANON_UNITS_CM ? 10.0 : 1.0))
#define FROM_PROG_ANG(prog) (prog)

/* Spindle speed is saved here */
static double spindleSpeed = 0.0;

/* Prepped tool is saved here */
static int preppedTool = 0;

/* Tool length offset is saved here */
static double currentToolLengthOffset = 0.0;

/*
  EMC traj interface uses one speed. For storing and applying separate
  feed and traverse rates, a flag for which was last applied is used.
  Requests for motions check this flag and if the improper speed was
  last set the proper one is set before the command goes out.
  */

static double lastVelSet = -1;
static double last_ini_maxvel = -1;
static double lastAccSet = -1;

/*
  Feed rate is saved here; values are in mm/sec or deg/sec.
  It will be initially set in INIT_CANON() below.
*/
static double currentLinearFeedRate = 0.0;
static double currentAngularFeedRate = 0.0;

/* Used to indicate whether the current move is linear, angular, or 
   a combination of both. */
   //AJ says: linear means axes XYZ move (lines or even circles)
   //         angular means axes ABC move
static int linear_move = 0;
static int angular_move = 0;

/* sends a request to set the vel, which is in internal units/sec */
static int sendVelMsg(double vel, double ini_maxvel)
{
    EMC_TRAJ_SET_VELOCITY velMsg;

    if (linear_move && !angular_move) {
	velMsg.velocity = TO_EXT_LEN(vel);
	velMsg.ini_maxvel = TO_EXT_LEN(ini_maxvel);
    } else if (!linear_move && angular_move) {
	velMsg.velocity = TO_EXT_ANG(vel);
	velMsg.ini_maxvel = TO_EXT_ANG(ini_maxvel);
    } else if (linear_move && angular_move) {
	velMsg.velocity = TO_EXT_LEN(vel);
	velMsg.ini_maxvel = TO_EXT_LEN(ini_maxvel);
    } else { //seems this case was forgotten, neither linear, neither angular move (we are only sending vel)
	velMsg.velocity = TO_EXT_LEN(vel);
	velMsg.ini_maxvel = TO_EXT_LEN(ini_maxvel);
    }	

    if (velMsg.velocity != lastVelSet ||
            velMsg.ini_maxvel != last_ini_maxvel) {
	lastVelSet = velMsg.velocity;
        last_ini_maxvel = velMsg.ini_maxvel;
	interp_list.append(velMsg);
    }
    return 0;
}

static int sendAccMsg(double acc)
{
    EMC_TRAJ_SET_ACCELERATION accMsg;

    if (linear_move && !angular_move) {
	accMsg.acceleration = TO_EXT_LEN(acc);
    } else if (!linear_move && angular_move) {
	accMsg.acceleration = TO_EXT_ANG(acc);
    } else if (linear_move && angular_move) {
	accMsg.acceleration = TO_EXT_LEN(acc);
    }

    if (accMsg.acceleration != lastAccSet) {
	lastAccSet = accMsg.acceleration;
	interp_list.append(accMsg);
    }
    return 0;
}


/* Representation */
void SET_ORIGIN_OFFSETS(double x, double y, double z,
			double a, double b, double c)
{
    EMC_TRAJ_SET_ORIGIN set_origin_msg;

    /* convert to mm units */
    x = FROM_PROG_LEN(x);
    y = FROM_PROG_LEN(y);
    z = FROM_PROG_LEN(z);
    a = FROM_PROG_ANG(a);
    b = FROM_PROG_ANG(b);
    c = FROM_PROG_ANG(c);

    programOrigin.x = x;
    programOrigin.y = y;
    programOrigin.z = z;
    programOrigin.a = a;
    programOrigin.b = b;
    programOrigin.c = c;

    /* append it to interp list so it gets updated at the right time, not at
       read-ahead time */
    set_origin_msg.origin.tran.x = TO_EXT_LEN(programOrigin.x);
    set_origin_msg.origin.tran.y = TO_EXT_LEN(programOrigin.y);
    set_origin_msg.origin.tran.z = TO_EXT_LEN(programOrigin.z);
    set_origin_msg.origin.a = TO_EXT_ANG(programOrigin.a);
    set_origin_msg.origin.b = TO_EXT_ANG(programOrigin.b);
    set_origin_msg.origin.c = TO_EXT_ANG(programOrigin.c);

    interp_list.append(set_origin_msg);
}

void USE_LENGTH_UNITS(CANON_UNITS in_unit)
{
    lengthUnits = in_unit;

    emcStatus->task.programUnits = in_unit;
}

/* Free Space Motion */
void SET_TRAVERSE_RATE(double rate)
{
    // nothing need be done here
}

void SET_FEED_RATE(double rate)
{

    /* convert from /min to /sec */
    rate /= 60.0;

    /* convert to traj units (mm & deg) if needed */
    currentLinearFeedRate = FROM_PROG_LEN(rate);
    currentAngularFeedRate = FROM_PROG_ANG(rate);
}

void SET_FEED_REFERENCE(CANON_FEED_REFERENCE reference)
{
    // nothing need be done here
}

double getStraightAcceleration(double x, double y, double z,
			   double a, double b, double c)
{
    double dx, dy, dz, da, db, dc;
    double tx, ty, tz, ta, tb, tc, tmax;
    double acc, dtot;

    acc = 0.0; // if a move to nowhere

    // Compute absolute travel distance for each axis:
    dx = fabs(x - canonEndPoint.x);
    dy = fabs(y - canonEndPoint.y);
    dz = fabs(z - canonEndPoint.z);
    da = fabs(a - canonEndPoint.a);
    db = fabs(b - canonEndPoint.b);
    dc = fabs(c - canonEndPoint.c);

    // Figure out what kind of move we're making:
    if (dx <= 0.0 && dy <= 0.0 && dz <= 0.0) {
	linear_move = 0;
    } else {
	linear_move = 1;
    }
    if (da <= 0.0 && db <= 0.0 && dc <= 0.0) {
	angular_move = 0;
    } else {
	angular_move = 1;
    }

    // Pure linear move:
    if (linear_move && !angular_move) {
	tx = (dx / FROM_EXT_LEN(AXIS_MAX_ACCELERATION[0]));
	ty = (dy / FROM_EXT_LEN(AXIS_MAX_ACCELERATION[1]));
	tz = (dz / FROM_EXT_LEN(AXIS_MAX_ACCELERATION[2]));
	tmax = tx > ty ? tx : ty;
	tmax = tz > tmax ? tz : tmax;

	dtot = sqrt(dx * dx + dy * dy + dz * dz);
	if (tmax > 0.0) {
	    acc = dtot / tmax;
	}
    }
    // Pure angular move:
    else if (!linear_move && angular_move) {
	ta = da? (da / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[3])): 0.0;
	tb = db? (db / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[4])): 0.0;
	tc = dc? (dc / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[5])): 0.0;
	tmax = ta > tb ? ta : tb;
	tmax = tc > tmax ? tc : tmax;

	dtot = sqrt(da * da + db * db + dc * dc);
	if (tmax > 0.0) {
	    acc = dtot / tmax;
	}
    }
    // Combination angular and linear move:
    else if (linear_move && angular_move) {
	tx = (dx / FROM_EXT_LEN(AXIS_MAX_ACCELERATION[0]));
	ty = (dy / FROM_EXT_LEN(AXIS_MAX_ACCELERATION[1]));
	tz = (dz / FROM_EXT_LEN(AXIS_MAX_ACCELERATION[2]));
	ta = da? (da / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[3])): 0.0;
	tb = db? (db / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[4])): 0.0;
	tc = dc? (dc / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[5])): 0.0;
	tmax = tx > ty ? tx : ty;
	tmax = tz > tmax ? tz : tmax;
	tmax = ta > tmax ? ta : tmax;
	tmax = tb > tmax ? tb : tmax;
	tmax = tc > tmax ? tc : tmax;

/*  According to NIST IR6556 Section 2.1.2.5 Paragraph A
    a combnation move is handled like a linear move, except
    that the angular axes are allowed sufficient time to
    complete their motion coordinated with the motion of
    the linear axes.
*/
	dtot = sqrt(dx * dx + dy * dy + dz * dz);
	if (tmax > 0.0) {
	    acc = dtot / tmax;
	}
    }
    return acc;
}

double getStraightVelocity(double x, double y, double z,
			   double a, double b, double c)
{
    double dx, dy, dz, da, db, dc;
    double tx, ty, tz, ta, tb, tc, tmax;
    double vel, dtot;

/* If we get a move to nowhere (!linear_move && !angular_move)
   we might as well go there at the currentLinearFeedRate...
*/
    vel = currentLinearFeedRate;

    // Compute absolute travel distance for each axis:
    dx = fabs(x - canonEndPoint.x);
    dy = fabs(y - canonEndPoint.y);
    dz = fabs(z - canonEndPoint.z);
    da = fabs(a - canonEndPoint.a);
    db = fabs(b - canonEndPoint.b);
    dc = fabs(c - canonEndPoint.c);

    // Figure out what kind of move we're making:
    if (dx <= 0.0 && dy <= 0.0 && dz <= 0.0) {
	linear_move = 0;
    } else {
	linear_move = 1;
    }
    if (da <= 0.0 && db <= 0.0 && dc <= 0.0) {
	angular_move = 0;
    } else {
	angular_move = 1;
    }

    // Pure linear move:
    if (linear_move && !angular_move) {
	tx = fabs(dx / FROM_EXT_LEN(AXIS_MAX_VELOCITY[0]));
	ty = fabs(dy / FROM_EXT_LEN(AXIS_MAX_VELOCITY[1]));
	tz = fabs(dz / FROM_EXT_LEN(AXIS_MAX_VELOCITY[2]));
	tmax = tx > ty ? tx : ty;
	tmax = tz > tmax ? tz : tmax;

	dtot = sqrt(dx * dx + dy * dy + dz * dz);
	if (tmax <= 0.0) {
	    vel = currentLinearFeedRate;
	} else {
	    vel = dtot / tmax;
	}
    }
    // Pure angular move:
    else if (!linear_move && angular_move) {
	ta = da? fabs(da / FROM_EXT_ANG(AXIS_MAX_VELOCITY[3])):0.0;
	tb = db? fabs(db / FROM_EXT_ANG(AXIS_MAX_VELOCITY[4])):0.0;
	tc = dc? fabs(dc / FROM_EXT_ANG(AXIS_MAX_VELOCITY[5])):0.0;
	tmax = ta > tb ? ta : tb;
	tmax = tc > tmax ? tc : tmax;

	dtot = sqrt(da * da + db * db + dc * dc);
	if (tmax <= 0.0) {
	    vel = currentAngularFeedRate;
	} else {
	    vel = dtot / tmax;
	}
    }
    // Combination angular and linear move:
    else if (linear_move && angular_move) {
	tx = fabs(dx / FROM_EXT_LEN(AXIS_MAX_VELOCITY[0]));
	ty = fabs(dy / FROM_EXT_LEN(AXIS_MAX_VELOCITY[1]));
	tz = fabs(dz / FROM_EXT_LEN(AXIS_MAX_VELOCITY[2]));
	ta = da? fabs(da / FROM_EXT_ANG(AXIS_MAX_VELOCITY[3])):0.0;
	tb = db? fabs(db / FROM_EXT_ANG(AXIS_MAX_VELOCITY[4])):0.0;
	tc = dc? fabs(dc / FROM_EXT_ANG(AXIS_MAX_VELOCITY[5])):0.0;
	tmax = tx > ty ? tx : ty;
	tmax = tz > tmax ? tz : tmax;
	tmax = ta > tmax ? ta : tmax;
	tmax = tb > tmax ? tb : tmax;
	tmax = tc > tmax ? tc : tmax;

/*  According to NIST IR6556 Section 2.1.2.5 Paragraph A
    a combnation move is handled like a linear move, except
    that the angular axes are allowed sufficient time to
    complete their motion coordinated with the motion of
    the linear axes.
*/
	dtot = sqrt(dx * dx + dy * dy + dz * dz);
	if (tmax <= 0.0) {
	    vel = currentLinearFeedRate;
	} else {
	    vel = dtot / tmax;
	}
    }
    return vel;
}

void STRAIGHT_TRAVERSE(double x, double y, double z,
		       double a, double b, double c)
{
    double vel, acc;
    EMC_TRAJ_LINEAR_MOVE linearMoveMsg;

    // convert to mm units
    x = FROM_PROG_LEN(x);
    y = FROM_PROG_LEN(y);
    z = FROM_PROG_LEN(z);
    a = FROM_PROG_ANG(a);
    b = FROM_PROG_ANG(b);
    c = FROM_PROG_ANG(c);

    x += programOrigin.x;
    y += programOrigin.y;
    z += programOrigin.z;
    a += programOrigin.a;
    b += programOrigin.b;
    c += programOrigin.c;

    z += currentToolLengthOffset;

    // now x, y, z, and b are in absolute mm or degree units
    linearMoveMsg.end.tran.x = TO_EXT_LEN(x);
    linearMoveMsg.end.tran.y = TO_EXT_LEN(y);
    linearMoveMsg.end.tran.z = TO_EXT_LEN(z);

    // fill in the orientation
    linearMoveMsg.end.a = TO_EXT_ANG(a);
    linearMoveMsg.end.b = TO_EXT_ANG(b);
    linearMoveMsg.end.c = TO_EXT_ANG(c);

    vel = getStraightVelocity(x, y, z, a, b, c);
    sendVelMsg(vel, vel);

    if((acc = getStraightAcceleration(x, y, z, a, b, c)))
        sendAccMsg(acc);

    linearMoveMsg.type = EMC_MOTION_TYPE_TRAVERSE;
    interp_list.append(linearMoveMsg);
    canonUpdateEndPoint(x, y, z, a, b, c);
}

void STRAIGHT_FEED(double x, double y, double z, double a, double b,
		   double c)
{
    double ini_maxvel, vel, acc;
    EMC_TRAJ_LINEAR_MOVE linearMoveMsg;

    // convert to mm units
    x = FROM_PROG_LEN(x);
    y = FROM_PROG_LEN(y);
    z = FROM_PROG_LEN(z);
    a = FROM_PROG_ANG(a);
    b = FROM_PROG_ANG(b);
    c = FROM_PROG_ANG(c);

    x += programOrigin.x;
    y += programOrigin.y;
    z += programOrigin.z;
    a += programOrigin.a;
    b += programOrigin.b;
    c += programOrigin.c;

    z += currentToolLengthOffset;

    // now x, y, z, and b are in absolute mm or degree units
    linearMoveMsg.end.tran.x = TO_EXT_LEN(x);
    linearMoveMsg.end.tran.y = TO_EXT_LEN(y);
    linearMoveMsg.end.tran.z = TO_EXT_LEN(z);

    // fill in the orientation
    linearMoveMsg.end.a = TO_EXT_ANG(a);
    linearMoveMsg.end.b = TO_EXT_ANG(b);
    linearMoveMsg.end.c = TO_EXT_ANG(c);

    ini_maxvel = vel = getStraightVelocity(x, y, z, a, b, c);

    if (linear_move && !angular_move) {
	if (vel > currentLinearFeedRate) {
	    vel = currentLinearFeedRate;
	}
    } else if (!linear_move && angular_move) {
	if (vel > currentAngularFeedRate) {
	    vel = currentAngularFeedRate;
	}
    } else if (linear_move && angular_move) {
	if (vel > currentLinearFeedRate) {
	    vel = currentLinearFeedRate;
	}
    }

    sendVelMsg(vel, ini_maxvel);

    if((acc = getStraightAcceleration(x, y, z, a, b, c)))
        sendAccMsg(acc);

    linearMoveMsg.type = EMC_MOTION_TYPE_FEED;
    interp_list.append(linearMoveMsg);
    canonUpdateEndPoint(x, y, z, a, b, c);
}

void STRAIGHT_PROBE(double x, double y, double z, double a, double b,
		    double c)
{
    double ini_maxvel, vel, acc;
    EMC_TRAJ_PROBE probeMsg;

    // convert to mm units
    x = FROM_PROG_LEN(x);
    y = FROM_PROG_LEN(y);
    z = FROM_PROG_LEN(z);
    a = FROM_PROG_ANG(a);
    b = FROM_PROG_ANG(b);
    c = FROM_PROG_ANG(c);

    x += programOrigin.x;
    y += programOrigin.y;
    z += programOrigin.z;
    a += programOrigin.a;
    b += programOrigin.b;
    c += programOrigin.c;
    z += currentToolLengthOffset;

    // now x, y, z, and b are in absolute mm or degree units
    probeMsg.pos.tran.x = TO_EXT_LEN(x);
    probeMsg.pos.tran.y = TO_EXT_LEN(y);
    probeMsg.pos.tran.z = TO_EXT_LEN(z);

    // fill in the orientation
    probeMsg.pos.a = TO_EXT_ANG(a);
    probeMsg.pos.b = TO_EXT_ANG(b);
    probeMsg.pos.c = TO_EXT_ANG(c);

    ini_maxvel = vel = getStraightVelocity(x, y, z, a, b, c);

    if (linear_move && !angular_move) {
	if (vel > currentLinearFeedRate) {
	    vel = currentLinearFeedRate;
	}
    } else if (!linear_move && angular_move) {
	if (vel > currentAngularFeedRate) {
	    vel = currentAngularFeedRate;
	}
    } else if (linear_move && angular_move) {
	if (vel > currentLinearFeedRate) {
	    vel = currentLinearFeedRate;
	}
    }

    sendVelMsg(vel, ini_maxvel);

    if((acc = getStraightAcceleration(x, y, z, a, b, c)))
        sendAccMsg(acc);

    interp_list.append(probeMsg);
    canonUpdateEndPoint(x, y, z, a, b, c);
}

/* Machining Attributes */

void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode, double tolerance)
{
    EMC_TRAJ_SET_TERM_COND setTermCondMsg;

    if ((mode != canonMotionMode) || (FROM_PROG_LEN(tolerance) != canonMotionTolerance)) {
	canonMotionMode = mode;
	canonMotionTolerance =  FROM_PROG_LEN(tolerance);

	switch (mode) {
	case CANON_CONTINUOUS:
	    setTermCondMsg.cond = EMC_TRAJ_TERM_COND_BLEND;
	    setTermCondMsg.tolerance = canonMotionTolerance;
	    break;

	default:
	    setTermCondMsg.cond = EMC_TRAJ_TERM_COND_STOP;
	    break;
	}

	interp_list.append(setTermCondMsg);
    }
}

CANON_MOTION_MODE GET_MOTION_CONTROL_MODE()
{
    return canonMotionMode;
}

double GET_MOTION_CONTROL_TOLERANCE()
{
    return canonMotionTolerance;
}

void SELECT_PLANE(CANON_PLANE in_plane)
{
    activePlane = in_plane;
}

void SET_CUTTER_RADIUS_COMPENSATION(double radius)
{
    // nothing need be done here
}

void START_CUTTER_RADIUS_COMPENSATION(int side)
{
    // nothing need be done here
}

void STOP_CUTTER_RADIUS_COMPENSATION()
{
    // nothing need be done here
}

void START_SPEED_FEED_SYNCH()
{
    /*! \todo FIXME-- unimplemented */
}

void STOP_SPEED_FEED_SYNCH()
{
    /*! \todo FIXME-- unimplemented */
}

void SELECT_MOTION_MODE(CANON_MOTION_MODE mode)
{
    // nothing need be done here
}

/* Machining Functions */

void ARC_FEED(double first_end, double second_end,
	      double first_axis, double second_axis, int rotation,
	      double axis_end_point, double a, double b, double c)
{
    EmcPose end;
    PM_CARTESIAN center, normal;
    EMC_TRAJ_CIRCULAR_MOVE circularMoveMsg;
    EMC_TRAJ_LINEAR_MOVE linearMoveMsg;
    double v1, v2, a1, a2, vel, ini_maxvel, acc=0.0;
    double radius, angle, theta1, theta2, helical_length, axis_len;
    double tmax, thelix, ta, tb, tc, da, db, dc;

    /* In response to  Bugs item #1274108 - rotary axis moves when coordinate
       offsets used with A. Original code failed to include programOrigin on
       rotary moves. */
    a = FROM_PROG_ANG(a);
    b = FROM_PROG_ANG(b);
    c = FROM_PROG_ANG(c);
    a += programOrigin.a;
    b += programOrigin.b;
    c += programOrigin.c;

    da = fabs(canonEndPoint.a - a);
    db = fabs(canonEndPoint.b - b);
    dc = fabs(canonEndPoint.c - c);

    /* Since there's no default case here,
       we need to initialise vel to something safe! */
    vel = ini_maxvel = currentLinearFeedRate;

    // convert to absolute mm units
    first_axis = FROM_PROG_LEN(first_axis);
    second_axis = FROM_PROG_LEN(second_axis);
    first_end = FROM_PROG_LEN(first_end);
    second_end = FROM_PROG_LEN(second_end);
    axis_end_point = FROM_PROG_LEN(axis_end_point);

    /* associate x with x, etc., offset by program origin, and set normals */
    switch (activePlane) {
    default: // to eliminate "uninitalized" warnings
    case CANON_PLANE_XY:

	// offset and align args properly
	end.tran.x = first_end + programOrigin.x;
	end.tran.y = second_end + programOrigin.y;
	end.tran.z = axis_end_point + programOrigin.z;
	end.tran.z += currentToolLengthOffset;
	center.x = first_axis + programOrigin.x;
	center.y = second_axis + programOrigin.y;
	center.z = end.tran.z;
	normal.x = 0.0;
	normal.y = 0.0;
	normal.z = 1.0;

        theta1 = atan2(canonEndPoint.y - center.y, canonEndPoint.x - center.x);
        theta2 = atan2(end.tran.y - center.y, end.tran.x - center.x);
        radius = hypot(canonEndPoint.x - center.x, canonEndPoint.y - center.y);
        axis_len = fabs(end.tran.z - canonEndPoint.z);

	v1 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[0]);
	v2 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[1]);
	a1 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[0]);
	a2 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[1]);
        ini_maxvel = MIN(v1, v2);
        acc = MIN(a1, a2);
        if(axis_len > 0.001) {
            v1 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[2]);
            a1 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[2]);
            ini_maxvel = MIN(ini_maxvel, v1);
            acc = MIN(acc, a1);
        }
	break;

    case CANON_PLANE_YZ:

	// offset and align args properly
	end.tran.y = first_end + programOrigin.y;
	end.tran.z = second_end + programOrigin.z;
	end.tran.x = axis_end_point + programOrigin.x;
	end.tran.z += currentToolLengthOffset;

	center.y = first_axis + programOrigin.y;
	center.z = second_axis + programOrigin.z;
	center.x = end.tran.x;
	normal.y = 0.0;
	normal.z = 0.0;
	normal.x = 1.0;

        theta1 = atan2(canonEndPoint.z - center.z, canonEndPoint.y - center.y);
        theta2 = atan2(end.tran.z - center.z, end.tran.y - center.y);
        radius = hypot(canonEndPoint.y - center.y, canonEndPoint.z - center.z);
        axis_len = fabs(end.tran.x - canonEndPoint.x);

	v1 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[1]);
	v2 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[2]);
	a1 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[1]);
	a2 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[2]);
        ini_maxvel = MIN(v1, v2);
        acc = MIN(a1, a2);
        if(axis_len > 0.001) {
            v1 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[0]);
            a1 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[0]);
            ini_maxvel = MIN(ini_maxvel, v1);
            acc = MIN(acc, a1);
        }

	break;

    case CANON_PLANE_XZ:

	// offset and align args properly
	end.tran.z = first_end + programOrigin.z;
	end.tran.x = second_end + programOrigin.x;
	end.tran.y = axis_end_point + programOrigin.y;
	end.tran.z += currentToolLengthOffset;

	center.z = first_axis + programOrigin.z;
	center.x = second_axis + programOrigin.x;
	center.y = end.tran.y;
	normal.z = 0.0;
	normal.x = 0.0;
	normal.y = 1.0;

        theta1 = atan2(canonEndPoint.x - center.x, canonEndPoint.z - center.z);
        theta2 = atan2(end.tran.x - center.x, end.tran.z - center.z);
        radius = hypot(canonEndPoint.x - center.x, canonEndPoint.z - center.z);
        axis_len = fabs(end.tran.y - canonEndPoint.y);

	v1 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[0]);
	v2 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[2]);
	a1 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[0]);
	a2 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[2]);
	ini_maxvel = MIN(v1, v2);
        acc = MIN(a1, a2);
        if(axis_len > 0.001) {
            v1 = FROM_EXT_LEN(AXIS_MAX_VELOCITY[1]);
            a1 = FROM_EXT_LEN(AXIS_MAX_ACCELERATION[1]);
            ini_maxvel = MIN(ini_maxvel, v1);
            acc = MIN(acc, a1);
        }
	break;
    }

    if(rotation < 0) {
        if(theta2 >= theta1) theta2 -= M_PI * 2.0;
    } else {
        if(theta2 <= theta1) theta2 += M_PI * 2.0;
    }
    angle = theta2 - theta1;
    helical_length = hypot(angle * radius, axis_len);

    thelix = fabs(helical_length / ini_maxvel);
    ta = da? fabs(da / FROM_EXT_ANG(AXIS_MAX_VELOCITY[3])):0.0;
    tb = db? fabs(db / FROM_EXT_ANG(AXIS_MAX_VELOCITY[4])):0.0;
    tc = dc? fabs(dc / FROM_EXT_ANG(AXIS_MAX_VELOCITY[5])):0.0;
    tmax = MAX4(thelix, ta, tb, tc);

    if (tmax <= 0.0) {
        vel = currentLinearFeedRate;
    } else {
        ini_maxvel = helical_length / tmax;
        vel = MIN(vel, ini_maxvel);
    }

    // for arcs we always user linear move, as the ABC axes can't move (currently)
    // linear move is actually a move involving axes X Y Z, not the type of the movement
    linear_move = 1;

    // set proper velocity
    sendVelMsg(vel, ini_maxvel);

    thelix = (helical_length / acc);
    ta = da? (da / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[3])): 0.0;
    tb = db? (db / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[4])): 0.0;
    tc = dc? (dc / FROM_EXT_ANG(AXIS_MAX_ACCELERATION[5])): 0.0;
    tmax = MAX4(thelix, ta, tb, tc);

    if (tmax > 0.0) {
        acc = helical_length / tmax;
    }

    if(acc) sendAccMsg(acc);

    /* 
       mapping of rotation to turns:

       rotation turns -------- ----- 0 none (linear move) 1 0 2 1 -1 -1 -2 -2 */

    if (rotation == 0) {
	// linear move

	linearMoveMsg.end.tran.x = TO_EXT_LEN(end.tran.x);
	linearMoveMsg.end.tran.y = TO_EXT_LEN(end.tran.y);
	linearMoveMsg.end.tran.z = TO_EXT_LEN(end.tran.z);

	// fill in the orientation
	linearMoveMsg.end.a = TO_EXT_ANG(a);
	linearMoveMsg.end.b = TO_EXT_ANG(b);
	linearMoveMsg.end.c = TO_EXT_ANG(c);

        linearMoveMsg.type = EMC_MOTION_TYPE_ARC;
	interp_list.append(linearMoveMsg);
    } else if (rotation > 0) {
	circularMoveMsg.end.tran.x = TO_EXT_LEN(end.tran.x);
	circularMoveMsg.end.tran.y = TO_EXT_LEN(end.tran.y);
	circularMoveMsg.end.tran.z = TO_EXT_LEN(end.tran.z);

	circularMoveMsg.center.x = TO_EXT_LEN(center.x);
	circularMoveMsg.center.y = TO_EXT_LEN(center.y);
	circularMoveMsg.center.z = TO_EXT_LEN(center.z);

	circularMoveMsg.normal.x = TO_EXT_LEN(normal.x);
	circularMoveMsg.normal.y = TO_EXT_LEN(normal.y);
	circularMoveMsg.normal.z = TO_EXT_LEN(normal.z);

	circularMoveMsg.turn = rotation - 1;

	// fill in the orientation
	circularMoveMsg.end.a = TO_EXT_ANG(a);
	circularMoveMsg.end.b = TO_EXT_ANG(b);
	circularMoveMsg.end.c = TO_EXT_ANG(c);

        circularMoveMsg.type = EMC_MOTION_TYPE_ARC;
	interp_list.append(circularMoveMsg);
    } else {
	// reverse turn

	circularMoveMsg.end.tran.x = TO_EXT_LEN(end.tran.x);
	circularMoveMsg.end.tran.y = TO_EXT_LEN(end.tran.y);
	circularMoveMsg.end.tran.z = TO_EXT_LEN(end.tran.z);

	circularMoveMsg.center.x = TO_EXT_LEN(center.x);
	circularMoveMsg.center.y = TO_EXT_LEN(center.y);
	circularMoveMsg.center.z = TO_EXT_LEN(center.z);

	circularMoveMsg.normal.x = TO_EXT_LEN(normal.x);
	circularMoveMsg.normal.y = TO_EXT_LEN(normal.y);
	circularMoveMsg.normal.z = TO_EXT_LEN(normal.z);

	circularMoveMsg.turn = rotation;

	// fill in the orientation
	circularMoveMsg.end.a = TO_EXT_ANG(a);
	circularMoveMsg.end.b = TO_EXT_ANG(b);
	circularMoveMsg.end.c = TO_EXT_ANG(c);

        circularMoveMsg.type = EMC_MOTION_TYPE_ARC;
	interp_list.append(circularMoveMsg);
    }

    // update the end point
    canonUpdateEndPoint(end.tran.x, end.tran.y, end.tran.z, a, b, c);
}

/*
  STRAIGHT_PROBE is exactly the same as STRAIGHT_FEED, except that it
  uses a probe message instead of a linear move message.
*/

void DWELL(double seconds)
{
    EMC_TRAJ_DELAY delayMsg;

    delayMsg.delay = seconds;

    interp_list.append(delayMsg);
}

/* Spindle Functions */
void SPINDLE_RETRACT_TRAVERSE()
{
    /*! \todo FIXME-- unimplemented */
}

/* 0 is off, -1 is CCW, 1 is CW; used as flag if settting speed again */
static int spindleOn = 0;

void START_SPINDLE_CLOCKWISE()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;

    if (spindleSpeed == 0)
	CANON_ERROR("Spindle speed needs to be non-zero in order to enable.\nIf speed is 0 we have no way of telling that you really wanted clockwise.");

    emc_spindle_on_msg.speed = spindleSpeed;

    interp_list.append(emc_spindle_on_msg);

    spindleOn = 1;
}

void START_SPINDLE_COUNTERCLOCKWISE()
{
    EMC_SPINDLE_ON emc_spindle_on_msg;

    if (spindleSpeed == 0)
	CANON_ERROR("Spindle speed needs to be non-zero in order to enable.\nIf speed is 0 we have no way of telling that you really wanted counterclockwise.");

    emc_spindle_on_msg.speed = -spindleSpeed;

    interp_list.append(emc_spindle_on_msg);

    spindleOn = -1;
}

void SET_SPINDLE_SPEED(double r)
{
    // speed is in RPMs everywhere
    spindleSpeed = r;

    // check if we need to resend command
    if (spindleOn == 1) {
	START_SPINDLE_CLOCKWISE();
    } else if (spindleOn == -1) {
	START_SPINDLE_COUNTERCLOCKWISE();
    }
}

void STOP_SPINDLE_TURNING()
{
    EMC_SPINDLE_OFF emc_spindle_off_msg;

    interp_list.append(emc_spindle_off_msg);

    spindleOn = 0;
}

void SPINDLE_RETRACT()
{
    /*! \todo FIXME-- unimplemented */
}

void ORIENT_SPINDLE(double orientation, CANON_DIRECTION direction)
{
    /*! \todo FIXME-- unimplemented */
}

void USE_SPINDLE_FORCE(void)
{
    /*! \todo FIXME-- unimplemented */
}

void LOCK_SPINDLE_Z(void)
{
    /*! \todo FIXME-- unimplemented */
}

void USE_NO_SPINDLE_FORCE(void)
{
    /*! \todo FIXME-- unimplemented */
}

/* Tool Functions */

/*
  EMC has no tool length offset. To implement it, we save it here,
  and apply it when necessary
  */
void USE_TOOL_LENGTH_OFFSET(double length)
{
    EMC_TRAJ_SET_OFFSET set_offset_msg;

    /* convert to mm units for internal canonical use */
    currentToolLengthOffset = FROM_PROG_LEN(length);

    /* append it to interp list so it gets updated at the right time, not at
       read-ahead time */
    set_offset_msg.offset.tran.x = 0.0;
    set_offset_msg.offset.tran.y = 0.0;
    set_offset_msg.offset.tran.z = TO_EXT_LEN(currentToolLengthOffset);
    set_offset_msg.offset.a = 0.0;
    set_offset_msg.offset.b = 0.0;
    set_offset_msg.offset.c = 0.0;

    interp_list.append(set_offset_msg);
}

/* CHANGE_TOOL results from M6, for example */
void CHANGE_TOOL(int slot)
{
    EMC_TRAJ_LINEAR_MOVE linear_move_msg;
    EMC_TOOL_LOAD load_tool_msg;

    /* optional first move to tool change position */
    if (HAVE_TOOL_CHANGE_POSITION) {
	linear_move_msg.end.tran = TOOL_CHANGE_POSITION.tran;	// struct
	// copy
	linear_move_msg.end.a = 0.0;
	linear_move_msg.end.b = 0.0;
	linear_move_msg.end.c = 0.0;
        linear_move_msg.type = EMC_MOTION_TYPE_TOOLCHANGE;
	interp_list.append(linear_move_msg);
	/*! \todo FIXME-- orient spindle command goes here. We don't yet have an NML 
	   message for this. */
	/* first EMC_TOOL_LOAD message tells emcio to take tool out */
	interp_list.append(load_tool_msg);
    }

    /* optional move to clear Z */
    if (HAVE_TOOL_HOLDER_CLEAR) {
	linear_move_msg.end.tran = TOOL_HOLDER_CLEAR.tran;	// struct
	// copy
	linear_move_msg.end.a = 0.0;
	linear_move_msg.end.b = 0.0;
	linear_move_msg.end.c = 0.0;
        linear_move_msg.type = EMC_MOTION_TYPE_TOOLCHANGE;
	interp_list.append(linear_move_msg);
	/* second EMC_TOOL_LOAD message tells emcio rotate carousel */
	interp_list.append(load_tool_msg);
    }

    /* optional move back to tool change position */
    if (HAVE_TOOL_CHANGE_POSITION) {
	linear_move_msg.end.tran = TOOL_CHANGE_POSITION.tran;	// struct
	// copy
	linear_move_msg.end.a = 0.0;
	linear_move_msg.end.b = 0.0;
	linear_move_msg.end.c = 0.0;
        linear_move_msg.type = EMC_MOTION_TYPE_TOOLCHANGE;
	interp_list.append(linear_move_msg);
    }

    /* regardless of optional moves above, we'll always send a load tool
       message */
    interp_list.append(load_tool_msg);
}

/* SELECT_TOOL results from T1, for example */
void SELECT_TOOL(int slot)
{
    EMC_TOOL_PREPARE prep_for_tool_msg;

    prep_for_tool_msg.tool = slot;

    interp_list.append(prep_for_tool_msg);
}

/* Misc Functions */

void CLAMP_AXIS(CANON_AXIS axis)
{
    /*! \todo FIXME-- unimplemented */
}

/*
  setString and addString initializes or adds src to dst, never exceeding
  dst's maxlen chars.
*/

static char *setString(char *dst, const char *src, int maxlen)
{
    dst[0] = 0;
    strncat(dst, src, maxlen - 1);
    dst[maxlen - 1] = 0;
    return dst;
}

static char *addString(char *dst, const char *src, int maxlen)
{
    int dstlen = strlen(dst);
    int srclen = strlen(src);
    int actlen;

    if (srclen >= maxlen - dstlen) {
	actlen = maxlen - dstlen - 1;
	dst[maxlen - 1] = 0;
    } else {
	actlen = srclen;
    }

    strncat(dst, src, actlen);

    return dst;
}

/*
  The probe file is opened with a hot-comment (PROBEOPEN <filename>),
  and the results of each probed point are written to that file.
  The file is closed with a (PROBECLOSE) comment.
*/

static FILE *probefile = NULL;

void COMMENT(char *comment)
{
    // nothing need be done here, but you can play tricks with hot comments

    char msg[LINELEN];
    char probefilename[LINELEN];
    char *ptr;

    // set RPY orientation for subsequent moves
    if (!strncmp(comment, "RPY", strlen("RPY"))) {
	PM_RPY rpy;
	// it's RPY <R> <P> <Y>
	if (3 !=
	    sscanf(comment, "%*s %lf %lf %lf", &rpy.r, &rpy.p, &rpy.y)) {
	    // print current orientation
	    printf("rpy = %f %f %f, quat = %f %f %f %f\n",
		   rpy.r, rpy.p, rpy.y, quat.s, quat.x, quat.y, quat.z);
	} else {
	    // set and print orientation
	    quat = rpy;
	    printf("rpy = %f %f %f, quat = %f %f %f %f\n",
		   rpy.r, rpy.p, rpy.y, quat.s, quat.x, quat.y, quat.z);
	}
	return;
    }
    // open probe output file
    if (!strncmp(comment, "PROBEOPEN", strlen("PROBEOPEN"))) {
	// position ptr to first char after PROBEOPEN
	ptr = &comment[strlen("PROBEOPEN")];
	// and step over white space to name, or NULL
	while (isspace(*ptr)) {
	    ptr++;
	}
	setString(probefilename, ptr, LINELEN);
	if (NULL == (probefile = fopen(probefilename, "w"))) {
	    // pop up a warning message
	    setString(msg, "can't open probe file ", LINELEN);
	    addString(msg, probefilename, LINELEN);
	    MESSAGE(msg);
	    probefile = NULL;
	}
	return;
    }
    // close probe output file
    if (!strncmp(comment, "PROBECLOSE", strlen("PROBECLOSE"))) {
	if (probefile != NULL) {
	    fclose(probefile);
	    probefile = NULL;
	}
	return;
    }

    return;
}

void DISABLE_FEED_OVERRIDE()
{
    /*! \todo FIXME-- unimplemented */
}

void DISABLE_SPEED_OVERRIDE()
{
    /*! \todo FIXME-- unimplemented */
}

void ENABLE_FEED_OVERRIDE()
{
    /*! \todo FIXME-- unimplemented */
}

void ENABLE_SPEED_OVERRIDE()
{
    /*! \todo FIXME-- unimplemented */
}

void FLOOD_OFF()
{
    EMC_COOLANT_FLOOD_OFF flood_off_msg;

    interp_list.append(flood_off_msg);
}

void FLOOD_ON()
{
    EMC_COOLANT_FLOOD_ON flood_on_msg;

    interp_list.append(flood_on_msg);
}

void MESSAGE(char *s)
{
    EMC_OPERATOR_DISPLAY operator_display_msg;

    operator_display_msg.id = 0;
    strncpy(operator_display_msg.display, s, LINELEN);
    operator_display_msg.display[LINELEN - 1] = 0;

    interp_list.append(operator_display_msg);
}

void MIST_OFF()
{
    EMC_COOLANT_MIST_OFF mist_off_msg;

    interp_list.append(mist_off_msg);
}

void MIST_ON()
{
    EMC_COOLANT_MIST_ON mist_on_msg;

    interp_list.append(mist_on_msg);
}

void PALLET_SHUTTLE()
{
    /*! \todo FIXME-- unimplemented */
}

void TURN_PROBE_OFF()
{
    // don't do anything-- this is called when the probing is done
}

void TURN_PROBE_ON()
{
    EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG clearMsg;

    interp_list.append(clearMsg);
}

void UNCLAMP_AXIS(CANON_AXIS axis)
{
    /*! \todo FIXME-- unimplemented */
}

/* Program Functions */

void STOP(void)
{
}

void PROGRAM_STOP()
{
    /* 
       implement this as a pause. A resume will cause motion to proceed. */
    EMC_TASK_PLAN_PAUSE pauseMsg;

    interp_list.append(pauseMsg);
}

void OPTIONAL_PROGRAM_STOP()
{
    /*! \todo FIXME-- implemented as PROGRAM_STOP, that is, no option */
    PROGRAM_STOP();
}

void PROGRAM_END()
{
    EMC_TASK_PLAN_END endMsg;

    interp_list.append(endMsg);
}

/* returns the current x, y, z origin offsets */
CANON_VECTOR GET_PROGRAM_ORIGIN()
{
    CANON_VECTOR origin;

    /* and convert from mm units to interpreter units */
    origin.x = TO_PROG_LEN(programOrigin.x);
    origin.y = TO_PROG_LEN(programOrigin.y);
    origin.z = TO_PROG_LEN(programOrigin.z);

    return origin;		/* in program units */
}

/* returns the current active units */
CANON_UNITS GET_LENGTH_UNITS()
{
    return lengthUnits;
}

CANON_PLANE GET_PLANE()
{
    return activePlane;
}

double GET_TOOL_LENGTH_OFFSET()
{
    return TO_EXT_LEN(currentToolLengthOffset);
}

/*
  INIT_CANON()
  Initialize canonical local variables to defaults
  */
void INIT_CANON()
{
    double units;

    // initialize locals to original values
    programOrigin.x = 0.0;
    programOrigin.y = 0.0;
    programOrigin.z = 0.0;
    programOrigin.a = 0.0;
    programOrigin.b = 0.0;
    programOrigin.c = 0.0;
    activePlane = CANON_PLANE_XY;
    canonEndPoint.x = 0.0;
    canonEndPoint.y = 0.0;
    canonEndPoint.z = 0.0;
    canonEndPoint.a = 0.0;
    canonEndPoint.b = 0.0;
    canonEndPoint.c = 0.0;
    SET_MOTION_CONTROL_MODE(CANON_CONTINUOUS, 0);
    spindleSpeed = 0.0;
    preppedTool = 0;
    linear_move = 0;
    angular_move = 0;
    currentLinearFeedRate = 0.0;
    currentAngularFeedRate = 0.0;
    currentToolLengthOffset = 0.0;
    /* 
       to set the units, note that GET_EXTERNAL_LENGTH_UNITS() returns
       traj->linearUnits, which is already set from the .ini file in
       iniTraj(). This is a floating point number, in user units per mm. We
       can compare this against known values and set the symbolic values
       accordingly. If it doesn't match, we have an error. */
    units = GET_EXTERNAL_LENGTH_UNITS();
    if (fabs(units - 1.0 / 25.4) < 1.0e-3) {
	lengthUnits = CANON_UNITS_INCHES;
    } else if (fabs(units - 1.0) < 1.0e-3) {
	lengthUnits = CANON_UNITS_MM;
    } else {
	CANON_ERROR
	    ("non-standard length units, setting interpreter to mm");
	lengthUnits = CANON_UNITS_MM;
    }
}

/* Sends error message */
void CANON_ERROR(const char *fmt, ...)
{
    va_list ap;
    EMC_OPERATOR_ERROR operator_error_msg;

    operator_error_msg.id = 0;
    if (fmt != NULL) {
	va_start(ap, fmt);
	vsprintf(operator_error_msg.error, fmt, ap);
	va_end(ap);
    } else {
	operator_error_msg.error[0] = 0;
    }

    interp_list.append(operator_error_msg);
}

/*
  GET_EXTERNAL_TOOL_TABLE(int pocket)

  Returns the tool table structure associated with pocket. Note that
  pocket can run from 0 (by definition, no tool), to pocket CANON_TOOL_MAX - 1.

  The value from emc status is in user units. We need to convert these
  to interpreter units, by calling FROM_EXT_LEN() to get them to mm, and
  then switching on lengthUnits.
  */
CANON_TOOL_TABLE GET_EXTERNAL_TOOL_TABLE(int pocket)
{
    CANON_TOOL_TABLE retval;

    if (pocket < 0 || pocket >= CANON_TOOL_MAX) {
	retval.id = 0;
	retval.length = 0.0;
	retval.diameter = 0.0;
    } else {
	retval = emcStatus->io.tool.toolTable[pocket];

	// convert from user to program units
	retval.length = TO_PROG_LEN(FROM_EXT_LEN(retval.length));
	retval.diameter = TO_PROG_LEN(FROM_EXT_LEN(retval.diameter));
    }

    return retval;
}

CANON_POSITION GET_EXTERNAL_POSITION()
{
    CANON_POSITION position;
    EmcPose pos;

    pos = emcStatus->motion.traj.position;

    // first update internal record of last position
    canonEndPoint.x = FROM_EXT_LEN(pos.tran.x);
    canonEndPoint.y = FROM_EXT_LEN(pos.tran.y);
    canonEndPoint.z = FROM_EXT_LEN(pos.tran.z);

    canonEndPoint.a = FROM_EXT_ANG(pos.a);
    canonEndPoint.b = FROM_EXT_ANG(pos.b);
    canonEndPoint.c = FROM_EXT_ANG(pos.c);

    // now calculate position in program units, for interpreter
    position.x = TO_PROG_LEN(canonEndPoint.x - programOrigin.x);
    position.y = TO_PROG_LEN(canonEndPoint.y - programOrigin.y);
    position.z =
	TO_PROG_LEN(canonEndPoint.z - programOrigin.z -
		    currentToolLengthOffset);

    position.a = TO_PROG_ANG(canonEndPoint.a - programOrigin.a);
    position.b = TO_PROG_ANG(canonEndPoint.b - programOrigin.b);
    position.c = TO_PROG_ANG(canonEndPoint.c - programOrigin.c);

    return position;
}

CANON_POSITION GET_EXTERNAL_PROBE_POSITION()
{
    CANON_POSITION position;
    EmcPose pos;
    static CANON_POSITION last_probed_position;

    pos = emcStatus->motion.traj.probedPosition;

    // first update internal record of last position
    canonEndPoint.x = FROM_EXT_LEN(pos.tran.x) - programOrigin.x;
    canonEndPoint.y = FROM_EXT_LEN(pos.tran.y) - programOrigin.y;
    canonEndPoint.z = FROM_EXT_LEN(pos.tran.z) - programOrigin.z;
    canonEndPoint.z -= currentToolLengthOffset;

    canonEndPoint.a = FROM_EXT_ANG(pos.a) - programOrigin.a;
    canonEndPoint.b = FROM_EXT_ANG(pos.b) - programOrigin.b;
    canonEndPoint.c = FROM_EXT_ANG(pos.c) - programOrigin.c;

    // now calculate position in program units, for interpreter
    position.x = TO_PROG_LEN(canonEndPoint.x);
    position.y = TO_PROG_LEN(canonEndPoint.y);
    position.z = TO_PROG_LEN(canonEndPoint.z);
    position.z -= TO_PROG_LEN(currentToolLengthOffset);

    position.a = TO_PROG_ANG(canonEndPoint.a);
    position.b = TO_PROG_ANG(canonEndPoint.b);
    position.c = TO_PROG_ANG(canonEndPoint.c);

    /*! \todo FIXME-- back end of hot comment */
    if (probefile != NULL) {
	if (last_probed_position.x != position.x ||
	    last_probed_position.y != position.y ||
	    last_probed_position.z != position.z) {
	    fprintf(probefile, "%f %f %f\n", position.x, position.y,
		    position.z);
	    last_probed_position = position;
	}
    }

    return position;
}

double GET_EXTERNAL_PROBE_VALUE()
{
    // only for analog non-contact probe, so force a 0
    return 0.0;
}

int IS_EXTERNAL_QUEUE_EMPTY()
{
    return emcStatus->motion.traj.queue == 0 ? 1 : 0;
}

// feed rate wanted is in program units per minute
double GET_EXTERNAL_FEED_RATE()
{
    double feed;

    // convert from external to program units
    // it is wrong to use emcStatus->motion.traj.velocity here, as that is the traj speed regardless of G0 / G1
    feed = TO_PROG_LEN(FROM_EXT_LEN(currentLinearFeedRate));
    // now convert from per-sec to per-minute
    feed *= 60.0;

    return feed;
}

// traverse rate wanted is in program units per minute
double GET_EXTERNAL_TRAVERSE_RATE()
{
    double traverse;

    // convert from external to program units
    traverse =
	TO_PROG_LEN(FROM_EXT_LEN(emcStatus->motion.traj.maxVelocity));

    // now convert from per-sec to per-minute
    traverse *= 60.0;

    return traverse;
}

double GET_EXTERNAL_LENGTH_UNITS(void)
{
    double u;

    u = emcStatus->motion.traj.linearUnits;

    if (u == 0) {
	CANON_ERROR("external length units are zero");
	return 1.0;
    } else {
	return u;
    }
}

double GET_EXTERNAL_ANGLE_UNITS(void)
{
    double u;

    u = emcStatus->motion.traj.angularUnits;

    if (u == 0) {
	CANON_ERROR("external angle units are zero");
	return 1.0;
    } else {
	return u;
    }
}

int GET_EXTERNAL_TOOL()
{
    return emcStatus->io.tool.toolInSpindle;
}

int GET_EXTERNAL_MIST()
{
    return emcStatus->io.coolant.mist;
}

int GET_EXTERNAL_FLOOD()
{
    return emcStatus->io.coolant.flood;
}

int GET_EXTERNAL_POCKET()
{
    return emcStatus->io.tool.toolPrepped;
}

double GET_EXTERNAL_SPEED()
{
    // speed is in RPMs everywhere
    return emcStatus->io.spindle.speed;
}

CANON_DIRECTION GET_EXTERNAL_SPINDLE()
{
    if (emcStatus->io.spindle.speed == 0) {
	return CANON_STOPPED;
    }

    if (emcStatus->io.spindle.speed >= 0.0) {
	return CANON_CLOCKWISE;
    }

    return CANON_COUNTERCLOCKWISE;
}

int GET_EXTERNAL_TOOL_MAX()
{
    return CANON_TOOL_MAX;
}

char _parameter_file_name[LINELEN];	/* Not static.Driver
					   writes */

void GET_EXTERNAL_PARAMETER_FILE_NAME(char *file_name,	/* string: to copy
							   file name into */
				      int max_size)
{				/* maximum number of characters to copy */
    // Paranoid checks
    if (0 == file_name)
	return;

    if (max_size < 0)
	return;

    if (strlen(_parameter_file_name) < ((size_t) max_size))
	strcpy(file_name, _parameter_file_name);
    else
	file_name[0] = 0;
}

double GET_EXTERNAL_POSITION_X(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_POSITION();
    return position.x;
}

double GET_EXTERNAL_POSITION_Y(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_POSITION();
    return position.y;
}

double GET_EXTERNAL_POSITION_Z(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_POSITION();
    return position.z;
}

double GET_EXTERNAL_POSITION_A(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_POSITION();
    return position.a;
}

double GET_EXTERNAL_POSITION_B(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_POSITION();
    return position.b;
}

double GET_EXTERNAL_POSITION_C(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_POSITION();
    return position.c;
}

double GET_EXTERNAL_PROBE_POSITION_X(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_PROBE_POSITION();
    return position.x;
}

double GET_EXTERNAL_PROBE_POSITION_Y(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_PROBE_POSITION();
    return position.y;
}

double GET_EXTERNAL_PROBE_POSITION_Z(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_PROBE_POSITION();
    return position.z;
}

double GET_EXTERNAL_PROBE_POSITION_A(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_PROBE_POSITION();
    return position.a;
}

double GET_EXTERNAL_PROBE_POSITION_B(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_PROBE_POSITION();
    return position.b;
}

double GET_EXTERNAL_PROBE_POSITION_C(void)
{
    CANON_POSITION position;
    position = GET_EXTERNAL_PROBE_POSITION();
    return position.c;
}

CANON_MOTION_MODE GET_EXTERNAL_MOTION_CONTROL_MODE()
{
    return canonMotionMode;
}

double GET_EXTERNAL_MOTION_CONTROL_TOLERANCE()
{
    return canonMotionTolerance;
}


CANON_UNITS GET_EXTERNAL_LENGTH_UNIT_TYPE()
{
    return lengthUnits;
}

int GET_EXTERNAL_QUEUE_EMPTY(void)
{
    return emcStatus->motion.traj.queue == 0 ? 1 : 0;
}

int GET_EXTERNAL_TOOL_SLOT()
{
    return emcStatus->io.tool.toolInSpindle;
}

int GET_EXTERNAL_SELECTED_TOOL_SLOT()
{
    return emcStatus->io.tool.toolPrepped;
}


CANON_PLANE GET_EXTERNAL_PLANE()
{
    return activePlane;
}

USER_DEFINED_FUNCTION_TYPE USER_DEFINED_FUNCTION[USER_DEFINED_FUNCTION_NUM]
    = { 0 };

int USER_DEFINED_FUNCTION_ADD(USER_DEFINED_FUNCTION_TYPE func, int num)
{
    if (num < 0 || num >= USER_DEFINED_FUNCTION_NUM) {
	return -1;
    }

    USER_DEFINED_FUNCTION[num] = func;

    return 0;
}

/*! \function SET_MOTION_OUTPUT_BIT

  sets a DIO pin
  this message goes to task, then to motion which sets the DIO 
  when the first motion starts.
  The pin gets set with value 1 at the begin of motion, and stays 1 at the end of motion
  (this behaviour can be changed if needed)
  
  warning: setting more then one for a motion segment will clear out the previous ones 
  (the TP doesn't implement a queue of these), 
  use SET_AUX_OUTPUT_BIT instead, that allows to set the value right away
*/
void SET_MOTION_OUTPUT_BIT(int index)
{
  EMC_MOTION_SET_DOUT dout_msg;

  dout_msg.index = index;
  dout_msg.start = 1;		// startvalue = 1
  dout_msg.end = 1;		// endvalue = 1, means it doesn't get reset after current motion
  dout_msg.now = 0;		// not immediate, but synched with motion (goes to the TP)

  interp_list.append(dout_msg);

  return;
}

/*! \function CLEAR_MOTION_OUTPUT_BIT

  clears a DIO pin
  this message goes to task, then to motion which clears the DIO 
  when the first motion starts.
  The pin gets set with value 0 at the begin of motion, and stays 0 at the end of motion
  (this behaviour can be changed if needed)
  
  warning: setting more then one for a motion segment will clear out the previous ones 
  (the TP doesn't implement a queue of these), 
  use CLEAR_AUX_OUTPUT_BIT instead, that allows to set the value right away
*/
void CLEAR_MOTION_OUTPUT_BIT(int index)
{
  EMC_MOTION_SET_DOUT dout_msg;

  dout_msg.index = index;
  dout_msg.start = 0;           // startvalue = 1
  dout_msg.end = 0;		// endvalue = 0, means it stays 0 after current motion
  dout_msg.now = 0;		// not immediate, but synched with motion (goes to the TP)

  interp_list.append(dout_msg);

  return;
}

/*! \function SET_AUX_OUTPUT_BIT

  sets a DIO pin
  this message goes to task, then to motion which sets the DIO 
  right away.
  The pin gets set with value 1 at the begin of motion, and stays 1 at the end of motion
  (this behaviour can be changed if needed)
  you can use any number of these, as the effect is imediate  
*/
void SET_AUX_OUTPUT_BIT(int index)
{
/* we're gonna use the EMC_MOTION_SET_DOUT for now
  EMC_AUX_DIO_WRITE dio_msg;

  dio_msg.index = index;
  dio_msg.value = 1;

  interp_list.append(dio_msg); */

  EMC_MOTION_SET_DOUT dout_msg;

  dout_msg.index = index;
  dout_msg.start = 1;		// startvalue = 1
  dout_msg.end = 1;		// endvalue = 1, means it doesn't get reset after current motion
  dout_msg.now = 1;		// immediate, we don't care about synching for AUX

  interp_list.append(dout_msg);

  return;
}

/*! \function CLEAR_AUX_OUTPUT_BIT

  clears a DIO pin
  this message goes to task, then to motion which clears the DIO 
  right away.
  The pin gets set with value 0 at the begin of motion, and stays 0 at the end of motion
  (this behaviour can be changed if needed)
  you can use any number of these, as the effect is imediate  
*/
void CLEAR_AUX_OUTPUT_BIT(int index)
{
/* we're gonna use the EMC_MOTION_SET_DOUT for now
  EMC_AUX_DIO_WRITE dio_msg;

  dio_msg.index = index;
  dio_msg.value = 0;

  interp_list.append(dio_msg);*/
  EMC_MOTION_SET_DOUT dout_msg;

  dout_msg.index = index;
  dout_msg.start = 0;           // startvalue = 1
  dout_msg.end = 0;		// endvalue = 0, means it stays 0 after current motion
  dout_msg.now = 1;		// immediate, we don't care about synching for AUX

  interp_list.append(dout_msg);

  return;
}

/*! \function SET_MOTION_OUTPUT_VALUE

  sets a AIO value, not used by the RS274 Interp,
  not fully implemented in the motion controller either
*/
void SET_MOTION_OUTPUT_VALUE(int index, double value)
{
  EMC_MOTION_SET_AOUT aout_msg;

  aout_msg.index = index;	// which output
  aout_msg.start = value;	// start value
  aout_msg.end = value;		// end value
  aout_msg.now = 0;		// immediate=1, or synched when motion start=0

  interp_list.append(aout_msg);

  return;
}

/*! \function SET_AUX_OUTPUT_VALUE

  sets a AIO value, not used by the RS274 Interp,
  not fully implemented in the motion controller either
*/
void SET_AUX_OUTPUT_VALUE(int index, double value)
{
  EMC_AUX_AIO_WRITE aio_msg;

  aio_msg.index = index;
  aio_msg.value = value;

  interp_list.append(aio_msg);

  return;
}
