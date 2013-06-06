// based on supply.c


#include "config.h"

// this probably should be an ARM335x #define
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

// try to make sense of IEP and ECAP counters
// not part of debugging
#define EXPLORE_COUNTERS 1

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include <pthread.h>

#include "prussdrv.h"           // UIO interface to uio_pruss
#include "pru.h"                // PRU-related defines
#include "pruss_intc_mapping.h"

static tprussdrv *pruss;                // driver descriptor
#define PRUSSDESC (pruss)

#define PCOUNTER_RST_VAL(control) (((control) >> 16) & 0x0000ffff)
#define RUNSTATE (1 << 15)
#define SINGLE_STEP (1 << 8)
#define COUNTER_ENABLE (1 << 3)
#define SLEEPING (1 << 2)
#define ENABLE (1 << 1)
#define SOFT_RST_N (1 << 0)

// a void * to the PRU's control base
#define CTRLBASE(pru) ((PRUSSDESC)->base[PN(pru)].control_base)
// a void * to the PRU's debug base
#define DEBUGBASE(pru) ((PRUSSDESC)->base[PN(pru)].debug_base)
// an unsigned long pointer to the control register
#define CONTROL_REG(pru) ((preg_ptr)CTRLBASE(pru))

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


#define IEP_COUNT  (iep_base[3])  // see SPRUHF8 p249
// http://e2e.ti.com/support/dsp/sitara_arm174_microprocessors/f/791/t/169620.aspx
#define ECAP_COUNT  (ecap_base[3])

#define UIO_PRUSS  "uio_pruss"  // required kernel module

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("AM335x PRU debugger component");
MODULE_LICENSE("GPL");

static int event = -1;
RTAPI_MP_INT(event, "PRU event number to listen for (0..7, default: none)");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

typedef struct {
    hal_bit_t *continuous;	// pin: sample CPU state every thread period if true
    hal_bit_t *prunum;	        // pin: 0 or 1

    // PRU control
    hal_bit_t *snap;		// pin: trigger state snapshot
    hal_bit_t prev_snap;
    hal_bit_t *step;
    hal_bit_t prev_step;
    hal_bit_t *_continue;
    hal_bit_t prev_continue;
    hal_bit_t *halt;
    hal_bit_t prev_halt;
    hal_bit_t *reset;
    hal_bit_t prev_reset;
    hal_u32_t *registers[32];
    hal_u32_t *controlreg, *program_counter, *events, *iep_count, *ecap_count;
} hal_pru_t, *hal_pru_ptr;

hal_pru_ptr hal_pru;

/* other globals */
static int comp_id;		/* component ID */

#define MODNAME "hal_prudebug"
static const char *modname = MODNAME;

static tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
static preg_ptr iep_base;  // to access IEP COUNT (200Mhz)
static preg_ptr ecap_base;  // to access eCAP COUNT (200Mhz)

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_pru(hal_pru_ptr addr);
static void update_pru(void *arg, long l);
static int setup_pru();
static void pru_shutdown(int pru);
static void read_pru_state();
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
    if ((retval = setup_pru())) {
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
    pru_shutdown(*(hal_pru->prunum));
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/

static void update_pru(void *arg, long l)
{
    hal_pru_ptr p = (hal_pru_ptr) arg;

    if ((*(p->snap) ^ p->prev_snap) && *(p->snap))  // on rising edge of snap
	read_pru_state(*(p->prunum));
    else if (*(p->continuous))                      // or if continuous set
	read_pru_state(*(p->prunum));

    if ((*(p->halt) ^ p->prev_halt) && *(p->halt)) {
	while (IS_RUNNING(*(p->prunum)))
	    PRU_DISABLE(*(p->prunum));
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU%d: disabled\n", modname, *(p->prunum));
    }
    if ((*(p->step) ^ p->prev_step) && *(p->step))  {
	PRU_SET_STEPPING(*(p->prunum));
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU%d: stepping enabled\n", modname, *(p->prunum));
    }
    if ((*(p->step) ^ p->prev_step) && !*(p->step))  {
	PRU_CLEAR_STEPPING(*(p->prunum));
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU%d: stepping disabled\n", modname, *(p->prunum));
    }
    if ((*(p->reset) ^ p->prev_reset) && *(p->reset))  {
	PRU_RESET(*(p->prunum));
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU%d: reset\n", modname, *(p->prunum));
    }
    if ((*(p->_continue) ^ p->prev_continue) && *(p->_continue))  {
	PRU_ENABLE(*(p->prunum));
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU%d: enabled\n", modname,*(p->prunum));
    }

    // tracking variables for edge detection
    p->prev_halt = *(p->halt);
    p->prev_step =  *(p->step);
    p->prev_continue = *(p->_continue);
    p->prev_snap =  *(p->snap);
    p->prev_reset =*(p->reset);

}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_pru(hal_pru_ptr addr)
{
    int retval, i;
    char buf[HAL_NAME_LEN + 1];

    /* export pins */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->continuous), comp_id, "%s.continuous", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->prunum), comp_id, "%s.pru", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->step), comp_id, "%s.step", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->halt), comp_id, "%s.halt", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->_continue), comp_id, "%s.continue", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->reset), comp_id, "%s.reset", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->snap), comp_id, "%s.snap", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_u32_newf(HAL_OUT, &(addr->events), comp_id, "%s.events", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_u32_newf(HAL_OUT, &(addr->controlreg), comp_id, "%s.CONTROL", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_u32_newf(HAL_OUT, &(addr->program_counter), comp_id, "%s.PC", modname);
    if (retval != 0) {
	return retval;
    }
#ifdef EXPLORE_COUNTERS
    retval = hal_pin_u32_newf(HAL_OUT, &(addr->iep_count), comp_id, "%s.IEP_COUNT", modname);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_u32_newf(HAL_OUT, &(addr->ecap_count), comp_id, "%s.ECAP_COUNT", modname);
    if (retval != 0) {
	return retval;
    }
#endif
    for (i = 0; i < 32; i++) {
	retval = hal_pin_u32_newf(HAL_OUT, &(addr->registers[i]), comp_id, "%s.R%d", modname, i);
	if (retval != 0)
	    return retval;
	*(addr->registers[i]) = 0;
    }

    /* init all structure members */
    *(addr->continuous) = 0;
    *(addr->prunum) = 0;
    *(addr->controlreg) = 0;
    *(addr->program_counter) = 0;
    *(addr->events) = 0;
#ifdef EXPLORE_COUNTERS
    *(addr->iep_count) = 0;
    *(addr->ecap_count) = 0;
#endif

    // PRU control pins
    *(addr->snap) = addr->prev_snap = 0;
    *(addr->halt) = addr->prev_halt = 0;
    *(addr->step) = addr->prev_step = 0;
    *(addr->_continue) = addr->prev_continue = 0;
    *(addr->reset) = addr->prev_reset = 0;

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
    int i;
    preg control_prev;
    preg_ptr reg;

    control_prev = IS_RUNNING(pru);  // RUNSTATE bit

    // dont use prussdrv_pru_disable(pru) - this resets the cpu
    // we just want to halt while looking at the registers
    while (IS_RUNNING(pru))
	PRU_DISABLE(pru);

    // since just halted for inspection, or in the RUNSTATE bit
    *(hal_pru->controlreg) = (*CONTROL_REG(pru)) |  control_prev;
    *(hal_pru->program_counter) = CURRENT_PC(pru);

#ifdef EXPLORE_COUNTERS
    // sample the Industrial Ethernet peripheral counter (200Mhz)
    // not important for debugging but good to know how it's accessed
    *(hal_pru->iep_count) = IEP_COUNT;
    *(hal_pru->ecap_count) = ECAP_COUNT; // doesnt work
#endif
    reg = DEBUGBASE(pru);
    for (i = 0; i < 32; i++)
	*(hal_pru->registers[i]) = reg[i];

    if (control_prev) // if PRU was found running, reenable
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

static int setup_pru(int pru)
{
    int retval = 0;

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

#ifdef EXPLORE_COUNTERS
    if (prussdrv_map_peripheral_io(PRUSS0_IEP, (void **)&iep_base)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: failed to map IEP base\n",
		    modname);
    }
    if (prussdrv_map_peripheral_io(PRUSS0_ECAP, (void **)&ecap_base)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: failed to map eCAP base\n",
		    modname);
    }
#endif
    // Map PRU's INTC
    if (prussdrv_pruintc_init(&pruss_intc_initdata) < 0)
	return -1;

    if (event > -1) {
	prussdrv_start_irqthread (event, sched_get_priority_max(SCHED_FIFO) - 2,
				  pruevent_thread, (void *) event);
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU event %d listener started\n",
		    modname, event);
    }
#ifdef EXPLORE_COUNTERS
    // this one works
    iep_base[0]  |= 1;  // enable 200Mhz IEP counter see p250

    // this doesnt work yet
    // struct ecap_regs *erp = (struct ecap_regs *)ecap_base;
    // erp->ecctl2   |= ECTRL2_CTRSTP_FREERUN;  // enable 200Mhz eCAP counter
    // ecap_base[0]  |= 1;  // enable 200Mhz eCAP counter
#endif
    return retval;
}

static void *pruevent_thread(void *arg)
{
    int event = (int) arg;
    int event_count = 0;
    do {
	if (prussdrv_pru_wait_event(event, &event_count) < 0)
	    continue;
	    //	    break;
	*(hal_pru->events) = (unsigned) event_count;
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU event %d received count=%d\n",
			modname, event, event_count);
	prussdrv_pru_clear_event(*(hal_pru->prunum) ? PRU1_ARM_INTERRUPT : PRU0_ARM_INTERRUPT);
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
