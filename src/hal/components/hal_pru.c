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
#define SOFT_RST_N (1 << 1)

// a void * to the PRU's control base
#define CTRLBASE(pru) (pru ? (PRUSSDESC)->pru1_control_base : (PRUSSDESC)->pru0_control_base)
// a void * to the PRU's debug base
#define DEBUGBASE(pru) (pru ? (PRUSSDESC)->pru1_debug_base : (PRUSSDESC)->pru0_debug_base)
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
#define PRU_RESET(pru) (SETCTRL(pru, SOFT_RST_N))

#define PC_AT_RESET(pru) (PCOUNTER_RST_VAL(*CONTROL_REG(pru)))
#define CURRENT_PC(pru) (*(CONTROL_REG(pru)+1))

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

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

typedef struct {
    hal_bit_t *enable;		// pin: enable PRU pin wiggling
    hal_bit_t *dump;		// pin: trigger register dump
    hal_bit_t prev_dump;
    hal_u32_t *speed;		// pin: parameter for PRU code
} hal_pru_t, *hal_pru_ptr;

hal_pru_ptr hal_pru;

/* other globals */
static int comp_id;		/* component ID */

#define MODNAME "hal_pru"
static const char *modname = MODNAME;

// shared with PRU
static unsigned char *pru_data_ram;     // points to PRU data RAM
static tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;


/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_pru(hal_pru_ptr addr);
static void update_pru(void *arg, long l);
static int setup_pru(int pru, char *filename);
static void pru_shutdown(int pru);
static void read_pru_state();

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
    if ((retval = setup_pru(pru, prucode))) {
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

    if (*(p->dump) ^ p->prev_dump)
	read_pru_state(pru);

    pru_data_ram[0] = *(p->enable);
    pru_data_ram[1] = *(p->enable);
    pru_data_ram[2] = *(p->enable);

    // pru_data_ram[3] =
    p->prev_dump =  *(p->dump);
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
    retval = hal_pin_bit_newf(HAL_IN, &(addr->dump), comp_id, "%s.dump", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_u32_newf(HAL_IN, &(addr->speed), comp_id, "%s.speed", modname);
    if (retval != 0) {
	return retval;
    }
    /* init all structure members */
    *(addr->enable) = 0;
    *(addr->dump) = 0;
    addr->prev_dump = 0;
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

static void read_pru_state(int pru)
{
    int i, was_running;
    pru_reg_ptr ctrl, reg;

    ctrl = CTRLBASE(pru);
    was_running = IS_RUNNING(pru);

    // dont use prussdrv_pru_disable(pru) - this resets the cpu
    // we just want to halt while looking at the registers
    while (IS_RUNNING(pru))
	PRU_DISABLE(pru);

    printf("CONTROL = %8.8lx, ",(*CONTROL_REG(pru)));
    printf(was_running ? "running" : "halted");
    printf(IS_STEPPING(pru) ? ", stepping" : "");
    printf(IS_SLEEPING(pru) ? ", sleeping" : "");
    printf(", start after reset at: %8.8lx\n", PC_AT_RESET(pru));

    printf("PC = %8.8lx (%ld)\n", CURRENT_PC(pru), CURRENT_PC(pru));
    printf("WAKEUP_EN = %8.8lx\t", ctrl[2]);
    printf("CYCLE = %8.8lx\t", ctrl[3]);
    printf("STALL = %8.8lx\n", ctrl[4]);
    printf("CTBIR0 = %8.8lx\t", ctrl[8]);
    printf("CTBIR1 = %8.8lx\n", ctrl[9]);
    printf("CTPPR0 = %8.8lx\t", ctrl[10]);
    printf("CTPPR1 = %8.8lx\n", ctrl[11]);

    reg = DEBUGBASE(pru);
    for (i = 0; i < 32; i++) {
	printf("R%-2d = %8.8lx  ", i,reg[i]);
	if ((i & 3) == 3)
	    printf("\n");
    }
    // for (i = 32; i < 64; i++)
    for (i = 32; i < 34; i++)
	printf("CT_REG%d = %8.8lx\t", i-32, reg[i]);
    printf("\n");

    if (was_running) // if PRU was found running, reenable
	PRU_ENABLE(pru);
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

static int setup_pru(int pru, char *filename)
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

    // expose the driver data, filled in by prussdrv_open
    pruss = prussdrv_self();

    // Map PRU's INTC
    prussdrv_pruintc_init(&pruss_intc_initdata);

    // Maps the PRU DRAM memory to input pointer
    prussdrv_map_prumem(pru ? PRUSS0_PRU1_DATARAM : PRUSS0_PRU0_DATARAM,
			(void **) &pru_data_ram);

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: PRU data ram mapped at %p\n",
		    modname, pru_data_ram);

    // setup dataram as expected by blinkleds.p
    pru_data_ram[0] = 0;
    pru_data_ram[1] = 0;
    pru_data_ram[2] = 0;
    pru_data_ram[3] = 1;

    // Load and execute binary on PRU
    if (!strlen(filename))
	filename = EMC2_RTLIB_DIR "/" DEFAULT_CODE;
    retval =  prussdrv_exec_program (pru, filename);

    return retval;
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
