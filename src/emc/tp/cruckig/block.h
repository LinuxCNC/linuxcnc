/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_BLOCK_H
#define CRUCKIG_BLOCK_H

#include "cruckig_internal.h"
#include "profile.h"

typedef struct {
    double left, right;
    CRuckigProfile profile;
    bool valid;
} CRuckigInterval;

typedef struct {
    CRuckigProfile p_min;
    double t_min;
    CRuckigInterval a;
    CRuckigInterval b;
} CRuckigBlock;

void cruckig_block_init(CRuckigBlock *block);
void cruckig_block_set_min_profile(CRuckigBlock *block, const CRuckigProfile *profile);

/* Calculate block from valid profiles. Returns true if successful. */
bool cruckig_block_calculate(CRuckigBlock *block, CRuckigProfile *valid_profiles,
                     size_t valid_profile_counter, size_t max_profiles);

/* Inlined for hot-path performance (called in tight synchronization loop) */
CRUCKIG_FORCE_INLINE bool cruckig_block_is_blocked(const CRuckigBlock *block, double t) {
    return (t < block->t_min)
        || (block->a.valid && block->a.left < t && t < block->a.right)
        || (block->b.valid && block->b.left < t && t < block->b.right);
}

const CRuckigProfile* cruckig_block_get_profile(const CRuckigBlock *block, double t);

#endif /* CRUCKIG_BLOCK_H */
