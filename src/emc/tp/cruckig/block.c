/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */

#include "block.h"

static inline double cruckig_profile_total_duration(const CRuckigProfile *p) {
    return p->t_sum[6] + p->brake.duration + p->accel.duration;
}

static void remove_profile(CRuckigProfile *valid_profiles, size_t *valid_profile_counter, size_t index) {
    for (size_t i = index; i < *valid_profile_counter - 1; ++i) {
        valid_profiles[i] = valid_profiles[i + 1];
    }
    *valid_profile_counter -= 1;
}

static void interval_from_profiles(CRuckigInterval *iv, const CRuckigProfile *profile_left, const CRuckigProfile *profile_right) {
    const double left_duration = cruckig_profile_total_duration(profile_left);
    const double right_duration = cruckig_profile_total_duration(profile_right);
    if (left_duration < right_duration) {
        iv->left = left_duration;
        iv->right = right_duration;
        iv->profile = *profile_right;
    } else {
        iv->left = right_duration;
        iv->right = left_duration;
        iv->profile = *profile_left;
    }
    iv->valid = true;
}

void cruckig_block_init(CRuckigBlock *block) {
    cruckig_profile_init(&block->p_min);
    block->t_min = 0.0;
    block->a.valid = false;
    block->b.valid = false;
}

void cruckig_block_set_min_profile(CRuckigBlock *block, const CRuckigProfile *profile) {
    block->p_min = *profile;
    block->t_min = cruckig_profile_total_duration(profile);
    block->a.valid = false;
    block->b.valid = false;
}

bool cruckig_block_calculate(CRuckigBlock *block, CRuckigProfile *valid_profiles,
                     size_t valid_profile_counter, size_t max_profiles) {
    (void)max_profiles;

    if (valid_profile_counter == 1) {
        cruckig_block_set_min_profile(block, &valid_profiles[0]);
        return true;

    } else if (valid_profile_counter == 2) {
        if (fabs(valid_profiles[0].t_sum[6] - valid_profiles[1].t_sum[6]) < 8 * DBL_EPSILON) {
            cruckig_block_set_min_profile(block, &valid_profiles[0]);
            return true;
        }

        /* numerical_robust = true */
        {
            const size_t idx_min = (valid_profiles[0].t_sum[6] < valid_profiles[1].t_sum[6]) ? 0 : 1;
            const size_t idx_else_1 = (idx_min + 1) % 2;

            cruckig_block_set_min_profile(block, &valid_profiles[idx_min]);
            interval_from_profiles(&block->a, &valid_profiles[idx_min], &valid_profiles[idx_else_1]);
            return true;
        }

    /* Only happens due to numerical issues */
    } else if (valid_profile_counter == 4) {
        /* Find "identical" profiles */
        if (fabs(valid_profiles[0].t_sum[6] - valid_profiles[1].t_sum[6]) < 32 * DBL_EPSILON && valid_profiles[0].direction != valid_profiles[1].direction) {
            remove_profile(valid_profiles, &valid_profile_counter, 1);
        } else if (fabs(valid_profiles[2].t_sum[6] - valid_profiles[3].t_sum[6]) < 256 * DBL_EPSILON && valid_profiles[2].direction != valid_profiles[3].direction) {
            remove_profile(valid_profiles, &valid_profile_counter, 3);
        } else if (fabs(valid_profiles[0].t_sum[6] - valid_profiles[3].t_sum[6]) < 256 * DBL_EPSILON && valid_profiles[0].direction != valid_profiles[3].direction) {
            remove_profile(valid_profiles, &valid_profile_counter, 3);
        } else {
            return false;
        }

    } else if (valid_profile_counter % 2 == 0) {
        return false;
    }

    /* Find index of fastest profile */
    size_t idx_min = 0;
    for (size_t i = 1; i < valid_profile_counter; ++i) {
        if (valid_profiles[i].t_sum[6] < valid_profiles[idx_min].t_sum[6]) {
            idx_min = i;
        }
    }

    cruckig_block_set_min_profile(block, &valid_profiles[idx_min]);

    if (valid_profile_counter == 3) {
        const size_t idx_else_1 = (idx_min + 1) % 3;
        const size_t idx_else_2 = (idx_min + 2) % 3;

        interval_from_profiles(&block->a, &valid_profiles[idx_else_1], &valid_profiles[idx_else_2]);
        return true;

    } else if (valid_profile_counter == 5) {
        const size_t idx_else_1 = (idx_min + 1) % 5;
        const size_t idx_else_2 = (idx_min + 2) % 5;
        const size_t idx_else_3 = (idx_min + 3) % 5;
        const size_t idx_else_4 = (idx_min + 4) % 5;

        if (valid_profiles[idx_else_1].direction == valid_profiles[idx_else_2].direction) {
            interval_from_profiles(&block->a, &valid_profiles[idx_else_1], &valid_profiles[idx_else_2]);
            interval_from_profiles(&block->b, &valid_profiles[idx_else_3], &valid_profiles[idx_else_4]);
        } else {
            interval_from_profiles(&block->a, &valid_profiles[idx_else_1], &valid_profiles[idx_else_4]);
            interval_from_profiles(&block->b, &valid_profiles[idx_else_2], &valid_profiles[idx_else_3]);
        }
        return true;
    }

    return false;
}

/* cruckig_block_is_blocked is now inlined in block.h */

const CRuckigProfile* cruckig_block_get_profile(const CRuckigBlock *block, double t) {
    if (block->b.valid && t >= block->b.right) {
        return &block->b.profile;
    }
    if (block->a.valid && t >= block->a.right) {
        return &block->a.profile;
    }
    return &block->p_min;
}
