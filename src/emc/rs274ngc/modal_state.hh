/********************************************************************
* Description: modal_state.hh
*
* State storage class for interpreter
*
* Copyright © 2014 Robert W. Ellenberg
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

#ifndef MODAL_STATE_HH
#define MODAL_STATE_HH
#include <vector>
#include <bitset>

// Bring in C struct for a state tag from motion
extern "C" {
#include "state_tag.h"
}

/**
 * C++ version of state_tag_t structure to stuff interp state info in
 * a motion message.  Previously, this information was stored only in
 * the interpreter, and as vectors of g codes, m codes, and
 * settings. Considering that the write_XXX and gen_XXX functions had
 * to jump through hoops to translate from a settings struct, the
 * extra packing here isn't much more complex to deal with, and will
 * cost much less space in an NML message.
 *
 * Using this class means we can work with a bitset instead of raw
 * bitmasking operations. Also, because we're inheriting from the C
 * struct, copy / assignment is valid.
 */
struct StateTag : public state_tag_t {
    StateTag();
    StateTag(state_tag_t const &basetag);
    std::bitset<64> flags;
    int is_valid(void) const;
    state_tag_t get_state_tag() const;
};
#endif
