/********************************************************************
 * Description: kinematics_user.c
 *   Non-RT loader for kinematics modules
 *
 * Loads a kinematics .so with dlopen and calls the nonrt_attach() it
 * exports, so this process evaluates the kinematics the machine is
 * running, at whatever poses it likes.  See nonrt_kins.h.
 *
 * Identity kinematics needs no module code: the module says so through
 * nonrt_ops_t and this file maps joints to axes directly.  A module
 * exporting no nonrt_attach() is not an error either; the context comes
 * back flagged rt_only.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "kinematics_user.h"
#include <nonrt_kins.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"  /* EMC2_HOME */

typedef int (*nonrt_attach_fn)(const char *coordinates, nonrt_ops_t *ops,
                               nonrt_resolve_fn resolve, void *arg);

/* One per value a kinematics module reads is a generous bound. */
#define MAX_MADE_SIGNALS 16
#define MAX_BOUND_PINS   16

struct KinematicsUserContext {
    int initialized;
    int rt_only;               /* 1 if the module exports no nonrt_attach() */
    int is_identity;           /* 1 for identity kinematics: no module code needed */
    KINEMATICS_TYPE kins_type;
    void *rt_handle;           /* dlopen handle */
    nonrt_ops_t ops;
    int num_joints;
    int joint_to_axis[KINEMATICS_USER_MAX_JOINTS]; /* identity path only */
    char module_name[64];
    int comp_id;               /* the caller's component, owns the pins made here */
    const char *prefix;        /* its name, which those pin names start with */
    char made_signal[MAX_MADE_SIGNALS][HAL_NAME_LEN + 1];
    int num_made_signals;
    hal_refs_u *cell;          /* HAL storage those pins are made against */
    int num_cells;
};

/* ========================================================================
 * Pin binding
 * ======================================================================== */

/*
 * Give a kinematics module a reference to a value it asked for.
 *
 * The reference is to a pin of ours rather than into the RT instance's,
 * so that its lifetime is ours: see nonrt_kins.h.  Ours is connected to
 * the signal the RT pin reads, or, when the RT pin has no signal, to one
 * made here and removed again in kinematicsUserFree().
 *
 * The reference has to live in HAL shared memory, since that is where
 * HAL rewrites it on connect and disconnect, so the pins are made
 * against hal_malloc() cells and the module gets what a cell holds once
 * the connection is in place.
 */
static int make_signal(KinematicsUserContext *ctx, const char *pin_name,
                       hal_type_t type, char *out, size_t outlen)
{
    if (ctx->num_made_signals >= MAX_MADE_SIGNALS) {
        fprintf(stderr, "kinematicsUserInit: too many signals to create\n");
        return -1;
    }
    if ((size_t)snprintf(out, outlen, "%s-nonrt", pin_name) >= outlen) {
        fprintf(stderr, "kinematicsUserInit: signal name for '%s' too long\n",
                pin_name);
        return -1;
    }
    if (hal_signal_new(out, type) != 0) return -1;
    if (hal_link(pin_name, out) != 0) {
        hal_signal_delete(out);
        return -1;
    }
    snprintf(ctx->made_signal[ctx->num_made_signals++],
             sizeof(ctx->made_signal[0]), "%s", out);
    return 0;
}

static int new_pin(int comp_id, hal_type_t type, hal_refs_u *out,
                   const char *name)
{
    switch (type) {
        case HAL_BIT:   return hal_pin_new_bool(comp_id, HAL_IN, &out->b, 0, "%s", name);
        case HAL_FLOAT: return hal_pin_new_real(comp_id, HAL_IN, &out->r, 0.0, "%s", name);
        case HAL_S32:   return hal_pin_new_si32(comp_id, HAL_IN, &out->s, 0, "%s", name);
        case HAL_U32:   return hal_pin_new_ui32(comp_id, HAL_IN, &out->u, 0, "%s", name);
        case HAL_S64:   return hal_pin_new_sint(comp_id, HAL_IN, &out->s, 0, "%s", name);
        case HAL_U64:   return hal_pin_new_uint(comp_id, HAL_IN, &out->u, 0, "%s", name);
        default: break;
    }
    return -1;
}

static int bind_pin(const char *pin_name, hal_type_t type,
                    hal_refs_u *out, void *arg)
{
    KinematicsUserContext *ctx = (KinematicsUserContext *)arg;
    char signal[HAL_NAME_LEN + 1];
    char mine[HAL_NAME_LEN + 1];
    hal_refs_u *cell;
    hal_query_t q;

    if (!ctx || !pin_name || !out) return -1;

    memset(&q, 0, sizeof(q));
    q.name  = pin_name;
    q.qtype = HAL_QTYPE_PIN;

    if (hal_getref_p(&q) != 0) {
        fprintf(stderr, "kinematicsUserInit: no such pin '%s'\n", pin_name);
        return -1;
    }
    if (q.pp.type != type) {
        fprintf(stderr, "kinematicsUserInit: pin '%s' has the wrong type\n",
                pin_name);
        return -1;
    }

    if (q.pp.signal) {
        snprintf(signal, sizeof(signal), "%s", q.pp.signal);
    } else if (make_signal(ctx, pin_name, type, signal, sizeof(signal))) {
        fprintf(stderr, "kinematicsUserInit: cannot reach '%s'\n", pin_name);
        return -1;
    }

    if ((size_t)snprintf(mine, sizeof(mine), "%s.%s", ctx->prefix, pin_name)
            >= sizeof(mine)) {
        fprintf(stderr, "kinematicsUserInit: pin name for '%s' too long\n",
                pin_name);
        return -1;
    }
    if (ctx->num_cells >= MAX_BOUND_PINS) {
        fprintf(stderr, "kinematicsUserInit: too many pins to bind\n");
        return -1;
    }
    cell = &ctx->cell[ctx->num_cells++];

    if (new_pin(ctx->comp_id, type, cell, mine) != 0) {
        fprintf(stderr, "kinematicsUserInit: cannot create pin '%s'\n", mine);
        return -1;
    }
    if (hal_link(mine, signal) != 0) {
        fprintf(stderr, "kinematicsUserInit: cannot link '%s' to '%s'\n",
                mine, signal);
        return -1;
    }

    *out = *cell;
    return 0;
}

/* ========================================================================
 * Identity joint mapping
 * ======================================================================== */

static void fill_identity_joint_map(KinematicsUserContext *ctx, const char *coords)
{
    int i, j = 0;
    for (i = 0; i < KINEMATICS_USER_MAX_JOINTS; i++) ctx->joint_to_axis[i] = -1;
    if (!coords) return;
    for (; *coords && j < ctx->num_joints; coords++) {
        int axis;
        switch (tolower((unsigned char)*coords)) {
            case 'x': axis = 0; break; case 'y': axis = 1; break;
            case 'z': axis = 2; break; case 'a': axis = 3; break;
            case 'b': axis = 4; break; case 'c': axis = 5; break;
            case 'u': axis = 6; break; case 'v': axis = 7; break;
            case 'w': axis = 8; break; default:  continue;
        }
        ctx->joint_to_axis[j++] = axis;
    }
}

/* ========================================================================
 * Module loading
 * ======================================================================== */

static int load_module(KinematicsUserContext *ctx,
                       const char *module_name,
                       const char *coordinates)
{
    char module_path[512];
    void *handle;
    nonrt_attach_fn attach;

    snprintf(module_path, sizeof(module_path),
             "%s/rtlib/%s.so", EMC2_HOME, module_name);

    handle = dlopen(module_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "kinematicsUserInit: dlopen '%s': %s\n",
                module_path, dlerror());
        return -1;
    }
    ctx->rt_handle = handle;

    attach = (nonrt_attach_fn)dlsym(handle, "nonrt_attach");
    if (!attach) {
        fprintf(stderr, "kinematicsUserInit: '%s' exports no nonrt_attach\n",
                module_name);
        dlclose(handle);
        ctx->rt_handle = NULL;
        ctx->rt_only = 1;
        return -1;
    }

    if (attach(coordinates, &ctx->ops, bind_pin, ctx) != 0) {
        fprintf(stderr, "kinematicsUserInit: nonrt_attach failed for '%s'\n",
                module_name);
        dlclose(handle);
        ctx->rt_handle = NULL;
        ctx->rt_only = 1;
        return -1;
    }

    if (ctx->ops.is_identity) {
        ctx->is_identity = 1;
        ctx->kins_type = KINEMATICS_IDENTITY;
        return 0;
    }

    if (!ctx->ops.forward || !ctx->ops.inverse) {
        fprintf(stderr, "kinematicsUserInit: '%s' set no fwd/inv\n", module_name);
        dlclose(handle);
        ctx->rt_handle = NULL;
        ctx->rt_only = 1;
        return -1;
    }

    ctx->kins_type = KINEMATICS_BOTH;
    return 0;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

KinematicsUserContext* kinematicsUserInit(const char* kins_type,
                                          int num_joints,
                                          const char* coordinates,
                                          int comp_id,
                                          const char* prefix)
{
    KinematicsUserContext *ctx;

    if (!kins_type || num_joints < 1 || num_joints > KINEMATICS_USER_MAX_JOINTS
        || comp_id < 0 || !prefix) {
        fprintf(stderr, "kinematicsUserInit: invalid arguments\n");
        return NULL;
    }

    ctx = (KinematicsUserContext *)calloc(1, sizeof(KinematicsUserContext));
    if (!ctx) return NULL;

    ctx->num_joints = num_joints;
    ctx->comp_id    = comp_id;
    ctx->prefix     = prefix;

    ctx->cell = (hal_refs_u *)hal_malloc(MAX_BOUND_PINS * sizeof(hal_refs_u));
    if (!ctx->cell) {
        fprintf(stderr, "kinematicsUserInit: out of HAL memory\n");
        free(ctx);
        return NULL;
    }
    strncpy(ctx->module_name, kins_type, sizeof(ctx->module_name) - 1);

    load_module(ctx, kins_type, coordinates);

    if (ctx->is_identity) {
        fill_identity_joint_map(ctx, coordinates);
    }

    ctx->initialized = 1;
    return ctx;
}

int kinematicsUserInverse(KinematicsUserContext* ctx,
                          const EmcPose* world,
                          double* joints)
{
    if (!ctx || !ctx->initialized || !world || !joints) return -1;

    if (ctx->is_identity) {
        int i;
        for (i = 0; i < ctx->num_joints; i++) {
            int ax = ctx->joint_to_axis[i];
            joints[i] = (ax >= 0) ? emcPoseGetAxis(world, ax) : 0.0;
        }
        return 0;
    }

    if (ctx->rt_only) return -1;
    return ctx->ops.inverse(world, joints, NULL, NULL);
}

int kinematicsUserForward(KinematicsUserContext* ctx,
                          const double* joints,
                          EmcPose* world)
{
    if (!ctx || !ctx->initialized || !joints || !world) return -1;

    if (ctx->is_identity) {
        int i;
        memset(world, 0, sizeof(*world));
        for (i = 0; i < ctx->num_joints; i++) {
            int ax = ctx->joint_to_axis[i];
            if (ax >= 0) emcPoseSetAxis(world, ax, joints[i]);
        }
        return 0;
    }

    if (ctx->rt_only) return -1;
    return ctx->ops.forward(joints, world, NULL, NULL);
}

int kinematicsUserIsIdentity(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return 0;
    return ctx->is_identity;
}

int kinematicsUserGetNumJoints(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return 0;
    return ctx->num_joints;
}

KINEMATICS_TYPE kinematicsUserGetType(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return KINEMATICS_IDENTITY;
    return ctx->kins_type;
}

const char* kinematicsUserGetModuleName(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return "unknown";
    return ctx->module_name;
}

int kinematicsUserRefreshParams(KinematicsUserContext* ctx)
{
    (void)ctx;
    return 0; /* nothing to refresh: the bound pins are the live values */
}

int kinematicsUserIsRtOnly(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return 1;
    return ctx->rt_only;
}

void kinematicsUserFree(KinematicsUserContext* ctx)
{
    int i;

    if (!ctx) return;

    /* Removing one hands its value back to the RT pin, leaving the
       machine as it was found. */
    for (i = 0; i < ctx->num_made_signals; i++) {
        hal_signal_delete(ctx->made_signal[i]);
    }
    if (ctx->rt_handle) dlclose(ctx->rt_handle);
    free(ctx);
}
