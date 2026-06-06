/*
 * enum.c — cmod HAL component: convert enumerated ints to/from bit pins.
 *
 * Each instance is either an encoder (bits→int) or decoder (int→bits).
 * The mode and pin names are specified in the 'enums' argument string.
 *
 * Usage:
 *   load enum enums="D;off;on;error" [name=my-decoder]
 *   load enum enums="E;idle;run;fault" [name=my-encoder]
 *
 * The first token after the mode letter (D or E) defines direction:
 *   D = decode (int input → bit outputs)
 *   E = encode (bit inputs → int output)
 * Subsequent ';'-separated tokens are the enumeration names.
 * Empty tokens (;;) skip an enumeration value.
 *
 * Original author: Andy Pugh
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

typedef struct {
    gomc_hal_bit_t *bit;
    gomc_hal_u32_t *en;
} enum_pin_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    int dir;         /* GOMC_HAL_IN for encode, GOMC_HAL_OUT for decode */
    int num_pins;
    enum_pin_t *pins;
    gomc_hal_u32_t *int_pin; /* the single int pin (input for decode, output for encode) */
} inst_t;

static void decode(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int i;
    for (i = 0; i < inst->num_pins; i++) {
        *(inst->pins[i].bit) = (*(inst->int_pin) == *(inst->pins[i].en)) ? 1 : 0;
    }
}

static void encode(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    int i;
    *(inst->int_pin) = 0;
    for (i = 0; i < inst->num_pins; i++) {
        if (*(inst->pins[i].bit))
            *(inst->int_pin) = *(inst->pins[i].en);
    }
}

static void inst_destroy(cmod_t *self) {
    inst_t *inst = (inst_t *)self;
    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    inst_t *inst;
    int r, i, j, v;
    const char *enums_arg = NULL;
    char *enums_copy = NULL;
    char *token;
    int dir;
    int num_pins;

    /* parse arguments */
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "enums=", 6) == 0)
            enums_arg = argv[i] + 6;
    }

    if (!enums_arg || !*enums_arg) {
        gomc_log_errorf(env->log, name,
                        "enum requires enums= argument (e.g. enums=\"D;off;on;error\")");
        return -EINVAL;
    }

    /* make a mutable copy of the enums string */
    size_t elen = strlen(enums_arg);
    enums_copy = env->rtapi->calloc(env->rtapi->ctx, elen + 1);
    if (!enums_copy) return -ENOMEM;
    memcpy(enums_copy, enums_arg, elen + 1);

    /* determine direction from first character */
    switch (enums_copy[0]) {
    case 'E': case 'e':
        dir = GOMC_HAL_IN;  /* bit pins are inputs (encode) */
        break;
    case 'D': case 'd':
        dir = GOMC_HAL_OUT; /* bit pins are outputs (decode) */
        break;
    default:
        gomc_log_errorf(env->log, name,
                        "enums string must start with E; or D;");
        env->rtapi->free(env->rtapi->ctx, enums_copy);
        return -EINVAL;
    }

    /* count pins: number of ';' that are not consecutive */
    num_pins = 0;
    for (j = (int)elen - 1; j > 0; j--) {
        if (enums_copy[j] == ';') {
            if (j > 0 && enums_copy[j - 1] != ';')
                num_pins++;
            enums_copy[j] = '\0'; /* replace ; with NUL for tokenization */
        }
    }

    if (num_pins < 1) {
        gomc_log_errorf(env->log, name, "enum: no enumeration values found");
        env->rtapi->free(env->rtapi->ctx, enums_copy);
        return -EINVAL;
    }

    /* allocate instance */
    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(inst_t));
    if (!inst) {
        env->rtapi->free(env->rtapi->ctx, enums_copy);
        return -ENOMEM;
    }

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->dir = dir;
    inst->num_pins = num_pins;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->pins = env->hal->malloc(env->hal->ctx, num_pins * sizeof(enum_pin_t));
    if (!inst->pins) goto err;
    memset(inst->pins, 0, num_pins * sizeof(enum_pin_t));

    /* create the single integer pin */
    if (dir == GOMC_HAL_OUT) {
        /* decode: int is input */
        r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IN, &inst->int_pin,
                                  inst->comp_id, "%s.input", name);
    } else {
        /* encode: int is output */
        r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_OUT, &inst->int_pin,
                                  inst->comp_id, "%s.output", name);
    }
    if (r != 0) goto err;

    /* create per-enumeration bit + value pins */
    token = enums_copy;
    /* skip past the mode character token */
    while (*token != '\0') token++;
    token++; /* skip NUL after mode char */

    v = 0;
    for (i = 0; i < num_pins; i++) {
        /* skip empty tokens (consecutive NULs = skipped enum values) */
        while (*token == '\0') { token++; v++; }

        r = gomc_hal_pin_bit_newf(env->hal, dir, &inst->pins[i].bit,
                                  inst->comp_id, "%s.%s-%s", name, token,
                                  (dir == GOMC_HAL_IN) ? "in" : "out");
        if (r != 0) goto err;

        r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IN, &inst->pins[i].en,
                                  inst->comp_id, "%s.%s-val", name, token);
        if (r != 0) goto err;
        *(inst->pins[i].en) = v++;

        /* advance token past this name */
        while (*token != '\0') token++;
        token++;
    }

    /* export function */
    if (dir == GOMC_HAL_OUT) {
        r = env->hal->export_funct(env->hal->ctx, name, decode, inst, 0, 0,
                                   inst->comp_id);
    } else {
        r = env->hal->export_funct(env->hal->ctx, name, encode, inst, 0, 0,
                                   inst->comp_id);
    }
    if (r != 0) goto err;

    r = env->hal->ready(env->hal->ctx, inst->comp_id);
    if (r != 0) goto err;

    env->rtapi->free(env->rtapi->ctx, enums_copy);
    *out = &inst->base;
    return 0;

err:
    if (inst->comp_id > 0)
        env->hal->exit(env->hal->ctx, inst->comp_id);
    env->rtapi->free(env->rtapi->ctx, enums_copy);
    env->rtapi->free(env->rtapi->ctx, inst);
    return -1;
}
