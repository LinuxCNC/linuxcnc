/*
 * test_interp_ext_finish.c — test cmod for a C interp_ext handler returning
 * INTERP_EXECUTE_FINISH (the queue-buster / read-ahead-barrier return path).
 *
 * The M66 re-expression (tests/interp/mdi-oword-m66) covers the *NGC*-side
 * queue-buster semantic; nothing exercised a *C* handler returning
 * EXECUTE_FINISH.  The only shipping C handler that does is stdglue.c
 * change_epilog() (the M6 toolchange epilog) — but M6 also sets toolchange_flag,
 * so it never isolates the pure C-return path.
 *
 * This cmod registers a remap prolog for a custom M-code (M510) that:
 *   - phase 0 (first execution): returns INTERP_EXT_EXECUTE_FINISH, forcing the
 *     interpreter to flush read-ahead, drain the motion queue, and re-execute
 *     the prolog after the drain;
 *   - phase 1 (re-execution after the queue drained): returns INTERP_EXT_OK so
 *     the remap completes and its (no-op) NGC body runs.
 * The remap sets no builtin _flag (M510 is not a user M-code and not M6/M66/
 * probe), so the observed barrier is attributable solely to the handler's
 * EXECUTE_FINISH return.  Each invocation logs its phase (read via the interp_ctx
 * get_phase() accessor) so a runtests test can confirm the two-phase cycle.
 *
 *   REMAP = M510 modalgroup=10 prolog=finish_prolog ngc=m510
 *
 * Load via HAL:  load test_interp_ext_finish
 * Args: milltask_instance=<name> (default "milltask").
 *
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gomc/pkg/cmodule/gomc_env.h"
#include "interp_ext_api.h"

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    const char *milltask_instance;
} finish_remap_module;

/* M510 remap prolog: return EXECUTE_FINISH the first time (phase 0) to force the
 * queue-buster, then OK on the post-drain re-execution (phase 1). */
static int32_t finish_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    (void)name;
    int phase = ctx->get_phase(ctx->ctx);

    fprintf(stderr, "test_interp_ext_finish: M510 prolog, phase=%d\n", phase);

    if (phase == 0)
        return INTERP_EXT_EXECUTE_FINISH;
    return INTERP_EXT_OK;
}

static int finish_remap_start(cmod_t *self)
{
    finish_remap_module *m = (finish_remap_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_interp_ext_finish: no API registry\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext =
        interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "test_interp_ext_finish: interp_ext API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    if (ext->register_remap_prolog(ext->ctx, "finish_prolog", finish_prolog, NULL) != 0) {
        fprintf(stderr, "test_interp_ext_finish: register_remap_prolog failed\n");
        return -1;
    }

    fprintf(stderr, "test_interp_ext_finish: M510 prolog finish_prolog registered\n");
    return 0;
}

static void finish_remap_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    finish_remap_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }
    m->base.Start   = finish_remap_start;
    m->base.Destroy = finish_remap_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
