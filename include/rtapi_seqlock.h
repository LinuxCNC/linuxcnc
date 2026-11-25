//    Copyright 2026 Luca Toniolo
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef __LINUXCNC_RTAPI_SEQLOCK_H
#define __LINUXCNC_RTAPI_SEQLOCK_H

/* Split read protection for a struct in shared memory that one writer
   changes and readers copy.  The struct holds two plain unsigned int
   counters, head and tail, so it stays trivially copyable in C++.

   The writer calls rtapi_seqlock_write_begin() before it changes the
   struct and rtapi_seqlock_write_end() when done.  A reader copies with
   rtapi_seqlock_read() and keeps the copy only when it returns nonzero:
   tail read before the copy matched head read after it, so no write
   started or was under way while the copy was taken.  The fences keep
   the counters and the data in that order on weakly ordered CPUs. */

#include "rtapi_string.h"	/* memcpy() */

static inline void rtapi_seqlock_write_begin(unsigned int *head)
{
    __atomic_store_n(head, *head + 1, __ATOMIC_RELAXED);
    /* readers see the new head before any change to the struct */
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

static inline void rtapi_seqlock_write_end(unsigned int *tail, const unsigned int *head)
{
    /* readers see every change to the struct before the new tail */
    __atomic_store_n(tail, *head, __ATOMIC_RELEASE);
}

static inline int rtapi_seqlock_read(void *dst, const void *src, size_t size,
                                     const unsigned int *head, const unsigned int *tail)
{
    unsigned int t = __atomic_load_n(tail, __ATOMIC_ACQUIRE);
    if (__atomic_load_n(head, __ATOMIC_RELAXED) != t) {
        return 0;	/* a write is under way */
    }
    memcpy(dst, src, size);
    /* the copy is complete before head is read again */
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return __atomic_load_n(head, __ATOMIC_RELAXED) == t;
}

#endif
