/*
 * test_mcode_handler.c — Test cmod for M-code handler API.
 *
 * Registers a handler for M101 that logs to stderr and returns success.
 * Used to verify the mcode_handler_api works end-to-end.
 *
 * Load via INI: [GOMC] CMOD=test_mcode_handler
 *
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include "gomc/pkg/cmodule/gomc_env.h"
#include "mcode_handler_api.h"

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    const char *milltask_instance;
} test_mcode_module;

static int my_m101_handler(const mcode_handler_mcode_call_t *call, void *user_data)
{
    fprintf(stderr, "test_mcode_handler: M%d called, P=%.4f Q=%.4f\n",
            call->mcode, call->p_number, call->q_number);

    /* Simulate a brief operation, checking abort_fd */
    struct pollfd pfd = { .fd = call->abort_fd, .events = POLLIN };
    for (int i = 0; i < 5; i++) {
        if (poll(&pfd, 1, 100) > 0) {
            fprintf(stderr, "test_mcode_handler: M%d aborted\n", call->mcode);
            return -1;
        }
    }

    fprintf(stderr, "test_mcode_handler: M%d completed\n", call->mcode);
    return 0;
}

static int test_mcode_start(cmod_t *self)
{
    test_mcode_module *m = (test_mcode_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "test_mcode_handler: no API registry\n");
        return -1;
    }

    const mcode_handler_callbacks_t *mapi =
        mcode_handler_api_get(m->env->api, m->milltask_instance);
    if (!mapi) {
        fprintf(stderr, "test_mcode_handler: mcode_handler API not found (instance '%s')\n", m->milltask_instance);
        return -1;
    }

    if (mapi->register_handler(mapi->ctx, 101, my_m101_handler, NULL) != 0) {
        fprintf(stderr, "test_mcode_handler: failed to register M101 handler\n");
        return -1;
    }

    fprintf(stderr, "test_mcode_handler: M101 handler registered\n");
    return 0;
}

static void test_mcode_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
       int argc, const char **argv, cmod_t **out)
{
    test_mcode_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
    }
    m->base.Start   = test_mcode_start;
    m->base.Destroy = test_mcode_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
