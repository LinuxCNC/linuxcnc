/********************************************************************
* Description: atomic_9d.h
*   Atomic operations for 9D planner lock-free queue
*
*   Ported from Tormach LinuxCNC implementation.
*   Uses SPSC (single-producer/single-consumer) model:
*   - Producer (userspace) ONLY writes to 'end' index
*   - Consumer (RT) ONLY writes to 'start' index
*   This eliminates race conditions without needing CAS loops.
*
* Author: Tormach (original), Port by LinuxCNC community
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2022-2026 All rights reserved.
*
********************************************************************/
#ifndef ATOMIC_9D_H
#define ATOMIC_9D_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Atomically increment queue index with wraparound
 *
 * For SPSC (single-producer/single-consumer) model:
 * Each index is only written by ONE thread, so no CAS needed.
 * Just load, compute, and exchange atomically.
 *
 * @param idx Pointer to index to increment
 * @param queue_size Size of circular buffer
 * @return New index value after increment
 *
 * MEMORY ORDERING:
 * - ACQUIRE on load: ensures we see all prior writes
 * - ACQ_REL on exchange: publishes new value with full barrier
 */
static inline int atomicIncrementIndex(int *idx, int queue_size)
{
    int new_idx = ((__atomic_load_n(idx, __ATOMIC_ACQUIRE)) + 1) % queue_size;
    __atomic_exchange_n(idx, new_idx, __ATOMIC_ACQ_REL);
    return new_idx;
}

/**
 * @brief Atomically decrement queue index with wraparound
 *
 * For SPSC model - same logic as increment but subtracts.
 *
 * @param idx Pointer to index to decrement
 * @param queue_size Size of circular buffer
 * @return New index value after decrement
 */
static inline int atomicDecrementIndex(int *idx, int queue_size)
{
    int new_idx = ((__atomic_load_n(idx, __ATOMIC_ACQUIRE)) - 1 + queue_size) % queue_size;
    __atomic_exchange_n(idx, new_idx, __ATOMIC_ACQ_REL);
    return new_idx;
}

/**
 * @brief Atomically load an integer value
 *
 * @param ptr Pointer to value to load
 * @return Loaded value
 */
static inline int atomicLoadInt(const int *ptr)
{
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

/**
 * @brief Atomically store an integer value
 *
 * @param ptr Pointer to location to store
 * @param value Value to store
 */
static inline void atomicStoreInt(int *ptr, int value)
{
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

/**
 * @brief Atomically load a double value
 *
 * Note: On most platforms, double loads/stores are not atomic.
 * This function provides atomic semantics using compiler built-ins.
 *
 * @param ptr Pointer to value to load
 * @return Loaded value
 */
static inline double atomicLoadDouble(const double *ptr)
{
    // Use memcpy to avoid strict aliasing issues
    double value;
    __atomic_load(ptr, &value, __ATOMIC_ACQUIRE);
    return value;
}

/**
 * @brief Atomically store a double value
 *
 * @param ptr Pointer to location to store
 * @param value Value to store
 */
static inline void atomicStoreDouble(double *ptr, double value)
{
    __atomic_store(ptr, &value, __ATOMIC_RELEASE);
}

#ifdef __cplusplus
}
#endif

#endif // ATOMIC_9D_H
