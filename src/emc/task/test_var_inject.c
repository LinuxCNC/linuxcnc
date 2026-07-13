/*
 * test_var_inject.c — test cmod for remap variable injection/retrieval.
 *
 * Re-expression of the classic tests/remap/variable-injection test, which used
 * Python remap prologs/epilogs to inject a named parameter, have the NGC body
 * modify it, and retrieve the modified value in the epilog — while confirming the
 * parameter is scoped to its own remap (not visible in a sibling remap).  gomc
 * removed the embedded Python interpreter, so the handlers are C interp_ext
 * prolog/epilog handlers using the interp_ctx set_param / get_param accessors:
 *
 *   REMAP=M405 prolog=prolog405 ngc=rm405 epilog=epilog405   (likewise 406, 407)
 *
 *   prologNNN injects  #<fooNNN> = 42
 *   rmNNN.ngc asserts   #<fooNNN> == 42, bumps it to 43, and asserts the sibling
 *                       remaps' vars are NOT visible (local scoping)
 *   epilogNNN retrieves #<fooNNN> and requires it to be 43 (the NGC change is
 *                       visible back in the epilog)
 *
 * Load via HAL:  load test_var_inject
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
} var_inject_module;

/* Derive the "fooNNN" parameter name from a "prologNNN"/"epilogNNN" handler name
 * by appending the trailing digits to "foo". */
static void param_name(const char *handler, char *out, size_t n)
{
    const char *p = handler;
    while (*p && (*p < '0' || *p > '9'))
        p++;
    snprintf(out, n, "foo%s", p);
}

/* prologNNN: inject #<fooNNN> = 42 as a local named parameter. */
static int32_t inject_prolog(interp_ctx_callbacks_t *ctx, const char *name)
{
    char pname[32];
    param_name(name, pname, sizeof(pname));
    ctx->set_param(ctx->ctx, pname, 42.0);
    fprintf(stderr, "test_var_inject: %s injected #<%s>=42\n", name, pname);
    return INTERP_EXT_OK;
}

/* epilogNNN: retrieve #<fooNNN>, which the NGC body bumped to 43. */
static int32_t inject_epilog(interp_ctx_callbacks_t *ctx, const char *name)
{
    char pname[32];
    param_name(name, pname, sizeof(pname));
    double v = ctx->get_param(ctx->ctx, pname);
    if (v != 43.0) {
        char msg[80];
        snprintf(msg, sizeof(msg), "#<%s> != 43 in %s (got %.4f)", pname, name, v);
        ctx->set_error(ctx->ctx, msg);
        return INTERP_EXT_ERROR;
    }
    fprintf(stderr, "test_var_inject: %s retrieved #<%s>=43 OK\n", name, pname);
    return INTERP_EXT_OK;
}

static int var_inject_start(cmod_t *self)
{
    var_inject_module *m = (var_inject_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_var_inject: no API registry\n");
        return -1;
    }
    const interp_ext_callbacks_t *ext =
        interp_ext_api_get(m->env->api, m->milltask_instance);
    if (!ext) {
        fprintf(stderr, "test_var_inject: interp_ext API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    static const char *nums[] = { "405", "406", "407" };
    for (size_t i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) {
        char pn[16], en[16];
        snprintf(pn, sizeof(pn), "prolog%s", nums[i]);
        snprintf(en, sizeof(en), "epilog%s", nums[i]);
        if (ext->register_remap_prolog(ext->ctx, pn, inject_prolog, NULL) != 0 ||
            ext->register_remap_epilog(ext->ctx, en, inject_epilog, NULL) != 0) {
            fprintf(stderr, "test_var_inject: registration failed for %s\n", nums[i]);
            return -1;
        }
    }

    fprintf(stderr, "test_var_inject: prolog/epilog 405/406/407 registered\n");
    return 0;
}

static void var_inject_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    var_inject_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }
    m->base.Start   = var_inject_start;
    m->base.Destroy = var_inject_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
