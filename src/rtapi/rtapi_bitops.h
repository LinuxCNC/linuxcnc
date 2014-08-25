//    Copyright 2006-2007 various authors
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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef RTAPI_BITOPS_H
#define RTAPI_BITOPS_H

// determine which flavor of atomic ops to use:
// the __sync_* legacy ops in gcc (<4.7) or the
// __atomic_* operations in gcc 4.7 and onwards, and llvm
//
// see also:
// http://gcc.gnu.org/onlinedocs/gcc-4.7.3/gcc/_005f_005fatomic-Builtins.html#g_t_005f_005fatomic-Builtins
// http://www.mjmwired.net/kernel/Documentation/atomic_ops.txt

#if defined(__clang__) && __clang__
#define RTAPI_USE_ATOMIC 1

// the __ATOMIC_* memory model macros will become available with C11 / C++11 only
// see http://clang.llvm.org/doxygen/InitPreprocessor_8cpp_source.html
#ifndef __ATOMIC_SEQ_CST
#define __ATOMIC_SEQ_CST 5
#endif
#elif defined(__GNUC__) && \
    (__GNUC__*100 + __GNUC_MINOR__*10 + __GNUC_PATCHLEVEL__ >= 470)
#define RTAPI_USE_ATOMIC 1
#else
#define RTAPI_USE_ATOMIC 0
#endif

// the Linux kernel has very nice bitmap handling
// unfortunately it is not available through /usr/include
// therefore replicate from linux/bitops.h and linux/kernel.h
// and prefix with an 'RTAPI_' to roll our own

typedef unsigned long rtapi_atomic_type;

#define RTAPI_CHAR_BIT 8
#define RTAPI_DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define RTAPI_BIT(nr)           (1UL << (nr))
#define RTAPI_BITS_PER_LONG     (RTAPI_CHAR_BIT * sizeof(rtapi_atomic_type))
#define RTAPI_BIT_MASK(nr)      (1UL << ((nr) % RTAPI_BITS_PER_LONG))
#define RTAPI_BIT_WORD(nr)      ((nr) / RTAPI_BITS_PER_LONG)
#define RTAPI_BITMAP_SIZE(nr)   RTAPI_DIV_ROUND_UP(nr, RTAPI_BITS_PER_LONG)
#define RTAPI_BITMAP_BYTES(nr) \
    (sizeof(rtapi_atomic_type) * RTAPI_BITMAP_SIZE(nr))
#define RTAPI_BIT_SET(a, b)     ((a)[RTAPI_BIT_WORD(b)] |=   RTAPI_BIT_MASK(b))
#define RTAPI_BIT_CLEAR(a, b)   ((a)[RTAPI_BIT_WORD(b)] &= ~ RTAPI_BIT_MASK(b))
#define RTAPI_BIT_TEST(a, b)    ((a)[RTAPI_BIT_WORD(b)] &    RTAPI_BIT_MASK(b))

#define RTAPI_DECLARE_BITMAP(name,bits) \
    rtapi_atomic_type name[RTAPI_BITMAP_SIZE(bits)]
#define RTAPI_ZERO_BITMAP(name,bits) memset(name, 0, RTAPI_BITMAP_BYTES(bits))
#define RTAPI_SET_BITMAP(name,bits) memset(name, ~0, RTAPI_BITMAP_BYTES(bits))

#if RTAPI_USE_ATOMIC

// http://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync
// default: Full barrier in both directions and synchronizes with
// acquire loads and release stores in all threads.
#define RTAPI_MEMORY_MODEL __ATOMIC_SEQ_CST

static inline int rtapi_test_and_set_bit(int nr, rtapi_atomic_type *bitmap)
{
    return (__atomic_fetch_or(bitmap + RTAPI_BIT_WORD(nr),
			      RTAPI_BIT_MASK(nr),
			      RTAPI_MEMORY_MODEL)
	    & RTAPI_BIT_MASK(nr)) != 0;
}

static inline rtapi_atomic_type rtapi_test_and_clear_bit(int nr, rtapi_atomic_type * bitmap)
{
    return (__atomic_fetch_and(bitmap + RTAPI_BIT_WORD(nr),
			       ~RTAPI_BIT_MASK(nr),
			       RTAPI_MEMORY_MODEL)
	    & RTAPI_BIT_MASK(nr)) != 0;
}

static inline void rtapi_set_bit(int nr, rtapi_atomic_type * bitmap)
{
    __atomic_or_fetch(bitmap + RTAPI_BIT_WORD(nr),
		      RTAPI_BIT_MASK(nr),
		      RTAPI_MEMORY_MODEL);
}

static inline void rtapi_clear_bit(int nr, rtapi_atomic_type * bitmap)
{
    __atomic_and_fetch(bitmap + RTAPI_BIT_WORD(nr),
		       ~RTAPI_BIT_MASK(nr),
		       RTAPI_MEMORY_MODEL);
}

static inline rtapi_atomic_type rtapi_test_bit(int nr, rtapi_atomic_type * const bitmap)
{
    return  (__atomic_fetch_or(bitmap + RTAPI_BIT_WORD(nr),
			       0,
			      RTAPI_MEMORY_MODEL)
	     & RTAPI_BIT_MASK(nr)) != 0;
}

static inline rtapi_atomic_type rtapi_add_and_fetch(int delta,
						    rtapi_atomic_type * const value)
{
    return __atomic_add_fetch (value, delta, RTAPI_MEMORY_MODEL);
}

static inline rtapi_atomic_type rtapi_subtract_and_fetch(int delta,
							 rtapi_atomic_type * const value)
{
    return __atomic_sub_fetch (value, delta, RTAPI_MEMORY_MODEL);
}

#else // ! RTAPI_USE_ATOMIC - use gcc legacy atomic operations

static inline int rtapi_test_and_set_bit(int nr, rtapi_atomic_type *bitmap)
{
    return (__sync_fetch_and_or(bitmap + RTAPI_BIT_WORD(nr),
				RTAPI_BIT_MASK(nr))
	    & RTAPI_BIT_MASK(nr)) != 0;
}

static inline rtapi_atomic_type rtapi_test_and_clear_bit(int nr, rtapi_atomic_type * bitmap)
{
    return (__sync_fetch_and_and(bitmap + RTAPI_BIT_WORD(nr),
				 ~RTAPI_BIT_MASK(nr))
	    & RTAPI_BIT_MASK(nr)) != 0;
}

static inline void rtapi_set_bit(int nr, rtapi_atomic_type * bitmap)
{
    __sync_or_and_fetch(bitmap + RTAPI_BIT_WORD(nr),
			RTAPI_BIT_MASK(nr));
}

static inline void rtapi_clear_bit(int nr, rtapi_atomic_type * bitmap)
{
    __sync_and_and_fetch(bitmap + RTAPI_BIT_WORD(nr),
			 ~RTAPI_BIT_MASK(nr));
}

static inline rtapi_atomic_type rtapi_test_bit(int nr, rtapi_atomic_type * const bitmap)
{
    return  (__sync_fetch_and_or(bitmap + RTAPI_BIT_WORD(nr), 0)
	     & RTAPI_BIT_MASK(nr)) != 0;
}

static inline rtapi_atomic_type rtapi_add_and_fetch(int delta,
						    rtapi_atomic_type * const value)
{
    return  __sync_add_and_fetch (value, delta);
}

static inline rtapi_atomic_type rtapi_subtract_and_fetch(int delta,
							 rtapi_atomic_type * const value)
{
    return  __sync_sub_and_fetch (value, delta);
}
#endif // ! RTAPI_USE_ATOMIC
#endif // RTAPI_BITOPS_H
