/********************************************************************
* Description: nullcanon.cc
* dummy layer for profiling the interpreter without any canon overhead
********************************************************************/

#include "canon.hh"
#include "rs274ngc.hh"
#include "rs274ngc_interp.hh"
#include "rtapi_math.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

/* where to print */
//extern FILE * _outfile;
FILE * _outfile=NULL;      /* where to print, set in main */

/* Dummy world model */

static CANON_PLANE       _active_plane = CANON_PLANE_XY;
static int               _active_slot = 1;
static int               _feed_mode = 0;
static double            _feed_rate = 0.0;
static int               _flood = 0;
static double            _length_unit_factor = 1; /* 1 for MM 25.4 for inch */
static CANON_UNITS       _length_unit_type = CANON_UNITS_MM;
static int               _line_number = 1;
static int               _mist = 0;
static CANON_MOTION_MODE _motion_mode = CANON_CONTINUOUS;
char                     _parameter_file_name[PARAMETER_FILE_NAME_LENGTH];/*Not static.Driver writes*/
static double            _probe_position_a = 0; /*AA*/
static double            _probe_position_b = 0; /*BB*/
static double            _probe_position_c = 0; /*CC*/
static double            _probe_position_x = 0;
static double            _probe_position_y = 0;
static double            _probe_position_z = 0;
static double _g5x_x, _g5x_y, _g5x_z;
static double _g5x_a, _g5x_b, _g5x_c;
static double _g92_x, _g92_y, _g92_z;
static double _g92_a, _g92_b, _g92_c;
static double            _program_position_a = 0; /*AA*/
static double            _program_position_b = 0; /*BB*/
static double            _program_position_c = 0; /*CC*/
static double            _program_position_x = 0;
static double            _program_position_y = 0;
static double            _program_position_z = 0;
static double            _spindle_speed;
static CANON_DIRECTION   _spindle_turning;
int                      _pockets_max = CANON_POCKETS_MAX; /*Not static. Driver reads  */
CANON_TOOL_TABLE         _tools[CANON_POCKETS_MAX]; /*Not static. Driver writes */
/* optional program stop */
static bool optional_program_stop = ON; //set enabled by default (previous EMC behaviour)
/* optional block delete */
static bool block_delete = ON; //set enabled by default (previous EMC behaviour)
static double motion_tolerance = 0.;
static double naivecam_tolerance = 0.;
/* Dummy status variables */
static double            _traverse_rate;

static EmcPose _tool_offset;
static bool _toolchanger_fault;
static int  _toolchanger_reason;

/************************************************************************/

/* Canonical "Do it" functions

This is a set of dummy definitions for the canonical machining functions
given in canon.hh. These functions just print themselves and, if necessary,
update the dummy world model. On each output line is printed:
1. an output line number (sequential, starting with 1).
2. an input line number read from the input (or ... if not provided).
3. a printed representation of the function call which was made.

If an interpreter which makes these calls is compiled with this set of
definitions, it can be used as a translator by redirecting output from
stdout to a file.

*/

//extern void rs274ngc_line_text(char * line_text, int max_size);
extern InterpBase *pinterp;
#define interp_new (*pinterp)

void print_nc_line_number()
{
}


#define PRINT0(control) if (1)                        \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++); \
           print_nc_line_number();                    \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control);                \
          } else
#define PRINT1(control, arg1) if (1)                  \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++); \
           print_nc_line_number();                    \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control, arg1);          \
          } else
#define PRINT2(control, arg1, arg2) if (1)            \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++); \
           print_nc_line_number();                    \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control, arg1, arg2);    \
          } else
#define PRINT3(control, arg1, arg2, arg3) if (1)         \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);    \
           print_nc_line_number();                       \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control, arg1, arg2, arg3); \
          } else
#define PRINT4(control, arg1, arg2, arg3, arg4) if (1)         \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);          \
           print_nc_line_number();                             \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control, arg1, arg2, arg3, arg4); \
          } else
#define PRINT5(control, arg1, arg2, arg3, arg4, arg5) if (1)         \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);                \
           print_nc_line_number();                                   \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control, arg1, arg2, arg3, arg4, arg5); \
          } else
#define PRINT6(control, arg1, arg2, arg3, arg4, arg5, arg6) if (1)         \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);                      \
           print_nc_line_number();                                         \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control, arg1, arg2, arg3, arg4, arg5, arg6); \
          } else
#define PRINT7(control, arg1, arg2, arg3, arg4, arg5, arg6, arg7) if (1) \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);                    \
           print_nc_line_number();                                       \
           {if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  control,                                    \
                           arg1, arg2, arg3, arg4, arg5, arg6, arg7);    \
          } else
#define PRINT9(control,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9) \
          if (1)                                                            \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);                       \
           print_nc_line_number();                                          \
           fprintf(_outfile, control,                                       \
                   arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9);           \
          } else
#define PRINT10(control,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10) \
          if (1)                                                            \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);                       \
           print_nc_line_number();                                          \
           fprintf(_outfile, control,                                       \
                   arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10);     \
          } else
#define PRINT14(control,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13,arg14) \
          if (1)                                                            \
          {{if(_outfile==NULL){_outfile=stdout;}} fprintf(_outfile,  "%5d ", _line_number++);                       \
           print_nc_line_number();                                          \
           fprintf(_outfile, control,                                       \
                   arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13,arg14); \
          } else

/* Representation */

void SET_XY_ROTATION(double t) {
}

void SET_G5X_OFFSET(int index,
                    double x, double y, double z,
                    double a, double b, double c,
                    double u, double v, double w) {
}

void SET_G92_OFFSET(double x, double y, double z,
                    double a, double b, double c,
                    double u, double v, double w) {
}

void USE_LENGTH_UNITS(CANON_UNITS in_unit)
{
}

/* Free Space Motion */
void SET_TRAVERSE_RATE(double rate)
{

}

void STRAIGHT_TRAVERSE( int line_number,
 double x, double y, double z
 , double a /*AA*/
 , double b /*BB*/
 , double c /*CC*/
 , double u, double v, double w
)
{
}

/* Machining Attributes */
void SET_FEED_MODE(int mode)
{
}
void SET_FEED_RATE(double rate)
{
}

void SET_FEED_REFERENCE(CANON_FEED_REFERENCE reference)
{

}

extern void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode, double tolerance)
{
}

extern void SET_NAIVECAM_TOLERANCE(double tolerance)
{
}

void SELECT_PLANE(CANON_PLANE in_plane)
{

}

void SET_CUTTER_RADIUS_COMPENSATION(double radius)
{}

void START_CUTTER_RADIUS_COMPENSATION(int side)
{
}

void STOP_CUTTER_RADIUS_COMPENSATION()
{}

void START_SPEED_FEED_SYNCH()
{}

void STOP_SPEED_FEED_SYNCH()
{}

/* Machining Functions */

void NURBS_FEED(int lineno,
std::vector<CONTROL_POINT> nurbs_control_points, unsigned int k)
{

}

void ARC_FEED(int line_number,
 double first_end, double second_end,
 double first_axis, double second_axis, int rotation, double axis_end_point
 , double a /*AA*/
 , double b /*BB*/
 , double c /*CC*/
 , double u, double v, double w
)
{

}

void STRAIGHT_FEED(int line_number,
 double x, double y, double z
 , double a /*AA*/
 , double b /*BB*/
 , double c /*CC*/
 , double u, double v, double w
)
{

}


/* This models backing the probe off 0.01 inch or 0.254 mm from the probe
point towards the previous location after the probing, if the probe
point is not the same as the previous point -- which it should not be. */

void STRAIGHT_PROBE(int line_number,
 double x, double y, double z
 , double a /*AA*/
 , double b /*BB*/
 , double c /*CC*/
 , double u, double v, double w, unsigned char probe_type
)
{

}


void RIGID_TAP(int line_number, double x, double y, double z)
{
}


void DWELL(double seconds)
{}

/* Spindle Functions */
void SPINDLE_RETRACT_TRAVERSE()
{}

void SET_SPINDLE_MODE(double arg) {
}

void START_SPINDLE_CLOCKWISE()
{
}

void START_SPINDLE_COUNTERCLOCKWISE()
{

}

void SET_SPINDLE_SPEED(double rpm)
{
}

void STOP_SPINDLE_TURNING()
{

}

void SPINDLE_RETRACT()
{}

void ORIENT_SPINDLE(double orientation, int mode)
{
}

void WAIT_SPINDLE_ORIENT_COMPLETE(double timeout)
{
}

void USE_NO_SPINDLE_FORCE()
{}

/* Tool Functions */
void SET_TOOL_TABLE_ENTRY(int pocket, int toolno, EmcPose offset, double diameter,
                          double frontangle, double backangle, int orientation) {
}

void USE_TOOL_LENGTH_OFFSET(EmcPose offset)
{
}

void CHANGE_TOOL(int slot)
{

}

void SELECT_POCKET(int slot, int tool)
{}

void CHANGE_TOOL_NUMBER(int slot)
{

}


/* Misc Functions */

void CLAMP_AXIS(CANON_AXIS axis)
{}

void COMMENT(const char *s)
{}

void DISABLE_ADAPTIVE_FEED()
{}

void DISABLE_FEED_HOLD()
{}

void DISABLE_FEED_OVERRIDE()
{}

void DISABLE_SPEED_OVERRIDE()
{}

void ENABLE_ADAPTIVE_FEED()
{}

void ENABLE_FEED_HOLD()
{}

void ENABLE_FEED_OVERRIDE()
{}

void ENABLE_SPEED_OVERRIDE()
{}

void FLOOD_OFF()
{
}

void FLOOD_ON()
{
}

void INIT_CANON()
{
}

void MESSAGE(char *s)
{}

void LOG(char *s)
{}
void LOGOPEN(char *s)
{}
void LOGAPPEND(char *s)
{}
void LOGCLOSE()
{}

void MIST_OFF()
{

}

void MIST_ON()
{
}

void PALLET_SHUTTLE()
{}

void TURN_PROBE_OFF()
{}

void TURN_PROBE_ON()
{}

void UNCLAMP_AXIS(CANON_AXIS axis)
{}

/* Program Functions */

void PROGRAM_STOP()
{}

void SET_BLOCK_DELETE(bool state)
{block_delete = state;} //state == ON, means we don't interpret lines starting with "/"

bool GET_BLOCK_DELETE()
{return block_delete;} //state == ON, means we  don't interpret lines starting with "/"

void SET_OPTIONAL_PROGRAM_STOP(bool state)
{optional_program_stop = state;} //state == ON, means we stop

bool GET_OPTIONAL_PROGRAM_STOP()
{return optional_program_stop;} //state == ON, means we stop

void OPTIONAL_PROGRAM_STOP()
{}

void PROGRAM_END()
{}


/*************************************************************************/

/* Canonical "Give me information" functions

In general, returned values are valid only if any canonical do it commands
that may have been called for have been executed to completion. If a function
returns a valid value regardless of execution, that is noted in the comments
below.

*/

/* The interpreter is not using this function
// Returns the system angular unit factor, in units / degree
extern double GET_EXTERNAL_ANGLE_UNIT_FACTOR()
{
  return 1;
}
*/

/* MGS - FIXME - These functions should not be stubbed out to return constants.
They should return variable values... */
int GET_EXTERNAL_ADAPTIVE_FEED_ENABLE() {return 0;}
int GET_EXTERNAL_FEED_OVERRIDE_ENABLE() {return 1;}
double GET_EXTERNAL_MOTION_CONTROL_TOLERANCE() { return motion_tolerance;}
double GET_EXTERNAL_LENGTH_UNITS() {return 0.03937007874016;}
int GET_EXTERNAL_FEED_HOLD_ENABLE() {return 1;}
int GET_EXTERNAL_AXIS_MASK() {return 0x3f;} // XYZABC machine
double GET_EXTERNAL_ANGLE_UNITS() {return 1.0;}
int GET_EXTERNAL_SELECTED_TOOL_SLOT() { return 0; }
int GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE() {return 1;}
void START_SPEED_FEED_SYNCH(double sync, bool vel)
{}
CANON_MOTION_MODE motion_mode;

int GET_EXTERNAL_DIGITAL_INPUT(int index, int def) { return def; }
double GET_EXTERNAL_ANALOG_INPUT(int index, double def) { return def; }
int WAIT(int index, int input_type, int wait_type, double timeout) { return 0; }
int UNLOCK_ROTARY(int line_no, int axis) {return 0;}
int LOCK_ROTARY(int line_no, int axis) {return 0;}

/* Returns the system feed rate */
double GET_EXTERNAL_FEED_RATE()
{
  return _feed_rate;
}

/* Returns the system flood coolant setting zero = off, non-zero = on */
int GET_EXTERNAL_FLOOD()
{
  return _flood;
}

/* Returns the system length unit type */
CANON_UNITS GET_EXTERNAL_LENGTH_UNIT_TYPE()
{
  return _length_unit_type;
}

/* Returns the system mist coolant setting zero = off, non-zero = on */
extern int GET_EXTERNAL_MIST()
{
  return _mist;
}

// Returns the current motion control mode
extern CANON_MOTION_MODE GET_EXTERNAL_MOTION_CONTROL_MODE()
{
  return _motion_mode;
}


void GET_EXTERNAL_PARAMETER_FILE_NAME(
 char * file_name,       /* string: to copy file name into       */
 int max_size)           /* maximum number of characters to copy */
{
    // Paranoid checks
    if (0 == file_name)
	return;

    if (max_size < 0)
	return;

  if (strlen(_parameter_file_name) < (unsigned int)max_size)
    strcpy(file_name, _parameter_file_name);
  else
    file_name[0] = 0;
}

CANON_PLANE GET_EXTERNAL_PLANE()
{
  return _active_plane;
}

/* returns the current a-axis position */
double GET_EXTERNAL_POSITION_A()
{
  return _program_position_a;
}

/* returns the current b-axis position */
double GET_EXTERNAL_POSITION_B()
{
  return _program_position_b;
}

/* returns the current c-axis position */
double GET_EXTERNAL_POSITION_C()
{
  return _program_position_c;
}

/* returns the current x-axis position */
double GET_EXTERNAL_POSITION_X()
{
  return _program_position_x;
}

/* returns the current y-axis position */
double GET_EXTERNAL_POSITION_Y()
{
  return _program_position_y;
}

/* returns the current z-axis position */
double GET_EXTERNAL_POSITION_Z()
{
  return _program_position_z;
}

// XXX fix sai for uvw
double GET_EXTERNAL_POSITION_U()
{
    return 0.;
}

double GET_EXTERNAL_POSITION_V()
{
    return 0.;
}

double GET_EXTERNAL_POSITION_W()
{
    return 0.;
}

double GET_EXTERNAL_PROBE_POSITION_U()
{
    return 0.;
}

double GET_EXTERNAL_PROBE_POSITION_V()
{
    return 0.;
}

double GET_EXTERNAL_PROBE_POSITION_W()
{
    return 0.;
}

/* returns the a-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_A()
{
  return _probe_position_a;
}

/* returns the b-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_B()
{
  return _probe_position_b;
}

/* returns the c-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_C()
{
  return _probe_position_c;
}

/* returns the x-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_X()
{
  return _probe_position_x;
}

/* returns the y-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_Y()
{
  return _probe_position_y;
}

/* returns the z-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_Z()
{
  return _probe_position_z;
}

/* Returns the value for any analog non-contact probing. */
/* This is a dummy of a dummy, returning a useless value. */
/* It is not expected this will ever be called. */
extern double GET_EXTERNAL_PROBE_VALUE()
{
  return 1.0;
}

extern int GET_EXTERNAL_PROBE_TRIPPED_VALUE()
{
    return 0;
}

/* Returns zero if queue is not empty, non-zero if the queue is empty */
/* In the stand-alone interpreter, there is no queue, so it is always empty */
extern int GET_EXTERNAL_QUEUE_EMPTY()
{
  return 1;
}

/* Returns the system value for spindle speed in rpm */
double GET_EXTERNAL_SPEED()
{
  return _spindle_speed;
}

/* Returns the system value for direction of spindle turning */
extern CANON_DIRECTION GET_EXTERNAL_SPINDLE()
{
  return _spindle_turning;
}

/* Returns the system value for the carousel slot in which the tool
currently in the spindle belongs. Return value zero means there is no
tool in the spindle. */
extern int GET_EXTERNAL_TOOL_SLOT()
{
  return _active_slot;
}

/* Returns maximum number of pockets */
int GET_EXTERNAL_POCKETS_MAX()
{
  return _pockets_max;
}

/* Returns the CANON_TOOL_TABLE structure associated with the tool
   in the given pocket */
extern CANON_TOOL_TABLE GET_EXTERNAL_TOOL_TABLE(int pocket)
{
  return _tools[pocket];
}

/* Returns the system traverse rate */
double GET_EXTERNAL_TRAVERSE_RATE()
{
  return _traverse_rate;
}

USER_DEFINED_FUNCTION_TYPE USER_DEFINED_FUNCTION[USER_DEFINED_FUNCTION_NUM] = {0};

int USER_DEFINED_FUNCTION_ADD(USER_DEFINED_FUNCTION_TYPE func, int num)
{
  if (num < 0 || num >= USER_DEFINED_FUNCTION_NUM) {
    return -1;
  }

  USER_DEFINED_FUNCTION[num] = func;

  return 0;
}

void SET_MOTION_OUTPUT_BIT(int index)
{
    return;
}

void CLEAR_MOTION_OUTPUT_BIT(int index)
{
    return;
}

void SET_MOTION_OUTPUT_VALUE(int index, double value)
{
    return;
}

void SET_AUX_OUTPUT_BIT(int index)
{
    return;
}

void CLEAR_AUX_OUTPUT_BIT(int index)
{
    return;
}

void SET_AUX_OUTPUT_VALUE(int index, double value)
{
    return;
}

double GET_EXTERNAL_TOOL_LENGTH_XOFFSET()
{
    return _tool_offset.tran.x;
}

double GET_EXTERNAL_TOOL_LENGTH_YOFFSET()
{
    return _tool_offset.tran.y;
}

double GET_EXTERNAL_TOOL_LENGTH_ZOFFSET()
{
    return _tool_offset.tran.z;
}

double GET_EXTERNAL_TOOL_LENGTH_AOFFSET()
{
    return _tool_offset.a;
}

double GET_EXTERNAL_TOOL_LENGTH_BOFFSET()
{
    return _tool_offset.b;
}

double GET_EXTERNAL_TOOL_LENGTH_COFFSET()
{
    return _tool_offset.c;
}

double GET_EXTERNAL_TOOL_LENGTH_UOFFSET()
{
    return _tool_offset.u;
}

double GET_EXTERNAL_TOOL_LENGTH_VOFFSET()
{
    return _tool_offset.v;
}

double GET_EXTERNAL_TOOL_LENGTH_WOFFSET()
{
    return _tool_offset.w;
}

void FINISH(void) {
}

void START_CHANGE(void) {
}


int GET_EXTERNAL_TC_FAULT()
{
    return _toolchanger_fault;
}

int GET_EXTERNAL_TC_REASON()
{
    return _toolchanger_reason;
}

/* Sends error message */
void CANON_ERROR(const char *fmt, ...)
{
    va_list ap;

    if (fmt != NULL) {
	va_start(ap, fmt);
	char *err;
	int res = vasprintf(&err, fmt, ap);
	if(res < 0) err = 0;
	va_end(ap);
	if(err)
	{
	    PRINT1("CANON_ERROR(%s)\n", err);
	    free(err);
	}
	else
	{
	    PRINT1("CANON_ERROR(vasprintf failed: %s)\n", strerror(errno));
	}
    }
}
void PLUGIN_CALL(int len, const char *call)
{
}

void IO_PLUGIN_CALL(int len, const char *call)
{
}
