// switchkins.h — common infrastructure for switchable kins cmods
//
// Provides coordinate-to-joint mapping, identity kins, and switch dispatch.
// Used by all switchkins-based cmod kinematics modules.
//
// License: GPL Version 2

#ifndef SWITCHKINS_CMOD_H
#define SWITCHKINS_CMOD_H

#include <string.h>
#include <ctype.h>
#include "kins_api.h"

// ─── Coordinate mapping ───

#define SK_MAX_JOINTS KINS_MAX_JOINTS
#define SK_NUM_AXES   9  // XYZABCUVW

typedef struct {
    int  num_joints;
    int  axis_for_joint[SK_MAX_JOINTS]; // joint j → axis index (0-8, or -1)
    int  principal[SK_NUM_AXES];        // axis index → first joint (-1 if unused)
} sk_map_t;

// Parse coordinates string → mapping.
// Returns 0 on success, -1 on error.
static inline int sk_map_coordinates(sk_map_t *m, const char *coordinates,
                                     int allow_duplicates)
{
    const char *axes = "XYZABCUVW";
    int dups[SK_NUM_AXES] = {0};

    for (int i = 0; i < SK_NUM_AXES; i++) m->principal[i] = -1;
    for (int i = 0; i < SK_MAX_JOINTS; i++) m->axis_for_joint[i] = -1;

    if (!coordinates || !*coordinates) coordinates = "XYZABCUVW";
    int jno = 0;
    for (const char *p = coordinates; *p; p++) {
        if (*p == ' ' || *p == '\t') continue;
        char uc = (char)toupper((unsigned char)*p);
        const char *f = strchr(axes, uc);
        if (!f) return -1; // bad letter
        int ai = (int)(f - axes);
        if (!allow_duplicates && dups[ai]) return -1;
        dups[ai]++;
        m->axis_for_joint[jno] = ai;
        if (m->principal[ai] < 0) m->principal[ai] = jno;
        jno++;
        if (jno >= SK_MAX_JOINTS) break;
    }
    m->num_joints = jno;
    return 0;
}

// Identity forward: joints → world using coordinate mapping.
static inline void sk_identity_forward(const sk_map_t *m,
                                       const double joints[SK_MAX_JOINTS],
                                       kins_pose_t *world)
{
    double *wp = (double *)world;
    for (int a = 0; a < SK_NUM_AXES; a++)
        wp[a] = (m->principal[a] >= 0) ? joints[m->principal[a]] : 0.0;
}

// Identity inverse: world → joints using coordinate mapping.
// Handles duplicate axes (all joints mapped to same axis get same value).
static inline void sk_identity_inverse(const sk_map_t *m,
                                       const kins_pose_t *world,
                                       double joints[SK_MAX_JOINTS])
{
    const double *wp = (const double *)world;
    for (int j = 0; j < m->num_joints; j++) {
        int ai = m->axis_for_joint[j];
        joints[j] = (ai >= 0 && ai < SK_NUM_AXES) ? wp[ai] : 0.0;
    }
}

// Convenience: populate joints from world (for use in module-specific inverse
// that computes a subset of joints and needs the rest identity-mapped).
static inline void sk_pos_to_joints(const sk_map_t *m,
                                    const kins_pose_t *world,
                                    double joints[SK_MAX_JOINTS])
{
    sk_identity_inverse(m, world, joints);
}

// ─── Switch state ───

#define SK_MAX_TYPES 3

typedef struct {
    int current_type;
    gomc_hal_bit_t *kinstype_is[SK_MAX_TYPES];
} sk_switch_t;

static inline int sk_switch_to(sk_switch_t *sw, int new_type)
{
    if (new_type < 0 || new_type >= SK_MAX_TYPES) return -1;
    sw->current_type = new_type;
    for (int i = 0; i < SK_MAX_TYPES; i++) {
        if (sw->kinstype_is[i])
            *(sw->kinstype_is[i]) = (i == new_type) ? 1 : 0;
    }
    return 0;
}

// Create the kinstype.is-N HAL pins.
static inline int sk_create_switch_pins(const gomc_hal_t *hal,
                                        int comp_id,
                                        sk_switch_t *sw)
{
    int rc = 0;
    rc = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &sw->kinstype_is[0],
                                comp_id, "kinstype.is-0");
    if (rc < 0) return rc;
    rc = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &sw->kinstype_is[1],
                                comp_id, "kinstype.is-1");
    if (rc < 0) return rc;
    rc = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &sw->kinstype_is[2],
                                comp_id, "kinstype.is-2");
    if (rc < 0) return rc;
    sw->current_type = 0;
    sk_switch_to(sw, 0);
    return 0;
}

#endif // SWITCHKINS_CMOD_H
