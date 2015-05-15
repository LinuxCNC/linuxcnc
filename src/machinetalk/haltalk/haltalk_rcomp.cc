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
#include "halpb.hh"
#include "pbutil.hh"

static int collect_unbound_comps(hal_compstate_t *cs,  void *cb_data);
static int comp_report_cb(int phase,  hal_compiled_comp_t *cc,
			  hal_pin_t *pin,
			  hal_data_u *vp,
			  void *cb_data);
static int add_pins_to_items(int phase,  hal_compiled_comp_t *cc,
			     hal_pin_t *pin, hal_data_u *vp, void *cb_data);

// handle timer event for a rcomp - report any changes in comp
int
handle_rcomp_timer(zloop_t *loop, int timer_id, void *arg)
{
    rcomp_t *rc = (rcomp_t *) arg;
    if (hal_ccomp_match(rc->cc))
	hal_ccomp_report(rc->cc, comp_report_cb, rc, rc->flags);
    return 0;
}

// handle message input on the XPUB channel, these would be:
//    subscribe events (\001<topic>), for every subscribe
//    unsubscribe events (\001<topic>), for the last unsubscribe
//    other - any commands sent to the XPUB - dubious how useful this is
int
handle_rcomp_input(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    htself_t *self = (htself_t *) arg;
    int retval;
    zmsg_t *msg = zmsg_recv(poller->socket);
    size_t nframes = zmsg_size( msg);

    if (nframes == 1) {
	// likely a subscribe/unsubscribe message

	zframe_t *f = zmsg_first(msg); // leaves message intact
	char *data = (char *) zframe_data(f);
	assert(data);
	char *topic = data + 1;

	switch (*data) {

	case '\001':

	    // scan for, and adopt any recently created rcomps
	    // so subscribe to them works (after haltalk was started)
	    scan_comps(self);

	    if (self->rcomps.count(topic) == 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: subscribe - no comp '%s'",
				    self->cfg->progname, topic);

		// not found, publish an error message on this topic
		self->tx.set_type(pb::MT_HALRCOMP_ERROR);
		note_printf(self->tx, "component '%s' does not exist", topic);
		retval = send_pbcontainer(topic, self->tx, self->mksock[SVC_HALRCOMP].socket);
		assert(retval == 0);

	    } else {
		// compiled component found, schedule a full update
		rcomp_t *g = self->rcomps[topic];
		self->tx.set_type(pb::MT_HALRCOMP_FULL_UPDATE);
		self->tx.set_uuid(self->netopts.proc_uuid, sizeof(self->netopts.proc_uuid));
		self->tx.set_serial(g->serial++);
		describe_parameters(self);
		describe_comp(self, topic, topic, poller->socket);

		// first subscriber - activate scanning
		if (g->timer_id < 0) { // not scanning
		    g->timer_id = zloop_timer(self->netopts.z_loop, g->msec, 0,
					      handle_rcomp_timer, (void *)g);
		    assert(g->timer_id > -1);
		    rtapi_print_msg(RTAPI_MSG_DBG,
				    "%s: start scanning comp %s, tid=%d, %d mS, %d pins tracked",
				    self->cfg->progname, topic, g->timer_id, g->msec, g->cc->n_pins);
		}

		if (g->cc->comp->state == COMP_UNBOUND) {
		    // once only by first subscriber
		    hal_bind(topic);
		    rtapi_print_msg(RTAPI_MSG_DBG, "%s: %s bound, serial=%d",
				    self->cfg->progname, topic, g->serial);
		} else
		    rtapi_print_msg(RTAPI_MSG_DBG, "%s: %s subscribed, serial=%d",
				    self->cfg->progname, topic, g->serial);
	    }
	    break;

	case '\000':
	    // last subscriber went away - unbind the component
	    if (self->rcomps.count(topic) > 0) {
		rcomp_t *g = self->rcomps[topic];

		// stop the scanning timer
		if (g->timer_id > -1) {  // currently scanning
		    rtapi_print_msg(RTAPI_MSG_DBG, "%s: stop scanning comp %s, tid=%d",
				    self->cfg->progname, topic, g->timer_id);
		    retval = zloop_timer_end (loop, g->timer_id);
		    assert(retval == 0);
		    g->timer_id = -1;
		}
		hal_unbind(topic);
		rtapi_print_msg(RTAPI_MSG_DBG, "%s: %s unbound",
				self->cfg->progname, topic);
	    }
	    break;

	default:
	    zmsg_destroy(&msg);
	}
	return 0;
    } else {

	zmsg_destroy(&msg);
    }
    return 0;
}


int
scan_comps(htself_t *self)
{
    int retval;
    int nfail = 0;

    // this needs to be done in two steps due to HAL locking:
    // 1. collect remote component names and populate dict keys
    // 2. acquire and compile remote components
    hal_retrieve_compstate(NULL, collect_unbound_comps, self);

    for (compmap_iterator c = self->rcomps.begin();
	 c != self->rcomps.end(); c++) {

	if (c->second != NULL) // already compiled
	    continue;

	const char *name = c->first.c_str();

	if ((retval = hal_acquire(name, self->pid)) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: hal_acquire(%s) failed: %s",
			    self->cfg->progname,
			    name, strerror(-retval));
	    nfail++;
	    continue;
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "%s: acquired '%s'",
			self->cfg->progname, name);

	hal_compiled_comp_t *cc;
	if ((retval = hal_compile_comp(name, &cc))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: scan_comps:hal_compile_comp(%s) failed - skipping component: %s",
			    self->cfg->progname,
			    name, strerror(-retval));
	    nfail++;
	    self->rcomps.erase(c);
	    continue;
	}
	// add pins to items dict
	hal_ccomp_report(cc, add_pins_to_items, self, true);

	int arg1, arg2;
	hal_ccomp_args(cc, &arg1, &arg2);
	int msec = arg1 ? arg1 : self->cfg->default_rcomp_timer;

	rcomp_t *rc = new rcomp_t();
	rc->flags = 0;
	rc->self = self;
	rc->cc = cc;
	rc->serial = 0;
	rc->msec = msec;
	rc->timer_id = -1; // invalid

	self->rcomps[name] = rc; // all prepared, timer not yet started

	rtapi_print_msg(RTAPI_MSG_DBG, "%s: component '%s' - using %d mS poll interval",
			self->cfg->progname, name, msec);
    }
    return nfail;
}

int release_comps(htself_t *self)
{
    int nfail = 0, retval;

    for (compmap_iterator c = self->rcomps.begin();
	 c != self->rcomps.end(); c++) {

	const char *name = c->first.c_str();
	rcomp_t *rc = c->second;
	if (rc->cc == NULL)
	    // remote created, but never bound and hence
	    // not compiled, bound and aquired
	    continue;
	hal_comp_t *comp = rc->cc->comp;

	// unbind all comps owned by us:
	if (comp->state == COMP_BOUND) {
	    if (comp->pid == self->pid) {
		retval = hal_unbind(name);
		if (retval < 0) {
		    rtapi_print_msg(RTAPI_MSG_ERR,
				    "%s: hal_unbind(%s) failed: %s",
				    self->cfg->progname,
				    name, strerror(-retval));
		    nfail++;
		} else
		    rtapi_print_msg(RTAPI_MSG_ERR,
				    "%s: unbound component '%s'",
				    self->cfg->progname, name);
	    } else {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"%s: BUG - comp %s bound but not by haltalk: %d/%d",
				self->cfg->progname, name, self->pid, comp->pid);
		nfail++;
	    }
	}
	if (comp->pid != 0) {
	    int retval = hal_release(name);
	    if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"%s: hal_release(%s) failed: %s",
				self->cfg->progname,
				name, strerror(-retval));
		nfail++;
	    } else
		rtapi_print_msg(RTAPI_MSG_ERR,
				"%s: released component '%s'",
				self->cfg->progname, name);
	}
    }
    return -nfail;
}

// ----- end of public functions ----

static int
collect_unbound_comps(hal_compstate_t *cs,  void *cb_data)
{
    htself_t *self = (htself_t *) cb_data;;

    // collect any unbound, un-aquired remote comps
    // which we dont know about yet
    if ((cs->type == TYPE_REMOTE) &&
	(cs->pid == 0) &&
	(cs->state == COMP_UNBOUND) &&
	(self->rcomps.count(cs->name) == 0)) {

	self->rcomps[cs->name] = NULL;

	rtapi_print_msg(RTAPI_MSG_DBG, "%s: found unbound remote comp '%s'",
			self->cfg->progname, cs->name);
    }
    return 0;
}


static
int comp_report_cb(int phase,  hal_compiled_comp_t *cc,
		   hal_pin_t *pin,
		   hal_data_u *vp,
		   void *cb_data)
{
    rcomp_t *rc = (rcomp_t *) cb_data;
    htself_t *self =  rc->self;
    pb::Pin *p;
    int retval;

    switch (phase) {

    case REPORT_BEGIN:	// report initialisation
	self->tx.set_type(pb::MT_HALRCOMP_INCREMENTAL_UPDATE);
	self->tx.set_serial(rc->serial++);
	break;

    case REPORT_PIN: // per-reported-pin action
	p = self->tx.add_pin();
	p->set_handle(pin->handle);
	if (hal_pin2pb(pin, p))
		rtapi_print_msg(RTAPI_MSG_ERR, "bad type %d for pin '%s'\n",
				pin->type, pin->name);
	break;

    case REPORT_END: // finalize & send
	retval = send_pbcontainer(cc->comp->name, self->tx, self->mksock[SVC_HALRCOMP].socket);
	assert(retval == 0);
	break;
    }
    return 0;
}

static int
add_pins_to_items(int phase,  hal_compiled_comp_t *cc,
		  hal_pin_t *pin,
		  hal_data_u *vp,
		  void *cb_data)
{
    if (phase != REPORT_PIN) return 0;

    htself_t *self = (htself_t *) cb_data;;
    itemmap_iterator it = self->items.find(pin->handle);

    if (it == self->items.end()) { // not in handle map
	halitem_t *hi = new halitem_t();
	hi->type = HAL_PIN;
	hi->o.pin = pin;
	hi->ptr = SHMPTR(pin->data_ptr_addr);
	self->items[pin->handle] = hi;
    }
    return 0;
}

// send a keepalive to all comp subscribers
int ping_comps(htself_t *self)
{
    for (compmap_iterator c = self->rcomps.begin();
	 c != self->rcomps.end(); c++) {
	self->tx.set_type(pb::MT_PING);
	int retval = send_pbcontainer(c->first.c_str(), self->tx,
				      self->mksock[SVC_HALRCOMP].socket);
	assert(retval == 0);
    }
    return 0;
}
