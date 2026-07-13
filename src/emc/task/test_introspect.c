/*
 * test_introspect.c — test cmod for O-word introspection of interpreter state.
 *
 * Re-expression of the classic tests/remap/introspect test, whose Python O-word
 * o<introspect> reached into the interpreter to read its call arguments plus live
 * state (current feed, spindle speed, named/predefined/INI parameters) and
 * returned a value.  gomc removed the embedded Python interpreter; the
 * re-expression uses the C interp_ext register_oword mechanism with the
 * interp_ctx accessors (get_feed_rate / get_speed / get_param).
 *
 * The Python-binding-only bits of the original (self.blocks[].params low-level
 * arrays, sub_context named_params iteration, params.locals()/globals() listing,
 * self.remaps) have no C interp_ext equivalent and are intentionally dropped —
 * they were the removed embedded-Python introspection API, not interpreter state.
 *
 *   o<introspect> [1] [2] [3] [#<_ini[example]variable>]  -> 2.71828
 *
 * Load via HAL:  load test_introspect
 *
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gomc/pkg/cmodule/gomc_env.h"
#include "interp_ext_api.h"

#define INTROSPECT_RETVAL 2.71828

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    const char *milltask_instance;
} introspect_module;

static int32_t oword_introspect(interp_ctx_callbacks_t *ctx, const char *name,
                                const double *args, size_t args_len, double *retval)
{
    (void)name;

    fprintf(stderr, "test_introspect: argc=%zu args=[", args_len);
    for (size_t i = 0; i < args_len; i++)
        fprintf(stderr, "%s%.5f", i ? "," : "", args[i]);
    fprintf(stderr, "]\n");

    /* Live interpreter state via the interp_ctx accessors. */
    fprintf(stderr, "test_introspect: feed=%.4f rpm=%.4f\n",
            ctx->get_feed_rate(ctx->ctx), ctx->get_speed(ctx->ctx, 0));

    /* Named / predefined / INI / user-global parameters via get_param. */
    double ini = ctx->get_param(ctx->ctx, "_ini[example]variable");
    fprintf(stderr, "test_introspect: _feed=%.4f _rpm=%.4f global=%.4f ini=%.5f\n",
            ctx->get_param(ctx->ctx, "_feed"),
            ctx->get_param(ctx->ctx, "_rpm"),
            ctx->get_param(ctx->ctx, "_a_global_set_in_test_dot_ngc"),
            ini);

    /* The 4th call argument is #<_ini[example]variable> evaluated by the NGC
     * caller; it must equal what get_param reads for the same INI variable. */
    if (args_len >= 4 && args[3] != ini) {
        ctx->set_error(ctx->ctx, "o<introspect>: INI arg != _ini[example]variable");
        return INTERP_EXT_ERROR;
    }

    if (retval)
        *retval = INTROSPECT_RETVAL;
    return 0;
}

static int introspect_start(cmod_t *self)
{
    introspect_module *m = (introspect_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_introspect: no API registry\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext =
        interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "test_introspect: interp_ext API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    if (ext->register_oword(ext->ctx, "introspect", oword_introspect, NULL) != 0) {
        fprintf(stderr, "test_introspect: register_oword failed\n");
        return -1;
    }

    fprintf(stderr, "test_introspect: o<introspect> registered\n");
    return 0;
}

static void introspect_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    introspect_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }
    m->base.Start   = introspect_start;
    m->base.Destroy = introspect_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
