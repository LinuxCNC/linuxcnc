//    Copyright (C) 2013 Andy Pugh

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

// A generic/configurable multiplexer component

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>


/* module information */
MODULE_AUTHOR("Andy Pugh");
MODULE_DESCRIPTION("Generic mux component for LinuxCNC");
MODULE_LICENSE("GPL");

#define MAX_CHAN 100
#define MAX_SIZE 1024
#define EPS 2e-7

typedef struct {
    hal_refs_u *inputs;
    hal_refs_u output;
    hal_uint_t sel_int;
    hal_bool_t *sel_bit;
    hal_uint_t selection;
    hal_uint_t debounce;
    hal_uint_t timer;
    hal_bool_t suppress;
    int in_type;
    int out_type;
    int size;
    int num_bits;
} mux_inst_t;

typedef struct {
    mux_inst_t *insts;
    int num_insts;
} mux_t;

static int comp_id;
static mux_t *mux;
static void write_fp(void *arg, long period);

char *config[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(config, MAX_CHAN, "mux specifiers inNUMout");

int rtapi_app_main(void){
    int retval;
    int i, f;
    char hal_name[HAL_NAME_LEN];
    char *types[5] = {"invalid", "bit", "float", "s32", "u32"};
    if (!config[0]) {
        rtapi_print_msg(RTAPI_MSG_ERR, "The mux_generic component requires at least"
                " one valid format string\n");
        return -EINVAL;
    }

    comp_id = hal_init("mux_generic");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "mux_generic: ERROR: hal_init() failed\n");
        return -1;
    }

    // allocate shared memory for the base struct
    mux = hal_malloc(sizeof(*mux));
    if (mux == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "mux_generic component: Out of Memory\n");
        hal_exit(comp_id);
        return -1;
    }

    // Count the instances.
    for (mux->num_insts = 0; config[mux->num_insts];mux->num_insts++) {}
    mux->insts = hal_malloc(mux->num_insts * sizeof(*mux->insts));
    // Parse the config string
    for (i = 0; i < mux->num_insts; i++) {
        char c;
        int s, p = 0;
        mux_inst_t *inst = &mux->insts[i];
        inst->in_type = -1;
        inst->out_type = -1;
        for (f = 0; (c = config[i][f]); f++) {
            int type;
            type = 0;
            switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                inst->size = (inst->size * 10) + (c - '0');
                if (inst->size > MAX_SIZE) inst->size = MAX_SIZE;
                break;
            case 'b':
            case 'B':
                type = HAL_BIT;
                break;
            case 'f':
            case 'F':
                type = HAL_FLOAT;
                break;
            case 's':
            case 'S':
                type = HAL_S32;
                break;
            case 'u':
            case 'U':
                type = HAL_U32;
                break;
            default:
                rtapi_print_msg(RTAPI_MSG_ERR, "mux_generic: invalid character in "
                        "fmt string\n");
                goto fail0;
            }
            if (type) {
                if (inst->in_type == -1) {
                    inst->in_type = type;
                }
                else if (inst->out_type == -1) {
                    inst->out_type = type;
                }
                else
                {
                    rtapi_print_msg(RTAPI_MSG_ERR, "mux_generic: too many type "
                            "specifiers in fmt string\n");
                    goto fail0;
                }
            }
        }
        if (inst->size < 1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "mux_generic: No entry count given\n");
            goto fail0;
        }
        else if (inst->size < 2) {
            rtapi_print_msg(RTAPI_MSG_ERR, "mux_generic: A one-element mux makes "
                    "no sense\n");
            goto fail0;
        }
        if (inst->in_type == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "mux_generic: No type specifiers in "
                    "fmt string\n");
            goto fail0;
        }
        else if (inst->out_type == -1) {
            inst->out_type = inst->in_type;
        }

        // We did away with the no-fp thread.
        // Always export the float case. It can do any-to-any.
        retval = hal_export_functf(write_fp, inst, 1, 0, comp_id, "mux-gen.%02i", i);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "mux_generic: ERROR: function export"
                    " failed\n");
            goto fail0;
        }

        // Input pins

        // if the mux size is a power of 2 then create the bit inputs
        s = inst->size;
        for(inst->num_bits = 1; (!((s >>= 1) & 1)); inst->num_bits++);
        if (s !=1){
            inst->num_bits = 0;
        } else { //make the bit pins
            inst->sel_bit = hal_malloc(inst->num_bits * sizeof(*inst->sel_bit));
            for (p = 0; p < inst->num_bits; p++) {
                retval = hal_pin_new_bool(comp_id, HAL_IN, &inst->sel_bit[p], 0,
                        "mux-gen.%02i.sel-bit-%02i", i, p);
                if (retval != 0) {
                    goto fail0;
                }
            }
        }

        retval = hal_pin_new_ui32(comp_id, HAL_IN, &(inst->sel_int), 0,
                "mux-gen.%02i.sel-int", i);
        if (retval != 0) {
            goto fail0;
        }

        inst->inputs = hal_malloc(inst->size * sizeof(*inst->inputs));
        for (p = 0; p < inst->size; p++) {
            retval = rtapi_snprintf(hal_name, HAL_NAME_LEN,
                    "mux-gen.%02i.in-%s-%02i", i, types[inst->in_type], p);
            if (retval >= HAL_NAME_LEN) {
                goto fail0;
            }
            switch(inst->in_type) {
            case HAL_BOOL: retval = hal_pin_new_bool(comp_id, HAL_IN, &inst->inputs[p].b, 0, "%s", hal_name); break;
            case HAL_REAL: retval = hal_pin_new_real(comp_id, HAL_IN, &inst->inputs[p].r, 0, "%s", hal_name); break;
            case HAL_S32:  retval = hal_pin_new_si32(comp_id, HAL_IN, &inst->inputs[p].s, 0, "%s", hal_name); break;
            case HAL_U32:  retval = hal_pin_new_ui32(comp_id, HAL_IN, &inst->inputs[p].u, 0, "%s", hal_name); break;
            // FIXME: Future...when we switch types
            case HAL_SINT: retval = hal_pin_new_sint(comp_id, HAL_IN, &inst->inputs[p].s, 0, "%s", hal_name); break;
            case HAL_UINT: retval = hal_pin_new_uint(comp_id, HAL_IN, &inst->inputs[p].u, 0, "%s", hal_name); break;
            default: retval = -ENOENT; break;
            }
            if (retval != 0) {
                goto fail0;
            }
        }

        // Behaviour-modifiers
        retval = hal_pin_new_bool(comp_id, HAL_IN, &inst->suppress, 0,
                "mux-gen.%02i.suppress-no-input", i);
        if (retval != 0) {
            goto fail0;
        }
        retval = hal_pin_new_ui32(comp_id, HAL_IN, &inst->debounce, 0,
                "mux-gen.%02i.debounce-us", i);
        if (retval != 0) {
            goto fail0;
        }
        retval = hal_param_new_ui32(comp_id, HAL_RO, &inst->timer, 0,
                "mux-gen.%02i.elapsed", i);
        if (retval != 0) {
            goto fail0;
        }
        retval = hal_param_new_ui32(comp_id, HAL_RO, &inst->selection, 0,
                "mux-gen.%02i.selected", i);
        if (retval != 0) {
            goto fail0;
        }

        //output pins
        retval = rtapi_snprintf(hal_name, HAL_NAME_LEN,
                "mux-gen.%02i.out-%s", i, types[inst->out_type]);
        if (retval >= HAL_NAME_LEN) {
            goto fail0;
        }
        switch(inst->out_type) {
        case HAL_BOOL: retval = hal_pin_new_bool(comp_id, HAL_OUT, &inst->output.b, 0, "%s", hal_name); break;
        case HAL_REAL: retval = hal_pin_new_real(comp_id, HAL_OUT, &inst->output.r, 0, "%s", hal_name); break;
        case HAL_S32:  retval = hal_pin_new_si32(comp_id, HAL_OUT, &inst->output.s, 0, "%s", hal_name); break;
        case HAL_U32:  retval = hal_pin_new_ui32(comp_id, HAL_OUT, &inst->output.u, 0, "%s", hal_name); break;
        // FIXME: Future...when we switch types
        case HAL_SINT: retval = hal_pin_new_sint(comp_id, HAL_OUT, &inst->output.s, 0, "%s", hal_name); break;
        case HAL_UINT: retval = hal_pin_new_uint(comp_id, HAL_OUT, &inst->output.u, 0, "%s", hal_name); break;
        default: retval = -ENOENT; break;
        }
        if (retval != 0) {
            goto fail0;
        }

    }

    hal_ready(comp_id);
    return 0;

    fail0:
    hal_exit(comp_id);
    return -1;

}

// From 'f' to 't' conversion
#define FT(f,t) ((((f) & 0x0f) << 4) + ((t) & 0x0f))

void write_fp(void *arg, long period) {
    mux_inst_t *inst = arg;
    int i = 0;
    unsigned s = 0;
    if (inst->num_bits > 0) {
        while (i < inst->num_bits) {
            s += (hal_get_bool(inst->sel_bit[i]) != 0) << i;
            i++;
        }
    }
    // if you document it, it's not a bug, it's a feature. Might even be useful
    s += hal_get_ui32(inst->sel_int);

    if (hal_get_bool(inst->suppress) && s == 0)
        return;
    if (s != hal_get_ui32(inst->selection) && hal_get_ui32(inst->timer) < hal_get_ui32(inst->debounce)) {
        hal_set_ui32(inst->timer, hal_get_ui32(inst->timer) + period / 1000);
        return;
    }

    hal_set_ui32(inst->selection, s);
    hal_set_ui32(inst->timer, 0);

    if ((int)s >= inst->size)
        s = inst->size - 1;

    switch (FT(inst->in_type, inst->out_type)) {
    case FT(HAL_BOOL, HAL_BOOL): //HAL_BIT => HAL_BIT
        hal_set_bool(inst->output.b, hal_get_bool(inst->inputs[s].b));
        break;
    case FT(HAL_BOOL, HAL_S32): //HAL_BIT => HAL_S32
        hal_set_si32(inst->output.s, hal_get_bool(inst->inputs[s].b));
        break;
    case FT(HAL_BOOL, HAL_U32): //HAL_BIT => HAL_U32
        hal_set_ui32(inst->output.u, hal_get_bool(inst->inputs[s].b));
        break;
    case FT(HAL_BOOL, HAL_REAL): //HAL_BIT => HAL_FLOAT
        hal_set_real(inst->output.r, hal_get_bool(inst->inputs[s].b) ? 1.0 : 0.0);
        break;

    case FT(HAL_REAL, HAL_BOOL): //HAL_FLOAT => HAL_BIT
        hal_set_bool(inst->output.b, fabs(hal_get_real(inst->inputs[s].r)) > EPS);
        break;
    case FT(HAL_REAL, HAL_REAL): //HAL_FLOAT => HAL_FLOAT
        hal_set_real(inst->output.r, hal_get_real(inst->inputs[s].r));
        break;
    case FT(HAL_REAL, HAL_S32): { //HAL_FLOAT => HAL_S32
        rtapi_real v = hal_get_real(inst->inputs[s].r);
        if (v > RTAPI_INT32_MAX) {
            hal_set_si32(inst->output.s, RTAPI_INT32_MAX);
        } else if (v < RTAPI_INT32_MIN) {
            hal_set_si32(inst->output.s, RTAPI_INT32_MIN);
        } else {
            hal_set_si32(inst->output.s, v);
        }
        break; }
    case FT(HAL_REAL, HAL_U32): { //HAL_FLOAT => HAL_U32
        rtapi_real v = hal_get_real(inst->inputs[s].r);
        if (v > RTAPI_UINT32_MAX) {
            hal_set_ui32(inst->output.u, RTAPI_UINT32_MAX);
        } else if (v < 0) {
            hal_set_ui32(inst->output.u, 0);
        } else {
            hal_set_ui32(inst->output.u, v);
        }
        break; }

    case FT(HAL_S32, HAL_BOOL): //HAL_S32 => HAL_BIT
        hal_set_bool(inst->output.b, hal_get_si32(inst->inputs[s].s) != 0);
        break;
    case FT(HAL_S32, HAL_S32): //HAL_S32 => HAL_S32
        hal_set_si32(inst->output.s, hal_get_si32(inst->inputs[s].s));
        break;
    case FT(HAL_S32, HAL_U32): { //HAL_S32 => HAL_U32
        rtapi_s32 v = hal_get_si32(inst->inputs[s].s);
        hal_set_ui32(inst->output.u, v >= 0 ? v : 0);
        break; }
    case FT(HAL_S32, HAL_REAL): //HAL_S32 => HAL_FLOAT
        hal_set_real(inst->output.r, hal_get_si32(inst->inputs[s].s));
        break;

    case FT(HAL_U32, HAL_BOOL): //HAL_U32 => HAL_BIT
        hal_set_bool(inst->output.b, hal_get_ui32(inst->inputs[s].u) != 0);
        break;
    case FT(HAL_U32, HAL_S32): { //HAL_U32 => HAL_S32
        rtapi_u32 v = hal_get_ui32(inst->inputs[s].u);
        hal_set_si32(inst->output.s, v > RTAPI_INT32_MAX ? RTAPI_INT32_MAX : v);
        break; }
    case FT(HAL_U32, HAL_U32): //HAL_U32 => HAL_U32
        hal_set_ui32(inst->output.u, hal_get_ui32(inst->inputs[s].u));
        break;
    case FT(HAL_U32, HAL_REAL): //HAL_U32 => HAL_FLOAT
        hal_set_real(inst->output.r, hal_get_ui32(inst->inputs[s].u));
        break;
    }
}

void rtapi_app_exit(void){
//everything is hal_malloc-ed which saves a lot of cleanup here
hal_exit(comp_id);
}
