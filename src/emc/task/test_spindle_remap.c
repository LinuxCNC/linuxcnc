/*
 * test_spindle_remap.c — test cmod for remap spindle introspection.
 *
 * Re-expression of the classic tests/remap/spindle test, which used a Python
 * remap handler (M500 py=m500) to introspect the interpreter's spindle state
 * (self.speed[] / self.active_spindle) from inside a remap.  gomc removed the
 * embedded Python interpreter, so the same capability is exercised here through
 * the C interp_ext mechanism: a remap prolog for M500 that reads the per-spindle
 * commanded speed via the interp_ctx get_speed() accessor and logs it, so a
 * runtests test can confirm the remap sees the live spindle state.
 *
 *   REMAP= M500 modalgroup=10 argspec=P prolog=m500_prolog ngc=m500
 *
 * Load via HAL:  load test_spindle_remap
 *
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gomc/pkg/cmodule/gomc_env.h"
#include "interp_ext_api.h"

/* Number of spindles to introspect (matches [TRAJ]SPINDLES in the test ini). */
#define DEFAULT_SPINDLES 3

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    const char *milltask_instance;
    int spindles;
} spindle_remap_module;

/* remember the configured spindle count for the (stateless) prolog callback */
static int g_spindles = DEFAULT_SPINDLES;

/* M500 remap prolog: introspect the live spindle speeds and log them.
 * argspec=P makes the P word available as the named local param #<p>. */
static int32_t m500_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    (void)name;
    int p = (int)ctx->get_param(ctx->ctx, "p");

    fprintf(stderr, "test_spindle_remap: M500 P%d speeds=[", p);
    for (int s = 0; s < g_spindles; s++)
        fprintf(stderr, "%s%.4f", s ? "," : "", ctx->get_speed(ctx->ctx, s));
    fprintf(stderr, "]\n");

    return INTERP_EXT_OK;
}

static int spindle_remap_start(cmod_t *self)
{
    spindle_remap_module *m = (spindle_remap_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_spindle_remap: no API registry\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext =
        interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "test_spindle_remap: interp_ext API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    g_spindles = m->spindles;
    if (ext->register_remap_prolog(ext->ctx, "m500_prolog", m500_prolog, NULL) != 0) {
        fprintf(stderr, "test_spindle_remap: register_remap_prolog failed\n");
        return -1;
    }

    fprintf(stderr, "test_spindle_remap: M500 prolog m500_prolog registered (%d spindles)\n",
            m->spindles);
    return 0;
}

static void spindle_remap_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    spindle_remap_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    m->spindles = DEFAULT_SPINDLES;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
        else if (strncmp(argv[i], "spindles=", 9) == 0)
            m->spindles = atoi(argv[i] + 9);
    }
    m->base.Start   = spindle_remap_start;
    m->base.Destroy = spindle_remap_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
