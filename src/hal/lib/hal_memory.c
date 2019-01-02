// HAL memory allocator functions

// the heap where HAL descriptors are stored
// grows upward from right after other hal_data data items

// This heap is unavailable until after init_hal_data()
// referenced through &hal_data->heap

// the top of HAL shm is used for  downwards allocation
// of RT storage through shmalloc_rt() as used by hal_malloc()

// any non-RT related stuff like strings goes
// to the heap in the global segment
// global_heap is available at hal_lib load time since expored by rtapi
// the only place where the global heap is referenced is hal_object.c
// extern struct rtapi_heap *global_heap;


//NB: take care to return memory to the proper heap.

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

// part of public API
void *halg_malloc(const int use_hal_mutex, size_t size)
{
    PCHECK_NULL(hal_data);
    {
        WITH_HAL_MUTEX_IF(use_hal_mutex);

	void *retval = shmalloc_rt(size);
	if (retval == NULL)
	    HALERR("out of rt memory - allocating %zu bytes", size);

	hal_data->hal_malloced += size; // beancounting
	return retval;
    }
}

// HAL library internal use only

void shmfree_desc(void *p)
{
    if (hal_data == NULL) {
	HALERR("freeing NULL pointer");
	return;
    }
    rtapi_free(&hal_data->heap, p);
}

int hal_heap_addmem(size_t click)
{
    size_t actual = RTAPI_ALIGN(click, RTAPI_CACHELINE);

    HALDBG("extending arena by %zu bytes", actual);

    if (hal_freemem() < HAL_HEAP_MINFREE) {
	HALERR("can't extend arena - below minfree: %zu", hal_freemem());
	return 0;
    }

    if (rtapi_heap_addmem(&hal_data->heap,
			  SHMPTR(hal_data->shmem_bot),
			  actual)) {
	HALFAIL_RC(ENOMEM, "rtapi_heap_addmem(%zu) failed", actual);
    }
    hal_data->shmem_bot += actual;
    return 0;
}

// must be called with HAL mutex held
void *shmalloc_desc(size_t size)
{
    void *retval = rtapi_calloc(&hal_data->heap, 1, size);

    // extend shm arena on failure
    if (retval == NULL) {
	hal_heap_addmem(HAL_HEAP_INCREMENT);

	retval = rtapi_calloc(&hal_data->heap, 1, size);
	if (retval == NULL)
	    HALFAIL_NULL(ENOMEM,
			 "giving up - can't allocate %zu bytes", size);
    }
    memset(retval, 0, size);
    return retval;
}

void *shmalloc_desc_aligned(size_t size, size_t alignment)
{
    // force arena expansion by alloc/free just in case
    // so the following internal malloc in rtapi_malloc_aligned()
    // does not fail
    // not terribly elegant but effective
    void *dummy = shmalloc_desc(size + alignment);
    if (dummy) {
	shmfree_desc(dummy);
    } else
	return NULL;

    void *ptr = rtapi_malloc_aligned(&hal_data->heap,
				     size,
				     alignment);
    if (ptr == NULL)
	HALFAIL_NULL(ENOMEM, "insufficient memory for %zu, align=%zu",
		     size, alignment);
    HAL_ASSERT(is_aligned(ptr, alignment));
    memset(ptr, 0, size);
    return ptr;
}

void *shmalloc_rt(size_t size)
{
    long int tmp_top;
    void *retval;

    /* tentatively allocate memory */
    tmp_top = hal_data->shmem_top - size;
    /* deal with alignment requirements */
    if (size >= 8) {
	/* align on 8 byte boundary */
	tmp_top &= (~7);
    } else if (size >= 4) {
	/* align on 4 byte boundary */
	tmp_top &= (~3);
    } else if (size == 2) {
	/* align on 2 byte boundary */
	tmp_top &= (~1);
    }
    /* is there enough memory available? */
    if (tmp_top < hal_data->shmem_bot) {
	/* no */
	HALFAIL_NULL(ENOMEM, "giving up - can't allocate %zu bytes", size);
    }
    size_t waste = hal_data->shmem_top - tmp_top - size;
    hal_data->rt_alignment_loss += waste;

    /* memory is available, allocate it */
    retval = SHMPTR(tmp_top);
    hal_data->shmem_top = tmp_top;
    return retval;
}

void report_heapstatus(const char *tag,  struct rtapi_heap *h)
{
	struct rtapi_heap_stat hs = {};
	rtapi_heap_status(h, &hs);
	HALDBG("%s heap status\n", tag);
	HALDBG("  arena=%zu totail_avail=%zu fragments=%zu largest=%zu\n",
	       hs.arena_size, hs.total_avail, hs.fragments, hs.largest);
	HALDBG("  requested=%zu allocated=%zu freed=%zu waste=%zu%%\n",
	       hs.requested, hs.allocated, hs.freed,
	       hs.allocated ?
	       (hs.allocated - hs.requested)*100/hs.allocated : 0);
}

void report_memory_usage(void)
{
	report_heapstatus("HAL heap", &hal_data->heap);

	report_heapstatus("global heap", global_heap);
	HALDBG("  strings on global heap: alloc=%zu freed=%zu balance=%zu\n",
	       hal_data->str_alloc,
	       hal_data->str_freed,
	       hal_data->str_alloc - hal_data->str_freed);

	size_t rtalloc = (size_t)(global_data->hal_size - hal_data->shmem_top);
	HALDBG("  RT objects: %zu  alignment loss: %zu  (%zu%%)\n",
	       rtalloc,
	       hal_data->rt_alignment_loss,
	       rtalloc ?
	       (hal_data->rt_alignment_loss * 100/rtalloc) : 0);
	HALDBG("  hal_malloc():   %zu\n",
	       hal_data->hal_malloced);
	HALDBG("  unused:   %ld\n",
	       (long)( hal_data->shmem_top - hal_data->shmem_bot));

}

// Added ArcEye 11082015 for use with instanceparam strings

char *halg_strdup(const int use_hal_mutex, const char *s)
{
    PCHECK_STRLEN(s, HAL_MAX_NAME_LEN);
    PCHECK_NULL(s);
    size_t sz = strlen(s);
    char *p = rtapi_calloc(global_heap, 1, sz + 1); // include trailing zero
    if (p == NULL) {
        HALFAIL_NULL(ENOMEM,
		     "out of memory allocating %zu bytes for '%s'",
		     sz+1, s);
    }
    strcpy(p, s);
    hal_data->str_alloc += (sz + 1);
    return p;
}

int halg_free_str(char **s)
{
    CHECK_NULL(s);
    hal_data->str_freed += strlen(*s) + 1;
    rtapi_free(global_heap, *s);
    *s = NULL;
    return 0;
}

int halg_free_single_str(char *s)
{
    CHECK_NULL(s);
    hal_data->str_freed += strlen(s) + 1;
    rtapi_free(global_heap, s);
    s = NULL;
    return 0;
}

char **halg_dupargv(const bool use_hal_mutex,
		    const int argc,
		    char * const *argv)
{
    int i;
    if (argc > MAX_ARGC)
	HALFAIL_NULL(EINVAL,"argv too large: argc=%d", argc);

    if (argv == NULL)
	return 0;

    char **nargv =  rtapi_calloc(global_heap, sizeof(char *), argc + 1);
    if (nargv == NULL)
	HALFAIL_NULL(ENOMEM, "argc=%d", argc);

    for (i = 0; i < argc - 1; i++) {
	nargv[i] = halg_strdup(0, argv[i]);
	if (nargv[i] == NULL)
	    HALFAIL_NULL(ENOMEM, "i=%d",i);
    }
    nargv[argc] = NULL;
    return nargv;
}

int halg_free_argv(const bool use_hal_mutex,
		   char **argv)
{
    WITH_HAL_MUTEX_IF(use_hal_mutex);

    if (argv == NULL)
	return 0;
    char **s = argv;
    while (*s) {
	halg_free_str(s);
	s++;
    }
    rtapi_free(global_heap, argv);
    return 0;
}
