/********************************************************************
* Description: modmath.c
*   Assorted modulo arithmetic functions for HAL.
*
*   This HAL component has just one function at the moment: closest_dir
*   this component takes a "actual" input and a "desired" input, and
*   has one parameter: the "base" (or, number of elements).  It outputs
*   an up or down bit, depending on which direction has the shortest path
*   from the actual to the desired input.
*
*   Each individual type of block is invoked by a parameter on
*   the insmdo command line.  Each parameter is of the form:
*   <blockname>=<number_of_blocks>
*
*   For example, mod_dir=2 installs two "closest direction" components.
*
*   List of functions currently implemented:
*      mod_dir: tells the direction to the closest neighbor, for sequences that roll over
*
*   Eventually, there should be more functions
*
*********************************************************************
*
* Author: Stephen Wille Padnos  (swpadnos AT sourceforge DOT net)
*       Based on a work by John Kasunich (jmkasunich AT att DOT net)
* License: GPL Version 2
* Created on: 2006/5/18
* System: Linux
*
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/

#include <rtapi.h>		/* RTAPI realtime OS API */
#include <rtapi_app.h>		/* RTAPI realtime module decls */
#include <hal.h>		/* HAL public API decls */

#include <rtapi_math.h>

/* module information */
MODULE_AUTHOR("Stephen Wille Padnos");
MODULE_DESCRIPTION("Modulo math blocks for EMC HAL");
MODULE_LICENSE("GPL");
static int mod_dir = 0;	/* number of mod_dirs */
RTAPI_MP_INT(mod_dir, "Modulo direction blocks");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** These structures contain the runtime data for a single block. */

typedef struct {
    hal_bool_t up;         /* output pin: go up to get to the desired position */
    hal_bool_t down;       /* output pin: go down to get to the desired position */
    hal_bool_t on_target;  /* output pin: go at desired position */
    hal_sint_t actual;     /* input pin: actual position */
    hal_sint_t desired;    /* input pin: desired position */
    hal_sint_t max_num;    /* input/output pin: highest value to allow */
    hal_sint_t min_num;    /* input/output pin: lowest value to allow */
    hal_bool_t wrap;       /* input/output pin: set true if the array is circular, false if linear */
} mod_dir_t;

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_mod_dir(int num);

static void mod_dir_funct(void *arg, long period);


/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n;

    /* connect to the HAL */
    comp_id = hal_init("modmath");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MODMATH: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate and export modulo direction finders */
    if (mod_dir > 0) {
	for (n = 0; n < mod_dir; n++) {
	    if (export_mod_dir(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "MODMATH: ERROR: export_mod_dir(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "MODMATH: installed %d mod-dirs\n", mod_dir);
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                     REALTIME BLOCK FUNCTIONS                         *
************************************************************************/

static void mod_dir_funct(void *arg, long period)
{
    (void)period;
    mod_dir_t *mod = (mod_dir_t *)arg; /* point to block data */

    rtapi_s32 min_num = hal_get_si32(mod->min_num);
    rtapi_s32 max_num = hal_get_si32(mod->max_num);

    rtapi_s32 range = max_num - min_num + 1;
    rtapi_s32 act = hal_get_si32(mod->actual);
    if (act > max_num || act < min_num) {
        act = min_num + ((act-min_num) % (range));
    }
    rtapi_s32 des = hal_get_si32(mod->desired);
    if (des > max_num || des < min_num) {
        des = min_num + ((des-min_num) % (range));
    }

    rtapi_s32 to_go = des-act;

    if ((hal_get_bool(mod->wrap)) && (to_go > range/2)) {
        to_go -= range;
    }
    if ((hal_get_bool(mod->wrap)) && (to_go < -range/2)) {
        to_go += range;
    }

    /* if (desired-actual) >= (actual+(max-min+1)-desired), output "up" */
    if (to_go == 0) {
        hal_set_bool(mod->up,  0);
        hal_set_bool(mod->down, 0);
        hal_set_bool(mod->on_target, 1);
    } else if (to_go > 0 ) {
        hal_set_bool(mod->down, 0);
        hal_set_bool(mod->on_target, 0);
        hal_set_bool(mod->up, 1);
    } else {
        hal_set_bool(mod->up, 0);
        hal_set_bool(mod->on_target, 0);
        hal_set_bool(mod->down, 1);
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

#define CHK(v,ps) do { \
        int _rv = (v); \
        if(0 != _rv) { \
            rtapi_print_msg(RTAPI_MSG_ERR, "MODMATH: ERROR: '%s.%s' pin export failed\n", base, (ps)); \
            return _rv; \
        } \
    } while(0)

static int export_mod_dir(int num)
{
    int retval;
    char base[HAL_NAME_LEN + 1];
    mod_dir_t *moddir;

    /* allocate shared memory for modulo "closest direction finder" */
    moddir = hal_malloc(sizeof(mod_dir_t));
    if (moddir == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "MODMATH: ERROR: hal_malloc() failed\n");
        return -1;
    }
    /* export output pins */
    rtapi_snprintf(base, sizeof(base), "mod-dir.%d", num);
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(moddir->up), 0, "%s.up", base), "up");
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(moddir->down), 0, "%s.down", base), "down");
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(moddir->on_target), 1, "%s.on-target", base), "on-target");

    /* export input pins */
    CHK(hal_pin_new_si32(comp_id, HAL_IN, &(moddir->actual), 0, "%s.actual", base), "actual");
    CHK(hal_pin_new_si32(comp_id, HAL_IN, &(moddir->desired), 0, "%s.desired", base), "desired");

    /* export pins for max and min values */
    CHK(hal_pin_new_si32(comp_id, HAL_IO, &(moddir->min_num), 0, "%s.min-num", base), "min-num");
    CHK(hal_pin_new_si32(comp_id, HAL_IO, &(moddir->max_num), 15, "%s.max-num", base), "max-num");
    CHK(hal_pin_new_bool(comp_id, HAL_IO, &(moddir->wrap), 1, "%s.wrap", base), "wrap");

    /* export function */
    retval = hal_export_funct(base, mod_dir_funct, moddir, 1, 0, comp_id);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "MODMATH: ERROR: '%s' funct export failed\n", base);
        return -1;
    }
    return 0;
}
