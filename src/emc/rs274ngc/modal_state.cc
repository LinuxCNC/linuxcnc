/********************************************************************
* Description: modal_state.cc
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
#include "interp_base.hh"
#include "modal_state.hh"
#include <string.h>

StateTag::StateTag(): flags(0)
{
    memset(fields,-1,sizeof(fields));
    packed_flags = 0;
    feed = 0.0;
    speed = 0.0;
}

StateTag::StateTag(struct state_tag_t const & basetag): flags(basetag.packed_flags)
{
    memcpy(fields,basetag.fields,sizeof(fields));
    packed_flags = 0;
    feed = basetag.feed;
    speed = basetag.speed;
}


/**
 * Return true if the tag is a valid state, and false if not
 */
int StateTag::is_valid(void) const
{

    if (fields[GM_FIELD_LINE_NUMBER] <= 1) {
        return false;
    }

    //TODO magic numbers
    if (fields[GM_FIELD_ORIGIN] < 540) {
        return false;
    }

    if (fields[GM_FIELD_PLANE] < 170 ) {
        return false;
    }

    return true;
}

