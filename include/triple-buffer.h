#ifndef _TRIPLE_BUFFER_H
#define _TRIPLE_BUFFER_H
#include <stdbool.h>
#include <memory.h>
#include <rtapi_atomics.h>

// A triple buffer is a message passing scheme for a single producer, and single consumer
//
// It is applicable if:
// - producer and consumer have different production/consumption rates
// - the consumer is only interested in data which was most recently updated by the producer.
// - both sides have full control when to switch to work on an update/to an updated view
// - neither side switching impacts the other side without explicit action, meaning
//   preparing a new message, or consuming a message will never be impacted by actions
//   of the other party.
//
// It is NOT applicable if the data to be conveyed represents a stream of events,
// since the last-update-wins property means intermediate updates (and hence events) are lost.
//
// A typical usage scenario is a component which:
//
// - has two thread functs (possibly called by different threads)
// - one (slow) funct preparing a set of driving variables for the other (fast) funct,
//   the driving variables being write-only by the slow funct, and read-only by the fast funct
// - the slow funct might do several intermediate steps until a complete set
//   of driving state is computed
// - the driving variables should be passed to the fast funct in an atomic operation
// - the fast funct full control over when to switch to an updated view.

// IMPORTANT: usage in a SMP-scenario (functs executed by different threads on possibly different cores):
// to assure all data is propagated to the buffer the before a flip():
//     ALWAYS execute a write barrier before rtapi_tb_flip(&tb):

// data structures:
// TB_FLAG(tb);      // declares a triple buffer containing clean, dirty and snapshot views
// struct buffer[3]; // the buffers indexed by rtapi_tb_write_idx() and rtapi_tb_snap_idx()
//
// initialisation:
//    rtapi_tb_init(&tb)
//
// producer methods:
// -----------------
//
// wi = rtapi_tb_write_idx(&tb):
//     retrieve the index of the clean write buffer
//     data written to/read from buffer[wi] will be private to the producer until
//     the rtapi_tb_flip() operation
//
// rtapi_tb_flip(&tb):
//     swaps the index of clean and dirty.
//     after a flip, the writer must retrieve the index of the clean write buffer
//     again using rtapi_tb_write_idx().
//
// consumer methods:
// -----------------
//
// bool rtapi_tb_snapshot(&tb):
//     determine if an updated (dirty) buffer is available.
//     will clear the 'new data available' flag in tb.
//
// si = rtapi_tb_snap_idx(&tb):
//     exchange dirty and snapshot index.
//     call only if rtapi_tb_snapshot() returns true.
//
// typical usage in an RT component:
// =================================
//
// initialisation (per component or per instance):
// ---------------------------------
// - declare the tb flag and buffer in the instance data
// - initialize with rtapi_tb_init()
// - note that initially there is no data available for the consumer.
//   if the consumer funct is to always have a snapshot available,
//   prepare the buffer in the setup code and rtapi_tb_flip(). This
//   guarantees that the consumer always has a snapshot at hand.
// - in the consumer private data, declare a pointer to a buffer and
//   initialize to NULL.
//
// producer funct flow:
// --------------------
// - retrieve the current write index with rtapi_tb_write_idx(&tb)
// - update values in buffer[write index] as needed
// - once an update is complete, rtapi_tb_flip().
// NB:
// an update to the current buffer may span several funct
// invocations, there is no requirement to finish in a single funct
// call - rtapi_tb_write_idx(&tb) will not change until after a flip.
//
// Important: initially, and after a flip, data in the current buffer is not valid.
// when preparing a new buffer, make sure ALL fields are set before the flip!

// consumer funct flow:
// --------------------
// - determine if a new snapshot is available with rtapi_tb_snapshot(&tb)
// - if true:
//      obtain its index with rtapi_tb_snap_idx(&tb)
//      store a pointer to the current snapshot in the funct private data.
//
// rest of funct: refer to data though the pointer to the current snapshot.
//
// Note that the snapshot pointer will be NULL if no initial flip() has been done
// in the initialisation code. To avoid repeated testing for NULL, it is best
// to always prepare an inital view and flip as outlined above.

// declare a triple buffer like so:
//
// struct foo {
//     int whatever;
// }
//
// TB_FLAG(foo_flag);         // alternatively:
// TB_FLAG_FAST(foo_flag);    // a cache-friendly flag trading off memory for speed
// struct foo foo_buffer[3];  // actual triple buffer
//
// initialisation code:
//     rtapi_tb_init(&foo_flag);
//     // prepare the initial view:
//     foo_buffer[rtapi_tb_write_idx(&foo_flag)].whatever = 123;
//     rtapi_tb_flip(&foo_flag);
//
// producer:
//     // write to the current buffer:
//     foo_buffer[rtapi_tb_write_idx(&foo_flag)].whatever = 42;
//
//     // once complete, write barrier before we flip the index
//	rtapi_smp_wmb();
//
//     // only after the barrier flip the current write buffer:
//     rtapi_tb_flip(&foo_flag);
//
// consumer:
//
// struct foo *snap = NULL;
//
// test for a new snapshot, take one if so, and treat it like local variablest:
//
// if (rtapi_tb_snapshot(&foo_flag)) {
//      // new snapshot data available
//      // access like so:
//      snap = foo_buffer[rtapi_tb_snap_idx(&foo_flag)]
// } else {
//       // no new snapshot available
// }
// refer to snap->whatever here


// idea taken from:
// http://remis-thoughts.blogspot.co.at/2012/01/triple-buffering-as-concurrency_30.html
// rewritten to use rtapi_cas
// see also: https://www.reddit.com/r/programming/comments/p1zm5/triple_buffering_as_a_concurrency_mechanism/?sort=old




// declare a triple buffer flag:
#define TB_FLAG(name) hal_u8_t name

// cache-friendly version of triple buffer flag declaration
#define TB_FLAG_FAST(name)					\
    hal_u8_t name __attribute__((aligned(RTAPI_CACHELINE)));	\
    char __##name##pad[RTAPI_CACHELINE - sizeof(hal_u8_t)];

// bit flags are (unused) (new write) (2x dirty) (2x clean) (2x snap)
// initially dirty = 0, clean = 1 and snap = 2
static inline void rtapi_tb_init(hal_u8_t *flags) {
    *flags = 0x6;
}

// index of current snap buffer
static inline hal_u8_t rtapi_tb_snap_idx(hal_u8_t *flags) {
    return rtapi_load_u8(flags) & 0x3;
}

// index of current write buffer
static inline hal_u8_t rtapi_tb_write_idx(hal_u8_t *flags) {
    return (rtapi_load_u8(flags) & 0x30) >> 4;
}

// create a new snapshot.
// returns false if no new data available.
static inline bool rtapi_tb_snapshot(hal_u8_t *flags) {
	hal_u8_t flagsNow = rtapi_load_u8(flags);
	hal_u8_t newFlags;
	do {
	    if ((flagsNow & 0x40) == 0)
		return false;
	    newFlags = (flagsNow & 0x30) |
		((flagsNow & 0x3) << 2) |
		((flagsNow & 0xC) >> 2);
	} while (!rtapi_cas_u8(flags,  flagsNow, newFlags));
	return true;
}

// flip the write buffer
static inline void rtapi_tb_flip(hal_u8_t *flags) {
    hal_u8_t flagsNow;
    hal_u8_t newFlags;
    do {
	flagsNow = rtapi_load_u8(flags);
	newFlags = 0x40 |
	    ((flagsNow & 0xC) << 2) |
	    ((flagsNow & 0x30) >> 2) |
	    (flagsNow & 0x3);
    } while (!rtapi_cas_u8(flags,  flagsNow, newFlags));
}

#endif
