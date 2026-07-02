/********************************************************************
* Description: debounce.c
*   Input debouncer/filter for HAL bit signals.
*
*   See the "Users Manual" at emc2/docs/Hal_Introduction.pdf
*
*   This is a HAL component that can be used to debounce or filter
*   noisy digital signals such as those from mechanical switches.
*   It is a realtime component.
*
*   It supports up to 8 groups of debounce filters.  Each group
*   can have any number of filters, subject to total HAL memory
*   limits.  All the filters in a group run at the same sample
*   rate and have the same delay (which determines the amount of
*   filtering).
*
*   There is one function for each group of filters, called
*   'debounce.G', where G is the group number.  The function
*   needs to be called from a realtime thread, which sets the
*   sample rate for all the filters in that group.
*
*   There is one parameter for each group, called 'debounce.G.delay'.
*   It is an integer, and sets the delay time (in samples) of each
*   filter in the group.
*
*   Each individual filter exports two pins, 'debounce.G.F.in'
*   and 'debounce.G.F.out'.  G is the group, and F is the
*   filter number within the group.
*
*********************************************************************
*
* Author: John Kasunich (jmkasunich AT att DOT net)
* License: GPL Version 2
* Created on: 2004/06/12
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
*
********************************************************************/

#include <rtapi_ctype.h>	/* isspace() */
#include <rtapi.h>		/* RTAPI realtime OS API */
#include <rtapi_app.h>		/* RTAPI realtime module decls */
#include <hal.h>		/* HAL public API decls */

/* module information */
#define MAX_GROUP 8
#define STRINGIZE(x) #x
#define MAX_GROUP_STR STRINGIZE(MAX_GROUP)
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Debounce filter for EMC HAL");
MODULE_LICENSE("GPL");
int cfg[MAX_GROUP] = {0,};
RTAPI_MP_ARRAY_INT(cfg,MAX_GROUP,"Group size for up to "MAX_GROUP_STR" groups");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** uncomment this line to export filter internal state variable */
/* #define EXPORT_STATE */

/** This structure contains the runtime data for a single filter. */

typedef struct {
    hal_bool_t in;      /* pin: input */
    hal_bool_t out;     /* pin: output */
#ifdef EXPORT_STATE
    hal_sint_t state;   /* parameter*: internal state */
#else
    rtapi_s32 state;    /* parameter*: internal state */
#endif
} debounce_t;

/*  *note - this parameter is only exported if EXPORT_STATE is defined */

/** This structure contains the runtime data for a group of filters */

typedef struct {
    int channels;               /* number of channels in group */
    hal_sint_t delay;           /* parameter: delay for this group */
    debounce_t *filter_array;   /* pointer to individual filter data */
} debounce_group_t;

/* ptr to array of debounce_group_t structs in shmem, 1 per group */
static debounce_group_t *group_array;

/* other globals */
static int comp_id;		/* component ID */
static int num_groups;		/* number of filter groups configured */
static int num_filters;		/* number of individual filters */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_filter(int num, debounce_t * addr, int group_num);
static int export_group(int num, debounce_group_t * addr, int group_size);
static void debounce(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_GROUP_SIZE 50

int rtapi_app_main(void)
{
    int retval, n;

    /* count number of groups and filters */
    num_groups = 0;
    num_filters = 0;

    while ((num_groups < MAX_GROUP) && (cfg[num_groups] != 0)) {
        if ((cfg[num_groups] < 1)
            || (cfg[num_groups] > MAX_GROUP_SIZE)) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "DEBOUNCE: ERROR: bad group size '%d'\n", cfg[num_groups]);
            return -1;
        }
        num_filters += cfg[num_groups];
        num_groups++;
    }
    /* OK, now we've counted everything */
    if (num_groups == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "DEBOUNCE: ERROR: no channels configured\n");
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("debounce");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "DEBOUNCE: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for filter group array */
    group_array = hal_malloc(num_groups * sizeof(debounce_group_t));
    if (group_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "DEBOUNCE: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export group data */
    for (n = 0; n < num_groups; n++) {
	/* export all vars */
	retval = export_group(n, &(group_array[n]), cfg[n]);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"DEBOUNCE: ERROR: group %d export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"DEBOUNCE: installed %d groups of debounce filters, %d total\n",
	num_groups, num_filters);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                     REALTIME DEBOUNCE FUNCTION                       *
************************************************************************/

#ifdef EXPORT_STATE
static inline rtapi_s32 get_state(hal_sint_t *s) { return hal_get_si32(*s); }
static inline rtapi_s32 set_state(hal_sint_t *s, rtapi_s32 v) { return hal_set_si32(*s, v); }
#else
static inline rtapi_s32 get_state(rtapi_s32 *s) { return *s; }
static inline rtapi_s32 set_state(rtapi_s32 *s, rtapi_s32 v) { return (*s = v); }
#endif

/** The debounce filter works by incrementing a counter whenever the
    input is true, and decrementing the counter when it is false.
    If the counter decrements to zero, the output is set false and
    the counter ignores further decrements.  If the counter increments
    up to a threshold, the output is set true and the counter ignores
    further increments.  If the counter is between zero and the
    threshold, the output retains its previous state.  The threshold
    determines the amount of filtering - a threshold of 1 does no
    filtering at all, and a threshold of N requires a signal to be
    present for N samples before the output changes state.
*/

/** this function processes an entire group of filters with the
    same threshold. */

static void debounce(void *arg, long period)
{
    (void)period;
    debounce_group_t *group;
    debounce_t *filter;
    int n;

    /* point to filter group */
    group = (debounce_group_t *) arg;
    /* first make sure delay is sane */
    if (hal_get_si32(group->delay) < 0) {
        hal_set_si32(group->delay, 1);
    }
    /* loop thru filters */
    for (n = 0; n < group->channels; n++) {
        /* point at a filter */
        filter = &(group->filter_array[n]);
        /* update this filter */
        if (hal_get_bool(filter->in)) {
            /* input true, is state at threshold? */
            if (get_state(&filter->state) < hal_get_si32(group->delay)) {
                /* no, increment */
                set_state(&filter->state, get_state(&filter->state) + 1);
            } else {
                /* yes, set output */
                hal_set_bool(filter->out, 1);
            }
        } else {
            /* input false, is state at zero? */
            if (get_state(&filter->state) > 0) {
                /* no, decrement */
                set_state(&filter->state, get_state(&filter->state) - 1);
            } else {
                /* yes, clear output */
                hal_set_bool(filter->out, 0);
            }
        }
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_group(int num, debounce_group_t * addr, int group_size)
{
    int n, retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for this filter group */
    addr->filter_array = hal_malloc(group_size * sizeof(debounce_t));
    if (addr->filter_array == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "DEBOUNCE: ERROR: hal_malloc() failed\n");
        return -1;
    }
    /* export param variable for delay */
    retval = hal_param_new_si32(comp_id, HAL_RW, &addr->delay, 5, "debounce.%d.delay", num);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "DEBOUNCE: ERROR: 'debounce.%d.delay' param export failed\n", num);
        return retval;
    }
    /* export function */
    retval = hal_export_functf(debounce, addr, 0, 0, comp_id, "debounce.%d", num);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "DEBOUNCE: ERROR: 'debounce.%d' funct export failed\n", num);
        return -1;
    }
    /* set default parameter values */
    addr->channels = group_size;

    /* loop to export each filter in group */
    for (n = 0; n < group_size; n++) {
        retval = export_filter(n, &(addr->filter_array[n]), num);
        if (retval != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "DEBOUNCE: ERROR: filter %d export failed\n", n);
            return -1;
        }
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_filter(int num, debounce_t * addr, int group_num)
{
    int retval;
    char buf[HAL_NAME_LEN + 1];
    rtapi_snprintf(buf, sizeof(buf), "debounce.%d.%d", group_num, num);

    /* export pin for input */
    retval = hal_pin_new_bool(comp_id, HAL_IN, &addr->in, 0, "%s.in", buf);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "DEBOUNCE: ERROR: '%s.in' pin export failed\n", buf);
        return retval;
    }
    /* export pin for output */
    retval = hal_pin_new_bool(comp_id, HAL_OUT, &addr->out, 0, "%s.out", buf);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "DEBOUNCE: ERROR: '%s.out' pin export failed\n", buf);
        return retval;
    }
#ifdef EXPORT_STATE
    /* export parameter containing internal state */
    retval = hal_param_new_si32(comp_id, HAL_RO, &addr->state, 0, "%s.state", buf);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "DEBOUNCE: ERROR: '%s.state' param export failed\n", buf);
        return retval;
    }
#else
    // Param is init to zero.
    // When no param is exported, we need to be sure too.
    set_state(&addr->state, 0);
#endif
    return 0;
}
