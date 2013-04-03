#ifndef RTAPI_RING_H
#define RTAPI_RING_H

//RTAPI_BEGIN_DECLS

// record mode: Negative numbers are needed for skips
typedef s32 ring_size_t;

// the ringbuffer shared data as laid out in
// the RTAPI shared memory segment
// defaults: record mode, no rmutex/wmutex use
typedef struct {
    u8 is_stream;      // record or stream mode
    u8 use_rmutex;     // hint to using code - use ringheader_t.rmutex
    u8 use_wmutex;     // hint to using code - use ringheader_t.wmutex

    int reader, writer;  // HAL module id's - informational // MOVE TO HAL LAYER FIXME
    unsigned long rmutex, wmutex; // optional use - if used by multiple readers/writers
    size_t scratchpad_size;
    size_t size_mask;    // stream mode only
    size_t size;         // common to stream and record mode
    size_t head __attribute__((aligned(16)));
     u64    generation;
    size_t tail __attribute__((aligned(16)));
    char buf[0];  // actual storage, plus scratchpad if any
} ringheader_t;

// the ringbuffer shared data as made accessible
// to the using code by rtapi_ring_getptr
// this structure lives in per-user memory, and points
// into the RTAPI shm segment of the ringbuffer
typedef struct {
    ringheader_t *header;
    char *buf;
    void *scratchpad;
} ringbuffer_t;

// using layer data structures
typedef struct {
    const ringbuffer_t *ring;
    u64   generation;
    size_t offset;
} ringiter_t;

typedef struct {
    void * rv_base;
    size_t rv_len;
} ringvec_t;

// mode flags passed in by rtapi_ring_new
// exposed in ringheader_t.mode
#define MODE_STREAM      _BIT(0)
#define USE_RMUTEX       _BIT(1)
#define USE_WMUTEX       _BIT(2)

#define RB_ALIGN 8

// Round up X to closest upper alignment boundary
static inline ring_size_t size_aligned(const ring_size_t x) // OK
{
    return x + (-x & (RB_ALIGN - 1));
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

#endif // RTAPI_RING_H
