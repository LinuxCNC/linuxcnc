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

group 0  - array[2]  g4, g10, g28, g30, g53, g92 g92.1, g92.2, g92.3 - misc
group 1  - array[1]  g0, g1, g2, g3, g38.2, g80, g81, g82, g83, g84, g85,
                     g86, g87, g88, g89 - motion
group 2  - array[3]  g17, g18, g19 - plane selection
group 3  - array[6]  g90, g91 - distance mode
group 4  - array[14] g90.1, g91.1 - IJK distance mode for arcs
group 5  - array[7]  g93, g94, g95 - feed rate mode
group 6  - array[5]  g20, g21 - units
group 7  - array[4]  g40, g41, g42 - cutter radius compensation
group 8  - array[9]  g43, g49 - tool length offset
group 9  - no such group
group 10 - array[10] g98, g99 - return mode in canned cycles
group 11 - no such group
group 12 - array[8]  g54, g55, g56, g57, g58, g59, g59.1, g59.2, g59.3
                     - coordinate system
group 13 - array[11] g61, g61.1, g64 - control mode
group 14 - array[12] g50, g51 - adaptive feed mode
group 15 - array[13] g96, g97 - spindle speed mode
group 16 - array[15] g7,g8 - lathe diameter mode

*/

int Interp::write_g_codes(block_pointer block,   //!< pointer to a block of RS274/NGC instructions
                         setup_pointer settings)        //!< pointer to machine settings                 
{
  settings->active_g_codes[0] = settings->sequence_number;
  settings->active_g_codes[1] = settings->motion_mode;
  settings->active_g_codes[2] = ((block == NULL) ? -1 : block->g_modes[GM_MODAL_0]);
  switch(settings->plane) {
  case CANON_PLANE::XY:
      settings->active_g_codes[3] = G_17;
      break;
  case CANON_PLANE::XZ:
      settings->active_g_codes[3] = G_18;
      break;
  case CANON_PLANE::YZ:
      settings->active_g_codes[3] = G_19;
      break;
  case CANON_PLANE::UV:
      settings->active_g_codes[3] = G_17_1;
      break;
  case CANON_PLANE::UW:
      settings->active_g_codes[3] = G_18_1;
      break;
  case CANON_PLANE::VW:
      settings->active_g_codes[3] = G_19_1;
      break;
  }
  settings->active_g_codes[4] =
    (settings->cutter_comp_side == CUTTER_COMP::RIGHT) ? G_42 :
    (settings->cutter_comp_side == CUTTER_COMP::LEFT) ? G_41 : G_40;
  settings->active_g_codes[5] = (settings->length_units == CANON_UNITS_INCHES) ? G_20 : G_21;
  settings->active_g_codes[6] = (settings->distance_mode == DISTANCE_MODE::ABSOLUTE) ? G_90 : G_91;
  settings->active_g_codes[7] = (settings->feed_mode == FEED_MODE::INVERSE_TIME) ? G_93 :
	                        (settings->feed_mode == FEED_MODE::UNITS_PER_MINUTE) ? G_94 : G_95;
  settings->active_g_codes[8] =
    (settings->origin_index <
     7) ? (530 + (10 * settings->origin_index)) : (584 +
                                                   settings->origin_index);
  settings->active_g_codes[9] =
    (settings->g43_with_zero_offset ||
     settings->tool_offset.tran.x || settings->tool_offset.tran.y || settings->tool_offset.tran.z ||
	 settings->tool_offset.a || settings->tool_offset.b || settings->tool_offset.c ||
	 settings->tool_offset.u || settings->tool_offset.v || settings->tool_offset.w) ? G_43 : G_49;
  settings->active_g_codes[10] = (settings->retract_mode == RETRACT_MODE::OLD_Z) ? G_98 : G_99;
  // Three modes:  G_64, G_61, G_61_1 or CANON_CONTINUOUS/EXACT_PATH/EXACT_STOP
  settings->active_g_codes[11] =
    (settings->control_mode == CANON_CONTINUOUS) ? G_64 :
    (settings->control_mode == CANON_EXACT_PATH) ? G_61 : G_61_1;
  settings->active_g_codes[12] = -1;
  settings->active_g_codes[13] = //I don't even know how to display the mode of an arbitrary number of spindles (andypugh 17/6/16)
    (settings->spindle_mode[0] == SPINDLE_MODE::CONSTANT_RPM) ? G_97 : G_96;
  settings->active_g_codes[14] = (settings->ijk_distance_mode == DISTANCE_MODE::ABSOLUTE) ? G_90_1 : G_91_1;
  settings->active_g_codes[15] = (settings->lathe_diameter_mode) ? G_7 : G_8;
  // 'G52','G92' are handled in modal group 0 which is cleared on startup, m2/m30 and abort, hence there
  // is no indication of active G92 offsets after such events so we need modal group 16 as a workaround
  if (block == NULL){ // this handles config startup
    if (settings->parameters[5210] == 1){
      settings->active_g_codes[16] = G_92_3;
    } else {
      settings->active_g_codes[16] = -1;
    }
  } else if (settings->parameters[5210] == 1 && block->g_modes[GM_MODAL_0] == -1){ // this handles aborts, m2/m30
    settings->active_g_codes[16] = G_92_3;
  } else {
    settings->active_g_codes[16] = block->g_modes[GM_G92_IS_APPLIED];
  }
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
  settings->active_m_codes[0] = settings->sequence_number;   /* 0 seq number  */
  settings->active_m_codes[1] = (block == NULL) ? -1 : block->m_modes[4];    /* 1 stopping    */
  settings->active_m_codes[2] = (settings->spindle_turning[0] == CANON_STOPPED) ? 5 :   /* 2 spindle     */
    (settings->spindle_turning[0] == CANON_CLOCKWISE) ? 3 : 4;
  settings->active_m_codes[3] =                      /* 3 tool change */
    (block == NULL) ? -1 : block->m_modes[6];
  settings->active_m_codes[4] =                      /* 4 mist        */
    (settings->mist) ? 7 : (settings->flood) ? -1 : 9;
  settings->active_m_codes[5] =                      /* 5 flood       */
    (settings->flood) ? 8 : -1;
  // This only considers spindle 0. This function //
  //doesn't even know how many spindles there are //
  if (settings->feed_override) {
    if (settings->speed_override[0]) settings->active_m_codes[6] =  48;
    else settings->active_m_codes[6] = 50;
  } else if (settings->speed_override[0]) {
    settings->active_m_codes[6] = 51;
  } else settings->active_m_codes[6] = 49;

  settings->active_m_codes[7] =                      /* 7 overrides   */
    (settings->adaptive_feed) ? 52 : -1;

  settings->active_m_codes[8] =                      /* 8 overrides   */
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
  settings->active_settings[0] = settings->sequence_number;    /* 0 sequence number */
  settings->active_settings[1] = settings->feed_rate;          /* 1 feed rate       */
  settings->active_settings[2] = settings->speed[0];           /* 2 spindle speed   */
  settings->active_settings[3] = settings->tolerance;          /* 3 blend tolerance */
  settings->active_settings[4] = settings->naivecam_tolerance; /* 4 naive CAM tolerance */

  return INTERP_OK;
}

int Interp::write_state_tag(block_pointer block,
			    setup_pointer settings,
			    StateTag &state)
{

    state.fields[GM_FIELD_LINE_NUMBER] = settings->sequence_number;
    //FIXME refactor these into setup methods, and maybe put this
    //whole method in setup struct
    bool in_remap = (settings->remap_level > 0);
    bool in_sub = (settings->call_level > 0 && settings->remap_level == 0);
    bool external_sub = strcmp(settings->filename,
			       settings->sub_context[0].filename);
    strncpy(state.filename, settings->filename, sizeof(state.filename));
    state.filename[sizeof(state.filename)-1] = 0;

    state.flags[GM_FLAG_IN_REMAP] = in_remap;
    state.flags[GM_FLAG_IN_SUB] = in_sub;
    state.flags[GM_FLAG_EXTERNAL_FILE] = external_sub;
    state.flags[GM_FLAG_RESTORABLE] = !in_remap && !in_sub;
    state.fields[GM_FIELD_G_MODE_0] =
	((block == NULL) ? -1 : block->g_modes[GM_MODAL_0]);
    state.fields[GM_FIELD_MOTION_MODE] = settings->motion_mode;
    switch(settings->plane) {
    case CANON_PLANE::XY:
	state.fields[GM_FIELD_PLANE] = G_17;
	break;
    case CANON_PLANE::XZ:
	state.fields[GM_FIELD_PLANE] = G_18;
	break;
    case CANON_PLANE::YZ:
	state.fields[GM_FIELD_PLANE] = G_19;
	break;
    case CANON_PLANE::UV:
	state.fields[GM_FIELD_PLANE] = G_17_1;
	break;
    case CANON_PLANE::UW:
	state.fields[GM_FIELD_PLANE] = G_18_1;
	break;
    case CANON_PLANE::VW:
	state.fields[GM_FIELD_PLANE] = G_19_1;
	break;
    }

    state.fields[GM_FIELD_CUTTER_COMP] =
	(settings->cutter_comp_side == CUTTER_COMP::RIGHT) ? G_42 :
	(settings->cutter_comp_side == CUTTER_COMP::LEFT) ? G_41 : G_40;

    state.flags[GM_FLAG_UNITS] =
	(settings->length_units == CANON_UNITS_INCHES);

    state.flags[GM_FLAG_DISTANCE_MODE] =
	(settings->distance_mode == DISTANCE_MODE::ABSOLUTE);
    state.flags[GM_FLAG_FEED_INVERSE_TIME] =
	(settings->feed_mode == FEED_MODE::INVERSE_TIME);
    state.flags[GM_FLAG_FEED_UPM] =
	(settings->feed_mode == FEED_MODE::UNITS_PER_MINUTE);


    state.fields[GM_FIELD_ORIGIN] =
        ((settings->origin_index < 7) ?
	 (530 + (10 * settings->origin_index)) :
	 (584 + settings->origin_index));

    state.flags[GM_FLAG_G92_IS_APPLIED] = settings->parameters[5210];

    state.flags[GM_FLAG_TOOL_OFFSETS_ON] =
    (settings->g43_with_zero_offset ||
     settings->tool_offset.tran.x ||
	 settings->tool_offset.tran.y ||
	 settings->tool_offset.tran.z ||
	 settings->tool_offset.a ||
	 settings->tool_offset.b ||
	 settings->tool_offset.c ||
	 settings->tool_offset.u ||
	 settings->tool_offset.v ||
	 settings->tool_offset.w);
    state.flags[GM_FLAG_RETRACT_OLDZ] =
	(settings->retract_mode == RETRACT_MODE::OLD_Z);

    state.flags[GM_FLAG_BLEND] =
	(settings->control_mode == CANON_CONTINUOUS);
    state.flags[GM_FLAG_EXACT_STOP] =
	(settings->control_mode == CANON_EXACT_STOP);
    state.fields_float[GM_FIELD_FLOAT_PATH_TOLERANCE] =
	settings->tolerance;
    state.fields_float[GM_FIELD_FLOAT_NAIVE_CAM_TOLERANCE] =
	settings->naivecam_tolerance;

    state.flags[GM_FLAG_CSS_MODE] =
	(settings->spindle_mode[0] == SPINDLE_MODE::CONSTANT_RPM);
    state.flags[GM_FLAG_IJK_ABS] =
	(settings->ijk_distance_mode == DISTANCE_MODE::ABSOLUTE);
    state.flags[GM_FLAG_DIAMETER_MODE] =
	(settings->lathe_diameter_mode);


    state.fields[GM_FIELD_M_MODES_4] =
	(block == NULL) ? -1 : block->m_modes[4];

    state.flags[GM_FLAG_SPINDLE_ON] =
	(settings->spindle_turning[0] != CANON_STOPPED);
    state.flags[GM_FLAG_SPINDLE_CW] =
	(settings->spindle_turning[0] == CANON_CLOCKWISE);

    state.fields[GM_FIELD_TOOLCHANGE] =
	(block == NULL) ? -1 : block->m_modes[6];

    state.flags[GM_FLAG_MIST] = (settings->mist) ;
    state.flags[GM_FLAG_FLOOD] = (settings->flood);

    state.flags[GM_FLAG_FEED_OVERRIDE] = settings->feed_override;
    state.flags[GM_FLAG_SPEED_OVERRIDE] = settings->speed_override[0];

    state.flags[GM_FLAG_ADAPTIVE_FEED] = (settings->adaptive_feed);

    state.flags[GM_FLAG_FEED_HOLD] =  (settings->feed_hold);

    state.fields_float[GM_FIELD_FLOAT_FEED] = settings->feed_rate;
    state.fields_float[GM_FIELD_FLOAT_SPEED] = settings->speed[0];

    return 0;
}

int Interp::write_canon_state_tag(block_pointer block, setup_pointer settings)
{
    StateTag tag;
    write_state_tag(block, settings, tag);
    update_tag(tag);
    return 0;
}

/****************************************************************************/

