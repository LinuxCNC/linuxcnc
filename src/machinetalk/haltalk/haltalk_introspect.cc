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
#include "hal_iter.h"
#include "halpb.hh"
#include "pbutil.hh"
#include "rtapi_hexdump.h"

#include <google/protobuf/text_format.h>

static int describe_comp_cb(hal_comp_t *comp,  void *arg);
static int describe_sig_cb(hal_sig_t *sig,  void *arg);
static int describe_funct_cb(hal_funct_t *funct,  void *arg);
static int describe_ring_cb(hal_ring_t *ring,  void *arg);
static int describe_thread_cb(hal_thread_t *thread,  void *arg);
static int describe_group_cb(hal_group_t *group,  void *arg);

// describe the current HAL universe as a protobuf message.
int
process_describe(htself_t *self, const std::string &from,  void *socket)
{
    int retval __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));

    halpr_foreach_comp(NULL, describe_comp_cb, self);
    halpr_foreach_sig(NULL, describe_sig_cb, self);
    halpr_foreach_group(NULL, describe_group_cb, self);
    halpr_foreach_funct(NULL, describe_funct_cb, self);
    halpr_foreach_ring(NULL, describe_ring_cb, self);
    halpr_foreach_thread(NULL, describe_thread_cb, self);
    return send_pbcontainer(from, self->tx, socket);
}

// describe a HAL group as a protobuf message.
int
describe_group(htself_t *self, const char *group, const std::string &from,  void *socket)
{
    int retval __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));

    hal_group_t *g = halpr_find_group_by_name(group);
    if (g == NULL) {
	self->tx.set_type(pb::MT_HALRCOMP_ERROR);
	note_printf(self->tx, "no such group: '%s'", group);
	return send_pbcontainer(from, self->tx, socket);
    }
    halpr_foreach_group(group, describe_group_cb, self);
    return send_pbcontainer(from, self->tx, socket);
}

// describe a HAL component as a protobuf message.
int
describe_comp(htself_t *self, const char *comp, const std::string &from,  void *socket)
{
    int retval __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));

    hal_comp_t *c = halpr_find_comp_by_name(comp);
    if (c == NULL) {
	self->tx.set_type(pb::MT_HALRCOMP_ERROR);
	note_printf(self->tx, "no such component: '%s'", comp);
	return send_pbcontainer(from, self->tx, socket);
    }
    halpr_foreach_comp(comp, describe_comp_cb, self);
    return send_pbcontainer(from, self->tx, socket);
}

// add protocol parameters the subscriber might want to know about
int describe_parameters(htself_t *self)
{
    pb::ProtocolParameters *pp = self->tx.mutable_pparams();
    pp->set_keepalive_timer(self->cfg->keepalive_timer);
    pp->set_group_timer(self->cfg->default_group_timer);
    pp->set_rcomp_timer(self->cfg->default_rcomp_timer);
    return 0;
}

// ----- end of public functions ---

static int describe_comp_cb(hal_comp_t *comp,  void *arg)
{
    htself_t *self = (htself_t *) arg;
    pb::Component *c = self->tx.add_comp();
    halpr_describe_component(comp, c);
    return 0;
}

static int describe_sig_cb(hal_sig_t *sig,  void *arg)
{
    htself_t *self = (htself_t *) arg;
    pb::Signal *s = self->tx.add_signal();
    halpr_describe_signal(sig, s);
    return 0;
}

static int describe_group_cb(hal_group_t *g,  void *arg)
{
    htself_t *self = (htself_t *) arg;
    pb::Group *pbgroup = self->tx.add_group();
    halpr_describe_group(g, pbgroup);
    return 0;
}

static int describe_funct_cb(hal_funct_t *funct,  void *arg)
{
    htself_t *self = (htself_t *) arg;
    pb::Function *f = self->tx.add_function();
    halpr_describe_funct(funct, f);
    return 0;
}

static int describe_ring_cb(hal_ring_t *ring,  void *arg)
{
    htself_t *self = (htself_t *) arg;
    pb::Ring *r = self->tx.add_ring();
    halpr_describe_ring(ring, r);
    return 0;
}

static int describe_thread_cb(hal_thread_t *thread,  void *arg)
{
    htself_t *self = (htself_t *) arg;
    pb::Thread *t = self->tx.add_thread();
    halpr_describe_thread(thread, t);
    return 0;
}
