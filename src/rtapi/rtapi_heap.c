/********************************************************************
 * Copyright (C) 2014 Michael Haberler <license AT mah DOT priv DOT at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ********************************************************************/

#include "rtapi.h"
#include "rtapi_heap.h"
#include "rtapi_heap_private.h"
#include "rtapi_export.h"
#include "rtapi_bitops.h"

// this is straight from the malloc code in:
// K&R The C Programming Language, Edition 2, pages 185-189
// adapted to use offsets relative to the heap descriptor
// so it can be used in as a shared memory resident malloc

// scoped lock helper
static void malloc_autorelease_mutex(rtapi_atomic_type **mutex) {
    rtapi_mutex_give(*mutex);
}

void rtapi_free(struct rtapi_heap *h, void *);

void *rtapi_malloc(struct rtapi_heap *h, size_t nbytes)
{
    unsigned long *m __attribute__((cleanup(malloc_autorelease_mutex))) = &h->mutex;
    rtapi_mutex_get(m);

    rtapi_malloc_hdr_t *p, *prevp;
    size_t nunits  = (nbytes + sizeof(rtapi_malloc_hdr_t) - 1) / sizeof(rtapi_malloc_hdr_t) + 1;

    // heaps are explicitly initialized, see rtapi_heap_init()
    // if ((prevp = h->freep) == NULL) {	// no free list yet
    // 	h->base.s.ptr = h->freep = prevp = &h->base;
    // 	h->base.s.size = 0;
    // }
    rtapi_malloc_hdr_t *freep = heap_ptr(h, h->free_p);

    prevp = freep;
    for (p = heap_ptr(h, prevp->s.next); ; prevp = p, p = heap_ptr(h, p->s.next)) {
	if (p->s.size >= nunits) {	/* big enough */
	    if (p->s.size == nunits)	/* exactly */
		prevp->s.next = p->s.next;
	    else {				/* allocate tail end */
		p->s.size -= nunits;
		p += p->s.size;
		p->s.size = nunits;
	    }
	    h->free_p = heap_off(h, prevp);
	    return (void *)(p+1);
	}
	if (p == freep)	{	/* wrapped around free list */
	    //rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_malloc: out of memory (size=%zu arena=%zu)", nbytes, h->arena_size);
	    //if ((p = morecore(nunits)) == NULL)
	    return NULL;	/* none left */
	}
    }
}

void rtapi_free(struct rtapi_heap *h,void *ap)
{
    unsigned long *m __attribute__((cleanup(malloc_autorelease_mutex))) = &h->mutex;
    rtapi_mutex_get(m);

    rtapi_malloc_hdr_t *bp, *p;
    rtapi_malloc_hdr_t *freep =  heap_ptr(h,h->free_p);

    bp = (rtapi_malloc_hdr_t *)ap - 1;	// point to block header
    for (p = freep; !(bp > p && bp < (rtapi_malloc_hdr_t *)heap_ptr(h,p->s.next)); p = heap_ptr(h,p->s.next))
	if (p >= (rtapi_malloc_hdr_t *)heap_ptr(h,p->s.next) &&
	    (bp > p || bp < (rtapi_malloc_hdr_t *)heap_ptr(h,p->s.next))) {
	    // freed block at start or end of arena
	    //rtapi_print_msg(RTAPI_MSG_DBG, "freed block at start or end of arena");
	    break;
	}

    if (bp + bp->s.size == ((rtapi_malloc_hdr_t *)heap_ptr(h,p->s.next))) { // join to upper neighbor
	bp->s.size += ((rtapi_malloc_hdr_t *)heap_ptr(h,p->s.next))->s.size;
	bp->s.next = ((rtapi_malloc_hdr_t *)heap_ptr(h,p->s.next))->s.next;
	//rtapi_print_msg(RTAPI_MSG_DBG, "join upper");
    } else
	bp->s.next = p->s.next;
    if (p + p->s.size == bp) {		/* join to lower nbr */
	p->s.size += bp->s.size;
	p->s.next = bp->s.next;
	//rtapi_print_msg(RTAPI_MSG_DBG, "join lower");
    } else
	p->s.next = heap_off(h,bp);
    h->free_p = heap_off(h,p);
}

size_t rtapi_allocsize(void *ap)
{
    rtapi_malloc_hdr_t *p = (rtapi_malloc_hdr_t *) ap - 1;
    return p->s.size * sizeof (rtapi_malloc_hdr_t);
}

void *rtapi_calloc(struct rtapi_heap *h, size_t nelem, size_t elsize)
{
    void *p = rtapi_malloc (h,nelem * elsize);
    if (!p)
        return NULL;
    memset(p, 0, nelem * elsize);
    return p;
}

void *rtapi_realloc(struct rtapi_heap *h, void *ptr, size_t size)
{
    void *p = rtapi_malloc (h, size);
    if (!p)
        return (p);
    size_t sz = rtapi_allocsize (ptr);
    memcpy(p, ptr, (sz > size) ? size : sz);
    rtapi_free(h, ptr);
    return p;
}

size_t rtapi_print_freelist(struct rtapi_heap *h)
{
    size_t free = 0;
    rtapi_malloc_hdr_t *p, *prevp, *freep = heap_ptr(h,h->free_p);
    prevp = freep;
    for (p = heap_ptr(h,prevp->s.next); ; prevp = p, p = heap_ptr(h,p->s.next)) {
	if (p->s.size) {
	    //rtapi_print_msg(RTAPI_MSG_DBG, "%d at %p", p->s.size * sizeof(rtapi_malloc_hdr_t),(void *)(p + 1));
	    free += p->s.size;
	}
	if (p == freep) {
	    //rtapi_print_msg(RTAPI_MSG_DBG, "end of free list %p",p);
	    return free;
	}
    }
}

int rtapi_heap_addmem(struct rtapi_heap *h, void *space, size_t size)
{
    if (space < (void*) h) return -EINVAL;
    if (size < RTAPI_HEAP_MIN_ALLOC) return -EINVAL;
    rtapi_malloc_hdr_t *arena = space;
    arena->s.size = size / sizeof(rtapi_malloc_hdr_t);
    rtapi_free(h, (void *) (arena + 1));
    return 0;
}

int rtapi_heap_init(struct rtapi_heap *heap)
{
    heap->base.s.next = 0; // because the first element in the heap ist the header
    heap->free_p = 0;      // and free list sentinel
    heap->base.s.size = 0;
    heap->mutex = 0;
    heap->arena_size = 0;
    return 0;
}

size_t rtapi_heap_status(struct rtapi_heap *h, struct rtapi_heap_stat *hs)
{
    hs->total_avail = 0;
    hs->fragments = 0;
    hs->largest = 0;

    rtapi_malloc_hdr_t *p, *prevp, *freep = heap_ptr(h, h->free_p);
    prevp = freep;
    for (p = heap_ptr(h, prevp->s.next); ; prevp = p, p = heap_ptr(h, p->s.next)) {
	if (p->s.size) {
	    hs->fragments++;
	    hs->total_avail += p->s.size;
	    if (p->s.size > hs->largest)
		hs->largest = p->s.size;
	}
	if (p == freep) {
	    hs->total_avail *= sizeof(rtapi_malloc_hdr_t);
	    hs->largest *= sizeof(rtapi_malloc_hdr_t);
	    return hs->largest;
	}
    }
}

#if 0 // assumes rtapi_print_msg() and hence RTAPI
size_t rtapi_print_freelist(struct rtapi_heap *h)
{
    size_t free = 0;
    rtapi_malloc_hdr_t *p, *prevp, *freep = heap_ptr(h,h->free_p);
    prevp = freep;
    for (p = heap_ptr(h,prevp->s.next); ; prevp = p, p = heap_ptr(h,p->s.next)) {
	if (p->s.size) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "%d at %p", p->s.size * sizeof(rtapi_malloc_hdr_t),(void *)(p + 1));
	    free += p->s.size;
	}
	if (p == freep) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "end of free list %p",p);
	    return free;
	}
    }
}
#endif

#ifdef RTAPI
EXPORT_SYMBOL(rtapi_malloc);
EXPORT_SYMBOL(rtapi_calloc);
EXPORT_SYMBOL(rtapi_realloc);
EXPORT_SYMBOL(rtapi_free);
EXPORT_SYMBOL(rtapi_allocsize);
EXPORT_SYMBOL(rtapi_heap_init);
EXPORT_SYMBOL(rtapi_heap_addmem);
EXPORT_SYMBOL(rtapi_heap_status);
#endif
