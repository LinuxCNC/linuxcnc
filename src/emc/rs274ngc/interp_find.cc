/********************************************************************
* Description: interp_find.cc
*
*   Derived from a work by Thomas Kramer
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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"

/****************************************************************************/

/*! find_arc_length

Returned Value: double (length of path between start and end points)

Side effects: none

Called by:
   inverse_time_rate_arc
   inverse_time_rate_arc2
   inverse_time_rate_as

This calculates the length of the path that will be made relative to
the XYZ axes for a motion in which the X,Y,Z, motion is a circular or
helical arc with its axis parallel to the Z-axis. If tool length
compensation is on, this is the path of the tool tip; if off, the
length of the path of the spindle tip. Any rotary axis motion is
ignored.

If the arc is helical, it is coincident with the hypotenuse of a right
triangle wrapped around a cylinder. If the triangle is unwrapped, its
base is [the radius of the cylinder times the number of radians in the
helix] and its height is [z2 - z1], and the path length can be found
by the Pythagorean theorem.

This is written as though it is only for arcs whose axis is parallel to
the Z-axis, but it will serve also for arcs whose axis is parallel
to the X-axis or Y-axis, with suitable permutation of the arguments.

This works correctly when turn is zero (find_turn returns 0 in that
case).

*/

double Interp::find_arc_length(double x1,        //!< X-coordinate of start point       
                              double y1,        //!< Y-coordinate of start point       
                              double z1,        //!< Z-coordinate of start point       
                              double center_x,  //!< X-coordinate of arc center        
                              double center_y,  //!< Y-coordinate of arc center        
                              int turn, //!< no. of full or partial circles CCW
                              double x2,        //!< X-coordinate of end point         
                              double y2,        //!< Y-coordinate of end point         
                              double z2)        //!< Z-coordinate of end point         
{
  double radius;
  double theta;                 /* amount of turn of arc in radians */

  radius = hypot((center_x - x1), (center_y - y1));
  theta = find_turn(x1, y1, center_x, center_y, turn, x2, y2);
  if (z2 == z1)
    return (radius * fabs(theta));
  else
    return hypot((radius * theta), (z2 - z1));
}

/****************************************************************************/

/*! find_ends

Returned Value: int (INTERP_OK)

Side effects:
   The values of px, py, pz, aa_p, bb_p, and cc_p are set

Called by:
   convert_arc
   convert_home
   convert_probe
   convert_straight

This finds the coordinates of a point, "end", in the currently
active coordinate system, and sets the values of the pointers to the
coordinates (which are the arguments to the function).

In all cases, if no value for the coodinate is given in the block, the
current value for the coordinate is used. When cutter radius
compensation is on, this function is called before compensation
calculations are performed, so the current value of the programmed
point is used, not the current value of the actual current_point.

There are three cases for when the coordinate is included in the block:

1. G_53 is active. This means to interpret the coordinates as machine
coordinates. That is accomplished by adding the two offsets to the
coordinate given in the block.

2. Absolute coordinate mode is in effect. The coordinate in the block
is used.

3. Incremental coordinate mode is in effect. The coordinate in the
block plus either (i) the programmed current position - when cutter
radius compensation is in progress, or (2) the actual current position.

*/

int Interp::find_ends(block_pointer block,       //!< pointer to a block of RS274/NGC instructions
                     setup_pointer settings,    //!< pointer to machine settings                 
                     double *px,        //!< pointer to end_x                            
                     double *py,        //!< pointer to end_y                            
                     double *pz,        //!< pointer to end_z                            
                     double *AA_p,      //!< pointer to end_a                      
                     double *BB_p,      //!< pointer to end_b                      
                     double *CC_p)      //!< pointer to end_c                      
{
  int mode;
  int middle;
  int comp;

  mode = settings->distance_mode;
  middle = (settings->program_x != UNKNOWN);
  comp = (settings->cutter_comp_side != OFF);

  if (block->g_modes[0] == G_53) {      /* distance mode is absolute in this case */
#ifdef DEBUG_EMC
    COMMENT("interpreter: offsets temporarily suspended");
#endif
    *px = (block->x_flag == ON) ? (block->x_number -
                                   (settings->origin_offset_x +
                                    settings->
                                    axis_offset_x)) : settings->current_x;
    *py =
      (block->y_flag ==
       ON) ? (block->y_number - (settings->origin_offset_y +
                                 settings->
                                 axis_offset_y)) : settings->current_y;
    *pz =
      (block->z_flag ==
       ON) ? (block->z_number - (settings->tool_length_offset +
                                 settings->origin_offset_z +
                                 settings->
                                 axis_offset_z)) : settings->current_z;
#ifndef LATHE
    *AA_p = (block->a_flag == ON) ? (block->a_number -
                                     (settings->AA_origin_offset +
                                      settings->
                                      AA_axis_offset)) : settings->AA_current;
    *BB_p =
      (block->b_flag ==
       ON) ? (block->b_number - (settings->BB_origin_offset +
                                 settings->
                                 BB_axis_offset)) : settings->BB_current;
    *CC_p =
      (block->c_flag ==
       ON) ? (block->c_number - (settings->CC_origin_offset +
                                 settings->
                                 CC_axis_offset)) : settings->CC_current;
#endif
  } else if (mode == MODE_ABSOLUTE) {
    *px = (block->x_flag == ON) ? block->x_number :
      (comp && middle) ? settings->program_x : settings->current_x;

    *py = (block->y_flag == ON) ? block->y_number :
      (comp && middle) ? settings->program_y : settings->current_y;

    *pz = (block->z_flag == ON) ? block->z_number : settings->current_z;
#ifndef LATHE
    *AA_p = (block->a_flag == ON) ? block->a_number : settings->AA_current;
    *BB_p = (block->b_flag == ON) ? block->b_number : settings->BB_current;
    *CC_p = (block->c_flag == ON) ? block->c_number : settings->CC_current;
#endif
  } else {                      /* mode is MODE_INCREMENTAL */

    *px = (block->x_flag == ON)
      ? ((comp
          && middle) ? (block->x_number +
                        settings->program_x) : (block->x_number +
                                                settings->current_x))
      : ((comp && middle) ? settings->program_x : settings->current_x);

    *py = (block->y_flag == ON)
      ? ((comp
          && middle) ? (block->y_number +
                        settings->program_y) : (block->y_number +
                                                settings->current_y))
      : ((comp && middle) ? settings->program_y : settings->current_y);

    *pz = (block->z_flag == ON) ?
      (settings->current_z + block->z_number) : settings->current_z;
#ifndef LATHE
    *AA_p = (block->a_flag == ON) ?
      (settings->AA_current + block->a_number) : settings->AA_current;
    *BB_p =
      (block->b_flag ==
       ON) ? (settings->BB_current + block->b_number) : settings->BB_current;
    *CC_p =
      (block->c_flag ==
       ON) ? (settings->CC_current + block->c_number) : settings->CC_current;
#endif
  }
  return INTERP_OK;
}

/****************************************************************************/

/*! find_relative

Returned Value: int (INTERP_OK)

Side effects:
   The values of x2, y2, z2, aa_2, bb_2, and cc_2 are set.
   (NOTE: aa_2 etc. are written with lower case letters in this
    documentation because upper case would confuse the pre-preprocessor.)

Called by:
   convert_home

This finds the coordinates in the current system, under the current
tool length offset, of a point (x1, y1, z1, aa_1, bb_1, cc_1) whose absolute
coordinates are known.

Don't confuse this with the inverse operation.

*/

int Interp::find_relative(double x1,     //!< absolute x position        
                         double y1,     //!< absolute y position        
                         double z1,     //!< absolute z position        
                         double AA_1,   //!< absolute a position         
                         double BB_1,   //!< absolute b position         
                         double CC_1,   //!< absolute c position         
                         double *x2,    //!< pointer to relative x      
                         double *y2,    //!< pointer to relative y      
                         double *z2,    //!< pointer to relative z      
                         double *AA_2,  //!< pointer to relative a       
                         double *BB_2,  //!< pointer to relative b       
                         double *CC_2,  //!< pointer to relative c       
                         setup_pointer settings)        //!< pointer to machine settings
{
  *x2 = (x1 - (settings->origin_offset_x + settings->axis_offset_x));
  *y2 = (y1 - (settings->origin_offset_y + settings->axis_offset_y));
  *z2 =
    (z1 -
     (settings->tool_length_offset + settings->origin_offset_z +
      settings->axis_offset_z));
#ifndef LATHE
  *AA_2 = (AA_1 - (settings->AA_origin_offset + settings->AA_axis_offset));
  *BB_2 = (BB_1 - (settings->BB_origin_offset + settings->BB_axis_offset));
  *CC_2 = (CC_1 - (settings->CC_origin_offset + settings->CC_axis_offset));
#endif
  return INTERP_OK;
}

/****************************************************************************/

/*! find_straight_length

Returned Value: double (length of path between start and end points)

Side effects: none

Called by:
  inverse_time_rate_straight
  inverse_time_rate_as

This calculates a number to use in feed rate calculations when inverse
time feed mode is used, for a motion in which X,Y,Z,A,B, and C each change
linearly or not at all from their initial value to their end value.

This is used when the feed_reference mode is CANON_XYZ, which is
always in rs274NGC.

If any of the X, Y, or Z axes move or the A-axis, B-axis, and C-axis
do not move, this is the length of the path relative to the XYZ axes
from the first point to the second, and any rotary axis motion is
ignored. The length is the simple Euclidean distance.

The formula for the Euclidean distance "length" of a move involving
only the A, B and C axes is based on a conversation with Jim Frohardt at
Boeing, who says that the Fanuc controller on their 5-axis machine
interprets the feed rate this way. Note that if only one rotary axis
moves, this formula returns the absolute value of that axis move,
which is what is desired.

*/

double Interp::find_straight_length(double x2,   //!< X-coordinate of end point   
                                   double y2,   //!< Y-coordinate of end point   
                                   double z2,   //!< Z-coordinate of end point   
                                   double AA_2, //!< A-coordinate of end point    
                                   double BB_2, //!< B-coordinate of end point    
                                   double CC_2, //!< C-coordinate of end point    
                                   double x1,   //!< X-coordinate of start point 
                                   double y1,   //!< Y-coordinate of start point 
                                   double z1,    //!< Z-coordinate of start point 
                                   double AA_1,        //!< A-coordinate of start point  
                                   double BB_1,        //!< B-coordinate of start point  
                                   double CC_1        //!< C-coordinate of start point  
  )
{
  if ((x1 != x2) || (y1 != y2) || (z1 != z2) || (1
#ifndef LATHE
                                                 && (AA_2 == AA_1)
                                                 && (BB_2 == BB_1)
                                                 && (CC_2 == CC_1)
#endif
      ))                        /* straight line */
    return sqrt(pow((x2 - x1), 2) + pow((y2 - y1), 2) + pow((z2 - z1), 2));
  else
    return sqrt(0 +
#ifndef LATHE
                pow((AA_2 - AA_1), 2) +
                pow((BB_2 - BB_1), 2) + pow((CC_2 - CC_1), 2) +
#endif
                0);
}

/****************************************************************************/

/*! find_turn

Returned Value: double (angle in radians between two radii of a circle)

Side effects: none

Called by: find_arc_length

All angles are in radians.

*/

double Interp::find_turn(double x1,      //!< X-coordinate of start point       
                        double y1,      //!< Y-coordinate of start point       
                        double center_x,        //!< X-coordinate of arc center        
                        double center_y,        //!< Y-coordinate of arc center        
                        int turn,       //!< no. of full or partial circles CCW
                        double x2,      //!< X-coordinate of end point         
                        double y2)      //!< Y-coordinate of end point         
{
  double alpha;                 /* angle of first radius */
  double beta;                  /* angle of second radius */
  double theta;                 /* amount of turn of arc CCW - negative if CW */

  if (turn == 0)
    return 0.0;
  alpha = atan2((y1 - center_y), (x1 - center_x));
  beta = atan2((y2 - center_y), (x2 - center_x));
  if (turn > 0) {
    if (beta <= alpha)
      beta = (beta + (2 * M_PIl));
    theta = ((beta - alpha) + ((turn - 1) * (2 * M_PIl)));
  } else {                      /* turn < 0 */

    if (alpha <= beta)
      alpha = (alpha + (2 * M_PIl));
    theta = ((beta - alpha) + ((turn + 1) * (2 * M_PIl)));
  }
  return (theta);
}
