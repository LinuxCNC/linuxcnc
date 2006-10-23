#ifndef RTAPI_BITOPS_H
#define RTAPI_BITOPS_H
#if defined(KERNEL) || defined(ASM_BITOPS_H_USABLE)
#include <asm/bitops.h>
#else

#if defined(__i386__)
/* From <asm/bitops.h>
 * Copyright 1992, Linus Torvalds.
 */

#define LOCK_PREFIX "lock ; "
#define ADDR (*(volatile long *) addr)

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */
static __inline__ int test_and_set_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"Ir" (nr) : "memory");
	return oldbit;
}


/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */
static __inline__ int test_and_clear_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__( 
		"btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"Ir" (nr) : "memory");
	return oldbit;
}
#else
#error The header file <asm/bitops.h> is not usable and rtapi does not yet have support for your CPU
#endif
#endif
#endif
