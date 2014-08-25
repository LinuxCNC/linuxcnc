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
#include <machinetalk/generated/message.pb.h>

// in halpb.cc:
int halpr_describe_signal(hal_sig_t *sig, pb::Signal *pbsig);
int halpr_describe_pin(hal_pin_t *pin, pb::Pin *pbpin);
int halpr_describe_ring(hal_ring_t *ring, pb::Ring *pbring);
int halpr_describe_funct(hal_funct_t *funct, pb::Function *pbfunct);
int halpr_describe_thread(hal_thread_t *thread, pb::Thread *pbthread);
int halpr_describe_component(hal_comp_t *comp, pb::Component *pbcomp);
int halpr_describe_group(hal_group_t *g, pb::Group *pbgroup);
int halpr_describe_member(hal_member_t *member, pb::Member *pbmember);


static inline const hal_data_u *hal_sig2u(const hal_sig_t *sig)
{
    return (hal_data_u *)SHMPTR(sig->data_ptr);
}

static inline const hal_data_u *hal_pin2u(const hal_pin_t *pin)
{
    const hal_sig_t *sig;
    if (pin->signal != 0) {
	sig = (const hal_sig_t *) SHMPTR(pin->signal);
	return (hal_data_u *)SHMPTR(sig->data_ptr);
    } else
	return (hal_data_u *)(hal_shmem_base + SHMOFF(&(pin->dummysig)));
}

static inline const hal_data_u *hal_param2u(const hal_param_t *param)
{
    return (hal_data_u *)SHMPTR(param->data_ptr);
}

static inline int hal_pin2pb(const hal_pin_t *hp, pb::Pin *p)
{
    const hal_data_u *vp  = hal_pin2u(hp);
    switch (hp->type) {
    default:
	return -1;
    case HAL_BIT:
	p->set_halbit(vp->b);
	break;
    case HAL_FLOAT:
	p->set_halfloat(vp->f);
	break;
    case HAL_S32:
	p->set_hals32(vp->s);
	break;
    case HAL_U32:
	p->set_halu32(vp->u);
	break;
    }
    return 0;
}

static inline int hal_sig2pb(const hal_sig_t *sp, pb::Signal *s)
{
    const hal_data_u *vp = hal_sig2u(sp);
    switch (sp->type) {
    default:
	return -1;
    case HAL_BIT:
	s->set_halbit(vp->b);
	break;
    case HAL_FLOAT:
	s->set_halfloat(vp->f);
	break;
    case HAL_S32:
	s->set_hals32(vp->s);
	break;
    case HAL_U32:
	s->set_halu32(vp->u);
	break;
    }
    return 0;
}

static inline int hal_param2pb(const hal_param_t *pp, pb::Param *p)
{
    const hal_data_u *vp = hal_param2u(pp);

    switch (pp->type) {
    default:
	return -1;
    case HAL_BIT:
	p->set_halbit(vp->b);
	break;
    case HAL_FLOAT:
	p->set_halfloat(vp->f);
	break;
    case HAL_S32:
	p->set_hals32(vp->s);
	break;
    case HAL_U32:
	p->set_halu32(vp->u);
	break;
    }
    return 0;
}

static inline int hal_pbpin2u(const pb::Pin *p, hal_data_u *vp)
{
    switch (p->type()) {
    default:
	return -1;
    case HAL_BIT:
	vp->b = p->halbit();
	break;
    case HAL_FLOAT:
	vp->f = p->halfloat();
	break;
    case HAL_S32:
	vp->s = p->hals32();
	break;
    case HAL_U32:
	vp->u = p->halu32();
	break;
    }
    return 0;
}

static inline int hal_pbsig2u(const pb::Signal *s, hal_data_u *vp)
{
    switch (s->type()) {
    default:
	return -1;
    case HAL_BIT:
	vp->b = s->halbit();
	break;
    case HAL_FLOAT:
	vp->f = s->halfloat();
	break;
    case HAL_S32:
	vp->s = s->hals32();
	break;
    case HAL_U32:
	vp->u = s->halu32();
	break;
    }
    return 0;
}


