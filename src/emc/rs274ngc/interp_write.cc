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
group 4  - no such group
group 5  - gez[7]  g93, g94 - feed rate mode
group 6  - gez[5]  g20, g21 - units
group 7  - gez[4]  g40, g41, g42 - cutter radius compensation
group 8  - gez[9]  g43, g49 - tool length offse
group 9  - no such group
group 10 - gez[10] g98, g99 - return mode in canned cycles
group 11 - no such group
group 12 - gez[8]  g54, g55, g56, g57, g58, g59, g59.1, g59.2, g59.3
                   - coordinate system
group 13 - gez[11] g61, g61.1, g64 - control mode

*/

int Interp::write_g_codes(block_pointer block,   //!< pointer to a block of RS274/NGC instructions
                         setup_pointer settings)        //!< pointer to machine settings                 
{
  int *gez;

  gez = settings->active_g_codes;
  gez[0] = settings->sequence_number;
  gez[1] = settings->motion_mode;
  gez[2] = ((block == NULL) ? -1 : block->g_modes[0]);
  gez[3] =
    (settings->plane == CANON_PLANE_XY) ? G_17 :
    (settings->plane == CANON_PLANE_XZ) ? G_18 : G_19;
  gez[4] =
    (settings->cutter_comp_side == RIGHT) ? G_42 :
    (settings->cutter_comp_side == LEFT) ? G_41 : G_40;
  gez[5] = (settings->length_units == CANON_UNITS_INCHES) ? G_20 : G_21;
  gez[6] = (settings->distance_mode == MODE_ABSOLUTE) ? G_90 : G_91;
  gez[7] = (settings->feed_mode == INVERSE_TIME) ? G_93 : G_94;
  gez[8] =
    (settings->origin_index <
     7) ? (530 + (10 * settings->origin_index)) : (584 +
                                                   settings->origin_index);
  gez[9] = (settings->tool_length_offset == 0.0) ? G_49 : G_43;
  gez[10] = (settings->retract_mode == OLD_Z) ? G_98 : G_99;
  gez[11] =
    (settings->control_mode == CANON_CONTINUOUS) ? G_64 :
    (settings->control_mode == CANON_EXACT_PATH) ? G_61 : G_61_1;

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
  emz[2] = (settings->spindle_turning == CANON_STOPPED) ? 5 :   /* 2 spindle     */
    (settings->spindle_turning == CANON_CLOCKWISE) ? 3 : 4;
  emz[3] =                      /* 3 tool change */
    (block == NULL) ? -1 : block->m_modes[6];
  emz[4] =                      /* 4 mist        */
    (settings->mist == ON) ? 7 : (settings->flood == ON) ? -1 : 9;
  emz[5] =                      /* 5 flood       */
    (settings->flood == ON) ? 8 : -1;
  emz[6] =                      /* 6 overrides   */
    (settings->feed_override == ON) ? 48 : 49;

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
  vals[2] = settings->speed;    /* 2 spindle speed   */

  return INTERP_OK;
}

/****************************************************************************/
