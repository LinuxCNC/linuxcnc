// memory barrier primitives
// required for SMP-safe lock-free datastructures
// see https://www.kernel.org/doc/Documentation/memory-barriers.txt

// use gcc intrinsics

#define	rtapi_smp_mb()  __sync_synchronize()
#define	rtapi_smp_wmb() __sync_synchronize()
#define	rtapi_smp_rmb() __sync_synchronize()
