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

static int group_report_cb(int phase, hal_compiled_group_t *cgroup,
			   hal_sig_t *sig, void *cb_data);
static int scan_group_cb(hal_group_t *g, void *cb_data);


// monitor group subscribe events:
//
// a new subscriber will cause the next update to be 'full', i.e. with current
// values and including signal names regardless of any change respective to the last scan
//
// this permits a new subscriber to establish the set of signal names immediately as
// well as retrieve all current values without constantly broadcasting all
// signal names
int
handle_group_input(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    htself_t *self = (htself_t *) arg;
    zframe_t *f_subscribe = zframe_recv(poller->socket);
    const char *s = (const char *) zframe_data(f_subscribe);

    if ((s == NULL) || ((*s != '\000') && (*s != '\001'))) {
	// some random message - ignore
	zframe_destroy(&f_subscribe);
	return 0;
    }
    const char *topic = s+1;

    switch (*s) {
    case '\001':   // non-zero: subscribe event

	// adopt and compile any groups defined since startup
	scan_groups(self);

	if (strlen(topic) == 0) {
	    // this was a subscribe("") - all topics
	    // describe all groups
	    for (groupmap_iterator gi = self->groups.begin();
		 gi != self->groups.end(); gi++) {

		group_t *g = gi->second;
		self->tx.set_type(pb::MT_HALGROUP_FULL_UPDATE);
		self->tx.set_uuid(self->netopts.proc_uuid, sizeof(self->netopts.proc_uuid));
		self->tx.set_serial(g->serial++);
		describe_parameters(self);
		describe_group(self, gi->first.c_str(), gi->first.c_str(), poller->socket);

		// if first subscriber: activate scanning
		if (g->timer_id < 0) { // not scanning
		    g->timer_id = zloop_timer(loop, g->msec,
					      0, handle_group_timer, (void *)g);
		    assert(g->timer_id > -1);
		    rtapi_print_msg(RTAPI_MSG_DBG,
				    "%s: start scanning group %s, tid=%d, %d mS, %d members, %d monitored",
				    self->cfg->progname, topic, g->timer_id, g->msec,
				    g->cg->n_members, g->cg->n_monitored);
		}
		rtapi_print_msg(RTAPI_MSG_DBG,
				"%s: wildcard subscribe group='%s' serial=%d",
				self->cfg->progname,
				gi->first.c_str(), gi->second->serial);
	    }
	} else {
	    // a selective subscribe - describe only the desired group
	    groupmap_iterator gi = self->groups.find(topic);
	    if (gi != self->groups.end()) {
		group_t *g = gi->second;
		self->tx.set_type(pb::MT_HALGROUP_FULL_UPDATE);
		self->tx.set_uuid(self->netopts.proc_uuid, sizeof(self->netopts.proc_uuid));
		self->tx.set_serial(g->serial++);
		describe_parameters(self);
		describe_group(self, gi->first.c_str(), gi->first.c_str(), poller->socket);
		rtapi_print_msg(RTAPI_MSG_DBG,
				"%s: subscribe group='%s' serial=%d",
				self->cfg->progname,
				gi->first.c_str(), gi->second->serial);

		// if first subscriber: activate scanning
		if (g->timer_id < 0) { // not scanning
		    g->timer_id = zloop_timer(loop, g->msec,
					      0, handle_group_timer, (void *)g);
		    assert(g->timer_id > -1);
		    rtapi_print_msg(RTAPI_MSG_DBG,
				    "%s: start scanning group %s, tid=%d, %d mS, %d members, %d monitored",
				    self->cfg->progname, topic,
				    g->timer_id, g->msec, g->cg->n_members, g->cg->n_monitored);
		}
	    } else {
		// non-existant topic, complain.
		self->tx.set_type(pb::MT_STP_NOGROUP);
		note_printf(self->tx, "no such group: '%s', currently %d valid groups",
			    topic, self->groups.size());
		if (self->groups.size())
		    note_printf(self->tx, ": ");
		for (groupmap_iterator g = self->groups.begin();
		     g != self->groups.end(); g++) {
		    note_printf(self->tx, "    %s", g->first.c_str());
		}
		int retval = send_pbcontainer(topic, self->tx,
					      self->mksock[SVC_HALGROUP].socket);
		assert(retval == 0);
	    }
	}
	break;

    case '\000':   // last unsubscribe
	if (self->groups.count(topic) > 0) {
	    group_t *g = self->groups[topic];
	    // stop the scanning timer
	    if (g->timer_id > -1) {  // currently scanning
		rtapi_print_msg(RTAPI_MSG_DBG,
				"%s: group %s stop scanning, tid=%d",
				self->cfg->progname, topic, g->timer_id);
		int retval = zloop_timer_end (loop, g->timer_id);
		assert(retval == 0);
		g->timer_id = -1;
	    }
	}
	break;

    default:
	break;
    }
    zframe_destroy(&f_subscribe);
    return 0;
}


// detect if a group needs reporting, and do so
int
handle_group_timer(zloop_t *loop, int timer_id, void *arg)
{
    group_t *g = (group_t *) arg;
    if (hal_cgroup_match(g->cg))
	hal_cgroup_report(g->cg, group_report_cb, g, 0);
    return 0;
}

// walk HAL groups, and compile any which are not in self->groups yet
// idempotent - will add new groups as found
int
scan_groups(htself_t *self)
{
    {   // scoped lock
	int retval __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	// run through all groups, execute a callback for each group found.
	if ((retval = halpr_foreach_group(NULL, scan_group_cb, self)) < 0)
	    return retval;
	rtapi_print_msg(RTAPI_MSG_DBG,"found %d group(s)\n", retval);
    }
    return 0;
}

// ----- end of public functions ----

static int
add_sig_to_items(int level, hal_group_t **groups,
		 hal_member_t *member, void *cb_data)
{
    hal_sig_t *sig = (hal_sig_t *) SHMPTR(member->sig_member_ptr);
    htself_t *self = (htself_t *)cb_data;
    itemmap_iterator it = self->items.find(sig->handle);

    if (it == self->items.end()) { // not in handle map
	halitem_t *hi = new halitem_t();
	hi->type = HAL_SIGNAL;
	hi->o.signal = sig;
	hi->ptr = SHMPTR(sig->data_ptr);
	self->items[sig->handle] = hi;
    }
    return 0;
}

// walk the signals of a group, and add them to the items dict
// for lookup-by-handle if not yet present - idempotent
static int
add_signals_from_group(htself_t *self, const char *name)
{
    return halpr_foreach_member(name, add_sig_to_items, self, RESOLVE_NESTED_GROUPS);
}

static int
scan_group_cb(hal_group_t *g, void *cb_data)
{
    htself_t *self = (htself_t *)cb_data;
    hal_compiled_group_t *cgroup;
    int retval;

    if (self->groups.count(g->name) > 0) // already compiled
	return 0;

    if ((retval = halpr_group_compile(g->name, &cgroup))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"hal_group_compile(%s) failed: %d - skipping group\n",
			g->name, retval);
	return 0;
    }

    add_signals_from_group(self, g->name);

    group_t *grp = new group_t();
    grp->cg = cgroup;
    grp->serial = 0;
    grp->self = self;
    grp->flags = 0;
    grp->timer_id = -1; // not yet scanning
    grp->msec =  hal_cgroup_timer(cgroup);
    if (grp->msec == 0)
	grp->msec = self->cfg->default_group_timer;

    self->groups[g->name] = grp;

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "%s: group '%s' - using %d mS poll interval",
		    self->cfg->progname, g->name, grp->msec);
    return 0;
}

// drop group reference counts
int release_groups(htself_t *self)
{
    int nfail = 0;
    for (groupmap_iterator g = self->groups.begin(); g != self->groups.end(); g++) {
	if (hal_unref_group(g->first.c_str()) < 0)
	    nfail++;
	rtapi_print_msg(RTAPI_MSG_DBG,
			"%s: unreferencing group '%s'",
			self->cfg->progname, g->first.c_str());
    }
    return -nfail;
}

static int
group_report_cb(int phase, hal_compiled_group_t *cgroup,
		hal_sig_t *sig, void *cb_data)
{
    group_t *grp = (group_t *) cb_data;
    htself_t *self = grp->self;
    pb::Signal *signal;
    int retval;

    switch (phase) {

    case REPORT_BEGIN:	// report initialisation
	self->tx.set_type(pb::MT_HALGROUP_INCREMENTAL_UPDATE);
	// the serial enables detection of lost updates
	// for a client to recover from a lost update:
	// unsubscribe + re-subscribe which will cause
	// a full state dump to be sent
	self->tx.set_serial(grp->serial++);
	break;

    case REPORT_SIGNAL: // per-reported-signal action
	signal = self->tx.add_signal();
	signal->set_handle(sig->handle);
	retval = hal_sig2pb(sig, signal);
	assert(retval == 0);
	break;

    case REPORT_END: // finalize & send
	retval = send_pbcontainer(cgroup->group->name, self->tx,
				  self->mksock[SVC_HALGROUP].socket);
	assert(retval == 0);

#if JSON_TIMING
	// timing test:
	try {
	    std::string json = pb2json(self->tx);
	    zframe_t *z_jsonframe = zframe_new( json.c_str(), json.size());
	    //assert(zframe_send(&z_jsonframe, self->z_status, 0) == 0);
	    zframe_destroy(&z_jsonframe);
	} catch (std::exception &ex) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "to_ws: pb2json exception: %s\n",
		     ex.what());
	    // std::string text;
	    // if (TextFormat::PrintToString(c, &text))
	    // 	fprintf(stderr, "container: %s\n", text.c_str());
	}
#endif // JSON_TIMING

	break;
    }
    return 0;
}

// send a keepalive to all group subscribers
int ping_groups(htself_t *self)
{
    for (groupmap_iterator g = self->groups.begin(); g != self->groups.end(); g++) {
	self->tx.set_type(pb::MT_PING);
	int retval = send_pbcontainer(g->first.c_str(), self->tx,
				      self->mksock[SVC_HALGROUP].socket);
	assert(retval == 0);
    }
    return 0;
}
