/*
 * mcode_coord_log.c — cmod that registers an M-code handler (default M100) to
 * log coordinate / tool-offset introspection values to a file.
 *
 * This is the gomc replacement for the classic USER_M_PATH `subs/M100` shell
 * script used by the tool tests (tests/t0, tests/toolchanger/toolno-pocket-differ):
 * gomc has no USER_M_PATH shell-script M-codes — user M-codes are cmods that
 * register via the mcode_handler GMI API. The test g-code calls
 *   M100 P<sel> Q<value>
 * where P selects the field and Q carries the (interp-resolved) value; each call
 * appends one line to the log file, matching the classic script's format:
 *   P0->"X = Q"  P1->"Y = Q"  P2->"Z = Q"  P3->"toolno = Q"
 *   P4->"tlo_z = Q"  P5->blank line  P6->"sequence number Q"
 *
 * Load via HAL:  load mcode_coord_log logfile=gcode-output
 * Args: logfile=<path> (default "gcode-output"), mcode=<n> (default 100),
 *       milltask_instance=<name> (default "milltask").
 *
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gomc/pkg/cmodule/gomc_env.h"
#include "mcode_handler_api.h"

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    const char *milltask_instance;
    int mcode;
    int raw;   /* format=raw: dump "P is <P>\nQ is <Q>" like the classic
                * linuxcncrsh subs/M100, instead of the P-selector fields. */
    char logfile[512];
} coord_log_module;

static int coord_log_handler(const mcode_handler_mcode_call_t *call, void *user_data)
{
    coord_log_module *m = (coord_log_module *)user_data;
    FILE *f = fopen(m->logfile, "a");
    if (!f)
        return -1;

    double q = call->q_number;
    int rc = 0;

    /* Raw mode: dump both words verbatim, matching the classic linuxcncrsh
     * subs/M100 ("echo P is $P; echo Q is $Q"). */
    if (m->raw) {
        fprintf(f, "P is %f\n", call->p_number);
        fprintf(f, "Q is %f\n", q);
        fclose(f);
        return 0;
    }

    /* P is an integer field selector; Q is the value (formatted like the classic
     * user-M shell script, which received the %f-formatted M-code words). */
    int p = (int)(call->p_number + (call->p_number < 0 ? -0.5 : 0.5));

    switch (p) {
    case 0: fprintf(f, "X = %f\n", q); break;
    case 1: fprintf(f, "Y = %f\n", q); break;
    case 2: fprintf(f, "Z = %f\n", q); break;
    case 3: fprintf(f, "toolno = %f\n", q); break;
    case 4: fprintf(f, "tlo_z = %f\n", q); break;
    case 5: fprintf(f, "\n"); break;
    case 6: fprintf(f, "sequence number %f\n", q); break;
    default:
        fprintf(f, "unknown P=%f (Q=%f)\n", call->p_number, q);
        rc = -1;
        break;
    }

    fclose(f);
    return rc;
}

static int coord_log_start(cmod_t *self)
{
    coord_log_module *m = (coord_log_module *)self->priv;

    if (!m->env->api) {
        fprintf(stderr, "mcode_coord_log: no API registry\n");
        return -1;
    }

    const mcode_handler_callbacks_t *mapi =
        mcode_handler_api_get(m->env->api, m->milltask_instance);
    if (!mapi) {
        fprintf(stderr, "mcode_coord_log: mcode_handler API not found (instance '%s')\n",
                m->milltask_instance);
        return -1;
    }

    if (mapi->register_handler(mapi->ctx, m->mcode, coord_log_handler, m) != 0) {
        fprintf(stderr, "mcode_coord_log: failed to register M%d handler\n", m->mcode);
        return -1;
    }

    return 0;
}

static void coord_log_destroy(cmod_t *self)
{
    free(self->priv);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    coord_log_module *m = calloc(1, sizeof(*m));
    if (!m)
        return -1;
    m->env = env;
    m->milltask_instance = "milltask";
    m->mcode = 100;
    snprintf(m->logfile, sizeof(m->logfile), "gcode-output");

    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "milltask_instance=", 18) == 0)
            m->milltask_instance = argv[i] + 18;
        else if (strncmp(argv[i], "logfile=", 8) == 0)
            snprintf(m->logfile, sizeof(m->logfile), "%s", argv[i] + 8);
        else if (strncmp(argv[i], "mcode=", 6) == 0)
            m->mcode = atoi(argv[i] + 6);
        else if (strcmp(argv[i], "format=raw") == 0)
            m->raw = 1;
    }

    m->base.Start   = coord_log_start;
    m->base.Destroy = coord_log_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
