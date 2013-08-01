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
#include "rtapi_compat.h"       /* RTAPI support functions */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// load this PRU code (prefixed by EMC_RTLIB_DIR)
#define  DEFAULT_CODE  "blinkleds.bin"

#include "prussdrv.h"           // UIO interface to uio_pruss
#include "pru.h"                // PRU-related defines
#include "pruss_intc_mapping.h"

static tprussdrv *pruss;                // driver descriptor
#define PRUSSDESC (pruss)

typedef volatile unsigned long pru_reg_t, *pru_reg_ptr;

#define PCOUNTER_RST_VAL(control) (((control) >> 16) & 0x0000ffff)
#define RUNSTATE (1 << 15)
#define SINGLE_STEP (1 << 8)
#define COUNTER_ENABLE (1 << 3)
#define SLEEPING (1 << 2)
#define ENABLE (1 << 1)
#define SOFT_RST_N (1 << 0)

#if 0
// a void * to the PRU's control base
#define CTRLBASE(pru) ((PRUSSDESC)->base[((pru) & 1)].control_base)
// a void * to the PRU's debug base
#define DEBUGBASE(pru) ((PRUSSDESC)->base[((pru) & 1)].debug_base)
// an unsigned long pointer to the control register
#define CONTROL_REG(pru) ((pru_reg_ptr)CTRLBASE(pru))

#define IS_RUNNING(pru) (*CONTROL_REG(pru) & RUNSTATE)
#define IS_ENABLED(pru) (*CONTROL_REG(pru) & ENABLE)
#define IS_SLEEPING(pru) (*CONTROL_REG(pru) & SLEEPING)
#define IS_STEPPING(pru) (*CONTROL_REG(pru) & SINGLE_STEP)

#define CLRCTRL(pru, bit) (*CONTROL_REG(pru) &=  ~(bit))
#define SETCTRL(pru, bit) (*CONTROL_REG(pru) |=  (bit))

#define PRU_DISABLE(pru) (CLRCTRL(pru, ENABLE))
#define PRU_ENABLE(pru) (SETCTRL(pru, ENABLE))
#define PRU_SET_STEPPING(pru) (SETCTRL(pru, SINGLE_STEP))
#define PRU_CLEAR_STEPPING(pru) (CLRCTRL(pru, SINGLE_STEP))
#define PRU_RESET(pru) (CLRCTRL(pru, SOFT_RST_N))

#define PC_AT_RESET(pru) (PCOUNTER_RST_VAL(*CONTROL_REG(pru)))
#define CURRENT_PC(pru) (*(CONTROL_REG(pru)+1))
#endif

#define UIO_PRUSS  "uio_pruss"  // required kernel module

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("AM335x PRU demo component");
MODULE_LICENSE("GPL");

static char *prucode = "";
RTAPI_MP_STRING(prucode, "filename of PRU code (.bin), default: blinkleds.bin");

static int pru = 0;
RTAPI_MP_INT(pru, "PRU number to execute this code in, default 0; values 0 or 1");

static int disabled = 0;
RTAPI_MP_INT(disabled, "start the PRU in disabled state (for debugging); default: enabled");

static int event = -1;
RTAPI_MP_INT(event, "PRU event number to listen for (0..7, default: none)");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

typedef struct {
    hal_bit_t *enable;		// pin: enable PRU pin wiggling
    hal_bit_t *exit;		// pin: exit PRU program and execute HALT
    hal_u32_t *speed;		// pin: parameter for PRU code

} hal_pru_t, *hal_pru_ptr;

hal_pru_ptr hal_pru;

/* other globals */
static int comp_id;		/* component ID */

#define MODNAME "hal_pru"
static const char *modname = MODNAME;

// if filename doesnt exist, prefix this path:
char *fw_path = "/lib/firmware/pru/";       

// shared with PRU
static unsigned long *pru_data_ram;     // points to PRU data RAM
static tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;


/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_pru(hal_pru_ptr addr);
static void update_pru(void *arg, long l);
static int setup_pru(int pru, char *filename, int disabled);
static void pru_shutdown(int pru);
static void *pruevent_thread(void *arg);

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
    /* allocate shared memory for HAL data */
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
    if ((retval = setup_pru(pru, prucode, disabled))) {
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

    // feeding params down to PRU ram
//    pru_data_ram[0] = *(p->enable);
//    pru_data_ram[1] = *(p->enable);
//    pru_data_ram[2] = *(p->enable);
//    pru_data_ram[3] = *(p->exit);

    pru_data_ram[1] = *(p->speed);
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
    retval = hal_pin_bit_newf(HAL_IN, &(addr->exit), comp_id, "%s.exit", modname);
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
    *(addr->exit) = 1;

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
    sprintf(line, "/sbin/modprobe %s", module);
    if ((retval = system(line))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: executing '%s'  %d - %s\n",
			modname, line, errno, strerror(errno));
	return -1;
    }
    return 0;
}

static int setup_pru(int pru, char *filename, int disabled)
{
    int i;
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
    if (prussdrv_open(event > -1 ? event : PRU_EVTOUT_0) < 0)
	return -1;

    // expose the driver data, filled in by prussdrv_open
    pruss = &prussdrv;

    // Map PRU's INTC
    if (prussdrv_pruintc_init(&pruss_intc_initdata) < 0)
	return -1;

    // Maps the PRU DRAM memory to input pointer
    if (prussdrv_map_prumem(pru ? PRUSS0_PRU1_DATARAM : PRUSS0_PRU0_DATARAM,
			(void **) &pru_data_ram) < 0)
	return -1;

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: PRU data ram mapped at %p\n",
		    modname, pru_data_ram);

    if (event > -1) {
	prussdrv_start_irqthread (event, sched_get_priority_max(SCHED_FIFO) - 2,
				  pruevent_thread, (void *) event);
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU event %d listener started\n",
		    modname, event);
    }
    // Initialize PRU structure with some defaults for testing
    // Enable all channels with a simple PWM setting
    for (i=0; i<7; i++) {
        pru_data_ram[8*i+0] = ((i*2+3) << 24) | ((i*2+2) << 16) | 0x0101L ;
        pru_data_ram[8*i+1] = 0x00100000 + ( i << 20);
        pru_data_ram[8*i+2] = 0x00060004;
        pru_data_ram[8*i+3] = 0x00070005;
        pru_data_ram[8*i+4] = 0;
        pru_data_ram[8*i+5] = 0;
        pru_data_ram[8*i+6] = 0;
        pru_data_ram[8*i+7] = 0;
    }

    // Setup channel 0 to do step/dir
    pru_data_ram[ 0] = 0x03020101;      // DirPin, StepPin, Mode, Enable
    pru_data_ram[ 1] = 0x00500000;      // Rate (27-bit, sign-extended)
    pru_data_ram[ 2] = 0x00060004;      // Dir Hold, Step High
    pru_data_ram[ 3] = 0x00070005;      // Dir Setup, Step Low
    pru_data_ram[ 4] = 0;
    pru_data_ram[ 5] = 0;
    pru_data_ram[ 6] = 0;
    pru_data_ram[ 7] = 0;

    // Load and execute binary on PRU
    // search for .bin files under rtlib/<flavorname>/filename.bin
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

static void *pruevent_thread(void *arg)
{
    int event = (int) arg;
    int event_count;
    do {
	if (prussdrv_pru_wait_event(event, &event_count) < 0)
	    continue;
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU event %d received\n",
		    modname, event);
	prussdrv_pru_clear_event(pru ? PRU1_ARM_INTERRUPT : PRU0_ARM_INTERRUPT);
    } while (1);
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: pruevent_thread exiting\n",
		    modname);
    return NULL; // silence compiler warning
}

static void pru_shutdown(int pru)
{
    // Disable PRU and close memory mappings
    prussdrv_pru_disable(pru);
    prussdrv_exit (); // also joins event listen thread
}
