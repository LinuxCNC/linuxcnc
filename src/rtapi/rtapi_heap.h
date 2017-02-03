#ifndef _RTAPI_HEAP_INCLUDED
#define _RTAPI_HEAP_INCLUDED

// functions for memory allocation in shared memory sgements

#ifdef __cplusplus
#define RTAPI_BEGIN_DECLS extern "C" {
#define RTAPI_END_DECLS }
#else
#define RTAPI_BEGIN_DECLS
#define RTAPI_END_DECLS
#endif

RTAPI_BEGIN_DECLS

#include "config.h"
#include "stdarg.h"
#include "rtapi_bitops.h" // RTAPI_BIT

// flags for _rtapi_heap_setflags()
#define RTAPIHEAP_TRACE_MALLOC RTAPI_BIT(0)
#define RTAPIHEAP_TRACE_FREE   RTAPI_BIT(1)
#define RTAPIHEAP_TRIM         RTAPI_BIT(2)  //  free alignment overallocations

struct rtapi_heap;
struct rtapi_heap_stat {
    size_t arena_size;
    size_t total_avail;
    size_t fragments;
    size_t largest;
    size_t requested;
    size_t allocated;
    size_t freed;
};

void  *_rtapi_malloc_aligned(struct rtapi_heap *h, size_t nbytes, size_t align);
void  *_rtapi_malloc(struct rtapi_heap *h, size_t nbytes);
void  *_rtapi_calloc(struct rtapi_heap *h, size_t n, size_t size);
void  *_rtapi_realloc(struct rtapi_heap *h, void *p, size_t size);
void   _rtapi_free(struct rtapi_heap *h, void *p);
size_t _rtapi_allocsize(struct rtapi_heap *h, const void *ap);
int   _rtapi_heap_init(struct rtapi_heap *h, const char *name);

// any memory added to the heap must lie above the rtapi_heap structure:
int _rtapi_heap_addmem(struct rtapi_heap *h, void *space, size_t size);

int    _rtapi_heap_setflags(struct rtapi_heap *heap, int flags);
size_t _rtapi_heap_status(struct rtapi_heap *h, struct rtapi_heap_stat *hs);

// callback for freelist iterator
typedef void (*chunk_t)(size_t size, void *chunk, void *user);
size_t _rtapi_heap_walk_freelist(struct rtapi_heap *h, chunk_t cb, void *user);

RTAPI_END_DECLS

#endif // _RTAPI_HEAP_INCLUDED
