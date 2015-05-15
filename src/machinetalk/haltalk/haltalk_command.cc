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
#include "rtapi_hexdump.h"

#include <google/protobuf/text_format.h>

static int dispatch_request(htself_t *self, const std::string &from, void *socket);
static int process_get(htself_t *self, const std::string &from, void *socket);
static int process_set(htself_t *self, bool halrcomp, const std::string &from, void *socket);
static int describe_pin_by_name(htself_t *self, const char *name);
static int describe_signal_by_name(htself_t *self, const char *name);
static int apply_initial_values(htself_t *self, const pb::Component *pbcomp);

int
handle_command_input(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    int retval = 0;

    htself_t *self = (htself_t *) arg;
    zmsg_t *msg = zmsg_recv(poller->socket);

    if (self->cfg->debug  > 4)
	zmsg_dump(msg);

    zframe_t *o = zmsg_pop (msg);
    std::string origin( (const char *) zframe_data(o), zframe_size(o));

    size_t nframes = zmsg_size( msg);

    for(size_t i = 0; i < nframes; i++) {
	zframe_t *f = zmsg_pop (msg);

	if (!self->rx.ParseFromArray(zframe_data(f), zframe_size(f))) {
	    rtapi_print_hex_dump(RTAPI_MSG_ALL, RTAPI_DUMP_PREFIX_OFFSET,
				 16, 1, zframe_data(f), zframe_size(f), true,
				 "%s: invalid pb ", origin.c_str());
	} else {
	    if (self->cfg->debug) {
		std::string s;
		gpb::TextFormat::PrintToString(self->rx, &s);
		fprintf(stderr,"%s: req=%s\n",__func__,s.c_str());
	    }
	    // a valid protobuf. Interpret and reply as needed.
	    dispatch_request(self, origin, poller->socket);
	}
	zframe_destroy(&f);
    }
    zmsg_destroy(&msg);
    zframe_destroy(&o);

    return retval;
}

// ----- end of public functions ---

static int
process_ping(htself_t *self, const std::string &from,  void *socket)
{
    self->tx.set_type( pb::MT_PING_ACKNOWLEDGE);
    self->tx.set_uuid(&self->netopts.proc_uuid, sizeof(uuid_t));
    return send_pbcontainer(from, self->tx, socket);
}

// validate name, number, type and direction of pins and params
// of the existing HAL component 'name' against the component described in
// pb::Component c.
// any errors are added as c.note strings.
// Returns the number of notes added (= errors).
// Acquires the HAL mutex.
static int
validate_component(const char *name, const pb::Component *pbcomp, pb::Container &e)
{
    hal_pin_t *hp __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));

    hal_comp_t *hc = halpr_find_comp_by_name(name);
    if (hc == NULL) {
	note_printf(e, "HAL component '%s' does not exist", name);
	return e.note_size();
    }

    int npins = halpr_pin_count(name);
    int nparams  = halpr_param_count(name);
    int npbpins = pbcomp->pin_size();
    int npbparams = pbcomp->param_size();
    std::string s;

    if (!pbcomp->has_name())
	note_printf(e, "pb component has no name");

    if (npbpins != npins)
	note_printf(e, "pin count mismatch:pb comp=%d hal comp=%d",
		    npbpins, npins);

    if (npbparams != nparams)
	note_printf(e, "param count mismatch:pb comp=%d hal comp=%d",
		    npbparams, nparams);

    for (int i = 0; i < npbpins; i++) {

	const pb::Pin &p = pbcomp->pin().Get(i);;

	// basic syntax - required attributes
	if (!p.has_name()) {
	    gpb::TextFormat::PrintToString(p, &s);
	    note_printf(e, "pin without name: %s", s.c_str());
	    continue;
	}
	if (!p.has_type()) {
	    gpb::TextFormat::PrintToString(p, &s);
	    note_printf(e, "pin without type: %s", s.c_str());
	    continue;
	}
	if (!p.has_dir()) {
	    gpb::TextFormat::PrintToString(p, &s);
	    note_printf(e, "pin without dir: %s", s.c_str());
	    continue;
	}

	// each pb pin must match an existing HAL pin
	hal_pin_t *hp = halpr_find_pin_by_name(p.name().c_str());
	if (hp == NULL) {
	    note_printf(e, "HAL pin '%s' does not exist", p.name().c_str());
	} else {
	    // HAL pin name exists, match attributes
	    if (hp->type != (hal_type_t) p.type())
		note_printf(e, "HAL pin '%s' type mismatch: hal=%d pb=%d",
			    p.name().c_str(), hp->type, p.type());

	    if (hp->dir != (hal_pin_dir_t) p.dir())
		note_printf(e, "HAL pin '%s' direction mismatch: hal=%d pb=%d",
			    p.name().c_str(), hp->dir, p.dir());
	}
    }
    // same for params:
    for (int i = 0; i < npbparams; i++) {

	const pb::Param &p = pbcomp->param().Get(i);;

	// basic syntax - required attributes
	if (!p.has_name()) {
	    gpb::TextFormat::PrintToString(p, &s);
	    note_printf(e, "param withtout name: %s", s.c_str());
	    continue;
	}
	if (!p.has_type()) {
	    gpb::TextFormat::PrintToString(p, &s);
	    note_printf(e, "param withtout type: %s", s.c_str());
	    continue;
	}
	if (!p.has_dir()) {
	    gpb::TextFormat::PrintToString(p, &s);
	    note_printf(e, "param withtout direction: %s", s.c_str());
	    continue;
	}

	// each pb param must match an existing HAL param
	hal_param_t *hp = halpr_find_param_by_name(p.name().c_str());
	if (hp == NULL) {
	    note_printf(e, "HAL param '%s' does not exist", p.name().c_str());
	} else {
	    // HAL param name exists, match attributes
	    if (hp->type != (hal_type_t) p.type())
		note_printf(e, "HAL param '%s' type mismatch: hal=%d pb=%d",
			    hp->type, p.type());

	    if (hp->dir != (hal_param_dir_t) p.dir())
		note_printf(e, "HAL param '%s' direction mismatch: hal=%d pb=%d",
			    hp->dir, p.dir());
	}
    }
    // this matching on pb objects only will not explicitly
    // enumerate HAL pins and params which are not in the pb request,
    // but the balance mismatch will have already been recorded
    return e.note_size();
}

// create a remote comp as per MT_HALRCOMP_BIND request message contents
// The Component submessage is assumed to exist and carry all required fields.
// compile and return a handle to the rcomp descriptor
// the rcomp will be taken into service once its name is subscribed to.
// accumulate any errors in self->tx.note.
static rcomp_t *
create_rcomp(htself_t *self,  const pb::Component *pbcomp,
	     const std::string &from, void *socket)
{
    int arg1 = 0, arg2 = 0, retval;
    rcomp_t *rc = new rcomp_t();
    int comp_id = 0;
    halitem_t *hi = NULL;
    const char *cname = pbcomp->name().c_str();

    rc->self = self;
    rc->timer_id = -1;
    rc->serial = 0;
    rc->flags = 0;
    rc->cc = NULL;

    // extract timer and userargs if set
    if (pbcomp->has_timer())
	rc->msec = pbcomp->timer();
    else
	rc->msec = self->cfg->default_rcomp_timer;

    if (pbcomp->has_userarg1()) arg1 = pbcomp->userarg1();
    if (pbcomp->has_userarg2()) arg2 = pbcomp->userarg2();

    // create the remote component
    comp_id = hal_xinit(TYPE_REMOTE, arg1, arg2, NULL, NULL, cname);
    if (comp_id < 0) {
	note_printf(self->tx, "hal_init_mode(%s): %s",
		    cname, strerror(-comp_id));
	goto ERROR;
    }

    // create the pins
    for (int i = 0; i < pbcomp->pin_size(); i++) {
	const pb::Pin &p = pbcomp->pin(i);
	hi = new halitem_t();
	if (hi == NULL) {
	    note_printf(self->tx, "new halitem_t() failed");
	    goto EXIT_COMP;
	}
	// storage for the pin
	hi->ptr = hal_malloc(sizeof(void *));
	if (hi->ptr == NULL) {
	    note_printf(self->tx,"hal_malloc() failed");
	    goto EXIT_COMP;
	}
	hi->type = HAL_PIN;
	retval = hal_pin_new(p.name().c_str(),
			     (hal_type_t) p.type(),
			     (hal_pin_dir_t) p.dir(),
			     (void **) hi->ptr,
			     comp_id);
	if (retval < 0) {
	    note_printf(self->tx, "hal_pin_new() failed");
	    goto EXIT_COMP;
	}
	hi->o.pin = hal_find_pin_by_name(p.name().c_str());
	if (hi->o.pin == NULL) {
	    note_printf(self->tx, "hal_find_pin_by_name() failed");
	    goto EXIT_COMP;
	}
	// add to items sparse array - needed for quick
	// lookup when updates by handle are received
	self->items[hi->o.pin->handle] = hi;
    }
    hal_ready(comp_id); // XXX check return value

    if ((retval = hal_acquire(cname, getpid())) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: create_rcomp:hal_acquire(%s)"
			" failed - skipping component: %s",
			self->cfg->progname,
			cname, hal_lasterror());
	note_printf(self->tx, "hal_acquire(%s) failed: %s",cname, hal_lasterror());
	goto EXIT_COMP;
    }

    // compile the component
    hal_compiled_comp_t *cc;
    if ((retval = hal_compile_comp(cname, &cc))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: create_rcomp:hal_compile_comp(%s)"
			" failed - skipping component: %s",
			self->cfg->progname,
			cname, strerror(-retval));
	note_printf(self->tx, "hal_compile_comp() failed");
	goto EXIT_COMP;
    }
    rc->cc = cc;
    return rc;

 EXIT_COMP:
    if (hi) delete hi;
    if (rc->cc)
	hal_ccomp_free(cc);
    if (rc) delete rc;
    if (comp_id > 0)
	hal_exit(comp_id);
 ERROR:
    return NULL;
}

static int
process_rcomp_bind(htself_t *self, const std::string &from,
		   const pb::Component *pbcomp, void *socket)
{
    int retval = 0;
    const char *cname = NULL;
    rcomp_t *rc;
    std::string s;

    // assume failure until proven otherwise
    self->tx.set_type( pb::MT_HALRCOMP_BIND_REJECT);
    self->tx.set_uuid(&self->netopts.proc_uuid, sizeof(uuid_t));

    // fail if comp.name not present
    if (!pbcomp->has_name()) {
	note_printf(self->tx, "request %d from '%s': no name in Component submessage",
		    self->rx.type(), from.c_str());
	return send_pbcontainer(from, self->tx, socket);
    }

    cname = pbcomp->name().c_str();

    // validate pinlist attributes if pins are present -
    // to create a pin, it must have, name, type, direction
    for (int i = 0; i < pbcomp->pin_size(); i++) {
	const pb::Pin &p = pbcomp->pin(i);
	if (!(p.has_name() &&
	      p.has_type() &&
	      p.has_dir())) {

	    // TODO if (type < HAL_BIT || type > HAL_U32)
	    gpb::TextFormat::PrintToString(p, &s);
	    note_printf(self->tx,
			"request %d from %s: invalid pin - name, type or dir missing: Pin=(%s)",
			self->rx.type(), from.c_str(), s.c_str());
	}
    }
    // reply if any bad news so far
    if (self->tx.note_size() > 0)
	return send_pbcontainer(from, self->tx, socket);

    // see if component already exists
    if (self->rcomps.count(cname) == 0) {
       // check if any rcomps defined in HAL since startup
       scan_comps(self);
    }

    if (self->rcomps.count(cname) == 0) {
        
    // fail if no_create flag is set in Component submessage
    // meaning: bind succeeds only if the component exists
    if (pbcomp->has_no_create() && pbcomp->no_create()) {
    note_printf(self->tx,
            "request %d from '%s': Component not created since no_create flag set",
            self->rx.type(), from.c_str());
    return send_pbcontainer(from, self->tx, socket);
    }

	// see if component already exists
	// there might be a comp but user might have
	// left out the 'ready <compname>' step or forgotten to call 'hal_ready()'
	// in which case the comp will be in state COMP_INITIALIZING
	int compstate = hal_comp_state_by_name(cname);
	if (compstate == COMP_INITIALIZING) {

	    note_printf(self->tx, "component '%s' exists but has state COMP_INITIALIZING", cname);
	    note_printf(self->tx, "this could be caused by a missing hal_ready() call or "
			"a missing 'ready <compname>' halcmd statement");
	    return send_pbcontainer(from, self->tx, socket);
	}

	// still no, new component being created remotely
	// any errors accumulate in self->tx.note
	rc = create_rcomp(self, pbcomp, from, socket);
	if (rc) {
	    self->rcomps[cname] = rc;
	    // acquire and bind happens during subscribe
	}
    } else {
	// component exists
	rc = self->rcomps[cname];
	// validate request against existing comp
	retval = validate_component(cname, pbcomp, self->tx);
	if (retval) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: bind request from %s:"
			    " mismatch against existing HAL component",
			    self->cfg->progname, from.c_str());
	    return send_pbcontainer(from, self->tx, socket);
	}

	// decide here if we want to carry over pin/param values
	// passed in the BIND request.
	// one possible route is to set pins only if the comp
	// is not currently bound, and was never bound before; this is made conditional
	// on a flag in the comp userarg2 so its optional and must be set
	// explicitly
	//
	// purpose: apply initial values from UI widgets
	// together with the waitbound halcmd operation this assures all values
	// are set up once waitbound finishes
	//
	hal_comp_t *c = rc->cc->comp;
	if ((c->userarg2 & RCOMP_ACCEPT_VALUES_ON_BIND) &&   // option set
	    (c->last_bound == 0) &&                          // never bound before
	    (c->state == COMP_UNBOUND)) {                    // currently unbound
	    rtapi_print_msg(RTAPI_MSG_DBG,
			    "%s: comp %s first bind, accepting initial pin values from BIND request",
			    self->cfg->progname, c->name);
	    if (apply_initial_values(self, pbcomp))
		return send_pbcontainer(from, self->tx, socket);
	}
    }
    // all good.
    if (rc) {
	// a valid component, either existing or new.

	pb::Component *c  __attribute__((cleanup(halpr_autorelease_mutex))) = self->tx.add_comp();
	rtapi_mutex_get(&(hal_data->mutex));
	hal_comp_t *comp = halpr_find_comp_by_name(cname);
	assert(comp != NULL);
	self->tx.set_type(pb::MT_HALRCOMP_BIND_CONFIRM);
	self->tx.set_uuid(&self->netopts.proc_uuid, sizeof(uuid_t));
	retval = halpr_describe_component(comp, c);
	assert(retval == 0);
    }
    return send_pbcontainer(from, self->tx, socket);
}

static int
dispatch_request(htself_t *self, const std::string &from,  void *socket)
{
    int retval = 0;
    pb::ContainerType type = self->rx.type();

    // rtapi_print_msg(RTAPI_MSG_INFO, "%s: rcommand type %d",
    // 		    self->cfg->progname, type);
    switch (type) {

    case pb::MT_PING:
	retval = process_ping(self, from, socket);
	break;

    case pb::MT_HALRCOMP_BIND:
	// check for component submessages, and fail if none present
	if (self->rx.comp_size() == 0) {
	    note_printf(self->tx, "request %d from '%s': no Component submessage",
			self->rx.type(), from.c_str());
	    return send_pbcontainer(from, self->tx, socket);
	}
	// bind them all
	for (int i = 0; i < self->rx.comp_size(); i++) {
	    const pb::Component *pbcomp = &self->rx.comp(i);
	    retval = process_rcomp_bind(self, from, pbcomp,  socket);
	}
	break;

    // HAL object set/get ops
    case pb::MT_HALRCOMMAND_SET:
    case pb::MT_HALRCOMP_SET:
	// XXX: param missing
	retval = process_set(self, type == pb::MT_HALRCOMP_SET, from, socket);
	break;

    case pb::MT_HALRCOMMAND_GET:
	// XXX: param missing
	retval = process_get(self, from, socket);
	break;

    case pb::MT_HALRCOMMAND_DESCRIBE:
	self->tx.set_type(pb::MT_HALRCOMMAND_DESCRIPTION);
	retval = process_describe(self, from, socket);
	break;

	// NIY - fall through:
    case pb::MT_HALRCOMMAND_CREATE:
    case pb::MT_HALRCOMMAND_DELETE:
    default:
	self->tx.set_type(pb::MT_HALRCOMMAND_ERROR);
	note_printf(self->tx, "rcommand %d: not implemented", self->rx.type());
	return send_pbcontainer(from, self->tx, socket);
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: rcommand from %s : unhandled type %d",
			self->cfg->progname, from.c_str(), (int) self->rx.type());
	retval = -1;
    }
    return retval;
}

static int
process_set(htself_t *self, bool halrcomp, const std::string &from,  void *socket)
{
    itemmap_iterator it;

    // work the pins
    for (int i = 0; i < self->rx.pin_size(); i++) {
	const pb::Pin &p = self->rx.pin(i);
	// required fields
	if (!p.has_type()) {
	    note_printf(self->tx,
			"type missing in pin: handle=%d", p.handle());
	    continue;
	}
	// value present?
	if (!(p.has_halfloat() ||
	      p.has_halbit() ||
	      p.has_halu32() ||
	      p.has_hals32())) {
	    note_printf(self->tx,
			"value missing in pin: handle=%d", p.handle());
	    continue;
	}
	// try fast path via item dict first
	if (p.has_handle()) {
	    int handle = p.handle();
	    it = self->items.find(handle);

	    if (it != self->items.end()) {
		// handle present and found
		halitem_t *hi = it->second;
		if (hi->type != HAL_PIN) {
		    note_printf(self->tx,
				"handle type mismatch - not a pin: handle=%d type=%d",
				handle, hi->type);
		    continue;
		}
		hal_pin_t *hp = hi->o.pin;
		assert(hp != NULL);
		if (halrcomp) {
		    if (hp->dir == HAL_IN) {
			note_printf(self->tx,
				    "HALrcomp cant write a HAL_IN pin: handle=%d name=%s",
				    handle, hp->name);
			continue;
		    }
		} else {
		    if (hp->dir == HAL_OUT) {
			note_printf(self->tx,
				    "HALrcommand: cant set an HAL_OUT pin: handle=%d name=%s",
				    handle, hp->name);
			continue;
		    }
		}
		if (hp->type != (hal_type_t) p.type()) {
		    note_printf(self->tx,
				"pin type mismatch: pb=%d/hal=%d, handle=%d name=%s",
				p.type(), hp->type, handle, hp->name);
		    continue;
		}
		// set value
		hal_data_u *vp = (hal_data_u *) hal_pin2u(hp);
		assert(vp != NULL);
		if (hal_pbpin2u(&p, vp)) {
		    note_printf(self->tx, "bad pin type %d name=%s",p.type(), hp->name);
		    continue;
		}
	    } else {
		// record handle lookup failure
		note_printf(self->tx, "no such handle: %d",handle);
		continue;
	    }
	} else {
	    // no handle given, try slow path via name, and add item.
	    if (!p.has_name()) {
		note_printf(self->tx,
			    "pin: no name and no handle!");
		continue;
	    }
	    if (describe_pin_by_name(self, p.name().c_str()))
		continue;
	}
    }

    // work the signals
    for (int i = 0; i < self->rx.signal_size(); i++) {
	const pb::Signal &s = self->rx.signal(i);
	// required fields
	if (!s.has_type()) {
	    note_printf(self->tx,
			"type missing in signal: handle=%d", s.handle());
	    continue;
	}
	// value present?
	if (!(s.has_halfloat() ||
	      s.has_halbit() ||
	      s.has_halu32() ||
	      s.has_hals32())) {
	    note_printf(self->tx,
			"value missing in signal: handle=%d", s.handle());
	    continue;
	}
	// try fast path via item dict first
	if (s.has_handle()) {
	    int handle = s.handle();
	    it = self->items.find(handle);

	    if (it != self->items.end()) {
		// handle present and found
		halitem_t *hi = it->second;
		if (hi->type != HAL_SIGNAL) {
		    note_printf(self->tx,
				"handle type mismatch - not a signal: handle=%d type=%d",
				handle, hi->type);
		    continue;
		}
		hal_sig_t *hs = hi->o.signal;
		assert(hs != NULL);
		if (hs->type != (hal_type_t) s.type()) {
		    note_printf(self->tx,
				"signal type mismatch: pb=%d/hal=%d, handle=%d name=%s",
				s.type(), hs->type, handle, hs->name);
		    continue;
		}
		if (hs->writers > 0) {
		    note_printf(self->tx,
				"cannot update signal '%s'  - %d output pin(s) linked",
				hs->name, hs->writers);
		    continue;
		}
		// set value
		hal_data_u *vp = (hal_data_u *) hal_sig2u(hs);
		assert(vp != NULL);
		if (hal_pbsig2u(&s, vp)) {
		    note_printf(self->tx, "bad signal type %d name=%s",s.type(), hs->name);
		    continue;
		}
	    } else {
		// record handle lookup failure
		note_printf(self->tx, "no such handle: %d",handle);
		continue;
	    }
	} else {
	    // no handle given, try slow path via name, and add item.
	    if (!s.has_name()) {
		note_printf(self->tx,
			    "signal: no name and no handle!");
		continue;
	    }
	    if (describe_signal_by_name(self, s.name().c_str()))
		continue;
	}
    }
    // XXX: add param handling here

    if (self->tx.note_size()) {
	self->tx.set_type(halrcomp ? pb::MT_HALRCOMP_SET_REJECT :
			  pb:: MT_HALRCOMMAND_SET_REJECT);
	return send_pbcontainer(from, self->tx, socket);
    }

    // otherwise reply only if explicitly required:
    if (self->rx.has_reply_required() && self->rx.reply_required()) {
	self->tx.set_type(halrcomp ? pb::MT_HALRCOMP_ACK : pb::MT_HALRCOMMAND_ACK);
	return send_pbcontainer(from, self->tx, socket);
    }
    return 0;
}

static int
process_get(htself_t *self, const std::string &from,  void *socket)
{
    itemmap_iterator it;

    for (int i = 0; i < self->rx.pin_size(); i++) {
	const pb::Pin &p = self->rx.pin(i);
	if (p.has_handle()) {
	    int handle = p.handle();
	    it = self->items.find(handle);
	    if (it != self->items.end()) {
		halitem_t *hi = it->second;
		if (hi->type != HAL_PIN) {
		    note_printf(self->tx,
				"get pin: handle type mismatch - not a pin: handle=%d type=%d",
				handle, hi->type);
		    continue;
		}
		hal_pin_t *hp = hi->o.pin;
		assert(hp != NULL);
		pb::Pin *pbpin = self->tx.add_pin();
		// reply with just value and handle
		pbpin->set_handle(hp->handle);
		hal_pin2pb(hp, pbpin);
	    }
	} else {
	    if (!p.has_name()) {
		note_printf(self->tx,
			    "get pin: no name and no handle!");
		continue;
	    }
	    // for named get, reply with full decoration
	    describe_pin_by_name(self, p.name().c_str());
	}
    }
    for (int i = 0; i < self->rx.signal_size(); i++) {
	const pb::Signal &s = self->rx.signal(i);
	if (s.has_handle()) {
	    int handle = s.handle();
	    it = self->items.find(handle);
	    if (it != self->items.end()) {
		halitem_t *hi = it->second;
		if (hi->type != HAL_SIGNAL) {
		    note_printf(self->tx,
				"get signal: handle type mismatch - not a signal: handle=%d type=%d",
				handle, hi->type);
		    continue;
		}
		hal_sig_t *hs = hi->o.signal;
		assert(hs != NULL);
		pb::Signal *pbsignal = self->tx.add_signal();
		// reply with just value and handle
		pbsignal->set_handle(hs->handle);
		hal_sig2pb(hs, pbsignal);
	    }
	} else {
	    if (!s.has_name()) {
		note_printf(self->tx,
			    "get signal: no name and no handle!");
		continue;
	    }
	    // for named get, reply with full decoration
	    describe_signal_by_name(self, s.name().c_str());
	}
    }
    // XXX: add param handling here

    self->tx.set_type(self->tx.note_size() ? pb::MT_HALRCOMMAND_GET_REJECT :
		      pb::MT_HALRCOMMAND_ACK);
    return send_pbcontainer(from, self->tx, socket);
}

// lookup pin by name and add pin with full decoration to self->tx if found
// add a halitem_t to the items array if pin handle not yet present
static
int describe_pin_by_name(htself_t *self, const char *name)
{
    hal_pin_t *hp __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));
    itemmap_iterator it;

    hp = halpr_find_pin_by_name(name);
    if (hp == NULL) {
	note_printf(self->tx, "no such pin: '%s'", name);
	return -1;
    }
    // add to items if not yet present
    it = self->items.find(hp->handle);
    if (it == self->items.end()) {
	// pin not found. add to items
	halitem_t *hi = new halitem_t();
	hi->type = HAL_PIN;
	hi->o.pin = hp;
	hi->ptr = SHMPTR(hp->data_ptr_addr);
	self->items[hp->handle] = hi;
	// printf("add pin %s to items\n", hp->name);
    }
    // add binding in reply - includes handle
    pb::Pin *pbpin = self->tx.add_pin();
    return halpr_describe_pin(hp, pbpin); // full decoration
}

// lookup signal by name and add signal with full decoration to self->tx if found
// add a halitem_t to the items array if signal handle not yet present
static
int describe_signal_by_name(htself_t *self, const char *name)
{
    hal_sig_t *hs __attribute__((cleanup(halpr_autorelease_mutex)));
    rtapi_mutex_get(&(hal_data->mutex));
    itemmap_iterator it;

    hs = halpr_find_sig_by_name(name);
    if (hs == NULL) {
	note_printf(self->tx, "no such signal: '%s'", name);
	return -1;
    }
    // add to items if not yet present
    it = self->items.find(hs->handle);
    if (it == self->items.end()) {
	// pin not found. add to items
	halitem_t *hi = new halitem_t();
	hi->type = HAL_SIGNAL;
	hi->o.signal = hs;
	hi->ptr = SHMPTR(hs->data_ptr);
	self->items[hs->handle] = hi;
	// printf("add signal %s to items\n", hs->name);
    }
    // add binding in reply - includes handle
    pb::Signal *pbsignal = self->tx.add_signal();
    return halpr_describe_signal(hs, pbsignal);
}

// extract pin (param too?) values out of the component passed int the
// MT_HALRCOMP_BIND message
// apply to HAL_OUT and HAL_IO pins
static int
apply_initial_values(htself_t *self, const pb::Component *pbcomp)
{
    for (int i = 0; i < pbcomp->pin_size(); i++) {
	const pb::Pin &p = pbcomp->pin(i);
	{
	    hal_pin_t *hp __attribute__((cleanup(halpr_autorelease_mutex)));
	    rtapi_mutex_get(&(hal_data->mutex));

	    const char *pname  = p.name().c_str();
	    hp = halpr_find_pin_by_name(pname);
	    if (hp == NULL) {
		note_printf(self->tx,
			    "BUG: pin %s missing in already validated component %s ?",
			    pname, pbcomp->name().c_str());
		continue;
	    }
	    // disconnected HAL_IN pins can be dealt with in halcmd with setp
	    // it would be wrong to set them from the remote
	    if (hp->dir == HAL_IN)
		continue;

	    // apply value
	    hal_data_u *vp = (hal_data_u *) hal_pin2u(hp);
	    assert(vp != NULL);
	    if (hal_pbpin2u(&p, vp)) {
		note_printf(self->tx, "bad pin type %d/%d name=%s", p.type(), hp->type, pname);
		continue;
	    }
	    rtapi_print_msg(RTAPI_MSG_DBG,
			    "%s: comp %s: applied inital value of %s",
			    self->cfg->progname, pbcomp->name().c_str(), pname);
	}
    }
    // do same for params?
    // better to get rid of params altogether and replace by pins

    return self->tx.note_size();
}
