#ifndef HAL_RING_H
#define HAL_RING_H
#include <rtapi.h>
#include "rtapi_mbarrier.h"	/* memory barrier primitves */

#if defined(BUILD_SYS_USER_DSO)
#include <stdbool.h>
#else // kernel thread styles
#if defined(RTAPI)
#include <linux/types.h>
#else // ULAPI
#include <stdbool.h>
#include <time.h>               /* remote comp timestamps */
#endif
#endif

RTAPI_BEGIN_DECLS


// record mode: Negative numbers are needed for skips
typedef s32 ring_size_t;

// the ringbuffer shared data as exposed to a user
// this is part of HAL shared memory and member of hal_ring_t
//
// defaults: record mode, HAL memory, no rmutex/wmutex use
typedef struct {
    char name[HAL_NAME_LEN + 1];  // ring HAL name
    bool is_stream;      // record or stream mode
    bool use_rtapishm;   // use RTAPI shm segment (default: HAL memory)
    bool use_rmutex;     // hint to using code - use ringheader_t.rmutex
    bool use_wmutex;     // hint to using code - use ringheader_t.wmutex

    int reader, writer;  // HAL module id's - informational
    unsigned long rmutex, wmutex; // optional use - if used by multiple readers/writers
    size_t scratchpad_size;
    size_t size_mask;    // stream mode only
    size_t size;         // common to stream and record mode
    size_t head __attribute__((aligned(16)));
    u64    generation;
    size_t tail __attribute__((aligned(16)));
} ringheader_t;

typedef struct {
    // private
    int next_ptr;		 // next ring in free list
    int owner;                   // creating HAL module
    // if rhdr.uses_rtapishm nonzero, use am RTAPI shm segment
    int shm_handle;              // as returned by rtapi_shmem_new
    int shm_key;                 // HAL_RING_SHM_KEY + running serial
    //  if rhdr.uses_rtapishm nonzero, this points to HAL memory
    unsigned int hal_buf_offset;
    // public: the ring descriptor - this part is visible to using code
    ringheader_t rhdr;
} hal_ring_t;

// descriptor structure for an accessible (attached) ringbuffer
// this structure is in per-user memory, NOT HAL/RTAPI shm
// it is filled in by hal_ring_attach()
// NB: this is actually Pavel's ringheader_t

// some components use a fifo and a scratchpad shared memory area,
// like sampler.c and streamer.c. ringbuffer_t supports this through
// the optional scratchpad, which is created if spsize is > 0
// on hal_ring_new(). The scratchpad size is recorded in
// ringheader_t.scratchpad_size.

typedef struct {
    ringheader_t *header;
    char *buf;           // the actual ring storage (either HALmem or RTAPI shmsegs)
    void *scratchpad;
} ringbuffer_t;

typedef struct
{
    const ringbuffer_t *ring;
    u64   generation;
    size_t offset;
} ringiter_t;

typedef struct {
    void * rv_base;
    int rv_len;
} ringvec_t;

// mode flags passed in by hal_ring_new
// exposed in ringheader_t.mode
//#define MODE_RECORD      _BIT(0)
#define MODE_STREAM      _BIT(1)

// hal_ring_t.flags bits:
//#define ALLOC_HALMEM         _BIT(2)  default
#define ALLOC_RTAPISHMSEG    _BIT(3)
// future non-HAL/RTAPI use:
// #define ALLOC_MALLOC         _BIT(4)  // inproc use only
// #define ALLOC_MMAP           _BIT(5)

// force deallocation of RTAPI shm segment, and put ring descriptor onto free list
// instead of deleted list (might not be a good idea if still in use)
#define FORCE_DEALLOC    _BIT(16)

#define USE_RMUTEX       _BIT(17)
#define USE_WMUTEX       _BIT(18)


// generic ring methods for all modes:

/* create a named ringbuffer, owned by comp module_id
 * mode is one of: MODE_RECORD, MODE_STREAM
 * optionally or'd with USE_RMUTEX, USE_WMUTEX
 * spsize > 0 will allocate a shm scratchpad buffer
 * accessible through ringbuffer_t.scratchpad/ringheader_t.scratchpad
 */
int hal_ring_new(const char *name, int size, int spsize, int module_id, int mode);

/* delete a named ringbuffer */
int hal_ring_delete(const char *name, int force);

/* make an existing ringbuffer accessible to a component
 * rb must point to storage of type ringbuffer_t
 */
int hal_ring_attach(const char *name, ringbuffer_t *rb, int module_id);

// given a ringbuffer_t, retrieve its HAL name
//const char *hal_ring_name(ringbuffer_t *rb);

// advisory flags for multiple reader/writer threads scenarios:
//
// if used, use the rmutex/wmutex fields in ringbuffer_t like so:
//
//  rtapi_mutex_get(&(rb->rmutex));
//  rtapi_mutex_try(&(rb->rmutex));
//  rtapi_mutex_give(&(rb->rmutex));
//int hal_ring_use_wmutex(ringbuffer_t *rb);
//int hal_ring_use_rmutex(ringbuffer_t *rb);

// the MODE_RECORD functions: hal_record_

/* write a variable-sized record into the ringbuffer.
 * record boundaries will be preserved.
 * returns 0 on success,
 * else errno:
 *     ERANGE:  record greater than ring buffer (fatal)
 *     EAGAIN:  currently not enough space in ring buffer (temporary)
 */
//int hal_record_write(ringbuffer_t *h, void * data, size_t sz);

/* return pointer to data available, or NULL if ringbuffer empty
 */
//void *hal_record_next(ringbuffer_t *h);

/* return size of the next readable record
 * return -1 if ringbuffer empty
 * NB: zero-sized records are supported
 */
//ring_size_t hal_record_next_size(ringbuffer_t *h);


/* advance ring buffer by one record after consuming data
 * with hal_record_next/hal_record_next_size
 *
 * NB: without hal_record_shift(), hal_record_next/hal_record_next_size
 * are in effect a peek operation
 */
//void hal_record_shift(ringbuffer_t *h);

/* returns the largest contiguous block available
 * such that hal_record_write(h, data, hal_record_write_space(h))
 * will succeed
 *
 * NB: a hal_record_shift() may or may not affect the value
 * returned by this function; as it may or may not change the size
 * largest contiguous block even if more memory for smaller messages
 * is available
 */
//size_t hal_record_write_space(const ringheader_t *h);

/* empty a ring. Not thread safe.
 *
 * NB: only the reader should execute this function!
 */
//void hal_record_flush(ringbuffer_t *h);

#define RB_ALIGN 8

// Round up X to closest upper alignment boundary
static inline ring_size_t size_aligned(const ring_size_t x) // OK
{
    return x + (-x & (RB_ALIGN - 1));
}

static inline ring_size_t * _size_at(const ringbuffer_t *rb, const size_t off) // OK
{
    return (ring_size_t *) (rb->buf + off);
}

/* hal_record_write_begin():
 *
 * begin a zero-copy write operation for at least sz bytes. This povides a buffer
 * to write to within free ringbuffer space.
 *
 * return 0 if there is sufficient space for the requested size
 * return EAGAIN if there is currently insufficient space
 * return ERANGE if the write size exceeds the ringbuffer size.
 *
 * The write needs to be committed with a corresponding hal_record_write_end()
 * operation whose size argument must be less or equal to the size requested in
 * hal_record_write_begin().
 */
static inline int hal_record_write_begin(ringbuffer_t *ring, void ** data, size_t sz)
{
    int free;
    ringheader_t *h = ring->header;
    size_t a = size_aligned(sz + sizeof(ring_size_t));
    if (a > h->size)
	return ERANGE;

    free = (h->size + h->head - h->tail - 1) % h->size + 1; // -1 + 1 is needed for head==tail

    //printf("Free space: %d; Need %zd\n", free, a);
    if (free <= a) return EAGAIN;
    if (h->tail + a > h->size) {
	if (h->head <= a)
	    return EAGAIN;
	*data = _size_at(ring, 0) + 1;
	return 0;
    }
    *data = _size_at(ring, h->tail) + 1;
    return 0;
}

/* hal_record_write_end()
 * 
 * commit a zero-copy write to the ring which was started by hal_record_write_begin().
 * sz on hal_record_write_end() must be less equal to the size requested in 
 * the corresponding hal_record_write_begin().
 */
static inline int hal_record_write_end(ringbuffer_t *ring, void * data, size_t sz) 
{
    ringheader_t *h = ring->header;
    size_t a = size_aligned(sz + sizeof(ring_size_t));
    if (data == _size_at(ring, 0) + 1) {
	// Wrap
	*_size_at(ring, h->tail) = -1;
	h->tail = 0;
    }
    *_size_at(ring, h->tail) = sz;
    // mah: see [2]:144
    // PaUtil_WriteMemoryBarrier(); ???

    // mah: see [6]:69
    // should this be CAS(&(h->tail, h->tail, h->tail+a) ?
    h->tail = (h->tail + a) % h->size;
    //printf("New head/tail: %zd/%zd\n", h->head, h->tail);
    return 0;
}

/* hal_record_write()
 * 
 * copying write operation from an existing buffer. 
 *
 * return 0 if there is sufficient space
 * return EAGAIN if there is currently insufficient space
 * return ERANGE if the write size exceeds the ringbuffer size.
 */
static inline int hal_record_write(ringbuffer_t *rb, void * data, size_t sz) // OK
{
    void * ptr;
 
   int r = hal_record_write_begin(rb, &ptr, sz);
    if (r) return r;
    memmove(ptr, data, sz);
    return hal_record_write_end(rb, ptr, sz);
}

/* internal use function
 */
static inline int _ring_read_at(const ringbuffer_t *ring, size_t offset,
				const void **data, size_t *size) // OK
{
    ring_size_t *sz;
    ringheader_t *h = ring->header;
    if (offset == h->tail)
	return EAGAIN;

    // mah: see [2]:181
    // PaUtil_ReadMemoryBarrier(); ???

    //printf("Head/tail: %zd/%zd\n", h->head, h->tail);
    sz = _size_at(ring, offset);
    if (*sz < 0)
        return _ring_read_at(ring, 0, data, size);

    *size = *sz;
    *data = sz + 1;
    return 0;
}

/* hal_record_read()
 * 
 * non-copying read operation.
 *
 * Fail with EAGAIN if no data available.
 * Otherwise set data to point to the current record, and size to the size of the
 * current record.
 *
 * Note this is a 'peek' operation, it does not advance the read pointer to the next
 * record. To actually consume the record, call hal_record_shift() after processing.
 */
static inline int hal_record_read(const ringbuffer_t *rb, const void **data, size_t *size)
{
    return _ring_read_at(rb, rb->header->head, data, size);
}

/* hal_record_next()
 *
 * test for data available. Return zero if none, else pointer to the data field
 * of the next available record.
 */
static inline const void *hal_record_next(ringbuffer_t *rb)
{
    const void *data;
    size_t size;
    if (hal_record_read(rb, &data, &size)) return 0;
    return data;
}

/* hal_record_next_size()
 *
 * retrieve the size of the next available record. 
 * Return -1 if no data available.
 *
 * Note that zero-length records are supported and valid.
 */
static inline ring_size_t hal_record_next_size(ringbuffer_t *rb) // CHANGED
{
    const void *data;
    size_t size;
    if (hal_record_read(rb, &data, &size)) return -1;
    return size;
}

/* hal_record_write_space()
 * 
 * return the size of the largest record which can be currently written
 * without failure.
 *
 * Note this does not retrieve the amount of free space in the buffer which 
 * might be larger than the value returned; however, this space maye not be written
 * by records larger than returned by hal_record_write_space() (i.e. many small
 * writes may be possible which are in sum larger than the value returned here).
 */
static inline size_t hal_record_write_space(const ringheader_t *h)
{
    int avail = 0;

    if (h->tail < h->head)
        avail = h->head - h->tail;
    else
        avail = MAX(h->head, h->size - h->tail);
    return MAX(0, avail - (2 * RB_ALIGN));
}

/* hal_record_flush()
 *
 * clear the buffer. 
 * NB: this is not thread safe.
 */
static inline void hal_record_flush(ringbuffer_t *rb)
{
    ringheader_t *h = rb->header;
    // FIXME - bump generation to invalidate iterator?
    h->generation++;
    h->head = h->tail;
}

/* internal function */
static ring_size_t _ring_shift_offset(const ringbuffer_t *ring, size_t offset) // OK
{
    ring_size_t size;
    ringheader_t *h = ring->header;
    if (h->head == h->tail)
	return -1;
    // mah: [2]:192
    // PaUtil_FullMemoryBarrier(); ???
    size = *_size_at(ring, offset);
    if (size < 0)
	return _ring_shift_offset(ring, 0);
    size = size_aligned(size + sizeof(ring_size_t));
    return (offset + size) % h->size;
}

/* hal_record_shift()
 *
 * consume a record in the ring buffer. 
 * used with a corresponding hal_record_read().
 *
 * example processing loop:
 *
 * void *data;
 * size_t size;
 *
 * while (hal_record_read(ring, &data, &size)) {
 *    // process(data,size)
 *    hal_record_shift(ring); 
 * }
 */
static inline void hal_record_shift(ringbuffer_t *rb) // OK
{
    ring_size_t off = _ring_shift_offset(rb, rb->header->head);
    if (off < 0) return;
    rb->header->generation++;
    rb->header->head = off;
}

// iterator functions

static inline int hal_record_iter_init(const ringbuffer_t *ring,
				       ringiter_t *iter) // OK
{
    iter->ring = ring;
    iter->generation = ring->header->generation;
    iter->offset = ring->header->head;
    if (ring->header->generation != iter->generation)
        return EAGAIN;
    return 0;
}

static inline int hal_record_iter_invalid(const ringiter_t *iter)// OK
{
    if (iter->ring->header->generation > iter->generation)
        return EINVAL;
    return 0;
}

static inline int hal_record_iter_shift(ringiter_t *iter)// OK
{
    ring_size_t off;

    if (hal_record_iter_invalid(iter)) return EINVAL;
    off = _ring_shift_offset(iter->ring, iter->offset);
    if (off < 0) return EAGAIN;
//    printf("Offset to %zd\n", off);
    iter->generation++;
    iter->offset = off;
    return 0;
}

static inline int hal_record_iter_read(const ringiter_t *iter,
				       const void **data, size_t *size)// OK
{
    if (hal_record_iter_invalid(iter)) return EINVAL;

//    printf("Read at %zd\n", iter->offset);
    return _ring_read_at(iter->ring, iter->offset, data, size);
}

// observer accessors:

static inline int hal_ring_isstream(ringbuffer_t *rb)
{
    return rb->header->is_stream;
}

static inline const char * hal_ring_name(ringbuffer_t *rb)
{
    return rb->header->name;
}

static inline int hal_ring_use_wmutex(ringbuffer_t *rb)
{
    return rb->header->use_wmutex ;
}

static inline int hal_ring_use_rmutex(ringbuffer_t *rb)
{
    return rb->header->use_rmutex;
}

static inline int hal_ring_scratchpad_size(ringbuffer_t *rb)
{
    return rb->header->scratchpad_size;
}

// compute the next highest power of 2 of 32-bit v
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static unsigned inline next_power_of_two(unsigned v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}


//  SMP barriers adapted from:
/*
 * $Id: pa_ringbuffer.c 1738 2011-08-18 11:47:28Z rossb $
 * Portable Audio I/O Library
 * Ring Buffer utility.
 *
 * Author: Phil Burk, http://www.softsynth.com
 * modified for SMP safety on Mac OS X by Bjorn Roche
 * modified for SMP safety on Linux by Leland Lucius
 * also, allowed for const where possible
 * modified for multiple-byte-sized data elements by Sven Fischer
 *
 * Note that this is safe only for a single-thread reader and a
 * single-thread writer.
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * The text above constitutes the entire PortAudio license; however,
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also
 * requested that these non-binding requests be included along with the
 * license above.
 */


//  MODE_STREAM ring operations: adapted from the jack2
//  https://github.com/jackaudio/jack2/blob/master/common/ringbuffer.c
//  XXX: I understand this to be compatible with the hal_lib.c license


/*
  Copyright (C) 2000 Paul Davis
  Copyright (C) 2003 Rohan Drape

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

  ISO/POSIX C version of Paul Davis's lock free ringbuffer C++ code.
  This is safe for the case of one read thread and one write thread.
*/

// the stream 'methods'
/* void   hal_stream_get_read_vector(const ringbuffer_t *rb, */
/* 				  ringvec_t *vec); */
/* void   hal_stream_get_write_vector(const ringbuffer_t *rb, */
/* 				   ringvec_t *vec); */
/* size_t hal_stream_read(ringbuffer_t *rb, char *dest, size_t cnt); */
/* size_t hal_stream_peek(ringbuffer_t *rb, char *dest, size_t cnt); */
/* void   hal_stream_read_advance(ringbuffer_t *rb, size_t cnt); */
/* size_t hal_stream_read_space(const ringheader_t *h); */
/* void   hal_stream_flush(ringbuffer_t *rb); */
/* size_t hal_stream_write(ringbuffer_t *rb, const char *src, */
/* 			size_t cnt); */
/* void   hal_stream_write_advance(ringbuffer_t *rb, size_t cnt); */
/* size_t hal_stream_write_space(const ringheader_t *h); */

/* The non-copying data reader.  `vec' is an array of two places.  Set
 * the values at `vec' to hold the current readable data at `rb'.  If
 * the readable data is in one segment the second segment has zero
 * length.
 */
static inline void hal_stream_get_read_vector(const ringbuffer_t *rb,
				ringvec_t *vec)
{
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;
    ringheader_t *h = rb->header;

    w = h->tail;
    r = h->head;

    if (w > r) {
	free_cnt = w - r;
    } else {
	free_cnt = (w - r + h->size) & h->size_mask;
    }
    cnt2 = r + free_cnt;
    if (cnt2 > h->size) {
	/* Two part vector: the rest of the buffer after the current write
	   ptr, plus some from the start of the buffer. */
	vec[0].rv_base = (void *) &(rb->buf[r]);
	vec[0].rv_len = h->size - r;
	vec[1].rv_base = (void *) rb->buf;
	vec[1].rv_len = cnt2 & h->size_mask;

    } else {
	/* Single part vector: just the rest of the buffer */
	vec[0].rv_base = (void *) &(rb->buf[r]);
	vec[0].rv_len = free_cnt;
	vec[1].rv_len = 0;
    }
}

/* The non-copying data writer.  `vec' is an array of two places.  Set
 * the values at `vec' to hold the current writeable data at `rb'.  If
 * the writeable data is in one segment the second segment has zero
 * length.
 */
static inline void hal_stream_get_write_vector(const ringbuffer_t *rb,
				 ringvec_t *vec)
{
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;
    ringheader_t *h = rb->header;

    w = h->tail;
    r = h->head;

    if (w > r) {
	free_cnt = ((r - w + h->size) & h->size_mask) - 1;
    } else if (w < r) {
	free_cnt = (r - w) - 1;
    } else {
	free_cnt = h->size - 1;
    }
    cnt2 = w + free_cnt;
    if (cnt2 > h->size) {
	// Two part vector: the rest of the buffer after the current write
	// ptr, plus some from the start of the buffer.
	vec[0].rv_base = (void *) &(rb->buf[w]);
	vec[0].rv_len = h->size - w;
	vec[1].rv_base = (void *) rb->buf;
	vec[1].rv_len = cnt2 & h->size_mask;
    } else {
	vec[0].rv_base = (void *) &(rb->buf[w]);
	vec[0].rv_len = free_cnt;
	vec[1].rv_len = 0;
    }
    if (free_cnt)
	rtapi_smp_mb();
}


/* Return the number of bytes available for reading.  This is the
 * number of bytes in front of the read pointer and behind the write
 * pointer.
 */
static inline size_t hal_stream_read_space(const ringheader_t *h)
{
    size_t w, r;

    w = h->tail;
    r = h->head;
    if (w > r) {
	return w - r;
    } else {
	return (w - r + h->size) & h->size_mask;
    }
}

/* The copying data reader.  Copy at most `cnt' bytes from `rb' to
 * `dest'.  Returns the actual number of bytes copied.
 */
static inline size_t hal_stream_read(ringbuffer_t *rb, char *dest, size_t cnt)
{
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;
    ringheader_t *h = rb->header;

    if ((free_cnt = hal_stream_read_space (h)) == 0) {
	return 0;
    }
    to_read = cnt > free_cnt ? free_cnt : cnt;
    cnt2 = h->head + to_read;
    if (cnt2 > h->size) {
	n1 = h->size - h->head;
	n2 = cnt2 & h->size_mask;
    } else {
	n1 = to_read;
	n2 = 0;
    }
    memcpy (dest, &(rb->buf[h->head]), n1);
    h->head = (h->head + n1) & h->size_mask;

    if (n2) {
	memcpy (dest + n1, &(rb->buf[h->head]), n2);
	h->head = (h->head + n2) & h->size_mask;
    }
    return to_read;
}


/* The copying data reader w/o read pointer advance.  Copy at most
 * `cnt' bytes from `rb' to `dest'.  Returns the actual number of bytes
 * copied.
 */
static inline size_t hal_stream_peek(ringbuffer_t *rb, char *dest, size_t cnt)
{
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;
    size_t tmp_head;
    ringheader_t *h = rb->header;

    tmp_head = h->head;
    if ((free_cnt = hal_stream_read_space (h)) == 0) {
	return 0;
    }
    to_read = cnt > free_cnt ? free_cnt : cnt;
    cnt2 = tmp_head + to_read;
    if (cnt2 > h->size) {
	n1 = h->size - tmp_head;
	n2 = cnt2 & h->size_mask;
    } else {
	n1 = to_read;
	n2 = 0;
    }
    memcpy (dest, &(rb->buf[tmp_head]), n1);
    tmp_head = (tmp_head + n1) & h->size_mask;

    if (n2) {
	memcpy (dest + n1, &(rb->buf[tmp_head]), n2);
    }
    return to_read;
}


/* Advance the read pointer `cnt' places. */
static inline void hal_stream_read_advance(ringbuffer_t *rb, size_t cnt)
{
    size_t tmp;
    ringheader_t *h = rb->header;

    /* ensure that previous reads (copies out of the ring buffer) are always
       completed before updating (writing) the read index.
       (write-after-read) => full barrier
    */
    rtapi_smp_mb();
    tmp = (h->head + cnt) & h->size_mask;
    h->head = tmp;
}


/* Reset the read and write pointers to zero. This is not thread safe.
 */
static inline void hal_stream_flush(ringbuffer_t *rb)
{
    ringheader_t *h = rb->header;

    h->head = 0;
    h->tail = 0;
    //memset(rb->buf, 0, h->size);
}

/* Return the number of bytes available for writing.  This is the
 * number of bytes in front of the write pointer and behind the read
 * pointer.
 */
static inline size_t hal_stream_write_space(const ringheader_t *h)
{
    size_t w, r;

    w = h->tail;
    r = h->head;
    if (w > r) {
	return ((r - w + h->size) & h->size_mask) - 1;
    } else if (w < r) {
	return (r - w) - 1;
    } else {
	return h->size - 1;
    }
}

/* The copying data writer.  Copy at most `cnt' bytes to `rb' from
 * `src'.  Returns the actual number of bytes copied.
 */
static inline size_t hal_stream_write(ringbuffer_t *rb, const char *src,
			size_t cnt)
{
    size_t free_cnt;
    size_t cnt2;
    size_t to_write;
    size_t n1, n2;
    ringheader_t *h = rb->header;

    if ((free_cnt = hal_stream_write_space (h)) == 0) {
	return 0;
    }
    to_write = cnt > free_cnt ? free_cnt : cnt;
    cnt2 = h->tail + to_write;
    if (cnt2 > h->size) {
	n1 = h->size - h->tail;
	n2 = cnt2 & h->size_mask;
    } else {
	n1 = to_write;
	n2 = 0;
    }
    memcpy (&(rb->buf[h->tail]), src, n1);
    h->tail = (h->tail + n1) & h->size_mask;
    if (n2) {
	memcpy (&(rb->buf[h->tail]), src + n1, n2);
	h->tail = (h->tail + n2) & h->size_mask;
    }
    return to_write;
}

/* Advance the write pointer `cnt' places. */
static inline void hal_stream_write_advance(ringbuffer_t *rb, size_t cnt)
{
    size_t tmp;
    ringheader_t *h = rb->header;

    /* ensure that previous writes are seen before we update the write index
       (write after write)
    */
    rtapi_smp_wmb();
    tmp = (h->tail + cnt) & h->size_mask;
    h->tail = tmp;
}



RTAPI_END_DECLS
#endif /* HAL_RING_PRIV_H */
