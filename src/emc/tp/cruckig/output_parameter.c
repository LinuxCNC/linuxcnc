/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#include "output_parameter.h"


CRuckigOutputParameter* cruckig_output_create(size_t dofs) {
    CRuckigOutputParameter *out = (CRuckigOutputParameter*)cruckig_calloc(1, sizeof(CRuckigOutputParameter));
    if (!out) return NULL;

    out->degrees_of_freedom = dofs;

    out->trajectory = cruckig_trajectory_create(dofs);
    if (!out->trajectory) {
        cruckig_free(out);
        return NULL;
    }

    out->new_position     = (double*)cruckig_calloc(dofs, sizeof(double));
    out->new_velocity     = (double*)cruckig_calloc(dofs, sizeof(double));
    out->new_acceleration = (double*)cruckig_calloc(dofs, sizeof(double));
    out->new_jerk         = (double*)cruckig_calloc(dofs, sizeof(double));

    if (!out->new_position || !out->new_velocity ||
        !out->new_acceleration || !out->new_jerk) {
        cruckig_output_destroy(out);
        return NULL;
    }

    out->time = 0.0;
    out->new_section = 0;
    out->did_section_change = false;
    out->new_calculation = false;
    out->was_calculation_interrupted = false;
    out->calculation_duration = 0.0;

    return out;
}

void cruckig_output_destroy(CRuckigOutputParameter *out) {
    if (!out) return;
    cruckig_trajectory_destroy(out->trajectory);
    cruckig_free(out->new_position);
    cruckig_free(out->new_velocity);
    cruckig_free(out->new_acceleration);
    cruckig_free(out->new_jerk);
    cruckig_free(out);
}

void cruckig_output_pass_to_input(const CRuckigOutputParameter *out, CRuckigInputParameter *inp) {
    if (!out || !inp) return;

    const size_t dofs = out->degrees_of_freedom;
    const size_t dsz = dofs * sizeof(double);

    memcpy(inp->current_position, out->new_position, dsz);
    memcpy(inp->current_velocity, out->new_velocity, dsz);
    memcpy(inp->current_acceleration, out->new_acceleration, dsz);

    /* If section changed and we have intermediate waypoints, remove the first waypoint */
    if (out->did_section_change && inp->num_intermediate_waypoints > 0) {
        size_t remaining = inp->num_intermediate_waypoints - 1;
        if (remaining == 0) {
            cruckig_free(inp->intermediate_positions);
            inp->intermediate_positions = NULL;
            inp->num_intermediate_waypoints = 0;
        } else {
            /* Shift waypoints forward by one */
            memmove(inp->intermediate_positions,
                    inp->intermediate_positions + dofs,
                    remaining * dofs * sizeof(double));
            inp->num_intermediate_waypoints = remaining;
        }

        /* Also shift per-section constraints if present */
        size_t old_nsec = remaining + 2; /* was num_waypoints+1 sections */
        size_t new_nsec = remaining + 1;

#define SHIFT_PER_SEC(field) \
        if (inp->field) { \
            memmove(inp->field, inp->field + dofs, new_nsec * dofs * sizeof(double)); \
        }

        SHIFT_PER_SEC(per_section_max_velocity)
        SHIFT_PER_SEC(per_section_max_acceleration)
        SHIFT_PER_SEC(per_section_max_jerk)
        SHIFT_PER_SEC(per_section_min_velocity)
        SHIFT_PER_SEC(per_section_min_acceleration)
        SHIFT_PER_SEC(per_section_max_position)
        SHIFT_PER_SEC(per_section_min_position)
#undef SHIFT_PER_SEC

        if (inp->per_section_minimum_duration) {
            memmove(inp->per_section_minimum_duration,
                    inp->per_section_minimum_duration + 1,
                    new_nsec * sizeof(double));
        }
        (void)old_nsec;
    }
}
