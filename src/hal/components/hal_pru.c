// based on supply.c
#include "config.h"

// this probably should be an ARM335x #define
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#include "prussdrv.h"           // UIO interface to uio_pruss
#include "pru.h"                // PRU-related defines
#include "pruss_intc_mapping.h"

#define UIO_PRUSS  "uio_pruss"  // required kernel module

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

// the code blob assembled from hal/components/maxrate.p
// this looks like so:
// const unsigned int maxrate_bin[] =  {
//     0x1f0ffefe,
//     0x1d0ffefe,
//     0x21000000 };

// PRU assembly code generated from maxrate.p

#include "blinkleds_bin.h"

#define PRU_CODE blinkleds_bin
#define PRU_CODE_SIZE sizeof(blinkleds_bin)

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("ARM335x PRU demo component");
MODULE_LICENSE("GPL");

static char *prucode = "";
RTAPI_MP_STRING(prucode, "alternative filename of PRU code (.bin), default: use compiled in code");

static int pru = 0;
RTAPI_MP_INT(pru, "PRU number to execute this code in, default 0; values 0 or 1");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

typedef struct {
    hal_bit_t *enable;		// pin: enable PRU pin wiggling
    hal_u32_t *speed;		// pin: parameter for PRU code
} hal_pru_t, *hal_pru_ptr;

hal_pru_ptr hal_pru;

/* other globals */
static int comp_id;		/* component ID */

#define MODNAME "hal_pru"
static const char *modname = MODNAME;

// shared with PRU
static unsigned char *pru_data_mem;     // points to PRU data RAM
static tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_pru(hal_pru_ptr addr);
static void update_pru(void *arg, long l);
static int setup_pru(int pru, char *filename, const unsigned int *code, int codelen);
static void pru_shutdown(int pru);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int retval;

    comp_id = hal_init(modname);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
	return -1;
    }
    /* allocate shared memory for supply data */
    hal_pru = hal_malloc(sizeof(hal_pru_t));
    if (hal_pru == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "%s: ERROR: hal_malloc() failed\n", modname);
	hal_exit(comp_id);
	return -1;
    }
    if ((retval = export_pru(hal_pru))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: var export failed: %d\n",
			modname, retval);
	hal_exit(comp_id);
	return -1;
    }
    if ((retval = setup_pru(pru, prucode, PRU_CODE, PRU_CODE_SIZE))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: failed to initialize PRU\n", modname);
	    hal_exit(comp_id);
	    return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed\n", modname);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    pru_shutdown(pru);
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/

static void update_pru(void *arg, long l)
{
    hal_pru_ptr p = (hal_pru_ptr) arg;
    pru_data_mem[0] = *(p->enable);
    pru_data_mem[1] = *(p->enable);
    pru_data_mem[2] = *(p->enable);

    // pru_data_mem[3] =
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_pru(hal_pru_ptr addr)
{
    int retval;
    char buf[HAL_NAME_LEN + 1];

    /* export pins */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->enable), comp_id, "%s.enable", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_u32_newf(HAL_IN, &(addr->speed), comp_id, "%s.speed", modname);
    if (retval != 0) {
	return retval;
    }
    /* init all structure members */
    *(addr->enable) = 0;
    *(addr->speed) = 0;

    /* export function  */
    rtapi_snprintf(buf, sizeof(buf), "%s.update", modname);
    retval = hal_export_funct(buf, update_pru, addr, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: update funct export failed\n", modname);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}

static int assure_module_loaded(const char *module)
{
    FILE *fd;
    char line[100];
    int len = strlen(module);
    int retval;

    fd = fopen("/proc/modules", "r");
    if (fd == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: cannot read /proc/modules\n",
			modname);
	return -1;
    }
    while (fgets(line, sizeof(line), fd)) {
	if (!strncmp(line, module, len)) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s: module '%s' already loaded\n",
			    modname, module);
	    fclose(fd);
	    return 0;
	}
    }
    fclose(fd);
    rtapi_print_msg(RTAPI_MSG_DBG, "%s: loading module '%s'\n",
		    modname, module);
    sprintf(line, "modprobe %s", module);
    if ((retval = system(line))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: executing '%s'  %d - %s\n",
			modname, line, errno, strerror(errno));
	return -1;
    }
    return 0;
}

static int setup_pru(int pru, char *filename, const unsigned int *code, int codelen)
{
    int retval;

    if (geteuid()) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: not running as root - need to 'sudo make setuid'?\n",
			modname);
	return -1;
    }
    if ((retval = assure_module_loaded(UIO_PRUSS)))
	return retval;

    // Allocate and initialize memory
    prussdrv_init ();
    // opens an event out and initializes memory mapping
    prussdrv_open (PRU_EVTOUT_0);

    // Map PRU's INTC
    prussdrv_pruintc_init(&pruss_intc_initdata);

    // Maps the PRU DRAM memory to input pointer
    prussdrv_map_prumem(pru ? PRUSS0_PRU1_DATARAM : PRUSS0_PRU0_DATARAM,
			(void **) &pru_data_mem);

    // setup dataram as expected by blinkleds.p
    pru_data_mem[0] = 0;
    pru_data_mem[1] = 0;
    pru_data_mem[2] = 0;
    pru_data_mem[3] = 1;

    if (strlen(filename) > 0) {
	// Load and execute binary on PRU
	prussdrv_exec_program (pru, filename);
    } else {
	prussdrv_exec_code(pru, code, codelen);
    }
    return 0;
}

static void pru_shutdown(int pru)
{
    // Wait for event completion from PRU
    // This assumes the PRU generates an interrupt
    // connected to event out 0 immediately before halting
    // prussdrv_pru_wait_event (PRU_EVTOUT_0);
    // prussdrv_pru_clear_event (PRU0_ARM_INTERRUPT);

    // Disable PRU and close memory mappings
    prussdrv_pru_disable(pru);
    prussdrv_exit ();
}
