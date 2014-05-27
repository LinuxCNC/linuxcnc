/********************************************************************
 * Description:  multiframe.h
 *
 *               This file, 'multiframe.h', implements multipart messages
 *               on top of the ringbuffer frame-mode API (see ring.h).
 *
 * Based on ring code by Pavel Shramov, 4/2014,
 * http://psha.org.ru/cgit/psha/ring/
 * License: MIT
 * Integration: Michael Haberler
 *
 * Terminology:
 *
 *   record:
 *            unit at the underlying ring.h record-oriented layer.
 *            a buffer with length. Length zero is a legitimate value.
 *            see ring.h for record_* operations.
 *
 *   frame:
 *            a structure at the multiframe layer.
 *            consists of buffer, length, and a flag value
 *            the flag is not interpreted by this code.
 *
 *   message:
 *            consists of 0 or more frames.
 *            a message is transported in a record of the underlying record_* layer.
 *
 *   messagebuffer:
 *            multiframe instance data. Local to a reader, or writer.
 *            instantiated with an existing record ringbuffer.
 *
 * This data format and terminology (frame/message) is intended to
 * closely match the zeroMQ ZMTP encoding of messages and frames - see
 * http://rfc.zeromq.org/spec:15 for a description of ZMTP
 * framing.
 *
 *
 * Public API:
 *
 * int msgbuffer_init(msgbuffer_t *mb, ringbuffer_t *ring);
 * int frame_write_begin(msgbuffer_t *mb, void ** data, size_t size, __u32 flags);
 * int frame_write_end(msgbuffer_t *mb, size_t size);
 * int frame_writev(msgbuffer_t *mb, ringvec_t *rv);
 * int frame_write(msgbuffer_t *mb, const void * data, size_t size, __u32 flags)
 * int msg_write_flush(msgbuffer_t *mb);
 * int msg_write_abort(msgbuffer_t *mb);

 * int frame_read(msgbuffer_t *mb, const void **data, size_t *size, __u32 *flags);
 * int frame_readv(msgbuffer_t *mb, ringvec_t *rv);
 * int frame_shift(msgbuffer_t *mb);
 * int msg_read_flush(msgbuffer_t *mb);
 * int msg_read_abort(msgbuffer_t *mb);
 *
 ********************************************************************/

#ifndef __MULTIFRAME_H__
#define __MULTIFRAME_H__

#include "ring.h"

typedef struct {
    ringbuffer_t *ring;
    void * _write;      // '_write' to disambiguate from kernel write() macro
    size_t write_size;
    size_t write_off;
    const void * _read;
    size_t read_size;
    size_t read_off;
} msgbuffer_t;

// ringvec_t is defined in ring.h.

// Layout of a frame in the underlying ringbuffer.
// not exposed at API - internal only.
typedef struct {
    __s32 size;
    __u32 flags;
    char data[0];  // actual frame contents
} frameheader_t;

// initialize a msgbuffer from an existing ringbuffer instance.
static inline int msgbuffer_init(msgbuffer_t *mb, ringbuffer_t *ring)
{
    memset(mb, 0, sizeof(msgbuffer_t));
    mb->ring = ring;
    return 0;
}

// start a zero-copy frame write in a multipart message.
// allocate size bytes for a frame, pass flag.
// returns a pointer to the buffer directly within the ringbuffer.
// return 0 on success.
// return EAGAIN if there is currently insufficient space
// return ERANGE if the write size exceeds the underlying ringbuffer size.
static inline int frame_write_begin(msgbuffer_t *mb, void ** data, size_t size, __u32 flags)
{
    const size_t sz = size + sizeof(frameheader_t);
    if (!mb->_write) {
	// allocate in record ring
	int r = record_write_begin(mb->ring, &mb->_write, sz);
	if (r) return r;
	mb->write_size = sz;
	mb->write_off = 0;
    }
    frameheader_t * frame = (frameheader_t *) ((unsigned char *)mb->_write + mb->write_off);
    if (mb->write_size < mb->write_off + sizeof(frameheader_t) + size) {
	// Reallocate
	const void * old = mb->_write;
	int r = record_write_begin(mb->ring, &mb->_write, mb->write_off + sz);
	if (r) return r;
	if (old != mb->_write)
	    memmove(mb->_write, old, mb->write_off);
	mb->write_size = mb->write_off + sz;
    }
    frame->size = size;
    frame->flags = flags;
    *data = frame->data; //frame + 1; // point past frameheader_t header
    return 0;
}

// commit a zero-copy frame write.
// size may be less than requested in frame_write_begin().
static inline int frame_write_end(msgbuffer_t *mb, size_t size)
{
    if (!mb->_write)
	return EINVAL;
    frameheader_t * frame = (frameheader_t *) mb->_write +  mb->write_off;
    frame->size = size;
    mb->write_off = mb->write_off + sizeof(*frame) + frame->size;
    return 0;
}

// copying frame write, discrete args style
static inline int frame_write(msgbuffer_t *mb, const void * data, size_t size, __u32 flags)
{
    void * ptr;
    int r = frame_write_begin(mb, &ptr, size, flags);
    if (r) return r;
    memmove(ptr, data, size);
    return frame_write_end(mb, size);
}

// copying frame write, iov-style args
static inline int frame_writev(msgbuffer_t *mb, ringvec_t *rv)
{
    return frame_write(mb, rv->rv_base, rv->rv_len, rv->rv_flags);
}

// commit and send off a multipart message, consisting of zero or more frames.
static inline int msg_write_flush(msgbuffer_t *mb)
{
    int r = record_write_end(mb->ring, mb->_write, mb->write_off);
    mb->_write = 0;
    return r;
}

// abort an unfinished message write operation
// any number of frames written so far are flushed.
static inline int msg_write_abort(msgbuffer_t *mb)
{
    mb->_write = 0;
    return 0;
}

// read a frame without consuming,  discrete args style
static inline int frame_read(msgbuffer_t *mb, const void ** data, size_t * size, __u32 * flags)
{
    if (!mb->_read) {
	int r = record_read(mb->ring, &mb->_read, &mb->read_size);
	if (r) return r;
	mb->read_off = 0;
    }
    if (mb->read_off == mb->read_size)
	return EAGAIN; //XXX?
    const frameheader_t * frame = (frameheader_t *) ((unsigned char *)mb->_read + mb->read_off);
    *data = frame + 1;
    *size = frame->size;
    *flags = frame->flags;
    return 0;
}


// read a frame without consuming, iov-style
static inline int frame_readv(msgbuffer_t *mb, ringvec_t *rv)
{
    return frame_read(mb, &rv->rv_base, &rv->rv_len, &rv->rv_flags);
}

// consume a frame.
// Advances read pointer to next frame in message.
static inline int frame_shift(msgbuffer_t *mb)
{
    if (!mb->_read || mb->read_off == mb->read_size) return EINVAL;
    const frameheader_t * frame = (frameheader_t *) ((unsigned char *)mb->_read + mb->read_off);
    mb->read_off += sizeof(*frame) + frame->size;
    return 0;
}

// consume a multi-frame message, regardless how many frames were consumed.
static inline int msg_read_flush(msgbuffer_t *mb)
{
    if (!mb->_read) return EINVAL;
    mb->_read = 0;
    return record_shift(mb->ring);
}

// abort a read operation, undoing any consumption in the multipart message
static inline int msg_read_abort(msgbuffer_t *mb)
{
    if (!mb->_read) return EINVAL;
    mb->_read = 0;
    return 0;
}

#endif //__MULTIFRAME_H__
