/********************************************************************
 * Description: kinematics_user.c
 *   Non-RT loader for kinematics modules
 *
 * Loads a kinematics .so with dlopen, asks it to describe itself through
 * kinsDescribe(), and evaluates its kinematics through the parameter
 * block (see kinematics.h).  The block is filled from HAL: one input pin
 * of the caller's component per table entry, connected to the signal the
 * RT instance's pin reads, so the values are the live ones.  The tool is
 * the caller's where it has given one, since a planner knows what a
 * segment runs under better than the machine does; otherwise it comes
 * from motion's own tooloffset pins where motion is loaded, so that the
 * tool the module sees is the one motion has, whether or not the config
 * netted it to the module's pin.
 *
 * A module exporting no kinsDescribe() is not an error; the context comes
 * back flagged rt_only and answers nothing.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "kinematics_user.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"  /* EMC2_HOME */

typedef int (*kins_describe_fn)(const char *coordinates, const char *sparm,
                                kins_module_info *info);

#define MAX_BOUND_PINS   (KINS_MAX_PARAMS + AXIS_COUNT)
#define MAX_MADE_SIGNALS MAX_BOUND_PINS

struct KinematicsUserContext {
    int initialized;
    int rt_only;               /* 1 if the module exports no kinsDescribe() */
    KINEMATICS_TYPE kins_type;
    void *rt_handle;           /* dlopen handle */
    kins_module_info info;
    kins_params params;
    kins_scratch scratch;
    int ktype;                 /* kinematics type being evaluated */
    int num_joints;
    char module_name[64];
    int comp_id;               /* the caller's component, owns the pins made here */
    const char *prefix;        /* its name, which those pin names start with */
    char made_signal[MAX_MADE_SIGNALS][HAL_NAME_LEN + 1];
    int num_made_signals;
    hal_refs_u *cell;          /* HAL storage those pins are made against */
    int num_cells;
    int cell_of_param[KINS_MAX_PARAMS];  /* -1 if not bound */
    int cell_of_tool[AXIS_COUNT];        /* motion.tooloffset.*, -1 if absent */
    int tool_param;                      /* the table's tool entry, -1 if none */
    int warned_tool;
    EmcPose caller_tool;                 /* from kinematicsUserSetTool() */
    int have_caller_tool;
    double last_joints[EMCMOT_MAX_JOINTS]; /* what the last inverse found */
};

/* ========================================================================
 * Pin binding
 * ======================================================================== */

/*
 * Give the block a reference to a value it needs.
 *
 * The reference is to a pin of ours rather than into the RT instance's,
 * so that its lifetime is ours.  Ours is connected to the signal the RT
 * pin reads, or, when the RT pin has no signal, to one made here and
 * removed again in kinematicsUserFree().
 *
 * The reference has to live in HAL shared memory, since that is where
 * HAL rewrites it on connect and disconnect, so the pins are made
 * against hal_malloc() cells and the block reads what a cell holds once
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
        default: break;
    }
    return -1;
}

/* Does a pin of this name exist?  Silent: absence is an answer, not an error. */
static int pin_exists(const char *pin_name)
{
    hal_query_t q;
    memset(&q, 0, sizeof(q));
    q.name  = pin_name;
    q.qtype = HAL_QTYPE_PIN;
    return hal_getref_p(&q) == 0;
}

/* Bind pin_name; returns the cell index, or -1. */
static int bind_pin(KinematicsUserContext *ctx, const char *pin_name,
                    hal_type_t type)
{
    char signal[HAL_NAME_LEN + 1];
    char mine[HAL_NAME_LEN + 1];
    hal_refs_u *cell;
    hal_query_t q;
    int idx;

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
    idx  = ctx->num_cells;
    cell = &ctx->cell[idx];

    if (new_pin(ctx->comp_id, type, cell, mine) != 0) {
        fprintf(stderr, "kinematicsUserInit: cannot create pin '%s'\n", mine);
        return -1;
    }
    if (hal_link(mine, signal) != 0) {
        fprintf(stderr, "kinematicsUserInit: cannot link '%s' to '%s'\n",
                mine, signal);
        return -1;
    }
    ctx->num_cells++;
    return idx;
}

static hal_type_t hal_type_of(kins_param_type t)
{
    switch (t) {
        case KINS_PARAM_BIT: return HAL_BIT;
        case KINS_PARAM_S32: return HAL_S32;
        case KINS_PARAM_U32: return HAL_U32;
        default:             return HAL_FLOAT;
    }
}

static double cell_value(const hal_refs_u *cell, kins_param_type t)
{
    switch (t) {
        case KINS_PARAM_BIT: return hal_get_bool(cell->b) ? 1.0 : 0.0;
        case KINS_PARAM_S32: return hal_get_si32(cell->s);
        case KINS_PARAM_U32: return hal_get_ui32(cell->u);
        default:             return hal_get_real(cell->r);
    }
}

/* Bind every input of the table, and motion's tool where motion is there. */
static int bind_all(KinematicsUserContext *ctx)
{
    static const char letter[AXIS_COUNT] = { 'x','y','z','a','b','c','u','v','w' };
    char name[HAL_NAME_LEN + 1];
    int i;

    for (i = 0; i < KINS_MAX_PARAMS; i++) ctx->cell_of_param[i] = -1;
    for (i = 0; i < AXIS_COUNT; i++) ctx->cell_of_tool[i] = -1;
    ctx->tool_param = -1;

    for (i = 0; i < ctx->info.nparams; i++) {
        const kins_param_desc *d = &ctx->info.params[i];
        if (d->dir == KINS_OUT) continue;
        if (d->tool) ctx->tool_param = i;
        snprintf(name, sizeof(name), "%s.%s", ctx->info.halprefix, d->name);
        ctx->cell_of_param[i] = bind_pin(ctx, name, hal_type_of(d->type));
        if (ctx->cell_of_param[i] < 0) return -1;
    }

    /* motion publishes the tool it applies; take it from there when it is
       loaded, so the module sees the tool whether or not the config netted
       it through.  Under halrun with the module alone there is no motion,
       and the module's own tool entry is all there is. */
    for (i = 0; i < AXIS_COUNT; i++) {
        snprintf(name, sizeof(name), "motion.tooloffset.%c", letter[i]);
        if (!pin_exists(name)) continue;
        ctx->cell_of_tool[i] = bind_pin(ctx, name, HAL_FLOAT);
        if (ctx->cell_of_tool[i] < 0) return -1;
    }
    return 0;
}

/* The block sees the pins as they are now, and the tool of whoever
   knows it best: the caller, then motion, then the module's own pin. */
static void refresh(KinematicsUserContext *ctx)
{
    int i;
    double tool[AXIS_COUNT];
    int have_motion_tool = 0;

    for (i = 0; i < ctx->info.nparams; i++) {
        int c = ctx->cell_of_param[i];
        if (c < 0) continue;
        ctx->params.geometry[i] = cell_value(&ctx->cell[c], ctx->info.params[i].type);
    }
    if (ctx->tool_param >= 0) {
        ctx->params.tool.tran.z = ctx->params.geometry[ctx->tool_param];
    }

    if (ctx->have_caller_tool) {
        ctx->params.tool = ctx->caller_tool;
        if (ctx->tool_param >= 0) {
            ctx->params.geometry[ctx->tool_param] = ctx->caller_tool.tran.z;
        }
        return;
    }

    for (i = 0; i < AXIS_COUNT; i++) {
        int c = ctx->cell_of_tool[i];
        tool[i] = 0.0;
        if (c < 0) continue;
        tool[i] = hal_get_real(ctx->cell[c].r);
        have_motion_tool = 1;
    }
    if (!have_motion_tool) return;

    /* the module's pin and motion disagree: the config lost the tool
       somewhere between them.  Say so once; motion's value is the one
       being cut with. */
    if (ctx->tool_param >= 0 && !ctx->warned_tool
        && fabs(tool[AXIS_Z] - ctx->params.geometry[ctx->tool_param]) > 1e-9) {
        fprintf(stderr,
                "kinematics_user: %s.%s is %.6g but motion.tooloffset.z is %.6g;"
                " using motion's value\n",
                ctx->info.halprefix, ctx->info.params[ctx->tool_param].name,
                ctx->params.geometry[ctx->tool_param], tool[AXIS_Z]);
        ctx->warned_tool = 1;
    }
    for (i = 0; i < AXIS_COUNT; i++) emcPoseSetAxis(&ctx->params.tool, i, tool[i]);
    if (ctx->tool_param >= 0) {
        ctx->params.geometry[ctx->tool_param] = tool[AXIS_Z];
    }
}

/* ========================================================================
 * Module loading
 * ======================================================================== */

static int load_module(KinematicsUserContext *ctx,
                       const char *module_name,
                       const char *coordinates,
                       const char *sparm)
{
    char module_path[512];
    void *handle;
    kins_describe_fn describe;

    snprintf(module_path, sizeof(module_path),
             "%s/rtlib/%s.so", EMC2_HOME, module_name);

    /* lazily: a halcompile component references hal_export_funct() and
       the rest of what its rtapi_app_main() needs, which only the realtime
       HAL library provides, and nothing here calls that main.  What is
       called, kinsDescribe() and the ops, resolves when it is called. */
    handle = dlopen(module_path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "kinematicsUserInit: dlopen '%s': %s\n",
                module_path, dlerror());
        return -1;
    }
    ctx->rt_handle = handle;

    describe = (kins_describe_fn)dlsym(handle, "kinsDescribe");
    if (!describe) {
        fprintf(stderr, "kinematicsUserInit: '%s' exports no kinsDescribe;"
                " it cannot be evaluated outside RT\n", module_name);
        dlclose(handle);
        ctx->rt_handle = NULL;
        ctx->rt_only = 1;
        return -1;
    }

    if (describe(coordinates, sparm, &ctx->info) != 0) {
        fprintf(stderr, "kinematicsUserInit: kinsDescribe failed for '%s'\n",
                module_name);
        dlclose(handle);
        ctx->rt_handle = NULL;
        ctx->rt_only = 1;
        return -1;
    }

    if (ctx->info.ntypes < 1 || !ctx->info.ops[0]) {
        fprintf(stderr, "kinematicsUserInit: '%s' has no type 0 in the"
                " parameter block form\n", module_name);
        dlclose(handle);
        ctx->rt_handle = NULL;
        ctx->rt_only = 1;
        return -1;
    }

    if (kinsParamsInit(&ctx->params, &ctx->info, coordinates) != 0) {
        fprintf(stderr, "kinematicsUserInit: '%s' refuses coordinates '%s'\n",
                module_name, coordinates ? coordinates : "(default)");
        dlclose(handle);
        ctx->rt_handle = NULL;
        ctx->rt_only = 1;
        return -1;
    }
    kinsScratchInit(&ctx->scratch);

    ctx->ktype = 0;
    ctx->kins_type = ctx->info.ops[0]->identity ? KINEMATICS_IDENTITY
                                                : KINEMATICS_BOTH;
    return 0;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

KinematicsUserContext* kinematicsUserInitSparm(const char* kins_type,
                                               int num_joints,
                                               const char* coordinates,
                                               const char* sparm,
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

    if (load_module(ctx, kins_type, coordinates, sparm) == 0) {
        if (bind_all(ctx) != 0) {
            fprintf(stderr, "kinematicsUserInit: cannot bind the pins of '%s'\n",
                    kins_type);
            ctx->rt_only = 1;
        }
    }

    ctx->initialized = 1;
    return ctx;
}

KinematicsUserContext* kinematicsUserInit(const char* kins_type,
                                          int num_joints,
                                          const char* coordinates,
                                          int comp_id,
                                          const char* prefix)
{
    return kinematicsUserInitSparm(kins_type, num_joints, coordinates, NULL,
                                   comp_id, prefix);
}

int kinematicsUserSetType(KinematicsUserContext* ctx, int ktype)
{
    if (!ctx || !ctx->initialized || ctx->rt_only) return -1;
    if (ktype < 0 || ktype >= ctx->info.ntypes || !ctx->info.ops[ktype]) {
        return -1;
    }
    ctx->ktype = ktype;
    ctx->params.ktype = ktype;
    kinsScratchInit(&ctx->scratch);
    ctx->kins_type = ctx->info.ops[ktype]->identity ? KINEMATICS_IDENTITY
                                                    : KINEMATICS_BOTH;
    return 0;
}

int kinematicsUserGetNumTypes(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized || ctx->rt_only) return 0;
    return ctx->info.ntypes;
}

int kinematicsUserSetTool(KinematicsUserContext* ctx, const EmcPose* tool)
{
    if (!ctx || !ctx->initialized || ctx->rt_only) return -1;
    if (tool) {
        ctx->caller_tool = *tool;
        ctx->have_caller_tool = 1;
    } else {
        ctx->have_caller_tool = 0;
    }
    return 0;
}

int kinematicsUserInverse(KinematicsUserContext* ctx,
                          const EmcPose* world,
                          double* joints)
{
    KINEMATICS_INVERSE_FLAGS iflags = 0;
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    double j[EMCMOT_MAX_JOINTS];
    int i;

    if (!ctx || !ctx->initialized || !world || !joints) return -1;
    if (ctx->rt_only) return -1;

    refresh(ctx);
    /* the joints go in as well as out: motion hands a module where the
       machine is, and some read that (a nutating head takes its rotary
       angles from it), so the caller's array is the seed */
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        j[i] = (i < ctx->num_joints) ? joints[i] : 0.0;
    }
    if (kinsOpsInverse(ctx->info.ops[ctx->ktype], &ctx->params, &ctx->scratch,
                       world, j, &iflags, &fflags) != 0) {
        return -1;
    }
    for (i = 0; i < ctx->num_joints; i++) joints[i] = j[i];
    memcpy(ctx->last_joints, j, sizeof(ctx->last_joints));
    return 0;
}

int kinematicsUserForward(KinematicsUserContext* ctx,
                          const double* joints,
                          EmcPose* world)
{
    KINEMATICS_INVERSE_FLAGS iflags = 0;
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    double j[EMCMOT_MAX_JOINTS];
    int i;

    if (!ctx || !ctx->initialized || !joints || !world) return -1;
    if (ctx->rt_only) return -1;

    refresh(ctx);
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        j[i] = (i < ctx->num_joints) ? joints[i] : 0.0;
    }
    /* a forward that iterates starts from the pose it is handed, so the
       caller's world is the seed; any other gets a clean one */
    if (!ctx->info.ops[ctx->ktype]->fwd_iterates) {
        memset(world, 0, sizeof(*world));
    }
    return kinsOpsForward(ctx->info.ops[ctx->ktype], &ctx->params, &ctx->scratch,
                          j, world, &fflags, &iflags);
}

int kinematicsUserJacobian(KinematicsUserContext* ctx,
                           const EmcPose* world,
                           double J[KINEMATICS_USER_MAX_JOINTS][AXIS_COUNT])
{
    KINEMATICS_INVERSE_FLAGS iflags = 0;
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS];
    double j[EMCMOT_MAX_JOINTS];
    int r, a;

    if (!ctx || !ctx->initialized || !world || !J) return -1;
    if (ctx->rt_only) return -1;

    refresh(ctx);
    /* the joints at this pose, on the branch the last inverse was on */
    memcpy(j, ctx->last_joints, sizeof(j));
    if (kinsOpsInverse(ctx->info.ops[ctx->ktype], &ctx->params, &ctx->scratch,
                       world, j, &iflags, &fflags) != 0) {
        return -1;
    }
    if (kinsOpsJacobian(ctx->info.ops[ctx->ktype], &ctx->params, &ctx->scratch,
                        j, world, jac, &iflags) != 0) {
        return -1;
    }
    for (r = 0; r < KINEMATICS_USER_MAX_JOINTS; r++) {
        for (a = 0; a < AXIS_COUNT; a++) J[r][a] = jac[r][a];
    }
    return 0;
}

int kinematicsUserIsIdentity(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized || ctx->rt_only) return 0;
    return ctx->info.ops[ctx->ktype]->identity;
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
    if (!ctx || !ctx->initialized || ctx->rt_only) return -1;
    refresh(ctx);
    return 0;
}

const kins_params* kinematicsUserParams(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized || ctx->rt_only) return NULL;
    refresh(ctx);
    return &ctx->params;
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
