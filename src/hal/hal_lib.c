/** HAL stands for Hardware Abstraction Layer, and is used by EMC to
    transfer realtime data to and from I/O devices and other low-level
    modules.
*/

/** This file, 'hal_lib.c', implements the HAL API, for both user
    space and realtime modules.  It uses the RTAPI and ULAPI #define
    symbols to determine which version to compile.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This library is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU Lesser General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.

*/

#ifdef RTAPI
/* includes for realtime config */
/* Suspect only very early kernels are missing the basic string functions.
   To be sure, see what has been implimented by looking in linux/string.h
   and {linux_src_dir}/lib/string.c */
#include <linux/string.h>
#ifdef strcmp
/* some kernels don't have strcmp */
static int strcmp(const char *cs, const char *ct)
{
    signed char __res;
    while (1) {
	if ((__res = *cs - *ct++) != 0 || !*cs++) {
	    break;
	}
    }
    return __res;
}
#endif

#include "rtapi_app.h"
#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Hardware Abstraction Layer for EMC");
#endif /* MODULE */
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* RTAPI */

#ifdef ULAPI
#include <string.h>		/* strcmp */
#endif

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))*/
#endif

char *hal_shmem_base = 0;
hal_data_t *hal_data = 0;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/* These functions are used internally by this file.  The code is at
   the end of the file.  */

/** init_hal_data() initializes the entire HAL data structure, only
    if the structure has not already been initialized.  (The init
    is done by the first HAL component to be loaded.
*/
static void init_hal_data(void);

/** The 'shmalloc_xx()' functions allocate blocks of shared memory.
    Each function allocates a block that is 'size' bytes long.
    If 'size' is 3 or more, the block is aligned on a 4 byte
    boundary.  If 'size' is 2, it is aligned on a 2 byte boundary,
    and if 'size' is 1, it is unaligned.
    These functions do not test a mutex - they are called from
    within the hal library by code that already has the mutex.
    (The public function 'hal_malloc()' is a wrapper that gets the
    mutex and then calls 'shmalloc_up()'.)
    The only difference between the two functions is the location
    of the allocated memory.  'shmalloc_up()' allocates from the
    base of shared memory and works upward, while 'shmalloc_dn()'
    starts at the top and works down.
    This is done to improve realtime performance.  'shmalloc_up()'
    is used to allocate data that will be accessed by realtime
    code, while 'shmalloc_dn()' is used to allocate the much
    larger structures that are accessed only occaisionally during
    init.  This groups all the realtime data together, inproving
    cache performance.
*/
static void *shmalloc_up(long int size);
static void *shmalloc_dn(long int size);

/** The alloc_xxx_struct() functions allocate a structure of the
    appropriate type and return a pointer to it, or 0 if they fail.
    They attempt to re-use freed structs first, if none are
    available, then they call hal_malloc() to create a new one.
    The free_xxx_struct() functions add the structure at 'p' to
    the appropriate free list, for potential re-use later.
    All of these functions assume that the caller has already
    grabbed the hal_data mutex.
*/
static hal_comp_t *alloc_comp_struct(void);
static hal_pin_t *alloc_pin_struct(void);
static hal_sig_t *alloc_sig_struct(void);
static hal_param_t *alloc_param_struct(void);
#ifdef RTAPI
static hal_funct_t *alloc_funct_struct(void);
#endif /* RTAPI */
static hal_funct_entry_t *alloc_funct_entry_struct(void);
#ifdef RTAPI
static hal_thread_t *alloc_thread_struct(void);
#endif /* RTAPI */

static void free_comp_struct(hal_comp_t * comp);
static void unlink_pin(hal_pin_t * pin);
static void free_pin_struct(hal_pin_t * pin);
static void free_sig_struct(hal_sig_t * sig);
static void free_param_struct(hal_param_t * param);
#ifdef RTAPI
static void free_funct_struct(hal_funct_t * funct);
#endif /* RTAPI */
static void free_funct_entry_struct(hal_funct_entry_t * funct_entry);
#ifdef RTAPI
static void free_thread_struct(hal_thread_t * thread);
#endif /* RTAPI */

#ifdef RTAPI
/** 'thread_task()' is a function that is invoked as a realtime task.
    It implements a thread, by running down the thread's function list
    and calling each function in turn.
*/
static void thread_task(void *arg);
#endif /* RTAPI */

/***********************************************************************
*                  PUBLIC (API) FUNCTION CODE                          *
************************************************************************/

int hal_init(char *name)
{
    int comp_id, mem_id, retval;
    void *mem;
    char rtapi_name[RTAPI_NAME_LEN + 1];
    char hal_name[HAL_NAME_LEN + 1];
    hal_comp_t *comp;

    if (name == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: no component name\n");
	return HAL_INVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: initializing component '%s'\n",
	name);
    /* copy name to local vars, truncating if needed */
    rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_%s", name);
    rtapi_snprintf(hal_name, HAL_NAME_LEN, "%s", name);
    /* do RTAPI init */
    comp_id = rtapi_init(rtapi_name);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: rtapi init failed\n");
	return HAL_FAIL;
    }
    /* get HAL shared memory block from RTAPI */
    mem_id = rtapi_shmem_new(HAL_KEY, comp_id, HAL_SIZE);
    if (mem_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: could not open shared memory\n");
	rtapi_exit(comp_id);
	return HAL_FAIL;
    }
    /* get address of shared memory area */
    retval = rtapi_shmem_getptr(mem_id, &mem);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: could not access shared memory\n");
	rtapi_exit(comp_id);
	return HAL_FAIL;
    }
    /* set up internal pointers to shared mem and data structure */
    hal_shmem_base = (char *) mem;
    hal_data = (hal_data_t *) mem;
    /* perform a global init if needed */
    init_hal_data();
    /* get mutex before manipulating the shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* make sure name is unique in the system */
    if (halpr_find_comp_by_name(hal_name) != 0) {
	/* a component with this name already exists */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: duplicate component name '%s'\n", hal_name);
	rtapi_exit(comp_id);
	return HAL_FAIL;
    }
    /* allocate a new component structure */
    comp = alloc_comp_struct();
    if (comp == 0) {
	/* couldn't allocate structure */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for component '%s'\n", hal_name);
	rtapi_exit(comp_id);
	return HAL_NOMEM;
    }
    /* initialize the structure */
    comp->comp_id = comp_id;
    comp->mem_id = mem_id;
#ifdef RTAPI
    comp->type = 1;
#else /* ULAPI */
    comp->type = 0;
#endif
    comp->shmem_base = hal_shmem_base;
    rtapi_snprintf(comp->name, HAL_NAME_LEN, "%s", hal_name);
    /* insert new structure at head of list */
    comp->next_ptr = hal_data->comp_list_ptr;
    hal_data->comp_list_ptr = SHMOFF(comp);
    /* done with list, release mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: component '%s' initialized, ID = %02d\n", hal_name, comp_id);
    return comp_id;
}

int hal_exit(int comp_id)
{
    int *prev, next, mem_id;
    hal_comp_t *comp;
    char name[HAL_NAME_LEN + 1];

    if (hal_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: exit called before init\n");
	return HAL_INVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: removing component %02d\n", comp_id);
    /* grab mutex before manipulating list */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search component list for 'comp_id' */
    prev = &(hal_data->comp_list_ptr);
    next = *prev;
    if (next == 0) {
	/* list is empty - should never happen, but... */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_INVAL;
    }
    comp = SHMPTR(next);
    while (comp->comp_id != comp_id) {
	/* not a match, try the next one */
	prev = &(comp->next_ptr);
	next = *prev;
	if (next == 0) {
	    /* reached end of list without finding component */
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: component %d not found\n", comp_id);
	    return HAL_INVAL;
	}
	comp = SHMPTR(next);
    }
    /* found our component, unlink it from the list */
    *prev = comp->next_ptr;
    /* save shmem ID and component name for later */
    mem_id = comp->mem_id;
    rtapi_snprintf(name, HAL_NAME_LEN, "%s", comp->name);
    /* get rid of the component */
    free_comp_struct(comp);
    /* release mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    /* release RTAPI resources */
    rtapi_shmem_delete(mem_id, comp_id);
    rtapi_exit(comp_id);
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: component %02d removed, name = '%s'\n", comp_id, name);
    return HAL_SUCCESS;
}

void *hal_malloc(long int size)
{
    void *retval;

    /* get the mutex */
    rtapi_mutex_get(&(hal_data->mutex));
    /* allocate memory */
    retval = shmalloc_up(size);
    /* release the mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    /* check return value */
    if (retval == 0) {
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "HAL: hal_malloc() can't allocate %ld bytes\n", size);
    }
    return retval;
}

/***********************************************************************
*                        "PIN" FUNCTIONS                               *
************************************************************************/

/* wrapper functs for typed pins - these call the generic funct below */

int hal_pin_bit_new(char *name, hal_dir_t dir,
    hal_bit_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_BIT, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_float_new(char *name, hal_dir_t dir,
    hal_float_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_FLOAT, dir, (void **) data_ptr_addr,
	comp_id);
}

int hal_pin_u8_new(char *name, hal_dir_t dir,
    hal_u8_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_U8, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_s8_new(char *name, hal_dir_t dir,
    hal_s8_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_S8, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_u16_new(char *name, hal_dir_t dir,
    hal_u16_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_U16, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_s16_new(char *name, hal_dir_t dir,
    hal_s16_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_S16, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_u32_new(char *name, hal_dir_t dir,
    hal_u32_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_U32, dir, (void **) data_ptr_addr, comp_id);
}

int hal_pin_s32_new(char *name, hal_dir_t dir,
    hal_s32_t ** data_ptr_addr, int comp_id)
{
    return hal_pin_new(name, HAL_S32, dir, (void **) data_ptr_addr, comp_id);
}

/* this is a generic function that does the majority of the work. */

int hal_pin_new(char *name, hal_type_t type, hal_dir_t dir,
    void **data_ptr_addr, int comp_id)
{
    int *prev, next, cmp;
    hal_pin_t *new, *ptr;
    hal_comp_t *comp;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating pin '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* validate comp_id */
    comp = halpr_find_comp_by_id(comp_id);
    if (comp == 0) {
	/* bad comp_id */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return HAL_INVAL;
    }
    /* allocate a new variable structure */
    new = alloc_pin_struct();
    if (new == 0) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for pin '%s'\n", name);
	return HAL_NOMEM;
    }
    /* initialize the structure */
    new->data_ptr_addr = SHMOFF(data_ptr_addr);
    new->owner_ptr = SHMOFF(comp);
    new->type = type;
    new->dir = dir;
    new->signal = 0;
    new->dummysig = 0;
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);
    /* make 'data_ptr' point to dummy signal */
    *data_ptr_addr = comp->shmem_base + SHMOFF(&(new->dummysig));
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_pin_struct(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate variable '%s'\n", name);
	    return HAL_INVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

/***********************************************************************
*                      "SIGNAL" FUNCTIONS                              *
************************************************************************/

int hal_signal_new(char *name, hal_type_t type)
{

    int *prev, next, cmp;
    hal_sig_t *new, *ptr;
    void *data_addr;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating signal '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* allocate memory for the signal value */
    switch (type) {
    case HAL_BIT:
    case HAL_S8:
    case HAL_U8:
	data_addr = shmalloc_up(1);
	break;
    case HAL_S16:
    case HAL_U16:
	data_addr = shmalloc_up(2);
	break;
    case HAL_S32:
    case HAL_U32:
    case HAL_FLOAT:
	data_addr = shmalloc_up(4);
	break;
    default:
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: illegal signal type %d'\n", type);
	return HAL_INVAL;
	break;
    }
    /* allocate a new signal structure */
    new = alloc_sig_struct();
    if ((new == 0) || (data_addr == 0)) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for signal '%s'\n", name);
	return HAL_NOMEM;
    }
    /* initialize the signal value */
    switch (type) {
    case HAL_BIT:
    case HAL_S8:
    case HAL_U8:
	*((char *) data_addr) = 0;
	break;
    case HAL_S16:
    case HAL_U16:
	*((short *) data_addr) = 0;
	break;
    case HAL_S32:
    case HAL_U32:
	*((long *) data_addr) = 0;
    case HAL_FLOAT:
	*((float *) data_addr) = 0.0;
	break;
    default:
	break;
    }
    /* initialize the structure */
    new->data_ptr = SHMOFF(data_addr);
    new->type = type;
    new->readers = 0;
    new->writers = 0;
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->sig_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_sig_struct(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate signal '%s'\n", name);
	    return HAL_INVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

int hal_signal_delete(char *name)
{
    hal_sig_t *sig;
    int *prev, next;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: deleting signal '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search for the signal */
    prev = &(hal_data->sig_list_ptr);
    next = *prev;
    while (next != 0) {
	sig = SHMPTR(next);
	if (strcmp(sig->name, name) == 0) {
	    /* this is the right signal, unlink from list */
	    *prev = sig->next_ptr;
	    /* and delete it */
	    free_sig_struct(sig);
	    /* done */
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	/* no match, try the next one */
	prev = &(sig->next_ptr);
	next = *prev;
    }
    /* if we get here, we didn't find a match */
    rtapi_mutex_give(&(hal_data->mutex));
    rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: signal '%s' not found\n",
	name);
    return HAL_INVAL;
}

int hal_link(char *pin_name, char *sig_name)
{
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_comp_t *comp;
    void **data_ptr_addr, *data_addr;

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: linking pin '%s' to '%s'\n", pin_name, sig_name);
    /* get mutex before accessing data structures */
    rtapi_mutex_get(&(hal_data->mutex));
    /* make sure we were given a pin name */
    if (pin_name == 0) {
	/* no pin name supplied */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: pin name not given\n");
	return HAL_INVAL;
    }
    /* locate the pin */
    pin = halpr_find_pin_by_name(pin_name);
    if (pin == 0) {
	/* not found */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin '%s' not found\n", pin_name);
	return HAL_INVAL;
    }
    /* found the pin, now check the signal */
    if (sig_name == 0) {
	/* no signal name supplied, so we unlink pin */
	unlink_pin(pin);
	/* done, release the mutex and return */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_SUCCESS;
    }
    /* we have a signal name, search for it */
    sig = halpr_find_sig_by_name(sig_name);
    if (sig == 0) {
	/* not found */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' not found\n", sig_name);
	return HAL_INVAL;
    }
    /* found both pin and signal, check types */
    if (pin->type != sig->type) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: type mismatch '%s' <- '%s'\n", pin_name, sig_name);
	return HAL_INVAL;
    }
    /* are we linking write_only pin to sig that already has writer? */
    if ((pin->dir == HAL_WR) && (sig->writers > 0)) {
	/* yes, can't do that */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' already has writer(s)\n", sig_name);
	return HAL_INVAL;
    }
    /* everything is OK, break any old link */
    unlink_pin(pin);
    /* and make the new link */
    data_ptr_addr = SHMPTR(pin->data_ptr_addr);
    comp = SHMPTR(pin->owner_ptr);
    data_addr = comp->shmem_base + sig->data_ptr;
    *data_ptr_addr = data_addr;
    /* update the signal's reader/writer counts */
    if ((pin->dir & HAL_RD) != 0) {
	sig->readers++;
    }
    if ((pin->dir & HAL_WR) != 0) {
	sig->writers++;
    }
    /* and update the pin */
    pin->signal = SHMOFF(sig);
    /* done, release the mutex and return */
    rtapi_mutex_give(&(hal_data->mutex));
    return HAL_SUCCESS;
}

/***********************************************************************
*                       "PARAM" FUNCTIONS                              *
************************************************************************/

/* wrapper functs for typed params - these call the generic funct below */

int hal_param_bit_new(char *name, hal_bit_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_BIT, (void *) data_addr, comp_id);
}

int hal_param_float_new(char *name, hal_float_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_FLOAT, (void *) data_addr, comp_id);
}

int hal_param_u8_new(char *name, hal_u8_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_U8, (void *) data_addr, comp_id);
}

int hal_param_s8_new(char *name, hal_s8_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_S8, (void *) data_addr, comp_id);
}

int hal_param_u16_new(char *name, hal_u16_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_U16, (void *) data_addr, comp_id);
}

int hal_param_s16_new(char *name, hal_s16_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_S16, (void *) data_addr, comp_id);
}

int hal_param_u32_new(char *name, hal_u32_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_U32, (void *) data_addr, comp_id);
}

int hal_param_s32_new(char *name, hal_s32_t * data_addr, int comp_id)
{
    return hal_param_new(name, HAL_S32, (void *) data_addr, comp_id);
}

/* this is a generic function that does the majority of the work. */

int hal_param_new(char *name, hal_type_t type, void *data_addr, int comp_id)
{
    int *prev, next, cmp;
    hal_param_t *new, *ptr;
    hal_comp_t *comp;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating parameter '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* validate comp_id */
    comp = halpr_find_comp_by_id(comp_id);
    if (comp == 0) {
	/* bad comp_id */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", comp_id);
	return HAL_INVAL;
    }
    /* allocate a new parameter structure */
    new = alloc_param_struct();
    if (new == 0) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for parameter '%s'\n", name);
	return HAL_NOMEM;
    }
    /* initialize the structure */
    new->owner_ptr = SHMOFF(comp);
    new->data_ptr = SHMOFF(data_addr);
    new->type = type;
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	ptr = SHMPTR(next);
	cmp = strcmp(ptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_param_struct(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate parameter '%s'\n", name);
	    return HAL_INVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(ptr->next_ptr);
	next = *prev;
    }
}

/* wrapper functs for typed params - these call the generic funct below */

int hal_param_bit_set(char *name, int value)
{
    return hal_param_set(name, HAL_BIT, &value);
}

int hal_param_float_set(char *name, float value)
{
    return hal_param_set(name, HAL_FLOAT, &value);
}

int hal_param_u8_set(char *name, unsigned char value)
{
    return hal_param_set(name, HAL_U8, &value);
}

int hal_param_s8_set(char *name, signed char value)
{
    return hal_param_set(name, HAL_S8, &value);
}

int hal_param_u16_set(char *name, unsigned short value)
{
    return hal_param_set(name, HAL_U16, &value);
}

int hal_param_s16_set(char *name, signed short value)
{
    return hal_param_set(name, HAL_S16, &value);
}

int hal_param_u32_set(char *name, unsigned long value)
{
    return hal_param_set(name, HAL_U32, &value);
}

int hal_param_s32_set(char *name, signed long value)
{
    return hal_param_set(name, HAL_S32, &value);
}

/* this is a generic function that does the majority of the work */

int hal_param_set(char *name, hal_type_t type, void *value_addr)
{
    hal_param_t *param;
    void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: setting parameter '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));

    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param == 0) {
	/* parameter not found */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: parameter '%s' not found\n", name);
	return HAL_INVAL;
    }
    /* found it, is type compatible? */
    if (param->type != type) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: type mismatch setting param '%s'\n", name);
	return HAL_INVAL;
    }
    /* everything is OK, set the value */
    d_ptr = SHMPTR(param->data_ptr);
    switch (param->type) {
    case HAL_BIT:
	if (*((int *) value_addr) == 0) {
	    *(hal_bit_t *) (d_ptr) = 0;
	} else {
	    *(hal_bit_t *) (d_ptr) = 1;
	}
	break;
    case HAL_FLOAT:
	*((hal_float_t *) (d_ptr)) = *((float *) (value_addr));
	break;
    case HAL_S8:
	*((hal_s8_t *) (d_ptr)) = *((signed char *) (value_addr));
	break;
    case HAL_U8:
	*((hal_u8_t *) (d_ptr)) = *((unsigned char *) (value_addr));
	break;
    case HAL_S16:
	*((hal_s16_t *) (d_ptr)) = *((signed short *) (value_addr));
	break;
    case HAL_U16:
	*((hal_u16_t *) (d_ptr)) = *((unsigned short *) (value_addr));
	break;
    case HAL_S32:
	*((hal_s32_t *) (d_ptr)) = *((signed long *) (value_addr));
	break;
    case HAL_U32:
	*((hal_u32_t *) (d_ptr)) = *((unsigned long *) (value_addr));
	break;
    default:
	/* Shouldn't get here, but just in case... */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: bad type %d setting param\n", param->type);
	return HAL_INVAL;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return HAL_SUCCESS;
}

/***********************************************************************
*                   EXECUTION RELATED FUNCTIONS                        *
************************************************************************/

#ifdef RTAPI

int hal_export_funct(char *name, void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id)
{
    int *prev, next, cmp;
    hal_funct_t *new, *fptr;
    hal_comp_t *comp;

    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* validate comp_id */
    comp = halpr_find_comp_by_id(comp_id);
    if (comp == 0) {
	/* bad comp_id */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_INVAL;
    }
    if (comp->type == 0) {
	/* not a realtime component */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_INVAL;
    }
    /* allocate a new function structure */
    new = alloc_funct_struct();
    if (new == 0) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_NOMEM;
    }
    /* initialize the structure */
    new->uses_fp = uses_fp;
    new->owner_ptr = SHMOFF(comp);
    new->reentrant = reentrant;
    new->users = 0;
    new->arg = arg;
    new->funct = funct;
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);
    /* search list for 'name' and insert new structure */
    prev = &(hal_data->funct_list_ptr);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	fptr = SHMPTR(next);
	cmp = strcmp(fptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_funct_struct(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_INVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(fptr->next_ptr);
	next = *prev;
    }
}

int hal_create_thread(char *name, unsigned long period_nsec,
    int uses_fp, int comp_id)
{
    int next, cmp, prev_priority;
    int retval, n;
    hal_thread_t *new, *tptr;
    hal_comp_t *comp;
    long curr_period;
    char buf[HAL_NAME_LEN + 1];

    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* validate comp_id */
    comp = halpr_find_comp_by_id(comp_id);
    if (comp == 0) {
	/* bad comp_id */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_INVAL;
    }
    if (comp->type == 0) {
	/* not a realtime component */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_INVAL;
    }
    /* make sure name is unique on thread list */
    next = hal_data->thread_list_ptr;
    while (next != 0) {
	tptr = SHMPTR(next);
	cmp = strcmp(tptr->name, name);
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_INVAL;
	}
	/* didn't find it yet, look at next one */
	next = tptr->next_ptr;
    }
    /* allocate a new thread structure */
    new = alloc_thread_struct();
    if (new == 0) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_NOMEM;
    }
    /* initialize the structure */
    new->uses_fp = uses_fp;
    new->owner_ptr = SHMOFF(comp);
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);
    /* have to create and start a task to run the thread */
    if (hal_data->thread_list_ptr == 0) {
	/* this is the first thread created */
	/* is timer started? if so, what period? */
	curr_period = rtapi_clock_set_period(0);
	if (curr_period == 0) {
	    /* not running, start it */
	    curr_period = rtapi_clock_set_period(period_nsec);
	    if (curr_period < 0) {
		rtapi_mutex_give(&(hal_data->mutex));
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL_LIB: ERROR: clock_set_period returned %ld\n",
		    curr_period);
		return HAL_FAIL;
	    }
	}
	/* make sure period <= desired period (allow 1% roundoff error) */
	if (curr_period > (period_nsec + (period_nsec / 100))) {
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL_LIB: ERROR: clock period too long: %ld\n", curr_period);
	    return HAL_FAIL;
	}
	hal_data->base_period = curr_period;
	/* reserve the highest priority (maybe for a watchdog?) */
	prev_priority = rtapi_prio_highest();
    } else {
	/* there are other threads */
	tptr = SHMPTR(hal_data->thread_list_ptr);
	prev_priority = tptr->priority;
    }
    /* make period an integer multiple of the timer period */
    n = (period_nsec + hal_data->base_period / 2) / hal_data->base_period;
    new->period = hal_data->base_period * n;
    /* make priority one lower than previous */
    new->priority = rtapi_prio_next_lower(prev_priority);
    /* create task */
    retval = rtapi_task_new(thread_task, new, new->priority,
	comp_id, HAL_STACKSIZE, uses_fp);
    if (retval < 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: could not create task for thread %s\n", name);
	return HAL_FAIL;
    }
    new->task_id = retval;
    /* start task */
    retval = rtapi_task_start(new->task_id, new->period);
    if (retval < 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: could not start task for thread %s\n", name);
	return HAL_FAIL;
    }
    /* insert new structure at head of list */
    new->next_ptr = hal_data->thread_list_ptr;
    hal_data->thread_list_ptr = SHMOFF(new);
    /* done, release mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    /* init time logging variables */
    new->runtime = 0;
    new->maxtime = 0;
    /* create a parameter with the thread's runtime in it */
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.time", name);
    hal_param_s32_new(buf, &(new->runtime), comp_id);
    /* create a parameter with the thread's maximum runtime in it */
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.tmax", name);
    hal_param_s32_new(buf, &(new->maxtime), comp_id);
    return HAL_SUCCESS;
}

#endif /* RTAPI */

int hal_add_funct_to_thread(char *funct_name, char *thread_name)
{
    int *prev, next, cmp;
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_funct_entry_t *funct_entry;

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: adding function '%s' to thread '%s'\n",
	funct_name, thread_name);
    /* get mutex before accessing data structures */
    rtapi_mutex_get(&(hal_data->mutex));
    /* make sure we were given a function name */
    if (funct_name == 0) {
	/* no name supplied */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing function name\n");
	return HAL_INVAL;
    }
    /* make sure we were given a thread name */
    if (thread_name == 0) {
	/* no name supplied */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing thread name\n");
	return HAL_INVAL;
    }
    /* search function list for the function */
    next = hal_data->funct_list_ptr;
    do {
	if (next == 0) {
	    /* reached end of list, function not found */
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: function '%s' not found\n", funct_name);
	    return HAL_INVAL;
	}
	funct = SHMPTR(next);
	next = funct->next_ptr;
	cmp = strcmp(funct->name, funct_name);
    } while (cmp != 0);
    /* found the function, is it available? */
    if ((funct->users > 0) && (funct->reentrant == 0)) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' is not reentrant\n", funct_name);
	return HAL_INVAL;
    }
    /* search thread list for thread_name */
    next = hal_data->thread_list_ptr;
    do {
	if (next == 0) {
	    /* reached end of list, thread not found */
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: thread '%s' not found\n", thread_name);
	    return HAL_INVAL;
	}
	thread = SHMPTR(next);
	next = thread->next_ptr;
	cmp = strcmp(thread->name, thread_name);
    } while (cmp != 0);
    /* ok, we have thread and function, are they compatible? */
    if ((funct->uses_fp) && (!thread->uses_fp)) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' needs FP\n", funct_name);
	return HAL_INVAL;
    }
    /* find end of function entry list */
    prev = &(thread->funct_list);
    while (*prev != 0) {
	funct_entry = SHMPTR(*prev);
	prev = &(funct_entry->next_ptr);
    }
    /* allocate a funct entry structure */
    funct_entry = alloc_funct_entry_struct();
    if (funct_entry == 0) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	return HAL_NOMEM;
    }
    /* init struct contents */
    funct_entry->next_ptr = 0;
    funct_entry->funct_ptr = SHMOFF(funct);
    funct_entry->arg = funct->arg;
    funct_entry->funct = funct->funct;
    /* add the entry to the list */
    *prev = SHMOFF(funct_entry);
    /* update the function usage count */
    funct->users++;
    rtapi_mutex_give(&(hal_data->mutex));
    return HAL_SUCCESS;
}

int hal_del_funct_from_thread(char *funct_name, char *thread_name)
{
    int *prev, next, cmp;
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_funct_entry_t *funct_entry;

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: removing function '%s' from thread '%s'\n",
	funct_name, thread_name);
    /* get mutex before accessing data structures */
    rtapi_mutex_get(&(hal_data->mutex));
    /* make sure we were given a function name */
    if (funct_name == 0) {
	/* no name supplied */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing function name\n");
	return HAL_INVAL;
    }
    /* make sure we were given a thread name */
    if (thread_name == 0) {
	/* no name supplied */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: missing thread name\n");
	return HAL_INVAL;
    }
    /* search function list for the function */
    next = hal_data->funct_list_ptr;
    do {
	if (next == 0) {
	    /* reached end of list, function not found */
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: function '%s' not found\n", funct_name);
	    return HAL_INVAL;
	}
	funct = SHMPTR(next);
	next = funct->next_ptr;
	cmp = strcmp(funct->name, funct_name);
    } while (cmp != 0);
    /* found the function, is it in use? */
    if (funct->users == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' is not in use\n", funct_name);
	return HAL_INVAL;
    }
    /* search thread list for thread_name */
    next = hal_data->thread_list_ptr;
    do {
	if (next == 0) {
	    /* reached end of list, thread not found */
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: thread '%s' not found\n", thread_name);
	    return HAL_INVAL;
	}
	thread = SHMPTR(next);
	next = thread->next_ptr;
	cmp = strcmp(thread->name, thread_name);
    } while (cmp != 0);
    /* ok, we have thread and function, does thread use funct? */
    prev = &(thread->funct_list);
    next = *prev;
    while (1) {
	if (next == 0) {
	    /* reached end of list, funct not found */
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: thread '%s' doesn't use %s\n", thread_name,
		funct_name);
	    return HAL_INVAL;
	}
	funct_entry = SHMPTR(next);
	if (SHMPTR(funct_entry->funct_ptr) == funct) {
	    /* this funct entry points to our funct, unlink */
	    *prev = funct_entry->next_ptr;
	    /* and delete it */
	    free_funct_entry_struct(funct_entry);
	    /* done */
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	/* try next one */
	prev = &(funct_entry->next_ptr);
	next = *prev;
    }
}

int hal_start_threads(void)
{
    /* a trivial function for a change! */
    hal_data->threads_running = 1;
    return HAL_SUCCESS;
}

int hal_stop_threads(void)
{
    /* wow, two in a row! */
    hal_data->threads_running = 0;
    return HAL_SUCCESS;
}

/***********************************************************************
*                    PRIVATE FUNCTION CODE                             *
************************************************************************/

hal_comp_t *halpr_find_comp_by_name(char *name)
{
    int next;
    hal_comp_t *comp;

    /* search component list for 'name' */
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if (strcmp(comp->name, name) == 0) {
	    /* found a match */
	    return comp;
	}
	/* didn't find it yet, look at next one */
	next = comp->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_pin_t *halpr_find_pin_by_name(char *name)
{
    int next;
    hal_pin_t *pin;

    /* search pin list for 'name' */
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if (strcmp(pin->name, name) == 0) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_sig_t *halpr_find_sig_by_name(char *name)
{
    int next;
    hal_sig_t *sig;

    /* search signal list for 'name' */
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if (strcmp(sig->name, name) == 0) {
	    /* found a match */
	    return sig;
	}
	/* didn't find it yet, look at next one */
	next = sig->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_param_t *halpr_find_param_by_name(char *name)
{
    int next;
    hal_param_t *param;

    /* search parameter list for 'name' */
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if (strcmp(param->name, name) == 0) {
	    /* found a match */
	    return param;
	}
	/* didn't find it yet, look at next one */
	next = param->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_thread_t *halpr_find_thread_by_name(char *name)
{
    int next;
    hal_thread_t *thread;

    /* search thread list for 'name' */
    next = hal_data->thread_list_ptr;
    while (next != 0) {
	thread = SHMPTR(next);
	if (strcmp(thread->name, name) == 0) {
	    /* found a match */
	    return thread;
	}
	/* didn't find it yet, look at next one */
	next = thread->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_funct_t *halpr_find_funct_by_name(char *name)
{
    int next;
    hal_funct_t *funct;

    /* search function list for 'name' */
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	funct = SHMPTR(next);
	if (strcmp(funct->name, name) == 0) {
	    /* found a match */
	    return funct;
	}
	/* didn't find it yet, look at next one */
	next = funct->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_comp_t *halpr_find_comp_by_id(int id)
{
    int next;
    hal_comp_t *comp;

    /* search list for 'comp_id' */
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if (comp->comp_id == id) {
	    /* found a match */
	    return comp;
	}
	/* didn't find it yet, look at next one */
	next = comp->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

hal_pin_t *halpr_find_pin_by_owner(hal_comp_t * owner, hal_pin_t * start)
{
    int owner_ptr, next;
    hal_pin_t *pin;

    /* get offset of 'owner' component */
    owner_ptr = SHMOFF(owner);
    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of pin list */
	next = hal_data->pin_list_ptr;
    } else {
	/* no, start at next pin */
	next = start->next_ptr;
    }
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->owner_ptr == owner_ptr) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

hal_param_t *halpr_find_param_by_owner(hal_comp_t * owner,
    hal_param_t * start)
{
    int owner_ptr, next;
    hal_param_t *param;

    /* get offset of 'owner' component */
    owner_ptr = SHMOFF(owner);
    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of param list */
	next = hal_data->param_list_ptr;
    } else {
	/* no, start at next param */
	next = start->next_ptr;
    }
    while (next != 0) {
	param = SHMPTR(next);
	if (param->owner_ptr == owner_ptr) {
	    /* found a match */
	    return param;
	}
	/* didn't find it yet, look at next one */
	next = param->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

hal_thread_t *halpr_find_thread_by_owner(hal_comp_t * owner,
    hal_thread_t * start)
{
    int owner_ptr, next;
    hal_thread_t *thread;

    /* get offset of 'owner' component */
    owner_ptr = SHMOFF(owner);
    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of thread list */
	next = hal_data->thread_list_ptr;
    } else {
	/* no, start at next thread */
	next = start->next_ptr;
    }
    while (next != 0) {
	thread = SHMPTR(next);
	if (thread->owner_ptr == owner_ptr) {
	    /* found a match */
	    return thread;
	}
	/* didn't find it yet, look at next one */
	next = thread->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

hal_funct_t *halpr_find_funct_by_owner(hal_comp_t * owner,
    hal_funct_t * start)
{
    int owner_ptr, next;
    hal_funct_t *funct;

    /* get offset of 'owner' component */
    owner_ptr = SHMOFF(owner);
    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of function list */
	next = hal_data->funct_list_ptr;
    } else {
	/* no, start at next function */
	next = start->next_ptr;
    }
    while (next != 0) {
	funct = SHMPTR(next);
	if (funct->owner_ptr == owner_ptr) {
	    /* found a match */
	    return funct;
	}
	/* didn't find it yet, look at next one */
	next = funct->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

hal_pin_t *halpr_find_pin_by_sig(hal_sig_t * sig, hal_pin_t * start)
{
    int sig_ptr, next;
    hal_pin_t *pin;

    /* get offset of 'sig' component */
    sig_ptr = SHMOFF(sig);
    /* is this the first call? */
    if (start == 0) {
	/* yes, start at beginning of pin list */
	next = hal_data->pin_list_ptr;
    } else {
	/* no, start at next pin */
	next = start->next_ptr;
    }
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->signal == sig_ptr) {
	    /* found a match */
	    return pin;
	}
	/* didn't find it yet, look at next one */
	next = pin->next_ptr;
    }
    /* if loop terminates, we reached end of list without finding a match */
    return 0;
}

/***********************************************************************
*                     LOCAL FUNCTION CODE                              *
************************************************************************/

#ifdef RTAPI
/* this code runs when the hal_lib module is insmod'ed, to set up
   the HAL shared memory area.
*/
static int hal_lib_module_id = -1;

int rtapi_app_main(void)
{
    int retval;

    hal_lib_module_id = hal_init("hal_lib");
    if (hal_lib_module_id < 0) {
	return -1;
    }
    /* export parameter for available shared memory */
    retval =
	hal_param_s32_new("hal_lib.shmem-avail", &(hal_data->shmem_avail),
	hal_lib_module_id);
    if (retval != 0) {
	return retval;
    }

    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(hal_lib_module_id);
}

/* this is the task function that implements threads in realtime */

static void thread_task(void *arg)
{
    int next_entry;
    hal_thread_t *thread;
    hal_funct_entry_t *funct_entry;
    long long int start_time, end_time;

    thread = arg;
    while (1) {
	start_time = rtapi_get_time();
	if (hal_data->threads_running > 0) {
	    /* run thru function list */
	    next_entry = thread->funct_list;
	    while (next_entry != 0) {
		/* point at function entry */
		funct_entry = SHMPTR(next_entry);
		/* get next entry in list (for use later) */
		next_entry = funct_entry->next_ptr;
		/* call the function */
		funct_entry->funct(funct_entry->arg, thread->period);
	    }
	}
	end_time = rtapi_get_time();
	thread->runtime = (long int) (end_time - start_time);
	if (thread->runtime > thread->maxtime) {
	    thread->maxtime = thread->runtime;
	}
	/* wait until next period */
	rtapi_wait();
    }
}

#endif /* RTAPI */

/* see the declarations of these functions (near top of file) for
   a description of what they do.
*/

static void init_hal_data(void)
{
    /* has the block already been initialized? */
    if (hal_data->magic == HAL_MAGIC) {
	/* yes, nothing to do */
	return;
    }
    /* no, we need to init it, grab the mutex unconditionally */
    rtapi_mutex_try(&(hal_data->mutex));
    /* set magic number so nobody else init's the block */
    hal_data->magic = HAL_MAGIC;
    /* initialize everything */
    hal_data->comp_list_ptr = 0;
    hal_data->pin_list_ptr = 0;
    hal_data->sig_list_ptr = 0;
    hal_data->param_list_ptr = 0;
    hal_data->funct_list_ptr = 0;
    hal_data->thread_list_ptr = 0;
    hal_data->base_period = 0;
    hal_data->threads_running = 0;
    hal_data->comp_free_ptr = 0;
    hal_data->pin_free_ptr = 0;
    hal_data->sig_free_ptr = 0;
    hal_data->param_free_ptr = 0;
    hal_data->funct_free_ptr = 0;
    hal_data->funct_entry_free_ptr = 0;
    hal_data->thread_free_ptr = 0;
    /* set up for shmalloc_xx() */
    hal_data->shmem_bot = sizeof(hal_data_t);
    hal_data->shmem_top = HAL_SIZE;
    /* done, release mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    return;
}

static void *shmalloc_up(long int size)
{
    long int tmp_bot;
    void *retval;

    /* deal with alignment requirements */
    tmp_bot = hal_data->shmem_bot;
    if (size >= 3) {
	/* align on 4 byte boundary */
	tmp_bot = (tmp_bot + 3) & (~3);
    } else if (size == 2) {
	/* align on 2 byte boundary */
	tmp_bot = (tmp_bot + 1) & (~1);
    }
    /* is there enough memory available? */
    if ((hal_data->shmem_top - tmp_bot) < size) {
	/* no */
	return 0;
    }
    /* memory is available, allocate it */
    retval = SHMPTR(tmp_bot);
    hal_data->shmem_bot = tmp_bot + size;
    hal_data->shmem_avail = hal_data->shmem_top - hal_data->shmem_bot;
    return retval;
}

static void *shmalloc_dn(long int size)
{
    long int tmp_top;
    void *retval;

    /* tentatively allocate memory */
    tmp_top = hal_data->shmem_top - size;
    /* deal with alignment requirements */
    if (size >= 3) {
	/* align on 4 byte boundary */
	tmp_top &= (~3);
    } else if (size == 2) {
	/* align on 2 byte boundary */
	tmp_top &= (~1);
    }
    /* is there enough memory available? */
    if (tmp_top < hal_data->shmem_bot) {
	/* no */
	return 0;
    }
    /* memory is available, allocate it */
    retval = SHMPTR(tmp_top);
    hal_data->shmem_top = tmp_top;
    hal_data->shmem_avail = hal_data->shmem_top - hal_data->shmem_bot;
    return retval;
}

static hal_comp_t *alloc_comp_struct(void)
{
    hal_comp_t *p;

    /* check the free list */
    if (hal_data->comp_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->comp_free_ptr);
	/* unlink it from the free list */
	hal_data->comp_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_comp_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->comp_id = 0;
	p->mem_id = 0;
	p->type = 0;
	p->shmem_base = 0;
	p->name[0] = '\0';
    }
    return p;
}

static hal_pin_t *alloc_pin_struct(void)
{
    hal_pin_t *p;

    /* check the free list */
    if (hal_data->pin_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->pin_free_ptr);
	/* unlink it from the free list */
	hal_data->pin_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_pin_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr_addr = 0;
	p->owner_ptr = 0;
	p->type = 0;
	p->dir = 0;
	p->signal = 0;
	p->dummysig = 0;
	p->name[0] = '\0';
    }
    return p;
}

static hal_sig_t *alloc_sig_struct(void)
{
    hal_sig_t *p;

    /* check the free list */
    if (hal_data->sig_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->sig_free_ptr);
	/* unlink it from the free list */
	hal_data->sig_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_sig_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr = 0;
	p->type = 0;
	p->readers = 0;
	p->writers = 0;
	p->name[0] = '\0';
    }
    return p;
}

static hal_param_t *alloc_param_struct(void)
{
    hal_param_t *p;

    /* check the free list */
    if (hal_data->param_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->param_free_ptr);
	/* unlink it from the free list */
	hal_data->param_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_param_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->data_ptr = 0;
	p->owner_ptr = 0;
	p->type = 0;
	p->name[0] = '\0';
    }
    return p;
}

#ifdef RTAPI
static hal_funct_t *alloc_funct_struct(void)
{
    hal_funct_t *p;

    /* check the free list */
    if (hal_data->funct_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->funct_free_ptr);
	/* unlink it from the free list */
	hal_data->funct_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_funct_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->uses_fp = 0;
	p->owner_ptr = 0;
	p->reentrant = 0;
	p->users = 0;
	p->arg = 0;
	p->funct = 0;
	p->name[0] = '\0';
    }
    return p;
}
#endif /* RTAPI */

static hal_funct_entry_t *alloc_funct_entry_struct(void)
{
    hal_funct_entry_t *p;

    /* check the free list */
    if (hal_data->funct_entry_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->funct_entry_free_ptr);
	/* unlink it from the free list */
	hal_data->funct_entry_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_funct_entry_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->funct_ptr = 0;
	p->arg = 0;
	p->funct = 0;
    }
    return p;
}

#ifdef RTAPI
static hal_thread_t *alloc_thread_struct(void)
{
    hal_thread_t *p;

    /* check the free list */
    if (hal_data->thread_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->thread_free_ptr);
	/* unlink it from the free list */
	hal_data->thread_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_thread_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->uses_fp = 0;
	p->period = 0;
	p->priority = 0;
	p->owner_ptr = 0;
	p->task_id = 0;
	p->funct_list = 0;
	p->name[0] = '\0';
    }
    return p;
}
#endif /* RTAPI */

static void free_comp_struct(hal_comp_t * comp)
{
    int *prev, next;
#ifdef RTAPI
    hal_thread_t *thread;
    hal_funct_t *funct;
#endif /* RTAPI */
    hal_pin_t *pin;
    hal_param_t *param;

    /* can't delete the component until we delete its "stuff" */
    /* need to check for functs and threads only if a realtime component */
#ifdef RTAPI
    /* search the thread list for this component's threads */
    prev = &(hal_data->thread_list_ptr);
    next = *prev;
    while (next != 0) {
	thread = SHMPTR(next);
	if (SHMPTR(thread->owner_ptr) == comp) {
	    /* this thread belongs to our component, unlink from list */
	    *prev = thread->next_ptr;
	    /* and delete it */
	    free_thread_struct(thread);
	} else {
	    /* no match, try the next one */
	    prev = &(thread->next_ptr);
	}
	next = *prev;
    }
    /* search the function list for this component's functs */
    prev = &(hal_data->funct_list_ptr);
    next = *prev;
    while (next != 0) {
	funct = SHMPTR(next);
	if (SHMPTR(funct->owner_ptr) == comp) {
	    /* this function belongs to our component, unlink from list */
	    *prev = funct->next_ptr;
	    /* and delete it */
	    free_funct_struct(funct);
	} else {
	    /* no match, try the next one */
	    prev = &(funct->next_ptr);
	}
	next = *prev;
    }
#endif /* RTAPI */
    /* search the pin list for this component's pins */
    prev = &(hal_data->pin_list_ptr);
    next = *prev;
    while (next != 0) {
	pin = SHMPTR(next);
	if (SHMPTR(pin->owner_ptr) == comp) {
	    /* this pin belongs to our component, unlink from list */
	    *prev = pin->next_ptr;
	    /* and delete it */
	    free_pin_struct(pin);
	} else {
	    /* no match, try the next one */
	    prev = &(pin->next_ptr);
	}
	next = *prev;
    }
    /* search the parameter list for this component's parameters */
    prev = &(hal_data->param_list_ptr);
    next = *prev;
    while (next != 0) {
	param = SHMPTR(next);
	if (SHMPTR(param->owner_ptr) == comp) {
	    /* this param belongs to our component, unlink from list */
	    *prev = param->next_ptr;
	    /* and delete it */
	    free_param_struct(param);
	} else {
	    /* no match, try the next one */
	    prev = &(param->next_ptr);
	}
	next = *prev;
    }
    /* now we can delete the component itself */
    /* clear contents of struct */
    comp->comp_id = 0;
    comp->mem_id = 0;
    comp->type = 0;
    comp->shmem_base = 0;
    comp->name[0] = '\0';
    /* add it to free list */
    comp->next_ptr = hal_data->comp_free_ptr;
    hal_data->comp_free_ptr = SHMOFF(comp);
}

static void unlink_pin(hal_pin_t * pin)
{
    hal_sig_t *sig;
    hal_comp_t *comp;
    void *dummy_addr, **data_ptr_addr;

    /* is this pin linked to a signal? */
    if (pin->signal != 0) {
	/* yes, need to unlink it */
	sig = SHMPTR(pin->signal);
	/* make pin's 'data_ptr' point to its dummy signal */
	data_ptr_addr = SHMPTR(pin->data_ptr_addr);
	comp = SHMPTR(pin->owner_ptr);
	dummy_addr = comp->shmem_base + SHMOFF(&(pin->dummysig));
	*data_ptr_addr = dummy_addr;
	/* update the signal's reader/writer counts */
	if ((pin->dir & HAL_RD) != 0) {
	    sig->readers--;
	}
	if ((pin->dir & HAL_WR) != 0) {
	    sig->writers--;
	}
	/* mark pin as unlinked */
	pin->signal = 0;
    }
}

static void free_pin_struct(hal_pin_t * pin)
{

    unlink_pin(pin);
    /* clear contents of struct */
    pin->data_ptr_addr = 0;
    pin->owner_ptr = 0;
    pin->type = 0;
    pin->dir = 0;
    pin->signal = 0;
    pin->dummysig = 0;
    pin->name[0] = '\0';
    /* add it to free list */
    pin->next_ptr = hal_data->pin_free_ptr;
    hal_data->pin_free_ptr = SHMOFF(pin);
}

static void free_sig_struct(hal_sig_t * sig)
{
    hal_pin_t *pin;

    /* look for pins linked to this signal */
    pin = halpr_find_pin_by_sig(sig, 0);
    while (pin != 0) {
	/* found one, unlink it */
	unlink_pin(pin);
	/* check for another pin linked to the signal */
	pin = halpr_find_pin_by_sig(sig, pin);
    }
    /* clear contents of struct */
    sig->data_ptr = 0;
    sig->type = 0;
    sig->readers = 0;
    sig->writers = 0;
    sig->name[0] = '\0';
    /* add it to free list */
    sig->next_ptr = hal_data->sig_free_ptr;
    hal_data->sig_free_ptr = SHMOFF(sig);
}

static void free_param_struct(hal_param_t * p)
{
    /* clear contents of struct */
    p->data_ptr = 0;
    p->owner_ptr = 0;
    p->type = 0;
    p->name[0] = '\0';
    /* add it to free list (params use the same struct as src vars) */
    p->next_ptr = hal_data->param_free_ptr;
    hal_data->param_free_ptr = SHMOFF(p);
}

#ifdef RTAPI
static void free_funct_struct(hal_funct_t * funct)
{
    int *prev_entry, next_thread, next_entry;
    hal_thread_t *thread;
    hal_funct_entry_t *funct_entry;

/*  int next_thread, next_entry;*/

    if (funct->users > 0) {
	/* We can't casually delete the function, there are thread(s) which
	   will call it.  So we must check all the threads and remove any
	   funct_entrys that call this function */
	/* start at root of thread list */
	next_thread = hal_data->thread_list_ptr;
	/* run through thread list */
	while (next_thread != 0) {
	    /* point to thread */
	    thread = SHMPTR(next_thread);
	    /* start at root of funct_entry list */
	    prev_entry = &(thread->funct_list);
	    next_entry = *prev_entry;
	    /* run thru funct_entry list */
	    while (next_entry != 0) {
		/* point to funct entry */
		funct_entry = SHMPTR(next_entry);
		/* test it */
		if (SHMPTR(funct_entry->funct_ptr) == funct) {
		    /* this funct entry points to our funct, unlink */
		    *prev_entry = funct_entry->next_ptr;
		    /* and delete it */
		    free_funct_entry_struct(funct_entry);
		} else {
		    /* no match, try the next one */
		    prev_entry = &(funct_entry->next_ptr);
		}
		next_entry = *prev_entry;
	    }
	    /* move on to the next thread */
	    next_thread = thread->next_ptr;
	}
    }
    /* clear contents of struct */
    funct->uses_fp = 0;
    funct->owner_ptr = 0;
    funct->reentrant = 0;
    funct->users = 0;
    funct->arg = 0;
    funct->funct = 0;
    funct->name[0] = '\0';
    /* add it to free list */
    funct->next_ptr = hal_data->funct_free_ptr;
    hal_data->funct_free_ptr = SHMOFF(funct);
}
#endif /* RTAPI */

static void free_funct_entry_struct(hal_funct_entry_t * funct_entry)
{
    hal_funct_t *funct;

    if (funct_entry->funct_ptr > 0) {
	/* entry points to a function, update the function struct */
	funct = SHMPTR(funct_entry->funct_ptr);
	funct->users--;
    }
    /* clear contents of struct */
    funct_entry->funct_ptr = 0;
    funct_entry->arg = 0;
    funct_entry->funct = 0;
    /* add it to free list */
    funct_entry->next_ptr = hal_data->funct_entry_free_ptr;
    hal_data->funct_entry_free_ptr = SHMOFF(funct_entry);
}

#ifdef RTAPI
static void free_thread_struct(hal_thread_t * thread)
{
    hal_funct_entry_t *funct_entry;

    /* if we're deleting a thread, we need to stop all threads */
    hal_data->threads_running = 0;
    /* and stop the task associated with this thread */
    rtapi_task_pause(thread->task_id);
    rtapi_task_delete(thread->task_id);
    /* clear contents of struct */
    thread->uses_fp = 0;
    thread->period = 0;
    thread->priority = 0;
    thread->owner_ptr = 0;
    thread->task_id = 0;
    /* clear the function entry list */
    while (thread->funct_list != 0) {
	/* entry found, unlink it */
	funct_entry = SHMPTR(thread->funct_list);
	thread->funct_list = funct_entry->next_ptr;
	/* and free it */
	free_funct_entry_struct(funct_entry);
    }
    thread->name[0] = '\0';
    /* add thread to free list */
    thread->next_ptr = hal_data->thread_free_ptr;
    hal_data->thread_free_ptr = SHMOFF(thread);
}
#endif /* RTAPI */
