//
//    Copyright (C) 2011 Andy Pugh
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
//
//    The code in this file is based on UFLBP.PAS by Peter C. Wallace.  

#include "gomc_env.h"
#include "hostmot2.h"
#include "sserial.h"
#include <stdlib.h>
#include <string.h>

// Local argv_split replacing argv_split.
static char **argv_split(const char *str, int *argcp) {
    int argc = 0;
    const char *p = str;
    while (*p) { while (*p && (*p == ' ' || *p == '\t')) p++; if (*p) { argc++; while (*p && *p != ' ' && *p != '\t') p++; } }
    char **argv = calloc(argc + 1, sizeof(char *));
    if (!argv) return NULL;
    if (argcp) *argcp = argc;
    p = str; int i = 0;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (*p) { const char *s = p; while (*p && *p != ' ' && *p != '\t') p++; argv[i] = strndup(s, p - s); if (!argv[i]) { for (int j=0; j<i; j++) free(argv[j]); free(argv); return NULL; } i++; }
    }
    argv[i] = NULL;
    return argv;
}

// ---------------------------------------------------------------------------
// Instance struct — all mutable state lives here (heap-allocated).
// ---------------------------------------------------------------------------

typedef struct {
    cmod_t               cmod;
    const cmod_env_t    *env;
    int                  comp_id;
    char                 cmd_buf[256];
    char               **cmd_list;
    hostmot2_t          *hm2;
    hm2_sserial_remote_t *remote;
} setsserial_inst_t;

static int waitfor(setsserial_inst_t *inst){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff;
    long long int starttime = inst->env->rtapi->get_time(inst->env->rtapi->ctx);
    do {
        inst->env->rtapi->delay(inst->env->rtapi->ctx, 50000);
        HM2READ(remote->command_reg_addr, buff);
        if (inst->env->rtapi->get_time(inst->env->rtapi->ctx) - starttime > 1000000000){
            gomc_log_errorf(inst->env->log, "setsserial", "Timeout waiting for CMD to clear\n");
            return -1;
        }
    } while (buff);
    
    return 0;
}

static int doit(setsserial_inst_t *inst){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = 0x1000 | (1 << remote->index);
    HM2WRITE(remote->command_reg_addr, buff);
    if (waitfor(inst) < 0) return -1;
    HM2READ(remote->data_reg_addr, buff);
    if (buff & (1 << remote->index)){
        gomc_log_errorf(inst->env->log, "setsserial", "Error flag set after CMD Clear %08x\n",
                        buff);
        return -1;
    }
    return 0;
}

static int stop_all(setsserial_inst_t *inst){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff=0x8FF;
    HM2WRITE(remote->command_reg_addr, buff);
    return waitfor(inst);
}

static int setup_start(setsserial_inst_t *inst){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff=0xF00 | 1 << remote->index;
    HM2WRITE(remote->command_reg_addr, buff);
    if (waitfor(inst) < 0) return -1;
    HM2READ(remote->data_reg_addr, buff); 
    gomc_log_infof(inst->env->log, "setsserial", "setup start: data_reg readback = %x\n", buff);
    if (buff & (1 << remote->index)){
        gomc_log_infof(inst->env->log, "setsserial", "Remote failed to start\n");
        return -1;
    }
    return 0;
}

static int nv_access(setsserial_inst_t *inst, uint32_t type){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = LBPNONVOL_flag + LBPWRITE;
    gomc_log_infof(inst->env->log, "setsserial", "buff = %x\n", buff);
    HM2WRITE(remote->reg_cs_addr, buff);
    HM2WRITE(remote->rw_addr[0], type);
    return doit(inst);
}

static int set_nvram_param(setsserial_inst_t *inst, uint32_t addr, uint32_t value){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff;
    
    if (stop_all(inst) < 0) goto fail0;
    if (setup_start(inst) < 0) goto fail0;
    if (nv_access(inst, LBPNONVOLEEPROM) < 0) goto fail0;
    
    // value to set
    HM2WRITE(remote->rw_addr[0], value);
    buff = WRITE_REM_WORD_CMD | addr; 
    HM2WRITE(remote->reg_cs_addr, buff);
    if (doit(inst) < 0) goto fail0;
    
    if (nv_access(inst, LBPNONVOLCLEAR) < 0) goto fail0;
    
    return 0;
fail0: // It's all gone wrong
    buff=0x800; //Stop
    HM2WRITE(remote->command_reg_addr, buff);
    gomc_log_errorf(inst->env->log, "setsserial",                     "Problem with Smart Serial parameter setting, see dmesg\n");
    return -1;
}


static int __attribute__((unused)) getlocal(setsserial_inst_t *inst, int addr, int bytes){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t val = 0;
    uint32_t buff;
    for (;bytes--;){
        buff = READ_LOCAL_CMD | (addr + bytes);
        HM2WRITE(remote->command_reg_addr, buff);
        waitfor(inst);
        HM2READ(remote->data_reg_addr, buff);
        val = (val << 8) | buff;
    }    return val;
}

static int __attribute__((unused)) setlocal(setsserial_inst_t *inst, int addr, int val, int bytes){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t b = 0;
    uint32_t buff;
    int i;
    for (i = 0; i < bytes; i++){
        b = val & 0xFF;
        val >>= 8;
        HM2WRITE(remote->data_reg_addr, b);
        buff = WRITE_LOCAL_CMD | (addr + i);
        HM2WRITE(remote->command_reg_addr, buff);
        if (waitfor(inst) < 0) return -1;
    }   
    return 0;
}

static void sslbp_write_lbp(setsserial_inst_t *inst, uint32_t cmd, uint32_t data){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = LBPWRITE + cmd;
    HM2WRITE(remote->reg_cs_addr, buff);
    HM2WRITE(remote->rw_addr[0], data);
    doit(inst);
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
}

static int sslbp_read_cookie(setsserial_inst_t *inst){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = READ_COOKIE_CMD;
    uint32_t res;
    HM2WRITE(remote->reg_cs_addr, buff);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_read_cookie, trying to abort\n");
        return -1;
    }
    HM2READ(remote->rw_addr[0], res);
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return res;
}

static uint8_t sslbp_read_byte(setsserial_inst_t *inst, uint32_t addr){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = READ_REM_BYTE_CMD + addr;
    uint32_t res;
    HM2WRITE(remote->reg_cs_addr, buff);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_read_byte, trying to abort\n");
        return -1;
    }
    HM2READ(remote->rw_addr[0], res);
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return (uint8_t)res;
}

static uint16_t __attribute__((unused)) sslbp_read_word(setsserial_inst_t *inst, uint32_t addr){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = READ_REM_WORD_CMD + addr;
    uint32_t res;
    HM2WRITE(remote->reg_cs_addr, buff);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_read_word, trying to abort\n");
        return -1;
    }
    HM2READ(remote->rw_addr[0], res);
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return (uint16_t)res;
}

static uint32_t __attribute__((unused)) sslbp_read_long(setsserial_inst_t *inst, uint32_t addr){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = READ_REM_LONG_CMD + addr;
    uint32_t res=0;
    HM2WRITE(remote->reg_cs_addr, buff);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_read_long, trying to abort\n");
        return -1;
    }
    HM2READ(remote->rw_addr[0], buff);
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return res;
}

static uint64_t __attribute__((unused)) sslbp_read_double(setsserial_inst_t *inst, uint32_t addr){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint64_t res;
    uint32_t buff = READ_REM_DOUBLE_CMD + addr;
    HM2WRITE(remote->reg_cs_addr, buff);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_read_double, trying to abort\n");
        return -1;
    }
    HM2READ(remote->rw_addr[1], buff);
    res = buff;
    res <<= 32;
    HM2READ(remote->rw_addr[0], buff);
    res += buff;
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return res;
}

static int sslbp_write_byte(setsserial_inst_t *inst, uint32_t addr, uint32_t data){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = WRITE_REM_BYTE_CMD + addr;
    HM2WRITE(remote->reg_cs_addr, buff);
    HM2WRITE(remote->rw_addr[0], data);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_write_byte, trying to abort\n");
        return -1;
    }
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return 0;
}

static int __attribute__((unused)) sslbp_write_word(setsserial_inst_t *inst, uint32_t addr, uint32_t data){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = WRITE_REM_WORD_CMD + addr;
    HM2WRITE(remote->reg_cs_addr, buff);
    HM2WRITE(remote->rw_addr[0], data);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_write_word, trying to abort\n");
        return -1;
    }
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return 0;
}

static int sslbp_write_long(setsserial_inst_t *inst, uint32_t addr, uint32_t data){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = WRITE_REM_LONG_CMD + addr;
    HM2WRITE(remote->reg_cs_addr, buff);
    HM2WRITE(remote->rw_addr[0], data);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_write_long, trying to abort\n");
        return -1;
    }
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return 0;
}

static int sslbp_write_double(setsserial_inst_t *inst, uint32_t addr, uint32_t data0, uint32_t data1){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    uint32_t buff = WRITE_REM_DOUBLE_CMD + addr;
    HM2WRITE(remote->reg_cs_addr, buff);
    HM2WRITE(remote->rw_addr[0], data0);
    HM2WRITE(remote->rw_addr[1], data1);
    if (doit(inst) < 0){
        HM2_ERR("Error in sslbp_write_double, trying to abort\n");
        return -1;
    }
    buff = 0;
    HM2WRITE(remote->reg_cs_addr, buff);
    return 0;
}

static void flash_start(setsserial_inst_t *inst){
    sslbp_write_lbp(inst, LBPNONVOL_flag, LBPNONVOLFLASH);
}

static void flash_stop(setsserial_inst_t *inst){
    sslbp_write_lbp(inst, LBPNONVOL_flag, 0);
}
    
static int sslbp_flash(setsserial_inst_t *inst, char *fname){
    hostmot2_t *hm2 = inst->hm2;
    hm2_sserial_remote_t *remote = inst->remote;
    const struct rtapi_firmware *fw;
    struct rtapi_device dev;
    int r;
    int write_sz, erase_sz;
    
    if (strstr("8i20", remote->name)){
        if (hm2->sserial.version < 37){
            gomc_log_infof(inst->env->log, "setsserial", "SSLBP Version must be at least v37 to flash the 8i20"
                        "This firmware has v%i. Sorry about that\n"
                        ,hm2->sserial.version);
            return -1;
        }
    }
    else if (hm2->sserial.version < 34){
        gomc_log_infof(inst->env->log, "setsserial", "SSLBP Version must be at least v34. This firmware has v%i"
                    "\n",hm2->sserial.version);
        return -1;
    }
    
    if (hm2->sserial.baudrate != 115200){
        gomc_log_infof(inst->env->log, "setsserial", "To flash firmware the baud rate of the board must be set "
                    "to 115200 by jumper, and in Hostmot2 using the "
                    "sserial_baudrate modparam\n");
        return -1;
    }
     
    memset(&dev, '\0', sizeof(dev));
    snprintf(dev.name, sizeof(dev.name), "%s", hm2->llio->name);
    r = rtapi_request_firmware(&fw, fname, &dev);
    if (r == -ENOENT) {
        HM2_ERR("firmware %s not found\n",fname);
        return -1;
    }
    if (r != 0) {
        HM2_ERR("request for firmware %s failed, aborting\n", fname);
        return -1;
    }    
    gomc_log_infof(inst->env->log, "setsserial", "Firmware size 0x%zx\n", fw->size);
    
    if (setup_start(inst) < 0) goto fail0;
    flash_start(inst);
    write_sz = 1 << sslbp_read_byte(inst, LBPFLASHWRITESIZELOC);
    erase_sz = 1 << sslbp_read_byte(inst, LBPFLASHERASESIZELOC);
    HM2_PRINT("Write Size = %x, Erase Size = %x\n", write_sz, erase_sz);
    flash_stop(inst);
    
    //Programming Loop
    {
        int ReservedBlock = 0;
        int StartBlock = ReservedBlock + 1;
        
        int blocknum = StartBlock;
        int block_start;
        int i, j, t;
        while ((size_t)(blocknum * erase_sz) < fw->size){
            block_start = blocknum * erase_sz;
            for (t = 0; t < erase_sz && fw->data[block_start + t] == 0 ; t++){ }
            if (t <  erase_sz){ // found a non-zero byte
                flash_start(inst);
                sslbp_write_long(inst, LBPFLASHOFFSETLOC, block_start);
                sslbp_write_byte(inst, LBPFLASHCOMMITLOC, FLASHERASE_CMD);
                if (sslbp_read_cookie(inst) != LBPCOOKIE){
                    HM2_ERR("Synch failed during block erase: aborting\n");
                    goto fail0;
                }
                flash_stop(inst);
                HM2_PRINT("Erased block %i\n", blocknum);
                flash_start(inst);
                for (i = 0; i < erase_sz ; i += write_sz){
                    sslbp_write_long(inst, LBPFLASHOFFSETLOC, block_start + i);
                    for (j = 0 ; j < write_sz ; j += 8){
                        uint32_t data0, data1, m;
                        m = block_start + i + j;
                        data0 = (fw->data[m] 
                              + (fw->data[m + 1] << 8)
                              + (fw->data[m + 2] << 16)
                              + (fw->data[m + 3] << 24));
                        data1 = (fw->data[m + 4] 
                              + (fw->data[m + 5] << 8)
                              + (fw->data[m + 6] << 16)
                              + (fw->data[m + 7] << 24));
                        sslbp_write_double(inst, j, data0, data1);
                    }
                    sslbp_write_byte(inst, LBPFLASHCOMMITLOC, FLASHWRITE_CMD);
                    if (sslbp_read_cookie(inst) != LBPCOOKIE){
                        HM2_ERR("Synch failed during block write: aborting\n");
                        goto fail0;
                    }
                }
                flash_stop(inst);
                HM2_PRINT("Wrote block %i\n", blocknum);
            }
            else // Looks like an all-zeros block
            { 
                HM2_PRINT("Skipped Block %i\n", blocknum);
            }
            blocknum++;
        }
    }
    
    rtapi_release_firmware(fw);
    
    return 0;
    
fail0:
    flash_stop(inst);
    return -1;
}

static void setsserial_destroy(cmod_t *self)
{
    setsserial_inst_t *inst = self->priv;
    inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

static int setsserial_run(setsserial_inst_t *inst, const char *cmd_str)
{
    int cnt;
    hm2_sserial_remote_t *remote;

    inst->cmd_list = argv_split(cmd_str, &cnt);

    inst->remote = hm2_get_sserial(&inst->hm2, inst->cmd_list[1]);
    if (!inst->remote) {
        gomc_log_errorf(inst->env->log, "setsserial",                         "Unable to find sserial remote corresponding to %s\n",
                        inst->cmd_list[1]);
        return -1;
    }

    remote = inst->remote;

    if (!strncmp("set", inst->cmd_list[0], 3) && cnt == 3){
        uint32_t value;
        uint32_t addr;
        int i;
        gomc_log_infof(inst->env->log, "setsserial", "set command %s\n", inst->cmd_list[1]);
        addr = 0;
        for (i = 0; i < remote->num_globals; i++){
            if (strstr(inst->cmd_list[1], remote->globals[i].NameString)){
                addr = remote->globals[i].ParmAddr;
                break;
            }
        }
        if (!addr) {
            gomc_log_errorf(inst->env->log, "setsserial",                             "Unable to find parameter corresponding to %s\n",
                            inst->cmd_list[1]);
            return -1;
        }
        value = simple_strtol(inst->cmd_list[2], NULL, 0);
        gomc_log_infof(inst->env->log, "setsserial", "remote name = %s ParamAddr = %x Value = %i\n",
                    remote->name, addr, value);
        if (set_nvram_param(inst, addr, value) < 0) {
            gomc_log_errorf(inst->env->log, "setsserial", "Parameter setting failed\n");
            return -1;
        } else {
            gomc_log_errorf(inst->env->log, "setsserial", "Parameter setting success\n");
            return 0;
        }
    }
    else if (!strncmp("flash", inst->cmd_list[0], 5) && cnt == 3){
        gomc_log_infof(inst->env->log, "setsserial", "flash command\n");
        if (!strstr(inst->cmd_list[2], ".BIN")){
            gomc_log_infof(inst->env->log, "setsserial", "Smart-Serial remote firmwares are .BIN format\n "
                        "flashing with the wrong one would be bad. Aborting\n");
            return -EINVAL;
        }
        if (sslbp_flash(inst, inst->cmd_list[2]) < 0){
            gomc_log_errorf(inst->env->log, "setsserial", "Firmware Flash Failed\n");
            return -1;
        } else {
            gomc_log_errorf(inst->env->log, "setsserial", "Firmware Flash Success\n");
            return 0;
        }
    }
    else {
        gomc_log_errorf(inst->env->log, "setsserial",                         "Unknown command or wrong number of parameters to "
                        "setsserial command");
        return -1;
    }

    return 0;
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    setsserial_inst_t *inst;
    const char *cmd_str = NULL;
    int ret;

    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "cmd=", 4) == 0)
            cmd_str = argv[i] + 4;
    }
    if (!cmd_str) {
        gomc_log_errorf(env->log, "setsserial", "setsserial: missing cmd= parameter\n");
        return -EINVAL;
    }

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;
    memset(inst, 0, sizeof(*inst));

    inst->env = env;

    inst->comp_id = env->hal->init(env->hal->ctx, "setsserial",
                                   env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) {
        ret = inst->comp_id;
        inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
        return ret;
    }
    env->hal->ready(env->hal->ctx, inst->comp_id);

    ret = setsserial_run(inst, cmd_str);
    if (ret != 0) {
        env->hal->exit(env->hal->ctx, inst->comp_id);
        inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
        return ret;
    }

    memset(&inst->cmod, 0, sizeof(inst->cmod));
    inst->cmod.Destroy = setsserial_destroy;
    inst->cmod.priv = inst;
    *out = &inst->cmod;
    return 0;
}
