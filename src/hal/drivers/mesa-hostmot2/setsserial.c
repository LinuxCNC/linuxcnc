#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hostmot2.h"

static int comp_id;

/* module information */
MODULE_AUTHOR("Andy Pugh");
MODULE_DESCRIPTION("A simple util for nvram setting on smart-serial cards");
MODULE_LICENSE("GPL");

static char *cmd;
RTAPI_MP_STRING(cmd, "smart-serial setting commands");

char **cmd_list;

int waitfor(hostmot2_t *hm2, hm2_sserial_remote_t *remote){
    u32 buff;
    long long int starttime = rtapi_get_time();
    do {
        rtapi_delay(50000);
        hm2->llio->read(hm2->llio, remote->command_reg_addr, &buff, sizeof(u32));
        if (rtapi_get_time() - starttime > 1000000000){
            rtapi_print_msg(RTAPI_MSG_ERR, "Timeout waiting for CMD to clear\n");
            return -1;
        }
    } while (buff);
                        
    hm2->llio->read(hm2->llio, remote->data_reg_addr, &buff, sizeof(u32));
    if (buff & (1 << remote->index)){
        rtapi_print_msg(RTAPI_MSG_ERR, "Error flag set after CMD Clear %08x\n",
                        buff);
        return -1;
    }
    return 0;
}
        

int set_nvram_param(hostmot2_t *hm2, hm2_sserial_remote_t *remote, 
                    u32 addr, 
                    u32 value){
    u32 buff;
    u32 doit;
    
    doit = 0x1000 | (1 << remote->index);
    
    //Stop
    buff=0x8FF;
    hm2->llio->write(hm2->llio, remote->command_reg_addr, &buff, sizeof(u32));
    if (waitfor(hm2, remote) < 0) { goto fail0;}
    
    // Setup Start
    buff=0xF00 | 1 << remote->index;
    hm2->llio->write(hm2->llio, remote->command_reg_addr, &buff, sizeof(u32));
    if (waitfor(hm2, remote) < 0) { goto fail0;}
    
    //Setup NV access on the correct channel and DoIt
    buff = 0xEC000000; // Write access
    hm2->llio->write(hm2->llio, remote->reg_cs_addr, &buff, sizeof(u32));
    buff = 0x01;  // write byte
    hm2->llio->write(hm2->llio, remote->reg_0_addr, &buff, sizeof(u32));
    hm2->llio->write(hm2->llio, remote->command_reg_addr, &doit, sizeof(u32));
    if (waitfor(hm2, remote) < 0) { goto fail0;}
    
    // value to set
    hm2->llio->write(hm2->llio, remote->reg_0_addr, &value, sizeof(u32));
    buff = 0x65000000 | addr; // 16 bit write access request
    hm2->llio->write(hm2->llio, remote->reg_cs_addr, &buff, sizeof(u32));
    hm2->llio->write(hm2->llio, remote->command_reg_addr, &doit, sizeof(u32));
    if (waitfor(hm2, remote) < 0) { goto fail0;}
    
     // Write access off
    buff = 0xEC000000;
    hm2->llio->write(hm2->llio, remote->reg_cs_addr, &buff, sizeof(u32));
    buff = 0x00;  //off
    hm2->llio->write(hm2->llio, remote->reg_0_addr, &buff, sizeof(u32));
    hm2->llio->write(hm2->llio, remote->command_reg_addr, &doit, sizeof(u32));

    return 0;
fail0: // It's all gone wrong
    buff=0x800; //Stop
    hm2->llio->write(hm2->llio, remote->command_reg_addr, &buff, sizeof(u32));
    rtapi_print_msg(RTAPI_MSG_ERR,
                    "Problem with Smart Serial parameter setting, see dmesg\n");
    return -1;
}

int rtapi_app_main(void)
{
    int cnt;
    hm2_sserial_remote_t *remote;
    hostmot2_t *hm2;

    comp_id = hal_init("setsserial");
    hal_ready(comp_id);
    
    cmd_list = argv_split(GFP_KERNEL, cmd, &cnt);
    
    if (! strncmp("set", cmd_list[0], 3) && cnt == 3){
        u32 value;
        u32 addr;
        int i;
        rtapi_print("set command %s\n", cmd_list[1]);
        remote = hm2_get_sserial(&hm2, cmd_list[1]);
        if (! remote) {   
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "Unable to find sserial remote corresponding to %s\n", 
                            cmd_list[1]);
            return -1;
        }
        addr = 0;
        for (i = 0; i < remote->num_globals; i++){
            if (strstr(cmd_list[1], remote->globals[i].NameString)){
                addr = remote->globals[i].ParmAddr;
                break;
            }
        }
        if (! addr) {   
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "Unable to find parameter corresponding to %s\n", 
                            cmd_list[1]);
            return -1;
        }
        value = simple_strtol(cmd_list[2], NULL, 0);
        rtapi_print("remote name = %s PamrAddr = %x Value = %i\n",
                    remote->name,
                    addr,
                    value);
        if (set_nvram_param(hm2, remote, addr, value) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Parameter setting failed\n");
            return -1;
        } 
        else
        {   rtapi_print_msg(RTAPI_MSG_ERR, "Parameter setting success\n");
            return 0;
        }
    } 
    else if (! strncmp("flash", cmd_list[0], 5) && cnt == 2){
        rtapi_print("flash command\n");
    }
    else {
        rtapi_print_msg(RTAPI_MSG_ERR, 
                        "Unknown commmand or wrong number of parameters to " 
                        "setsserial command");
        return -1;
    }
    
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}



