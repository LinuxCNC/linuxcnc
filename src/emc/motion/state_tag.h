/********************************************************************
* Description: state_tag.h
*
* A "tag" struct that is used to add interpreter state information to
* a given motion line. This state info isn't actually used by motion
* directly, but indicates the motion state.
*
* Copyright © 2015 Robert W. Ellenberg
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
********************************************************************/


#ifndef STATE_TAG_H
#define STATE_TAG_H


/**
 * Enum to define bit names for StateTag's flags register.
 * The actual position of the flag isn't important, so the actual index doesn't
 * matter. However, the bit field should be <=64 bits to fit within a long int.
 */
typedef enum {
    GM_FLAG_UNITS,
    GM_FLAG_DISTANCE_MODE,
    GM_FLAG_TOOL_OFFSETS_ON,
    GM_FLAG_RETRACT_OLDZ,
    GM_FLAG_BLEND,
    GM_FLAG_EXACT_STOP,
    GM_FLAG_FEED_INVERSE_TIME,
    GM_FLAG_FEED_UPM,
    GM_FLAG_CSS_MODE,
    GM_FLAG_IJK_ABS,
    GM_FLAG_DIAMETER_MODE,
    GM_FLAG_G92_IS_APPLIED,
    GM_FLAG_SPINDLE_ON,
    GM_FLAG_SPINDLE_CW,
    GM_FLAG_MIST,
    GM_FLAG_FLOOD,
    GM_FLAG_FEED_OVERRIDE,
    GM_FLAG_SPEED_OVERRIDE,
    GM_FLAG_ADAPTIVE_FEED,
    GM_FLAG_FEED_HOLD,
    GM_FLAG_RESTORABLE,
    GM_FLAG_IN_REMAP,
    GM_FLAG_IN_SUB,
    GM_FLAG_EXTERNAL_FILE,
    GM_FLAG_MAX_FLAGS
} StateFlag;


/**
 * Enum for various fields of state info that are int type.
 *
 * WARNING:
 *
 * 1) Since these are used as array indices, they have to start at 0,
 * be monotonic, and the MAX_FIELDS enum MUST be last in the list.
 *
 * 2) If your application needs to pass state tags through NML, then
 * you MUST update the corresponding cms->update function for state
 * tags.
 *
 * TODO: make that standalone function a method here for maintainability
 */
typedef enum {
    GM_FIELD_LINE_NUMBER,
    GM_FIELD_G_MODE_0,
    GM_FIELD_CUTTER_COMP,
    GM_FIELD_MOTION_MODE,
    GM_FIELD_PLANE,
    GM_FIELD_M_MODES_4,
    GM_FIELD_ORIGIN,
    GM_FIELD_TOOLCHANGE,
    GM_FIELD_MAX_FIELDS
} StateField;


/**
 * Enum for indexing state tag `fields_float`, machine state float
 * array:  feed, speed, etc.
 */
typedef enum {
    GM_FIELD_FLOAT_LINE_NUMBER, // eww
    GM_FIELD_FLOAT_FEED,
    GM_FIELD_FLOAT_SPEED,
    GM_FIELD_FLOAT_PATH_TOLERANCE,
    GM_FIELD_FLOAT_NAIVE_CAM_TOLERANCE,
    GM_FIELD_FLOAT_MAX_FIELDS
} StateFieldFloat;

/**
 * Tag structure that is added to a motion segment so that motion has a copy of
 * the relevant interp state.
 *
 * Previously, this information was stored only in the interpreter, and as
 * vectors of g codes, m codes, and settings. Considering that the write_XXX
 * and gen_XXX functions had to jump through hoops to translate from a settings
 * struct, the extra packing here isn't much more complex to deal with, and
 * will cost much less space when copying back and forth.
 */
struct state_tag_t {

    // Float-type machine settings:  feed, speed, etc., indexed by the
    // StateFieldFloat enum above
    float fields_float[GM_FIELD_FLOAT_MAX_FIELDS];

    // Any G / M code states that doesn't pack nicely into a single bit
    // These are an array mostly because it's easier to pass an
    // arbitrary-length array through NML than individual fields
    int fields[GM_FIELD_MAX_FIELDS];

    /** G / M mode flags for simple states like inch / mm, feedhold enable, etc.
     * This stores packed bits in one field (since we can't use a bitset in a
     * pure C struct).
     */
    unsigned long int packed_flags;
    char filename[256];
};

#endif
