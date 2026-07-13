/*
 * test_oword_math.c — test cmod for C O-word subroutines with parameter passing
 * and return values.
 *
 * Re-expression of the classic tests/remap/oword-pycall test, which exercised
 * Python O-word subroutines (o<square>, o<multiply>) with fixed and variable
 * argument counts and return values surfaced as #<_value>.  gomc removed the
 * embedded Python interpreter, so the same is done via the C interp_ext
 * register_oword mechanism:
 *
 *   o<square>   [x]        -> x*x     (single argument)
 *   o<multiply> [a] [b]... -> product (variable argument count)
 *
 * Each logs its arguments and result to stderr so a runtests test can confirm
 * the args were passed through and, by feeding a prior call's #<_value> back in
 * as an argument, that the return value round-tripped into the interpreter.
 *
 * Load via HAL:  load test_oword_math
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
} oword_math_module;

static void log_call(const char *name, const double *args, size_t n, double result)
{
    fprintf(stderr, "test_oword_math: o<%s> args_len=%zu", name, n);
    for (size_t i = 0; i < n; i++)
        fprintf(stderr, " arg[%zu]=%.4f", i, args[i]);
    fprintf(stderr, " -> %.4f\n", result);
}

static int32_t oword_square(interp_ctx_callbacks_t *ctx, const char *name,
                            const double *args, size_t args_len, double *retval)
{
    (void)ctx;
    if (args_len != 1) {
        ctx->set_error(ctx->ctx, "o<square> expects exactly one argument");
        return INTERP_EXT_ERROR;
    }
    double r = args[0] * args[0];
    log_call(name, args, args_len, r);
    if (retval)
        *retval = r;
    return 0;
}

static int32_t oword_multiply(interp_ctx_callbacks_t *ctx, const char *name,
                              const double *args, size_t args_len, double *retval)
{
    (void)ctx;
    double r = 1.0;
    for (size_t i = 0; i < args_len; i++)
        r *= args[i];
    log_call(name, args, args_len, r);
    if (retval)
        *retval = r;
    return 0;
}

static int oword_math_start(cmod_t *self)
{
    oword_math_module *m = (oword_math_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_oword_math: no API registry\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext =
        interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "test_oword_math: interp_ext API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    if (ext->register_oword(ext->ctx, "square", oword_square, NULL) != 0 ||
        ext->register_oword(ext->ctx, "multiply", oword_multiply, NULL) != 0) {
        fprintf(stderr, "test_oword_math: register_oword failed\n");
        return -1;
    }

    fprintf(stderr, "test_oword_math: o<square>/o<multiply> registered\n");
    return 0;
}

static void oword_math_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    oword_math_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }
    m->base.Start   = oword_math_start;
    m->base.Destroy = oword_math_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
