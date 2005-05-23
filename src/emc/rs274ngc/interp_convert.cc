/********************************************************************
* Description: interp_convert.cc
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
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_internal.hh"

/****************************************************************************/

/*! convert_arc

Returned Value: int
   If one of the following functions returns an error code,
   this returns that error code.
      convert_arc_comp1
      convert_arc_comp2
      convert_arc2
   If any of the following errors occur, this returns the error code shown.
   Otherwise, this returns RS274NGC_OK.
   1. The block has neither an r value nor any i,j,k values:
      NCE_R_I_J_K_WORDS_ALL_MISSING_FOR_ARC
   2. The block has both an r value and one or more i,j,k values:
      NCE_MIXED_RADIUS_IJK_FORMAT_FOR_ARC
   3. In the ijk format the XY-plane is selected and
      the block has a k value: NCE_K_WORD_GIVEN_FOR_ARC_IN_XY_PLANE
   4. In the ijk format the YZ-plane is selected and
      the block has an i value: NCE_I_WORD_GIVEN_FOR_ARC_IN_YZ_PLANE
   5. In the ijk format the XZ-plane is selected and
      the block has a j value: NCE_J_WORD_GIVEN_FOR_ARC_IN_XZ_PLANE
   6. In either format any of the following occurs.
      a. The XY-plane is selected and the block has no x or y value:
         NCE_X_AND_Y_WORDS_MISSING_FOR_ARC_IN_XY_PLANE
      b. The YZ-plane is selected and the block has no y or z value:
         NCE_Y_AND_Z_WORDS_MISSING_FOR_ARC_IN_YZ_PLANE
      c. The ZX-plane is selected and the block has no z or x value:
         NCE_X_AND_Z_WORDS_MISSING_FOR_ARC_IN_XZ_PLANE
   7. The selected plane is an unknown plane:
      NCE_BUG_PLANE_NOT_XY_YZ__OR_XZ
   8. The feed rate mode is UNITS_PER_MINUTE and feed rate is zero:
      NCE_CANNOT_MAKE_ARC_WITH_ZERO_FEED_RATE
   9. The feed rate mode is INVERSE_TIME and the block has no f word:
      NCE_F_WORD_MISSING_WITH_INVERSE_TIME_ARC_MOVE

Side effects:
   This generates and executes an arc command at feed rate
   (and, possibly a second arc command). It also updates the setting
   of the position of the tool point to the end point of the move.

Called by: convert_motion.

This converts a helical or circular arc.  The function calls:
convert_arc2 (when cutter radius compensation is off) or
convert_arc_comp1 (when cutter comp is on and this is the first move) or
convert_arc_comp2 (when cutter comp is on and this is not the first move).

If the ijk format is used, at least one of the offsets in the current
plane must be given in the block; it is common but not required to
give both offsets. The offsets are always incremental [NCMS, page 21].

*/

int Interp::convert_arc(int move,        //!< either G_2 (cw arc) or G_3 (ccw arc)    
                       block_pointer block,     //!< pointer to a block of RS274 instructions
                       setup_pointer settings)  //!< pointer to machine settings             
{
  static char name[] = "convert_arc";
  int status;
  int first;                    /* flag set ON if this is first move after comp ON */
  int ijk_flag;                 /* flag set ON if any of i,j,k present in NC code  */
  double end_x;
  double end_y;
  double end_z;
#ifndef LATHE
  double AA_end;
  double BB_end;
  double CC_end;
#endif

  ijk_flag = ((block->i_flag || block->j_flag) || block->k_flag) ? ON : OFF;
  first = (settings->program_x == UNKNOWN);

  CHK(((block->r_flag != ON) && (ijk_flag != ON)),
      NCE_R_I_J_K_WORDS_ALL_MISSING_FOR_ARC);
  CHK(((block->r_flag == ON) && (ijk_flag == ON)),
      NCE_MIXED_RADIUS_IJK_FORMAT_FOR_ARC);
  if (settings->feed_mode == UNITS_PER_MINUTE) {
    CHK((settings->feed_rate == 0.0),
        NCE_CANNOT_MAKE_ARC_WITH_ZERO_FEED_RATE);
  } else if (settings->feed_mode == INVERSE_TIME) {
    CHK((block->f_number == -1.0),
        NCE_F_WORD_MISSING_WITH_INVERSE_TIME_ARC_MOVE);
  }
  if (ijk_flag) {
    if (settings->plane == CANON_PLANE_XY) {
      CHK((block->k_flag), NCE_K_WORD_GIVEN_FOR_ARC_IN_XY_PLANE);
      if (block->i_flag == OFF) /* i or j flag on to get here */
        block->i_number = 0.0;
      else if (block->j_flag == OFF)
        block->j_number = 0.0;
    } else if (settings->plane == CANON_PLANE_YZ) {
      CHK((block->i_flag), NCE_I_WORD_GIVEN_FOR_ARC_IN_YZ_PLANE);
      if (block->j_flag == OFF) /* j or k flag on to get here */
        block->j_number = 0.0;
      else if (block->k_flag == OFF)
        block->k_number = 0.0;
    } else if (settings->plane == CANON_PLANE_XZ) {
      CHK((block->j_flag), NCE_J_WORD_GIVEN_FOR_ARC_IN_XZ_PLANE);
      if (block->i_flag == OFF) /* i or k flag on to get here */
        block->i_number = 0.0;
      else if (block->k_flag == OFF)
        block->k_number = 0.0;
    } else
      ERM(NCE_BUG_PLANE_NOT_XY_YZ_OR_XZ);
  } else;                       /* r format arc; no other checks needed specific to this format */

  if (settings->plane == CANON_PLANE_XY) {      /* checks for both formats */
    CHK(((block->x_flag == OFF) && (block->y_flag == OFF)),
        NCE_X_AND_Y_WORDS_MISSING_FOR_ARC_IN_XY_PLANE);
  } else if (settings->plane == CANON_PLANE_YZ) {
    CHK(((block->y_flag == OFF) && (block->z_flag == OFF)),
        NCE_Y_AND_Z_WORDS_MISSING_FOR_ARC_IN_YZ_PLANE);
  } else if (settings->plane == CANON_PLANE_XZ) {
    CHK(((block->x_flag == OFF) && (block->z_flag == OFF)),
        NCE_X_AND_Z_WORDS_MISSING_FOR_ARC_IN_XZ_PLANE);
  }

  find_ends(block, settings, &end_x, &end_y, &end_z,
#ifndef LATHE
            &AA_end, &BB_end, &CC_end);
#else
            0, 0, 0);
#endif
  settings->motion_mode = move;

  if (settings->plane == CANON_PLANE_XY) {
    if ((settings->cutter_comp_side == OFF) ||
        (settings->cutter_comp_radius == 0.0)) {
      status =
        convert_arc2(move, block, settings,
                     &(settings->current_x), &(settings->current_y),
                     &(settings->current_z), end_x, end_y, end_z,
#ifndef LATHE
                     AA_end, BB_end, CC_end,
#else
                     0, 0, 0,
#endif
                     block->i_number, block->j_number);
      CHP(status);
    } else if (first) {
      status = convert_arc_comp1(move, block, settings, end_x, end_y, end_z,
#ifndef LATHE
                                 AA_end, BB_end, CC_end);
#else
                                 0, 0, 0);
#endif
      CHP(status);
    } else {
      status = convert_arc_comp2(move, block, settings, end_x, end_y, end_z,
#ifndef LATHE
                                 AA_end, BB_end, CC_end);
#else
                                 0, 0, 0);
#endif

      CHP(status);
    }
  } else if (settings->plane == CANON_PLANE_XZ) {
    status =
      convert_arc2(move, block, settings,
                   &(settings->current_z), &(settings->current_x),
                   &(settings->current_y), end_z, end_x, end_y,
#ifndef LATHE
                   AA_end, BB_end, CC_end,
#else
                   0, 0, 0,
#endif
                   block->k_number, block->i_number);
    CHP(status);
  } else if (settings->plane == CANON_PLANE_YZ) {
    status =
      convert_arc2(move, block, settings,
                   &(settings->current_y), &(settings->current_z),
                   &(settings->current_x), end_y, end_z, end_x,
#ifndef LATHE
                   AA_end, BB_end, CC_end,
#else
                   0, 0, 0,
#endif
                   block->j_number, block->k_number);
    CHP(status);
  } else
    ERM(NCE_BUG_PLANE_NOT_XY_YZ_OR_XZ);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_arc2

Returned Value: int
   If arc_data_ijk or arc_data_r returns an error code,
   this returns that code.
   Otherwise, it returns RS274NGC_OK.

Side effects:
   This executes an arc command at feed rate. It also updates the
   setting of the position of the tool point to the end point of the move.
   If inverse time feed rate is in effect, it also resets the feed rate.

Called by: convert_arc.

This converts a helical or circular arc.

*/

int Interp::convert_arc2(int move,       //!< either G_2 (cw arc) or G_3 (ccw arc)    
                        block_pointer block,    //!< pointer to a block of RS274 instructions
                        setup_pointer settings, //!< pointer to machine settings             
                        double *current1,       //!< pointer to current value of coordinate 1
                        double *current2,       //!< pointer to current value of coordinate 2
                        double *current3,       //!< pointer to current value of coordinate 3
                        double end1,    //!< coordinate 1 value at end of arc        
                        double end2,    //!< coordinate 2 value at end of arc        
                        double end3,    //!< coordinate 3 value at end of arc        
                        double AA_end,  //!< a-value at end of arc                   
                        double BB_end,  //!< b-value at end of arc                   
                        double CC_end,  //!< c-value at end of arc                   
                        double offset1, //!< offset of center from current1          
                        double offset2) //!< offset of center from current2          
{
  static char name[] = "convert_arc2";
  double center1;
  double center2;
  int status;                   /* status returned from CHP function call     */
  double tolerance;             /* tolerance for difference of radii          */
  int turn;                     /* number of full or partial turns CCW in arc */

  tolerance = (settings->length_units == CANON_UNITS_INCHES) ?
    TOLERANCE_INCH : TOLERANCE_MM;

  if (block->r_flag) {
    CHP(arc_data_r(move, *current1, *current2, end1, end2,
                   block->r_number, &center1, &center2, &turn));
  } else {
    CHP(arc_data_ijk(move, *current1, *current2, end1, end2, offset1,
                     offset2, &center1, &center2, &turn, tolerance));
  }

  if (settings->feed_mode == INVERSE_TIME)
    inverse_time_rate_arc(*current1, *current2, *current3, center1, center2,
                          turn, end1, end2, end3, block, settings);
  ARC_FEED(end1, end2, center1, center2, turn, end3,
#ifndef LATHE
           AA_end, BB_end, CC_end);
#else
           0, 0, 0);
#endif
  *current1 = end1;
  *current2 = end2;
  *current3 = end3;
#ifndef LATHE
  settings->AA_current = AA_end;
  settings->BB_current = BB_end;
  settings->CC_current = CC_end;
#endif
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_arc_comp1

Returned Value: int
   If arc_data_comp_ijk or arc_data_comp_r returns an error code,
   this returns that code.
   Otherwise, it returns RS274NGC_OK.

Side effects:
   This executes an arc command at
   feed rate. It also updates the setting of the position of
   the tool point to the end point of the move.

Called by: convert_arc.

This function converts a helical or circular arc, generating only one
arc. The axis must be parallel to the z-axis. This is called when
cutter radius compensation is on and this is the first cut after the
turning on.

The arc which is generated is derived from a second arc which passes
through the programmed end point and is tangent to the cutter at its
current location. The generated arc moves the tool so that it stays
tangent to the second arc throughout the move.

*/

int Interp::convert_arc_comp1(int move,  //!< either G_2 (cw arc) or G_3 (ccw arc)            
                             block_pointer block,       //!< pointer to a block of RS274/NGC instructions    
                             setup_pointer settings,    //!< pointer to machine settings                     
                             double end_x,      //!< x-value at end of programmed (then actual) arc  
                             double end_y,      //!< y-value at end of programmed (then actual) arc  
                             double end_z,      //!< z-value at end of arc                           
                             double AA_end,     //!< a-value at end of arc                     
                             double BB_end,     //!< b-value at end of arc                     
                             double CC_end)     //!< c-value at end of arc                     
{
  static char name[] = "convert_arc_comp1";
  double center_x;
  double center_y;
  double gamma;                 /* direction of perpendicular to arc at end */
  int side;                     /* offset side - right or left              */
  int status;                   /* status returned from CHP function call   */
  double tolerance;             /* tolerance for difference of radii        */
  double tool_radius;
  int turn;                     /* 1 for counterclockwise, -1 for clockwise */

  side = settings->cutter_comp_side;
  tool_radius = settings->cutter_comp_radius;   /* always is positive */
  tolerance = (settings->length_units == CANON_UNITS_INCHES) ?
    TOLERANCE_INCH : TOLERANCE_MM;

  CHK((hypot((end_x - settings->current_x),
             (end_y - settings->current_y)) <= tool_radius),
      NCE_CUTTER_GOUGING_WITH_CUTTER_RADIUS_COMP);

  if (block->r_flag) {
    CHP(arc_data_comp_r(move, side, tool_radius, settings->current_x,
                        settings->current_y, end_x, end_y, block->r_number,
                        &center_x, &center_y, &turn));
  } else {
    CHP(arc_data_comp_ijk(move, side, tool_radius, settings->current_x,
                          settings->current_y, end_x, end_y,
                          block->i_number, block->j_number,
                          &center_x, &center_y, &turn, tolerance));
  }

  gamma =
    (((side == LEFT) && (move == G_3)) ||
     ((side == RIGHT) && (move == G_2))) ?
    atan2((center_y - end_y), (center_x - end_x)) :
    atan2((end_y - center_y), (end_x - center_x));

  settings->program_x = end_x;
  settings->program_y = end_y;
  end_x = (end_x + (tool_radius * cos(gamma))); /* end_x reset actual */
  end_y = (end_y + (tool_radius * sin(gamma))); /* end_y reset actual */

  if (settings->feed_mode == INVERSE_TIME)
    inverse_time_rate_arc(settings->current_x, settings->current_y,
                          settings->current_z, center_x, center_y, turn,
                          end_x, end_y, end_z, block, settings);
  ARC_FEED(end_x, end_y, center_x, center_y, turn, end_z,
#ifndef LATHE
           AA_end, BB_end, CC_end);
#else
           0, 0, 0);
#endif
  settings->current_x = end_x;
  settings->current_y = end_y;
  settings->current_z = end_z;
#ifndef LATHE
  settings->AA_current = AA_end;
  settings->BB_current = BB_end;
  settings->CC_current = CC_end;
#endif

  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_arc_comp2

Returned Value: int
   If arc_data_ijk or arc_data_r returns an error code,
   this returns that code.
   If any of the following errors occurs, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. A concave corner is found: NCE_CONCAVE_CORNER_WITH_CUTTER_RADIUS_COMP
   2. The tool will not fit inside an arc:
      NCE_TOOL_RADIUS_NOT_LESS_THAN_ARC_RADIUS_WITH_COMP

Side effects:
   This executes an arc command feed rate. If needed, at also generates
   an arc to go around a convex corner. It also updates the setting of
   the position of the tool point to the end point of the move. If
   inverse time feed rate mode is in effect, the feed rate is reset.

Called by: convert_arc.

This function converts a helical or circular arc. The axis must be
parallel to the z-axis. This is called when cutter radius compensation
is on and this is not the first cut after the turning on.

If one or more rotary axes is moved in this block and an extra arc is
required to go around a sharp corner, all the rotary axis motion
occurs on the main arc and none on the extra arc.  An alternative
might be to distribute the rotary axis motion over the extra arc and
the programmed arc in proportion to their lengths.

If the Z-axis is moved in this block and an extra arc is required to
go around a sharp corner, all the Z-axis motion occurs on the main arc
and none on the extra arc.  An alternative might be to distribute the
Z-axis motion over the extra arc and the main arc in proportion to
their lengths.

*/

int Interp::convert_arc_comp2(int move,  //!< either G_2 (cw arc) or G_3 (ccw arc)          
                             block_pointer block,       //!< pointer to a block of RS274/NGC instructions  
                             setup_pointer settings,    //!< pointer to machine settings                   
                             double end_x,      //!< x-value at end of programmed (then actual) arc
                             double end_y,      //!< y-value at end of programmed (then actual) arc
                             double end_z,      //!< z-value at end of arc                         
                             double AA_end,     //!< a-value at end of arc
                             double BB_end,     //!< b-value at end of arc
                             double CC_end)     //!< c-value at end of arc
{
  static char name[] = "convert_arc_comp2";
  double alpha;                 /* direction of tangent to start of arc */
  double arc_radius;
  double beta;                  /* angle between two tangents above */
  double center_x;              /* center of arc */
  double center_y;
  double delta;                 /* direction of radius from start of arc to center of arc */
  double gamma;                 /* direction of perpendicular to arc at end */
  double mid_x;
  double mid_y;
  int side;
  double small = TOLERANCE_CONCAVE_CORNER;      /* angle for testing corners */
  double start_x;
  double start_y;
  int status;                   /* status returned from CHP function call     */
  double theta;                 /* direction of tangent to last cut */
  double tolerance;
  double tool_radius;
  int turn;                     /* number of full or partial circles CCW */

/* find basic arc data: center_x, center_y, and turn */

  start_x = settings->program_x;
  start_y = settings->program_y;
  tolerance = (settings->length_units == CANON_UNITS_INCHES) ?
    TOLERANCE_INCH : TOLERANCE_MM;

  if (block->r_flag) {
    CHP(arc_data_r(move, start_x, start_y, end_x, end_y,
                   block->r_number, &center_x, &center_y, &turn));
  } else {
    CHP(arc_data_ijk(move, start_x, start_y, end_x, end_y,
                     block->i_number, block->j_number,
                     &center_x, &center_y, &turn, tolerance));
  }

/* compute other data */
  side = settings->cutter_comp_side;
  tool_radius = settings->cutter_comp_radius;   /* always is positive */
  arc_radius = hypot((center_x - end_x), (center_y - end_y));
  theta = atan2(settings->current_y - start_y, settings->current_x - start_x);
  theta = (side == LEFT) ? (theta - M_PI_2l) : (theta + M_PI_2l);
  delta = atan2(center_y - start_y, center_x - start_x);
  alpha = (move == G_3) ? (delta - M_PI_2l) : (delta + M_PI_2l);
  beta = (side == LEFT) ? (theta - alpha) : (alpha - theta);
  beta = (beta > (1.5 * M_PIl)) ? (beta - (2 * M_PIl)) :
    (beta < -M_PI_2l) ? (beta + (2 * M_PIl)) : beta;

  if (((side == LEFT) && (move == G_3)) || ((side == RIGHT) && (move == G_2))) {
    gamma = atan2((center_y - end_y), (center_x - end_x));
    CHK((arc_radius <= tool_radius),
        NCE_TOOL_RADIUS_NOT_LESS_THAN_ARC_RADIUS_WITH_COMP);
  } else {
    gamma = atan2((end_y - center_y), (end_x - center_x));
    delta = (delta + M_PIl);
  }

  settings->program_x = end_x;
  settings->program_y = end_y;
  end_x = (end_x + (tool_radius * cos(gamma))); /* end_x reset actual */
  end_y = (end_y + (tool_radius * sin(gamma))); /* end_y reset actual */

/* check if extra arc needed and insert if so */

  CHK(((beta < -small) || (beta > (M_PIl + small))),
      NCE_CONCAVE_CORNER_WITH_CUTTER_RADIUS_COMP);
  if (beta > small) {           /* two arcs needed */
    mid_x = (start_x + (tool_radius * cos(delta)));
    mid_y = (start_y + (tool_radius * sin(delta)));
    if (settings->feed_mode == INVERSE_TIME)
      inverse_time_rate_arc2(start_x, start_y, (side == LEFT) ? -1 : 1,
                             mid_x, mid_y, center_x, center_y, turn,
                             end_x, end_y, end_z, block, settings);
    ARC_FEED(mid_x, mid_y, start_x, start_y, ((side == LEFT) ? -1 : 1),
             settings->current_z,
#ifndef LATHE
             AA_end, BB_end, CC_end);
#else
             0, 0, 0);
#endif
    ARC_FEED(end_x, end_y, center_x, center_y, turn, end_z,
#ifndef LATHE
             AA_end, BB_end, CC_end);
#else
             0, 0, 0);
#endif
  } else {                      /* one arc needed */

    if (settings->feed_mode == INVERSE_TIME)
      inverse_time_rate_arc(settings->current_x, settings->current_y,
                            settings->current_z, center_x, center_y, turn,
                            end_x, end_y, end_z, block, settings);
    ARC_FEED(end_x, end_y, center_x, center_y, turn, end_z,
#ifndef LATHE
             AA_end, BB_end, CC_end);
#else
             0, 0, 0);
#endif
  }

  settings->current_x = end_x;
  settings->current_y = end_y;
  settings->current_z = end_z;
#ifndef LATHE
  settings->AA_current = AA_end;
  settings->BB_current = BB_end;
  settings->CC_current = CC_end;
#endif

  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_axis_offsets

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. The function is called when cutter radius compensation is on:
      NCE_CANNOT_CHANGE_AXIS_OFFSETS_WITH_CUTTER_RADIUS_COMP
   2. The g_code argument is not G_92, G_92_1, G_92_2, or G_92_3
      NCE_BUG_CODE_NOT_IN_G92_SERIES

Side effects:
   SET_PROGRAM_ORIGIN is called, and the coordinate
   values for the axis offsets are reset. The coordinates of the
   current point are reset. Parameters may be set.

Called by: convert_modal_0.

The action of G92 is described in [NCMS, pages 10 - 11] and {Fanuc,
pages 61 - 63]. [NCMS] is ambiguous about the intent, but [Fanuc]
is clear. When G92 is executed, an offset of the origin is calculated
so that the coordinates of the current point with respect to the moved
origin are as specified on the line containing the G92. If an axis
is not mentioned on the line, the coordinates of the current point
are not changed. The execution of G92 results in an axis offset being
calculated and saved for each of the six axes, and the axis offsets
are always used when motion is specified with respect to absolute
distance mode using any of the nine coordinate systems (those designated
by G54 - G59.3). Thus all nine coordinate systems are affected by G92.

Being in incremental distance mode has no effect on the action of G92
in this implementation. [NCMS] is not explicit about this, but it is
implicit in the second sentence of [Fanuc, page 61].

The offset is the amount the origin must be moved so that the
coordinate of the controlled point has the specified value. For
example, if the current point is at X=4 in the currently specified
coordinate system and the current X-axis offset is zero, then "G92 x7"
causes the X-axis offset to be reset to -3.

Since a non-zero offset may be already be in effect when the G92 is
called, that must be taken into account.

In addition to causing the axis offset values in the _setup model to be
set, G92 sets parameters 5211 to 5216 to the x,y,z,a,b,c axis offsets.

The action of G92.2 is described in [NCMS, page 12]. There is no
equivalent command in [Fanuc]. G92.2 resets axis offsets to zero.
G92.1, also included in [NCMS, page 12] (but the usage here differs
slightly from the spec), is like G92.2, except that it also causes
the axis offset parameters to be set to zero, whereas G92.2 does not
zero out the parameters.

G92.3 is not in [NCMS]. It sets the axis offset values to the values
given in the parameters.

*/

int Interp::convert_axis_offsets(int g_code,     //!< g_code being executed (must be in G_92 series)
                                block_pointer block,    //!< pointer to a block of RS274/NGC instructions  
                                setup_pointer settings) //!< pointer to machine settings                   
{
  static char name[] = "convert_axis_offsets";
  double *pars;                 /* short name for settings->parameters            */

  CHK((settings->cutter_comp_side != OFF),      /* not "== ON" */
      NCE_CANNOT_CHANGE_AXIS_OFFSETS_WITH_CUTTER_RADIUS_COMP);
  pars = settings->parameters;
  if (g_code == G_92) {
    if (block->x_flag == ON) {
      settings->axis_offset_x =
        (settings->current_x + settings->axis_offset_x - block->x_number);
      settings->current_x = block->x_number;
    }

    if (block->y_flag == ON) {
      settings->axis_offset_y =
        (settings->current_y + settings->axis_offset_y - block->y_number);
      settings->current_y = block->y_number;
    }

    if (block->z_flag == ON) {
      settings->axis_offset_z =
        (settings->current_z + settings->axis_offset_z - block->z_number);
      settings->current_z = block->z_number;
    }
#ifndef LATHE
    if (block->a_flag == ON) {
      settings->AA_axis_offset = (settings->AA_current +
                                  settings->AA_axis_offset - block->a_number);
      settings->AA_current = block->a_number;
    }
    if (block->b_flag == ON) {
      settings->BB_axis_offset = (settings->BB_current +
                                  settings->BB_axis_offset - block->b_number);
      settings->BB_current = block->b_number;
    }
    if (block->c_flag == ON) {
      settings->CC_axis_offset = (settings->CC_current +
                                  settings->CC_axis_offset - block->c_number);
      settings->CC_current = block->c_number;
    }
#endif

    SET_ORIGIN_OFFSETS(settings->origin_offset_x + settings->axis_offset_x,
                       settings->origin_offset_y + settings->axis_offset_y,
                       settings->origin_offset_z + settings->axis_offset_z,
#ifndef LATHE
                       (settings->AA_origin_offset +
                        settings->AA_axis_offset),
                       (settings->BB_origin_offset +
                        settings->BB_axis_offset),
                       (settings->CC_origin_offset +
                        settings->CC_axis_offset));
#else
                       0, 0, 0);
#endif
    pars[5211] = settings->axis_offset_x;
    pars[5212] = settings->axis_offset_y;
    pars[5213] = settings->axis_offset_z;
#ifndef LATHE
    pars[5214] = settings->AA_axis_offset;
    pars[5215] = settings->BB_axis_offset;
    pars[5216] = settings->CC_axis_offset;
#endif

  } else if ((g_code == G_92_1) || (g_code == G_92_2)) {
    settings->current_x = settings->current_x + settings->axis_offset_x;
    settings->current_y = settings->current_y + settings->axis_offset_y;
    settings->current_z = settings->current_z + settings->axis_offset_z;
#ifndef LATHE
    settings->AA_current = (settings->AA_current + settings->AA_axis_offset);
    settings->BB_current = (settings->BB_current + settings->BB_axis_offset);
    settings->CC_current = (settings->CC_current + settings->CC_axis_offset);
#endif
    SET_ORIGIN_OFFSETS(settings->origin_offset_x,
                       settings->origin_offset_y, settings->origin_offset_z,
#ifndef LATHE
                       settings->AA_origin_offset,
                       settings->BB_origin_offset,
                       settings->CC_origin_offset);
#else
                       0, 0, 0);
#endif

    settings->axis_offset_x = 0.0;
    settings->axis_offset_y = 0.0;
    settings->axis_offset_z = 0.0;
#ifndef LATHE
    settings->AA_axis_offset = 0.0;
    settings->BB_axis_offset = 0.0;
    settings->CC_axis_offset = 0.0;
#endif
    if (g_code == G_92_1) {
      pars[5211] = 0.0;
      pars[5212] = 0.0;
      pars[5213] = 0.0;
#ifndef LATHE
      pars[5214] = 0.0;
      pars[5215] = 0.0;
      pars[5216] = 0.0;
#endif
    }
  } else if (g_code == G_92_3) {
    settings->current_x =
      settings->current_x + settings->axis_offset_x - pars[5211];
    settings->current_y =
      settings->current_y + settings->axis_offset_y - pars[5212];
    settings->current_z =
      settings->current_z + settings->axis_offset_z - pars[5213];
#ifndef LATHE
    settings->AA_current =
      settings->AA_current + settings->AA_axis_offset - pars[5214];
    settings->BB_current =
      settings->BB_current + settings->BB_axis_offset - pars[5215];
    settings->CC_current =
      settings->CC_current + settings->CC_axis_offset - pars[5216];
#endif
    settings->axis_offset_x = pars[5211];
    settings->axis_offset_y = pars[5212];
    settings->axis_offset_z = pars[5213];
#ifndef LATHE
    settings->AA_axis_offset = pars[5214];
    settings->BB_axis_offset = pars[5215];
    settings->CC_axis_offset = pars[5216];
#endif
    SET_ORIGIN_OFFSETS(settings->origin_offset_x + settings->axis_offset_x,
                       settings->origin_offset_y + settings->axis_offset_y,
                       settings->origin_offset_z + settings->axis_offset_z,
#ifndef LATHE
                       (settings->AA_origin_offset +
                        settings->AA_axis_offset),
                       (settings->BB_origin_offset +
                        settings->BB_axis_offset),
                       (settings->CC_origin_offset +
                        settings->CC_axis_offset));
#else
                       0, 0, 0);
#endif

  } else
    ERM(NCE_BUG_CODE_NOT_IN_G92_SERIES);

  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_comment

Returned Value: int (RS274NGC_OK)

Side effects:
   The message function is called if the string starts with "MSG,".
   Otherwise, the comment function is called.

Called by: execute_block

To be a message, the first four characters of the comment after the
opening left parenthesis must be "MSG,", ignoring the case of the
letters and allowing spaces or tabs anywhere before the comma (to make
the treatment of case and white space consistent with how it is
handled elsewhere).

Messages are not provided for in [NCMS]. They are implemented here as a
subtype of comment. This is an extension to the rs274NGC language.

*/

int Interp::convert_comment(char *comment)       //!< string with comment
{
  enum
  { LC_SIZE = 256 };            // 256 from comment[256] in rs274ngc.hh
  char lc[LC_SIZE];
  char MSG_STR[] = "msg,";
  char SYSTEM_STR[] = "system,";
  int m, n, start;
  int item;

  // step over leading white space in comment
  m = 0;
  while (isspace(comment[m]))
    m++;
  start = m;
  // copy lowercase comment to lc[]
  for (n = 0; n < LC_SIZE && comment[m] != 0; m++, n++) {
    lc[n] = tolower(comment[m]);
  }
  lc[n] = 0;                    // null terminate

  // compare with MSG, SYSTEM
  if (!strncmp(lc, MSG_STR, strlen(MSG_STR))) {
    MESSAGE(comment + start + strlen(MSG_STR));
    return RS274NGC_OK;
  } else if (!strncmp(lc, SYSTEM_STR, strlen(SYSTEM_STR))) {
#if 0
/* FIX-ME Impliment these at a later stage... */
    SYSTEM(comment + start + strlen(SYSTEM_STR));
    return RS274NGC_EXECUTE_FINISH;     // inhibit read-ahead until this is done
#endif
  }
  // else it's a real comment
  COMMENT(comment + start);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_control_mode

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. g_code isn't G_61, G_61_1 or G_64: NCE_BUG_CODE_NOT_G61_G61_1_OR_G64

Side effects: See below

Called by: convert_g.

The interpreter switches the machine settings to indicate the
control mode (CANON_EXACT_STOP, CANON_EXACT_PATH or CANON_CONTINUOUS).

A call is made to SET_MOTION_CONTROL_MODE(CANON_XXX), where CANON_XXX is
CANON_EXACT_PATH if g_code is G_61, CANON_EXACT_STOP if g_code is G_61_1,
and CANON_CONTINUOUS if g_code is G_64.

Setting the control mode to CANON_EXACT_STOP on G_61 would correspond
more closely to the meaning of G_61 as given in [NCMS, page 40], but
CANON_EXACT_PATH has the advantage that the tool does not stop if it
does not have to, and no evident disadvantage compared to
CANON_EXACT_STOP, so it is being used for G_61. G_61_1 is not defined
in [NCMS], so it is available and is used here for setting the control
mode to CANON_EXACT_STOP.

It is OK to call SET_MOTION_CONTROL_MODE(CANON_XXX) when CANON_XXX is
already in force.

*/

int Interp::convert_control_mode(int g_code,     //!< g_code being executed (G_61, G61_1, || G_64)
                                setup_pointer settings) //!< pointer to machine settings                 
{
  static char name[] = "convert_control_mode";
  if (g_code == G_61) {
    SET_MOTION_CONTROL_MODE(CANON_EXACT_PATH);
    settings->control_mode = CANON_EXACT_PATH;
  } else if (g_code == G_61_1) {
    SET_MOTION_CONTROL_MODE(CANON_EXACT_STOP);
    settings->control_mode = CANON_EXACT_STOP;
  } else if (g_code == G_64) {
    SET_MOTION_CONTROL_MODE(CANON_CONTINUOUS);
    settings->control_mode = CANON_CONTINUOUS;
  } else
    ERM(NCE_BUG_CODE_NOT_G61_G61_1_OR_G64);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_coordinate_system

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. The value of the g_code argument is not 540, 550, 560, 570, 580, 590
      591, 592, or 593:
      NCE_BUG_CODE_NOT_IN_RANGE_G54_TO_G593

Side effects:
   If the coordinate system selected by the g_code is not already in
   use, the canonical program coordinate system axis offset values are
   reset and the coordinate values of the current point are reset.

Called by: convert_g.

COORDINATE SYSTEMS (involves g10, g53, g54 - g59.3, g92)

The canonical machining functions view of coordinate systems is:
1. There are two coordinate systems: absolute and program.
2. All coordinate values are given in terms of the program coordinate system.
3. The offsets of the program coordinate system may be reset.

The RS274/NGC view of coordinate systems, as given in section 3.2
of [NCMS] is:
1. there are ten coordinate systems: absolute and 9 program. The
   program coordinate systems are numbered 1 to 9.
2. you can switch among the 9 but not to the absolute one. G54
   selects coordinate system 1, G55 selects 2, and so on through
   G56, G57, G58, G59, G59.1, G59.2, and G59.3.
3. you can set the offsets of the 9 program coordinate systems
   using G10 L2 Pn (n is the number of the coordinate system) with
   values for the axes in terms of the absolute coordinate system.
4. the first one of the 9 program coordinate systems is the default.
5. data for coordinate systems is stored in parameters [NCMS, pages 59 - 60].
6. g53 means to interpret coordinate values in terms of the absolute
   coordinate system for the one block in which g53 appears.
7. You can offset the current coordinate system using g92. This offset
   will then apply to all nine program coordinate systems.

The approach used in the interpreter mates the canonical and NGC views
of coordinate systems as follows:

During initialization, data from the parameters for the first NGC
coordinate system is used in a SET_ORIGIN_OFFSETS function call and
origin_index in the machine model is set to 1.

If a g_code in the range g54 - g59.3 is encountered in an NC program,
the data from the appropriate NGC coordinate system is copied into the
origin offsets used by the interpreter, a SET_ORIGIN_OFFSETS function
call is made, and the current position is reset.

If a g10 is encountered, the convert_setup function is called to reset
the offsets of the program coordinate system indicated by the P number
given in the same block.

If a g53 is encountered, the axis values given in that block are used
to calculate what the coordinates are of that point in the current
coordinate system, and a STRAIGHT_TRAVERSE or STRAIGHT_FEED function
call to that point using the calculated values is made. No offset
values are changed.

If a g92 is encountered, that is handled by the convert_axis_offsets
function. A g92 results in an axis offset for each axis being calculated
and stored in the machine model. The axis offsets are applied to all
nine coordinate systems. Axis offsets are initialized to zero.

*/

int Interp::convert_coordinate_system(int g_code,        //!< g_code called (must be one listed above)     
                                     setup_pointer settings)    //!< pointer to machine settings                  
{
  static char name[] = "convert_coordinate_system";
  int origin;
  double x;
  double y;
  double z;
#ifndef LATHE
  double a;
  double b;
  double c;
#endif
  double *parameters;

  parameters = settings->parameters;
  switch (g_code) {
  case 540:
    origin = 1;
    break;
  case 550:
    origin = 2;
    break;
  case 560:
    origin = 3;
    break;
  case 570:
    origin = 4;
    break;
  case 580:
    origin = 5;
    break;
  case 590:
    origin = 6;
    break;
  case 591:
    origin = 7;
    break;
  case 592:
    origin = 8;
    break;
  case 593:
    origin = 9;
    break;
  default:
    ERM(NCE_BUG_CODE_NOT_IN_RANGE_G54_TO_G593);
  }

  if (origin == settings->origin_index) {       /* already using this origin */
#ifdef DEBUG_EMC
    COMMENT("interpreter: continuing to use same coordinate system");
#endif
    return RS274NGC_OK;
  }

  settings->origin_index = origin;
  parameters[5220] = (double) origin;

/* axis offsets could be included in the two set of calculations for
   current_x, current_y, etc., but do not need to be because the results
   would be the same. They would be added in then subtracted out. */
  settings->current_x = (settings->current_x + settings->origin_offset_x);
  settings->current_y = (settings->current_y + settings->origin_offset_y);
  settings->current_z = (settings->current_z + settings->origin_offset_z);
#ifndef LATHE
  settings->AA_current = (settings->AA_current + settings->AA_origin_offset);
  settings->BB_current = (settings->BB_current + settings->BB_origin_offset);
  settings->CC_current = (settings->CC_current + settings->CC_origin_offset);
#endif

  x = parameters[5201 + (origin * 20)];
  y = parameters[5202 + (origin * 20)];
  z = parameters[5203 + (origin * 20)];
#ifndef LATHE
  a = parameters[5204 + (origin * 20)];
  b = parameters[5205 + (origin * 20)];
  c = parameters[5206 + (origin * 20)];
#endif

  settings->origin_offset_x = x;
  settings->origin_offset_y = y;
  settings->origin_offset_z = z;
#ifndef LATHE
  settings->AA_origin_offset = a;
  settings->BB_origin_offset = b;
  settings->CC_origin_offset = c;
#endif

  settings->current_x = (settings->current_x - x);
  settings->current_y = (settings->current_y - y);
  settings->current_z = (settings->current_z - z);
#ifndef LATHE
  settings->AA_current = (settings->AA_current - a);
  settings->BB_current = (settings->BB_current - b);
  settings->CC_current = (settings->CC_current - c);
#endif

  SET_ORIGIN_OFFSETS(x + settings->axis_offset_x,
                     y + settings->axis_offset_y, z + settings->axis_offset_z,
#ifndef LATHE
                     a + settings->AA_axis_offset,
                     b + settings->BB_axis_offset,
                     c + settings->CC_axis_offset);
#else
                     0, 0, 0);
#endif
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_cutter_compensation

Returned Value: int
   If convert_cutter_compensation_on or convert_cutter_compensation_off
      is called and returns an error code, this returns that code.
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns RS274NGC_OK.
   1. g_code is not G_40, G_41, or G_42:
      NCE_BUG_CODE_NOT_G40_G41_OR_G42

Side effects:
   The value of cutter_comp_side in the machine model mode is
   set to RIGHT, LEFT, or OFF. The currently active tool table index in
   the machine model (which is the index of the slot whose diameter
   value is used in cutter radius compensation) is updated.

Since cutter radius compensation is performed in the interpreter, no
call is made to any canonical function regarding cutter radius compensation.

Called by: convert_g

*/

int Interp::convert_cutter_compensation(int g_code,      //!< must be G_40, G_41, or G_42             
                                       block_pointer block,     //!< pointer to a block of RS274 instructions
                                       setup_pointer settings)  //!< pointer to machine settings             
{
  static char name[] = "convert_cutter_compensation";
  int status;

  if (g_code == G_40) {
    CHP(convert_cutter_compensation_off(settings));
  } else if (g_code == G_41) {
    CHP(convert_cutter_compensation_on(LEFT, block, settings));
  } else if (g_code == G_42) {
    CHP(convert_cutter_compensation_on(RIGHT, block, settings));
  } else
    ERM(NCE_BUG_CODE_NOT_G40_G41_OR_G42);

  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_cutter_compensation_off

Returned Value: int (RS274NGC_OK)

Side effects:
   A comment is made that cutter radius compensation is turned off.
   The machine model of the cutter radius compensation mode is set to OFF.
   The value of program_x in the machine model is set to UNKNOWN.
     This serves as a flag when cutter radius compensation is
     turned on again.

Called by: convert_cutter_compensation

*/

int Interp::convert_cutter_compensation_off(setup_pointer settings)      //!< pointer to machine settings
{
#ifdef DEBUG_EMC
  COMMENT("interpreter: cutter radius compensation off");
#endif
  settings->cutter_comp_side = OFF;
  settings->program_x = UNKNOWN;
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_cutter_compensation_on

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. The selected plane is not the XY plane:
      NCE_CANNOT_TURN_CUTTER_RADIUS_COMP_ON_OUT_OF_XY_PLANE
   2. Cutter radius compensation is already on:
      NCE_CANNOT_TURN_CUTTER_RADIUS_COMP_ON_WHEN_ON

Side effects:
   A COMMENT function call is made (conditionally) saying that the
   interpreter is switching mode so that cutter radius compensation is on.
   The value of cutter_comp_radius in the machine model mode is
   set to the absolute value of the radius given in the tool table.
   The value of cutter_comp_side in the machine model mode is
   set to RIGHT or LEFT. The currently active tool table index in
   the machine model is updated.

Called by: convert_cutter_compensation

check_other_codes checks that a d word occurs only in a block with g41
or g42.

Cutter radius compensation is carried out in the interpreter, so no
call is made to a canonical function (although there is a canonical
function, START_CUTTER_RADIUS_COMPENSATION, that could be called if
the primitive level could execute it).

This version uses a D word if there is one in the block, but it does
not require a D word, since the sample programs which the interpreter
is supposed to handle do not have them.  Logically, the D word is
optional, since the D word is always (except in cases we have never
heard of) the slot number of the tool in the spindle. Not requiring a
D word is contrary to [Fanuc, page 116] and [NCMS, page 79], however.
Both manuals require the use of the D-word with G41 and G42.

This version handles a negative offset radius, which may be
encountered if the programmed tool path is a center line path for
cutting a profile and the path was constructed using a nominal tool
diameter. Then the value in the tool table for the diameter is set to
be the difference between the actual diameter and the nominal
diameter. If the actual diameter is less than the nominal, the value
in the table is negative. The method of handling a negative radius is
to switch the side of the offset and use a positive radius. This
requires that the profile use arcs (not straight lines) to go around
convex corners.

*/

int Interp::convert_cutter_compensation_on(int side,     //!< side of path cutter is on (LEFT or RIGHT)
                                          block_pointer block,  //!< pointer to a block of RS274 instructions 
                                          setup_pointer settings)       //!< pointer to machine settings              
{
  static char name[] = "convert_cutter_compensation_on";
  double radius;
  int index;

  CHK((settings->plane != CANON_PLANE_XY),
      NCE_CANNOT_TURN_CUTTER_RADIUS_COMP_ON_OUT_OF_XY_PLANE);
  CHK((settings->cutter_comp_side != OFF),
      NCE_CANNOT_TURN_CUTTER_RADIUS_COMP_ON_WHEN_ON);
  index = (block->d_number != -1) ? block->d_number : settings->current_slot;
  radius = ((settings->tool_table[index].diameter) / 2.0);

  if (radius < 0.0) {           /* switch side & make radius positive if radius negative */
    radius = -radius;
    if (side == RIGHT)
      side = LEFT;
    else
      side = RIGHT;
  }
#ifdef DEBUG_EMC
  if (side == RIGHT)
    COMMENT("interpreter: cutter radius compensation on right");
  else
    COMMENT("interpreter: cutter radius compensation on left");
#endif

  settings->cutter_comp_radius = radius;
  settings->tool_table_index = index;
  settings->cutter_comp_side = side;
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_distance_mode

Returned Value: int
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns RS274NGC_OK.
   1. g_code isn't G_90 or G_91: NCE_BUG_CODE_NOT_G90_OR_G91

Side effects:
   The interpreter switches the machine settings to indicate the current
   distance mode (absolute or incremental).

   The canonical machine to which commands are being sent does not have
   an incremental mode, so no command setting the distance mode is
   generated in this function. A comment function call explaining the
   change of mode is made (conditionally), however, if there is a change.

Called by: convert_g.

*/

int Interp::convert_distance_mode(int g_code,    //!< g_code being executed (must be G_90 or G_91)
                                 setup_pointer settings)        //!< pointer to machine settings                 
{
  static char name[] = "convert_distance_mode";
  if (g_code == G_90) {
    if (settings->distance_mode != MODE_ABSOLUTE) {
#ifdef DEBUG_EMC
      COMMENT("interpreter: distance mode changed to absolute");
#endif
      settings->distance_mode = MODE_ABSOLUTE;
    }
  } else if (g_code == G_91) {
    if (settings->distance_mode != MODE_INCREMENTAL) {
#ifdef DEBUG_EMC
      COMMENT("interpreter: distance mode changed to incremental");
#endif
      settings->distance_mode = MODE_INCREMENTAL;
    }
  } else
    ERM(NCE_BUG_CODE_NOT_G90_OR_G91);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_dwell

Returned Value: int (RS274NGC_OK)

Side effects:
   A dwell command is executed.

Called by: convert_g.

*/

int Interp::convert_dwell(double time)   //!< time in seconds to dwell  */
{
  DWELL(time);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_feed_mode

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1.  g_code isn't G_93 or G_94: NCE_BUG_CODE_NOT_G93_OR_G94

Side effects:
   The interpreter switches the machine settings to indicate the current
   feed mode (UNITS_PER_MINUTE or INVERSE_TIME).

   The canonical machine to which commands are being sent does not have
   a feed mode, so no command setting the distance mode is generated in
   this function. A comment function call is made (conditionally)
   explaining the change in mode, however.

Called by: execute_block.

*/

int Interp::convert_feed_mode(int g_code,        //!< g_code being executed (must be G_93 or G_94)
                             setup_pointer settings)    //!< pointer to machine settings                 
{
  static char name[] = "convert_feed_mode";
  if (g_code == G_93) {
#ifdef DEBUG_EMC
    COMMENT("interpreter: feed mode set to inverse time");
#endif
    settings->feed_mode = INVERSE_TIME;
  } else if (g_code == G_94) {
#ifdef DEBUG_EMC
    COMMENT("interpreter: feed mode set to units per minute");
#endif
    settings->feed_mode = UNITS_PER_MINUTE;
  } else
    ERM(NCE_BUG_CODE_NOT_G93_OR_G94);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_feed_rate

Returned Value: int (RS274NGC_OK)

Side effects:
   The machine feed_rate is set to the value of f_number in the
     block by function call.
   The machine model feed_rate is set to that value.

Called by: execute_block

This is called only if the feed mode is UNITS_PER_MINUTE.

*/

int Interp::convert_feed_rate(block_pointer block,       //!< pointer to a block of RS274 instructions
                             setup_pointer settings)    //!< pointer to machine settings             
{
  SET_FEED_RATE(block->f_number);
  settings->feed_rate = block->f_number;
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_g

Returned Value: int
   If one of the following functions is called and returns an error code,
   this returns that code.
      convert_control_mode
      convert_coordinate_system
      convert_cutter_compensation
      convert_distance_mode
      convert_dwell
      convert_length_units
      convert_modal_0
      convert_motion
      convert_retract_mode
      convert_set_plane
      convert_tool_length_offset
   Otherwise, it returns RS274NGC_OK.

Side effects:
   Any g_codes in the block (excluding g93 and 94) and any implicit
   motion g_code are executed.

Called by: execute_block.

This takes a pointer to a block of RS274/NGC instructions (already
read in) and creates the appropriate output commands corresponding to
any "g" codes in the block.

Codes g93 and g94, which set the feed mode, are executed earlier by
execute_block before reading the feed rate.

G codes are are executed in the following order.
1.  mode 0, G4 only - dwell. Left here from earlier versions.
2.  mode 2, one of (G17, G18, G19) - plane selection.
3.  mode 6, one of (G20, G21) - length units.
4.  mode 7, one of (G40, G41, G42) - cutter radius compensation.
5.  mode 8, one of (G43, G49) - tool length offset
6.  mode 12, one of (G54, G55, G56, G57, G58, G59, G59.1, G59.2, G59.3)
    - coordinate system selection.
7.  mode 13, one of (G61, G61.1, G64) - control mode
8.  mode 3, one of (G90, G91) - distance mode.
9.  mode 10, one of (G98, G99) - retract mode.
10. mode 0, one of (G10, G28, G30, G92, G92.1, G92.2, G92.3) -
    setting coordinate system locations, return to reference point 1,
    return to reference point 2, setting or cancelling axis offsets.
11. mode 1, one of (G0, G1, G2, G3, G38.2, G80, G81 to G89) - motion or cancel.
    G53 from mode 0 is also handled here, if present.

Some mode 0 and most mode 1 G codes must be executed after the length units
are set, since they use coordinate values. Mode 1 codes also must wait
until most of the other modes are set.

*/

int Interp::convert_g(block_pointer block,       //!< pointer to a block of RS274/NGC instructions
                     setup_pointer settings)    //!< pointer to machine settings                 
{
  static char name[] = "convert_g";
  int status;

  if (block->g_modes[0] == G_4) {
    CHP(convert_dwell(block->p_number));
  }
  if (block->g_modes[2] != -1) {
    CHP(convert_set_plane(block->g_modes[2], settings));
  }
  if (block->g_modes[6] != -1) {
    CHP(convert_length_units(block->g_modes[6], settings));
  }
  if (block->g_modes[7] != -1) {
    CHP(convert_cutter_compensation(block->g_modes[7], block, settings));
  }
  if (block->g_modes[8] != -1) {
    CHP(convert_tool_length_offset(block->g_modes[8], block, settings));
  }
  if (block->g_modes[12] != -1) {
    CHP(convert_coordinate_system(block->g_modes[12], settings));
  }
  if (block->g_modes[13] != -1) {
    CHP(convert_control_mode(block->g_modes[13], settings));
  }
  if (block->g_modes[3] != -1) {
    CHP(convert_distance_mode(block->g_modes[3], settings));
  }
  if (block->g_modes[10] != -1) {
    CHP(convert_retract_mode(block->g_modes[10], settings));
  }
  if (block->g_modes[0] != -1) {
    CHP(convert_modal_0(block->g_modes[0], block, settings));
  }
  if (block->motion_to_be != -1) {
    CHP(convert_motion(block->motion_to_be, block, settings));
  }
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_home

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. cutter radius compensation is on:
      NCE_CANNOT_USE_G28_OR_G30_WITH_CUTTER_RADIUS_COMP
   2. The code is not G28 or G30: NCE_BUG_CODE_NOT_G28_OR_G30

Side effects:
   This executes a straight traverse to the programmed point, using
   the current coordinate system, tool length offset, and motion mode
   to interpret the coordinate values. Then it executes a straight
   traverse to the location of reference point 1 (if G28) or reference
   point 2 (if G30). It also updates the setting of the position of the
   tool point to the end point of the move.

Called by: convert_modal_0.

During the motion from the intermediate point to the home point, this
function currently makes the A and C axes turn counterclockwise if a
turn is needed.  This is not necessarily the most efficient way to do
it. A check might be made of which direction to turn to have the least
turn to get to the reference position, and the axis would turn that
way.

*/

int Interp::convert_home(int move,       //!< G code, must be G_28 or G_30            
                        block_pointer block,    //!< pointer to a block of RS274 instructions
                        setup_pointer settings) //!< pointer to machine settings             
{
  static char name[] = "convert_home";
  double end_x;
  double end_y;
  double end_z;
  double AA_end;
  double AA_end2;
  double BB_end;
  double BB_end2;
  double CC_end;
  double CC_end2;
  double *parameters;

  parameters = settings->parameters;
  find_ends(block, settings, &end_x, &end_y, &end_z,
            &AA_end, &BB_end, &CC_end);

  CHK((settings->cutter_comp_side != OFF),
      NCE_CANNOT_USE_G28_OR_G30_WITH_CUTTER_RADIUS_COMP);
  STRAIGHT_TRAVERSE(end_x, end_y, end_z,
#ifndef LATHE
                    AA_end, BB_end, CC_end);
#else
                    0, 0, 0);
#endif
  if (move == G_28) {
    find_relative(parameters[5161], parameters[5162], parameters[5163],
#ifndef LATHE
                  parameters[5164], parameters[5165], parameters[5166],
#else
                  0, 0, 0,
#endif
                  &end_x, &end_y, &end_z,
                  &AA_end2, &BB_end2, &CC_end2, settings);
  } else if (move == G_30) {
    find_relative(parameters[5181], parameters[5182], parameters[5183],
#ifndef LATHE
                  parameters[5184], parameters[5185], parameters[5186],
#else
                  0, 0, 0,
#endif
                  &end_x, &end_y, &end_z,
                  &AA_end2, &BB_end2, &CC_end2, settings);
  } else
    ERM(NCE_BUG_CODE_NOT_G28_OR_G30);
  STRAIGHT_TRAVERSE(end_x, end_y, end_z,
#ifndef LATHE
                    AA_end, BB_end, CC_end);
#else
                    0, 0, 0);
#endif
  settings->current_x = end_x;
  settings->current_y = end_y;
  settings->current_z = end_z;
#ifndef LATHE
  settings->AA_current = AA_end2;
  settings->BB_current = BB_end2;
  settings->CC_current = CC_end2;
#endif
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_length_units

Returned Value: int
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns RS274NGC_OK.
   1. The g_code argument isnt G_20 or G_21:
      NCE_BUG_CODE_NOT_G20_OR_G21
   2. Cutter radius compensation is on:
      NCE_CANNOT_CHANGE_UNITS_WITH_CUTTER_RADIUS_COMP

Side effects:
   A command setting the length units is executed. The machine
   settings are reset regarding length units and current position.

Called by: convert_g.

We are not changing tool length offset values or tool diameter values.
Those values must be given in the table in the correct units. Thus it
will generally not be feasible to switch units in the middle of a
program.

We are not changing the parameters that represent the positions
of the nine work coordinate systems.

We are also not changing feed rate values when length units are
changed, so the actual behavior may change.

Several other distance items in the settings (such as the various
parameters for cycles) are also not reset.

We are changing origin offset and axis offset values, which are
critical. If this were not done, when length units are set and the new
length units are not the same as the default length units
(millimeters), and any XYZ origin or axis offset is not zero, then any
subsequent change in XYZ origin or axis offset values will be
incorrect.  Also, g53 (motion in absolute coordinates) will not work
correctly.

*/

int Interp::convert_length_units(int g_code,     //!< g_code being executed (must be G_20 or G_21)
                                setup_pointer settings) //!< pointer to machine settings                 
{
  static char name[] = "convert_length_units";
  CHK((settings->cutter_comp_side != OFF),
      NCE_CANNOT_CHANGE_UNITS_WITH_CUTTER_RADIUS_COMP);
  if (g_code == G_20) {
    USE_LENGTH_UNITS(CANON_UNITS_INCHES);
    if (settings->length_units != CANON_UNITS_INCHES) {
      settings->length_units = CANON_UNITS_INCHES;
      settings->current_x = (settings->current_x * INCH_PER_MM);
      settings->current_y = (settings->current_y * INCH_PER_MM);
      settings->current_z = (settings->current_z * INCH_PER_MM);
      settings->axis_offset_x = (settings->axis_offset_x * INCH_PER_MM);
      settings->axis_offset_y = (settings->axis_offset_y * INCH_PER_MM);
      settings->axis_offset_z = (settings->axis_offset_z * INCH_PER_MM);
      settings->origin_offset_x = (settings->origin_offset_x * INCH_PER_MM);
      settings->origin_offset_y = (settings->origin_offset_y * INCH_PER_MM);
      settings->origin_offset_z = (settings->origin_offset_z * INCH_PER_MM);
    }
  } else if (g_code == G_21) {
    USE_LENGTH_UNITS(CANON_UNITS_MM);
    if (settings->length_units != CANON_UNITS_MM) {
      settings->length_units = CANON_UNITS_MM;
      settings->current_x = (settings->current_x * MM_PER_INCH);
      settings->current_y = (settings->current_y * MM_PER_INCH);
      settings->current_z = (settings->current_z * MM_PER_INCH);
      settings->axis_offset_x = (settings->axis_offset_x * MM_PER_INCH);
      settings->axis_offset_y = (settings->axis_offset_y * MM_PER_INCH);
      settings->axis_offset_z = (settings->axis_offset_z * MM_PER_INCH);
      settings->origin_offset_x = (settings->origin_offset_x * MM_PER_INCH);
      settings->origin_offset_y = (settings->origin_offset_y * MM_PER_INCH);
      settings->origin_offset_z = (settings->origin_offset_z * MM_PER_INCH);
    }
  } else
    ERM(NCE_BUG_CODE_NOT_G20_OR_G21);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_m

Returned Value: int
   If convert_tool_change returns an error code, this returns that code.
   Otherwise, it returns RS274NGC_OK.

Side effects:
   m_codes in the block are executed. For each m_code
   this consists of making a function call(s) to a canonical machining
   function(s) and setting the machine model.

Called by: execute_block.

This handles four separate types of activity in order:
1. changing the tool (m6) - which also retracts and stops the spindle.
2. Turning the spindle on or off (m3, m4, and m5)
3. Turning coolant on and off (m7, m8, and m9)
4. turning a-axis clamping on and off (m26, m27) - commented out.
5. enabling or disabling feed and speed overrides (m49, m49).
Within each group, only the first code encountered will be executed.

This does nothing with m0, m1, m2, m30, or m60 (which are handled in
convert_stop).

*/

int Interp::convert_m(block_pointer block,       //!< pointer to a block of RS274/NGC instructions
                     setup_pointer settings)    //!< pointer to machine settings                 
{
  static char name[] = "convert_m";
  int status;
  int index;
#if 0
/* FIX-ME Impliment these at a later stage... */
  if (block->m_modes[5] == 62) {
    SET_MOTION_OUTPUT_BIT(round_to_int(block->p_number));
  } else if (block->m_modes[5] == 63) {
    CLEAR_MOTION_OUTPUT_BIT(round_to_int(block->p_number));
  } else if (block->m_modes[5] == 64) {
    SET_AUX_OUTPUT_BIT(round_to_int(block->p_number));
  } else if (block->m_modes[5] == 65) {
    CLEAR_AUX_OUTPUT_BIT(round_to_int(block->p_number));
  }
#endif
  if (block->m_modes[6] != -1) {
    CHP(convert_tool_change(settings));
  }

  if (block->m_modes[7] == 3) {
    START_SPINDLE_CLOCKWISE();
    settings->spindle_turning = CANON_CLOCKWISE;
  } else if (block->m_modes[7] == 4) {
    START_SPINDLE_COUNTERCLOCKWISE();
    settings->spindle_turning = CANON_COUNTERCLOCKWISE;
  } else if (block->m_modes[7] == 5) {
    STOP_SPINDLE_TURNING();
    settings->spindle_turning = CANON_STOPPED;
  }

  if (block->m_modes[8] == 7) {
    MIST_ON();
    settings->mist = ON;
  } else if (block->m_modes[8] == 8) {
    FLOOD_ON();
    settings->flood = ON;
  } else if (block->m_modes[8] == 9) {
    MIST_OFF();
    settings->mist = OFF;
    FLOOD_OFF();
    settings->flood = OFF;
  }

/* No axis clamps in this version
  if (block->m_modes[2] == 26)
    {
#ifdef DEBUG_EMC
      COMMENT("interpreter: automatic A-axis clamping turned on");
#endif
      settings->a_axis_clamping = ON;
    }
  else if (block->m_modes[2] == 27)
    {
#ifdef DEBUG_EMC
      COMMENT("interpreter: automatic A-axis clamping turned off");
#endif
      settings->a_axis_clamping = OFF;
    }
*/

  if (block->m_modes[9] == 48) {
    ENABLE_FEED_OVERRIDE();
    ENABLE_SPEED_OVERRIDE();
    settings->feed_override = ON;
    settings->speed_override = ON;
  } else if (block->m_modes[9] == 49) {
    DISABLE_FEED_OVERRIDE();
    DISABLE_SPEED_OVERRIDE();
    settings->feed_override = OFF;
    settings->speed_override = OFF;
  }
#if 0
/* FIX-ME Impliment these at a later stage... */
  /* user-defined M codes */
  for (index = 100; index < 200; index++) {
    if (block->m_modes[index] == index) {
      if (USER_DEFINED_FUNCTION[index - 100] != 0) {
        (*(USER_DEFINED_FUNCTION[index - 100])) (index - 100,
                                                 block->p_number,
                                                 block->q_number);
      } else {
        CHK(1, NCE_UNKNOWN_M_CODE_USED);
      }
    }
  }
#endif
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_modal_0

Returned Value: int
   If one of the following functions is called and returns an error code,
   this returns that code.
      convert_axis_offsets
      convert_home
      convert_setup
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. code is not G_4, G_10, G_28, G_30, G_53, G92, G_92_1, G_92_2, or G_92_3:
      NCE_BUG_CODE_NOT_G4_G10_G28_G30_G53_OR_G92_SERIES

Side effects: See below

Called by: convert_g

If the g_code is g10, g28, g30, g92, g92.1, g92.2, or g92.3 (all are in
modal group 0), it is executed. The other two in modal group 0 (G4 and
G53) are executed elsewhere.

*/

int Interp::convert_modal_0(int code,    //!< G code, must be from group 0                
                           block_pointer block, //!< pointer to a block of RS274/NGC instructions
                           setup_pointer settings)      //!< pointer to machine settings                 
{
  static char name[] = "convert_modal_0";
  int status;

  if (code == G_10) {
    CHP(convert_setup(block, settings));
  } else if ((code == G_28) || (code == G_30)) {
    CHP(convert_home(code, block, settings));
  } else if ((code == G_92) || (code == G_92_1) ||
             (code == G_92_2) || (code == G_92_3)) {
    CHP(convert_axis_offsets(code, block, settings));
  } else if ((code == G_4) || (code == G_53));  /* handled elsewhere */
  else
    ERM(NCE_BUG_CODE_NOT_G4_G10_G28_G30_G53_OR_G92_SERIES);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_motion

Returned Value: int
   If one of the following functions is called and returns an error code,
   this returns that code.
      convert_arc
      convert_cycle
      convert_probe
      convert_straight
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns RS274NGC_OK.
   1. The motion code is not 0,1,2,3,38.2,80,81,82,83,84,85,86,87, 88, or 89:
      NCE_BUG_UNKNOWN_MOTION_CODE

Side effects:
   A g_code from the group causing motion (mode 1) is executed.

Called by: convert_g.

*/

int Interp::convert_motion(int motion,   //!< g_code for a line, arc, canned cycle     
                          block_pointer block,  //!< pointer to a block of RS274 instructions 
                          setup_pointer settings)       //!< pointer to machine settings              
{
  static char name[] = "convert_motion";
  int status;

  if ((motion == G_0) || (motion == G_1) || (motion == G_33)) {
    CHP(convert_straight(motion, block, settings));
  } else if ((motion == G_3) || (motion == G_2)) {
    CHP(convert_arc(motion, block, settings));
  } else if (motion == G_38_2) {
    CHP(convert_probe(block, settings));
  } else if (motion == G_80) {
#ifdef DEBUG_EMC
    COMMENT("interpreter: motion mode set to none");
#endif
    settings->motion_mode = G_80;
  } else if ((motion > G_80) && (motion < G_90)) {
    CHP(convert_cycle(motion, block, settings));
//  } else if (motion == G_33) {
//    COMMENT("convert_motion: G33 code");
  } else
    ERM(NCE_BUG_UNKNOWN_MOTION_CODE);

  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_probe

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. No value is given in the block for any of X, Y, or Z:
      NCE_X_Y_AND_Z_WORDS_ALL_MISSING_WITH_G38_2
   2. feed mode is inverse time: NCE_CANNOT_PROBE_IN_INVERSE_TIME_FEED_MODE
   3. cutter radius comp is on: NCE_CANNOT_PROBE_WITH_CUTTER_RADIUS_COMP_ON
   4. Feed rate is zero: NCE_CANNOT_PROBE_WITH_ZERO_FEED_RATE
   5. Rotary axis motion is programmed:
      NCE_CANNOT_MOVE_ROTARY_AXES_DURING_PROBING
   6. The starting point for the probe move is within 0.01 inch or 0.254
      millimeters of the point to be probed:
      NCE_START_POINT_TOO_CLOSE_TO_PROBE_POINT

Side effects:
   This executes a straight_probe command.
   The probe_flag in the settings is set to ON.
   The motion mode in the settings is set to G_38_2.

Called by: convert_motion.

The approach to operating in incremental distance mode (g91) is to
put the the absolute position values into the block before using the
block to generate a move.

After probing is performed, the location of the probe cannot be
predicted. This differs from every other command, all of which have
predictable results. The next call to the interpreter (with either
rs274ngc_read or rs274ngc_execute) will result in updating the
current position by calls to get_external_position_x, etc.

*/

int Interp::convert_probe(block_pointer block,   //!< pointer to a block of RS274 instructions
                         setup_pointer settings)        //!< pointer to machine settings             
{
  static char name[] = "convert_probe";
  double distance;
  double end_x;
  double end_y;
  double end_z;
#ifndef LATHE
  double AA_end;
  double BB_end;
  double CC_end;
#endif

  CHK((((block->x_flag == OFF) && (block->y_flag == OFF)) &&
       (block->z_flag == OFF)), NCE_X_Y_AND_Z_WORDS_ALL_MISSING_WITH_G38_2);
  CHK((settings->feed_mode == INVERSE_TIME),
      NCE_CANNOT_PROBE_IN_INVERSE_TIME_FEED_MODE);
  CHK((settings->cutter_comp_side != OFF),
      NCE_CANNOT_PROBE_WITH_CUTTER_RADIUS_COMP_ON);
  CHK((settings->feed_rate == 0.0), NCE_CANNOT_PROBE_WITH_ZERO_FEED_RATE);
  find_ends(block, settings, &end_x, &end_y, &end_z,
#ifndef LATHE
            &AA_end, &BB_end, &CC_end);
#else
            0, 0, 0);
#endif
  if (0
#ifndef LATHE
      || (AA_end != settings->AA_current)
      || (BB_end != settings->BB_current) || (CC_end != settings->CC_current)
#endif
    )
    ERM(NCE_CANNOT_MOVE_ROTARY_AXES_DURING_PROBING);
  distance = sqrt(pow((settings->current_x - end_x), 2) +
                  pow((settings->current_y - end_y), 2) +
                  pow((settings->current_z - end_z), 2));
  CHK((distance <
       ((settings->length_units == CANON_UNITS_MM) ? 0.254 : 0.01)),
      NCE_START_POINT_TOO_CLOSE_TO_PROBE_POINT);
  TURN_PROBE_ON();
  STRAIGHT_PROBE(end_x, end_y, end_z,
#ifndef LATHE
                 AA_end, BB_end, CC_end);
#else
                 0, 0, 0);
#endif

  TURN_PROBE_OFF();
  settings->motion_mode = G_38_2;
  settings->probe_flag = ON;
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_retract_mode

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. g_code isn't G_98 or G_99: NCE_BUG_CODE_NOT_G98_OR_G99

Side effects:
   The interpreter switches the machine settings to indicate the current
   retract mode for canned cycles (OLD_Z or R_PLANE).

Called by: convert_g.

The canonical machine to which commands are being sent does not have a
retract mode, so no command setting the retract mode is generated in
this function.

*/

int Interp::convert_retract_mode(int g_code,     //!< g_code being executed (must be G_98 or G_99)
                                setup_pointer settings) //!< pointer to machine settings                 
{
  static char name[] = "convert_retract_mode";
  if (g_code == G_98) {
#ifdef DEBUG_EMC
    COMMENT("interpreter: retract mode set to old_z");
#endif
    settings->retract_mode = OLD_Z;
  } else if (g_code == G_99) {
#ifdef DEBUG_EMC
    COMMENT("interpreter: retract mode set to r_plane");
#endif
    settings->retract_mode = R_PLANE;
  } else
    ERM(NCE_BUG_CODE_NOT_G98_OR_G99);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_setup

Returned Value: int (RS274NGC_OK)

Side effects:
   SET_PROGRAM_ORIGIN is called, and the coordinate
   values for the program origin are reset.
   If the program origin is currently in use, the values of the
   the coordinates of the current point are updated.

Called by: convert_modal_0.

This is called only if g10 is called. g10 L2 may be used to alter the
location of coordinate systems as described in [NCMS, pages 9 - 10] and
[Fanuc, page 65]. [Fanuc] has only six coordinate systems, while
[NCMS] has nine (the first six of which are the same as the six [Fanuc]
has). All nine are implemented here.

Being in incremental distance mode has no effect on the action of G10
in this implementation. The manual is not explicit about what is
intended.

See documentation of convert_coordinate_system for more information.

*/

int Interp::convert_setup(block_pointer block,   //!< pointer to a block of RS274/NGC instructions
                         setup_pointer settings)        //!< pointer to machine settings                 
{
  static char name[] = "convert_setup";
  double x;
  double y;
  double z;
#ifndef LATHE
  double a;
  double b;
  double c;
#endif
  double *parameters;
  int p_int;

  parameters = settings->parameters;
  p_int = (int) (block->p_number + 0.0001);

  if (block->x_flag == ON) {
    x = block->x_number;
    parameters[5201 + (p_int * 20)] = x;
  } else
    x = parameters[5201 + (p_int * 20)];

  if (block->y_flag == ON) {
    y = block->y_number;
    parameters[5202 + (p_int * 20)] = y;
  } else
    y = parameters[5202 + (p_int * 20)];
  if (block->z_flag == ON) {
    z = block->z_number;
    parameters[5203 + (p_int * 20)] = z;
  } else
    z = parameters[5203 + (p_int * 20)];

#ifndef LATHE
  if (block->a_flag == ON) {
    a = block->a_number;
    parameters[5204 + (p_int * 20)] = a;
  } else
    a = parameters[5204 + (p_int * 20)];

  if (block->b_flag == ON) {
    b = block->b_number;
    parameters[5205 + (p_int * 20)] = b;
  } else
    b = parameters[5205 + (p_int * 20)];

  if (block->c_flag == ON) {
    c = block->c_number;
    parameters[5206 + (p_int * 20)] = c;
  } else
    c = parameters[5206 + (p_int * 20)];
#endif

/* axis offsets could be included in the two sets of calculations for
   current_x, current_y, etc., but do not need to be because the results
   would be the same. They would be added in then subtracted out. */
  if (p_int == settings->origin_index) {        /* system is currently used */
    settings->current_x = (settings->current_x + settings->origin_offset_x);
    settings->current_y = (settings->current_y + settings->origin_offset_y);
    settings->current_z = (settings->current_z + settings->origin_offset_z);
#ifndef LATHE
    settings->AA_current =
      (settings->AA_current + settings->AA_origin_offset);
    settings->BB_current =
      (settings->BB_current + settings->BB_origin_offset);
    settings->CC_current =
      (settings->CC_current + settings->CC_origin_offset);
#endif

    settings->origin_offset_x = x;
    settings->origin_offset_y = y;
    settings->origin_offset_z = z;
#ifndef LATHE
    settings->AA_origin_offset = a;
    settings->BB_origin_offset = b;
    settings->CC_origin_offset = c;
#endif

    settings->current_x = (settings->current_x - x);
    settings->current_y = (settings->current_y - y);
    settings->current_z = (settings->current_z - z);
#ifndef LATHE
    settings->AA_current = (settings->AA_current - a);
    settings->BB_current = (settings->BB_current - b);
    settings->CC_current = (settings->CC_current - c);
#endif

    SET_ORIGIN_OFFSETS(x + settings->axis_offset_x,
                       y + settings->axis_offset_y,
                       z + settings->axis_offset_z,
#ifndef LATHE
                       a + settings->AA_axis_offset,
                       b + settings->BB_axis_offset,
                       c + settings->CC_axis_offset);
#else
                       0, 0, 0);
#endif
  }
#ifdef DEBUG_EMC
  else
    COMMENT("interpreter: setting coordinate system origin");
#endif
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_set_plane

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. G_18 or G_19 is called when cutter radius compensation is on:
      NCE_CANNOT_USE_XZ_PLANE_WITH_CUTTER_RADIUS_COMP
      NCE_CANNOT_USE_YZ_PLANE_WITH_CUTTER_RADIUS_COMP
   2. The g_code is not G_17, G_18, or G_19:
      NCE_BUG_CODE_NOT_G17_G18_OR_G19

Side effects:
   A canonical command setting the current plane is executed.

Called by: convert_g.

*/

int Interp::convert_set_plane(int g_code,        //!< must be G_17, G_18, or G_19 
                             setup_pointer settings)    //!< pointer to machine settings 
{
  static char name[] = "convert_set_plane";
  if (g_code == G_17) {
    SELECT_PLANE(CANON_PLANE_XY);
    settings->plane = CANON_PLANE_XY;
  } else if (g_code == G_18) {
    CHK((settings->cutter_comp_side != OFF),
        NCE_CANNOT_USE_XZ_PLANE_WITH_CUTTER_RADIUS_COMP);
    SELECT_PLANE(CANON_PLANE_XZ);
    settings->plane = CANON_PLANE_XZ;
  } else if (g_code == G_19) {
    CHK((settings->cutter_comp_side != OFF),
        NCE_CANNOT_USE_YZ_PLANE_WITH_CUTTER_RADIUS_COMP);
    SELECT_PLANE(CANON_PLANE_YZ);
    settings->plane = CANON_PLANE_YZ;
  } else
    ERM(NCE_BUG_CODE_NOT_G17_G18_OR_G19);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_speed

Returned Value: int (RS274NGC_OK)

Side effects:
  The machine spindle speed is set to the value of s_number in the
  block by a call to SET_SPINDLE_SPEED.
  The machine model for spindle speed is set to that value.

Called by: execute_block.

*/

int Interp::convert_speed(block_pointer block,   //!< pointer to a block of RS274 instructions
                         setup_pointer settings)        //!< pointer to machine settings             
{
  SET_SPINDLE_SPEED(block->s_number);
  settings->speed = block->s_number;
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_stop

Returned Value: int
   When an m2 or m30 (program_end) is encountered, this returns RS274NGC_EXIT.
   If the code is not m0, m1, m2, m30, or m60, this returns
   NCE_BUG_CODE_NOT_M0_M1_M2_M30_M60
   Otherwise, it returns RS274NGC_OK.

Side effects:
   An m0, m1, m2, m30, or m60 in the block is executed.

   For m0, m1, and m60, this makes a function call to the PROGRAM_STOP
   canonical machining function (which stops program execution).
   In addition, m60 calls PALLET_SHUTTLE.

   For m2 and m30, this resets the machine and then calls PROGRAM_END.
   In addition, m30 calls PALLET_SHUTTLE.

Called by: execute_block.

This handles stopping or ending the program (m0, m1, m2, m30, m60)

[NCMS] specifies how the following modes should be reset at m2 or
m30. The descriptions are not collected in one place, so this list
may be incomplete.

G52 offsetting coordinate zero points [NCMS, page 10]
G92 coordinate offset using tool position [NCMS, page 10]

The following should have reset values, but no description of reset
behavior could be found in [NCMS].
G17, G18, G19 selected plane [NCMS, pages 14, 20]
G90, G91 distance mode [NCMS, page 15]
G93, G94 feed mode [NCMS, pages 35 - 37]
M48, M49 overrides enabled, disabled [NCMS, pages 37 - 38]
M3, M4, M5 spindle turning [NCMS, page 7]

The following should be set to some value at machine start-up but
not automatically reset by any of the stopping codes.
1. G20, G21 length units [NCMS, page 15]. This is up to the installer.
2. motion_control_mode. This is set in rs274ngc_init but not reset here.
   Might add it here.

The following resets have been added by calling the appropriate
canonical machining command and/or by resetting interpreter
settings. They occur on M2 or M30.

1. Axis offsets are set to zero (like g92.2) and      - SET_ORIGIN_OFFSETS
   origin offsets are set to the default (like G54)
2. Selected plane is set to CANON_PLANE_XY (like G17) - SELECT_PLANE
3. Distance mode is set to MODE_ABSOLUTE (like G90)   - no canonical call
4. Feed mode is set to UNITS_PER_MINUTE (like G94)    - no canonical call
5. Feed and speed overrides are set to ON (like M48)  - ENABLE_FEED_OVERRIDE
                                                      - ENABLE_SPEED_OVERRIDE
6. Cutter compensation is turned off (like G40)       - no canonical call
7. The spindle is stopped (like M5)                   - STOP_SPINDLE_TURNING
8. The motion mode is set to G_1 (like G1)            - no canonical call
9. Coolant is turned off (like M9)                    - FLOOD_OFF & MIST_OFF

*/

int Interp::convert_stop(block_pointer block,    //!< pointer to a block of RS274/NGC instructions
                        setup_pointer settings) //!< pointer to machine settings                 
{
  static char name[] = "convert_stop";
  int index;
  char *line;
  int length;

  if (block->m_modes[4] == 0) {
    PROGRAM_STOP();
  } else if (block->m_modes[4] == 60) {
    PALLET_SHUTTLE();
    PROGRAM_STOP();
  } else if (block->m_modes[4] == 1) {
    OPTIONAL_PROGRAM_STOP();
  } else if ((block->m_modes[4] == 2) || (block->m_modes[4] == 30)) {   /* reset stuff here */
/*1*/
    settings->current_x = settings->current_x
      + settings->origin_offset_x + settings->axis_offset_x;
    settings->current_y = settings->current_y
      + settings->origin_offset_y + settings->axis_offset_y;
    settings->current_z = settings->current_z
      + settings->origin_offset_z + settings->axis_offset_z;
#ifndef LATHE
    settings->AA_current = settings->AA_current
      + settings->AA_origin_offset + settings->AA_axis_offset;
    settings->BB_current = settings->BB_current
      + settings->BB_origin_offset + settings->BB_axis_offset;
    settings->CC_current = settings->CC_current
      + settings->CC_origin_offset + settings->CC_axis_offset;
#endif
    settings->origin_index = 1;
    settings->parameters[5220] = 1.0;
    settings->origin_offset_x = settings->parameters[5221];
    settings->origin_offset_y = settings->parameters[5222];
    settings->origin_offset_z = settings->parameters[5223];
#ifndef LATHE
    settings->AA_origin_offset = settings->parameters[5224];
    settings->BB_origin_offset = settings->parameters[5225];
    settings->CC_origin_offset = settings->parameters[5226];
#endif

    settings->axis_offset_x = 0;
    settings->axis_offset_x = 0;
    settings->axis_offset_x = 0;
#ifndef LATHE
    settings->AA_axis_offset = 0;
    settings->BB_axis_offset = 0;
    settings->CC_axis_offset = 0;
#endif

    settings->current_x = settings->current_x - settings->origin_offset_x;
    settings->current_y = settings->current_y - settings->origin_offset_y;
    settings->current_z = settings->current_z - settings->origin_offset_z;
#ifndef LATHE
    settings->AA_current = settings->AA_current - settings->AA_origin_offset;
    settings->BB_current = settings->BB_current - settings->BB_origin_offset;
    settings->CC_current = settings->CC_current - settings->CC_origin_offset;
#endif

    SET_ORIGIN_OFFSETS(settings->origin_offset_x,
                       settings->origin_offset_y, settings->origin_offset_z,
#ifndef LATHE
                       settings->AA_origin_offset,
                       settings->BB_origin_offset,
                       settings->CC_origin_offset);
#else
                       0, 0, 0);
#endif

/*2*/ if (settings->plane != CANON_PLANE_XY) {
      SELECT_PLANE(CANON_PLANE_XY);
      settings->plane = CANON_PLANE_XY;
    }

/*3*/
    settings->distance_mode = MODE_ABSOLUTE;

/*4*/ settings->feed_mode = UNITS_PER_MINUTE;

/*5*/ if (settings->feed_override != ON) {
      ENABLE_FEED_OVERRIDE();
      settings->feed_override = ON;
    }
    if (settings->speed_override != ON) {
      ENABLE_SPEED_OVERRIDE();
      settings->speed_override = ON;
    }

/*6*/
    settings->cutter_comp_side = OFF;
    settings->program_x = UNKNOWN;

/*7*/ STOP_SPINDLE_TURNING();
    settings->spindle_turning = CANON_STOPPED;

/*8*/ settings->motion_mode = G_1;

/*9*/ if (settings->mist == ON) {
      MIST_OFF();
      settings->mist = OFF;
    }
    if (settings->flood == ON) {
      FLOOD_OFF();
      settings->flood = OFF;
    }

    if (block->m_modes[4] == 30)
      PALLET_SHUTTLE();
    PROGRAM_END();
    if (_setup.percent_flag == ON) {
      CHK((_setup.file_pointer == NULL), NCE_UNABLE_TO_OPEN_FILE);
      line = _setup.linetext;
      for (;;) {                /* check for ending percent sign and comment if missing */
        if (fgets(line, LINELEN, _setup.file_pointer) == NULL) {
          COMMENT("interpreter: percent sign missing from end of file");
          break;
        }
        length = strlen(line);
        if (length == (LINELEN - 1)) {       // line is too long. need to finish reading the line
          for (; fgetc(_setup.file_pointer) != '\n';);
          continue;
        }
        for (index = (length - 1);      // index set on last char
             (index >= 0) && (isspace(line[index])); index--);
        if (line[index] == '%') // found line with % at end
        {
          for (index--; (index >= 0) && (isspace(line[index])); index--);
          if (index == -1)      // found line with only percent sign
            break;
        }
      }
    }
    return RS274NGC_EXIT;
  } else
    ERM(NCE_BUG_CODE_NOT_M0_M1_M2_M30_M60);
  return RS274NGC_OK;
}

/*************************************************************************** */

/*! convert_straight

Returned Value: int
   If convert_straight_comp1 or convert_straight_comp2 is called
   and returns an error code, this returns that code.
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns RS274NGC_OK.
   1. The value of move is not G_0 or G_1:
      NCE_BUG_CODE_NOT_G0_OR_G1
   2. A straight feed (g1) move is called with feed rate set to 0:
      NCE_CANNOT_DO_G1_WITH_ZERO_FEED_RATE
   3. A straight feed (g1) move is called with inverse time feed in effect
      but no f word (feed time) is provided:
      NCE_F_WORD_MISSING_WITH_INVERSE_TIME_G1_MOVE
   4. A move is called with G53 and cutter radius compensation on:
      NCE_CANNOT_USE_G53_WITH_CUTTER_RADIUS_COMP
   5. A G33 move is called without the necessary support compiled in:
      NCE_G33_NOT_SUPPORTED

Side effects:
   This executes a STRAIGHT_FEED command at cutting feed rate
   (if move is G_1) or a STRAIGHT_TRAVERSE command (if move is G_0).
   It also updates the setting of the position of the tool point to the
   end point of the move. If cutter radius compensation is on, it may
   also generate an arc before the straight move. Also, in INVERSE_TIME
   feed mode, SET_FEED_RATE will be called the feed rate setting changed.

Called by: convert_motion.

The approach to operating in incremental distance mode (g91) is to
put the the absolute position values into the block before using the
block to generate a move.

In inverse time feed mode, a lower bound of 0.1 is placed on the feed
rate so that the feed rate is never set to zero. If the destination
point is the same as the current point, the feed rate would be
calculated as zero otherwise.

*/

int Interp::convert_straight(int move,   //!< either G_0 or G_1                       
                            block_pointer block,        //!< pointer to a block of RS274 instructions
                            setup_pointer settings)     //!< pointer to machine settings             
{
  static char name[] = "convert_straight";
  double end_x;
  double end_y;
  double end_z;
  double AA_end;
  double BB_end;
  double CC_end;
  int status;

  if (move == G_1) {
    if (settings->feed_mode == UNITS_PER_MINUTE) {
      CHK((settings->feed_rate == 0.0), NCE_CANNOT_DO_G1_WITH_ZERO_FEED_RATE);
    } else if (settings->feed_mode == INVERSE_TIME) {
      CHK((block->f_number == -1.0),
          NCE_F_WORD_MISSING_WITH_INVERSE_TIME_G1_MOVE);
    }
  }

  settings->motion_mode = move;
  find_ends(block, settings, &end_x, &end_y, &end_z,
            &AA_end, &BB_end, &CC_end);

  if ((settings->cutter_comp_side != OFF) &&    /* ! "== ON" */
      (settings->cutter_comp_radius > 0.0)) {   /* radius always is >= 0 */
    CHK((block->g_modes[0] == G_53),
        NCE_CANNOT_USE_G53_WITH_CUTTER_RADIUS_COMP);
    if (settings->program_x == UNKNOWN) {
      status =
        convert_straight_comp1(move, block, settings, end_x, end_y, end_z,
#ifndef LATHE
                               AA_end, BB_end, CC_end);
#else
                               0, 0, 0);
#endif

      CHP(status);
    } else {
      status =
        convert_straight_comp2(move, block, settings, end_x, end_y, end_z,
#ifndef LATHE
                               AA_end, BB_end, CC_end);
#else
                               0, 0, 0);
#endif
      CHP(status);
    }
  } else if (move == G_0) {
    STRAIGHT_TRAVERSE(end_x, end_y, end_z,
#ifndef LATHE
                      AA_end, BB_end, CC_end);
#else
                      0, 0, 0);
#endif
    settings->current_x = end_x;
    settings->current_y = end_y;
  } else if (move == G_1) {
    if (settings->feed_mode == INVERSE_TIME)
      inverse_time_rate_straight(end_x, end_y, end_z,
#ifndef LATHE
                                 AA_end, BB_end, CC_end,
#else
                                 0, 0, 0,
#endif
                                 block, settings);
    STRAIGHT_FEED(end_x, end_y, end_z,
#ifndef LATHE
                  AA_end, BB_end, CC_end);
#else
                  0, 0, 0);
#endif
    settings->current_x = end_x;
    settings->current_y = end_y;
  } else
    if (move == G_33) {
#ifndef LATHE
      ERM(NCE_G33_NOT_SUPPORTED);
#else
    COMMENT("Call THREADING_FUNCTION here");
    COMMENT("in the meantime, do STRAIGHT_TRAVERSE");
    STRAIGHT_TRAVERSE(end_x, end_y, end_z, 0, 0, 0);
#endif    
  } else
    ERM(NCE_BUG_CODE_NOT_G0_OR_G1);

  settings->current_z = end_z;
#ifndef LATHE
  settings->AA_current = AA_end;
  settings->BB_current = BB_end;
  settings->CC_current = CC_end;
#endif
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_straight_comp1

Returned Value: int
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns RS274NGC_OK.
   1. The side is not RIGHT or LEFT:
      NCE_BUG_SIDE_NOT_RIGHT_OR_LEFT
   2. The destination tangent point is not more than a tool radius
      away (indicating gouging): NCE_CUTTER_GOUGING_WITH_CUTTER_RADIUS_COMP
   3. The value of move is not G_0 or G_1
      NCE_BUG_CODE_NOT_G0_OR_G1

Side effects:
   This executes a STRAIGHT_MOVE command at cutting feed rate
   or a STRAIGHT_TRAVERSE command.
   It also updates the setting of the position of the tool point
   to the end point of the move and updates the programmed point.
   If INVERSE_TIME feed rate mode is in effect, it resets the feed rate.

Called by: convert_straight.

This is called if cutter radius compensation is on and settings->program_x
is UNKNOWN, indicating that this is the first move after cutter radius
compensation is turned on.

The algorithm used here for determining the path is to draw a straight
line from the destination point which is tangent to a circle whose
center is at the current point and whose radius is the radius of the
cutter. The destination point of the cutter tip is then found as the
center of a circle of the same radius tangent to the tangent line at
the destination point.

*/

int Interp::convert_straight_comp1(int move,     //!< either G_0 or G_1                        
                                  block_pointer block,  //!< pointer to a block of RS274 instructions 
                                  setup_pointer settings,       //!< pointer to machine settings              
                                  double px,    //!< X coordinate of end point                
                                  double py,    //!< Y coordinate of end point                
                                  double end_z, //!< Z coordinate of end point                
                                  double AA_end,        //!< A coordinate of end point          
                                  double BB_end,        //!< B coordinate of end point          
                                  double CC_end)        //!< C coordinate of end point          
{
  static char name[] = "convert_straight_comp1";
  double alpha;
  double cx;                    /* first current point x then end point x */
  double cy;                    /* first current point y then end point y */
  double distance;
  double radius;
  int side;
  double theta;

  side = settings->cutter_comp_side;
  cx = settings->current_x;
  cy = settings->current_y;

  radius = settings->cutter_comp_radius;        /* always will be positive */
  distance = hypot((px - cx), (py - cy));

  CHK(((side != LEFT) && (side != RIGHT)), NCE_BUG_SIDE_NOT_RIGHT_OR_LEFT);
  CHK((distance <= radius), NCE_CUTTER_GOUGING_WITH_CUTTER_RADIUS_COMP);

  theta = acos(radius / distance);
  alpha = (side == LEFT) ? (atan2((cy - py), (cx - px)) - theta) :
    (atan2((cy - py), (cx - px)) + theta);
  cx = (px + (radius * cos(alpha)));    /* reset to end location */
  cy = (py + (radius * sin(alpha)));
  if (move == G_0)
    STRAIGHT_TRAVERSE(cx, cy, end_z,
#ifndef LATHE
                      AA_end, BB_end, CC_end);
#else
                      0, 0, 0);
#endif
  else if (move == G_1) {
    if (settings->feed_mode == INVERSE_TIME)
      inverse_time_rate_straight(cx, cy, end_z,
#ifndef LATHE
                                 AA_end, BB_end, CC_end,
#else
                                 0, 0, 0,
#endif
                                 block, settings);
    STRAIGHT_FEED(cx, cy, end_z,
#ifndef LATHE
                  AA_end, BB_end, CC_end);
#else
                  0, 0, 0);
#endif
  } else
    ERM(NCE_BUG_CODE_NOT_G0_OR_G1);

  settings->current_x = cx;
  settings->current_y = cy;
  settings->program_x = px;
  settings->program_y = py;
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_straight_comp2

Returned Value: int
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns RS274NGC_OK.
   1. The compensation side is not RIGHT or LEFT:
      NCE_BUG_SIDE_NOT_RIGHT_OR_LEFT
   2. A concave corner is found:
      NCE_CONCAVE_CORNER_WITH_CUTTER_RADIUS_COMP

Side effects:
   This executes a STRAIGHT_FEED command at cutting feed rate
   or a STRAIGHT_TRAVERSE command.
   It also generates an ARC_FEED to go around a corner, if necessary.
   It also updates the setting of the position of the tool point to
   the end point of the move and updates the programmed point.
   If INVERSE_TIME feed mode is in effect, it also calls SET_FEED_RATE
   and resets the feed rate in the machine model.

Called by: convert_straight.

This is called if cutter radius compensation is on and
settings->program_x is not UNKNOWN, indicating that this is not the
first move after cutter radius compensation is turned on.

The algorithm used here is:
1. Determine the direction of the last motion. This is done by finding
   the direction of the line from the last programmed point to the
   current tool tip location. This line is a radius of the tool and is
   perpendicular to the direction of motion since the cutter is tangent
   to that direction.
2. Determine the direction of the programmed motion.
3. If there is a convex corner, insert an arc to go around the corner.
4. Find the destination point for the tool tip. The tool will be
   tangent to the line from the last programmed point to the present
   programmed point at the present programmed point.
5. Go in a straight line from the current tool tip location to the
   destination tool tip location.

This uses an angle tolerance of TOLERANCE_CONCAVE_CORNER (0.01 radian)
to determine if:
1) an illegal concave corner exists (tool will not fit into corner),
2) no arc is required to go around the corner (i.e. the current line
   is in the same direction as the end of the previous move), or
3) an arc is required to go around a convex corner and start off in
   a new direction.

If a rotary axis is moved in this block and an extra arc is required
to go around a sharp corner, all the rotary axis motion occurs on the
arc.  An alternative might be to distribute the rotary axis motion
over the arc and the straight move in proportion to their lengths.

If the Z-axis is moved in this block and an extra arc is required to
go around a sharp corner, all the Z-axis motion occurs on the straight
line and none on the extra arc.  An alternative might be to distribute
the Z-axis motion over the extra arc and the straight line in
proportion to their lengths.

This handles inverse time feed rates by computing the length of the
compensated path.

This handles the case of there being no XY motion.

This handles G0 moves. Where an arc is inserted to round a corner in a
G1 move, no arc is inserted for a G0 move; a STRAIGHT_TRAVERSE is made
from the current point to the end point. The end point for a G0
move is the same as the end point for a G1 move, however.

*/

int Interp::convert_straight_comp2(int move,     //!< either G_0 or G_1                        
                                  block_pointer block,  //!< pointer to a block of RS274 instructions 
                                  setup_pointer settings,       //!< pointer to machine settings              
                                  double px,    //!< X coordinate of programmed end point     
                                  double py,    //!< Y coordinate of programmed end point     
                                  double end_z, //!< Z coordinate of end point                
                                  double AA_end,        //!< A coordinate of end point
                                  double BB_end,        //!< B coordinate of end point
                                  double CC_end)        //!< C coordinate of end point
{
  static char name[] = "convert_straight_comp2";
  double alpha;
  double beta;
  double end_x;                 /* x-coordinate of actual end point */
  double end_y;                 /* y-coordinate of actual end point */
  double gamma;
  double mid_x;                 /* x-coordinate of end of added arc, if needed */
  double mid_y;                 /* y-coordinate of end of added arc, if needed */
  double radius;
  int side;
  double small = TOLERANCE_CONCAVE_CORNER;      /* radians, testing corners */
  double start_x, start_y;      /* programmed beginning point */
  double theta;

  start_x = settings->program_x;
  start_y = settings->program_y;
  if ((py == start_y) && (px == start_x)) {     /* no XY motion */
    end_x = settings->current_x;
    end_y = settings->current_y;
    if (move == G_0)
      STRAIGHT_TRAVERSE(end_x, end_y, end_z,
#ifndef LATHE
                        AA_end, BB_end, CC_end);
#else
                        0, 0, 0);
#endif
    else if (move == G_1) {
      if (settings->feed_mode == INVERSE_TIME)
        inverse_time_rate_straight(end_x, end_y, end_z,
#ifndef LATHE
                                   AA_end, BB_end, CC_end,
#else
                                   0, 0, 0,
#endif
                                   block, settings);
      STRAIGHT_FEED(end_x, end_y, end_z,
#ifndef LATHE
                    AA_end, BB_end, CC_end);
#else
                    0, 0, 0);
#endif
    } else
      ERM(NCE_BUG_CODE_NOT_G0_OR_G1);
  } else {
    side = settings->cutter_comp_side;
    radius = settings->cutter_comp_radius;      /* will always be positive */
    theta = atan2(settings->current_y - start_y,
                  settings->current_x - start_x);
    alpha = atan2(py - start_y, px - start_x);

    if (side == LEFT) {
      if (theta < alpha)
        theta = (theta + (2 * M_PIl));
      beta = ((theta - alpha) - M_PI_2l);
      gamma = M_PI_2l;
    } else if (side == RIGHT) {
      if (alpha < theta)
        alpha = (alpha + (2 * M_PIl));
      beta = ((alpha - theta) - M_PI_2l);
      gamma = -M_PI_2l;
    } else
      ERM(NCE_BUG_SIDE_NOT_RIGHT_OR_LEFT);
    end_x = (px + (radius * cos(alpha + gamma)));
    end_y = (py + (radius * sin(alpha + gamma)));
    mid_x = (start_x + (radius * cos(alpha + gamma)));
    mid_y = (start_y + (radius * sin(alpha + gamma)));

    CHK(((beta < -small) || (beta > (M_PIl + small))),
        NCE_CONCAVE_CORNER_WITH_CUTTER_RADIUS_COMP);
    if (move == G_0)
      STRAIGHT_TRAVERSE(end_x, end_y, end_z,
#ifndef LATHE
                        AA_end, BB_end, CC_end);
#else
                        0, 0, 0);
#endif
    else if (move == G_1) {
      if (beta > small) {       /* ARC NEEDED */
        if (settings->feed_mode == INVERSE_TIME)
          inverse_time_rate_as(start_x, start_y,
                               (side == LEFT) ? -1 : 1, mid_x,
                               mid_y, end_x, end_y, end_z,
#ifndef LATHE
                               AA_end, BB_end, CC_end,
#else
                               0, 0, 0,
#endif
                               block, settings);
        ARC_FEED(mid_x, mid_y, start_x, start_y,
                 ((side == LEFT) ? -1 : 1), settings->current_z,
#ifndef LATHE
                 AA_end, BB_end, CC_end);
#else
                 0, 0, 0);
#endif
        STRAIGHT_FEED(end_x, end_y, end_z,
#ifndef LATHE
                      AA_end, BB_end, CC_end);
#else
                      0, 0, 0);
#endif
      } else {
        if (settings->feed_mode == INVERSE_TIME)
          inverse_time_rate_straight(end_x, end_y, end_z,
#ifndef LATHE
                                     AA_end, BB_end, CC_end,
#else
                                     0, 0, 0,
#endif
                                     block, settings);
        STRAIGHT_FEED(end_x, end_y, end_z,
#ifndef LATHE
                      AA_end, BB_end, CC_end);
#else
                      0, 0, 0);
#endif
      }
    } else
      ERM(NCE_BUG_CODE_NOT_G0_OR_G1);
  }

  settings->current_x = end_x;
  settings->current_y = end_y;
  settings->program_x = px;
  settings->program_y = py;
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_tool_change

Returned Value: int (RS274NGC_OK)

Side effects:
   This makes function calls to canonical machining functions, and sets
   the machine model as described below.

Called by: convert_m

This function carries out an m6 command, which changes the tool in the
spindle. The only function call this makes is to the CHANGE_TOOL
function. The semantics of this function call is that when it is
completely carried out, the tool that was selected is in the spindle,
the tool that was in the spindle (if any) is returned to its changer
slot, the spindle will be stopped (but the spindle speed setting will
not have changed) and the x, y, z, a, b, and c positions will be the same
as they were before (although they may have moved around during the
change).

It would be nice to add more flexibility to this function by allowing
more changes to occur (position changes, for example) as a result of
the tool change. There are at least two ways of doing this:

1. Require that certain machine settings always have a given fixed
value after a tool change (which may be different from what the value
was before the change), and record the fixed values somewhere (in the
world model that is read at initialization, perhaps) so that this
function can retrieve them and reset any settings that have changed.
Fixed values could even be hard coded in this function.

2. Allow the executor of the CHANGE_TOOL function to change the state
of the world however it pleases, and have the interpreter read the
executor's world model after the CHANGE_TOOL function is carried out.
Implementing this would require a change in other parts of the EMC
system, since calls to the interpreter would then have to be
interleaved with execution of the function calls output by the
interpreter.

There may be other commands in the block that includes the tool change.
They will be executed in the order described in execute_block.

This implements the "Next tool in T word" approach to tool selection.
The tool is selected when the T word is read (and the carousel may
move at that time) but is changed when M6 is read.

Note that if a different tool is put into the spindle, the current_z
location setting may be incorrect for a time. It is assumed the
program will contain an appropriate USE_TOOL_LENGTH_OFFSET command
near the CHANGE_TOOL command, so that the incorrect setting is only
temporary.

In [NCMS, page 73, 74] there are three other legal approaches in addition
to this one.

*/

int Interp::convert_tool_change(setup_pointer settings)  //!< pointer to machine settings
{
  static char name[] = "convert_tool_change";

  CHANGE_TOOL(settings->selected_tool_slot);
  settings->current_slot = settings->selected_tool_slot;
  settings->spindle_turning = CANON_STOPPED;

  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_tool_length_offset

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns RS274NGC_OK.
   1. The block has no offset index (h number): NCE_OFFSET_INDEX_MISSING
   2. The g_code argument is not G_43 or G_49:
      NCE_BUG_CODE_NOT_G43_OR_G49

Side effects:
   A USE_TOOL_LENGTH_OFFSET function call is made. Current_z,
   tool_length_offset, and length_offset_index are reset.

Called by: convert_g

This is called to execute g43 or g49.

The g49 RS274/NGC command translates into a USE_TOOL_LENGTH_OFFSET(0.0)
function call.

The g43 RS274/NGC command translates into a USE_TOOL_LENGTH_OFFSET(length)
function call, where length is the value of the entry in the tool length
offset table whose index is the H number in the block.

The H number in the block (if present) was checked for being a non-negative
integer when it was read, so that check does not need to be repeated.

*/

int Interp::convert_tool_length_offset(int g_code,       //!< g_code being executed (must be G_43 or G_49)
                                      block_pointer block,      //!< pointer to a block of RS274/NGC instructions
                                      setup_pointer settings)   //!< pointer to machine settings                 
{
  static char name[] = "convert_tool_length_offset";
  int index;
  double offset;

  if (g_code == G_49) {
    USE_TOOL_LENGTH_OFFSET(0.0);
    settings->current_z = (settings->current_z +
                           settings->tool_length_offset);
    settings->tool_length_offset = 0.0;
    settings->length_offset_index = 0;
  } else if (g_code == G_43) {
    index = block->h_number;
    CHK((index == -1), NCE_OFFSET_INDEX_MISSING);
    offset = settings->tool_table[index].length;
    USE_TOOL_LENGTH_OFFSET(offset);
    settings->current_z =
      (settings->current_z + settings->tool_length_offset - offset);
    settings->tool_length_offset = offset;
    settings->length_offset_index = index;
  } else
    ERM(NCE_BUG_CODE_NOT_G43_OR_G49);
  return RS274NGC_OK;
}

/****************************************************************************/

/*! convert_tool_select

Returned Value: int
   If the tool slot given in the block is larger than allowed,
   this returns NCE_SELECTED_TOOL_SLOT_NUMBER_TOO_LARGE.
   Otherwise, it returns RS274NGC_OK.

Side effects: See below

Called by: execute_block

A select tool command is given, which causes the changer chain to move
so that the slot with the t_number given in the block is next to the
tool changer, ready for a tool change.  The
settings->selected_tool_slot is set to the given slot.

An alternative in this function is to select by tool id. This was used
in the K&T and VGER interpreters. It is easy to code.

A check that the t_number is not negative has already been made in read_t.
A zero t_number is allowed and means no tool should be selected.

*/

int Interp::convert_tool_select(block_pointer block,     //!< pointer to a block of RS274 instructions
                               setup_pointer settings)  //!< pointer to machine settings             
{
  static char name[] = "convert_tool_select";

  CHK((block->t_number > settings->tool_max),
      NCE_SELECTED_TOOL_SLOT_NUMBER_TOO_LARGE);
  SELECT_TOOL(block->t_number);
  settings->selected_tool_slot = block->t_number;
  return RS274NGC_OK;
}
