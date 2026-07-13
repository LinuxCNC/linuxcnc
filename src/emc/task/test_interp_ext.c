/*
 * test_interp_ext.c — test cmod for the interp_ext API.
 *
 * The interp_ext API (register_oword / register_remap_prolog / _epilog) is the
 * gomc replacement for the classic Python interpreter extension points (Python
 * O-word subroutines and py= remap prolog/epilog).  Remap prologs are already
 * exercised in production by stdglue.c; register_oword had no test at all.
 *
 * This cmod registers a C O-word subroutine `o<test_oword>` that returns the
 * sum of its call arguments, logging the invocation to stderr so a runtests
 * test can confirm it was dispatched with the right args.
 *
 * Load via HAL:  load test_interp_ext
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
} test_ext_module;

/* O-word handler: log the call and return a fixed value.
 *
 * NB: gomc does not currently populate the call-frame positional params at
 * C-oword dispatch time, so args_len is always 0 (see PRODUCTION_READINESS.md).
 * This test therefore verifies registration + dispatch + the fixed return value,
 * not argument passing. */
static int32_t test_oword(interp_ctx_callbacks_t *ctx, const char *name,
                          const double *args, size_t args_len, double *retval)
{
    (void)ctx;
    (void)args;
    fprintf(stderr, "test_interp_ext: oword '%s' called (n_args=%zu), returning 42\n",
            name, args_len);
    if (retval)
        *retval = 42.0;
    return 0;
}

static int test_ext_start(cmod_t *self)
{
    test_ext_module *m = (test_ext_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_interp_ext: no API registry\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext =
        interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "test_interp_ext: interp_ext API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    if (ext->register_oword(ext->ctx, "test_oword", test_oword, NULL) != 0) {
        fprintf(stderr, "test_interp_ext: register_oword failed\n");
        return -1;
    }

    fprintf(stderr, "test_interp_ext: o<test_oword> registered\n");
    return 0;
}

static void test_ext_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    test_ext_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }
    m->base.Start   = test_ext_start;
    m->base.Destroy = test_ext_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
