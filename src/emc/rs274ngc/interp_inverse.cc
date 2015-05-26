/********************************************************************
* Description: interp_inverse.cc
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
********************************************************************/
#include <boost/python.hpp>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "rtapi_math.h"
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "interp_queue.hh"
#include "rs274ngc_interp.hh"

/****************************************************************************/

/*! inverse_time_rate_arc

Returned Value: int (INTERP_OK)

Side effects: a call is made to SET_FEED_RATE and _setup.feed_rate is set.

Called by:
  convert_arc2
  convert_arc_comp1
  convert_arc_comp2

This finds the feed rate needed by an inverse time move. The move
consists of an a single arc. Most of the work here is in finding the
length of the arc.

*/

int Interp::inverse_time_rate_arc(double x1,     //!< x coord of start point of arc           
                                 double y1,     //!< y coord of start point of arc           
                                 double z1,     //!< z coord of start point of arc           
                                 double cx,     //!< x coord of center of arc                
                                 double cy,     //!< y coord of center of arc                
                                 int turn,      //!< turn of arc                             
                                 double x2,     //!< x coord of end point of arc             
                                 double y2,     //!< y coord of end point of arc             
                                 double z2,     //!< z coord of end point of arc             
                                 block_pointer block,   //!< pointer to a block of RS274 instructions
                                 setup_pointer settings)        //!< pointer to machine settings             
{
  double length;
  double rate;

  if (settings->feed_mode != INVERSE_TIME) return -1;

  length = find_arc_length(x1, y1, z1, cx, cy, turn, x2, y2, z2);
  rate = MAX(0.1, (length * block->f_number));
  enqueue_SET_FEED_RATE(rate);
  settings->feed_rate = rate;

  return INTERP_OK;
}

/****************************************************************************/

/*! inverse_time_rate_straight

Returned Value: int (INTERP_OK)

Side effects: a call is made to SET_FEED_RATE and _setup.feed_rate is set.

Called by:
  convert_straight
  convert_straight_comp1
  convert_straight_comp2

This finds the feed rate needed by an inverse time straight move. Most
of the work here is in finding the length of the line.

*/

int Interp::inverse_time_rate_straight(double end_x,     //!< x coordinate of end point of straight line
                                       double end_y,     //!< y coordinate of end point of straight line
                                       double end_z,     //!< z coordinate of end point of straight line
                                       double AA_end,    //!< A coordinate of end point of straight line/*AA*/
                                       double BB_end,    //!< B coordinate of end point of straight line/*BB*/
                                       double CC_end,    //!< C coordinate of end point of straight line/*CC*/
                                       double u_end, double v_end, double w_end,
                                       block_pointer block,      //!< pointer to a block of RS274 instructions  
                                       setup_pointer settings)   //!< pointer to machine settings               
{
  double length;
  double rate;
  double cx, cy, cz;

  if (settings->feed_mode != INVERSE_TIME) return -1;

  if (settings->cutter_comp_side && settings->cutter_comp_radius > 0.0 &&
      !settings->cutter_comp_firstmove) {
      cx = settings->program_x;
      cy = settings->program_y;
      cz = settings->program_z;
  } else {
      cx = settings->current_x;
      cy = settings->current_y;
      cz = settings->current_z;
  }

  length = find_straight_length(end_x, end_y, end_z,
                                AA_end, BB_end, CC_end,
                                u_end, v_end, w_end,
                                cx, cy, cz,
                                settings->AA_current, settings->BB_current, settings->CC_current,
                                settings->u_current, settings->v_current, settings->w_current);

  rate = MAX(0.1, (length * block->f_number));
  enqueue_SET_FEED_RATE(rate);
  settings->feed_rate = rate;

  return INTERP_OK;
}
