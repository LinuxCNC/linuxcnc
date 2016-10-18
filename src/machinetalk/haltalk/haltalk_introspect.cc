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

#include "haltalk.hh"
#include "hal_logging.h"
#include "halpb.hh"
#include "pbutil.hh"
#include "rtapi_hexdump.h"

#include <google/protobuf/text_format.h>

static int describe_toplevel_object_cb(hal_object_ptr o,
				       foreach_args_t *args);


// add HAL objects recursively to a Container
// reflecting the object hierarchy
int halg_object2pb(const bool use_hal_mutex,
		   machinetalk::Container *msg,
		   const char *name,     // selector or NULL
		   const int type,       // selector or 0
		   const int id)         // selector or 0
{
    WITH_HAL_MUTEX_IF(use_hal_mutex);
    foreach_args_t args = {};
    args.name = (char *)name;
    args.type = type;
    args.id = id;
    args.user_ptr1 =  (void *) msg;

    return halg_foreach(use_hal_mutex, &args, describe_toplevel_object_cb);
}

// describe the current HAL universe as a protobuf message.
int
process_describe(htself_t *self,
		 zmsg_t *from,
		 void *socket)
{
    WITH_HAL_MUTEX();
    halg_object2pb(0, &self->tx, NULL, 0, 0);
    return send_pbcontainer(from, self->tx, socket);
}


// describe a HAL group as a protobuf message.
int
describe_group(htself_t *self,
	       const char *group,
	       const std::string &from,
	       void *socket)
{
    WITH_HAL_MUTEX();
    int ret = halg_object2pb(0, &self->tx, group, HAL_GROUP, 0);
    if (ret != 1)  {
	self->tx.set_type(machinetalk::MT_HALRCOMP_ERROR);
	note_printf(self->tx, "no such group: '%s'", group);
	return send_pbcontainer(from, self->tx, socket);
    }
    return send_pbcontainer(from, self->tx, socket);
}


// describe a HAL component as a protobuf message.
int
describe_comp(htself_t *self,
	      const char *comp,
	      const std::string &from,
	      void *socket)
{
    WITH_HAL_MUTEX();
    int ret = halg_object2pb(0, &self->tx, comp, HAL_COMPONENT, 0);
    if (ret != 1)  {
	self->tx.set_type(machinetalk::MT_HALRCOMP_ERROR);
	note_printf(self->tx, "no such component: '%s'", comp);
	return send_pbcontainer(from, self->tx, socket);
    }
    return send_pbcontainer(from, self->tx, socket);
}

// add protocol parameters the subscriber might want to know about
int describe_parameters(htself_t *self)
{
    machinetalk::ProtocolParameters *pp = self->tx.mutable_pparams();
    pp->set_keepalive_timer(self->cfg->keepalive_timer);
    pp->set_group_timer(self->cfg->default_group_timer);
    pp->set_rcomp_timer(self->cfg->default_rcomp_timer);
    return 0;
}


// ----- end of public functions ---

static int describe_toplevel_object_cb(hal_object_ptr o,
				       foreach_args_t *args)
{
    if (!hh_is_toplevel(hh_get_object_type(o.hdr)))
	return 0;

    machinetalk::Container *msg = (machinetalk::Container *) args->user_ptr1;

    switch (hh_get_object_type(o.hdr)) {

    case HAL_SIGNAL:
	return halpr_describe_signal(o.sig, msg->add_signal());
	break;

    case HAL_THREAD:
	return halpr_describe_thread(o.thread, msg->add_thread());
	break;

    case HAL_COMPONENT:
	return halpr_describe_component(o.comp,msg->add_comp());
	break;

    case HAL_VTABLE:
	// TBD
	break;

    case HAL_RING:
	return halpr_describe_ring(o.ring, msg->add_ring());
	break;

    case HAL_GROUP:
	return halpr_describe_group(o.group, msg->add_group());
	break;

    default:
	HALBUG("%s object %s  not supported",
	       hh_get_object_typestr(o.hdr),
	       hh_get_name(o.hdr));
    }
    return 0; // continue
}
