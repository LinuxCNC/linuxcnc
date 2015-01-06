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

ModalState::ModalState(): g_codes(ACTIVE_G_CODES,0),
    m_codes(ACTIVE_M_CODES,0),
    settings(ACTIVE_SETTINGS,0.0)
{}

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

