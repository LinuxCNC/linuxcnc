/********************************************************************
* Description: modal_state.hh
*
* State storage class for interpreter
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2014 All rights reserved.
*
********************************************************************/

#ifndef MODAL_STATE_HH
#define MODAL_STATE_HH
#include <vector>
#include <bitset>

// Bring in C struct for a state tag from motion
extern "C" {
#include "state_tag.h"
}

/**
 * C++ version of state_tag_t structure to stuff interp state info in a motion message.
 * Previously, this information was stored only in the interpreter, and as
 * vectors of g codes, m codes, and settings. Considering that the write_XXX
 * and gen_XXX functions had to jump through hoops to translate from a settings
 * struct, the extra packing here isn't much more complex to deal with, and
 * will cost much less space in an NML message.
 *
 * Using this class means we can work with a bitset instead of raw bitmasking
 * operations. Also, because we're inheriting from the C struct, copy /
 * assignment is valid.
 */
struct StateTag : public state_tag_t {
    StateTag();
    StateTag(state_tag_t const &basetag);
    std::bitset<64> flags;
    int is_valid(void) const;
    state_tag_t get_state_tag() const;
};
#endif
