/********************************************************************
 * Description: comp_kins_uspace.h
 *   Glue header for .comp-based kinematics to support planner type 2
 *
 *   This header provides macros and helpers so that kinematics modules
 *   written as .comp files (compiled by halcompile) can work with the
 *   userspace trajectory planner (PLANNER_TYPE = 2) with minimal changes.
 *
 *   The approach: HAL pin values are mirrored into params.raw[] in the
 *   kinematics_params_t shared memory block.  RT pushes values each servo
 *   cycle; userspace reads a consistent snapshot via KINS_SHMEM_READ.
 *
 * Usage in a .comp file (after the ;; line):
 *
 *   #include "comp_kins_uspace.h"
 *
 *   // In your setup function, after hal_ready():
 *   comp_kins_uspace_setup(comp_id, "mykins", num_joints, "XYZAB");
 *
 *   // In kinematicsForward / kinematicsInverse:
 *   COMP_KINS_BEGIN(haldata);
 *   double x_rot = KINS_READ(haldata->x_rot_point, 0);
 *   double y_rot = KINS_READ(haldata->y_rot_point, 1);
 *   // ... math using x_rot, y_rot ...
 *   COMP_KINS_END();
 *
 *   // At the end of the file:
 *   COMP_KINS_NONRT_ATTACH("mykins")
 *
 * Parameter indices (the second arg to KINS_READ) are assigned by the
 * .comp author.  Up to 127 double-precision parameters are supported
 * via params.raw[].  Index 0 is reserved for switchkins_type if the
 * module uses switchable kinematics.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef COMP_KINS_USPACE_H
#define COMP_KINS_USPACE_H

#include "kinematics_params.h"
#include "hal.h"

/* ====================================================================
 * Static state — one instance per .so (separate for RT and userspace)
 * ==================================================================== */

static kinematics_params_t *_comp_uspace_params = NULL;
static kinematics_params_t  _comp_uspace_local;
static int                  _comp_uspace_loaded = 0;

/* ====================================================================
 * Setup — call once in your setup function after HAL pins are created
 *
 * comp_id:      from rtapi_app_main / halcompile
 * name:         module name, e.g. "xyzab_tdr_kins"
 * num_joints:   number of joints
 * coordinates:  coordinate letters, e.g. "XYZAB"
 *
 * Returns 0 on success, <0 on failure.
 * ==================================================================== */

static inline int comp_kins_uspace_setup(int comp_id, const char *name,
                                          int num_joints,
                                          const char *coordinates)
{
    char pname[HAL_NAME_LEN + 1];
    int result, i, j;

    result = hal_struct_newf(comp_id, sizeof(kinematics_params_t), NULL,
                             "%s.params", name);
    if (result < 0) return result;

    rtapi_snprintf(pname, sizeof(pname), "%s.params", name);
    result = hal_struct_attach(pname, (void **)&_comp_uspace_params);
    if (result < 0) return result;

    memset(_comp_uspace_params, 0, sizeof(kinematics_params_t));
    strncpy(_comp_uspace_params->module_name, name,
            sizeof(_comp_uspace_params->module_name) - 1);

    _comp_uspace_params->num_joints = num_joints;

    /* Initialise joint↔axis maps to "unmapped" */
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++)
        _comp_uspace_params->joint_to_axis[i] = -1;
    for (i = 0; i < 9; i++)
        _comp_uspace_params->axis_to_joint[i] = -1;

    /* Parse coordinates string → joint/axis maps */
    if (coordinates) {
        strncpy(_comp_uspace_params->coordinates, coordinates,
                sizeof(_comp_uspace_params->coordinates) - 1);
        for (j = 0; j < num_joints && coordinates[j]; j++) {
            int ax = -1;
            switch (coordinates[j]) {
                case 'X': case 'x': ax = 0; break;
                case 'Y': case 'y': ax = 1; break;
                case 'Z': case 'z': ax = 2; break;
                case 'A': case 'a': ax = 3; break;
                case 'B': case 'b': ax = 4; break;
                case 'C': case 'c': ax = 5; break;
                case 'U': case 'u': ax = 6; break;
                case 'V': case 'v': ax = 7; break;
                case 'W': case 'w': ax = 8; break;
            }
            if (ax >= 0) {
                _comp_uspace_params->axis_to_joint[ax] = j;
                _comp_uspace_params->joint_to_axis[j]  = ax;
            }
        }
    }

    _comp_uspace_params->is_identity = 0;
    _comp_uspace_params->valid       = 1;

    return 0;
}

/* ====================================================================
 * Begin/End — bracket each kinematicsForward / kinematicsInverse call
 *
 * COMP_KINS_BEGIN(haldata):
 *   RT  → increments shmem head counter (start of atomic write batch)
 *   USP → takes a consistent shmem snapshot
 *
 * COMP_KINS_END():
 *   RT  → sets tail = head (close the atomic write batch)
 *   USP → no-op
 * ==================================================================== */

#define COMP_KINS_BEGIN(hd) do {                                         \
    if (!(hd) && _comp_uspace_params) {                                  \
        KINS_SHMEM_READ(_comp_uspace_params, _comp_uspace_local);       \
        _comp_uspace_loaded = 1;                                         \
    } else {                                                             \
        _comp_uspace_loaded = 0;                                         \
        if (_comp_uspace_params) _comp_uspace_params->head++;            \
    }                                                                    \
} while (0)

#define COMP_KINS_END() do {                                             \
    if (!_comp_uspace_loaded && _comp_uspace_params) {                   \
        _comp_uspace_params->tail = _comp_uspace_params->head;          \
    }                                                                    \
} while (0)

/* ====================================================================
 * KINS_READ — dual-path parameter read
 *
 * In RT:        reads HAL pin, pushes value to params.raw[idx]
 * In userspace: reads from shmem snapshot (HAL pin is NOT touched)
 *
 * The ternary short-circuits: when _comp_uspace_loaded is true the
 * halpin expression is never evaluated, so haldata being NULL is safe.
 * ==================================================================== */

static inline double _comp_kins_push(hal_float_t *pin, int idx)
{
    double v = *pin;
    if (_comp_uspace_params) _comp_uspace_params->params.raw[idx] = v;
    return v;
}

#define KINS_READ(halpin, idx)                                           \
    (_comp_uspace_loaded ? _comp_uspace_local.params.raw[idx]            \
                         : _comp_kins_push((halpin), (idx)))

/* ====================================================================
 * KINS_READ_S32 / KINS_READ_BIT — for non-float HAL pins
 *
 * Stores integer values as doubles in params.raw[].
 * ==================================================================== */

static inline double _comp_kins_push_s32(hal_s32_t *pin, int idx)
{
    double v = (double)*pin;
    if (_comp_uspace_params) _comp_uspace_params->params.raw[idx] = v;
    return v;
}

static inline double _comp_kins_push_bit(hal_bit_t *pin, int idx)
{
    double v = (double)*pin;
    if (_comp_uspace_params) _comp_uspace_params->params.raw[idx] = v;
    return v;
}

#define KINS_READ_S32(halpin, idx)                                       \
    ((int)(_comp_uspace_loaded ? _comp_uspace_local.params.raw[idx]      \
                               : _comp_kins_push_s32((halpin), (idx))))

#define KINS_READ_BIT(halpin, idx)                                       \
    ((int)(_comp_uspace_loaded ? _comp_uspace_local.params.raw[idx]      \
                               : _comp_kins_push_bit((halpin), (idx))))

/* ====================================================================
 * Switchkins support
 *
 * COMP_KINS_SET_SWITCH_TYPE(type) — call from kinematicsSwitch()
 *   Stores the switch type in params.raw[0] so userspace can read it.
 *
 * COMP_KINS_GET_SWITCH_TYPE() — read switch type (works in both paths)
 *   Returns int.  In userspace, reads from the shmem snapshot.
 *
 * Convention: params.raw[0] is reserved for switchkins_type.
 *             User parameters start at index 1 for switchable modules.
 * ==================================================================== */

#define COMP_KINS_SET_SWITCH_TYPE(type) do {                             \
    if (_comp_uspace_params) {                                           \
        _comp_uspace_params->head++;                                     \
        _comp_uspace_params->params.raw[0] = (double)(type);            \
        _comp_uspace_params->tail = _comp_uspace_params->head;          \
    }                                                                    \
} while (0)

#define COMP_KINS_GET_SWITCH_TYPE()                                      \
    ((int)(_comp_uspace_loaded ? _comp_uspace_local.params.raw[0] : 0))

/* ====================================================================
 * nonrt_attach — generates the entry point for kinematics_user.c
 *
 * COMP_KINS_NONRT_ATTACH("module_name")
 *
 * Generates:
 *   void nonrt_attach(nonrt_ops_t *ops) { ... }
 *   EXPORT_SYMBOL(nonrt_attach);
 *
 * The forward/inverse functions registered here are the same ones
 * used by the RT module — the !haldata guard inside them switches
 * to the shmem read path automatically.
 * ==================================================================== */

#define COMP_KINS_NONRT_ATTACH(module_name)                              \
void nonrt_attach(nonrt_ops_t *ops)                                      \
{                                                                        \
    hal_struct_attach(module_name ".params",                              \
                      (void **)&_comp_uspace_params);                    \
    ops->forward = kinematicsForward;                                    \
    ops->inverse = kinematicsInverse;                                    \
}                                                                        \
EXPORT_SYMBOL(nonrt_attach);

#endif /* COMP_KINS_USPACE_H */
