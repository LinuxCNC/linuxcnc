/*
 * test_remap_fail.c — test cmod for remap prolog/epilog failure handling.
 *
 * Re-expression of the classic tests/remap/fail/{prolog,epilog} tests, which
 * used Python remap handlers that returned INTERP_ERROR to check that a failing
 * prolog/epilog properly aborts the remap, conveys the error text, and unwinds
 * back to top level (call_level/remap_level restored).  gomc removed the embedded
 * Python interpreter, so the same is exercised through the C interp_ext mechanism:
 *
 *   failingprolog  — register_remap_prolog handler; set_error() + INTERP_EXT_ERROR.
 *   failingepilog  — register_remap_epilog handler; set_error() + INTERP_EXT_ERROR.
 *   o<mark_body>   — register_oword; logs "body executed" so a test can assert the
 *                    remap NGC body ran (epilog case) or did NOT run (prolog case,
 *                    where the prolog fails before the body is reached).
 *
 * Load via HAL:  load test_remap_fail
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
} remap_fail_module;

static int32_t failing_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    (void)name;
    fprintf(stderr, "test_remap_fail: prolog failing with INTERP_ERROR\n");
    ctx->set_error(ctx->ctx, "A failed C prolog returning INTERP_ERROR");
    return INTERP_EXT_ERROR;
}

static int32_t failing_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    (void)name;
    fprintf(stderr, "test_remap_fail: epilog failing with INTERP_ERROR\n");
    ctx->set_error(ctx->ctx, "A failed C epilog returning INTERP_ERROR");
    return INTERP_EXT_ERROR;
}

/* O-word marker: proves the remap NGC body was (or was not) executed. */
static int32_t mark_body(interp_ctx_callbacks_t *ctx, const char *name,
                         const double *args, size_t args_len, double *retval)
{
    (void)ctx; (void)args; (void)args_len;
    fprintf(stderr, "test_remap_fail: body executed (o<%s>)\n", name);
    if (retval)
        *retval = 0.0;
    return 0;
}

static int remap_fail_start(cmod_t *self)
{
    remap_fail_module *m = (remap_fail_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_remap_fail: no API registry\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext =
        interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "test_remap_fail: interp_ext API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    if (ext->register_remap_prolog(ext->ctx, "failingprolog", failing_prolog, NULL) != 0 ||
        ext->register_remap_epilog(ext->ctx, "failingepilog", failing_epilog, NULL) != 0 ||
        ext->register_oword(ext->ctx, "mark_body", mark_body, NULL) != 0) {
        fprintf(stderr, "test_remap_fail: registration failed\n");
        return -1;
    }

    fprintf(stderr, "test_remap_fail: failingprolog/failingepilog/mark_body registered\n");
    return 0;
}

static void remap_fail_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    remap_fail_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }
    m->base.Start   = remap_fail_start;
    m->base.Destroy = remap_fail_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
