#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <czmq.h>

#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

#include <rtapi.h>
#include <rtapi_hexdump.h>
#include <hal.h>
#include <hal_priv.h>
#include <hal_ring.h>

#include <machinetalk/include/pb-linuxcnc.h>
#include <machinetalk/nanopb/pb_decode.h>
#include <machinetalk/nanopb/pb_encode.h>
#include <machinetalk/include/container.h>

#include "messagebus.hh"
#include "rtproxy.hh"
#include "multiframe_flag.h"

// inproc variant for comms with RT proxy threads
// defined in messagbus.cc
extern const char *proxy_cmd_uri;
extern const char *proxy_response_uri;

#if 0
static int test_decode(zframe_t *f, const pb_field_t *fields);
static zframe_t *test_encode(const void *msg, const pb_field_t *fields);
#endif

int
send_subscribe(void *socket, const char *topic)
{
    size_t topiclen = strlen(topic);
    zframe_t *f = zframe_new (NULL, topiclen + 1 );
    assert(f);

    unsigned char *data = zframe_data(f);
    *data++ = '\001';
    memcpy(data, topic, topiclen);
    return zframe_send (&f, socket, 0);
}

void
rtproxy_thread(void *arg, zctx_t *ctx, void *pipe)
{
    rtproxy_t *self = (rtproxy_t *) arg;
    int retval;

    self->proxy_cmd = zsocket_new (ctx, ZMQ_XSUB);
    retval = zsocket_connect(self->proxy_cmd, proxy_cmd_uri);
    assert(retval == 0);

    self->proxy_response = zsocket_new (ctx, ZMQ_XSUB);
    assert(zsocket_connect(self->proxy_response, proxy_response_uri) == 0);

    if (self->flags & (ACTOR_RESPONDER|ACTOR_ECHO|ACTOR_SUBSCRIBER)) {
	retval = send_subscribe(self->proxy_cmd, self->name);
	assert(retval == 0);
    }

    if (self->flags & (ACTOR_INJECTOR|ACTOR_PUBLISHER)) {
	retval = send_subscribe(self->proxy_response, self->name);
	assert(retval == 0);
    }

    if (self->to_rt_name) {
	unsigned flags;
	if ((retval = hal_ring_attach(self->to_rt_name, &self->to_rt_ring, &flags))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: hal_ring_attach(%s) failed - %d\n",
			    progname, self->to_rt_name, retval);
	    return;
	}
	self->to_rt_ring.header->writer = comp_id;
    }
    if (self->from_rt_name) {
	unsigned flags;
	if ((retval = hal_ring_attach(self->from_rt_name, &self->from_rt_ring, &flags))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: hal_ring_attach(%s) failed - %d\n",
			    progname, self->from_rt_name, retval);
	    return;
	}
	self->from_rt_ring.header->reader = comp_id;
    }
    self->buffer = zmalloc(FROMRT_SIZE);
    assert(self->buffer);

    msgbuffer_init(&self->to_rt_mframe, &self->to_rt_ring);
    msgbuffer_init(&self->from_rt_mframe, &self->from_rt_ring);

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: %s startup", progname, self->name);

    if (self->flags & ACTOR_ECHO) {
	while (1) {
	    zmsg_t *msg = zmsg_recv(self->proxy_cmd);
	    if (msg == NULL)
		break;
	    if (self->flags & (TRACE_TO_RT|TRACE_FROM_RT))
		zmsg_dump_to_stream (msg, stderr);
	    zmsg_send(&msg, self->proxy_response);
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "%s: %s exit", progname, self->name);
    }

    if (self->flags & (ACTOR_RESPONDER|ACTOR_SUBSCRIBER)) {
	zpoller_t *cmdpoller = zpoller_new (self->proxy_cmd, NULL);
	zpoller_t *delay = zpoller_new(NULL);

	while (1) {
	    self->state = WAIT_FOR_COMMAND;
	    void *cmdsocket = zpoller_wait (cmdpoller, -1);
	    if (cmdsocket == NULL) // terminated
		goto DONE;
	    zmsg_t *to_rt = zmsg_recv(cmdsocket);
	    //	    zmsg_fprint (to_rt,stderr);
	    zframe_t *f;
	    int i;
	    pb_Container rx;

	    size_t mframe_size;
	    // compute buffer requirements and block until available
	    for (i = 0,f = zmsg_first (to_rt);
		 f != NULL;
		 f = zmsg_next(to_rt),i++) {
		switch (i) {
		case 0:
		case 1:
		    // the to/from strings are left untouched
		    mframe_size += zframe_size(f);
		    break;
		default:
		    if (self->flags & DESERIALIZE_TO_RT)
			mframe_size += sizeof(pb_Container); // FIXME pb_rt_Container eventually
		    else
			mframe_size += zframe_size(f);
		}
	    }
	    // add in the frameheaders
	    mframe_size += i * sizeof(frameheader_t);

	    // dont overrun to_rt ring
	    self->current_delay = self->min_delay;
	    self->state = WAIT_FOR_TO_RT_SPACE;

	    while (record_write_space(self->to_rt_ring.header) - mframe_size < 0) {
		zpoller_wait (delay, self->current_delay);
		// exponential backoff
		self->current_delay <<= 1;
		self->current_delay = MIN(self->current_delay, self->max_delay);
	    }

	    // good to go, should not fail due to lack of space
	    self->state = WRITING_TO_RT;

	    msg_write_abort(&self->to_rt_mframe);
	    for (i = 0,f = zmsg_first (to_rt);
		 f != NULL;
		 f = zmsg_next(to_rt),i++) {

		mflag_t flags;
		flags.u = 0;
		void *data = zframe_data(f);
		size_t size = zframe_size(f);

		switch (i) {
		case 0:
		case 1:
		    flags.f.frametype = MF_TRANSPARENT;
		    break;

		default:
		    if (!(self->flags & DESERIALIZE_TO_RT)) {
			// as is, no attempt at deserializing payload frames
			flags.f.frametype = MF_TRANSPARENT;
			flags.f.npbtype = NPB_UNSPECIFIED;
		    } else {
			// try to deserialize as (rt_) Container:  // FIXME rt_
			pb_istream_t stream = pb_istream_from_buffer(zframe_data(f),
								     zframe_size(f));
			if (pb_decode(&stream, pb_Container_fields, &rx)) {
			    // send the resulting nanopb C struct
			    data = (void *) &rx;
			    size = sizeof(rx);
			    flags.f.frametype = MF_NPB_CSTRUCT;
			    flags.f.npbtype = NPB_CONTAINER;
			    rtapi_print_msg(RTAPI_MSG_DBG,
					    "%s: sending as deserialized"
						" Container\n",
						progname);
			} else {
			    // hm, decode failed - must be something else.
			    self->decode_fail++;
			    rtapi_print_msg(RTAPI_MSG_DBG,
					    "%s: pb_decode(Container) failed: '%s'\n",
					progname, PB_GET_ERROR(&stream));
			    flags.f.frametype = MF_TRANSPARENT;
			    flags.f.npbtype = NPB_UNSPECIFIED;
			}
		    }
		    break;
		}
		retval = frame_write(&self->to_rt_mframe, data, size, flags.u);
		if (retval) {
		    self->rb_txfail++;
		    rtapi_print_msg(RTAPI_MSG_ERR, "frame_write: %s",
				    strerror(retval));
		} else
		    self->ftx++;
		if (self->flags & TRACE_TO_RT)
			rtapi_print_hex_dump(RTAPI_MSG_ERR, RTAPI_DUMP_PREFIX_OFFSET,
					     16,1, data, (size > 16) ? 16: size, 1,
					     "%s->%s size=%d t=%d c=%d: ",
					     self->name, self->to_rt_name, size,
					     flags.f.frametype, flags.f.npbtype);
	    }

	    msg_write_flush(&self->to_rt_mframe);
	    self->mftx++;
	    zmsg_destroy(&to_rt);

	    self->state = WAIT_FOR_RT_RESPONSE;
	    self->current_delay = self->min_delay;

	    zmsg_t *from_rt = zmsg_new();
	    msg_read_abort(&self->from_rt_mframe);
	    i = 0;
	    while (1) {
		zpoller_wait (delay, self->current_delay);
		if ( zpoller_terminated (delay) ) {
		    rtapi_print_msg(RTAPI_MSG_ERR, "%s: wait interrupted",
				    self->from_rt_name);
		}
		const void *data;
		size_t size;
		mflag_t flags;
		if (frame_read(&self->from_rt_mframe,
			       &data, &size, &flags.u) == 0) {
		    if (self->flags &  TRACE_FROM_RT)
			rtapi_print_hex_dump(RTAPI_MSG_ERR, RTAPI_DUMP_PREFIX_OFFSET,
					     16,1, data, (size > 16) ? 16: size, 1,
					     "%s->%s size=%d t=%d c=%d: ",
					     self->from_rt_name,self->name, size,
					     flags.f.frametype, flags.f.npbtype);
		    pb_ostream_t sstream, ostream;

		    // decide what to do based on flags
		    switch (flags.f.frametype) {

		    case MF_NPB_CSTRUCT:

			switch (flags.f.npbtype) {
			case NPB_CONTAINER:
			    ostream = pb_ostream_from_buffer((uint8_t *)self->buffer,FROMRT_SIZE);
			    if (!pb_encode(&ostream, pb_Container_fields, data)) {
				rtapi_print_msg(RTAPI_MSG_ERR,
						"%s: from_rt encoding failed %s type=%d written=%zu\n",
						progname, PB_GET_ERROR(&sstream),flags.f.npbtype,
						ostream.bytes_written);
				// send as-is
				f = zframe_new (data, size);
			    } else {
				rtapi_print_msg(RTAPI_MSG_DBG,"%s: sent as protobuf size=%zu\n",
						progname, ostream.bytes_written);
				f = zframe_new (self->buffer, ostream.bytes_written);
			    }
			    zmsg_append (from_rt, &f);
			    break;

			default:
			    rtapi_print_msg(RTAPI_MSG_ERR,
					    "%s: invalid npbtype from RT: %d, sending as-is\n",
					    progname, flags.f.npbtype);
			    zframe_t *f = zframe_new (data, size);
			    zmsg_append (from_rt, &f);
			}
			break;

		    case MF_TRANSPARENT:
		    case MF_PROTOBUF:
			// leave as is
			// zframe_new copies the data
			zframe_t *f = zframe_new (data, size);
			zmsg_append (from_rt, &f);
		    }
		    frame_shift(&self->from_rt_mframe);

		} else
		    break;

		// exponential backoff
		self->current_delay <<= 1;
		self->current_delay = MIN(self->current_delay, self->max_delay);
	    }
	    msg_read_flush(&self->from_rt_mframe);
	    zmsg_send (&from_rt, self->proxy_response);
	}
    DONE:
	zpoller_destroy(&cmdpoller);
	zpoller_destroy(&delay);
    }
    if (self->buffer)
	free(self->buffer);

    if (self->to_rt_name) {
	if ((retval = hal_ring_detach(self->to_rt_name, &self->to_rt_ring))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: hal_ring_detach(%s) failed - %d\n",
			    progname, self->to_rt_name, retval);
	    return;
	}
	self->to_rt_ring.header->writer = 0;
    }
    if (self->from_rt_name) {
	if ((retval = hal_ring_detach(self->from_rt_name, &self->from_rt_ring))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: hal_ring_detach(%s) failed - %d\n",
			    progname, self->from_rt_name, retval);
	    return;
	}
	self->from_rt_ring.header->reader = 0;
    }

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: %s exit", progname, self->name);
}

#if 0
// exercise Nanopb en/decode in case that's done in the proxy
static int test_decode(zframe_t *f, const pb_field_t *fields)
{
    pb_Container rx;
    uint8_t *buffer = zframe_data(f);
    size_t size = zframe_size(f);
    pb_istream_t stream = pb_istream_from_buffer(buffer, size);

    if (!pb_decode(&stream, fields, &rx)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: pb_decode(Container) failed: '%s'\n",
			progname, PB_GET_ERROR(&stream));
	return -1;
    }
    return 0;
}

static zframe_t *test_encode(const void *msg, const pb_field_t *fields)
{
    size_t size;

    // determine size
    pb_ostream_t sstream = PB_OSTREAM_SIZING;
    if (!pb_encode(&sstream, fields,  msg)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: sizing pb_encode(): %s written=%zu\n",
			    progname, PB_GET_ERROR(&sstream), sstream.bytes_written);
	    return NULL;
    }
    size = sstream.bytes_written;
    zframe_t *f = zframe_new(NULL, size);

    // encode directly into the new frame
    pb_ostream_t rstream = pb_ostream_from_buffer(zframe_data(f), size);
    if (!pb_encode(&rstream, fields,  msg)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: pb_encode failed: %s, msgsize=%d written=%zu\n",
			progname, PB_GET_ERROR(&rstream), size, rstream.bytes_written);
	zframe_destroy(&f);
	return NULL;
    }
    return f;
}
#endif
