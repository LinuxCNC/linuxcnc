/*
 * Copyright (C) 2013-2014 Michael Haberler <license@mah.priv.at>
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
 */

// various functions and inlines dealing both with protobuf and HAL objects

#include <rtapi.h>
#include <hal.h>
#include <hal_priv.h>
#include <hal_group.h>
#include <hal_rcomp.h>
#include <hal_ring.h>
#include "message.pb.h"

// in halpb.cc:
int halpr_describe_signal(hal_sig_t *sig, machinetalk::Signal *pbsig);
int halpr_describe_pin(hal_pin_t *pin, machinetalk::Pin *pbpin);
int halpr_describe_ring(hal_ring_t *ring, machinetalk::Ring *pbring);
int halpr_describe_funct(hal_funct_t *funct, machinetalk::Function *pbfunct);
int halpr_describe_thread(hal_thread_t *thread, machinetalk::Thread *pbthread);
int halpr_describe_component(hal_comp_t *comp, machinetalk::Component *pbcomp);
int halpr_describe_group(hal_group_t *g, machinetalk::Group *pbgroup);
int halpr_describe_member(hal_member_t *member, machinetalk::Member *pbmember);

static inline int hal_pin2pb(hal_pin_t *hp, machinetalk::Pin *p)
{
    const hal_data_u *vp  = pin_value(hp);
    switch (hp->type) {
    default:
	return -1;
    case HAL_BIT:
	p->set_halbit(get_bit_value(vp));
	break;
    case HAL_FLOAT:
	p->set_halfloat(get_float_value(vp));
	break;
    case HAL_S32:
	p->set_hals32(get_s32_value(vp));
	break;
    case HAL_U32:
	p->set_halu32(get_u32_value(vp));
	break;
    }
    return 0;
}

static inline int hal_sig2pb(hal_sig_t *sp, machinetalk::Signal *s)
{
    const hal_data_u *vp = sig_value(sp);
    switch (sp->type) {
    default:
	return -1;
    case HAL_BIT:
	s->set_halbit(get_bit_value(vp));
	break;
    case HAL_FLOAT:
	s->set_halfloat(get_float_value(vp));
	break;
    case HAL_S32:
	s->set_hals32(get_s32_value(vp));
	break;
    case HAL_U32:
	s->set_halu32(get_u32_value(vp));
	break;
    }
    return 0;
}

static inline int hal_param2pb(const hal_param_t *pp, machinetalk::Param *p)
{
    const hal_data_u *vp = param_value(pp);

    switch (pp->type) {
    default:
	return -1;
    case HAL_BIT:
	p->set_halbit(get_bit_value(vp));
	break;
    case HAL_FLOAT:
	p->set_halfloat(get_float_value(vp));
	break;
    case HAL_S32:
	p->set_hals32(get_s32_value(vp));
	break;
    case HAL_U32:
	p->set_halu32(get_u32_value(vp));
	break;
    }
    return 0;
}

static inline int hal_pbpin2u(const machinetalk::Pin *p, hal_data_u *vp)
{
    switch (p->type()) {
    default:
	return -1;
    case HAL_BIT:
	set_bit_value(vp, p->halbit());
	break;
    case HAL_FLOAT:
	set_float_value(vp, p->halfloat());
	break;
    case HAL_S32:
	set_s32_value(vp, p->hals32());
	break;
    case HAL_U32:
	set_s32_value(vp, p->hals32());
	break;
    }
    return 0;
}

static inline int hal_pbsig2u(const machinetalk::Signal *s, hal_data_u *vp)
{
    switch (s->type()) {
    default:
	return -1;
    case HAL_BIT:
	set_bit_value(vp, s->halbit());
	break;
    case HAL_FLOAT:
	set_float_value(vp, s->halfloat());
	break;
    case HAL_S32:
	set_s32_value(vp, s->hals32());
	break;
    case HAL_U32:
	set_u32_value(vp, s->halu32());
	break;
    }
    return 0;
}


