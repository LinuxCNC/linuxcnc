/********************************************************************
* Description: state_tag.h
*
* A "tag" struct that is used to add interpreter state information to
* a given motion line. This state info isn't actually used by motion
* directly, but indicates the motion state.
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved
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
 * Enum for various fields of state info that aren't binary.
 * WARNING:
 * 1) Since these are used as array indices, they have to start at 0,
 * be monotonic, and the MAX_FIELDS enum MUST be last in the list.
 * 2) If your application needs to pass state tags through NML, then you MUST update the corresponding cms->update function for state tags.
 * TODO: make that standalone function a method here for maintainability
 */
typedef enum {
    GM_FIELD_LINE_NUMBER,
    GM_FIELD_STEPPING_ID,
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

    // Machine settings
    float feed;
    float speed;

    // Any G / M code states that doesn't pack nicely into a single bit
    // These are an array mostly because it's easier to pass an
    // arbitrary-length array through NML than individual fields
    int fields[GM_FIELD_MAX_FIELDS];

    /** G / M mode flags for simple states like inch / mm, feedhold enable, etc.
     * This stores packed bits in one field (since we can't use a bitset in a
     * pure C struct).
     */
    unsigned long int packed_flags;
};

#endif
