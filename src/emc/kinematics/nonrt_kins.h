/********************************************************************
 * Description: nonrt_kins.h
 *   Interface a kinematics module exports so that a non-RT caller can
 *   evaluate it.
 *
 *   A trajectory planner needs forward and inverse kinematics at poses
 *   the machine has not reached yet, which means calling them outside
 *   the servo thread.  A module opts in by exporting nonrt_attach().
 *
 *   The caller dlopens the module and calls nonrt_attach() once with
 *   the coordinates string and a resolver callback.  The module names
 *   each of the pins it reads, keeps the references the resolver
 *   returns in its own haldata, and hands back its existing forward
 *   and inverse.  The kinematics code itself does not change.
 *
 *   A reference does not point into the RT instance's pin.  The
 *   resolver creates an input pin on the caller's own component and
 *   connects it to the signal the RT pin reads, so the reference
 *   belongs to the caller and rewiring cannot strand it.
 *
 *   Name lookup belongs to the caller, userspace code linked against
 *   liblinuxcnchal.  This file is compiled into an RT module, which
 *   has no business walking the HAL name space and would risk binding
 *   against rtlib's copy of the same symbols.
 *
 *   Resolve input pins only.  Output pins and scratch storage stay
 *   private to the non-RT copy, or the two copies write to each
 *   other's state.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef NONRT_KINS_H
#define NONRT_KINS_H

#include <stdarg.h>

#include <rtapi.h>
#include <hal.h>
#include <emcpos.h>
#include <kinematics.h>

/* Supplied by the caller.  Finds 'pin_name' in HAL, checks that it has
   type 'type', and writes to 'out' a reference carrying that pin's
   value.  The reference is to storage the caller owns, not to the named
   pin itself.  Returns 0 on success. */
typedef int (*nonrt_resolve_fn)(const char *pin_name,
                                hal_type_t type,
                                hal_refs_u *out,
                                void *arg);

/* Filled in by nonrt_attach().  A module that reports is_identity has
   joints equal to axes and the caller needs no module code at all, so
   forward and inverse may be left NULL. */
typedef struct {
    int (*forward)(const double *joints, EmcPose *pos,
                   const KINEMATICS_FORWARD_FLAGS *fflags,
                   KINEMATICS_INVERSE_FLAGS *iflags);
    int (*inverse)(const EmcPose *pos, double *joints,
                   const KINEMATICS_INVERSE_FLAGS *iflags,
                   KINEMATICS_FORWARD_FLAGS *fflags);
    int is_identity;
} nonrt_ops_t;

/* Exported by a participating module:
     int nonrt_attach(const char *coordinates, nonrt_ops_t *ops,
                      nonrt_resolve_fn resolve, void *arg);
   Returns 0 on success. */

/* Convenience for the common case: resolve one float pin, by printf
   style name, into a haldata field. */
static inline int nonrt_resolve_real(nonrt_resolve_fn resolve, void *arg,
                                     hal_real_t *dst, const char *fmt, ...)
{
    char name[HAL_NAME_LEN + 1];
    hal_refs_u ref;
    va_list ap;

    if (!resolve || !dst) return -1;

    va_start(ap, fmt);
    rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);

    if (resolve(name, HAL_FLOAT, &ref, arg) != 0) return -1;

    *dst = ref.r;
    return 0;
}

#endif /* NONRT_KINS_H */
