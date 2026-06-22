/* classicladder_rt.c — RT ladder evaluation engine for gomc.
 *
 * Based on Classic Ladder Project by Marc Le Douarain (calc.c, module_hal.c).
 * Copyright (C) Marc Le Douarain <marc.le.douarain@free.fr> (LGPL 2.1+)
 * Adapted for the gomc cgo architecture — no shared memory, direct struct access.
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License v2.1+.
 */

#include "classicladder_rt.h"
#include <stdlib.h>
#include <string.h>

/* --- Variable access helpers --- */

static int read_var(classicladder_rt_t *rt, int type, int offset) {
    switch (type) {
    case CL_VAR_MEM_BIT:
        return rt->var_bits[offset];
    case CL_VAR_PHYS_INPUT:
        return rt->var_bits[rt->sizes.nbr_bits + offset];
    case CL_VAR_PHYS_OUTPUT:
        return rt->var_bits[rt->sizes.nbr_bits + rt->sizes.nbr_phys_inputs + offset];
    case CL_VAR_ERROR_BIT:
        return rt->var_bits[rt->sizes.nbr_bits + rt->sizes.nbr_phys_inputs +
                           rt->sizes.nbr_phys_outputs + CL_MAX_STEPS + offset];
    case CL_VAR_STEP_ACTIVITY:
        return rt->var_bits[rt->sizes.nbr_bits + rt->sizes.nbr_phys_inputs +
                           rt->sizes.nbr_phys_outputs + offset];
    case CL_VAR_TIMER_DONE:
        return rt->timers[offset].output_done;
    case CL_VAR_TIMER_RUNNING:
        return rt->timers[offset].output_running;
    case CL_VAR_TIMER_IEC_DONE:
        return rt->timers_iec[offset].output;
    case CL_VAR_MONOSTABLE_RUNNING:
        return rt->monostables[offset].output_running;
    case CL_VAR_COUNTER_DONE:
        return rt->counters[offset].output_done;
    case CL_VAR_COUNTER_EMPTY:
        return rt->counters[offset].output_empty;
    case CL_VAR_COUNTER_FULL:
        return rt->counters[offset].output_full;
    /* Word variables */
    case CL_VAR_MEM_WORD:
        return rt->var_words[offset];
    case CL_VAR_STEP_TIME:
        return rt->var_words[rt->sizes.nbr_words + offset];
    case CL_VAR_TIMER_PRESET:
        return rt->timers[offset].preset;
    case CL_VAR_TIMER_VALUE:
        return rt->timers[offset].value;
    case CL_VAR_MONOSTABLE_PRESET:
        return rt->monostables[offset].preset;
    case CL_VAR_MONOSTABLE_VALUE:
        return rt->monostables[offset].value;
    case CL_VAR_COUNTER_PRESET:
        return rt->counters[offset].preset;
    case CL_VAR_COUNTER_VALUE:
        return rt->counters[offset].value;
    case CL_VAR_TIMER_IEC_PRESET:
        return rt->timers_iec[offset].preset;
    case CL_VAR_TIMER_IEC_VALUE:
        return rt->timers_iec[offset].value;
    case CL_VAR_PHYS_WORD_INPUT:
        return rt->var_words[rt->sizes.nbr_words + offset];
    case CL_VAR_PHYS_WORD_OUTPUT:
        return rt->var_words[rt->sizes.nbr_words + rt->sizes.nbr_s32_in + offset];
    default:
        return 0;
    }
}

static void write_var(classicladder_rt_t *rt, int type, int offset, int value) {
    switch (type) {
    case CL_VAR_MEM_BIT:
        rt->var_bits[offset] = value ? 1 : 0;
        break;
    case CL_VAR_PHYS_INPUT:
        rt->var_bits[rt->sizes.nbr_bits + offset] = value ? 1 : 0;
        break;
    case CL_VAR_PHYS_OUTPUT:
        rt->var_bits[rt->sizes.nbr_bits + rt->sizes.nbr_phys_inputs + offset] = value ? 1 : 0;
        break;
    case CL_VAR_ERROR_BIT:
        rt->var_bits[rt->sizes.nbr_bits + rt->sizes.nbr_phys_inputs +
                    rt->sizes.nbr_phys_outputs + CL_MAX_STEPS + offset] = value ? 1 : 0;
        break;
    case CL_VAR_STEP_ACTIVITY:
        rt->var_bits[rt->sizes.nbr_bits + rt->sizes.nbr_phys_inputs +
                    rt->sizes.nbr_phys_outputs + offset] = value ? 1 : 0;
        break;
    case CL_VAR_MEM_WORD:
        rt->var_words[offset] = value;
        break;
    case CL_VAR_STEP_TIME:
        rt->var_words[rt->sizes.nbr_words + offset] = value;
        break;
    case CL_VAR_TIMER_PRESET:
        rt->timers[offset].preset = value;
        break;
    case CL_VAR_TIMER_VALUE:
        rt->timers[offset].value = value;
        break;
    case CL_VAR_MONOSTABLE_PRESET:
        rt->monostables[offset].preset = value;
        break;
    case CL_VAR_MONOSTABLE_VALUE:
        rt->monostables[offset].value = value;
        break;
    case CL_VAR_COUNTER_PRESET:
        rt->counters[offset].preset = value;
        break;
    case CL_VAR_COUNTER_VALUE:
        rt->counters[offset].value = value;
        break;
    case CL_VAR_TIMER_IEC_PRESET:
        rt->timers_iec[offset].preset = value;
        break;
    case CL_VAR_TIMER_IEC_VALUE:
        rt->timers_iec[offset].value = value;
        break;
    case CL_VAR_PHYS_WORD_INPUT:
        rt->var_words[rt->sizes.nbr_words + offset] = value;
        break;
    case CL_VAR_PHYS_WORD_OUTPUT:
        rt->var_words[rt->sizes.nbr_words + rt->sizes.nbr_s32_in + offset] = value;
        break;
    default:
        break;
    }
}

/* --- HAL I/O --- */

static void read_hal_inputs(classicladder_rt_t *rt) {
    for (int i = 0; i < rt->sizes.nbr_phys_inputs; i++) {
        if (rt->hal_inputs[i])
            write_var(rt, CL_VAR_PHYS_INPUT, i, *(rt->hal_inputs[i]));
    }
}

static void write_hal_outputs(classicladder_rt_t *rt) {
    for (int i = 0; i < rt->sizes.nbr_phys_outputs; i++) {
        if (rt->hal_outputs[i])
            *(rt->hal_outputs[i]) = read_var(rt, CL_VAR_PHYS_OUTPUT, i);
    }
}

static void read_hal_s32_inputs(classicladder_rt_t *rt) {
    for (int i = 0; i < rt->sizes.nbr_s32_in; i++) {
        if (rt->hal_s32_inputs[i])
            write_var(rt, CL_VAR_PHYS_WORD_INPUT, i, *(rt->hal_s32_inputs[i]));
    }
}

static void write_hal_s32_outputs(classicladder_rt_t *rt) {
    for (int i = 0; i < rt->sizes.nbr_s32_out; i++) {
        if (rt->hal_s32_outputs[i])
            *(rt->hal_s32_outputs[i]) = read_var(rt, CL_VAR_PHYS_WORD_OUTPUT, i);
    }
}

static void read_hal_float_inputs(classicladder_rt_t *rt) {
    for (int i = 0; i < rt->sizes.nbr_float_in; i++) {
        if (rt->hal_float_inputs[i])
            write_var(rt, CL_VAR_PHYS_FLOAT_INPUT, i, (int)(*(rt->hal_float_inputs[i])));
    }
}

static void write_hal_float_outputs(classicladder_rt_t *rt) {
    for (int i = 0; i < rt->sizes.nbr_float_out; i++) {
        if (rt->hal_float_outputs[i])
            *(rt->hal_float_outputs[i]) = (double)read_var(rt, CL_VAR_PHYS_FLOAT_OUTPUT, i);
    }
}

/* --- Timer evaluation --- */

static void eval_timers(classicladder_rt_t *rt, int ms_elapsed) {
    for (int i = 0; i < rt->sizes.nbr_timers; i++) {
        cl_timer_t *t = &rt->timers[i];
        if (t->input_enable) {
            if (t->input_control) {
                if (t->value < t->preset) {
                    t->value += ms_elapsed;
                    t->output_running = 1;
                } else {
                    t->output_done = 1;
                    t->output_running = 0;
                }
            } else {
                t->value = 0;
                t->output_done = 0;
                t->output_running = 0;
            }
        }
    }
}

/* --- Monostable evaluation --- */

static void eval_monostables(classicladder_rt_t *rt, int ms_elapsed) {
    for (int i = 0; i < rt->sizes.nbr_monostables; i++) {
        cl_monostable_t *m = &rt->monostables[i];
        if (m->input && !m->input_bak) {
            /* Rising edge — start */
            m->value = m->preset;
            m->output_running = 1;
        }
        m->input_bak = m->input;
        if (m->output_running) {
            m->value -= ms_elapsed;
            if (m->value <= 0) {
                m->value = 0;
                m->output_running = 0;
            }
        }
    }
}

/* --- IEC Timer evaluation --- */

static void eval_timers_iec(classicladder_rt_t *rt, int ms_elapsed) {
    for (int i = 0; i < rt->sizes.nbr_timers_iec; i++) {
        cl_timer_iec_t *t = &rt->timers_iec[i];
        int base_ms;
        switch (t->base) {
        case 0: base_ms = 60000; break; /* minutes */
        case 1: base_ms = 1000; break;  /* seconds */
        case 2: base_ms = 100; break;   /* 100ms */
        default: base_ms = 1000; break;
        }

        switch (t->timer_mode) {
        case CL_TIMER_IEC_TON: /* ON-delay */
            if (t->input) {
                if (!t->timer_started) {
                    t->timer_started = 1;
                    t->value = 0;
                    t->value_to_reach_one_base_unit = 0;
                }
                t->value_to_reach_one_base_unit += ms_elapsed;
                while (t->value_to_reach_one_base_unit >= base_ms) {
                    t->value_to_reach_one_base_unit -= base_ms;
                    t->value++;
                }
                if (t->value >= t->preset) {
                    t->output = 1;
                    t->value = t->preset;
                }
            } else {
                t->timer_started = 0;
                t->output = 0;
                t->value = 0;
                t->value_to_reach_one_base_unit = 0;
            }
            break;

        case CL_TIMER_IEC_TOF: /* OFF-delay */
            if (t->input) {
                t->output = 1;
                t->timer_started = 0;
                t->value = 0;
                t->value_to_reach_one_base_unit = 0;
            } else {
                if (!t->timer_started && t->output) {
                    t->timer_started = 1;
                    t->value = 0;
                    t->value_to_reach_one_base_unit = 0;
                }
                if (t->timer_started) {
                    t->value_to_reach_one_base_unit += ms_elapsed;
                    while (t->value_to_reach_one_base_unit >= base_ms) {
                        t->value_to_reach_one_base_unit -= base_ms;
                        t->value++;
                    }
                    if (t->value >= t->preset) {
                        t->output = 0;
                        t->timer_started = 0;
                        t->value = t->preset;
                    }
                }
            }
            break;

        case CL_TIMER_IEC_TP: /* Pulse */
            if (t->input && !t->input_bak) {
                t->timer_started = 1;
                t->output = 1;
                t->value = 0;
                t->value_to_reach_one_base_unit = 0;
            }
            if (t->timer_started) {
                t->value_to_reach_one_base_unit += ms_elapsed;
                while (t->value_to_reach_one_base_unit >= base_ms) {
                    t->value_to_reach_one_base_unit -= base_ms;
                    t->value++;
                }
                if (t->value >= t->preset) {
                    t->output = 0;
                    t->timer_started = 0;
                    t->value = t->preset;
                }
            }
            break;
        }
        t->input_bak = t->input;
    }
}

/* --- Counter evaluation --- */

static void eval_counters(classicladder_rt_t *rt) {
    for (int i = 0; i < rt->sizes.nbr_counters; i++) {
        cl_counter_t *c = &rt->counters[i];
        if (c->input_reset) {
            c->value = 0;
        }
        if (c->input_preset) {
            c->value = c->preset;
        }
        if (c->input_count_up && !c->input_count_up_bak) {
            c->value++;
        }
        c->input_count_up_bak = c->input_count_up;
        if (c->input_count_down && !c->input_count_down_bak) {
            c->value--;
        }
        c->input_count_down_bak = c->input_count_down;
        c->output_done = (c->value == c->preset) ? 1 : 0;
        c->output_empty = (c->value <= 0) ? 1 : 0;
        c->output_full = (c->value >= c->preset) ? 1 : 0;
        c->value_bak = c->value;
    }
}

/* --- Rung evaluation (core ladder logic) --- */

/* Evaluate a single rung. Power flows left-to-right through the grid.
 * Returns 1 if the rung modified any outputs. */
static void eval_rung(classicladder_rt_t *rt, cl_rung_t *rung) {
    char coils[CL_RUNG_WIDTH][CL_RUNG_HEIGHT];
    memset(coils, 0, sizeof(coils));

    /* First column always has power */
    for (int y = 0; y < CL_RUNG_HEIGHT; y++) {
        coils[0][y] = 1;
    }

    /* Propagate power left to right */
    for (int x = 0; x < CL_RUNG_WIDTH; x++) {
        for (int y = 0; y < CL_RUNG_HEIGHT; y++) {
            cl_element_t *ele = &rung->elements[x][y];

            /* Vertical connection: inherit power from above */
            if (ele->connected_with_top && y > 0) {
                coils[x][y] |= coils[x][y - 1];
            }

            if (!coils[x][y] && x > 0) {
                /* No power from left or top */
                continue;
            }

            int input_state = (x > 0) ? coils[x][y] : 1;
            int output_state = 0;

            switch (ele->type) {
            case CL_ELE_FREE:
                output_state = 0;
                break;
            case CL_ELE_CONNECTION:
                output_state = input_state;
                break;
            case CL_ELE_INPUT:
                output_state = input_state & read_var(rt, ele->var_type, ele->var_num);
                break;
            case CL_ELE_INPUT_NOT:
                output_state = input_state & !read_var(rt, ele->var_type, ele->var_num);
                break;
            case CL_ELE_RISING_INPUT:
                {
                    int cur = read_var(rt, ele->var_type, ele->var_num);
                    output_state = input_state & (cur && !ele->dynamic_var_bak);
                    ele->dynamic_var_bak = cur;
                }
                break;
            case CL_ELE_FALLING_INPUT:
                {
                    int cur = read_var(rt, ele->var_type, ele->var_num);
                    output_state = input_state & (!cur && ele->dynamic_var_bak);
                    ele->dynamic_var_bak = cur;
                }
                break;
            case CL_ELE_OUTPUT:
                write_var(rt, ele->var_type, ele->var_num, input_state);
                output_state = input_state;
                break;
            case CL_ELE_OUTPUT_NOT:
                write_var(rt, ele->var_type, ele->var_num, !input_state);
                output_state = input_state;
                break;
            case CL_ELE_OUTPUT_SET:
                if (input_state)
                    write_var(rt, ele->var_type, ele->var_num, 1);
                output_state = input_state;
                break;
            case CL_ELE_OUTPUT_RESET:
                if (input_state)
                    write_var(rt, ele->var_type, ele->var_num, 0);
                output_state = input_state;
                break;
            case CL_ELE_TIMER:
                rt->timers[ele->var_num].input_enable = input_state;
                rt->timers[ele->var_num].input_control = input_state;
                output_state = rt->timers[ele->var_num].output_done;
                break;
            case CL_ELE_MONOSTABLE:
                rt->monostables[ele->var_num].input = input_state;
                output_state = rt->monostables[ele->var_num].output_running;
                break;
            case CL_ELE_COUNTER:
                rt->counters[ele->var_num].input_count_up = input_state;
                output_state = rt->counters[ele->var_num].output_done;
                break;
            case CL_ELE_TIMER_IEC:
                rt->timers_iec[ele->var_num].input = input_state;
                output_state = rt->timers_iec[ele->var_num].output;
                break;
            case CL_ELE_COMPAR:
                output_state = cl_eval_compare(rt, ele->var_num) && input_state;
                break;
            case CL_ELE_OUTPUT_OPERATE:
                if (input_state)
                    cl_eval_operate(rt, ele->var_num);
                output_state = input_state;
                break;
            case CL_ELE_OUTPUT_JUMP:
            case CL_ELE_OUTPUT_CALL:
                /* Handled at section level */
                output_state = input_state;
                break;
            case CL_ELE_UNUSABLE:
                output_state = input_state;
                break;
            default:
                output_state = 0;
                break;
            }

            ele->dynamic_output = output_state;
            ele->dynamic_input = input_state;
            ele->dynamic_state = output_state;

            /* Propagate to next column */
            if (x + 1 < CL_RUNG_WIDTH) {
                coils[x + 1][y] |= output_state;
            }
        }
    }
}

/* Evaluate all ladder sections in order */
static void eval_all_sections(classicladder_rt_t *rt) {
    for (int s = 0; s < rt->sizes.nbr_sections; s++) {
        cl_section_t *sec = &rt->sections[s];
        if (!sec->used)
            continue;

        if (sec->language == CL_SECTION_LADDER) {
            int rung_idx = sec->first_rung;
            int safety = 0;
            while (rung_idx >= 0 && rung_idx < rt->sizes.nbr_rungs && safety < CL_MAX_RUNGS) {
                cl_rung_t *rung = &rt->rungs[rung_idx];
                if (rung->used) {
                    eval_rung(rt, rung);
                }
                if (rung_idx == sec->last_rung)
                    break;
                rung_idx = rung->next_rung;
                safety++;
            }
        } else if (sec->language == CL_SECTION_SEQUENTIAL) {
            cl_refresh_sequential_page(rt, sec->sequential_page);
        }
    }
}

/* --- Public API --- */

void classicladder_refresh(void *arg, long period) {
    classicladder_rt_t *rt = (classicladder_rt_t *)arg;
    static unsigned long leftover = 0;

    leftover += (unsigned long)period;
    unsigned long ms = leftover / 1000000;
    leftover %= 1000000;

    if (ms < 1)
        return;

    int state = atomic_load_explicit(&rt->state, memory_order_acquire);
    if (state != CL_STATE_RUN)
        return;

    /* Read hide_gui pin */
    if (rt->hal_hide_gui)
        atomic_store_explicit(&rt->hide_gui, *(rt->hal_hide_gui), memory_order_relaxed);

    unsigned long t0 = rtapi_get_time();

    /* Update periodic refresh period from actual thread timing */
    rt->periodic_refresh_ms = (int)ms;

    /* Read HAL inputs → internal variables */
    read_hal_inputs(rt);
    read_hal_s32_inputs(rt);
    read_hal_float_inputs(rt);

    /* Evaluate timers/monostables/counters */
    eval_timers(rt, (int)ms);
    eval_monostables(rt, (int)ms);
    eval_timers_iec(rt, (int)ms);
    eval_counters(rt);

    /* Evaluate all ladder sections */
    eval_all_sections(rt);

    /* Write internal variables → HAL outputs */
    write_hal_outputs(rt);
    write_hal_s32_outputs(rt);
    write_hal_float_outputs(rt);

    unsigned long t1 = rtapi_get_time();
    atomic_store_explicit(&rt->duration_of_last_scan_ns,
                         (int32_t)(t1 - t0), memory_order_relaxed);
}

classicladder_rt_t *classicladder_rt_alloc(const cl_sizes_t *sizes) {
    classicladder_rt_t *rt = (classicladder_rt_t *)calloc(1, sizeof(classicladder_rt_t));
    if (!rt)
        return NULL;
    rt->sizes = *sizes;
    atomic_store(&rt->state, CL_STATE_STOP);
    atomic_store(&rt->generation, 0);
    return rt;
}

void classicladder_rt_free(classicladder_rt_t *rt) {
    if (rt)
        free(rt);
}

void classicladder_rt_init_data(classicladder_rt_t *rt) {
    memset(rt->var_bits, 0, sizeof(rt->var_bits));
    memset(rt->var_words, 0, sizeof(rt->var_words));
    memset(rt->var_floats, 0, sizeof(rt->var_floats));
    memset(rt->timers, 0, sizeof(rt->timers));
    memset(rt->monostables, 0, sizeof(rt->monostables));
    memset(rt->counters, 0, sizeof(rt->counters));
    memset(rt->timers_iec, 0, sizeof(rt->timers_iec));
    /* Initialize SFC: set all steps/transitions to unused */
    for (int i = 0; i < CL_MAX_STEPS; i++) {
        rt->steps[i].num_page = -1;
        rt->steps[i].activated = 0;
        rt->steps[i].time_activated = 0;
    }
    for (int i = 0; i < CL_MAX_TRANSITIONS; i++) {
        rt->transitions[i].num_page = -1;
        for (int j = 0; j < CL_MAX_SWITCHS; j++) {
            rt->transitions[i].num_step_to_activ[j] = -1;
            rt->transitions[i].num_step_to_desactiv[j] = -1;
            rt->transitions[i].num_trans_linked_for_start[j] = -1;
            rt->transitions[i].num_trans_linked_for_end[j] = -1;
        }
    }
    for (int i = 0; i < CL_MAX_SEQ_COMMENTS; i++) {
        rt->seq_comments[i].num_page = -1;
    }
    /* Default timer presets */
    for (int i = 0; i < CL_MAX_TIMERS; i++) {
        rt->timers[i].base = 1; /* seconds */
    }
    for (int i = 0; i < CL_MAX_MONOSTABLES; i++) {
        rt->monostables[i].base = 1;
    }
    for (int i = 0; i < CL_MAX_TIMERS_IEC; i++) {
        rt->timers_iec[i].base = 1;
        rt->timers_iec[i].timer_mode = CL_TIMER_IEC_TON;
    }
}

void write_var_ext(classicladder_rt_t *rt, int type, int offset, int value) {
    write_var(rt, type, offset, value);
}

int read_var_ext(classicladder_rt_t *rt, int type, int offset) {
    return read_var(rt, type, offset);
}

/* --- Bytecode expression evaluator (RT-safe: no malloc, no string ops) --- */

static int ipow(int base, int exp) {
    if (exp < 0) return 0;
    int result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

static int eval_bytecode(classicladder_rt_t *rt, const cl_compiled_expr_t *ce) {
    int32_t stack[CL_EXPR_STACK_DEPTH];
    int sp = 0;
    int a, b;

    for (int i = 0; i < ce->len; i++) {
        const cl_instruction_t *ins = &ce->code[i];
        switch (ins->opcode) {
        case CL_OP_PUSH_CONST:
            if (sp >= CL_EXPR_STACK_DEPTH) return 0;
            stack[sp++] = ins->operand;
            break;
        case CL_OP_LOAD_VAR:
            if (sp >= CL_EXPR_STACK_DEPTH) return 0;
            stack[sp++] = read_var(rt, (ins->operand >> 16) & 0xFFFF,
                                   ins->operand & 0xFFFF);
            break;
        case CL_OP_LOAD_VAR_IDX:
            if (sp < 1) return 0;
            a = stack[--sp]; /* index value */
            if (sp >= CL_EXPR_STACK_DEPTH) return 0;
            stack[sp++] = read_var(rt, (ins->operand >> 16) & 0xFFFF,
                                   (ins->operand & 0xFFFF) + a);
            break;
        case CL_OP_STORE_VAR:
            if (sp < 1) return 0;
            a = stack[--sp];
            write_var(rt, (ins->operand >> 16) & 0xFFFF,
                      ins->operand & 0xFFFF, a);
            break;
        case CL_OP_STORE_VAR_IDX:
            if (sp < 2) return 0;
            a = stack[--sp]; /* value */
            b = stack[--sp]; /* index */
            write_var(rt, (ins->operand >> 16) & 0xFFFF,
                      (ins->operand & 0xFFFF) + b, a);
            break;
        case CL_OP_ADD:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] + stack[sp]; break;
        case CL_OP_SUB:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] - stack[sp]; break;
        case CL_OP_MUL:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] * stack[sp]; break;
        case CL_OP_DIV:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp] ? stack[sp-1] / stack[sp] : 0; break;
        case CL_OP_MOD:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp] ? stack[sp-1] % stack[sp] : 0; break;
        case CL_OP_POW:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = ipow(stack[sp-1], stack[sp]); break;
        case CL_OP_AND:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] & stack[sp]; break;
        case CL_OP_OR:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] | stack[sp]; break;
        case CL_OP_XOR:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] ^ stack[sp]; break;
        case CL_OP_NOT:
            if (sp < 1) return 0;
            stack[sp-1] = !stack[sp-1]; break;
        case CL_OP_NEG:
            if (sp < 1) return 0;
            stack[sp-1] = -stack[sp-1]; break;
        case CL_OP_CMP_LT:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] < stack[sp]; break;
        case CL_OP_CMP_GT:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] > stack[sp]; break;
        case CL_OP_CMP_EQ:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] == stack[sp]; break;
        case CL_OP_CMP_LE:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] <= stack[sp]; break;
        case CL_OP_CMP_GE:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] >= stack[sp]; break;
        case CL_OP_CMP_NE:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] != stack[sp]; break;
        case CL_OP_ABS:
            if (sp < 1) return 0;
            stack[sp-1] = stack[sp-1] < 0 ? -stack[sp-1] : stack[sp-1]; break;
        case CL_OP_MINI:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] < stack[sp] ? stack[sp-1] : stack[sp]; break;
        case CL_OP_MAXI:
            if (sp < 2) return 0;
            sp--; stack[sp-1] = stack[sp-1] > stack[sp] ? stack[sp-1] : stack[sp]; break;
        default:
            return 0;
        }
    }
    return sp > 0 ? stack[0] : 0;
}

int cl_eval_compare(classicladder_rt_t *rt, int expr_index) {
    if (expr_index < 0 || expr_index >= CL_MAX_ARITHM_EXPR)
        return 0;
    const cl_compiled_expr_t *ce = &rt->compiled_exprs[expr_index];
    if (!ce->valid || ce->len == 0)
        return 0;
    return eval_bytecode(rt, ce) ? 1 : 0;
}

void cl_eval_operate(classicladder_rt_t *rt, int expr_index) {
    if (expr_index < 0 || expr_index >= CL_MAX_ARITHM_EXPR)
        return;
    const cl_compiled_expr_t *ce = &rt->compiled_exprs[expr_index];
    if (!ce->valid || ce->len == 0)
        return;
    eval_bytecode(rt, ce);
}

/* --- Sequential Function Chart (SFC/Grafcet) evaluation --- */

void cl_prepare_sequential(classicladder_rt_t *rt) {
    for (int i = 0; i < CL_MAX_STEPS; i++) {
        rt->steps[i].activated = 0;
        rt->steps[i].time_activated = 0;
        if (rt->steps[i].init_step && rt->steps[i].num_page >= 0)
            rt->steps[i].activated = 1;
    }
    for (int i = 0; i < CL_MAX_TRANSITIONS; i++) {
        rt->transitions[i].activated = 0;
    }
}

static int refresh_transition(classicladder_rt_t *rt, cl_transition_t *trans) {
    /* Read transition condition */
    trans->activated = read_var(rt, trans->var_type_condi, trans->var_num_condi);
    if (!trans->activated)
        return 0;

    /* Check all steps to deactivate are currently active */
    int all_on = 1;
    for (int i = 0; i < CL_MAX_SWITCHS && trans->num_step_to_desactiv[i] != -1; i++) {
        int step_idx = trans->num_step_to_desactiv[i];
        if (step_idx >= 0 && step_idx < CL_MAX_STEPS) {
            if (!rt->steps[step_idx].activated) {
                all_on = 0;
                break;
            }
        }
    }

    if (!all_on)
        return 0;

    /* Transition fires: deactivate upstream steps */
    for (int i = 0; i < CL_MAX_SWITCHS && trans->num_step_to_desactiv[i] != -1; i++) {
        int step_idx = trans->num_step_to_desactiv[i];
        if (step_idx >= 0 && step_idx < CL_MAX_STEPS)
            rt->steps[step_idx].activated = 0;
    }

    /* Activate downstream steps */
    for (int i = 0; i < CL_MAX_SWITCHS && trans->num_step_to_activ[i] != -1; i++) {
        int step_idx = trans->num_step_to_activ[i];
        if (step_idx >= 0 && step_idx < CL_MAX_STEPS)
            rt->steps[step_idx].activated = 1;
    }

    return 1; /* state changed */
}

static void refresh_steps_vars(classicladder_rt_t *rt) {
    for (int i = 0; i < CL_MAX_STEPS; i++) {
        cl_step_t *step = &rt->steps[i];
        if (step->num_page < 0)
            continue;
        if (step->activated)
            step->time_activated += rt->periodic_refresh_ms;
        else
            step->time_activated = 0;
        write_var(rt, CL_VAR_STEP_ACTIVITY, step->step_number, step->activated);
        write_var(rt, CL_VAR_STEP_TIME, step->step_number, step->time_activated / 1000);
    }
}

void cl_refresh_sequential_page(classicladder_rt_t *rt, int page_nbr) {
    int state_changed;
    int loop_safety = 0;
    do {
        state_changed = 0;
        for (int i = 0; i < CL_MAX_TRANSITIONS; i++) {
            cl_transition_t *trans = &rt->transitions[i];
            if (trans->num_page != page_nbr)
                continue;
            if (refresh_transition(rt, trans))
                state_changed = 1;
        }
        loop_safety++;
    } while (state_changed && loop_safety < 50);
    refresh_steps_vars(rt);
}
