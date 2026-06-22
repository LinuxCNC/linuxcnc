/********************************************************************
* Description: stdglue.c
*   Standard remap prolog/epilog handlers for LinuxCNC.
*   This cmod registers the same handlers that stdglue.py provided,
*   ported from Python to C.
*
*   Covers: T (prepare), M6 (change), M61 (settool),
*           S (setspeed), F (setfeed), and cycle prolog/epilog.
*
* License: GPL Version 2
* Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — C port of stdglue.py
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gomc/pkg/cmodule/gomc_env.h"

#include "interp_ext_api.h"

/* TOLERANCE_EQUAL from the interpreter (used for float comparisons) */
#define TOLERANCE_EQUAL 1e-6

/* Feed modes matching the interpreter enum */
#define INVERSE_TIME 1

/* ================================================================
 * T — prepare_prolog / prepare_epilog
 * REMAP=T prolog=prepare_prolog ngc=prepare epilog=prepare_epilog
 * ================================================================ */

static int prepare_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->block_t_flag(ctx->ctx)) {
        ctx->set_error(ctx->ctx, "T requires a tool number");
        return INTERP_EXT_ERROR;
    }
    int tool = ctx->block_t_number(ctx->ctx);
    int pocket;
    if (tool) {
        pocket = ctx->find_tool_pocket(ctx->ctx, tool);
        if (pocket < 0) {
            char msg[80];
            snprintf(msg, sizeof(msg), "T%d: pocket not found", tool);
            ctx->set_error(ctx->ctx, msg);
            return INTERP_EXT_ERROR;
        }
    } else {
        pocket = -1; /* T0 = tool unload */
    }
    ctx->set_param(ctx->ctx, "tool", (double)tool);
    ctx->set_param(ctx->ctx, "pocket", (double)pocket);
    return INTERP_EXT_OK;
}

static int prepare_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->get_value_returned(ctx->ctx)) {
        ctx->set_error(ctx->ctx,
            "T: remap procedure did not return a value");
        return INTERP_EXT_ERROR;
    }
    if (ctx->block_builtin_used(ctx->ctx))
        return INTERP_EXT_OK;

    double rv = ctx->get_return_value(ctx->ctx);
    if (rv > 0.0) {
        int tool = (int)ctx->get_param(ctx->ctx, "tool");
        int pocket = (int)ctx->get_param(ctx->ctx, "pocket");
        ctx->set_selected_tool(ctx->ctx, tool);
        ctx->set_selected_pocket(ctx->ctx, pocket);
        ctx->canon_select_tool(ctx->ctx, tool);
        return INTERP_EXT_OK;
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "T%d: aborted (return code %.1f)",
                 (int)ctx->get_param(ctx->ctx, "tool"), rv);
        ctx->set_error(ctx->ctx, msg);
        return INTERP_EXT_ERROR;
    }
}

/* ================================================================
 * M6 — change_prolog / change_epilog
 * REMAP=M6 modalgroup=6 prolog=change_prolog ngc=change epilog=change_epilog
 * ================================================================ */

static int change_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    /* iocontrol-v2 fault check */
    double p5600 = ctx->get_param(ctx->ctx, "5600");
    if (p5600 > 0.0) {
        double p5601 = ctx->get_param(ctx->ctx, "5601");
        if (p5601 < 0.0) {
            char msg[80];
            snprintf(msg, sizeof(msg),
                     "Toolchanger hard fault %d", (int)p5601);
            ctx->set_error(ctx->ctx, msg);
            return INTERP_EXT_ERROR;
        }
    }

    if (ctx->get_selected_pocket(ctx->ctx) < 0) {
        ctx->set_error(ctx->ctx, "M6: no tool prepared");
        return INTERP_EXT_ERROR;
    }
    if (ctx->get_cutter_comp_side(ctx->ctx)) {
        ctx->set_error(ctx->ctx,
            "Cannot change tools with cutter radius compensation on");
        return INTERP_EXT_ERROR;
    }
    ctx->set_param(ctx->ctx, "tool_in_spindle",
                   (double)ctx->get_current_tool(ctx->ctx));
    ctx->set_param(ctx->ctx, "selected_tool",
                   (double)ctx->get_selected_tool(ctx->ctx));
    ctx->set_param(ctx->ctx, "current_pocket",
                   (double)ctx->get_current_pocket(ctx->ctx));
    ctx->set_param(ctx->ctx, "selected_pocket",
                   (double)ctx->get_selected_pocket(ctx->ctx));
    return INTERP_EXT_OK;
}

static int change_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    /* Phase 0: first call after NGC body returns */
    if (ctx->get_phase(ctx->ctx) == 0) {
        if (!ctx->get_value_returned(ctx->ctx)) {
            ctx->set_error(ctx->ctx,
                "M6: remap procedure did not return a value");
            return INTERP_EXT_ERROR;
        }
        /* iocontrol-v2 fault check */
        double p5600 = ctx->get_param(ctx->ctx, "5600");
        if (p5600 > 0.0) {
            double p5601 = ctx->get_param(ctx->ctx, "5601");
            if (p5601 < 0.0) {
                char msg[80];
                snprintf(msg, sizeof(msg),
                         "Toolchanger hard fault %d", (int)p5601);
                ctx->set_error(ctx->ctx, msg);
                return INTERP_EXT_ERROR;
            }
        }
        if (ctx->block_builtin_used(ctx->ctx))
            return INTERP_EXT_OK;

        double rv = ctx->get_return_value(ctx->ctx);
        if (rv > 0.0) {
            int pocket = (int)ctx->get_param(ctx->ctx, "selected_pocket");
            ctx->set_selected_pocket(ctx->ctx, pocket);
            ctx->canon_change_tool(ctx->ctx, pocket);
            ctx->set_current_pocket(ctx->ctx, pocket);
            ctx->set_selected_pocket(ctx->ctx, -1);
            ctx->set_selected_tool(ctx->ctx, -1);
            ctx->call_set_tool_parameters(ctx->ctx);
            ctx->set_toolchange_flag(ctx->ctx, 1);
            return INTERP_EXT_EXECUTE_FINISH; /* pause, flush motion */
        } else {
            /* yield to print messages, then error */
            return INTERP_EXT_EXECUTE_FINISH;
        }
    }

    /* Phase 1: after EXECUTE_FINISH */
    double rv = ctx->get_return_value(ctx->ctx);
    if (rv <= 0.0) {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "M6 aborted (return code %.1f)", rv);
        ctx->set_error(ctx->ctx, msg);
        return INTERP_EXT_ERROR;
    }
    return INTERP_EXT_OK;
}

/* ================================================================
 * M61 — settool_prolog / settool_epilog
 * REMAP=M61 modalgroup=6 prolog=settool_prolog ngc=settool epilog=settool_epilog
 * ================================================================ */

static int settool_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->block_q_flag(ctx->ctx)) {
        ctx->set_error(ctx->ctx, "M61 requires a Q parameter");
        return INTERP_EXT_ERROR;
    }
    int tool = (int)ctx->block_q_number(ctx->ctx);
    if (tool < 0) {
        ctx->set_error(ctx->ctx, "M61: Q value < 0");
        return INTERP_EXT_ERROR;
    }
    int pocket = ctx->find_tool_pocket(ctx->ctx, tool);
    if (pocket < 0) {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "M61 failed: requested tool %d not in table", tool);
        ctx->set_error(ctx->ctx, msg);
        return INTERP_EXT_ERROR;
    }
    ctx->set_param(ctx->ctx, "tool", (double)tool);
    ctx->set_param(ctx->ctx, "pocket", (double)pocket);
    return INTERP_EXT_OK;
}

static int settool_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->get_value_returned(ctx->ctx)) {
        ctx->set_error(ctx->ctx,
            "M61: remap procedure did not return a value");
        return INTERP_EXT_ERROR;
    }
    if (ctx->block_builtin_used(ctx->ctx))
        return INTERP_EXT_OK;

    double rv = ctx->get_return_value(ctx->ctx);
    if (rv > 0.0) {
        int tool = (int)ctx->get_param(ctx->ctx, "tool");
        int pocket = (int)ctx->get_param(ctx->ctx, "pocket");
        ctx->set_current_tool(ctx->ctx, tool);
        ctx->set_current_pocket(ctx->ctx, pocket);
        ctx->canon_change_tool_number(ctx->ctx, pocket);
        ctx->set_toolchange_flag(ctx->ctx, 1);
        ctx->call_set_tool_parameters(ctx->ctx);
        return INTERP_EXT_OK;
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "M61 aborted (return code %.1f)", rv);
        ctx->set_error(ctx->ctx, msg);
        return INTERP_EXT_ERROR;
    }
}

/* ================================================================
 * S — setspeed_prolog / setspeed_epilog
 * REMAP=S prolog=setspeed_prolog ngc=setspeed epilog=setspeed_epilog
 * ================================================================ */

static int setspeed_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->block_s_flag(ctx->ctx)) {
        ctx->set_error(ctx->ctx, "S requires a value");
        return INTERP_EXT_ERROR;
    }
    ctx->set_param(ctx->ctx, "speed",
                   ctx->block_s_number(ctx->ctx));
    return INTERP_EXT_OK;
}

static int setspeed_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->get_value_returned(ctx->ctx)) {
        ctx->set_error(ctx->ctx,
            "S: remap procedure did not return a value");
        return INTERP_EXT_ERROR;
    }
    double rv = ctx->get_return_value(ctx->ctx);
    if (rv < -TOLERANCE_EQUAL) {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "S: remap procedure returned %f", rv);
        ctx->set_error(ctx->ctx, msg);
        return INTERP_EXT_ERROR;
    }
    if (!ctx->block_builtin_used(ctx->ctx)) {
        double speed = ctx->get_param(ctx->ctx, "speed");
        ctx->set_speed_value(ctx->ctx, 0, speed);
        ctx->canon_enqueue_set_spindle_speed(ctx->ctx, 0, speed);
    }
    return INTERP_EXT_OK;
}

/* ================================================================
 * F — setfeed_prolog / setfeed_epilog
 * REMAP=F prolog=setfeed_prolog ngc=setfeed epilog=setfeed_epilog
 * ================================================================ */

static int setfeed_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->block_f_flag(ctx->ctx)) {
        ctx->set_error(ctx->ctx, "F requires a value");
        return INTERP_EXT_ERROR;
    }
    ctx->set_param(ctx->ctx, "feed",
                   ctx->block_f_number(ctx->ctx));
    return INTERP_EXT_OK;
}

static int setfeed_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    if (!ctx->get_value_returned(ctx->ctx)) {
        ctx->set_error(ctx->ctx,
            "F: remap procedure did not return a value");
        return INTERP_EXT_ERROR;
    }
    if (!ctx->block_builtin_used(ctx->ctx)) {
        double feed = ctx->get_param(ctx->ctx, "feed");
        ctx->set_feed_rate_value(ctx->ctx, feed);
        ctx->canon_enqueue_set_feed_rate(ctx->ctx, feed);
    }
    return INTERP_EXT_OK;
}

/* ================================================================
 * Cycle — cycle_prolog / cycle_epilog
 * Generic code-independent support for oword sub cycles.
 * REMAP=G84.3 modalgroup=1 argspec=xyzqp prolog=cycle_prolog ngc=g843 epilog=cycle_epilog
 * ================================================================ */

static int cycle_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    /* Retain the current motion mode so next line keeps it */
    int motion = ctx->block_motion_code(ctx->ctx);
    ctx->set_motion_mode(ctx->ctx, motion);
    return INTERP_EXT_OK;
}

/* ================================================================
 * cmod lifecycle — lookup API in Start(), register handlers
 * ================================================================ */

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    const char *milltask_instance;
} stdglue_module;

static int stdglue_start(cmod_t *self)
{
    stdglue_module *m = (stdglue_module *)self->priv;

    /* Look up the interp_ext API registered by milltask in its Start() */
    if (!m->env->api) {
        fprintf(stderr, "stdglue: no API registry available\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext = interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "stdglue: interp_ext API not found "
                "(instance '%s', milltask must be started first)\n", m->milltask_instance);
        return -1;
    }

    /* Register all prolog/epilog handlers */
    ext->register_remap_prolog(ext->ctx, "prepare_prolog", prepare_prolog, NULL);
    ext->register_remap_epilog(ext->ctx, "prepare_epilog", prepare_epilog, NULL);
    ext->register_remap_prolog(ext->ctx, "change_prolog", change_prolog, NULL);
    ext->register_remap_epilog(ext->ctx, "change_epilog", change_epilog, NULL);
    ext->register_remap_prolog(ext->ctx, "settool_prolog", settool_prolog, NULL);
    ext->register_remap_epilog(ext->ctx, "settool_epilog", settool_epilog, NULL);
    ext->register_remap_prolog(ext->ctx, "setspeed_prolog", setspeed_prolog, NULL);
    ext->register_remap_epilog(ext->ctx, "setspeed_epilog", setspeed_epilog, NULL);
    ext->register_remap_prolog(ext->ctx, "setfeed_prolog", setfeed_prolog, NULL);
    ext->register_remap_epilog(ext->ctx, "setfeed_epilog", setfeed_epilog, NULL);
    ext->register_remap_epilog(ext->ctx, "cycle_epilog", cycle_epilog, NULL);

    return 0;
}

static void stdglue_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
       int argc, const char **argv, cmod_t **out)
{
    stdglue_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }

    m->base.Init    = NULL;
    m->base.Start   = stdglue_start;
    m->base.Stop    = NULL;
    m->base.Destroy = stdglue_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
