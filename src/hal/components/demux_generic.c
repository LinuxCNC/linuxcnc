//    Copyright (C) 2013 Andy Pugh, 2026 Hans Unzner

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

// A generic/configurable demultiplexer component

#include <rtapi.h>
#include <rtapi_app.h>
#include <hal.h>


/* module information */
MODULE_AUTHOR("Hans Unzner");
MODULE_DESCRIPTION("Generic demux component for LinuxCNC");
MODULE_LICENSE("GPL");

#define MAX_CHAN 100
#define MAX_SIZE 1024
#define EPS 2e-7
#define MAX_S32 0x7FFFFFFF
#define MAX_U32 0xFFFFFFFF

typedef struct {
    hal_data_u *input;
    hal_data_u **outputs;
    hal_u32_t *sel_int;
    hal_bit_t **sel_bit;
    unsigned int selection;
    hal_u32_t *debounce;
    unsigned int timer;
    hal_bit_t *suppress;
    int in_type;
    int out_type;
    int size;
    int num_bits;
} demux_inst_t;

typedef struct {
    demux_inst_t *insts;
    int num_insts;
} demux_t;

static int comp_id;
static demux_t *demux;
static void write_fp(void *arg, long period);
static void write_nofp(void *arg, long period);

char *config[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(config, MAX_CHAN, "demux specifiers inNUMout");

int rtapi_app_main(void){
    int retval;
    int i, f;
    char hal_name[HAL_NAME_LEN];
    char *types[5] = {"invalid", "bit", "float", "s32", "u32"};
    if (!config[0]) {
        rtapi_print_msg(RTAPI_MSG_ERR, "The demux_generic component requires at least"
            " one valid format string\n");
        return -EINVAL;
    }

    comp_id = hal_init("demux_generic");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: ERROR: hal_init() failed\n");
        return -1;
    }

    // allocate shared memory for the base struct
    demux = hal_malloc(sizeof(demux_t));
    if (demux == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "demux_generic component: Out of Memory\n");
        hal_exit(comp_id);
        return -1;
    }

    // Count the instances.
    for (demux->num_insts = 0; config[demux->num_insts];demux->num_insts++) {}
    demux->insts = hal_malloc(demux->num_insts * sizeof(demux_inst_t));
    // Parse the config string
    for (i = 0; i < demux->num_insts; i++) {
        char c;
        int s, p = 0;
        demux_inst_t *inst = &demux->insts[i];
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
                rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: invalid character in "
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
                    rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: too many type "
                            "specifiers in fmt string\n");
                    goto fail0;
                }
            }
        }
        if (inst->size < 1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: No entry count given\n");
            goto fail0;
        }
        else if (inst->size < 2) {
            rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: A one-element demux makes "
                    "no sense\n");
            goto fail0;
        }
        if (inst->in_type == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: No type specifiers in "
                    "fmt string\n");
            goto fail0;
        }
        else if (inst->out_type == -1) {
            inst->out_type = inst->in_type;
        }

        if (inst->in_type == HAL_FLOAT || inst->out_type == HAL_FLOAT) {
            retval = hal_export_functf(write_fp, inst, 1, 0, comp_id, "demux-gen.%02i", i);
            if (retval < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: ERROR: function export"
                        " failed\n");
                goto fail0;
            }
        }
        else
        {
            retval = hal_export_functf(write_nofp, inst, 0, 0, comp_id, "demux-gen.%02i", i);
            if (retval < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, "demux_generic: ERROR: function export"
                        " failed\n");
                goto fail0;
            }
        }
        // Input pins
        
        // if the demux size is a power of 2 then create the bit inputs
        s = inst->size;
        for(inst->num_bits = 1; (!((s >>= 1) & 1)); inst->num_bits++);
        if (s !=1){
            inst->num_bits = 0;
        } else { //make the bit pins
            inst->sel_bit = hal_malloc(inst->num_bits * sizeof(hal_bit_t*));
            for (p = 0; p < inst->num_bits; p++) {
                retval = hal_pin_bit_newf(HAL_IN, &inst->sel_bit[p], comp_id,
                    "demux-gen.%02i.sel-bit-%02i", i, p);
                if (retval != 0) {
                    goto fail0;
                }
            }
        }

        retval = hal_pin_u32_newf(HAL_IN, &(inst->sel_int), comp_id,
                "demux-gen.%02i.sel-int", i);
        if (retval != 0) {
            goto fail0;
        }

        retval = rtapi_snprintf(hal_name, HAL_NAME_LEN,
                "demux-gen.%02i.in-%s", i, types[inst->in_type]);
        if (retval >= HAL_NAME_LEN) {
            goto fail0;
        }
        retval = hal_pin_new(hal_name, inst->in_type, HAL_IN,
                (void**)&(inst->input), comp_id);
        if (retval != 0) {
            goto fail0;
        }

        // Behaviour-modifiers
        retval = hal_pin_bit_newf(HAL_IN, &inst->suppress, comp_id,
                "demux-gen.%02i.suppress-no-input", i);
        if (retval != 0) {
            goto fail0;
        }
        retval = hal_pin_u32_newf(HAL_IN, &inst->debounce, comp_id,
                "demux-gen.%02i.debounce-us", i);
        if (retval != 0) {
            goto fail0;
        }
        retval = hal_param_u32_newf(HAL_RO, &inst->timer, comp_id,
                "demux-gen.%02i.elapsed", i);
        if (retval != 0) {
            goto fail0;
        }
        retval = hal_param_u32_newf(HAL_RO, &inst->selection, comp_id,
                "demux-gen.%02i.selected", i);
        if (retval != 0) {
            goto fail0;
        }

        //output pins
        inst->outputs = hal_malloc(inst->size * sizeof(hal_data_u*));
        for (p = 0; p < inst->size; p++) {
            retval = rtapi_snprintf(hal_name, HAL_NAME_LEN,
                    "demux-gen.%02i.out-%s-%02i", i, types[inst->out_type], p);
            if (retval >= HAL_NAME_LEN) {
                goto fail0;
            }
            retval = hal_pin_new(hal_name, inst->out_type, HAL_OUT,
                    (void**)&(inst->outputs[p]), comp_id);
            if (retval != 0) {
                goto fail0;
            }
        }

    }

    hal_ready(comp_id);
    return 0;

    fail0:
    hal_exit(comp_id);
    return -1;

}

void write_fp(void *arg, long period) {
    demux_inst_t *inst = arg;
    int i = 0;
    unsigned s = 0;
    if (inst->num_bits > 0) {
        while (i < inst->num_bits) {
            s += (*inst->sel_bit[i] != 0) << i;
            i++;
        }
    }
    // if you document it, it's not a bug, it's a feature. Might even be useful
    s += *inst->sel_int;

    if (*inst->suppress && s == 0)
        return;
    if (s != inst->selection && inst->timer < *inst->debounce) {
        inst->timer += period / 1000;
        return;
    }

    inst->selection = s;
    inst->timer = 0;

    if ((int)s >= inst->size)
        s = inst->size - 1;

    switch (inst->in_type * 8 + inst->out_type) {
    case 012: //HAL_BIT => HAL_FLOAT
        inst->outputs[s]->f = inst->input->b ? 1.0 : 0.0; //
        break;
    case 021: //HAL_FLOAT => HAL_BIT
        inst->outputs[s]->b =
                (inst->input->f > EPS || inst->input->f < -EPS) ? 1 : 0;
        break;
    case 022: //HAL_FLOAT => HAL_FLOAT
        inst->outputs[s]->f = inst->input->f;
        break;
    case 023: //HAL_FLOAT => HAL_S32
        if (inst->input->f > MAX_S32) {
            inst->outputs[s]->s = MAX_S32;
        } else if (inst->input->f < -MAX_S32) {
            inst->outputs[s]->s = -MAX_S32;
        } else {
            inst->outputs[s]->s = inst->input->f;
        }
        break;
    case 024: //HAL_FLOAT => HAL_U32
        if (inst->input->f > MAX_U32) {
            inst->outputs[s]->u = MAX_U32;
        } else if (inst->input->f < 0) {
            inst->outputs[s]->u = 0;
        } else {
            inst->outputs[s]->u = inst->input->f;
        }
        break;
    case 032: //HAL_S32 => HAL_FLOAT
        inst->outputs[s]->f = inst->input->s;
        break;
    case 042: //HAL_U32 => HAL_FLOAT
        inst->outputs[s]->f = (unsigned int) inst->input->u;
        break;
    }
}

void write_nofp(void *arg, long period) {
    demux_inst_t *inst = arg;
    int i = 0;
    unsigned s = 0;
    if (inst->num_bits > 0) {
        while (i < inst->num_bits) {
            s += (*inst->sel_bit[i] != 0) << i;
            i++;
        }
    }

    s += *inst->sel_int;

    if (*inst->suppress && s == 0)
        return;
    if (s != inst->selection && inst->timer < *inst->debounce) {
        inst->timer += period / 1000;
        return;
    }

    inst->selection = s;
    inst->timer = 0;

    if ((int)s >= inst->size)
        s = inst->size - 1;
    switch (inst->in_type * 8 + inst->out_type) {
    case 011: //HAL_BIT => HAL_BIT
        inst->outputs[s]->b = inst->input->b;
        break;
    case 013: //HAL_BIT => HAL_S32
        inst->outputs[s]->s = inst->input->b;
        break;
    case 014: //HAL_BIT => HAL_U32
        inst->outputs[s]->u = inst->input->b;
        break;
    case 031: //HAL_S32 => HAL_BIT
        inst->outputs[s]->b = inst->input->s == 0 ? 0 : 1;
        break;
    case 033: //HAL_S32 => HAL_S32
        inst->outputs[s]->s = inst->input->s;
        break;
    case 034: //HAL_S32 => HAL_U32
        inst->outputs[s]->u = (inst->input->s > 0) ? inst->input->s : 0;
        break;
    case 041: //HAL_U32 => HAL_BIT
        inst->outputs[s]->b = inst->input->u == 0 ? 0 : 1;
        break;
    case 043: //HAL_U32 => HAL_S32
        inst->outputs[s]->s =
                ((unsigned int) inst->input->u > MAX_S32) ?
                        MAX_S32 : inst->input->u;
        break;
    case 044: //HAL_U32 => HAL_U32
        inst->outputs[s]->u = inst->input->u;
        break;
    }
}

void rtapi_app_exit(void){
//everything is hal_malloc-ed which saves a lot of cleanup here
hal_exit(comp_id);
}
