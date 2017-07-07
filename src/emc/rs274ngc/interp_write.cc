/********************************************************************
* Description: interp_write.cc
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
#include "rs274ngc_interp.hh"

/****************************************************************************/
/*! write_g_codes

Returned Value: int (INTERP_OK)

Side effects:
   The active_g_codes in the settings are updated.

Called by:
   Interp::execute
   Interp::init

The block may be NULL.

This writes active g_codes into the settings->active_g_codes array by
examining the interpreter settings. The array of actives is composed
of ints, so (to handle codes like 59.1) all g_codes are reported as
ints ten times the actual value. For example, 59.1 is reported as 591.

The correspondence between modal groups and array indexes is as follows
(no apparent logic to it).

The group 0 entry is taken from the block (if there is one), since its
codes are not modal.

group 0  - gez[2]  g4, g10, g28, g30, g53, g92 g92.1, g92.2, g92.3 - misc
group 1  - gez[1]  g0, g1, g2, g3, g38.2, g80, g81, g82, g83, g84, g85,
                   g86, g87, g88, g89 - motion
group 2  - gez[3]  g17, g18, g19 - plane selection
group 3  - gez[6]  g90, g91 - distance mode
group 4  - gez[14] g90.1, g91.1 - IJK distance mode for arcs
group 5  - gez[7]  g93, g94, g95 - feed rate mode
group 6  - gez[5]  g20, g21 - units
group 7  - gez[4]  g40, g41, g42 - cutter radius compensation
group 8  - gez[9]  g43, g49 - tool length offse
group 9  - no such group
group 10 - gez[10] g98, g99 - return mode in canned cycles
group 11 - no such group
group 12 - gez[8]  g54, g55, g56, g57, g58, g59, g59.1, g59.2, g59.3
                   - coordinate system
group 13 - gez[11] g61, g61.1, g64 - control mode
group 14 - gez[12] g50, g51 - adaptive feed mode
group 15 - gez[13] g96, g97 - spindle speed mode
group 16 - gez[15] g7,g8 - lathe diameter mode

*/

int Interp::write_g_codes(block_pointer block,   //!< pointer to a block of RS274/NGC instructions
                         setup_pointer settings)        //!< pointer to machine settings                 
{
  int *gez;

  gez = settings->active_g_codes;
  gez[0] = settings->sequence_number;
  gez[1] = settings->motion_mode;
  gez[2] = ((block == NULL) ? -1 : block->g_modes[0]);
  switch(settings->plane) {
  case CANON_PLANE_XY:
      gez[3] = G_17;
      break;
  case CANON_PLANE_XZ:
      gez[3] = G_18;
      break;
  case CANON_PLANE_YZ:
      gez[3] = G_19;
      break;
  case CANON_PLANE_UV:
      gez[3] = G_17_1;
      break;
  case CANON_PLANE_UW:
      gez[3] = G_18_1;
      break;
  case CANON_PLANE_VW:
      gez[3] = G_19_1;
      break;
  }
  gez[4] =
    (settings->cutter_comp_side == RIGHT) ? G_42 :
    (settings->cutter_comp_side == LEFT) ? G_41 : G_40;
  gez[5] = (settings->length_units == CANON_UNITS_INCHES) ? G_20 : G_21;
  gez[6] = (settings->distance_mode == MODE_ABSOLUTE) ? G_90 : G_91;
  gez[7] = (settings->feed_mode == INVERSE_TIME) ? G_93 :
	    (settings->feed_mode == UNITS_PER_MINUTE) ? G_94 : G_95;
  gez[8] =
    (settings->origin_index <
     7) ? (530 + (10 * settings->origin_index)) : (584 +
                                                   settings->origin_index);
  gez[9] = (settings->tool_offset.tran.x || settings->tool_offset.tran.y || settings->tool_offset.tran.z ||
            settings->tool_offset.a || settings->tool_offset.b || settings->tool_offset.c ||
            settings->tool_offset.u || settings->tool_offset.v || settings->tool_offset.w) ? G_43 : G_49;
  gez[10] = (settings->retract_mode == OLD_Z) ? G_98 : G_99;
  gez[11] =
    (settings->control_mode == CANON_CONTINUOUS) ? G_64 :
    (settings->control_mode == CANON_EXACT_PATH) ? G_61 : G_61_1;
  gez[12] = -1;
  gez[13] = //I don't even know how to display the mode of an arbitray number of spindles (andypugh 17/6/16)
    (settings->spindle_mode[0] == CONSTANT_RPM) ? G_97 : G_96;
  gez[14] = (settings->ijk_distance_mode == MODE_ABSOLUTE) ? G_90_1 : G_91_1;
  gez[15] = (settings->lathe_diameter_mode) ? G_7 : G_8;
  return INTERP_OK;
}

/****************************************************************************/

/*! write_m_codes

Returned Value: int (INTERP_OK)

Side effects:
   The settings->active_m_codes are updated.

Called by:
   Interp::execute
   Interp::init

This is testing only the feed override to see if overrides is on.
Might add check of speed override.

*/

int Interp::write_m_codes(block_pointer block,   //!< pointer to a block of RS274/NGC instructions
                         setup_pointer settings)        //!< pointer to machine settings                 
{
  int *emz;

  emz = settings->active_m_codes;
  emz[0] = settings->sequence_number;   /* 0 seq number  */
  emz[1] = (block == NULL) ? -1 : block->m_modes[4];    /* 1 stopping    */
  emz[2] = (settings->spindle_turning[0] == CANON_STOPPED) ? 5 :   /* 2 spindle     */
    (settings->spindle_turning[0] == CANON_CLOCKWISE) ? 3 : 4;
  emz[3] =                      /* 3 tool change */
    (block == NULL) ? -1 : block->m_modes[6];
  emz[4] =                      /* 4 mist        */
    (settings->mist) ? 7 : (settings->flood) ? -1 : 9;
  emz[5] =                      /* 5 flood       */
    (settings->flood) ? 8 : -1;
  if (settings->feed_override) {
    if (settings->speed_override) emz[6] =  48;
    else emz[6] = 50;
  } else if (settings->speed_override) {
    emz[6] = 51;
  } else emz[6] = 49;
  
  emz[7] =                      /* 7 overrides   */
    (settings->adaptive_feed) ? 52 : -1;

  emz[8] =                      /* 8 overrides   */
    (settings->feed_hold) ? 53 : -1;

  return INTERP_OK;
}

/****************************************************************************/

/*! write_settings

Returned Value: int (INTERP_OK)

Side effects:
  The settings->active_settings array of doubles is updated with the
  sequence number, feed, and speed settings.

Called by:
  Interp::execute
  Interp::init

*/

int Interp::write_settings(setup_pointer settings)       //!< pointer to machine settings
{
  double *vals;

  vals = settings->active_settings;
  vals[0] = settings->sequence_number;  /* 0 sequence number */
  vals[1] = settings->feed_rate;        /* 1 feed rate       */
  vals[2] = settings->speed[0];    /* 2 spindle speed   */

  return INTERP_OK;
}

/****************************************************************************/
