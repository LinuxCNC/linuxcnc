//    Copyright (C) 2013 Andy Pugh
//    Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port

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

/* A component to convert 7i73 bytecodes to bit pins */

#include "gomc_env.h"

#include <string.h>

#define MAX_CHAN 8

typedef struct {
    struct {
        gomc_hal_bit_t **key;
        gomc_hal_bit_t **rows;
        gomc_hal_bit_t **cols;
        gomc_hal_u32_t *keycode;
    } hal;
    struct {
        gomc_hal_u32_t rollover;
        gomc_hal_bit_t invert;
    } param;
    gomc_hal_u32_t ncols;
    gomc_hal_u32_t nrows;
    gomc_hal_u32_t *now;
    gomc_hal_u32_t *then;
    gomc_hal_bit_t invert;
    char name[GOMC_HAL_NAME_LEN + 1];
    struct input_dev *key_dev;
    gomc_hal_u32_t index;
    int keydown;
    int keyup;
    int rowshift;
    int row;
    int num_keys;
    gomc_hal_bit_t scan;
    gomc_hal_bit_t keystroke;
}kb_inst_t;

typedef struct {
    kb_inst_t *insts;
    int num_insts;
}kb_t;

typedef struct {
    cmod_t cmod;
    const cmod_env_t *env;
    int comp_id;
    kb_t *kb;
    char *config[MAX_CHAN];
    char *names[MAX_CHAN];
} matrix_kb_inst_t;

void keyup(kb_inst_t *inst){
    int r, c;
    int keycode = *inst->hal.keycode & ~(inst->keydown | inst->keyup);

    r = keycode >> inst->rowshift;
    c = keycode & ~(0xFFFFFFFF << inst->rowshift);
    
    if  (r < 0 
         || c < 0
         || (gomc_hal_u32_t)r >= inst->nrows 
         || (gomc_hal_u32_t)c >= inst->ncols
         || inst->hal.key[(gomc_hal_u32_t)r * inst->ncols + (gomc_hal_u32_t)c] == NULL){
        return;
    }
    
    if (inst->num_keys > 0) inst->num_keys--;
    
    *inst->hal.key[(gomc_hal_u32_t)r * inst->ncols + (gomc_hal_u32_t)c] = 0;
}
void keydown(kb_inst_t *inst){
    int r, c;
    int keycode = *inst->hal.keycode & ~(inst->keydown | inst->keyup);
    
    r = keycode >> inst->rowshift;
    c = keycode & ~(0xFFFFFFFF << inst->rowshift);
    
    if  (r < 0 
         || c < 0
         || (gomc_hal_u32_t)r >= inst->nrows 
         || (gomc_hal_u32_t)c >= inst->ncols
         || inst->hal.key[(gomc_hal_u32_t)r * inst->ncols + (gomc_hal_u32_t)c] == NULL){
        return;
    }
    
    if ((gomc_hal_u32_t)inst->num_keys >= inst->param.rollover) return;
    inst->num_keys++;
    
    *inst->hal.key[(gomc_hal_u32_t)r * inst->ncols + (gomc_hal_u32_t)c] = 1;
}

    void loop(void *arg, long period){
    int c;
    gomc_hal_u32_t scan = 0;
    kb_inst_t *inst = arg;
    (void)period;
    
    if (inst->scan){ //scanning request
        for (c = 0; (gomc_hal_u32_t)c < inst->ncols; c++){
            scan += ((*inst->hal.cols[c] != inst->param.invert) << c);
        }
        if (scan == inst->now[inst->row] && scan != inst->then[inst->row]){
            // debounced and changed
            for (c = 0; (gomc_hal_u32_t)c < inst->ncols; c++){
                int mask = 1 << c;
                if ((inst->then[inst->row] & mask) && !(scan & mask)){ //keyup
                    *inst->hal.keycode = inst->keyup 
                    + (inst->row << inst->rowshift) 
                    + c;
                    keyup(inst);
                }
                else if (!(inst->then[inst->row] & mask) && (scan & mask)){//keydown
                    *inst->hal.keycode = inst->keydown 
                    + (inst->row << inst->rowshift) 
                    + c;
                    
                    keydown(inst);
                }
            }
        }
        else {
            *inst->hal.keycode = 0x40;//nochange
        }
        
        inst->then[inst->row] = inst->now[inst->row];
        inst->now[inst->row] = scan;
        
        *inst->hal.rows[inst->row] = inst->param.invert;
        inst->row++;
        if ((gomc_hal_u32_t)inst->row >= inst->nrows) inst->row = 0;
        *inst->hal.rows[inst->row] = !inst->param.invert;
    }
    else
    {
        if (*inst->hal.keycode == 0x40) return;
        if ((*inst->hal.keycode & (gomc_hal_u32_t)inst->keydown) == (gomc_hal_u32_t)inst->keydown){
            keydown(inst);
        }
        else if ((*inst->hal.keycode & (gomc_hal_u32_t)inst->keydown) == (gomc_hal_u32_t)inst->keyup)
        {
            keyup(inst);
        }
    }
}


static void matrix_kb_parse_argv(matrix_kb_inst_t *inst, int argc, const char **argv) {
    int ci = 0, ni = 0;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "config=", 7) == 0 && ci < MAX_CHAN)
            inst->config[ci++] = (char *)argv[i] + 7;
        else if (strncmp(argv[i], "names=", 6) == 0 && ni < MAX_CHAN)
            inst->names[ni++] = (char *)argv[i] + 6;
    }
}

static void matrix_kb_destroy(cmod_t *self) {
    matrix_kb_inst_t *inst = (matrix_kb_inst_t *)self;
    const gomc_hal_t *hal = inst->env->hal;
    if (inst->comp_id > 0)
        hal->exit(hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    int i, j, n;
    int retval;
    matrix_kb_inst_t *inst;
    kb_t *kb;
    const gomc_hal_t *hal = env->hal;

    (void)name;

    inst = (matrix_kb_inst_t *)env->rtapi->calloc(env->rtapi->ctx,
                sizeof(matrix_kb_inst_t));
    if (!inst) return -1;

    inst->cmod.Destroy = matrix_kb_destroy;
    inst->env = env;

    matrix_kb_parse_argv(inst, argc, argv);

    inst->comp_id = env->hal->init(env->hal->ctx, "matrix_kb",
                                   env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) {
        gomc_log_errorf(inst->env->log, "matrix_kb", "matrix_kb: ERROR: hal_init() failed\n");
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }
    
    // allocate shared memory for data
    kb = env->hal->malloc(env->hal->ctx, sizeof(kb_t));
    if (kb == 0) {
        gomc_log_errorf(inst->env->log, "matrix_kb",
                        "matrix_kb component: Out of Memory\n");
        env->hal->exit(env->hal->ctx, inst->comp_id);
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }
    inst->kb = kb;
    
    // Count the instances.
    for (kb->num_insts = 0; inst->config[kb->num_insts];kb->num_insts++);
    // Count the names.
    for (n = 0; inst->names[n];n++);
    
    if (n && n != kb->num_insts){
        gomc_log_errorf(inst->env->log, "matrix_kb", "matrix_kb: Number of sizes and number"
                        " of names must match\n");
        env->hal->exit(env->hal->ctx, inst->comp_id);
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }
    
    kb->insts = env->hal->malloc(env->hal->ctx, kb->num_insts * sizeof(kb_inst_t));
    
    for (i = 0; i < kb->num_insts; i++){
        int a = 0;
        gomc_hal_u32_t c, r;
        kb_inst_t *kinst = &kb->insts[i];

        kinst->index = i;
        kinst->nrows = 0;
        kinst->ncols = 0;
        kinst->scan = 0;
        kinst->keystroke = 0;
        kinst->param.invert = 1;
        
        for(j = 0; inst->config[i][j] !=0; j++){
            int ch = (inst->config[i][j] | 0x20); //lower case
            if (ch == 'x'){
                kinst->nrows = a;
                a = 0;
            }
            else if (ch >= '0' && ch <= '9'){
                a = (a * 10) + (ch - '0');
            }
            else if (ch == 's'){
                kinst->scan = 1;
            }
        }
        kinst->ncols = a;
        
        if (kinst->ncols == 0 || kinst->nrows == 0){
            gomc_log_errorf(inst->env->log, "matrix_kb",
                            "matrix_kb: Invalid size format. should be NxN\n");
            env->hal->exit(env->hal->ctx, inst->comp_id);
            env->rtapi->free(env->rtapi->ctx, inst);
            return -1;
        }
        
        if (kinst->ncols > 32){
            gomc_log_errorf(inst->env->log, "matrix_kb",
                            "matrix_kb: maximum number of columns is 32. Sorry\n");
            env->hal->exit(env->hal->ctx, inst->comp_id);
            env->rtapi->free(env->rtapi->ctx, inst);
            return -1;
        }
        
        for (kinst->rowshift = 1; kinst->ncols > (1U << kinst->rowshift); kinst->rowshift++);
        for (kinst->keydown = 0xC0, kinst->keyup = 0x80
             ; (int)(kinst->nrows << kinst->rowshift) > kinst->keydown
             ; kinst->keydown <<= 1, kinst->keyup <<= 1);
        
        kinst->hal.key = (gomc_hal_bit_t **)env->hal->malloc(env->hal->ctx, kinst->nrows * kinst->ncols * sizeof(gomc_hal_bit_t*));
        kinst->now = env->hal->malloc(env->hal->ctx, kinst->nrows * sizeof(gomc_hal_u32_t));
        kinst->then = env->hal->malloc(env->hal->ctx, kinst->nrows * sizeof(gomc_hal_u32_t));
        kinst->row = 0;
        kinst->param.rollover = 2;
        
        
        if (inst->names[i]){
            snprintf(kinst->name, sizeof(kinst->name), "%s", inst->names[i]);
        }
        else
        {
            snprintf(kinst->name, sizeof(kinst->name), "matrix_kb.%i", i);
        }
        
        for (c = 0; c < kinst->ncols; c++){
            for (r = 0; r < kinst->nrows; r++){  
                retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT,
                                          &(kinst->hal.key[r * kinst->ncols + c]), 
                                          inst->comp_id,
                                          "%s.key.r%xc%x", 
                                          kinst->name, r, c);
                if (retval != 0) {
                    gomc_log_errorf(inst->env->log, "matrix_kb",
                                    "matrix_kb: Failed to create output pin\n");
                    env->hal->exit(env->hal->ctx, inst->comp_id);
                    env->rtapi->free(env->rtapi->ctx, inst);
                    return -1;
                }
            }
        }
        
        if (kinst->scan){ //internally generated scanning
            kinst->hal.rows = (gomc_hal_bit_t **)env->hal->malloc(env->hal->ctx, kinst->nrows * sizeof(gomc_hal_bit_t*));
            kinst->hal.cols = (gomc_hal_bit_t **)env->hal->malloc(env->hal->ctx, kinst->ncols * sizeof(gomc_hal_bit_t*));
            
            for (r = 0; r < kinst->nrows; r++){
                retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT,
                                          &(kinst->hal.rows[r]), inst->comp_id,
                                          "%s.row-%02i-out",kinst->name, r);
                if (retval != 0) {
                    gomc_log_errorf(inst->env->log, "matrix_kb",
                                    "matrix_kb: Failed to create output row pin\n");
                    env->hal->exit(env->hal->ctx, inst->comp_id);
                    env->rtapi->free(env->rtapi->ctx, inst);
                    return -1;
                }
            }
            for (c = 0; c < kinst->ncols; c++){
                retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN,
                                          &(kinst->hal.cols[c]), inst->comp_id,
                                          "%s.col-%02i-in",kinst->name, c);
                if (retval != 0) {
                    gomc_log_errorf(inst->env->log, "matrix_kb",
                                    "matrix_kb: Failed to create input col pin\n");
                    env->hal->exit(env->hal->ctx, inst->comp_id);
                    env->rtapi->free(env->rtapi->ctx, inst);
                    return -1;
                }
            }
                
            retval = gomc_hal_pin_u32_newf(hal, GOMC_HAL_OUT,
                                      &(kinst->hal.keycode), inst->comp_id,
                                      "%s.keycode",kinst->name);
            if (retval != 0) {
                gomc_log_errorf(inst->env->log, "matrix_kb",
                                "matrix_kb: Failed to create output pin\n");
                env->hal->exit(env->hal->ctx, inst->comp_id);
                env->rtapi->free(env->rtapi->ctx, inst);
                return -1;
            }
            
            retval = gomc_hal_param_bit_newf(hal, GOMC_HAL_RW,
                                      &(kinst->param.invert), inst->comp_id,
                                      "%s.negative-logic",kinst->name);
            if (retval != 0) {
                gomc_log_errorf(inst->env->log, "matrix_kb",
                                "matrix_kb: Failed to create output pin\n");
                env->hal->exit(env->hal->ctx, inst->comp_id);
                env->rtapi->free(env->rtapi->ctx, inst);
                return -1;
            }
            
            
            retval = gomc_hal_param_u32_newf(hal, GOMC_HAL_RW,
                                      &(kinst->param.rollover), inst->comp_id,
                                      "%s.key_rollover",kinst->name);
            if (retval != 0) {
                gomc_log_errorf(inst->env->log, "matrix_kb",
                                "matrix_kb: Failed to create rollover param\n");
                env->hal->exit(env->hal->ctx, inst->comp_id);
                env->rtapi->free(env->rtapi->ctx, inst);
                return -1;
            }
            
        }
        else // scanning by 7i73 or similar
        {
            retval = gomc_hal_pin_u32_newf(hal, GOMC_HAL_IN,
                                      &(kinst->hal.keycode), inst->comp_id,
                                      "%s.keycode",kinst->name);
            if (retval != 0) {
                gomc_log_errorf(inst->env->log, "matrix_kb",
                                "matrix_kb: Failed to create input pin\n");
                env->hal->exit(env->hal->ctx, inst->comp_id);
                env->rtapi->free(env->rtapi->ctx, inst);
                return -1;
            }
        }
        
        retval = hal->export_funct(hal->ctx, kinst->name, loop, kinst, 1, 0, inst->comp_id); //needs fp?
        if (retval < 0) {
            gomc_log_errorf(inst->env->log, "matrix_kb", "matrix_kb: ERROR: function export failed\n");
            env->hal->exit(env->hal->ctx, inst->comp_id);
            env->rtapi->free(env->rtapi->ctx, inst);
            return -1;
        }
    }
    env->hal->ready(env->hal->ctx, inst->comp_id);
    
    *out = &inst->cmod;
    return 0;
}
