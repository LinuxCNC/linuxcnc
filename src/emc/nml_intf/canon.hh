/********************************************************************
* Description: canon.hh
*
*   Derived from a work by Thomas Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
#ifndef CANON_HH
#define CANON_HH

#include <stdio.h>		// FILE
#include <vector>

#include "emcpos.h"
#include "emctool.h"
#include "canon_position.hh"
#include "emcmotcfg.h" // Just for EMCMOT_NUM_SPINDLES

/*
  This is the header file that all applications that use the
  canonical commands for three- to nine-axis machining should include.

  Three mutually orthogonal (in a right-handed system) X, Y, and Z axes
  are always present. In addition, there may be zero to three rotational
  axes: A (parallel to the X-axis), B (parallel to the Y-axis), and C
  (parallel to the Z-axis). Additionally there may be zero to three linear
  axes: U, V and W.

  In the functions that use rotational axes, the axis value is that of a
  wrapped linear axis, in degrees.

  It is assumed in these activities that the spindle tip is always at
  some location called the 'current location,' and the controller always
  knows where that is. It is also assumed that there is always a
  'selected plane' which must be the XY-plane, the YZ-plane, or the
  ZX-plane of the machine.
*/

enum CanonBool {
    OFF,
    ON
};

struct CONTROL_POINT {          /* type for NURBS control points */
    double X,
    Y,
    W;
};

struct PLANE_POINT
{
    double X,
    Y;
};


enum CANON_PLANE
{
    CANON_PLANE_XY = 1,
    CANON_PLANE_YZ,
    CANON_PLANE_XZ,
    CANON_PLANE_UV,
    CANON_PLANE_VW,
    CANON_PLANE_UW,
};

enum CANON_UNITS
{
    CANON_UNITS_INCHES = 1,
    CANON_UNITS_MM,
    CANON_UNITS_CM,
};

enum CANON_MOTION_MODE
{
    CANON_EXACT_STOP = 1,
    CANON_EXACT_PATH,
    CANON_CONTINUOUS,
};

enum CANON_SPEED_FEED_MODE {
    CANON_SYNCHED = 1,
    CANON_INDEPENDENT,
};

enum CANON_DIRECTION {
    CANON_STOPPED = 1,
    CANON_CLOCKWISE,
    CANON_COUNTERCLOCKWISE,
};

enum CANON_FEED_REFERENCE {
    CANON_WORKPIECE = 1,
    CANON_XYZ,
};

enum CANON_SIDE
{
    CANON_SIDE_RIGHT = 1,
    CANON_SIDE_LEFT,
    CANON_SIDE_OFF,
};

enum CANON_AXIS
{
    CANON_AXIS_X = 1,
    CANON_AXIS_Y,
    CANON_AXIS_Z,
    CANON_AXIS_A,
    CANON_AXIS_B,
    CANON_AXIS_C,
    CANON_AXIS_U,
    CANON_AXIS_V,
    CANON_AXIS_W,
};

struct CANON_VECTOR {
    CANON_VECTOR() {
    } CANON_VECTOR(double _x, double _y, double _z) {
	x = _x;
	y = _y;
	z = _z;
    }
    double x, y, z;
};

typedef struct {
    int feed_mode;
    int synched;
    double speed;
    int dir;
    double css_maximum;
    double css_factor;
} CanonSpindle_t;

typedef struct CanonConfig_t {
    CanonConfig_t() : rotary_unlock_for_traverse(-1) {}

    double xy_rotation;
    int rotary_unlock_for_traverse; // jointnumber or -1

    CANON_POSITION g5xOffset;
    CANON_POSITION g92Offset;
/*
  canonEndPoint is the last programmed end point, stored in case it's
  needed for subsequent calculations. It's in absolute frame, mm units.

  note that when segments are queued for the naive cam detector that the
  canonEndPoint may not be the last programmed endpoint.  get_last_pos()
  retrieves the xyz position after the last of the queued segments.  these
  are also in absolute frame, mm units.
  */
    CANON_POSITION endPoint;
    CANON_UNITS lengthUnits;
    CANON_PLANE activePlane;
/* Tool length offset is saved here */
    EmcPose toolOffset;
/* motion control mode is used to signify blended v. stop-at-end moves.
   Set to 0 (invalid) at start, so first call will send command out */
    CANON_MOTION_MODE motionMode;
/* motion path-following tolerance is used to set the max path-following
   deviation during CANON_CONTINUOUS.
   If this param is 0, then it will behave as emc always did, allowing
   almost any deviation trying to keep speed up. */
   double motionTolerance;
   double naivecamTolerance;
   int feed_mode;
   int spindle_num; //current spindle for spindle-synch motion
   CanonSpindle_t spindle[EMCMOT_MAX_SPINDLES];

/* Prepped tool is saved here */
//   int preppedTool;
/*
  Feed rate is saved here; values are in mm/sec or deg/sec.
  It will be initially set in INIT_CANON() below.
*/
    double linearFeedRate;
    double angularFeedRate;
/* optional program stop */
    bool optional_program_stop;
/* optional block delete */
    bool block_delete;
/* Used to indicate whether the current move is linear, angular, or 
   a combination of both. */
   //AJ says: linear means axes XYZ move (lines or even circles)
   //         angular means axes ABC move
    int cartesian_move;
    int angular_move;
} CanonConfig_t;

/* Initialization */

/* reads world model data into the canonical interface */

/* Representation */




/* Offset the origin to the point with absolute coordinates x, y, z,
a, b, c, u, v, and w. Values of x, y, z, a, b, c, u, v, and w are real 
numbers. The units are whatever length units are being used at the time 
this command is given. */

/* Called from emctask to update the canon position during skipping through
   programs started with start-from-line > 0. */


/* Use the specified units for length. Conceptually, the units must
be either inches or millimeters. */


/* Use the plane designated by selected_plane as the selected plane.
Conceptually, the selected_plane must be the XY-plane, the XZ-plane, or
the YZ-plane. */

/* Free Space Motion */


/* Set the traverse rate that will be used when the spindle traverses. It
is expected that no cutting will occur while a traverse move is being
made. */

/*

Move at traverse rate so that at any time during the move, all axes
have covered the same proportion of their required motion. The final
XYZ position is given by x, y, and z. If there is an a-axis, its final
position is given by a_position, and similarly for the b-axis and c-axis.
A more positive value of a rotational axis is in the counterclockwise
direction.

Clockwise or counterclockwise is from the point of view of the
workpiece. If the workpiece is fastened to a turntable, the turntable
will turn clockwise (from the point of view of the machinist or anyone
else not moving with respect to the machining center) in order to make
the tool move counterclockwise from the point of view of the
workpiece.

*/

/* Machining Attributes */


/*

SET_FEED_RATE sets the feed rate that will be used when the spindle is
told to move at the currently set feed rate. The rate is either:
1. the rate of motion of the tool tip in the workpiece coordinate system,
   which is used when the feed_reference mode is "CANON_WORKPIECE", or
2. the rate of motion of the tool tip in the XYZ axis system, ignoring
   motion of other axes, which is used when the feed_reference mode is
   "CANON_XYZ".

The units of the rate are:

1. If the feed_reference mode is CANON_WORKPIECE:
length units (inches or millimeters according to the setting of
CANON_UNITS) per minute along the programmed path as seen by the
workpiece.

2. If the feed_reference mode is CANON_XYZ:
A. For motion including one rotational axis only: degrees per minute.
B. For motion of two or three rotational axes with X, Y, Z, U, V, and W 
   axes not moving, the rate is applied as follows. Let dA, dB, and dC 
   be the angles in degrees through which the A, B, and C axes, 
   respectively, must move. Let D = sqrt(dA*dA + dB*dB + dC*dC). 
   Conceptually, D is a measure of total angular motion, using the usual 
   Euclidean metric. Let T be the amount of time required to move through 
   D degrees at the current feed rate in degrees per minute. The 
   rotational axes should be moved in coordinated linear motion so that 
   the elapsed time from the start to the end of the motion is T plus any 
   time required for acceleration or deceleration.
C. For motion of secondary linear axes (U, V, and/or W) with X, Y, and Z 
   axes not moving (with or without simultaneous rotational axis motion): 
   length units (inches or millimeters according to the setting of 
   CANON_UNITS) per minute in the UVW cartesian system.
D. For motion involving one or more of the XYZ axes (with or without
   simultaneous motion of other axes): length units (inches or
   millimeters according to the setting of CANON_UNITS) per minute
   along the programmed XYZ path.

*/


/*

This sets the feed_reference mode to either CANON_WORKPIECE or
CANON_XYZ.

The CANON_WORKPIECE mode is more natural and general, since the rate
at which the tool passes through the material must be controlled for
safe and effective machining. For machines with more than the three
standard XYZ axes, however, computing the feed rate may be
time-consuming because the trajectories that result from motion in
four or more axes may be complex. Computation of path lengths when
only XYZ motion is considered is quite simple for the two standard
motion types (straight lines and helical arcs).

Some programming languages (rs274kt, in particular) use CANON_XYZ
mode. In these languages, the task of dealing with the rate at which
the tool tip passes through material is pushed back on the NC-program
generator, where the computation of path lengths is (almost always in
1995) an off-line activity where speed of calculation is not critical.

In CANON_WORKPIECE mode, some motions cannot be carried out as fast as
the programmed feed rate would require because axis motions tend to
cancel each other. For example, an arc in the YZ-plane can exactly
cancel a rotation around the A-axis, so that the location of the tool
tip with respect to the workpiece does not change at all during the
motion; in this case, the motion should take no time, which is
impossible at any finite rate of axis motion. In such cases, the axes
should be moved as fast as possible consistent with accurate
machining.

It would be possible to omit the SET_FEED_REFERENCE command from the
canonical commands and operate always in one mode or the other,
letting the interpreter issue SET_FEED_RATE commands, if necessary to
compensate if the NC language being interpreted used the other mode.

This would create two disadvantages when the feed_reference mode
assumed by the canonical commands differed from that assumed by the NC
language being interpreted:

1. The output code could have a lot of SET_FEED_RATE commands not
found in the input code; this is a relatively minor consideration.

2. If the interpreter reads a program in language which uses the
CANON_XYZ mode and writes canonical commands in the CANON_WORKPIECE
mode, both the interpreter and the executor of the output canonical
commands would have to perform a lot of complex calculations. With the
SET_FEED_REFERENCE command available, both do only simple calculations
for the same motions.

*/


/* This sets the feed mode: 0 for feed in units per minute, and 1 for feed in
 * units per revolution.  In units per revolution mode, the values are in
 * inches per revolution (G20 in effect) or mm per minute (G21 in effect)
 * The spindle number indicates which spindle the movement is synchronised to */



/*

This sets the motion control mode to one of: CANON_EXACT_STOP,
CANON_EXACT_PATH, or CANON_CONTINUOUS.

For CANON_CONTINUOUS another parameter defines the maximum path deviation. 
If tolerance=0 then any path deviation may occur, speed is maximized.

*/


/* Set the radius to use when performing cutter radius compensation. */


/* Conceptually, the direction must be left (meaning the cutter
stays to the left of the programmed path) or right. */


/* Do not apply cutter radius compensation when executing spindle
translation commands. */

/* used for threading */


/* Machining Functions */


/* Move in a helical arc from the current location at the existing feed
rate. The axis of the helix is parallel to the x, y, or z axis,
according to which one is perpendicular to the selected plane. The
helical arc may degenerate to a circular arc if there is no motion
parallel to the axis of the helix.

1. If the selected plane is the xy-plane:
A. first_end is the x-coordinate of the end of the arc.
B. second_end is the y-coordinate of the end of the arc.
C. first_axis is the x-coordinate of the axis (center) of the arc.
D. second_axis is the y-coordinate of the axis.
E. axis_end_point is the z-coordinate of the end of the arc.

2. If the selected plane is the yz-plane:
A. first_end is the y-coordinate of the end of the arc.
B. second_end is the z-coordinate of the end of the arc.
C. first_axis is the y-coordinate of the axis (center) of the arc.
D. second_axis is the z-coordinate of the axis.
E. axis_end_point is the x-coordinate of the end of the arc.

3. If the selected plane is the zx-plane:
A. first_end is the z-coordinate of the end of the arc.
B. second_end is the x-coordinate of the end of the arc.
C. first_axis is the z-coordinate of the axis (center) of the arc.
D. second_axis is the x-coordinate of the axis.
E. axis_end_point is the y-coordinate of the end of the arc.

If rotation is positive, the arc is traversed counterclockwise as
viewed from the positive end of the coordinate axis perpendicular to
the currently selected plane. If rotation is negative, the arc is
traversed clockwise. If rotation is 0, first_end and second_end must
be the same as the corresponding coordinates of the current point and
no arc is made (but there may be translation parallel to the axis
perpendicular to the selected plane and motion along the rotational axes).
If rotation is 1, more than 0 but not more than 360 degrees of arc
should be made. In general, if rotation is n, the amount of rotation
in the arc should be more than ([n-1] x 360) but not more than (n x
360).

The radius of the helix is determined by the distance from the current
location to the axis of helix or by the distance from the end location
to the axis of the helix. It is recommended that the executing system
verify that the two radii are the same (within some tolerance) at the
beginning of executing this function.

While the XYZ motion is going on, move the rotational axes so that
they have always covered the same proportion of their total motion as
a point moving along the arc has of its total motion.

*/


/* Additional functions needed to calculate nurbs points */

extern std::vector<unsigned int> knot_vector_creator(unsigned int n, unsigned int k);
extern double Nmix(unsigned int i, unsigned int k, double u, 
                    std::vector<unsigned int> knot_vector);
extern double Rden(double u, unsigned int k,
                  std::vector<CONTROL_POINT> nurbs_control_points,
                  std::vector<unsigned int> knot_vector);
extern PLANE_POINT nurbs_point(double u, unsigned int k, 
                  std::vector<CONTROL_POINT> nurbs_control_points,
                  std::vector<unsigned int> knot_vector);
extern PLANE_POINT nurbs_tangent(double u, unsigned int k,
                  std::vector<CONTROL_POINT> nurbs_control_points,
                  std::vector<unsigned int> knot_vector);
extern double alpha_finder(double dx, double dy);

/* Canon calls */

/* Move at the feed rate along an approximation of a NURBS with a variable number
 * of control points
 */

/* Move at existing feed rate so that at any time during the move,
all axes have covered the same proportion of their required motion.
The meanings of the parameters is the same as for STRAIGHT_TRAVERSE.*/


/* Move linear and synced with the previously set pitch.
Only linear moves are allowed, axes A,B,C are not allowed to move.*/



/* Perform a probing operation. This is a temporary addition to the
canonical machining functions and its semantics are not defined.
When the operation is finished, all axes should be back where they
started. */


/* stop motion after current feed */


/* freeze x,y,z for a time */

/* Spindle Functions */


/* Retract the spindle at traverse rate to the fully retracted position. */


/* Turn the spindle clockwise at the currently set speed rate. If the
spindle is already turning that way, this command has no effect. */


/* Turn the spindle counterclockwise at the currently set speed rate. If
the spindle is already turning that way, this command has no effect. */


/* Set the spindle speed that will be used when the spindle is turning.
This is usually given in rpm and refers to the rate of spindle
rotation. If the spindle is already turning and is at a different
speed, change to the speed given with this command. */


/* Stop the spindle from turning. If the spindle is already stopped, this
command may be given, but it will have no effect. */


/* Tool Functions */



/* In extension to the comment above - for CHANGE_TOOL, sometimes on 
startup one would want to tell emc2 what tool it has loaded. As the last
toolnumber before shutdown isn't currently written, there is no provision
to allow emc2 to safely restart knowing what tool is in the spindle.
Using CHANGE_TOOL_NUMBER one can tell emc2 (without any physical action)
to set the mapping of the currently loaded tool to a certain number */

/* executed at the very start of an M6 command before any movements,
spindle stop or quill up have been issued, to speed up toolchanging
process. Passed through to iocontrol to drive a pin. */

/* Miscellaneous Functions */


/* Clamp the given axis. If the machining center does not have a clamp
for that axis, this command should result in an error condition in the
controller.

An attempt to move an axis while it is clamped should result in an
error condition in the controller. */


/* This function has no physical effect. If commands are being printed or
logged, the comment command is printed or logged, including the string
which is the value of comment_text. This serves to allow formal
comments at specific locations in programs or command files. */

/* used for EDM adaptive moves with motion internal feed override (0..1) */

/* used to deactivate user control of feed override */

/* used to deactivate user control of spindle speed override */

/* used to deactivate user control of feed hold */


/* Turn flood coolant off. */
/* Turn flood coolant on. */



/* Turn mist coolant off. */

/* Turn mist coolant on. */


/* If the machining center has a pallet shuttle mechanism (a mechanism
which switches the position of two pallets), this command should cause
that switch to be made. If either or both of the pallets are missing,
this will not result in an error condition in the controller.

If the machining center does not have a pallet shuttle, this command
should result in an error condition in the controller. */



/* Unclamp the given axis. If the machining center does not have a clamp
for that axis, this command should result in an error condition in the
controller. */

/* NURB Functions */


/* Block delete */
/* Command to set the internal reference of block delete.
The ON value for enabled will cause the interpreter to discard lines
that start with the "/" character. */

/* Command to get the internal reference of optional block delete. */


/* Program Functions */
/* If the machining center has an optional stop switch, and it is on
when this command is read from a program, stop executing the program
at this point, but be prepared to resume with the next line of the
program. If the machining center does not have an optional stop
switch, or commands are being executed with a stop after each one
already (such as when the interpreter is being used with keyboard
input), this command has no effect. */

/* Command to set the internal reference of optional program stop.
Any non-zero value for state will cause the execution to stop on
optional stops. */

/* Command to get the internal reference of optional program stop. */

/* If a program is being read, stop executing the program and be prepared
to accept a new program or to be shut down. */

/* If this command is read from a program, stop executing the program at
this point, but be prepared to resume with the next line of the
program. If commands are being executed with a stop after each one
already (such as when the interpreter is being used with keyboard
input), this command has no effect. */


/* Commands to set/reset output bits and analog values */


/* Commands to wait for, query input bits and analog values */

#define DIGITAL_INPUT 1
#define ANALOG_INPUT 0

#define WAIT_MODE_IMMEDIATE	0
#define WAIT_MODE_RISE 		1
#define WAIT_MODE_FALL		2
#define WAIT_MODE_HIGH		3
#define WAIT_MODE_LOW		4


/* tell canon that is no longer the case */

/*************************************************************************/

/* Canonical "Give me information" functions for the interpreter to call

In general, returned values are valid only if any canonical do it commands
that may have been called for have been executed to completion. If a function
returns a valid value regardless of execution, that is noted in the comments
below.

*/

/* The interpreter is not using this function
// Returns the system angular unit factor, in units / degree
*/

// Returns the system feed rate

// Returns the system value for flood coolant, zero = off, non-zero = on

/* The interpreter is not using this function
// Returns the system length unit factor, in units / mm
*/

// Returns the system length unit type
CANON_UNITS GET_EXTERNAL_LENGTH_UNIT_TYPE();


// Returns the system value for mist coolant, zero = off, non-zero = on

// Returns the current motion control mode

// Returns the current motion path-following tolerance

// Returns the current motion naive CAM tolerance

/* The interpreter is not using these six GET_EXTERNAL_ORIGIN functions

// returns the current a-axis origin offset

// returns the current b-axis origin offset

// returns the current c-axis origin offset

// returns the current x-axis origin offset

// returns the current y-axis origin offset

// returns the current z-axis origin offset

*/

// returns nothing but copies the name of the parameter file into
// the filename array, stopping at max_size if the name is longer
// An empty string may be placed in filename.


// returns the currently active plane

// returns the current a-axis position

// returns the current b-axis position

// returns the current c-axis position

// returns the current x-axis position

// returns the current y-axis position

// returns the current z-axis position

// returns the current u-axis position

// returns the current v-axis position

// returns the current w-axis position


// Returns the position of the specified axis at the last probe trip,
// in the current work coordinate system.

// Returns the value for any analog non-contact probing.

// Whether the probe changed state during the last probing move

// Returns zero if queue is not empty, non-zero if the queue is empty
// This always returns a valid value

// Returns the system value for spindle speed in rpm

// Returns the system value for direction of spindle turning

// returns current tool length offset

// Returns the system value for the carousel slot in which the tool
// currently in the spindle belongs. Return value zero means there is no
// tool in the spindle.

// Returns the system value for the selected slot. That one will be the next
// valid tool after a toolchange (m6). Return value -1 means there is no
// selected tool.

// Returns the CANON_TOOL_TABLE structure associated with the tool
// in the given pocket

// return the value of iocontrol's toolchanger-fault pin

// return the value of iocontrol's toolchanger-reason pin
int GET_EXTERNAL_TC_REASON();

// Returns the system traverse rate

// Returns the enabled/disabled status for feed override, spindle
// override, adaptive feed, and feed hold

// Functions to query digital/analog Inputs
/* def is a default value which should be returned by canon functions
   that can't actually read external hardware - simulators and such */
/* returns current value of the digital input selected by index.*/

/* returns current value of the analog input selected by index.*/

/* returns result of user defined function */

// Returns the mask of axes present in the system


#define PARAMETER_FILE_NAME_LENGTH 100

#define USER_DEFINED_FUNCTION_NUM 100
typedef void (*USER_DEFINED_FUNCTION_TYPE) (int num, double arg1,
					    double arg2);

#endif				/* ifndef CANON_HH */
