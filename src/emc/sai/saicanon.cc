/********************************************************************
* Description: saicanon.cc
*
* This file contains two sets of functions:
* 1. functions for the interpreter to call to tell the rest of the world to
* do something. These all return nothing.
* 2. functions for the interpreter to call to get information from the rest
* of the world. These all return some type of information.
* These functions implement the interface between the RS274NGC interpreter
* and some external environment.
*
* This version of canon.cc also includes a third set of stuff: a dummy
* model of the external world. The dummy model is used by the second set
* of interface functions.
*
* Modification history:
*
* 22-Mar-2004  FMP added USER_DEFINED_FUNCTION(), SET_MOTION_OUTPUT_BIT()
* and related
* 16-Mar-2004  FMP added SYSTEM()
* Early March 2007 MGS adapted this to emc2
*
*   Derived from a work by Tom Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2007 All rights reserved.
*
* Last change:
********************************************************************/

#include <saicanon.hh>
#include "emctool.h"

/* SAI tool table API (sai_tooltable.cc) */
extern "C" {
enum toolidx_t { IDX_OK = 0, IDX_NEW, IDX_FAIL };
toolidx_t tooldata_get(CANON_TOOL_TABLE *pdata, int idx);
toolidx_t tooldata_put(CANON_TOOL_TABLE tdata, int idx);
}

#include "rs274ngc.hh"
#include "rs274ngc_interp.hh"
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <cstdio>

#define CANON_API_CGO
#include "gomc/generated/gmi/canon/canon_api.h"
#undef CANON_API_CGO

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

StandaloneInterpInternals _sai = StandaloneInterpInternals();

char               _parameter_file_name[PARAMETER_FILE_NAME_LENGTH];

/* where to print */
FILE * _outfile=nullptr;      /* where to print, set in main */
static bool fo_enable=true, so_enable=true;

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

void print_nc_line_number()
{
  char text[256];
  int k;
  int m;

  if(!_outfile)
    {
      _outfile = stdout;
    }

  pinterp->line_text(text, 256);
  for (k = 0;
       ((k < 256) &&
        ((text[k] == '\t') || (text[k] == ' ') || (text[k] == '/')));
       k++);
  if ((k < 256) && ((text[k] == 'n') || (text[k] == 'N')))
    {
      fputc('N', _outfile);
      for (k++, m = 0;
           ((k < 256) && (text[k] >= '0') && (text[k] <= '9'));
           k++, m++)
        fputc(text[k], _outfile);
      for (; m < 6; m++)
        fputc(' ', _outfile);
    }
  else if (k < 256)
    fprintf(_outfile, "N..... ");
}

#define ECHO_WITH_ARGS(fmt, ...) PRINT("%s(" fmt ")\n", __FUNCTION__, ##__VA_ARGS__)

#define PRINT(control, ...) do \
{ \
    _outfile = _outfile ?: stdout; \
    fprintf(_outfile,  "%5d ", _sai._line_number++); \
    print_nc_line_number(); \
    fprintf(_outfile, control, ##__VA_ARGS__); \
} while (false)

/* Representation */

void SET_XY_ROTATION(double t) {
  ECHO_WITH_ARGS("%.4f", t);
}

void SET_G5X_OFFSET(int index,
                    double x, double y, double z,
                    double a, double b, double c,
                    double u, double v, double w) {

  ECHO_WITH_ARGS("%d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f",
          index, x, y, z, a, b, c);
  _sai._program_position_x = _sai._program_position_x + _sai._g5x_x - x;
  _sai._program_position_y = _sai._program_position_y + _sai._g5x_y - y;
  _sai._program_position_z = _sai._program_position_z + _sai._g5x_z - z;
  _sai._program_position_a = _sai._program_position_a + _sai._g5x_a - a;/*AA*/
  _sai._program_position_b = _sai._program_position_b + _sai._g5x_b - b;/*BB*/
  _sai._program_position_c = _sai._program_position_c + _sai._g5x_c - c;/*CC*/

  _sai._g5x_x = x;
  _sai._g5x_y = y;
  _sai._g5x_z = z;
  _sai._g5x_a = a;  /*AA*/
  _sai._g5x_b = b;  /*BB*/
  _sai._g5x_c = c;  /*CC*/
}

void SET_G92_OFFSET(double x, double y, double z,
                    double a, double b, double c,
                    double u, double v, double w) {
  ECHO_WITH_ARGS("%.4f, %.4f, %.4f, %.4f, %.4f, %.4f",
                      x, y, z, a, b, c);
  _sai._program_position_x = _sai._program_position_x + _sai._g92_x - x;
  _sai._program_position_y = _sai._program_position_y + _sai._g92_y - y;
  _sai._program_position_z = _sai._program_position_z + _sai._g92_z - z;
  _sai._program_position_a = _sai._program_position_a + _sai._g92_a - a;/*AA*/
  _sai._program_position_b = _sai._program_position_b + _sai._g92_b - b;/*BB*/
  _sai._program_position_c = _sai._program_position_c + _sai._g92_c - c;/*CC*/

  _sai._g92_x = x;
  _sai._g92_y = y;
  _sai._g92_z = z;
  _sai._g92_a = a;  /*AA*/
  _sai._g92_b = b;  /*BB*/
  _sai._g92_c = c;  /*CC*/
}

void USE_LENGTH_UNITS(CANON_UNITS in_unit)
{
  if (in_unit == CANON_UNITS_INCHES)
    {
      PRINT("USE_LENGTH_UNITS(CANON_UNITS_INCHES)\n");
      if (_sai._length_unit_type == CANON_UNITS_MM)
        {
          _sai._length_unit_type = CANON_UNITS_INCHES;
          _sai._length_unit_factor = 25.4;

          _sai._program_position_x /= 25.4;
          _sai._program_position_y /= 25.4;
          _sai._program_position_z /= 25.4;

          _sai._g5x_x /= 25.4;
          _sai._g5x_y /= 25.4;
          _sai._g5x_z /= 25.4;

          _sai._g92_x /= 25.4;
          _sai._g92_y /= 25.4;
          _sai._g92_z /= 25.4;
        }
    }
  else if (in_unit == CANON_UNITS_MM)
    {
      PRINT("USE_LENGTH_UNITS(CANON_UNITS_MM)\n");
      if (_sai._length_unit_type == CANON_UNITS_INCHES)
        {
          _sai._length_unit_type = CANON_UNITS_MM;
          _sai._length_unit_factor = 1.0;

          _sai._program_position_x *= 25.4;
          _sai._program_position_y *= 25.4;
          _sai._program_position_z *= 25.4;

          _sai._g5x_x *= 25.4;
          _sai._g5x_y *= 25.4;
          _sai._g5x_z *= 25.4;

          _sai._g92_x *= 25.4;
          _sai._g92_y *= 25.4;
          _sai._g92_z *= 25.4;
        }
    }
  else
    PRINT("USE_LENGTH_UNITS(UNKNOWN)\n");
}

/* Free Space Motion */
void SET_TRAVERSE_RATE(double rate)
{
  PRINT("SET_TRAVERSE_RATE(%.4f)\n", rate);
  _sai._traverse_rate = rate;
}

void STRAIGHT_TRAVERSE( int line_number,
 double x, double y, double z
 , double a /*AA*/
 , double b /*BB*/
 , double c /*CC*/
 , double u, double v, double w
)
{
  ECHO_WITH_ARGS("%.4f, %.4f, %.4f"
         ", %.4f" /*AA*/
         ", %.4f" /*BB*/
         ", %.4f" /*CC*/
         , x, y, z
         , a /*AA*/
         , b /*BB*/
         , c /*CC*/
         );
  _sai._program_position_x = x;
  _sai._program_position_y = y;
  _sai._program_position_z = z;
  _sai._program_position_a = a; /*AA*/
  _sai._program_position_b = b; /*BB*/
  _sai._program_position_c = c; /*CC*/
}

/* Machining Attributes */
void SET_FEED_MODE(int spindle, int mode)
{
  PRINT("SET_FEED_MODE(%d, %d)\n", spindle, mode);
  _sai._feed_mode = mode;
}
void SET_FEED_RATE(double rate)
{
  PRINT("SET_FEED_RATE(%.4f)\n", rate);
  _sai._feed_rate = rate;
}

void SET_FEED_REFERENCE(CANON_FEED_REFERENCE reference)
{
  PRINT("SET_FEED_REFERENCE(%s)\n",
         (reference == CANON_WORKPIECE) ? "CANON_WORKPIECE" : "CANON_XYZ");
}

extern void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode, double tolerance)
{
  _sai.motion_tolerance = 0;
  if (mode == CANON_EXACT_STOP)
    {
      PRINT("SET_MOTION_CONTROL_MODE(CANON_EXACT_STOP)\n");
      _sai._motion_mode = CANON_EXACT_STOP;
    }
  else if (mode == CANON_EXACT_PATH)
    {
      PRINT("SET_MOTION_CONTROL_MODE(CANON_EXACT_PATH)\n");
      _sai._motion_mode = CANON_EXACT_PATH;
    }
  else if (mode == CANON_CONTINUOUS)
    {
      _sai.motion_tolerance = tolerance;
      PRINT("SET_MOTION_CONTROL_MODE(CANON_CONTINUOUS, %f)\n", tolerance);
      _sai._motion_mode = CANON_CONTINUOUS;
    }
  else
    PRINT("SET_MOTION_CONTROL_MODE(UNKNOWN)\n");
}

extern void SET_NAIVECAM_TOLERANCE(double tolerance)
{
  _sai.naivecam_tolerance = tolerance;
  PRINT("SET_NAIVECAM_TOLERANCE(%.4f)\n", tolerance);
}

void SELECT_PLANE(CANON_PLANE in_plane)
{
  PRINT("SELECT_PLANE(CANON_PLANE_%s)\n",
         ((in_plane == CANON_PLANE_XY) ? "XY" :
          (in_plane == CANON_PLANE_YZ) ? "YZ" :
          (in_plane == CANON_PLANE_XZ) ? "XZ" : "UNKNOWN"));
  _sai._active_plane = in_plane;
}

void SET_CUTTER_RADIUS_COMPENSATION(double radius)
{PRINT("SET_CUTTER_RADIUS_COMPENSATION(%.4f)\n", radius);}

void START_CUTTER_RADIUS_COMPENSATION(int side)
{PRINT("START_CUTTER_RADIUS_COMPENSATION(%s)\n",
        (side == CANON_SIDE_LEFT)  ? "LEFT"  :
        (side == CANON_SIDE_RIGHT) ? "RIGHT" : "UNKNOWN");
}

void STOP_CUTTER_RADIUS_COMPENSATION()
{PRINT ("STOP_CUTTER_RADIUS_COMPENSATION()\n");}

void START_SPEED_FEED_SYNCH()
{PRINT ("START_SPEED_FEED_SYNCH()\n");}

void STOP_SPEED_FEED_SYNCH()
{PRINT ("STOP_SPEED_FEED_SYNCH()\n");}

/* Machining Functions */

void NURBS_FEED(int lineno,
std::vector<CONTROL_POINT> nurbs_control_points, unsigned int k)
{
  ECHO_WITH_ARGS("%lu, ...", (unsigned long)nurbs_control_points.size());

  _sai._program_position_x = nurbs_control_points[nurbs_control_points.size()].X;
  _sai._program_position_y = nurbs_control_points[nurbs_control_points.size()].Y;
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
  ECHO_WITH_ARGS("%.4f, %.4f, %.4f, %.4f, %d, %.4f"
         ", %.4f" /*AA*/
         ", %.4f" /*BB*/
         ", %.4f" /*CC*/
         , first_end, second_end, first_axis, second_axis,
         rotation, axis_end_point
         , a /*AA*/
         , b /*BB*/
         , c /*CC*/
         );
  if (_sai._active_plane == CANON_PLANE_XY)
    {
      _sai._program_position_x = first_end;
      _sai._program_position_y = second_end;
      _sai._program_position_z = axis_end_point;
    }
  else if (_sai._active_plane == CANON_PLANE_YZ)
    {
      _sai._program_position_x = axis_end_point;
      _sai._program_position_y = first_end;
      _sai._program_position_z = second_end;
    }
  else /* if (_active_plane == CANON_PLANE_XZ) */
    {
      _sai._program_position_x = second_end;
      _sai._program_position_y = axis_end_point;
      _sai._program_position_z = first_end;
    }
  _sai._program_position_a = a; /*AA*/
  _sai._program_position_b = b; /*BB*/
  _sai._program_position_c = c; /*CC*/
}

void STRAIGHT_FEED(int line_number,
 double x, double y, double z
 , double a /*AA*/
 , double b /*BB*/
 , double c /*CC*/
 , double u, double v, double w
)
{
  ECHO_WITH_ARGS("%.4f, %.4f, %.4f"
         ", %.4f" /*AA*/
         ", %.4f" /*BB*/
         ", %.4f" /*CC*/
         , x, y, z
         , a /*AA*/
         , b /*BB*/
         , c /*CC*/
         );
  _sai._program_position_x = x;
  _sai._program_position_y = y;
  _sai._program_position_z = z;
  _sai._program_position_a = a; /*AA*/
  _sai._program_position_b = b; /*BB*/
  _sai._program_position_c = c; /*CC*/
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
  double distance;
  double dx, dy, dz;
  double backoff;

  dx = (_sai._program_position_x - x);
  dy = (_sai._program_position_y - y);
  dz = (_sai._program_position_z - z);
  distance = sqrt((dx * dx) + (dy * dy) + (dz * dz));

  ECHO_WITH_ARGS("%.4f, %.4f, %.4f"
         ", %.4f" /*AA*/
         ", %.4f" /*BB*/
         ", %.4f" /*CC*/
         , x, y, z
         , a /*AA*/
         , b /*BB*/
         , c /*CC*/
         );
  _sai._probe_position_x = x;
  _sai._probe_position_y = y;
  _sai._probe_position_z = z;
  _sai._probe_position_a = a; /*AA*/
  _sai._probe_position_b = b; /*BB*/
  _sai._probe_position_c = c; /*CC*/
  if (distance != 0)
    {
      backoff = ((_sai._length_unit_type == CANON_UNITS_MM) ? 0.254 : 0.01);
      _sai._program_position_x = (x + (backoff * (dx / distance)));
      _sai._program_position_y = (y + (backoff * (dy / distance)));
      _sai._program_position_z = (z + (backoff * (dz / distance)));
    }
  _sai._program_position_a = a; /*AA*/
  _sai._program_position_b = b; /*BB*/
  _sai._program_position_c = c; /*CC*/
}


void RIGID_TAP(int line_number, double x, double y, double z, double scale)
{
    ECHO_WITH_ARGS("%.4f, %.4f, %.4f", x, y, z);
}


void DWELL(double seconds)
{
  ECHO_WITH_ARGS("%.4f", seconds);
}

/* Spindle Functions */
void SET_SPINDLE_MODE(int spindle, double arg) {
  PRINT("SET_SPINDLE_MODE(%d %.4f)\n", spindle, arg);
}

void START_SPINDLE_CLOCKWISE(int spindle, int wait_for_atspeed)
{
  PRINT("START_SPINDLE_CLOCKWISE(%i)\n", spindle);
  _sai._spindle_turning[spindle] = ((_sai._spindle_speed[spindle] == 0) ? CANON_STOPPED :
                                                   CANON_CLOCKWISE);
}

void START_SPINDLE_COUNTERCLOCKWISE(int spindle, int wait_for_atspeed)
{
  PRINT("START_SPINDLE_COUNTERCLOCKWISE(%i)\n", spindle);
  _sai._spindle_turning[spindle] = ((_sai._spindle_speed[spindle] == 0) ? CANON_STOPPED :
                                                   CANON_COUNTERCLOCKWISE);
}

void SET_SPINDLE_SPEED(int spindle, double rpm)
{
  PRINT("SET_SPINDLE_SPEED(%i, %.4f)\n", spindle, rpm);
  _sai._spindle_speed[spindle] = rpm;
}

void STOP_SPINDLE_TURNING(int spindle)
{
  PRINT("STOP_SPINDLE_TURNING(%i)\n", spindle);
  _sai._spindle_turning[spindle] = CANON_STOPPED;
}

void ORIENT_SPINDLE(int spindle, double orientation, int mode)
{PRINT("ORIENT_SPINDLE(%d, %.4f, %d)\n", spindle, orientation, mode);
}

void WAIT_SPINDLE_ORIENT_COMPLETE(int spindle, double timeout)
{
  PRINT("SPINDLE.%i.WAIT_ORIENT_COMPLETE(%.4f)\n", spindle, timeout);
}

/* Tool Functions */
void SET_TOOL_TABLE_ENTRY(int idx, int toolno, EmcPose offset, double diameter,
                          double frontangle, double backangle, int orientation) {

#ifdef TOOL_NML //{
    _sai._tools[idx].toolno = toolno;
    _sai._tools[idx].offset = offset;
    _sai._tools[idx].diameter = diameter;
    _sai._tools[idx].frontangle = frontangle;
    _sai._tools[idx].backangle = backangle;
    _sai._tools[idx].orientation = orientation;
#else //}{
    CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata,idx) != IDX_OK) {
        UNEXPECTED_MSG; 
    }
    tdata.toolno = toolno;
    tdata.offset = offset;
    tdata.diameter = diameter;
    tdata.frontangle = frontangle;
    tdata.backangle = backangle;
    tdata.orientation = orientation;
    if (tooldata_put(tdata,idx) == IDX_FAIL) {
        fprintf(stderr,"UNEXPECTED idx %s %d\n",__FILE__,__LINE__);
    }
#endif //}

    ECHO_WITH_ARGS("%d, %d, %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f, %.4f, %.4f, %d",
            idx, toolno,
            offset.tran.x, offset.tran.y, offset.tran.z, offset.a, offset.b, offset.c, offset.u, offset.v, offset.w,
            frontangle, backangle, orientation);
}

void USE_TOOL_LENGTH_OFFSET(EmcPose offset)
{
    _sai._tool_offset = offset;
    ECHO_WITH_ARGS("%.4f %.4f %.4f, %.4f %.4f %.4f, %.4f %.4f %.4f",
         offset.tran.x, offset.tran.y, offset.tran.z, offset.a, offset.b, offset.c, offset.u, offset.v, offset.w);
}

void CHANGE_TOOL(int slot)
{
  PRINT("CHANGE_TOOL(%d)\n", slot);
  _sai._active_slot = slot;
#ifdef TOOL_NML //{
  _sai._tools[0] = _sai._tools[slot];
#else //}{
    CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata,slot) != IDX_OK) {
        UNEXPECTED_MSG;
    }
    _sai._tools[0] = tdata;
    if (tooldata_put(tdata,0) == IDX_FAIL) {
        fprintf(stderr,"UNEXPECTED idx %s %d\n",__FILE__,__LINE__);
    }
#endif //}
}

void SELECT_TOOL(int tool)//TODO: fix slot number
{PRINT("SELECT_TOOL(%d)\n", tool);}

void CHANGE_TOOL_NUMBER(int tool)
{
  PRINT("CHANGE_TOOL_NUMBER(%d)\n", tool);
  _sai._active_slot = tool;
}

void RELOAD_TOOLDATA(void)
{
  PRINT("RELOAD_TOOLDATA()\n");
}


/* Misc Functions */

void CLAMP_AXIS(CANON_AXIS axis)
{PRINT("CLAMP_AXIS(%s)\n",
        (axis == CANON_AXIS_X) ? "CANON_AXIS_X" :
        (axis == CANON_AXIS_Y) ? "CANON_AXIS_Y" :
        (axis == CANON_AXIS_Z) ? "CANON_AXIS_Z" :
        (axis == CANON_AXIS_A) ? "CANON_AXIS_A" :
        (axis == CANON_AXIS_B) ? "CANON_AXIS_B" :
        (axis == CANON_AXIS_C) ? "CANON_AXIS_C" : "UNKNOWN");}

void COMMENT(const char *s)
{PRINT("COMMENT(\"%s\")\n", s);}

void DISABLE_ADAPTIVE_FEED()
{PRINT("DISABLE_ADAPTIVE_FEED()\n");}

void DISABLE_FEED_HOLD()
{PRINT("DISABLE_FEED_HOLD()\n");}

void DISABLE_FEED_OVERRIDE()
{PRINT("DISABLE_FEED_OVERRIDE()\n"); fo_enable = false; }

void DISABLE_SPEED_OVERRIDE(int spindle)
{PRINT("DISABLE_SPEED_OVERRIDE(%i)\n", spindle); so_enable = false; }

void ENABLE_ADAPTIVE_FEED()
{PRINT("ENABLE_ADAPTIVE_FEED()\n");}

void ENABLE_FEED_HOLD()
{PRINT("ENABLE_FEED_HOLD()\n");}

void ENABLE_FEED_OVERRIDE()
{PRINT("ENABLE_FEED_OVERRIDE()\n"); fo_enable = true; }

void ENABLE_SPEED_OVERRIDE(int spindle)
{PRINT("ENABLE_SPEED_OVERRIDE(%i)\n", spindle); so_enable = true; }

void FLOOD_OFF()
{
  PRINT("FLOOD_OFF()\n");
  _sai._flood = 0;
}

void FLOOD_ON()
{
  PRINT("FLOOD_ON()\n");
  _sai._flood = 1;
}

void INIT_CANON()
{
}

void MESSAGE(char *s)
{PRINT("MESSAGE(\"%s\")\n", s);}

void LOG(char *s)
{PRINT("LOG(\"%s\")\n", s);}
void LOGOPEN(char *s)
{PRINT("LOGOPEN(\"%s\")\n", s);}
void LOGAPPEND(char *s)
{PRINT("LOGAPPEND(\"%s\")\n", s);}
void LOGCLOSE()
{PRINT("LOGCLOSE()\n");}

void MIST_OFF()
{
  PRINT("MIST_OFF()\n");
  _sai._mist = 0;
}

void MIST_ON()
{
  PRINT("MIST_ON()\n");
  _sai._mist = 1;
}

void PALLET_SHUTTLE()
{PRINT("PALLET_SHUTTLE()\n");}

void TURN_PROBE_OFF()
{PRINT("TURN_PROBE_OFF()\n");}

void TURN_PROBE_ON()
{PRINT("TURN_PROBE_ON()\n");}

void UNCLAMP_AXIS(CANON_AXIS axis)
{PRINT("UNCLAMP_AXIS(%s)\n",
        (axis == CANON_AXIS_X) ? "CANON_AXIS_X" :
        (axis == CANON_AXIS_Y) ? "CANON_AXIS_Y" :
        (axis == CANON_AXIS_Z) ? "CANON_AXIS_Z" :
        (axis == CANON_AXIS_A) ? "CANON_AXIS_A" :
        (axis == CANON_AXIS_B) ? "CANON_AXIS_B" :
        (axis == CANON_AXIS_C) ? "CANON_AXIS_C" : "UNKNOWN");}

/* Program Functions */

void PROGRAM_STOP()
{PRINT("PROGRAM_STOP()\n");}

void SET_BLOCK_DELETE(bool state)
{_sai.block_delete = state;} //state == ON, means we don't interpret lines starting with "/"

bool GET_BLOCK_DELETE()
{return _sai.block_delete;} //state == ON, means we  don't interpret lines starting with "/"

void SET_OPTIONAL_PROGRAM_STOP(bool state)
{_sai.optional_program_stop = state;} //state == ON, means we stop

bool GET_OPTIONAL_PROGRAM_STOP()
{return _sai.optional_program_stop;} //state == ON, means we stop

void OPTIONAL_PROGRAM_STOP()
{PRINT("OPTIONAL_PROGRAM_STOP()\n");}

void PROGRAM_END()
{PRINT("PROGRAM_END()\n");}


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
int GET_EXTERNAL_FEED_OVERRIDE_ENABLE() {return fo_enable;}
double GET_EXTERNAL_MOTION_CONTROL_TOLERANCE() { return _sai.motion_tolerance;}
double GET_EXTERNAL_MOTION_CONTROL_NAIVECAM_TOLERANCE()
                                        { return _sai.naivecam_tolerance; }
double GET_EXTERNAL_LENGTH_UNITS() {return _sai._external_length_units;}
int GET_EXTERNAL_FEED_HOLD_ENABLE() {return 1;}
int GET_EXTERNAL_AXIS_MASK() {return 0x3f;} // XYZABC machine
double GET_EXTERNAL_ANGLE_UNITS() {return 1.0;}
int GET_EXTERNAL_SELECTED_TOOL_SLOT() { return 0; }
int GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE(int spindle) {return so_enable;}
void START_SPEED_FEED_SYNCH(int spindle, double sync, bool vel)
{PRINT("START_SPEED_FEED_SYNC(%f,%d)\n", sync, vel);}
CANON_MOTION_MODE motion_mode;

int GET_EXTERNAL_DIGITAL_INPUT(int index, int def) { return def; }
double GET_EXTERNAL_ANALOG_INPUT(int index, double def) { return def; }
int WAIT(int index, int input_type, int wait_type, double timeout) { return 0; }
int UNLOCK_ROTARY(int line_no, int joint_num) {return 0;}
int LOCK_ROTARY(int line_no, int joint_num) {return 0;}

/* Returns the system feed rate */
double GET_EXTERNAL_FEED_RATE()
{
  return _sai._feed_rate;
}

/* Returns the system flood coolant setting zero = off, non-zero = on */
int GET_EXTERNAL_FLOOD()
{
  return _sai._flood;
}

/* Returns the system length unit type */
CANON_UNITS GET_EXTERNAL_LENGTH_UNIT_TYPE()
{
  return _sai._length_unit_type;
}

/* Returns the system mist coolant setting zero = off, non-zero = on */
extern int GET_EXTERNAL_MIST()
{
  return _sai._mist;
}

// Returns the current motion control mode
extern CANON_MOTION_MODE GET_EXTERNAL_MOTION_CONTROL_MODE()
{
  return _sai._motion_mode;
}

extern void SET_PARAMETER_FILE_NAME(const char *name)
{
  strncpy(_parameter_file_name, name, PARAMETER_FILE_NAME_LENGTH - 1);
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

  if (strlen(_parameter_file_name) < (size_t)max_size)
    strcpy(file_name, _parameter_file_name);
  else
    file_name[0] = 0;
}

CANON_PLANE GET_EXTERNAL_PLANE()
{
  return _sai._active_plane;
}

/* returns the current a-axis position */
double GET_EXTERNAL_POSITION_A()
{
  return _sai._program_position_a;
}

/* returns the current b-axis position */
double GET_EXTERNAL_POSITION_B()
{
  return _sai._program_position_b;
}

/* returns the current c-axis position */
double GET_EXTERNAL_POSITION_C()
{
  return _sai._program_position_c;
}

/* returns the current x-axis position */
double GET_EXTERNAL_POSITION_X()
{
  return _sai._program_position_x;
}

/* returns the current y-axis position */
double GET_EXTERNAL_POSITION_Y()
{
  return _sai._program_position_y;
}

/* returns the current z-axis position */
double GET_EXTERNAL_POSITION_Z()
{
  return _sai._program_position_z;
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
  return _sai._probe_position_a;
}

/* returns the b-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_B()
{
  return _sai._probe_position_b;
}

/* returns the c-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_C()
{
  return _sai._probe_position_c;
}

/* returns the x-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_X()
{
  return _sai._probe_position_x;
}

/* returns the y-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_Y()
{
  return _sai._probe_position_y;
}

/* returns the z-axis position at the last probe trip. This is only valid
   once the probe command has executed to completion. */
double GET_EXTERNAL_PROBE_POSITION_Z()
{
  return _sai._probe_position_z;
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
double GET_EXTERNAL_SPEED(int spindle)
{
  return _sai._spindle_speed[spindle];
}

/* Returns the system value for direction of spindle turning */
extern CANON_DIRECTION GET_EXTERNAL_SPINDLE(int spindle)
{
  return _sai._spindle_turning[spindle];
}

/* Returns the system value for the carousel slot in which the tool
currently in the spindle belongs. Return value zero means there is no
tool in the spindle. */
extern int GET_EXTERNAL_TOOL_SLOT()
{
  return _sai._active_slot;
}

/* Returns the CANON_TOOL_TABLE structure associated with the tool
   in the given pocket */
extern CANON_TOOL_TABLE GET_EXTERNAL_TOOL_TABLE(int idx)
{
#ifdef TOOL_NML //{
  return _sai._tools[idx];
#else //}{
    CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata,idx) != IDX_OK) {
        UNEXPECTED_MSG;
    }
  return tdata;
#endif //}
}

/* Returns the system traverse rate */
double GET_EXTERNAL_TRAVERSE_RATE()
{
  return _sai._traverse_rate;
}



double GET_USER_DEFINED_RESULT()
{
  return 0.0;
}

void SET_MOTION_OUTPUT_BIT(int index)
{
    PRINT("SET_MOTION_OUTPUT_BIT(%d)\n", index);
    return;
}

void CLEAR_MOTION_OUTPUT_BIT(int index)
{
    PRINT("CLEAR_MOTION_OUTPUT_BIT(%d)\n", index);
    return;
}

void SET_MOTION_OUTPUT_VALUE(int index, double value)
{
    PRINT("SET_MOTION_OUTPUT_VALUE(%d,%f)\n", index, value);
    return;
}

void SET_AUX_OUTPUT_BIT(int index)
{
    PRINT("SET_AUX_OUTPUT_BIT(%d)\n", index);
    return;
}

void CLEAR_AUX_OUTPUT_BIT(int index)
{
    PRINT("CLEAR_AUX_OUTPUT_BIT(%d)\n", index);
    return;
}

void SET_AUX_OUTPUT_VALUE(int index, double value)
{
    PRINT("SET_AUX_OUTPUT_VALUE(%d,%f)\n", index, value);
    return;
}

double GET_EXTERNAL_TOOL_LENGTH_XOFFSET()
{
    return _sai._tool_offset.tran.x;
}

double GET_EXTERNAL_TOOL_LENGTH_YOFFSET()
{
    return _sai._tool_offset.tran.y;
}

double GET_EXTERNAL_TOOL_LENGTH_ZOFFSET()
{
    return _sai._tool_offset.tran.z;
}

double GET_EXTERNAL_TOOL_LENGTH_AOFFSET()
{
    return _sai._tool_offset.a;
}

double GET_EXTERNAL_TOOL_LENGTH_BOFFSET()
{
    return _sai._tool_offset.b;
}

double GET_EXTERNAL_TOOL_LENGTH_COFFSET()
{
    return _sai._tool_offset.c;
}

double GET_EXTERNAL_TOOL_LENGTH_UOFFSET()
{
    return _sai._tool_offset.u;
}

double GET_EXTERNAL_TOOL_LENGTH_VOFFSET()
{
    return _sai._tool_offset.v;
}

double GET_EXTERNAL_TOOL_LENGTH_WOFFSET()
{
    return _sai._tool_offset.w;
}

void FINISH(void) {
    PRINT("FINISH()\n");
}

void ON_RESET(void)
{
    PRINT("ON_RESET()\n");
}

void START_CHANGE(void) {
    PRINT("START_CHANGE()\n");
}


int GET_EXTERNAL_TC_FAULT()
{
    return _sai._toolchanger_fault;
}

int GET_EXTERNAL_TC_REASON()
{
    return _sai._toolchanger_reason;
}

int GET_EXTERNAL_OFFSET_APPLIED() {
    return 0;
};

EmcPose GET_EXTERNAL_OFFSETS() {
    EmcPose e;
    e.tran.x = 0;
    e.tran.y = 0;
    e.tran.z = 0;
    e.a      = 0;
    e.b      = 0;
    e.c      = 0;
    e.u      = 0;
    e.v      = 0;
    e.w      = 0;
    return e;
};

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
        PRINT("CANON_ERROR(%s)\n", err);
	    free(err);
	}
	else
	{
        PRINT("CANON_ERROR(vasprintf failed: %s)\n", strerror(errno));
	}
    }
}
void reset_internals()
{
  _sai = StandaloneInterpInternals();
}

StandaloneInterpInternals::StandaloneInterpInternals() :
  _active_plane(CANON_PLANE_XY),
  _active_slot(1),
  _feed_mode(0),
  _feed_rate(0.0),
  _flood(0),
  _length_unit_factor(1), /* 1 for MM 25.4 for inch */
  _length_unit_type(CANON_UNITS_MM),
  _line_number(1),
  _mist(0),
  _motion_mode(CANON_CONTINUOUS),
  _probe_position_a(0), /*AA*/
  _probe_position_b(0), /*BB*/
  _probe_position_c(0), /*CC*/
  _probe_position_x(0),
  _probe_position_y(0),
  _probe_position_z(0),
  _g5x_x(0),
  _g5x_y(0.0),
  _g5x_z(0.0),
  _g5x_a(0.0),
  _g5x_b(0.0),
  _g5x_c(0.0),
  _g92_x(0.0),
  _g92_y(0.0),
  _g92_z(0.0),
  _g92_a(0.0),
  _g92_b(0.0),
  _g92_c(0.0),
  _program_position_a(0), /*AA*/
  _program_position_b(0), /*BB*/
  _program_position_c(0), /*CC*/
  _program_position_x(0),
  _program_position_y(0),
  _program_position_z(0),
  _spindle_speed{0},
  _spindle_turning{CANON_STOPPED},
  _pockets_max(CANON_POCKETS_MAX),
  _tools{},
  /* optional program stop */
  optional_program_stop(ON), //set enabled by default (previous EMC behaviour)
  /* optional block delete */
  block_delete(ON), //set enabled by default (previous EMC behaviour)
  motion_tolerance(0.),
  naivecam_tolerance(0.),
  /* Dummy status variables */
  _traverse_rate(0.0),

  _tool_offset({}),
  _toolchanger_fault(false),
  _toolchanger_reason(0)
{
}
void UPDATE_TAG(StateTag tag){
    //Do nothing
}

// ---- SAI canon callback table ----
// Wraps the local SAI canon functions into the generated callback struct.

static void sc_init_canon(void *) { INIT_CANON(); }
static void sc_set_g5x_offset(void *ctx, int32_t origin, double x, double y, double z, double a, double b, double c, double u, double v, double w) { SET_G5X_OFFSET(origin, x, y, z, a, b, c, u, v, w); }
static void sc_set_g92_offset(void *ctx, double x, double y, double z, double a, double b, double c, double u, double v, double w) { SET_G92_OFFSET(x, y, z, a, b, c, u, v, w); }
static void sc_set_xy_rotation(void *ctx, double t) { SET_XY_ROTATION(t); }
static void sc_update_end_point(void *ctx, double x, double y, double z, double a, double b, double c, double u, double v, double w) {
    _sai._program_position_x = x;
    _sai._program_position_y = y;
    _sai._program_position_z = z;
    _sai._program_position_a = a;
    _sai._program_position_b = b;
    _sai._program_position_c = c;
}
static void sc_use_length_units(void *ctx, int32_t u) { USE_LENGTH_UNITS((CANON_UNITS)u); }
static void sc_select_plane(void *ctx, int32_t pl) { SELECT_PLANE((CANON_PLANE)pl); }
static void sc_set_traverse_rate(void *ctx, double rate) { SET_TRAVERSE_RATE(rate); }
static void sc_straight_traverse(void *ctx, int32_t ln, double x, double y, double z, double a, double b, double c, double u, double v, double w) { STRAIGHT_TRAVERSE(ln, x, y, z, a, b, c, u, v, w); }
static void sc_set_feed_rate(void *ctx, double rate) { SET_FEED_RATE(rate); }
static void sc_set_feed_reference(void *ctx, int32_t ref) { SET_FEED_REFERENCE((CANON_FEED_REFERENCE)ref); }
static void sc_set_feed_mode(void *ctx, int32_t spindle, int32_t mode) { SET_FEED_MODE(spindle, mode); }
static void sc_set_motion_control_mode(void *ctx, int32_t mode, double tol) { SET_MOTION_CONTROL_MODE((CANON_MOTION_MODE)mode, tol); }
static void sc_set_naivecam_tolerance(void *ctx, double tol) { SET_NAIVECAM_TOLERANCE(tol); }
static void sc_set_cutter_radius_compensation(void *ctx, double r) { SET_CUTTER_RADIUS_COMPENSATION(r); }
static void sc_start_cutter_radius_compensation(void *ctx, int32_t d) { START_CUTTER_RADIUS_COMPENSATION(d); }
static void sc_stop_cutter_radius_compensation(void *ctx) { STOP_CUTTER_RADIUS_COMPENSATION(); }
static void sc_start_speed_feed_synch(void *ctx, int32_t spindle, double sync, int32_t vel) { START_SPEED_FEED_SYNCH(spindle, sync, vel); }
static void sc_stop_speed_feed_synch(void *ctx) { STOP_SPEED_FEED_SYNCH(); }
static void sc_arc_feed(void *ctx, int32_t ln, double first_end, double second_end, double first_axis, double second_axis, int32_t rotation, double axis_end_point, double a, double b, double c, double u, double v, double w) { ARC_FEED(ln, first_end, second_end, first_axis, second_axis, rotation, axis_end_point, a, b, c, u, v, w); }
static void sc_straight_feed(void *ctx, int32_t ln, double x, double y, double z, double a, double b, double c, double u, double v, double w) { STRAIGHT_FEED(ln, x, y, z, a, b, c, u, v, w); }
static void sc_nurbs_feed(void *ctx, int32_t ln, const canon_control_point_t *pts, size_t npts, uint32_t k) {
    std::vector<CONTROL_POINT> cpts(npts);
    for (size_t i = 0; i < npts; i++) { cpts[i].X = pts[i].x; cpts[i].Y = pts[i].y; cpts[i].W = pts[i].w; }
    NURBS_FEED(ln, cpts, k);
}
static void sc_rigid_tap(void *ctx, int32_t ln, double x, double y, double z, double scale) { RIGID_TAP(ln, x, y, z, scale); }
static void sc_straight_probe(void *ctx, int32_t ln, double x, double y, double z, double a, double b, double c, double u, double v, double w, uint8_t pt) { STRAIGHT_PROBE(ln, x, y, z, a, b, c, u, v, w, pt); }
static void sc_stop(void *ctx) {}
static void sc_dwell(void *ctx, double s) { DWELL(s); }
static void sc_finish(void *ctx) { FINISH(); }
static void sc_set_spindle_mode(void *ctx, int32_t spindle, double m) { SET_SPINDLE_MODE(spindle, m); }
static void sc_start_spindle_clockwise(void *ctx, int32_t spindle, int32_t wait) { START_SPINDLE_CLOCKWISE(spindle, wait); }
static void sc_start_spindle_counterclockwise(void *ctx, int32_t spindle, int32_t wait) { START_SPINDLE_COUNTERCLOCKWISE(spindle, wait); }
static void sc_set_spindle_speed(void *ctx, int32_t spindle, double rpm) { SET_SPINDLE_SPEED(spindle, rpm); }
static void sc_stop_spindle_turning(void *ctx, int32_t spindle) { STOP_SPINDLE_TURNING(spindle); }
static void sc_orient_spindle(void *ctx, int32_t spindle, double d, int32_t i) { ORIENT_SPINDLE(spindle, d, i); }
static void sc_wait_spindle_orient_complete(void *ctx, int32_t s, double t) { WAIT_SPINDLE_ORIENT_COMPLETE(s, t); }
static void sc_select_tool(void *ctx, int32_t t) { SELECT_TOOL(t); }
static void sc_start_change(void *ctx) { START_CHANGE(); }
static void sc_change_tool(void *ctx, int32_t p) { CHANGE_TOOL(p); }
static void sc_change_tool_number(void *ctx, int32_t p) { CHANGE_TOOL_NUMBER(p); }
static void sc_reload_tooldata(void *ctx) { RELOAD_TOOLDATA(); }
static void sc_set_tool_table_entry(void *ctx, int32_t pocket, int32_t toolno, double ox, double oy, double oz, double oa, double ob, double oc, double ou, double ov, double ow, double diameter, double frontangle, double backangle, int32_t orient) {
    EmcPose offset;
    offset.tran.x = ox; offset.tran.y = oy; offset.tran.z = oz;
    offset.a = oa; offset.b = ob; offset.c = oc;
    offset.u = ou; offset.v = ov; offset.w = ow;
    SET_TOOL_TABLE_ENTRY(pocket, toolno, offset, diameter, frontangle, backangle, orient);
}
static void sc_use_tool_length_offset(void *ctx, double ox, double oy, double oz, double oa, double ob, double oc, double ou, double ov, double ow) {
    EmcPose offset;
    offset.tran.x = ox; offset.tran.y = oy; offset.tran.z = oz;
    offset.a = oa; offset.b = ob; offset.c = oc;
    offset.u = ou; offset.v = ov; offset.w = ow;
    USE_TOOL_LENGTH_OFFSET(offset);
}
static void sc_flood_on(void *ctx) { FLOOD_ON(); }
static void sc_flood_off(void *ctx) { FLOOD_OFF(); }
static void sc_mist_on(void *ctx) { MIST_ON(); }
static void sc_mist_off(void *ctx) { MIST_OFF(); }
static void sc_enable_feed_override(void *ctx) { ENABLE_FEED_OVERRIDE(); }
static void sc_disable_feed_override(void *ctx) { DISABLE_FEED_OVERRIDE(); }
static void sc_enable_speed_override(void *ctx, int32_t s) { ENABLE_SPEED_OVERRIDE(s); }
static void sc_disable_speed_override(void *ctx, int32_t s) { DISABLE_SPEED_OVERRIDE(s); }
static void sc_enable_feed_hold(void *ctx) { ENABLE_FEED_HOLD(); }
static void sc_disable_feed_hold(void *ctx) { DISABLE_FEED_HOLD(); }
static void sc_enable_adaptive_feed(void *ctx) { ENABLE_ADAPTIVE_FEED(); }
static void sc_disable_adaptive_feed(void *ctx) { DISABLE_ADAPTIVE_FEED(); }
static void sc_set_motion_output_bit(void *ctx, int32_t b) { SET_MOTION_OUTPUT_BIT(b); }
static void sc_clear_motion_output_bit(void *ctx, int32_t b) { CLEAR_MOTION_OUTPUT_BIT(b); }
static void sc_set_aux_output_bit(void *ctx, int32_t b) { SET_AUX_OUTPUT_BIT(b); }
static void sc_clear_aux_output_bit(void *ctx, int32_t b) { CLEAR_AUX_OUTPUT_BIT(b); }
static void sc_set_motion_output_value(void *ctx, int32_t i, double v) { SET_MOTION_OUTPUT_VALUE(i, v); }
static void sc_set_aux_output_value(void *ctx, int32_t i, double v) { SET_AUX_OUTPUT_VALUE(i, v); }
static int32_t sc_wait_input(void *ctx, int32_t index, int32_t input_type, int32_t wait_type, double timeout) { return WAIT(index, input_type, wait_type, timeout); }
static void sc_clamp_axis(void *ctx, int32_t a) { CLAMP_AXIS((CANON_AXIS)a); }
static void sc_unclamp_axis(void *ctx, int32_t a) { UNCLAMP_AXIS((CANON_AXIS)a); }
static int32_t sc_lock_rotary(void *ctx, int32_t ln, int32_t j) { return LOCK_ROTARY(ln, j); }
static int32_t sc_unlock_rotary(void *ctx, int32_t ln, int32_t j) { return UNLOCK_ROTARY(ln, j); }
static void sc_program_stop(void *ctx) { PROGRAM_STOP(); }
static void sc_optional_program_stop(void *ctx) { OPTIONAL_PROGRAM_STOP(); }
static void sc_program_end(void *ctx) { PROGRAM_END(); }
static void sc_pallet_shuttle(void *ctx) { PALLET_SHUTTLE(); }
static void sc_comment(void *ctx, const char *s) { COMMENT(s); }
static void sc_message(void *ctx, const char *s) { MESSAGE((char*)s); }
static void sc_log_msg(void *ctx, const char *s) { LOG((char*)s); }
static void sc_logopen(void *ctx, const char *s) { LOGOPEN((char*)s); }
static void sc_logappend(void *ctx, const char *s) { LOGAPPEND((char*)s); }
static void sc_logclose(void *ctx) { LOGCLOSE(); }
static void sc_canon_error(void *ctx, const char *msg) { CANON_ERROR("%s", msg); }
static void sc_turn_probe_on(void *ctx) { TURN_PROBE_ON(); }
static void sc_turn_probe_off(void *ctx) { TURN_PROBE_OFF(); }
static void sc_set_block_delete(void *ctx, int32_t e) { SET_BLOCK_DELETE(e); }
static int32_t sc_get_block_delete(void *ctx) { return GET_BLOCK_DELETE(); }
static void sc_set_optional_program_stop(void *ctx, int32_t e) { SET_OPTIONAL_PROGRAM_STOP(e); }
static int32_t sc_get_optional_program_stop(void *ctx) { return GET_OPTIONAL_PROGRAM_STOP(); }
static void sc_update_tag(void *ctx, uint64_t tag) { (void)tag; }
static void sc_set_parameter_file_name(void *ctx, const char *n) { SET_PARAMETER_FILE_NAME(n); }
static void sc_on_reset(void *ctx) { ON_RESET(); }
static double sc_get_user_defined_result(void *ctx) { return GET_USER_DEFINED_RESULT(); }
static double sc_get_external_feed_rate(void *ctx) { return GET_EXTERNAL_FEED_RATE(); }
static double sc_get_external_traverse_rate(void *ctx) { return GET_EXTERNAL_TRAVERSE_RATE(); }
static int32_t sc_get_external_length_unit_type(void *ctx) { return (int32_t)GET_EXTERNAL_LENGTH_UNIT_TYPE(); }
static double sc_get_external_length_units(void *ctx) { return GET_EXTERNAL_LENGTH_UNITS(); }
static double sc_get_external_angle_units(void *ctx) { return GET_EXTERNAL_ANGLE_UNITS(); }
static int32_t sc_get_external_motion_control_mode(void *ctx) { return (int32_t)GET_EXTERNAL_MOTION_CONTROL_MODE(); }
static double sc_get_external_motion_control_tolerance(void *ctx) { return GET_EXTERNAL_MOTION_CONTROL_TOLERANCE(); }
static double sc_get_external_motion_control_naivecam_tolerance(void *ctx) { return GET_EXTERNAL_MOTION_CONTROL_NAIVECAM_TOLERANCE(); }
static int32_t sc_get_external_flood(void *ctx) { return GET_EXTERNAL_FLOOD(); }
static int32_t sc_get_external_mist(void *ctx) { return GET_EXTERNAL_MIST(); }
static double sc_get_external_position_x(void *ctx) { return GET_EXTERNAL_POSITION_X(); }
static double sc_get_external_position_y(void *ctx) { return GET_EXTERNAL_POSITION_Y(); }
static double sc_get_external_position_z(void *ctx) { return GET_EXTERNAL_POSITION_Z(); }
static double sc_get_external_position_a(void *ctx) { return GET_EXTERNAL_POSITION_A(); }
static double sc_get_external_position_b(void *ctx) { return GET_EXTERNAL_POSITION_B(); }
static double sc_get_external_position_c(void *ctx) { return GET_EXTERNAL_POSITION_C(); }
static double sc_get_external_position_u(void *ctx) { return GET_EXTERNAL_POSITION_U(); }
static double sc_get_external_position_v(void *ctx) { return GET_EXTERNAL_POSITION_V(); }
static double sc_get_external_position_w(void *ctx) { return GET_EXTERNAL_POSITION_W(); }
static double sc_get_external_probe_position_x(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_X(); }
static double sc_get_external_probe_position_y(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_Y(); }
static double sc_get_external_probe_position_z(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_Z(); }
static double sc_get_external_probe_position_a(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_A(); }
static double sc_get_external_probe_position_b(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_B(); }
static double sc_get_external_probe_position_c(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_C(); }
static double sc_get_external_probe_position_u(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_U(); }
static double sc_get_external_probe_position_v(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_V(); }
static double sc_get_external_probe_position_w(void *ctx) { return GET_EXTERNAL_PROBE_POSITION_W(); }
static double sc_get_external_probe_value(void *ctx) { return GET_EXTERNAL_PROBE_VALUE(); }
static int32_t sc_get_external_probe_tripped_value(void *ctx) { return GET_EXTERNAL_PROBE_TRIPPED_VALUE(); }
static double sc_get_external_speed(void *ctx, int32_t s) { return GET_EXTERNAL_SPEED(s); }
static int32_t sc_get_external_spindle(void *ctx, int32_t s) { return (int32_t)GET_EXTERNAL_SPINDLE(s); }
static double sc_get_external_tool_length_xoffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_XOFFSET(); }
static double sc_get_external_tool_length_yoffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_YOFFSET(); }
static double sc_get_external_tool_length_zoffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_ZOFFSET(); }
static double sc_get_external_tool_length_aoffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_AOFFSET(); }
static double sc_get_external_tool_length_boffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_BOFFSET(); }
static double sc_get_external_tool_length_coffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_COFFSET(); }
static double sc_get_external_tool_length_uoffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_UOFFSET(); }
static double sc_get_external_tool_length_voffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_VOFFSET(); }
static double sc_get_external_tool_length_woffset(void *ctx) { return GET_EXTERNAL_TOOL_LENGTH_WOFFSET(); }
static int32_t sc_get_external_tool_slot(void *ctx) { return GET_EXTERNAL_TOOL_SLOT(); }
static int32_t sc_get_external_selected_tool_slot(void *ctx) { return GET_EXTERNAL_SELECTED_TOOL_SLOT(); }
static int32_t sc_get_external_tool_table(void *ctx, int32_t pocket, int32_t *toolno, double offset[9], double *diameter, double *frontangle, double *backangle, int32_t *orientation) {
    CANON_TOOL_TABLE t = GET_EXTERNAL_TOOL_TABLE(pocket);
    *toolno = t.toolno;
    offset[0] = t.offset.tran.x; offset[1] = t.offset.tran.y; offset[2] = t.offset.tran.z;
    offset[3] = t.offset.a; offset[4] = t.offset.b; offset[5] = t.offset.c;
    offset[6] = t.offset.u; offset[7] = t.offset.v; offset[8] = t.offset.w;
    *diameter = t.diameter; *frontangle = t.frontangle; *backangle = t.backangle;
    *orientation = t.orientation;
    return 0;
}
static int32_t sc_get_external_tc_fault(void *ctx) { return GET_EXTERNAL_TC_FAULT(); }
static int32_t sc_get_external_tc_reason(void *ctx) { return GET_EXTERNAL_TC_REASON(); }
static int32_t sc_get_external_queue_empty(void *ctx) { return GET_EXTERNAL_QUEUE_EMPTY(); }
static int32_t sc_get_external_axis_mask(void *ctx) { return GET_EXTERNAL_AXIS_MASK(); }
static int32_t sc_get_external_digital_input(void *ctx, int32_t index, int32_t def) { return GET_EXTERNAL_DIGITAL_INPUT(index, def); }
static double sc_get_external_analog_input(void *ctx, int32_t index, double def) { return GET_EXTERNAL_ANALOG_INPUT(index, def); }
static int32_t sc_get_external_feed_override_enable(void *ctx) { return GET_EXTERNAL_FEED_OVERRIDE_ENABLE(); }
static int32_t sc_get_external_spindle_override_enable(void *ctx, int32_t s) { return GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE(s); }
static int32_t sc_get_external_adaptive_feed_enable(void *ctx) { return GET_EXTERNAL_ADAPTIVE_FEED_ENABLE(); }
static int32_t sc_get_external_feed_hold_enable(void *ctx) { return GET_EXTERNAL_FEED_HOLD_ENABLE(); }
static int32_t sc_get_external_plane(void *ctx) { return (int32_t)GET_EXTERNAL_PLANE(); }
static void sc_get_external_parameter_file_name(void *ctx, const char **buf) {
    static char filename[PARAMETER_FILE_NAME_LENGTH];
    GET_EXTERNAL_PARAMETER_FILE_NAME(filename, sizeof(filename));
    *buf = filename;
}
static int32_t sc_get_external_offset_applied(void *ctx) { return GET_EXTERNAL_OFFSET_APPLIED(); }
static double sc_get_external_hal_value(void *ctx, const char *name, int32_t *found) {
    (void)ctx; (void)name; *found = 0; return 0.0;
}
static void sc_get_external_offsets(void *ctx, double offsets[9]) {
    EmcPose o = GET_EXTERNAL_OFFSETS();
    offsets[0] = o.tran.x; offsets[1] = o.tran.y; offsets[2] = o.tran.z;
    offsets[3] = o.a; offsets[4] = o.b; offsets[5] = o.c;
    offsets[6] = o.u; offsets[7] = o.v; offsets[8] = o.w;
}

static const canon_callbacks_t saicanon_table = {
    .init_canon = sc_init_canon,
    .set_g5x_offset = sc_set_g5x_offset,
    .set_g92_offset = sc_set_g92_offset,
    .set_xy_rotation = sc_set_xy_rotation,
    .update_end_point = sc_update_end_point,
    .use_length_units = sc_use_length_units,
    .select_plane = sc_select_plane,
    .set_traverse_rate = sc_set_traverse_rate,
    .straight_traverse = sc_straight_traverse,
    .set_feed_rate = sc_set_feed_rate,
    .set_feed_reference = sc_set_feed_reference,
    .set_feed_mode = sc_set_feed_mode,
    .set_motion_control_mode = sc_set_motion_control_mode,
    .set_naivecam_tolerance = sc_set_naivecam_tolerance,
    .set_cutter_radius_compensation = sc_set_cutter_radius_compensation,
    .start_cutter_radius_compensation = sc_start_cutter_radius_compensation,
    .stop_cutter_radius_compensation = sc_stop_cutter_radius_compensation,
    .start_speed_feed_synch = sc_start_speed_feed_synch,
    .stop_speed_feed_synch = sc_stop_speed_feed_synch,
    .arc_feed = sc_arc_feed,
    .straight_feed = sc_straight_feed,
    .nurbs_feed = sc_nurbs_feed,
    .rigid_tap = sc_rigid_tap,
    .straight_probe = sc_straight_probe,
    .stop = sc_stop,
    .dwell = sc_dwell,
    .finish = sc_finish,
    .set_spindle_mode = sc_set_spindle_mode,
    .start_spindle_clockwise = sc_start_spindle_clockwise,
    .start_spindle_counterclockwise = sc_start_spindle_counterclockwise,
    .set_spindle_speed = sc_set_spindle_speed,
    .stop_spindle_turning = sc_stop_spindle_turning,
    .orient_spindle = sc_orient_spindle,
    .wait_spindle_orient_complete = sc_wait_spindle_orient_complete,
    .select_tool = sc_select_tool,
    .start_change = sc_start_change,
    .change_tool = sc_change_tool,
    .change_tool_number = sc_change_tool_number,
    .reload_tooldata = sc_reload_tooldata,
    .set_tool_table_entry = sc_set_tool_table_entry,
    .use_tool_length_offset = sc_use_tool_length_offset,
    .flood_on = sc_flood_on,
    .flood_off = sc_flood_off,
    .mist_on = sc_mist_on,
    .mist_off = sc_mist_off,
    .enable_feed_override = sc_enable_feed_override,
    .disable_feed_override = sc_disable_feed_override,
    .enable_speed_override = sc_enable_speed_override,
    .disable_speed_override = sc_disable_speed_override,
    .enable_feed_hold = sc_enable_feed_hold,
    .disable_feed_hold = sc_disable_feed_hold,
    .enable_adaptive_feed = sc_enable_adaptive_feed,
    .disable_adaptive_feed = sc_disable_adaptive_feed,
    .set_motion_output_bit = sc_set_motion_output_bit,
    .clear_motion_output_bit = sc_clear_motion_output_bit,
    .set_aux_output_bit = sc_set_aux_output_bit,
    .clear_aux_output_bit = sc_clear_aux_output_bit,
    .set_motion_output_value = sc_set_motion_output_value,
    .set_aux_output_value = sc_set_aux_output_value,
    .wait_input = sc_wait_input,
    .clamp_axis = sc_clamp_axis,
    .unclamp_axis = sc_unclamp_axis,
    .lock_rotary = sc_lock_rotary,
    .unlock_rotary = sc_unlock_rotary,
    .program_stop = sc_program_stop,
    .optional_program_stop = sc_optional_program_stop,
    .program_end = sc_program_end,
    .pallet_shuttle = sc_pallet_shuttle,
    .comment = sc_comment,
    .message = sc_message,
    .log_msg = sc_log_msg,
    .logopen = sc_logopen,
    .logappend = sc_logappend,
    .logclose = sc_logclose,
    .canon_error = sc_canon_error,
    .turn_probe_on = sc_turn_probe_on,
    .turn_probe_off = sc_turn_probe_off,
    .set_block_delete = sc_set_block_delete,
    .get_block_delete = sc_get_block_delete,
    .set_optional_program_stop = sc_set_optional_program_stop,
    .get_optional_program_stop = sc_get_optional_program_stop,
    .update_tag = sc_update_tag,
    .set_parameter_file_name = sc_set_parameter_file_name,
    .on_reset = sc_on_reset,
    .get_user_defined_result = sc_get_user_defined_result,
    .get_external_feed_rate = sc_get_external_feed_rate,
    .get_external_traverse_rate = sc_get_external_traverse_rate,
    .get_external_length_unit_type = sc_get_external_length_unit_type,
    .get_external_length_units = sc_get_external_length_units,
    .get_external_angle_units = sc_get_external_angle_units,
    .get_external_motion_control_mode = sc_get_external_motion_control_mode,
    .get_external_motion_control_tolerance = sc_get_external_motion_control_tolerance,
    .get_external_motion_control_naivecam_tolerance = sc_get_external_motion_control_naivecam_tolerance,
    .get_external_flood = sc_get_external_flood,
    .get_external_mist = sc_get_external_mist,
    .get_external_position_x = sc_get_external_position_x,
    .get_external_position_y = sc_get_external_position_y,
    .get_external_position_z = sc_get_external_position_z,
    .get_external_position_a = sc_get_external_position_a,
    .get_external_position_b = sc_get_external_position_b,
    .get_external_position_c = sc_get_external_position_c,
    .get_external_position_u = sc_get_external_position_u,
    .get_external_position_v = sc_get_external_position_v,
    .get_external_position_w = sc_get_external_position_w,
    .get_external_probe_position_x = sc_get_external_probe_position_x,
    .get_external_probe_position_y = sc_get_external_probe_position_y,
    .get_external_probe_position_z = sc_get_external_probe_position_z,
    .get_external_probe_position_a = sc_get_external_probe_position_a,
    .get_external_probe_position_b = sc_get_external_probe_position_b,
    .get_external_probe_position_c = sc_get_external_probe_position_c,
    .get_external_probe_position_u = sc_get_external_probe_position_u,
    .get_external_probe_position_v = sc_get_external_probe_position_v,
    .get_external_probe_position_w = sc_get_external_probe_position_w,
    .get_external_probe_value = sc_get_external_probe_value,
    .get_external_probe_tripped_value = sc_get_external_probe_tripped_value,
    .get_external_speed = sc_get_external_speed,
    .get_external_spindle = sc_get_external_spindle,
    .get_external_tool_length_xoffset = sc_get_external_tool_length_xoffset,
    .get_external_tool_length_yoffset = sc_get_external_tool_length_yoffset,
    .get_external_tool_length_zoffset = sc_get_external_tool_length_zoffset,
    .get_external_tool_length_aoffset = sc_get_external_tool_length_aoffset,
    .get_external_tool_length_boffset = sc_get_external_tool_length_boffset,
    .get_external_tool_length_coffset = sc_get_external_tool_length_coffset,
    .get_external_tool_length_uoffset = sc_get_external_tool_length_uoffset,
    .get_external_tool_length_voffset = sc_get_external_tool_length_voffset,
    .get_external_tool_length_woffset = sc_get_external_tool_length_woffset,
    .get_external_tool_slot = sc_get_external_tool_slot,
    .get_external_selected_tool_slot = sc_get_external_selected_tool_slot,
    .get_external_tool_table = sc_get_external_tool_table,
    .get_external_tc_fault = sc_get_external_tc_fault,
    .get_external_tc_reason = sc_get_external_tc_reason,
    .get_external_queue_empty = sc_get_external_queue_empty,
    .get_external_axis_mask = sc_get_external_axis_mask,
    .get_external_digital_input = sc_get_external_digital_input,
    .get_external_analog_input = sc_get_external_analog_input,
    .get_external_feed_override_enable = sc_get_external_feed_override_enable,
    .get_external_spindle_override_enable = sc_get_external_spindle_override_enable,
    .get_external_adaptive_feed_enable = sc_get_external_adaptive_feed_enable,
    .get_external_feed_hold_enable = sc_get_external_feed_hold_enable,
    .get_external_plane = sc_get_external_plane,
    .get_external_parameter_file_name = sc_get_external_parameter_file_name,
    .get_external_offset_applied = sc_get_external_offset_applied,
    .get_external_offsets = sc_get_external_offsets,
    .get_external_hal_value = sc_get_external_hal_value,
};

const canon_callbacks_t *saicanon_get_callbacks(void) {
    return &saicanon_table;
}
