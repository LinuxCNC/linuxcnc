//----------------------------------------------------------------------//
// Description: hal_pru_generic.c                                       //
// HAL module to communicate with PRU code implementing step/dir        //
// generation and other functions of hopeful use to off-load timing     //
// critical code from LinuxCNC HAL                                      //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2013-May    Charles Steinkuehler                                     //
//             Split into several files                                 //
//             Added support for PRU task list                          //
//             Refactored code to more closely match mesa-hostmot2      //
// 2012-Dec-30 Charles Steinkuehler                                     //
//             Initial version, based in part on:                       //
//               hal_pru.c      Micheal Haberler                        //
//               supply.c       Matt Shaver                             //
//               stepgen.c      John Kasunich                           //
//               hostmot2 code  Sebastian Kuzminsky                     //
//----------------------------------------------------------------------//
// This file is part of LinuxCNC HAL                                    //
//                                                                      //
// Copyright (C) 2012  Charles Steinkuehler                             //
//                     <charles AT steinkuehler DOT net>                //
//                                                                      //
// This program is free software; you can redistribute it and/or        //
// modify it under the terms of the GNU General Public License          //
// as published by the Free Software Foundation; either version 2       //
// of the License, or (at your option) any later version.               //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program; if not, write to the Free Software          //
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA        //
// 02110-1301, USA.                                                     //
//                                                                      //
// THE AUTHORS OF THIS PROGRAM ACCEPT ABSOLUTELY NO LIABILITY FOR       //
// ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE   //
// TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of      //
// harming persons must have provisions for completely removing power   //
// from all motors, etc, before persons enter any danger area.  All     //
// machinery must be designed to comply with local and national safety  //
// codes, and the authors of this software can not, and do not, take    //
// any responsibility for such compliance.                              //
//                                                                      //
// This code was written as part of the LinuxCNC project.  For more     //
// information, go to www.linuxcnc.org.                                 //
//----------------------------------------------------------------------//

// Use config_module.h instead of config.h so we can use RTAPI_INC_LIST_H
#include "config_module.h"

// this probably should be an ARM335x #define
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#include RTAPI_INC_LIST_H
#include "rtapi.h"          /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */
#include "rtapi_compat.h"   /* RTAPI support functions */
#include "rtapi_math.h"
#include "hal.h"            /* HAL public API decls */
#include <pthread.h>

#include "prussdrv.h"           // UIO interface to uio_pruss
//#include "pru.h"                // PRU-related defines
#include "pruss_intc_mapping.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hal/drivers/hal_pru_generic/hal_pru_generic.h"
#include "hal/drivers/hal_pru_generic/beaglebone_pinmap.h"

MODULE_AUTHOR("Charles Steinkuehler");
MODULE_DESCRIPTION("AM335x PRU demo component");
MODULE_LICENSE("GPL");

/***********************************************************************
*                    MODULE PARAMETERS AND DEFINES                     *
************************************************************************/

// Maximum number of PRU "channels"
#define MAX_CHAN 8

// Default PRU code to load (prefixed by EMC_RTLIB_DIR)
// Fixme: This should probably be compiled in, via a header file generated
//        by pasm -PRUv2 -c myprucode.p
#define  DEFAULT_CODE  "stepgen.bin"

// The kernel module required to talk to the PRU
#define UIO_PRUSS  "uio_pruss"

// Default pin to use for PRU modules...use a pin that does not leave the PRU
#define PRU_DEFAULT_PIN 17

// Start out with default pulse length/width and setup/hold delays of 1 mS (1000000 nS) 
#define DEFAULT_DELAY 1000000

#define f_period_s ((double)(l_period_ns * 1e-9))

static int num_stepgens = 0;
RTAPI_MP_INT(num_stepgens, "Number of step generators (default: 0)");

static int num_pwmgens = 0;
RTAPI_MP_INT(num_pwmgens, "Number of PWM outputs (default: 0)");
//int num_pwmgens[MAX_CHAN] = { -1, -1, -1, -1, -1, -1, -1, -1 };
//RTAPI_MP_ARRAY_INT(num_pwmgens, "Number of PWM outputs for up to 8 banks (default: 0)");

static int num_encoders = 0;
RTAPI_MP_INT(num_encoders, "Number of encoder channels (default: 0)");

static char *halname = "hal_pru_generic";
RTAPI_MP_STRING(halname, "Prefix for hal names (default: hal_pru_generic)");

static char *prucode = "";
RTAPI_MP_STRING(prucode, "filename of PRU code (.bin, default: stepgen.bin)");

static int pru = 1;
RTAPI_MP_INT(pru, "PRU number to execute this code (0 or 1, default: 1)");

static int pru_period = 10000;
RTAPI_MP_INT(pru_period, "PRU task period (in nS, default: 10,000 nS or 100 KHz)");

static int disabled = 0;
RTAPI_MP_INT(disabled, "start the PRU in disabled state for debugging (0=enabled, 1=disabled, default: enabled");

static int event = -1;
RTAPI_MP_INT(event, "PRU event number to listen for (0..7, default: none)");

/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/

static tprussdrv *pruss;                // driver descriptor

/* other globals */
static int comp_id;     /* component ID */

static const char *modname = "hal_pru_generic";

// if filename doesnt exist, prefix this path:
char *fw_path = "/lib/firmware/pru/";       

// shared with PRU
static unsigned long *pru_data_ram;     // points to PRU data RAM
static tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;


/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static void hpg_read(void *void_hpg, long period);
static void hpg_write(void *arg, long l);
int export_pru(hal_pru_generic_t *hpg);
int pru_init(int pru, char *filename, int disabled, hal_pru_generic_t *hpg);
int setup_pru(int pru, char *filename, int disabled, hal_pru_generic_t *hpg);
void pru_shutdown(int pru);
static void *pruevent_thread(void *arg);

int hpg_wait_init(hal_pru_generic_t *hpg);
void hpg_wait_force_write(hal_pru_generic_t *hpg);
void hpg_wait_update(hal_pru_generic_t *hpg);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    hal_pru_generic_t *hpg;
    int retval;

    comp_id = hal_init("hal_pru_generic");
    if (comp_id < 0) {
        HPG_ERR("ERROR: hal_init() failed\n");
        return -1;
    }

    // Allocate HAL shared memory for state data
    hpg = hal_malloc(sizeof(hal_pru_generic_t));
    if (hpg == 0) {
        HPG_ERR("ERROR: hal_malloc() failed\n");
    hal_exit(comp_id);
    return -1;
    }

    // Clear memory
    memset(hpg, 0, sizeof(hal_pru_generic_t));

    // Initialize PRU and map PRU data memory
    if ((retval = pru_init(pru, prucode, disabled, hpg))) {
        HPG_ERR("ERROR: failed to initialize PRU\n");
        hal_exit(comp_id);
        return -1;
    }

    // Setup global state
    hpg->config.num_pwmgens  = num_pwmgens;
    hpg->config.num_stepgens = num_stepgens;
    hpg->config.num_encoders = num_encoders;
    hpg->config.comp_id      = comp_id;
    hpg->config.pru_period   = pru_period;
    hpg->config.name         = modname;
    hpg->config.halname      = halname;

    rtapi_print("num_pwmgens : %d\n",num_pwmgens);
    rtapi_print("num_stepgens: %d\n",num_stepgens);
    rtapi_print("num_encoders: %d\n",num_encoders);

    rtapi_print("Init pwm\n");
    // Initialize various functions and generate PRU data ram contents
    if ((retval = hpg_pwmgen_init(hpg))) {
        HPG_ERR("ERROR: pwmgen init failed: %d\n", retval);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_print("Init stepgen\n");
    if ((retval = hpg_stepgen_init(hpg))) {
        HPG_ERR("ERROR: stepgen init failed: %d\n", retval);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_print("Init encoder\n");
    if ((retval = hpg_encoder_init(hpg))) {
        HPG_ERR("ERROR: encoder init failed: %d\n", retval);
        hal_exit(comp_id);
        return -1;
    }

    if ((retval = hpg_wait_init(hpg))) {
        HPG_ERR("ERROR: global task init failed: %d\n", retval);
        hal_exit(comp_id);
        return -1;
    }

    if ((retval = export_pru(hpg))) {
        HPG_ERR("ERROR: var export failed: %d\n", retval);
        hal_exit(comp_id);
        return -1;
    }

    hpg_stepgen_force_write(hpg);
    hpg_pwmgen_force_write(hpg);
    hpg_encoder_force_write(hpg);
    hpg_wait_force_write(hpg);

    if ((retval = setup_pru(pru, prucode, disabled, hpg))) {
        HPG_ERR("ERROR: failed to initialize PRU\n");
        hal_exit(comp_id);
        return -1;
    }
    HPG_INFO("installed\n");
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    pru_shutdown(pru);
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/
static void hpg_read(void *void_hpg, long period) {
    hal_pru_generic_t *hpg = void_hpg;

    hpg_stepgen_read(hpg, period);
    hpg_encoder_read(hpg);

}

u16 ns2periods(hal_pru_generic_t *hpg, hal_u32_t ns) {
    u16 p = rtapi_ceil((double)ns / (double)hpg->config.pru_period);
    return p;
}

static void hpg_write(void *void_hpg, long period) {
    hal_pru_generic_t *hpg      = void_hpg;

    hpg_stepgen_update(hpg, period);
    hpg_pwmgen_update(hpg);
    hpg_encoder_update(hpg);
    hpg_wait_update(hpg);

}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

// Allocate 32-bit words from PRU data memory
// Start at the beginning of memory, and contiguously allocate RAM
// until we run out of requests.  No free, no garbage colletion, etc.
// Feel free to enhance this when you start adding and removing PRU
// tasks at run-time!  :)
pru_addr_t pru_malloc(hal_pru_generic_t *hpg, int len) {
    // Return address is first free memory location
    pru_addr_t a = hpg->pru_data_free;

    // Insure length is a natural 32-bit length
    int len32 = (len & ~0x03);
    if ((len % 4) != 0) len32 += 4;

    // Point to the next free location
    hpg->pru_data_free += len32;

    HPG_DBG("pru_malloc requested %d bytes, allocated %d bytes starting at %04x\n", len, len32, a);

    // ...and we're done
    return a;
}

int export_pru(hal_pru_generic_t *hpg)
{
    int r;
    char name[HAL_NAME_LEN + 1];

    // Export functions
    rtapi_snprintf(name, sizeof(name), "%s.update", halname);
    r = hal_export_funct(name, hpg_write, hpg, 1, 0, comp_id);
    if (r != 0) {
        HPG_ERR("ERROR: function export failed: %s\n", name);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_snprintf(name, sizeof(name), "%s.capture-position", halname);
    r = hal_export_funct(name, hpg_read, hpg, 1, 0, comp_id);
    if (r != 0) {
        HPG_ERR("ERROR: function export failed: %s\n", name);
        hal_exit(comp_id);
        return -1;
    }

    return 0;
}


int assure_module_loaded(const char *module)
{
    FILE *fd;
    char line[100];
    int len = strlen(module);
    int retval;

    fd = fopen("/proc/modules", "r");
    if (fd == NULL) {
        HPG_ERR("ERROR: cannot read /proc/modules\n");
        return -1;
    }
    while (fgets(line, sizeof(line), fd)) {
        if (!strncmp(line, module, len)) {
            HPG_DBG("module '%s' already loaded\n", module);
            fclose(fd);
            return 0;
        }
    }
    fclose(fd);
    HPG_DBG("loading module '%s'\n", module);
    sprintf(line, "/sbin/modprobe %s", module);
    if ((retval = system(line))) {
        HPG_ERR("ERROR: executing '%s'  %d - %s\n", line, errno, strerror(errno));
        return -1;
    }
    return 0;
}

int pru_init(int pru, char *filename, int disabled, hal_pru_generic_t *hpg) {
    
    int i;
    int retval;

    if (geteuid()) {
        HPG_ERR("ERROR: not running as root - need to 'sudo make setuid'?\n");
        return -1;
    }
    if ((retval = assure_module_loaded(UIO_PRUSS)))
    return retval;

rtapi_print("prussdrv_init\n");
    // Allocate and initialize memory
    prussdrv_init ();

    // opens an event out and initializes memory mapping
rtapi_print("prussdrv_open\n");
    if (prussdrv_open(event > -1 ? event : PRU_EVTOUT_0) < 0)
    return -1;

    // expose the driver data, filled in by prussdrv_open
    pruss = &prussdrv;

    // Map PRU's INTC
rtapi_print("prussdrv_pruintc_init\n");
    if (prussdrv_pruintc_init(&pruss_intc_initdata) < 0)
    return -1;

    // Maps the PRU DRAM memory to input pointer
rtapi_print("prussdrv_map_prumem\n");
    if (prussdrv_map_prumem(pru ? PRUSS0_PRU1_DATARAM : PRUSS0_PRU0_DATARAM,
            (void **) &pru_data_ram) < 0)
    return -1;

rtapi_print("PRU data ram mapped\n");
    rtapi_print_msg(RTAPI_MSG_DBG, "%s: PRU data ram mapped at %p\n",
            modname, pru_data_ram);

    hpg->pru_data = (u32 *) pru_data_ram;

    // Zero PRU data memory
    for (i = 0; i < 8192/4; i++) {
        hpg->pru_data[i] = 0;
    }

    // Reserve PRU memory for static configuration variables
    hpg->pru_stat_addr = PRU_DATA_START;
    hpg->pru_data_free = hpg->pru_stat_addr + sizeof(PRU_statics_t);

    // Setup PRU globals
    hpg->pru_stat.task.hdr.dataX = 0xAB;
    hpg->pru_stat.task.hdr.dataY = 0xFE;
    hpg->pru_stat.period = pru_period;
    hpg->config.pru_period = pru_period;

    PRU_statics_t *stat = (PRU_statics_t *) ((u32) hpg->pru_data + (u32) hpg->pru_stat_addr);
    *stat = hpg->pru_stat;

    return 0;
}

int setup_pru(int pru, char *filename, int disabled, hal_pru_generic_t *hpg) {
    
    int retval;

    if (event > -1) {
    prussdrv_start_irqthread (event, sched_get_priority_max(SCHED_FIFO) - 2,
                  pruevent_thread, (void *) event);
    HPG_ERR("PRU event %d listener started\n",event);
    }

    // Load and execute binary on PRU
    // search for .bin files as passed in and under fw_path
    char pru_binpath[PATH_MAX];

    // default the .bin filename if not given
    if (!strlen(filename))
    filename = DEFAULT_CODE;
    
    strcpy(pru_binpath, filename);

    struct stat statb;

    if (!((stat(pru_binpath, &statb) == 0) &&
     S_ISREG(statb.st_mode))) {

    // filename not found, prefix fw_path & try that:
    strcpy(pru_binpath, fw_path);
    strcat(pru_binpath, filename);

    if (!((stat(pru_binpath, &statb) == 0) &&
          S_ISREG(statb.st_mode))) {
        // nyet, complain
        getcwd(pru_binpath, sizeof(pru_binpath));
        rtapi_print_msg(RTAPI_MSG_ERR,
                "%s: cant find %s in %s or %s\n",
                modname, filename, pru_binpath, fw_path);
        return -ENOENT;
    }
    }
    retval =  prussdrv_exec_program (pru, pru_binpath, disabled);
    return retval;
}

void pru_task_add(hal_pru_generic_t *hpg, pru_task_t *task) {

    if (hpg->last_task == 0) {
        // This is the first task
        HPG_DBG("Adding first task: addr=%04x\n", task->addr);
        hpg->pru_stat.task.hdr.addr = task->addr;
        task->next = task->addr;
        hpg->last_task  = task;
    } else {
        // Add this task to the end of the task list
        HPG_DBG("Adding task: addr=%04x prev=%04x\n", task->addr, hpg->last_task->addr);
        task->next = hpg->pru_stat.task.hdr.addr;
        hpg->last_task->next = task->addr;
        hpg->last_task = task;
    }
}

static void *pruevent_thread(void *arg)
{
    int event = (int) arg;
    int event_count;
    do {
    if (prussdrv_pru_wait_event(event, &event_count) < 0)
        continue;
    HPG_ERR("PRU event %d received\n",event);
    prussdrv_pru_clear_event(pru ? PRU1_ARM_INTERRUPT : PRU0_ARM_INTERRUPT);
    } while (1);
    HPG_ERR("pruevent_thread exiting\n");
    return NULL; // silence compiler warning
}

void pru_shutdown(int pru)
{
    // Disable PRU and close memory mappings
    prussdrv_pru_disable(pru);
    prussdrv_exit (); // also joins event listen thread
}

int hpg_wait_init(hal_pru_generic_t *hpg) {
    int r;

    hpg->wait.task.addr = pru_malloc(hpg, sizeof(hpg->wait.pru));

    pru_task_add(hpg, &(hpg->wait.task));

    r = hal_pin_u32_newf(HAL_IN, &(hpg->hal.pin.pru_busy_pin), hpg->config.comp_id, "%s.pru_busy_pin", hpg->config.name);
    if (r != 0) { return r; }

    *(hpg->hal.pin.pru_busy_pin) = 0x80;

    return 0;
}

void hpg_wait_force_write(hal_pru_generic_t *hpg) {
    hpg->wait.pru.task.hdr.mode = eMODE_WAIT;
    hpg->wait.pru.task.hdr.dataX = *(hpg->hal.pin.pru_busy_pin);
    hpg->wait.pru.task.hdr.dataY = 0x00;
    hpg->wait.pru.task.hdr.addr = hpg->wait.task.next;

    PRU_task_wait_t *pru = (PRU_task_wait_t *) ((u32) hpg->pru_data + (u32) hpg->wait.task.addr);
    *pru = hpg->wait.pru;

    PRU_statics_t *stat = (PRU_statics_t *) ((u32) hpg->pru_data + (u32) hpg->pru_stat_addr);
    *stat = hpg->pru_stat;
}

void hpg_wait_update(hal_pru_generic_t *hpg) {
    if (hpg->wait.pru.task.hdr.dataX != *(hpg->hal.pin.pru_busy_pin))
        hpg->wait.pru.task.hdr.dataX = *(hpg->hal.pin.pru_busy_pin);

    PRU_task_wait_t *pru = (PRU_task_wait_t *) ((u32) hpg->pru_data + (u32) hpg->wait.task.addr);
    *pru = hpg->wait.pru;
}

int fixup_pin(u32 hal_pin) {
    int ret = 0;
    u32 type, p89, index;

    // Original brain-dead pin numbering
    if (hal_pin < 192) {
        ret = hal_pin;
    } else {
        type  =  hal_pin / 100;
        p89   = (hal_pin % 1000) / 100 ;
        index =  hal_pin % 100;

        // Fixup index value for P9 pins with two CPU pins attached
        if (p89 == 9) {
            if ((index == 91) || (index == 92)) {
                index = index - 50 + (47 - 41);
            } else if (index > 46) {
                index = 0;
            }
        } else if (index > 46) {
            index = 0;
        }

        switch (type) {
        case 8 :
            ret = p8_pins[index].gpio_pin_num;
            break;
        case 9 :
            ret = p9_pins[index].gpio_pin_num;
            break;
        case 18 :
            ret = p8_pins[index].pruO_pin_num;
            break;
        case 19 :
            ret = p9_pins[index].pruO_pin_num;
            break;
        case 28 :
            ret = p8_pins[index].pruI_pin_num;
            break;
        case 29 :
            ret = p9_pins[index].pruI_pin_num;
            break;
        default:
            ret = 0;
        }    

        if (ret == 0)
            HPG_ERR("Unknown pin: %d\n",(int)hal_pin);

        if (ret < 0) {
            HPG_ERR("Requested pin unavailable: %d\n",(int)hal_pin);
            ret = 0;
        }
    }

    return ret;
}

