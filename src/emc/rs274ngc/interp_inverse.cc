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
* $Revision$
* $Author$
* $Date$
********************************************************************/
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

  length = find_arc_length(x1, y1, z1, cx, cy, turn, x2, y2, z2);
  rate = MAX(0.1, (length * block->f_number));
  SET_FEED_RATE(rate);
  settings->feed_rate = rate;

  return INTERP_OK;
}

/****************************************************************************/

/*! inverse_time_rate_arc2

Returned Value: int (INTERP_OK)

Side effects: a call is made to SET_FEED_RATE and _setup.feed_rate is set.

Called by: convert_arc_comp2

This finds the feed rate needed by an inverse time move in
convert_arc_comp2. The move consists of an extra arc and a main
arc. Most of the work here is in finding the lengths of the two arcs.

All rotary motion is assumed to occur on the extra arc, as done by
convert_arc_comp2.

All z motion is assumed to occur on the main arc, as done by
convert_arc_comp2.


*/

int Interp::inverse_time_rate_arc2(double start_x,       //!< x coord of last program point, extra arc center x
                                  double start_y,       //!< y coord of last program point, extra arc center y
                                  int turn1,    //!< turn of extra arc                                
                                  double mid_x, //!< x coord of end point of extra arc                
                                  double mid_y, //!< y coord of end point of extra arc                
                                  double cx,    //!< x coord of center of main arc                    
                                  double cy,    //!< y coord of center of main arc                    
                                  int turn2,    //!< turn of main arc                                 
                                  double end_x, //!< x coord of end point of main arc                 
                                  double end_y, //!< y coord of end point of main arc                 
                                  double end_z, //!< z coord of end point of main arc                 
                                  block_pointer block,  //!< pointer to a block of RS274 instructions         
                                  setup_pointer settings)       //!< pointer to machine settings                      
{
  double length;
  double rate;

  length =
    (find_arc_length
     (settings->current_x, settings->current_y, settings->current_z,
      start_x, start_y, turn1, mid_x, mid_y,
      settings->current_z) + find_arc_length(mid_x, mid_y,
                                             settings->current_z,
                                             cx, cy, turn2, end_x,
                                             end_y, end_z));
  rate = MAX(0.1, (length * block->f_number));
  SET_FEED_RATE(rate);
  settings->feed_rate = rate;

  return INTERP_OK;
}

/****************************************************************************/

/*! inverse_time_rate_as

Returned Value: int (INTERP_OK)

Side effects: a call is made to SET_FEED_RATE and _setup.feed_rate is set.

Called by: convert_straight_comp2

This finds the feed rate needed by an inverse time move in
convert_straight_comp2. The move consists of an extra arc and a straight
line. Most of the work here is in finding the lengths of the arc and
the line.

All rotary motion is assumed to occur on the arc, as done by
convert_straight_comp2.

All z motion is assumed to occur on the line, as done by
convert_straight_comp2.

*/

int Interp::inverse_time_rate_as(double start_x, //!< x coord of last program point, extra arc center x
                                double start_y, //!< y coord of last program point, extra arc center y
                                int turn,       //!< turn of extra arc                                
                                double mid_x,   //!< x coord of end point of extra arc                
                                double mid_y,   //!< y coord of end point of extra arc                
                                double end_x,   //!< x coord of end point of straight line            
                                double end_y,   //!< y coord of end point of straight line            
                                double end_z,   //!< z coord of end point of straight line            
                                double AA_end,  //!< A coord of end point of straight line      
                                double BB_end,  //!< B coord of end point of straight line      
                                double CC_end,  //!< C coord of end point of straight line      
                                block_pointer block,    //!< pointer to a block of RS274 instructions         
                                setup_pointer settings) //!< pointer to machine settings                      
{
  double length;
  double rate;

  length =
    (find_arc_length
     (settings->current_x, settings->current_y, settings->current_z,
      start_x, start_y, turn, mid_x, mid_y,
      settings->current_z) + find_straight_length(end_x, end_y, end_z,
#ifndef LATHE
                            AA_end, BB_end, CC_end,
#else
			    0, 0, 0,
#endif
                            mid_x, mid_y, settings->current_z,
#ifndef LATHE
                            AA_end, BB_end, CC_end));
#else
			    0, 0, 0));
#endif


  rate = MAX(0.1, (length * block->f_number));
  SET_FEED_RATE(rate);
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
                                      block_pointer block,      //!< pointer to a block of RS274 instructions  
                                      setup_pointer settings)   //!< pointer to machine settings               
{
  static char name[] = "inverse_time_rate_straight";
  double length;
  double rate;

  length = find_straight_length(end_x, end_y, end_z,
#ifndef LATHE
                                AA_end, BB_end, CC_end,
#else
				0, 0, 0,
#endif
                                settings->current_x, settings->current_y, settings->current_z,
#ifndef LATHE
                                settings->AA_current, settings->BB_current, settings->CC_current);
#else
				0, 0, 0);
#endif

  rate = MAX(0.1, (length * block->f_number));
  SET_FEED_RATE(rate);
  settings->feed_rate = rate;

  return INTERP_OK;
}
