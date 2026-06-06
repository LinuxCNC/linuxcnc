//
//    Copyright (C) 2013 Andy Pugh
//    cmod port (C) 2026
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

/* A component to stream HAL data to an LCD in a user-defined format */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>

#include "gomc_env.h"

#define MAX_ENTRY 20

typedef volatile int32_t   hal_s32_t;
typedef volatile uint32_t  hal_u32_t;
typedef volatile double    hal_float_t;
typedef volatile unsigned  hal_bit_t;

typedef struct {
    void **args;
    int num_args;
    char *fmt;
    int length;
} lcd_page_t;

typedef struct {
    lcd_page_t *pages;
    int num_pages;
    hal_u32_t *page_num;
    hal_u32_t last_page;
    hal_u32_t *out;
    hal_float_t *contrast;
    hal_float_t last_contrast;
    char buff[MAX_ENTRY + 1];
    int c_ptr;
    int f_ptr;
    int a_ptr;
    unsigned int dp;
} lcd_inst_t;

typedef struct {
    cmod_env_t env;
    cmod_t mod;
    int comp_id;
    char name[64];
    lcd_inst_t inst;
} lcd_priv_t;

static void do_write(void *arg, long period);
static void write_one(lcd_inst_t *inst);
static int parse_fmt(char *in, int *ptr, char *out, void *val, char dp);

static const char *digits = "0123456789ABCDEF";

#ifndef do_div
# define do_div(n,base) ({                                       \
            uint32_t __base = (base);                            \
            uint32_t __rem;                                      \
            __rem = ((uint64_t)(n)) % __base;                    \
            (n) = ((uint64_t)(n)) / __base;                      \
            __rem;                                               \
        })
#endif

static int num_digits_baseN(uint64_t val, int base) {
    int n = 1;
    uint64_t m = 1;
    while ((m *= base) <= val) {
        n += 1;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

static void lcd_destroy(cmod_t *self) {
    lcd_priv_t *priv = (lcd_priv_t *)self->priv;
    if (priv->comp_id > 0) {
        priv->env.hal->exit(priv->env.hal->ctx, priv->comp_id);
    }
    // Free pages and args (hal_malloc'd, freed when HAL shuts down)
    free(priv);
}

// ---------------------------------------------------------------------------
// New — constructor
// ---------------------------------------------------------------------------

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    int i, f, f1, k, p;
    int retval;
    const char *fmt_string = NULL;

    // Parse argv for fmt= parameter
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "fmt=", 4) == 0) {
            fmt_string = argv[i] + 4;
        }
    }

    if (!fmt_string || !fmt_string[0]) {
        gomc_log_errorf(env->log, name,
                        "The LCD component requires a 'fmt=' parameter");
        return -EINVAL;
    }

    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -EINVAL;
    }

    lcd_priv_t *priv = calloc(1, sizeof(lcd_priv_t));
    if (!priv) return -ENOMEM;

    priv->env = *env;
    snprintf(priv->name, sizeof(priv->name), "%s", name);

    priv->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (priv->comp_id < 0) {
        gomc_log_errorf(env->log, name, "hal init failed");
        free(priv);
        return -1;
    }

    lcd_inst_t *inst = &priv->inst;
    inst->num_pages = 1;

    // Make a mutable copy of fmt_string
    int fmt_len = strlen(fmt_string);
    char *fmt_copy = (char *)env->hal->malloc(env->hal->ctx, fmt_len + 1);
    if (!fmt_copy) {
        gomc_log_errorf(env->log, name, "Out of memory");
        env->hal->exit(env->hal->ctx, priv->comp_id);
        free(priv);
        return -ENOMEM;
    }
    memcpy(fmt_copy, fmt_string, fmt_len + 1);

    // Count pages demarked by | chars
    for (f = 0; fmt_copy[f]; f++) {
        if (fmt_copy[f] == '|') inst->num_pages++;
    }
    inst->pages = (lcd_page_t *)env->hal->malloc(env->hal->ctx,
                                                  inst->num_pages * sizeof(lcd_page_t));
    if (!inst->pages) {
        gomc_log_errorf(env->log, name, "Out of memory");
        env->hal->exit(env->hal->ctx, priv->comp_id);
        free(priv);
        return -ENOMEM;
    }
    memset(inst->pages, 0, inst->num_pages * sizeof(lcd_page_t));

    // Second pass: parse format strings and create pins
    f1 = k = p = 0;
    for (f = 0; fmt_copy[f]; f++) {
        if (fmt_copy[f] == '%') {
            int type = parse_fmt(fmt_copy, &f, NULL, NULL, 0);
            if (type > 0) {
                inst->pages[p].num_args++;
            }
        }

        if (fmt_copy[f + 1] == '|' || fmt_copy[f + 1] == 0) {
            inst->pages[p].fmt = (char *)env->hal->malloc(env->hal->ctx, f - f1 + 2);
            snprintf(inst->pages[p].fmt, f - f1 + 2, "%s", fmt_copy + f1);
            inst->pages[p].length = f - f1 + 2;
            inst->pages[p].args = (void **)env->hal->malloc(env->hal->ctx,
                                      inst->pages[p].num_args * sizeof(hal_float_t *));
            f1 = f + 2;
            {
                int a = -1, s = -1;
                lcd_page_t page = inst->pages[p];

                while (page.fmt[++s]) {
                    if (page.fmt[s] == '%') {
                        int type = parse_fmt(page.fmt, &s, NULL, NULL, 0);
                        a++;
                        char pin_name[128];
                        switch (type) {
                        case 'f':
                            snprintf(pin_name, sizeof(pin_name),
                                     "%s.page.%02i.arg.%02i", name, p, a);
                            retval = env->hal->pin_new(env->hal->ctx, pin_name,
                                         GOMC_HAL_FLOAT, GOMC_HAL_IN,
                                         (void **)&(inst->pages[p].args[a]),
                                         priv->comp_id);
                            if (retval != 0) goto fail;
                            break;
                        case 'u':
                        case 'c':
                            snprintf(pin_name, sizeof(pin_name),
                                     "%s.page.%02i.arg.%02i", name, p, a);
                            retval = env->hal->pin_new(env->hal->ctx, pin_name,
                                         GOMC_HAL_U32, GOMC_HAL_IN,
                                         (void **)&(inst->pages[p].args[a]),
                                         priv->comp_id);
                            if (retval != 0) goto fail;
                            break;
                        case 's':
                            snprintf(pin_name, sizeof(pin_name),
                                     "%s.page.%02i.arg.%02i", name, p, a);
                            retval = env->hal->pin_new(env->hal->ctx, pin_name,
                                         GOMC_HAL_S32, GOMC_HAL_IN,
                                         (void **)&(inst->pages[p].args[a]),
                                         priv->comp_id);
                            if (retval != 0) goto fail;
                            break;
                        case 'b':
                            snprintf(pin_name, sizeof(pin_name),
                                     "%s.page.%02i.arg.%02i", name, p, a);
                            retval = env->hal->pin_new(env->hal->ctx, pin_name,
                                         GOMC_HAL_BIT, GOMC_HAL_IN,
                                         (void **)&(inst->pages[p].args[a]),
                                         priv->comp_id);
                            if (retval != 0) goto fail;
                            break;
                        }
                    }
                }
            }
            p++;
        }
    }

    // Export the write function
    retval = env->hal->export_funct(env->hal->ctx, name,
                                    do_write, inst, 1, 0, priv->comp_id);
    if (retval < 0) {
        gomc_log_errorf(env->log, name, "function export failed");
        goto fail;
    }

    // Create page_num, out, contrast pins and decimal-separator param
    {
        char pin_name[128];

        snprintf(pin_name, sizeof(pin_name), "%s.page_num", name);
        retval = env->hal->pin_new(env->hal->ctx, pin_name,
                     GOMC_HAL_U32, GOMC_HAL_IN,
                     (void **)&inst->page_num, priv->comp_id);
        if (retval != 0) goto fail;

        snprintf(pin_name, sizeof(pin_name), "%s.out", name);
        retval = env->hal->pin_new(env->hal->ctx, pin_name,
                     GOMC_HAL_U32, GOMC_HAL_OUT,
                     (void **)&inst->out, priv->comp_id);
        if (retval != 0) goto fail;

        snprintf(pin_name, sizeof(pin_name), "%s.contrast", name);
        retval = env->hal->pin_new(env->hal->ctx, pin_name,
                     GOMC_HAL_FLOAT, GOMC_HAL_IN,
                     (void **)&inst->contrast, priv->comp_id);
        if (retval != 0) goto fail;

        snprintf(pin_name, sizeof(pin_name), "%s.decimal-separator", name);
        retval = env->hal->param_new(env->hal->ctx, pin_name,
                     GOMC_HAL_U32, GOMC_HAL_RW,
                     (void *)&inst->dp, priv->comp_id);
        if (retval != 0) goto fail;
    }

    inst->dp = '.';
    inst->last_page = (uint32_t)-1; // force screen refresh
    inst->f_ptr = 0;
    inst->buff[0]  = 0x0D; // CR
    inst->buff[1]  = 0x0A; // LF
    inst->buff[2]  = 0x18; // CLEAR
    inst->buff[3]  = 0x0D; // CR
    inst->buff[4]  = 0x0A; // LF
    inst->buff[5]  = 0x18; // CLEAR
    inst->buff[6]  = 0x0D; // CR
    inst->buff[7]  = 0x0A; // LF
    inst->buff[8]  = 0x18; // CLEAR
    inst->buff[9]  = 0x0D; // CR
    inst->buff[10] = 0x0A; // LF
    inst->buff[11] = 0x18; // CLEAR
    inst->buff[12] = 0x11; // cursor off
    inst->buff[13] = 0;
    inst->c_ptr = 0;

    env->hal->ready(env->hal->ctx, priv->comp_id);

    // Set up cmod_t
    priv->mod.priv = priv;
    priv->mod.Init = NULL;
    priv->mod.Start = NULL;
    priv->mod.Stop = NULL;
    priv->mod.Destroy = lcd_destroy;

    *out = &priv->mod;
    return 0;

fail:
    env->hal->exit(env->hal->ctx, priv->comp_id);
    free(priv);
    return retval;
}

// ---------------------------------------------------------------------------
// RT function — streams formatted data one character at a time
// ---------------------------------------------------------------------------

static void do_write(void *arg, long period) {
    lcd_inst_t *inst = (lcd_inst_t *)arg;
    write_one(inst);
}

static void write_one(lcd_inst_t *inst) {
    static int counter = 100; // 100 cycle delay before start
    int retval;
    char c1, c2;

    if (counter > 0) {
        --counter;
        return;
    }

    if (inst->buff[inst->c_ptr] != 0) {
        *inst->out = inst->buff[inst->c_ptr++];
        return;
    }

    inst->c_ptr = 0;
    inst->buff[0] = 0;

    if (*inst->page_num >= (uint32_t)inst->num_pages) return;

    if (*inst->page_num != inst->last_page) {
        inst->last_page = *inst->page_num;
        *inst->out = 0x11; // cursor off
        inst->buff[0] = 0x1E; // cursor home
        inst->buff[1] = 0x1A; // clear screen
        inst->buff[2] = 0;
        inst->c_ptr = 0;
        inst->f_ptr = 0;
        inst->a_ptr = 0;
        return;
    }

    if (inst->f_ptr >= inst->pages[*inst->page_num].length) {
        *inst->out = 0x1B; // ESC
        inst->buff[0] = 0x3D; // =
        inst->buff[1] = 0x20; // Line 0
        inst->buff[2] = 0x20; // Column 0
        inst->buff[3] = 0;
        inst->c_ptr = 0;
        inst->f_ptr = 0;
        inst->a_ptr = 0;
        return;
    }

    if (*inst->contrast != inst->last_contrast) {
        int c = *inst->contrast * 159.0 + 0x20;
        if (c > 0xBF) c = 0xBF;
        if (c < 0x20) c = 0x20;
        inst->last_contrast = *inst->contrast;
        *inst->out = 0x1B;
        inst->buff[0] = 'C';
        inst->buff[1] = c;
        inst->buff[2] = 0;
        inst->c_ptr = 0;
        return;
    }

    switch (inst->pages[*inst->page_num].fmt[inst->f_ptr]) {
    case '\\': // escape chars
        c1 = inst->pages[*inst->page_num].fmt[++inst->f_ptr];
        switch (c1) {
        case 'n':
        case 'N':
            inst->f_ptr++;
            inst->buff[0] = 0x18; // erase to end
            inst->buff[1] = 0x0A; // CR
            inst->buff[2] = 0x0D; // LF
            inst->buff[3] = 0;
            break;
        case 't':
        case 'T':
            inst->f_ptr++;
            inst->buff[0] = ' ';
            inst->buff[1] = ' ';
            inst->buff[2] = ' ';
            inst->buff[3] = ' ';
            inst->buff[4] = 0;
            break;
        case '\\':
            inst->f_ptr++;
            inst->buff[0] = '\\';
            inst->buff[1] = 0;
            break;
        default: // check for hex
            c2 = inst->pages[*inst->page_num].fmt[++inst->f_ptr];
            inst->f_ptr++;
            if (c1 > '9') c1 &= 0xDF; // upper case
            if (c2 > '9') c2 &= 0xDF;
            if (strchr(digits, c1) && strchr(digits, c2)) {
                inst->buff[0] = (16 * (strchr(digits, c1) - digits)
                                     + (strchr(digits, c2) - digits));
                inst->buff[1] = 0;
            } else {
                inst->buff[0] = 0;
            }
        }
        *inst->out = inst->buff[0];
        inst->c_ptr = 1;
        return;
    case '%':
        retval = parse_fmt(inst->pages[*inst->page_num].fmt,
                           &inst->f_ptr,
                           inst->buff,
                           inst->pages[*inst->page_num].args[inst->a_ptr++],
                           (char)inst->dp);
        if (retval >= 0) {
            *inst->out = inst->buff[0];
            inst->c_ptr = 1;
            inst->f_ptr++;
            return;
        }
        /* fall through */
    default:
        *inst->out = inst->pages[*inst->page_num].fmt[inst->f_ptr++];
    }
}

// ---------------------------------------------------------------------------
// Format parser — converts HAL values to LCD character sequences
// ---------------------------------------------------------------------------

static int parse_fmt(char *in, int *ptr, char *out, void *val, char dp) {
    /*parse val into the format in in pointed to by ptr.
     if out is null, then simply return the type of the format
     as u, s or f. a return value of -1 indicates an invalid pointer
     on exit ptr will point after the end of the format*/

    const double pow10[] = {1,10,1e2,1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11,
        1e12,1e13,1e14,1e15,1e16,1e17,1e18,1e19,1e20};

    char fill = ' ';

    int d = 0; // dot found
    char sgn = ' ';
    int c = -1, m = -1; // Length and Mantissa sig. figs
    int base = 10;
    if (in[*ptr] != '%') return -EINVAL;
    (*ptr)++;
    while (in[*ptr]) {
        switch (in[*ptr]) {
        case '%':
            if (out != NULL) out[0] = '%';
            return 't';
        case '.':
            if (d) return -EINVAL;
            d = 1;
            break;
        case ' ':
            fill = ' ';
            break;
        case '+':
            sgn = '+';
            break;
        case '0':
            if (c < 0 && d == 0) {
                fill = '0';
                break;
            }
            /* fall through */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (d) {
                if (m > 0) {
                    m = m * 10 + (in[*ptr] - '0');
                } else {
                    m = (in[*ptr] - '0');
                }
            } else {
                if (c > 0) {
                    c = c * 10 + (in[*ptr] - '0');
                } else {
                    c = (in[*ptr] - '0');
                }
            }
            if (c > MAX_ENTRY) c = MAX_ENTRY;
            break;

        case 'x':
        case 'X':
            base = 16;
            /* fall through */
        case 'o':
            if (base == 10) base = 8;
            /* fall through */
        case 'u':
            if (out == NULL || val == NULL) return 'u';
            {
                unsigned int tmp;
                int i;
                unsigned int v = *((hal_u32_t *)val);

                if (c < 1) c = num_digits_baseN(v, base);

                tmp = v;
                for (i = c - 1; i >= 0; i--) {
                    if (tmp != 0 || i == (c - 1)) {
                        out[i] = digits[tmp % base];
                    } else {
                        out[i] = fill;
                    }
                    tmp /= base;
                }
                if (tmp != 0) { // it didn't fit
                    for (i = 0; i < c; i++) { out[i] = '*'; }
                }
                out[c] = 0;
            }
            return 'u';

        case 'd':
        case 'i':
            if (out == NULL || val == NULL) return 's';
            {
                int tmp, s = 0;
                int i;
                int v = *((hal_s32_t *)val);

                if (sgn == '+') s = 1;
                if (v < 0) { s = 1; sgn = '-'; v = -v; }

                if (c < 1) c = num_digits_baseN(v, base) + s;

                tmp = abs(v);
                for (i = c - 1; i >= s; i--) {
                    if (tmp != 0 || i == c - 1) {
                        out[i] = digits[tmp % 10];
                        tmp /= 10;
                    } else {
                        if (s && fill == ' ') {
                            out[i] = sgn;
                            s = 0;
                        } else {
                            out[i] = fill;
                        }
                    }
                }
                if (tmp != 0) { // it didn't fit
                    for (i = 0; i < c; i++) { out[i] = '*'; }
                }
                if (s) out[0] = sgn;
                out[c] = 0;
                return 's';
            }

        case 'f':
        case 'F':
            if (out == NULL || val == NULL) return 'f';
            {
                int i;
                double v = *((hal_float_t *)val);
                uint64_t tmp = 0;
                int s = 0;

                if (sgn != ' ') s = 1;
                if (v < 0) { s = 1; sgn = '-'; v = -v; }

                if (m < 0) m = 4;
                if (c < 1) c = num_digits_baseN(v, base) + m + 1 + s;

                if (c > MAX_ENTRY) { // then it won't fit
                    tmp = 2;
                    goto overflow;
                }

                tmp = v * pow10[m] + 0.5;

                for (i = c - 1; i > c - m - 1; i--) {
                    out[i] = digits[do_div(tmp, 10)];
                }

                if (m > 0) {
                    out[i] = dp;
                } else {
                    m = -1; // shuffle down into decimal point
                }

                for (i = c - m - 2; i >= s; i--) {
                    if (tmp > 0 || i == c - m - 2) {
                        out[i] = digits[do_div(tmp, 10)];
                    } else {
                        if (sgn != ' ' && fill == ' ') {
                            s = 0;
                            out[i] = sgn;
                            sgn = ' ';
                        } else {
                            out[i] = fill;
                        }
                    }
                }

                if (s) {
                    out[0] = sgn;
                }

            overflow:
                if (tmp > 0) { // it didn't fit
                    for (i = 1; i < c; i++) { out[i] = '*'; }
                }
                out[c] = 0;
                return 'f';
            }

        case 'c':
            if (out == NULL || val == NULL) return 'c';
            {
                int i;
                unsigned char v = *((hal_u32_t *)val);

                if (c == 0) c = 1;
                for (i = 0; i < c; i++) {
                    out[i] = (v > ' ') ? v : ' ';
                }
                out[i] = 0;
                return 'c';
            }

        case 'b':
            if (out == NULL || val == NULL) return 'b';
            {
                char c1b, c2b;
                char bt = 'E', bf = 'E'; // hint that there is a format error
                int v = *((hal_bit_t *)val);
                if (in[++(*ptr)] == '\\') {
                    c1b = in[++(*ptr)];
                    if (c1b > '9') c1b &= 0xDF;
                    c2b = in[++(*ptr)];
                    if (c2b > '9') c2b &= 0xDF;
                    if (strchr(digits, c1b) && strchr(digits, c2b)) {
                        bt = (16 * (strchr(digits, c1b) - digits)
                              + (strchr(digits, c2b) - digits));
                    }
                } else {
                    bt = in[*ptr];
                }
                if (in[++(*ptr)] == '\\') {
                    c1b = in[++(*ptr)] & 0xDF;
                    c2b = in[++(*ptr)] & 0xDF;
                    if (strchr(digits, c1b) && strchr(digits, c2b)) {
                        bf = (16 * (strchr(digits, c1b) - digits)
                              + (strchr(digits, c2b) - digits));
                    }
                } else {
                    bf = in[*ptr];
                }
                out[0] = (v) ? bt : bf;
                out[1] = 0;
                return 'b';
            }
        }
        (*ptr)++;
    }
    return -EINVAL;
}
