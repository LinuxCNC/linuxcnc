/* avahi event loopt adapter for czmq
 *
 * Copyright Michael Haberler 2013-2015
 * License: Mozilla Public License Version 2.0
 */

#include <avahi-common/llist.h>
#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>

#include "czmq-watch.h"

#ifdef CZMQ_WATCH_DEBUG
#define DPRINTF(...) fprintf(stderr,  ## __VA_ARGS__)
#else
#define DPRINTF(...)
#endif

struct AvahiWatch {
    AvahiCzmqPoll *czmq_poll;
    zmq_pollitem_t poller;
    AvahiWatchCallback callback;
    void *userdata;
    AVAHI_LLIST_FIELDS(AvahiWatch, watches);
};

struct AvahiTimeout {
    AvahiCzmqPoll *czmq_poll;
    int timer_id;
    AvahiTimeoutCallback callback;
    void  *userdata;
    AVAHI_LLIST_FIELDS(AvahiTimeout, timeouts);
};

struct AvahiCzmqPoll {
    zloop_t *loop;
    AvahiPoll api;
    AVAHI_LLIST_HEAD(AvahiWatch, watches);
    AVAHI_LLIST_HEAD(AvahiTimeout, timeouts);
};

static void cleanup_watches(AvahiCzmqPoll *g)
{
    AvahiWatch *w, *next;
    assert(g);
    DPRINTF( "--cleanup_watches\n");
    for (w = g->watches; w; w = next) {
        next = w->watches_next;
        if (w->poller.fd < 0) {
	    AVAHI_LLIST_REMOVE(AvahiWatch, watches, g->watches, w);
	    avahi_free(w);
	}
    }
}

static short map_events_to_czmq(AvahiWatchEvent events)
{
    return
        (events & AVAHI_WATCH_IN ? ZMQ_POLLIN : 0) |
        (events & AVAHI_WATCH_OUT ? ZMQ_POLLOUT : 0) ;
}

static AvahiWatchEvent map_events_from_czmq(short events)
{
    return
        (events & ZMQ_POLLIN ? AVAHI_WATCH_IN : 0) |
        (events & ZMQ_POLLOUT ? AVAHI_WATCH_OUT : 0);
}

static int watch_handler(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
    AvahiWatch *w = (AvahiWatch *) arg;
    DPRINTF( "--watch_handler fd=%d ev=%d rev=%d\n",
	    item->fd,item->events,item->revents);
    assert(w->callback);
    w->callback(w, item->fd, map_events_from_czmq(item->revents), w->userdata);
    return 0;
}

static AvahiWatch* watch_new(const AvahiPoll *api,
			     int fd,
			     AvahiWatchEvent events,
			     AvahiWatchCallback callback,
			     void *userdata)
{
    AvahiWatch *w;
    AvahiCzmqPoll *g;

    DPRINTF( "--watch_new fd=%d ev=%d\n", fd, events);
    assert(api);
    assert(fd >= 0);
    assert(callback);

    g = api->userdata;
    assert(g);

    if (!(w = avahi_new(AvahiWatch, 1)))
        return NULL;

    w->czmq_poll = g;
    w->poller.socket = NULL;
    w->poller.fd = fd;
    w->poller.events =  map_events_to_czmq(events);
    w->poller.revents =  0;

    w->callback = callback;
    w->userdata = userdata;
    zloop_poller (g->loop, &w->poller, watch_handler, w);

    AVAHI_LLIST_PREPEND(AvahiWatch, watches, g->watches, w);
    return w;
}

static void watch_update(AvahiWatch *w, AvahiWatchEvent events)
{
    DPRINTF( "--watch_update\n");

    assert(w);
    assert(w->poller.fd > -1);

    zloop_poller_end(w->czmq_poll->loop, &w->poller);
    w->poller.events = map_events_to_czmq(events);
    zloop_poller(w->czmq_poll->loop, &w->poller, watch_handler, w);
}

static AvahiWatchEvent watch_get_events(AvahiWatch *w)
{
    assert(w);
    return map_events_from_czmq(w->poller.revents);
}

static void watch_free(AvahiWatch *w)
{
    DPRINTF( "--watch_free\n");
    assert(w);
    if (w->poller.fd > -1) {
	zloop_poller_end(w->czmq_poll->loop, &w->poller);
    }
    w->poller.fd = -1;
}

static int timer_handler (zloop_t *loop, int timer_id, void *arg)
{
    AvahiTimeout *t = arg;
    assert(t);
    assert(t->callback);

    DPRINTF( "--timer_handler id=%d\n", t->timer_id);

    t->callback(t, t->userdata);
    return 0;
}

static AvahiTimeout* timeout_new(const AvahiPoll *api,
				 const struct timeval *tv,
				 AvahiTimeoutCallback callback,
				 void *userdata)
{
    AvahiTimeout *t;
    AvahiCzmqPoll *g;
    DPRINTF( "--timeout_new\n");

    assert(api);
    assert(callback);

    g = api->userdata;
    assert(g);

    if (!(t = avahi_new(AvahiTimeout, 1)))
        return NULL;

    t->czmq_poll = g;
    t->callback = callback;
    t->userdata = userdata;
    t->timer_id = -1;
    if (tv) {
	AvahiUsec age = avahi_age(tv);
	if (age < 0) {
	    size_t delay = -age/1000;
	    t->timer_id = zloop_timer (g->loop, delay, 1, timer_handler, t);
	    DPRINTF( "--timer_new ID=%d mSEC=%zu\n",t->timer_id, delay);
	}
    }
    AVAHI_LLIST_PREPEND(AvahiTimeout, timeouts, g->timeouts, t);
    return t;
}

static void timeout_update(AvahiTimeout *t, const struct timeval *tv)
{
    assert(t);
    if (t->czmq_poll->loop == NULL) return;

    zloop_timer_end (t->czmq_poll->loop,  t->timer_id);

    if (tv == 0) {
	DPRINTF( "--timeout_update CANCEL tid=%d\n", t->timer_id);
	return;     // cancel
    }
    size_t msecs = avahi_age(tv) / 1000;
    if (msecs > 0)
	msecs = 0;
    else
	msecs = -msecs;
    t->timer_id = zloop_timer (t->czmq_poll->loop, msecs, 1, timer_handler, t);
    DPRINTF( "--timeout_update UPDATE tid=%d msec=%d\n",
	    t->timer_id, msecs);
}

static void timeout_free(AvahiTimeout *t)
{
    DPRINTF( "--timeout_free tid=%d\n", t->timer_id);

    assert(t);
    assert(t->timer_id > -1);
    if (t->czmq_poll->loop)
	zloop_timer_end (t->czmq_poll->loop,  t->timer_id);
    t->timer_id = -1;
}

static void cleanup_timeouts(AvahiCzmqPoll *g)
{
    AvahiTimeout *t, *next;
    DPRINTF( "--cleanup_timeouts\n");
    assert(g);
    for (t = g->timeouts; t; t = next) {
        next = t->timeouts_next;
        if (t->timer_id > -1) {
	    zloop_timer_end (t->czmq_poll->loop,  t->timer_id);
	    AVAHI_LLIST_REMOVE(AvahiTimeout, timeouts, t->czmq_poll->timeouts, t);
	    avahi_free(t);
	}
    }
}


AvahiCzmqPoll *avahi_czmq_poll_new(zloop_t *loop)
{
    AvahiCzmqPoll *g;
    DPRINTF( "--avahi_czmq_poll_new\n");

    if (!(g = avahi_new(AvahiCzmqPoll, 1)))
        return NULL;
    g->loop = loop;
    g->api.userdata = g;

    g->api.watch_new = watch_new;
    g->api.watch_free = watch_free;
    g->api.watch_update = watch_update;
    g->api.watch_get_events = watch_get_events;

    g->api.timeout_new = timeout_new;
    g->api.timeout_free = timeout_free;
    g->api.timeout_update = timeout_update;

    AVAHI_LLIST_HEAD_INIT(AvahiWatch, g->watches);
    AVAHI_LLIST_HEAD_INIT(AvahiTimeout, g->timeouts);
    return g;
}

void avahi_czmq_poll_free(AvahiCzmqPoll *g)
{
    DPRINTF( "--avahi_czmq_poll_free\n");

    assert(g);
    cleanup_watches(g);
    cleanup_timeouts(g);
}

const AvahiPoll* avahi_czmq_poll_get(AvahiCzmqPoll *g)
{
    assert(g);
    return &g->api;
}


void avahi_czmq_poll_quit(AvahiCzmqPoll *g)
{
    cleanup_watches(g);
    cleanup_timeouts(g);
}
