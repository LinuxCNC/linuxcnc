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

struct rtapi_heap;
struct rtapi_heap_stat {
    size_t total_avail;
    size_t fragments;
    size_t largest;
};

void *rtapi_malloc(struct rtapi_heap *h, size_t nbytes);
void *rtapi_calloc(struct rtapi_heap *h, size_t n, size_t size);
void *rtapi_realloc(struct rtapi_heap *h, void *p, size_t size);
void  rtapi_free(struct rtapi_heap *h, void *p);
size_t rtapi_allocsize(void *p);

int rtapi_heap_init(struct rtapi_heap *h);
// any memory added to the heap must lie above the rtapi_heap structure:
int rtapi_heap_addmem(struct rtapi_heap *h, void *space, size_t size);
size_t rtapi_heap_status(struct rtapi_heap *h, struct rtapi_heap_stat *hs);

RTAPI_END_DECLS

#endif // _RTAPI_HEAP_INCLUDED
