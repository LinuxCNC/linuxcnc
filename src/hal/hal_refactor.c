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

    Other contributors:
                       Paul Fox
                       <pgf AT foxharp DOT boston DOT ma DOT us>
*/

#ifdef RTAPI

#include <linux/string.h>

#ifndef __HAVE_ARCH_STRCMP	/* This flag will be defined if we do */
#define __HAVE_ARCH_STRCMP	/* have strcmp */
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
#endif	// __HAVE_ARCH_STRCMP

#include <linux/malloc.h>
#include <linux/mm.h>

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
#include "hal_refactor.h"

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))*/
#endif

#ifdef RTAPI
static int lib_module_id = -1;	/* RTAPI module ID for library module */
static int lib_mem_id = 0;	/* RTAPI shmem ID for library module */

hal_module_list_t	*global_module_list=0;
hal_part_t		*global_part_list=0;
hal_signal_t		*global_signal_list=0;
hal_thread_t		*global_thread_list=0;
hal_function_t		*global_function_list=0;

int			global_threads_running=0;
unsigned long 		global_mutex;

int global_part_id=1;

#endif


/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

#ifdef RTAPI
/***********************************************************************
*                      "MEMORY" FUNCTIONS                              *
************************************************************************/

/* Dang it... I was hoping I'd find someone who made their own
   block malloc code, but ... so far, nobody.

   TODO: Fix this with a b-tree or other better optimized mechanism.
   TODO: Consider looking for smallest block, or best aligned block...

*/

hal_memory_t *global_shared_memory;

hal_memory_t *alloc_new_memory(void)
{
hal_memory_t *newblock;

    newblock=kmalloc(sizeof(hal_memory_t), GFP_KERNEL);
    if (!newblock)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: alloc_new_memory: out of memory!");
        return 0;
        }
    newblock->start=0;
    newblock->allocated=0;
    newblock->size=0;
    newblock->next=0;
    return (newblock);
}

hal_memory_t *block_malloc(hal_memory_t **memory_handle, int size, int align)
{
hal_memory_t *ptr;
hal_memory_t *newblock;
int extra;

    if (align<1)
	align=1;

    for (ptr=*memory_handle; ptr; ptr=ptr->next)
        {
	if (!ptr->allocated)	// 
	    {
            extra=(align - (((int)ptr->start) % align)) % align;
            if ( ptr->size >= size + extra)
		{	// This block is big enough...
		if (ptr->size != size+extra)     // we split this block...
			{
                        newblock=alloc_new_memory();
			if (!newblock)
				return 0;
			newblock->size=ptr->size - extra - size;
			newblock->start=ptr->start+extra+size;
			newblock->next=ptr->next;
			ptr->allocated=ptr->start+extra;
			ptr->size=size;
			ptr->next=newblock;
			return(ptr);
			}
		}
	    }
        }
    return NULL;	// no memory worked out !
}

int block_free(hal_memory_t **memory_handle, void *memory)
{
hal_memory_t **handle;
	for (handle=memory_handle; (*handle); handle=&((*handle)->next))
	    {
	    if ((*handle)->allocated == memory)
		{
		(*handle)->allocated=NULL;
		/* TODO ... aggregate blocks around this one */
		return HAL_SUCCESS;
		}
	    }
	return HAL_INVAL;	// no memory worked out !
}

void free_block_memory(hal_memory_t **memory_handle)
{
hal_memory_t *p;

    while(*memory_handle)
	{
	p=*memory_handle;
	memory_handle=&( (*memory_handle)->next);
	kfree(p);
	}
}


void *hal_shmalloc(int size)
{
hal_memory_t *mem;
	mem=block_malloc(&global_shared_memory, size, 4);
	if (mem)
		return(mem->allocated);
	return 0;
}

void hal_shfree(void *memory)
{
	block_free(&global_shared_memory, memory);
}

void *hal_malloc(int owner, int size)
{
void *mem;
/* TODO: keep track of this memory and release it when the owner
   goes away! */
    mem=kmalloc(sizeof(hal_pin_t), GFP_KERNEL);
return mem;
}


/** 'thread_task()' is a function that is invoked as a realtime task.
    It implements a thread, by running down the thread's function list
    and calling each function in turn.
*/
static void thread_task(void *arg);


static hal_pin_t *alloc_new_pin(void)
{
    hal_pin_t *p;

    p=kmalloc(sizeof(hal_pin_t), GFP_KERNEL);

    if (p) {
	/* make sure it's empty */
	p->next=0;
	p->pin_handle=0;
	p->dummysignal.signal=0;
	p->signal=0;
	p->type=-1;
	p->dir=0;
	p->name[0]=0;
    }
    return p;
}


hal_signal_t *alloc_new_signal(void)
{
hal_signal_t *signal;
int n;

	signal=kmalloc(sizeof(hal_signal_t), GFP_KERNEL);
	if (!signal)
	    {
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_signal() failed - out of memory!\n");
	    return 0;
	    }
	signal->shm_data=hal_shmalloc(sizeof(hal_shm_signal_t));
	if (!signal->shm_data)
	    {
	    kfree(signal);
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_signal() failed - out of shared memory!");
	    return 0;
	    }

	signal->next=0;
	signal->type=0;
        signal->readers=0;
        signal->writers=0;

	for (n=0; n<HAL_NAME_LEN+1; n++)
	    signal->name[n]=0;	

	return(signal);
}

hal_thread_t *alloc_new_thread(void)
{
hal_thread_t *thread;
int n;

	thread=kmalloc(sizeof(hal_thread_t), GFP_KERNEL);
	if (!thread)
	    {
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_thread() failed - out of memory!");
	    return 0;
	    }

	thread->next=0;
	thread->uses_fp=0;
	thread->period=0;
	thread->priority=0;
	thread->task_id=0;
	thread->runtime=0;
	thread->maxtime=0;
	thread->function_list=0;

	for (n=0; n<HAL_NAME_LEN+1; n++)
	    thread->name[n]=0;	

	return(thread);
}

hal_parameter_t *alloc_new_parameter(void)
{
hal_parameter_t *param;
int n;

	param=kmalloc(sizeof(hal_parameter_t), GFP_KERNEL);
	if (!param)
	    {
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_parameter() failed - out of memory!\n");
	    return 0;
	    }
	param->shm_data=hal_shmalloc(sizeof(hal_shm_signal_t));
	if (!param->shm_data)
	    {
	    kfree(param);
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_parameter() failed - out of shared memory!\n");
	    return 0;
	    }

	param->next=0;
	param->type=0;

	for (n=0; n<HAL_NAME_LEN+1; n++)
	    param->name[n]=0;	
	param->shm_data=0;

	return(param);
}

hal_module_list_t *alloc_new_module_list(void)
{
hal_module_list_t *ml;

	ml=kmalloc(sizeof(hal_module_list_t), GFP_KERNEL);
	if (!ml)
	    {
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_module_list() failed - out of memory!");
	    return 0;
	    }

	ml->next=0;
	ml->module_info=0;
	ml->part_types=0;
	ml->module_id=0;

	return(ml);
}

hal_part_type_list_t *alloc_new_part_type_list(void)
{
hal_part_type_list_t *pl;

	pl=kmalloc(sizeof(hal_part_type_list_t), GFP_KERNEL);
	if (!pl)
	    {
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_part_type_list() failed - out of memory!");
	    return 0;
	    }

	pl->next=0;
	pl->part=0;

	return(pl);
}

hal_part_t *alloc_new_part(void)
{
hal_part_t *p;

	p=kmalloc(sizeof(hal_part_t), GFP_KERNEL);
	if (!p)
	    {
    	    rtapi_print_msg(RTAPI_MSG_ERR,
       	        "HAL: hal_alloc_new_part() failed - out of memory!");
	    return 0;
	    }

	p->part_id=-1;
	p->module=0;
	p->type=0;
	p->next=0;
        p->name[0]=0;
	p->pins=0;
	p->functions=0;	
	p->parameters=0;	

	return(p);
}

hal_module_list_t **find_module_by_id(int module_id)
{
    hal_module_list_t **module_handle;

    for (module_handle=&global_module_list; 
		*module_handle && ((*module_handle)->module_id != module_id);
		module_handle=&( (*module_handle)->next ) );

    return module_handle;
}

hal_part_t **find_part_by_id(int part_id)
{
    hal_part_t **part_handle;

    for (part_handle=&global_part_list; 
		*part_handle && ((*part_handle)->part_id != part_id);
		part_handle=&( (*part_handle)->next ) );

    return part_handle;
}

/***********************************************************************
*                  PUBLIC (API) FUNCTION CODE                          *
************************************************************************/

int hal_register_module(hal_module_info *mb)
{
    char rtapi_name[RTAPI_NAME_LEN + 1];
    char hal_name[HAL_NAME_LEN + 1];
    int module_id;

    hal_module_list_t *newmodule;
    hal_module_list_t **module_handle;

   rtapi_set_msg_level(RTAPI_MSG_DBG);

    if (mb == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: no hal_module_info\n");
	return HAL_INVAL;
    }

    if (mb->name == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: no module name\n");
	return HAL_INVAL;
    }


    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: initializing component '%s'\n",
	mb->name);
    /* copy name to local vars, truncating if needed */
    rtapi_snprintf(rtapi_name, RTAPI_NAME_LEN, "HAL_%s", mb->name);
    rtapi_snprintf(hal_name, HAL_NAME_LEN, "%s", mb->name);
    /* do RTAPI init */
    module_id = rtapi_init(rtapi_name);
    if (module_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: rtapi init failed\n");
	return HAL_FAIL;
    }

   newmodule=alloc_new_module_list();

   /* It may be silly to copy this data here...  Will it usually
      be a static block in the calling kernel module?  Is this wasteful? */

   newmodule->module_info=mb;	/* MUST REMAIN FOR DURATION OF MODULE */
   newmodule->module_id=module_id;	// Store the module id for later
   
/* Walk the linked list of module_list_t's until we reach the end... */
    for (module_handle=&global_module_list; *module_handle;
	module_handle=&( (*module_handle)->next ) );

    *module_handle=newmodule;

    return module_id;
}


int hal_unregister_module(int module_id)
{
    hal_module_list_t *module;
    hal_module_list_t **module_handle;

    hal_part_t **part_handle;
    hal_part_t *part;
    hal_part_type_list_t **part_type_handle;
    hal_part_type_list_t *part_type;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: removing module %02d\n", module_id);
    /* search module list for 'module_id' */

    module_handle=find_module_by_id(module_id);
    module=*module_handle;

    if (!module)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: module %d not found\n", module_id);
	    return HAL_INVAL;
	}


   /* TODO... Deallocate all of this module's resources */

   /* Release all of the parts owned by this module... */
 
    part_handle=&global_part_list; 
    while (*part_handle)
	{
	part=*part_handle;
	part_handle=&( (*part_handle)->next );	 // move on...
        if (part->module==module)
	    hal_remove_part(part->part_id);
	}

   /* Release all of the part types owned by this module... */

    part_type_handle=&module->part_types;
    while (*part_type_handle)
	{
	part_type=*part_type_handle;
	part_type_handle=&( (*part_type_handle)->next ); // move on...
        rtapi_print_msg(RTAPI_MSG_DBG,
	    "HAL: part type '%s' unregistered\n", part_type->part->name);
	kfree(part_type);
	}


   /* /TODO */


    /* release RTAPI resources */

    /* Unlink the module... */
	(*module_handle)=module->next;


    /* Release shared memory management structures */

    free_block_memory(&global_shared_memory);


    rtapi_exit(module->module_id);

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: module '%s' removed\n", module->module_info->name);
    return HAL_SUCCESS;
}

/*
hal_register_part_type

*/

int hal_register_part_type(int module_id, hal_part_type_info *part_info)
{
hal_part_type_list_t *p;
hal_part_type_list_t **part_handle;

hal_module_list_t *module;
hal_module_list_t **module_handle;

    /* Validate the module_id, and get a pointer to the module structure */
    module_handle=find_module_by_id(module_id);
    module=*module_handle;

    if (!module)
	{
    	rtapi_print_msg(RTAPI_MSG_DBG,
       	    "HAL: hal_register_block_type() failed - INVALID module_id! (%d)", 
		module_id);
	return HAL_INVAL;
	}

    /* Create the new part_type list structure */

    p=alloc_new_part_type_list();
    if (!p)
	{
    	rtapi_print_msg(RTAPI_MSG_DBG,
       	    "HAL: hal_register_part_type() failed - NOMEM! (0)");
	return HAL_NOMEM;
	}

    p->part=part_info;

/* Now link this part type on to the end of our list of part types
   for this module */

    for (part_handle=&(module->part_types);
	*part_handle;
	part_handle=&(*part_handle)->next );

    *part_handle=p;

    rtapi_print_msg(RTAPI_MSG_DBG,
        "HAL: hal_register_part_type: registered type %s\n", part_info->name);
    return HAL_SUCCESS;
}


/*
 * hal_remove_part(int part_id)
 */

int hal_remove_part(int part_id)
{
hal_part_t 		**part_list_handle;
hal_part_t 		*part;
hal_pin_t 		*pin, *freepin;
hal_function_t		*function, *freefunction;
hal_parameter_t 	*parameter, *freeparameter;

    for (part_list_handle=&global_part_list; 
	*part_list_handle && ( (*part_list_handle)->part_id != part_id);
	part_list_handle=&( (*part_list_handle)->next ) );

    if (! (*part_list_handle) ){
    	rtapi_print_msg(RTAPI_MSG_ERR,
       	    "HAL: hal_remove_part() failed to find part %d", part_id);
	return HAL_INVAL;
	}
    /* Unlink the part from the list of parts... */

    part=*part_list_handle;
    *part_list_handle=part->next;

    /* Now destroy all resources used by this part */

    /* TODO ... destroy all functions */

    function=part->functions;
    while (function)
	{
	freefunction=function;
	function=function->next;
	/* TODO ... destroy and release shm */

	kfree(freefunction);	
	}

    /* destroy and release parameters */

    parameter=part->parameters;
    while (parameter)
	{
	freeparameter=parameter;
	parameter=parameter->next;

	/* release shm */
	hal_shfree(freeparameter->shm_data);

	kfree(freeparameter);	
	}



    /* destroy all pins */
    pin=part->pins;
    while (pin)
	{
	freepin=pin;
	pin=pin->next;
	/* TODO ... destroy and release signal */

	kfree(freepin);	
	}


    kfree(part);

    return HAL_SUCCESS;
}

/* 
   Create a new block using the module pointed to by module and the
   part_type pointed to by block_type
*/

int hal_create_part(hal_module_list_t *module, 
	hal_part_type_list_t *part_type, const char *part_name)
{
hal_part_t	 	*newpart=0;
int 			result;
int 			result2;
hal_part_t	 	**part_list_handle;


    if (!module)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: hal_create_part: attempted to create a part without a module\n");
	return HAL_INVAL;
	}

    if (!part_type)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: hal_create_part: attempted to create a part without a part type\n");
	return HAL_INVAL;
	}

    newpart=alloc_new_part();

    if (!newpart)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: hal_create_part: out of memory!\n");
	return HAL_NOMEM;
	}


    newpart->part_id=global_part_id;
    global_part_id++;
    newpart->module=module;
    newpart->type=part_type;
    if (part_name)
	strncpy(newpart->name, part_name, HAL_NAME_LEN);

    /* Link this part into existence... */
/* Walk the linked list of block_lists until we reach the end... */
    for (part_list_handle=&global_part_list; *part_list_handle;
	part_list_handle=&( (*part_list_handle)->next ) );

    *part_list_handle=newpart;

    rtapi_print_msg(RTAPI_MSG_DBG,
        "HAL: hal_create_part: creating new part %d\n", newpart->part_id);

    result=part_type->part->create(newpart->part_id,
	part_type->part->type_id);

    if (result!=HAL_SUCCESS)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: hal_create function failed: %d!\n", result);
	/* The part was created as far as we're concerned, so since
           initialization failed, now we have to nuke it... */
	result2=hal_remove_part(newpart->part_id);
        if (result!=HAL_SUCCESS)
	    {
   	    rtapi_print_msg(RTAPI_MSG_ERR,
                "HAL: hal_create also failed to cleanup!: %d!\n", result2);
	    }

	return result;
	}

    rtapi_print_msg(RTAPI_MSG_DBG,
        "HAL: hal_create_part: created new part %d\n", newpart->part_id);

    return HAL_SUCCESS;
}

hal_module_list_t *find_module_by_name(const char *name)
{
hal_module_list_t *p;
    for (p=global_module_list; 
	p && strcmp(p->module_info->name, name); 
	p=p->next);

    return p;
}

hal_part_type_list_t *find_part_type_by_name(hal_module_list_t *module, const char *name)
{
hal_part_type_list_t *p;

    for (p=module->part_types;
	p && strcmp(p->part->name, name); 
	p=p->next);

    return p;
}


int hal_create_part_by_names(char *module_name, char *part_type_name, const char *part_name)
{
int result;
hal_module_list_t *module;
hal_part_type_list_t *part_type;

    module=find_module_by_name(module_name);
    if (!module)
        {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "hal_create_part_by_names: Could not find module named '%s'\n",
                module_name);
        return HAL_INVAL;
        }

    part_type=find_part_type_by_name(module, part_type_name);
    if (!part_type)
        {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "hal_create_part_by_names: Could not find part type '%s' for module named '%s'\n", part_type_name, module_name);
        return HAL_INVAL;
        }

    result=hal_create_part(module, part_type, part_name);
    
return(result);
}




#endif // RTAPI


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
    void **data_ptr_addr, int part_id)
{
    hal_pin_t *new;

    hal_part_t *part;
    hal_part_t **part_handle;
    hal_pin_t **pin_handle;
    int module_id;

    part_handle=find_part_by_id(part_id);
    part=*part_handle;

    if (!part)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_new called with an invalid part_id%d\n", part_id);
	return HAL_INVAL;
	}


    /* Search for a pin of this name... */
    for (pin_handle=&part->pins;
	*pin_handle && (strncmp((*pin_handle)->name, name, HAL_NAME_LEN));
	pin_handle=&( (*pin_handle)->next ) );

    if (*pin_handle)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_new called to create pin '%s' which already exists on part_id%d\n", name, part_id);
	return HAL_INVAL;
	}



    module_id=part->module->module_id;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating pin '%s'\n", name);

    /* allocate a new variable structure */
    new = alloc_new_pin();
    if (new == 0) {
	/* alloc failed */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for pin '%s'\n", name);
	return HAL_NOMEM;
    }
    /* initialize the structure */
    new->type = type;
    new->dir = dir;
    new->signal = 0;
    new->dummysignal.signal = 0;
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);
    /* make 'data_ptr' point to dummy signal */

    new->pin_handle=(hal_shm_signal_t **)data_ptr_addr;

/* Disconnect the pin... */
    *data_ptr_addr = &new->dummysignal.signal;

    /* Insert the pin... */
    for (pin_handle=&part->pins;
	*pin_handle; 
	pin_handle=&( (*pin_handle)->next ) );
    *pin_handle=new;

    return HAL_SUCCESS;
}

int unlink_pin(hal_pin_t *pin)
{
	*(pin->pin_handle)=&pin->dummysignal;
	if (pin->signal)
		{
		if (pin->type & HAL_RD)
			pin->signal->readers--;
		if (pin->type & HAL_WR)
			pin->signal->writers--;
		pin->signal=0;
		}
		

	return HAL_SUCCESS;
}

int link_pin(hal_pin_t *pin, hal_signal_t *signal)
{
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: connecting pin '%s' to '%s'\n", pin->name, signal->name);

    if (!pin)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: connect_pin called without a valid pin!\n");
	return HAL_INVAL;
	}
	

    /* found the pin, now check the signal */
    if (!signal)
	/* no signal name supplied, so we unlink pin */
	return (unlink_pin(pin));

    /* found both pin and signal, check types */
    if (pin->type != signal->type) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: type mismatch '%s' <- '%s'\n", pin->name, 	
		signal->name);
	return HAL_INVAL;
    }
    /* are we linking write_only pin to sig that already has writer? */
    if ((pin->dir == HAL_WR) && (signal->writers > 0)) {
	/* yes, can't do that */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: signal '%s' already has writer(s)\n", signal->name);
	return HAL_INVAL;
    }
    /* everything is OK, break any old link */
    unlink_pin(pin);
    /* and make the new link */

    *(pin->pin_handle) =  signal->shm_data;
    pin->signal=signal;

    /* update the signal's reader/writer counts */
    if ((pin->dir & HAL_RD) != 0) {
	signal->readers++;
    }
    if ((pin->dir & HAL_WR) != 0) {
	signal->writers++;
    }

    return HAL_SUCCESS;
}

int link_pin_by_name(int part_id, const char *pin_name, const char *signal_name)
{
int result;
hal_part_t **part_handle;
hal_part_t *part;
hal_pin_t *pin;
hal_signal_t **signal_handle;
hal_signal_t *signal;

part_handle=find_part_by_id(part_id);

part=*part_handle;

    if (!(part))
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: link_pin_by_name called with an invalid part!\n");
	return HAL_INVAL;
	}

    for (pin=part->pins; pin && strncmp(pin_name, pin->name, HAL_NAME_LEN); pin=pin->next);

    if (!pin)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: link_pin_by_name: no pin named '%s' found on part %d!\n",
		pin_name, part_id);
	return HAL_INVAL;
	}

    for (signal_handle=&global_signal_list;
	*signal_handle && (strncmp((*signal_handle)->name, 
		signal_name, HAL_NAME_LEN));
	signal_handle=&( (*signal_handle)->next ) );

    signal=*signal_handle;

    if (signal)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: link_pin_by_name: no signal named '%s' found!\n",
		signal_name);
	return HAL_INVAL;
	}
		
    result=link_pin(pin, signal);

return result;
}

int unlink_pin_by_name(int part_id, char *pin_name)
{
int result;
hal_part_t **part_handle;
hal_part_t *part;
hal_pin_t *pin;

part_handle=find_part_by_id(part_id);

part=*part_handle;

    if (!(part))
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: unlink_pin_by_name called with an invalid part!\n");
	return HAL_INVAL;
	}

    for (pin=part->pins; pin && strncmp(pin_name, pin->name, HAL_NAME_LEN); pin=pin->next);

    if (!pin)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: link_unlink_pin_by_name: no pin named '%s' found on part %d!\n",
		pin_name, part_id);
	return HAL_INVAL;
	}

    result=unlink_pin(pin);

return result;
}

/***********************************************************************
*                      "SIGNAL" FUNCTIONS                              *
************************************************************************/

int hal_signal_new(char *name, hal_type_t type)
{
 
    void *data_addr=0;
    hal_signal_t *new;
    hal_signal_t **signal_handle;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating signal '%s'\n", name);

    if (!name)
	{
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL: signal_new called with no name\n");
	return HAL_INVAL;
	}
    /* check for an existing signal with the same name */
    for (signal_handle=&global_signal_list;
	*signal_handle && (strncmp((*signal_handle)->name, name, HAL_NAME_LEN));
	signal_handle=&( (*signal_handle)->next ) );

    if (*signal_handle)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: hal_signal_new called to create signal'%s' which already exists\n", name);
	return HAL_INVAL;
	}


    /* allocate a new signal structure */
    new = alloc_new_signal();
    if (!new)
	{
	/* alloc failed */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for signal '%s'\n", name);
	return HAL_NOMEM;
    	}

    /* allocate memory for the signal value */
    switch (type) {
    case HAL_BIT:
    case HAL_S8:
    case HAL_U8:
	data_addr = hal_shmalloc(1);
	break;
    case HAL_S16:
    case HAL_U16:
	data_addr = hal_shmalloc(2);
	break;
    case HAL_S32:
    case HAL_U32:
    case HAL_FLOAT:
	data_addr = hal_shmalloc(4);
	break;
    default:
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: illegal signal type %d'\n", type);
	kfree(new);	// give back the memory for the struct...
	return HAL_INVAL;
	break;
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
    new->shm_data = (hal_shm_signal_t *)data_addr;
    new->type = type;
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);

    /* Insert the signal... */
    for (signal_handle=&global_signal_list;
	(*signal_handle);
	signal_handle=&( (*signal_handle)->next ) );
    *signal_handle=new;

    return HAL_SUCCESS;
}


int hal_signal_delete(char *name)
{
    hal_signal_t *signal;
    hal_signal_t **signal_handle;
    hal_pin_t *pin;
    hal_part_t *part;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: deleting signal '%s'\n", name);
    /* search for the signal */
    for (signal_handle=&global_signal_list;
	*signal_handle && (strncmp((*signal_handle)->name, name, HAL_NAME_LEN));
	signal_handle=&( (*signal_handle)->next ) );

    if (!(*signal_handle))
	{
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: signal '%s' not found\n",
	name);
	return HAL_INVAL;
	}

    /* unlink the signal. */

    signal=*signal_handle;
    *signal_handle=signal->next;

    /* TODO: Go through and disconnect everything that's connected to this
       signal */

    for (part=global_part_list; 
	(part);
        part=part->next ) 
	{	// visit every part...
   	    /* destroy all pins */
            for (pin=part->pins; pin; pin=pin->next)
		{ // visit every pin...
		if (pin->signal==signal)
		    unlink_pin(pin);
		}
	}


    /* TODO: Free the shared memory associated with this signal. */
    hal_shfree( (*signal_handle)->shm_data );

    kfree(*signal_handle);
    return HAL_SUCCESS;
}

/***********************************************************************
*                       "PARAM" FUNCTIONS                              *
************************************************************************/

/* wrapper functs for typed params - these call the generic funct below */

int hal_param_bit_new(char *name, hal_dir_t dir, hal_bit_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_BIT, dir, (void *) data_addr, comp_id);
}

int hal_param_float_new(char *name, hal_dir_t dir, hal_float_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_FLOAT, dir, (void *) data_addr, comp_id);
}

int hal_param_u8_new(char *name, hal_dir_t dir, hal_u8_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_U8, dir, (void *) data_addr, comp_id);
}

int hal_param_s8_new(char *name, hal_dir_t dir, hal_s8_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_S8, dir, (void *) data_addr, comp_id);
}

int hal_param_u16_new(char *name, hal_dir_t dir, hal_u16_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_U16, dir, (void *) data_addr, comp_id);
}

int hal_param_s16_new(char *name, hal_dir_t dir, hal_s16_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_S16, dir, (void *) data_addr, comp_id);
}

int hal_param_u32_new(char *name, hal_dir_t dir, hal_u32_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_U32, dir, (void *) data_addr, comp_id);
}

int hal_param_s32_new(char *name, hal_dir_t dir, hal_s32_t * data_addr,
    int comp_id)
{
    return hal_param_new(name, HAL_S32, dir, (void *) data_addr, comp_id);
}

/* this is a generic function that does the majority of the work. */

int hal_param_new(char *name, hal_type_t type, hal_dir_t dir, void *data_addr,
    int part_id)
{
    hal_parameter_t *new;
    hal_parameter_t **param_handle;

    int module_id;
    hal_part_t **part_handle;
    hal_part_t *part;


    part_handle=find_part_by_id(part_id);
    part=*part_handle;
    if (!part)
        {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: param_new called with an invalid part_id%d\n", part_id);
        return HAL_INVAL;
        }


    /* Search for a signal of this name... */
    for (param_handle=&part->parameters;
        *param_handle && (strncmp((*param_handle)->name, name, HAL_NAME_LEN));
        param_handle=&( (*param_handle)->next ) );

    if (*param_handle)
        {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "HAL: ERROR: param_new called to create parameter '%s' which already exists on part_id%d\n", name, part_id);
        return HAL_INVAL;
        }

    part_handle=find_part_by_id(part_id);
   
    if (!(*part_handle))
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param_new called with an invalid part_id %d\n", part_id);
	return HAL_INVAL;
	}

    module_id=(*part_handle)->module->module_id;


    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: creating parameter '%s'\n", name);

    /* allocate a new parameter structure */
    new = alloc_new_parameter();
    if (new == 0) {
	/* alloc failed */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for parameter '%s'\n", name);
	return HAL_NOMEM;
    }
    /* TODO fix allocate shm_data */
    new->shm_data=hal_shmalloc(4);	// for now, we get 4 bytes always.

    /* initialize the structure */
    new->type = type;
    new->dir = dir;
    rtapi_snprintf(new->name, HAL_NAME_LEN, "%s", name);

    /* Insert the parameter... */
    for (param_handle=&part->parameters;
	*param_handle; 
	param_handle=&( (*param_handle)->next ) );
    *param_handle=new;

    return HAL_SUCCESS;
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
#if 0
    hal_parameter_t *param;
    void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: setting parameter '%s'\n", name);

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
    /* is it writable? */
    if (param->dir == HAL_RD) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: param '%s' is not writable\n", name);
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
#endif
    return HAL_SUCCESS;
}


/***********************************************************************
*                   EXECUTION RELATED FUNCTIONS                        *
************************************************************************/

#ifdef RTAPI

int hal_export_funct(char *name, void (*funct) (void *, long),
    void *arg, int uses_fp, int reentrant, int comp_id)
{
#if 0
/* TODO TODO TODO */

    int *prev, next, cmp;
    hal_funct_t *new, *fptr;
    hal_comp_t *comp;
    char buf[HAL_NAME_LEN + 1];

    int module_id;
    hal_block_list_t *block_list;

    if (hal_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: export_funct called before init\n");
	return HAL_INVAL;
    }

#ifdef RTAPI
    block_list=find_block_list_by_id(comp_id);

    if (!block_list)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: pin_new called with an invalid module_id %d\n", comp_id);
	return HAL_INVAL;
	}

    module_id=block_list->module->module_id;
#else
/* TODO FIXME FIXME FIXME */
    module_id=comp_id;

#endif




    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: exporting function '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* validate comp_id */
    comp = halpr_find_comp_by_id(module_id);
    if (comp == 0) {
	/* bad comp_id */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d not found\n", module_id);
	return HAL_INVAL;
    }
    if (comp->type == 0) {
	/* not a realtime component */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: component %d is not realtime\n", module_id);
	return HAL_INVAL;
    }
    /* allocate a new function structure */
    new = alloc_funct_struct();
    if (new == 0) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for function '%s'\n", name);
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
	    /* break out of loop and init the new function */
	    break;
	}
	fptr = SHMPTR(next);
	cmp = strcmp(fptr->name, new->name);
	if (cmp > 0) {
	    /* found the right place for it, insert here */
	    new->next_ptr = next;
	    *prev = SHMOFF(new);
	    /* break out of loop and init the new function */
	    break;
	}
	if (cmp == 0) {
	    /* name already in list, can't insert */
	    free_funct_struct(new);
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: duplicate function '%s'\n", name);
	    return HAL_INVAL;
	}
	/* didn't find it yet, look at next one */
	prev = &(fptr->next_ptr);
	next = *prev;
    }
    /* at this point we have a new function and can yield the mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    /* init time logging variables */
    new->runtime = 0;
    new->maxtime = 0;
    /* note that failure to successfully create the following params
       does not cause the "export_funct()" call to fail - they are
       for debugging and testing use only */
    /* create a parameter with the function's runtime in it */
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.time", name);
    hal_param_s32_new(buf, HAL_RD, &(new->runtime), comp_id);
    /* create a parameter with the function's maximum runtime in it */
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.tmax", name);
    hal_param_s32_new(buf, HAL_RD_WR, &(new->maxtime), comp_id);
#endif
    return HAL_SUCCESS;
}

int hal_create_thread(char *name, unsigned long period_nsec, int uses_fp)
{
    hal_thread_t *new;
    long curr_period;
    hal_thread_t **handle;
    int prev_priority;
    int retval;

    if (!name)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: hal_create_thread called without a name\n");
	return HAL_INVAL;
    	}


    rtapi_print_msg(RTAPI_MSG_INFO,
	"HAL: creating thread %s, %d nsec\n", name, period_nsec);
    if (period_nsec == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: create_thread called with period of zero\n");
	return HAL_INVAL;
    }

    for (handle=&global_thread_list; 
		*handle && (strncmp((*handle)->name, name, HAL_NAME_LEN));
		handle=&( (*handle)->next ) );

    if (*handle)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: create_thread: a thread named '%s' already exists\n",
	    name);
	return HAL_INVAL;
	}

    /* allocate a new thread structure */
    new = alloc_new_thread();
    if (!new)
	{
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory to create thread\n");
	return HAL_NOMEM;
	}

    /* initialize the structure */
    new->uses_fp = uses_fp;
    strncpy(new->name, name, HAL_NAME_LEN);


    /* have to create and start a task to run the thread */
    if (!global_thread_list) {
	/* this is the first thread created */
	/* is timer started? if so, what period? */
	curr_period = rtapi_clock_set_period(0);
	if (curr_period == 0) {
	    /* not running, start it */
	    curr_period = rtapi_clock_set_period(period_nsec);
	    if (curr_period < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL_LIB: ERROR: clock_set_period returned %ld\n",
		    curr_period);
		return HAL_FAIL;
	    }
	}
	/* make sure period <= desired period (allow 1% roundoff error) */
	if (curr_period > (period_nsec + (period_nsec / 100))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL_LIB: ERROR: clock period too long: %ld\n", curr_period);
	    return HAL_FAIL;
	}
	/* reserve the highest priority (maybe for a watchdog?) */
	prev_priority = rtapi_prio_highest();
    } else {
	/* there are other threads */
	prev_priority = global_thread_list->priority;
    }


    /* Link the structure in.. */

    *handle=new;


    /* make period an integer multiple of the timer period */
//    n = (period_nsec + hal_data->base_period / 2) / hal_data->base_period;
//    new->period = hal_data->base_period * n;
    /* make priority one lower than previous */
    new->priority = rtapi_prio_next_lower(prev_priority);
    /* create task - owned by library module, not caller */

    retval = rtapi_task_new(thread_task, new, new->priority,
	lib_module_id, HAL_STACKSIZE, uses_fp);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: could not create task for thread %s - %d\n", 
		name, retval);
	return HAL_FAIL;
    }
    new->task_id = retval;
    /* start task */
    retval = rtapi_task_start(new->task_id, new->period);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: could not start task for thread %s\n", name);
	return HAL_FAIL;
    }
    new->runtime = 0;
    new->maxtime = 0;


#if 0
/* These params need to be re-visited when I refactor HAL.  Right
   now they cause problems - they can no longer be owned by the calling
   component, and they can't be owned by the hal_lib because it isn't
   actually a component.
*/
    hal_param_s32_new(buf, HAL_RD, &(new->runtime), lib_module_id);
    /* create a parameter with the thread's maximum runtime in it */
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.tmax", name);
    hal_param_s32_new(buf, HAL_RD_WR, &(new->maxtime), lib_module_id);
#endif
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL: thread created\n");
    return HAL_SUCCESS;
}

int hal_thread_delete(char *name)
{
    hal_thread_t *thread;
    hal_thread_t **handle;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL: deleting thread '%s'\n", name);
    /* search for the signal */
    for (handle=&global_thread_list;
	*handle && (strncmp((*handle)->name, name, HAL_NAME_LEN));
	handle=&( (*handle)->next ) );

    if (!(*handle))
	{
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: thread_delet: thread '%s' not found\n",
	name);
	return HAL_INVAL;
	}

    /* unlink the thread. */

    thread=*handle;
    *handle=thread->next;

    /* TODO: Go through and disconnect everything that's connected to this
       thread */


    kfree(thread);
    return HAL_SUCCESS;
}

#endif /* RTAPI */

int hal_add_funct_to_thread(char *funct_name, char *thread_name, int position)
{
#if 0
    hal_thread_t *thread;
    hal_function_t *funct;
    hal_list_t *list_root, *list_entry;
    int n;
    hal_funct_entry_t *funct_entry;

    if (hal_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: add_funct called before init\n");
	return HAL_INVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: adding function '%s' to thread '%s'\n",
	funct_name, thread_name);
    /* get mutex before accessing data structures */
    rtapi_mutex_get(&(hal_data->mutex));
    /* make sure position is valid */
    if (position == 0) {
	/* zero is not allowed */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL: ERROR: bad position: 0\n");
	return HAL_INVAL;
    }
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
    funct = halpr_find_funct_by_name(funct_name);
    if (funct == 0) {
	/* function not found */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' not found\n", funct_name);
	return HAL_INVAL;
    }
    /* found the function, is it available? */
    if ((funct->users > 0) && (funct->reentrant == 0)) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' is not reentrant\n", funct_name);
	return HAL_INVAL;
    }
    /* search thread list for thread_name */
    thread = halpr_find_thread_by_name(thread_name);
    if (thread == 0) {
	/* thread not found */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: thread '%s' not found\n", thread_name);
	return HAL_INVAL;
    }
    /* ok, we have thread and function, are they compatible? */
    if ((funct->uses_fp) && (!thread->uses_fp)) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' needs FP\n", funct_name);
	return HAL_INVAL;
    }
    /* find insertion point */
    list_root = &(thread->funct_list);
    list_entry = list_root;
    n = 0;
    if (position > 0) {
	/* insertion is relative to start of list */
	while (++n < position) {
	    /* move further into list */
	    list_entry = list_next(list_entry);
	    if (list_entry == list_root) {
		/* reached end of list */
		rtapi_mutex_give(&(hal_data->mutex));
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL: ERROR: position '%d' is too high\n", position);
		return HAL_INVAL;
	    }
	}
    } else {
	/* insertion is relative to end of list */
	while (--n > position) {
	    /* move further into list */
	    list_entry = list_prev(list_entry);
	    if (list_entry == list_root) {
		/* reached end of list */
		rtapi_mutex_give(&(hal_data->mutex));
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL: ERROR: position '%d' is too low\n", position);
		return HAL_INVAL;
	    }
	}
	/* want to insert before list_entry, so back up one more step */
	list_entry = list_prev(list_entry);
    }
    /* allocate a funct entry structure */
    funct_entry = alloc_funct_entry_struct();
    if (funct_entry == 0) {
	/* alloc failed */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: insufficient memory for thread->function link\n");
	return HAL_NOMEM;
    }
    /* init struct contents */
    funct_entry->funct_ptr = SHMOFF(funct);
    funct_entry->arg = funct->arg;
    funct_entry->funct = funct->funct;
    /* add the entry to the list */
    list_add_after((hal_list_t *) funct_entry, list_entry);
    /* update the function usage count */
    funct->users++;
    rtapi_mutex_give(&(hal_data->mutex));
#endif 
    return HAL_SUCCESS;
}

int hal_del_funct_from_thread(char *funct_name, char *thread_name)
{
#if 0
    hal_thread_t *thread;
    hal_funct_t *funct;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *funct_entry;

    if (hal_data == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: del_funct called before init\n");
	return HAL_INVAL;
    }
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
    funct = halpr_find_funct_by_name(funct_name);
    if (funct == 0) {
	/* function not found */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' not found\n", funct_name);
	return HAL_INVAL;
    }
    /* found the function, is it in use? */
    if (funct->users == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: function '%s' is not in use\n", funct_name);
	return HAL_INVAL;
    }
    /* search thread list for thread_name */
    thread = halpr_find_thread_by_name(thread_name);
    if (thread == 0) {
	/* thread not found */
	rtapi_mutex_give(&(hal_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: thread '%s' not found\n", thread_name);
	return HAL_INVAL;
    }
    /* ok, we have thread and function, does thread use funct? */
    list_root = &(thread->funct_list);
    list_entry = list_next(list_root);
    while (1) {
	if (list_entry == list_root) {
	    /* reached end of list, funct not found */
	    rtapi_mutex_give(&(hal_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL: ERROR: thread '%s' doesn't use %s\n", thread_name,
		funct_name);
	    return HAL_INVAL;
	}
	funct_entry = (hal_funct_entry_t *) list_entry;
	if (SHMPTR(funct_entry->funct_ptr) == funct) {
	    /* this funct entry points to our funct, unlink */
	    list_remove_entry(list_entry);
	    /* and delete it */
	    free_funct_entry_struct(funct_entry);
	    /* done */
	    rtapi_mutex_give(&(hal_data->mutex));
	    return HAL_SUCCESS;
	}
	/* try next one */
	list_entry = list_next(list_entry);
    }
#endif 
return HAL_SUCCESS;
}

int hal_start_threads(void)
{
    /* a trivial function for a change! */

    rtapi_print_msg(RTAPI_MSG_INFO, "HAL: starting threads\n");
    global_threads_running = 1;
    return HAL_SUCCESS;
}

int hal_stop_threads(void)
{
    global_threads_running = 0;
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL: threads stopped\n");
    return HAL_SUCCESS;
}


/***********************************************************************
*                    PRIVATE FUNCTION CODE                             *
************************************************************************/


/***********************************************************************
*                     LOCAL FUNCTION CODE                              *
************************************************************************/

#ifdef RTAPI
/* these functions are called when the hal_lib module is insmod'ed
   or rmmod'ed.
*/

int rtapi_app_main(void)
{
    int retval;
    void *mem;
    hal_memory_t *shared_memory;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL_LIB: loading kernel lib\n");
    /* do RTAPI init */
    lib_module_id = rtapi_init("HAL_LIB");
    if (lib_module_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL_LIB: ERROR: rtapi init failed\n");
	return HAL_FAIL;
    }
    /* get HAL shared memory block from RTAPI */
    lib_mem_id = rtapi_shmem_new(HAL_KEY, lib_module_id, HAL_SIZE);
    if (lib_mem_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL_LIB: ERROR: could not open shared memory\n");
	rtapi_exit(lib_module_id);
	return HAL_FAIL;
    }
    /* get address of shared memory area */
    retval = rtapi_shmem_getptr(lib_mem_id, &mem);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: ERROR: could not access shared memory\n");
	rtapi_exit(lib_module_id);
	return HAL_FAIL;
    }
    /* set up internal pointers to shared mem and data structure */

    shared_memory=alloc_new_memory();	/* get a memory management struct */
    shared_memory->start=mem;
    shared_memory->size=HAL_SIZE;

    global_shared_memory=shared_memory;




//    hal_data = (hal_data_t *) mem;
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL_LIB: kernel lib installed successfully\n");

    #ifdef PROCFS
    hal_init_procfs();
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL_LIB: Using proc\n");
    #else
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL_LIB: NOT Using proc\n");
    #endif
    return 0;
}

void rtapi_app_exit(void)
{
int result;
//    hal_thread_t *thread;

    rtapi_print_msg(RTAPI_MSG_DBG, "HAL_LIB: removing kernel lib\n");

    #ifdef PROCFS
    hal_shutdown_procfs();
    #endif
 
    /* Stop all the threads before removing them... */

    hal_stop_threads();

    /* must remove all threads before unloading this module */

    while (global_thread_list)
	hal_thread_delete(global_thread_list->name);
	
    /* release RTAPI resources */
    rtapi_shmem_delete(lib_mem_id, lib_module_id);
    rtapi_exit(lib_module_id);
    /* done */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL_LIB: kernel lib removed successfully\n");
}

/* this is the task function that implements threads in realtime */

static void thread_task(void *arg)
{
    hal_thread_t *thread;
    hal_function_t *function;
    hal_function_entry_t *ptr;
    long long int start_time, end_time;
    long long int thread_start_time;

    thread = arg;
    while (1) {
	if (global_threads_running > 0) {
	    /* execution time logging */

	    start_time = rtapi_get_time();
	    end_time = start_time;
	    thread_start_time = start_time;

	    /* run thru function list */

            for (ptr=thread->function_list; (ptr); ptr=ptr->next)
		{
		function=ptr->function;
		function->function(function->arg, thread->period);

		/* capture execution time */
		end_time = rtapi_get_time();
		/* point to function structure */
		function->runtime = (long int) (end_time - start_time);
		if (function->runtime > function->maxtime) {
		    function->maxtime = function->runtime;
		}
		/* prepare to measure time for next funct */
		start_time = end_time;
	    	}
	    /* update thread execution time */
	    thread->runtime = (long int) (end_time - thread_start_time);
	    if (thread->runtime > thread->maxtime) {
		thread->maxtime = thread->runtime;
	    }
	}
	/* wait until next period */
	rtapi_wait();
    }
}

#endif /* RTAPI */


/*
 * /proc support
 *
 * split this off into another module some time...
 */

#ifdef PROCFS
#include <linux/proc_fs.h>

struct proc_dir_entry *hal_procfs_root;

int isspace(char c)
{
	if ( (c==' ')  || (c=='\r') || (c=='\t') || (c=='\n') )
		return 1;
	return 0;
}


long int atoi(char *data)
{
long int n=0;
char *ptr;
	for (ptr=data; *ptr; ptr++)
		{
		if ((*ptr<'0') || (*ptr>'9'))
		    return n;
                n=n*10+(*ptr-'0');
		}
return n;
}

int tokenize_string(char *command, char *tokens[], int maximum_tokens)
{
    int offset;
    char c;
    int m=0;
    enum { BETWEEN_TOKENS,
           IN_TOKEN,
	   SINGLE_QUOTE,
	   DOUBLE_QUOTE } state;

    offset = 0;
    state = BETWEEN_TOKENS;
    while ( m < maximum_tokens ) {
	c=command[offset];
	switch ( state ) {
	case BETWEEN_TOKENS:
	    if (( c == '\n' ) || ( c == '\0' )) {
		/* end of the line */
		return(m);
	    } else if ( isspace(c) ) {
		/* whitespace, skip it */
		offset++;
	    } else {
		/* first char of a token */
		tokens[m] = &command[offset++];
		state = IN_TOKEN;
	    }
	    break;
	case IN_TOKEN:
	    if (( c == '\n' ) || ( c == '\0' )) {
		/* end of the line, terminate current token */
		command[offset++]=0;
		m++;
		return(m);
	    } else if ( c == '\'' ) {
		/* start of a quoted string */
		offset++;
		state = SINGLE_QUOTE;
	    } else if ( c == '\"' ) {
		/* start of a quoted string */
		offset++;
		state = DOUBLE_QUOTE;
	    } else if ( isspace(c) ) {
		/* end of the current token */
		command[offset++]=0;
		m++;
		state = BETWEEN_TOKENS;
	    } else {
		/* ordinary character */
		offset++;
	    }
	    break;
	case SINGLE_QUOTE:
	    if (( c == '\n' ) || ( c == '\0' )) {
		/* end of the line, terminate current token */
		command[offset++] = '\0';
		m++;
		return(m);
	    } else if ( c == '\'' ) {
		/* end of quoted string */
		offset++;
		state = IN_TOKEN;
	    } else {
		/* ordinary character */
		offset++;
	    }
	    break;
	case DOUBLE_QUOTE:
	    if (( c == '\n' ) || ( c == '\0' )) {
		/* end of the line, terminate current token */
		command[offset++] = '\0';
		m++;
		return(m);
	    } else if ( c == '\"' ) {
		/* end of quoted string */
		offset++;
		state = IN_TOKEN;
	    } else {
		/* ordinary character, copy to buffer */
		offset++;
	    }
	    break;
	default:
	    /* should never get here */
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"Bad state in token parser\n");
	    return -1;
	}
    }
return(m);
}



int hal_procfs_read_modules(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
int len=0;
hal_module_list_t **module_handle;
int bytes;

//	hal_data->comp_list_ptr;

/* Walk the linked list of module_list_t's until we reach the end... */
    for (module_handle=&global_module_list; 
	(*module_handle && (count-len >0));
        module_handle=&( (*module_handle)->next ) )
	{
	bytes=snprintf(page+off, count-len, "%s\n", 
		(*module_handle)->module_info->name);
	len+=bytes;
	off+=bytes;
	}
return(len);
}

int hal_procfs_read_signals(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
int len=0;
hal_signal_t **signal_handle;
int bytes;

//	hal_data->comp_list_ptr;

/* Walk the linked list of module_list_t's until we reach the end... */
    for (signal_handle=&global_signal_list; 
	(*signal_handle && (count-len >0));
        signal_handle=&( (*signal_handle)->next ) )
	{
	bytes=snprintf(page+off, count-len, "%s\n", 
		(*signal_handle)->name);
	len+=bytes;
	off+=bytes;
	}
return(len);
}


int hal_procfs_read_parts(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
int len=0;
int bytes;
hal_part_t **part_handle;

    for (part_handle=&global_part_list; 
	(*part_handle && (count-len >0));
        part_handle=&( (*part_handle)->next ) )
	{
	bytes=snprintf(page+off, count-len, "%d: %s %s '%s'\n", 
		(*part_handle)->part_id,
		(*part_handle)->module->module_info->name,
		(*part_handle)->type->part->name,
		(*part_handle)->name);
	len+=bytes;
	off+=bytes;
	}
return(len);
}

int hal_procfs_read_functions(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
int len=0;
int bytes;
hal_function_t **handle;

    for (handle=&global_function_list; 
	(*handle && (count-len >0));
        handle=&( (*handle)->next ) )
	{
	bytes=snprintf(page+off, count-len, "%s\n", 
		(*handle)->name);
	len+=bytes;
	off+=bytes;
	}
return(len);
}

int hal_procfs_read_threads(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
int len=0;
int bytes;
hal_thread_t **handle;

    for (handle=&global_thread_list; 
	(*handle && (count-len >0));
        handle=&( (*handle)->next ) )
	{
	bytes=snprintf(page+off, count-len, "%s\n", 
		(*handle)->name);
	len+=bytes;
	off+=bytes;
	}
return(len);
}
#define MAX_COMMAND_LENGTH 100

int hal_procfs_write_create_signal(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
static char command[MAX_COMMAND_LENGTH];
char *argv[1];
int argc;
int result;

    strncpy(command, buffer, MAX_COMMAND_LENGTH);
    command[MAX_COMMAND_LENGTH-1]=0;

    argc=tokenize_string(command, argv, 1);
    if (argc!=1)
	{
    	rtapi_print_msg(RTAPI_MSG_ERR,
       	     "HAL: hal_create_signal procfs called with wrong number of arguments (%d)\n", argc);
        return -EINVAL;
	}

/* TODO ... make hal_type_t work out ... */
    result=hal_signal_new(argv[0], HAL_U32);

return count;
}

int hal_procfs_write_remove_signal(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
static char command[MAX_COMMAND_LENGTH];
char *argv[1];
int argc;
int result;

    strncpy(command, buffer, MAX_COMMAND_LENGTH);
    command[MAX_COMMAND_LENGTH-1]=0;

    argc=tokenize_string(command, argv, 1);
    if (argc!=1)
	{
    	rtapi_print_msg(RTAPI_MSG_ERR,
       	     "HAL: hal_remove_signal procfs called with wrong number of arguments (%d)\n", argc);
        return -EINVAL;
	}

    result=hal_signal_delete(argv[0]);
    if (HAL_SUCCESS != result)
	return -EINVAL;

return count;
}

int hal_procfs_write_create_thread(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
static char command[MAX_COMMAND_LENGTH];
char *argv[3];
int argc;
int result;

unsigned long period_nsec;
int uses_fp;

    strncpy(command, buffer, MAX_COMMAND_LENGTH);
    command[MAX_COMMAND_LENGTH-1]=0;

    strncpy(command, buffer, MAX_COMMAND_LENGTH);
    argc=tokenize_string(command, argv, 3);
    if (argc!=3)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
         	"hal_procfs_create_thrad: requires 3 arguments\n");
	return -EINVAL;
	}

    period_nsec=atoi(argv[1]);
    uses_fp=atoi(argv[2]);
    result=hal_create_thread(argv[0], period_nsec, uses_fp);
    if (HAL_SUCCESS != result)
	return -EINVAL;

return(count);
}

int hal_procfs_write_create_part(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
int len=count;
int result;

static char command[MAX_COMMAND_LENGTH];
char *type=NULL;
char *name=NULL;
int offset;

    if (len>MAX_COMMAND_LENGTH)
	len=MAX_COMMAND_LENGTH;

    strncpy(command, buffer, len);

    for (offset=0; offset<len; offset++)
	{
	if (command[offset]==' ')
		{
		command[offset]=0;
		type=command+offset+1;
		break;
		}
	}


    if (!type)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
         	"hal_procfs_create_part: No type specifier found in proc create write\n");
	return -EINVAL;
	}

    for (offset=offset+1; offset<len; offset++)
	{
	if (command[offset]==' ')
		{
		command[offset]=0;
		name=command+offset+1;
		break;
		}
	}

    for (offset=0; offset<len; offset++)
	if (command[offset]=='\n' || command[offset]==0)
	    command[offset]=0;

    command[MAX_COMMAND_LENGTH-1]=0;

    result=hal_create_part_by_names(command, type, name);
    if (HAL_SUCCESS != result)
	return -EINVAL;

return(len);
}


int hal_procfs_write_remove_part(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
int len=count;
int result;
int part_id;

static char command[MAX_COMMAND_LENGTH];
int offset;



    if (len>MAX_COMMAND_LENGTH)
	len=MAX_COMMAND_LENGTH;

    strncpy(command, buffer, len);


    for (offset=0; offset<len; offset++)
	if (command[offset]=='\n' || command[offset]==0)
	    command[offset]=0;

    command[MAX_COMMAND_LENGTH-1]=0;

    part_id=atoi(command);

    result=hal_remove_part(part_id);

    if (HAL_SUCCESS != result)
	return -EINVAL;

return(len);
}

int hal_init_procfs(void)
{
struct proc_dir_entry *pfs;
	hal_procfs_root=proc_mkdir("hal", NULL);

	pfs=create_proc_entry("show_modules", S_IRUSR, hal_procfs_root);
	pfs->read_proc = hal_procfs_read_modules;

	pfs=create_proc_entry("show_signals", S_IRUSR, hal_procfs_root);
	pfs->read_proc = hal_procfs_read_signals;


	pfs=create_proc_entry("show_parts", S_IRUSR, hal_procfs_root);
	pfs->read_proc = hal_procfs_read_parts;

	pfs=create_proc_entry("show_threads", S_IRUSR, hal_procfs_root);
	pfs->read_proc = hal_procfs_read_threads;

	pfs=create_proc_entry("show_functions", S_IRUSR, hal_procfs_root);
	pfs->read_proc = hal_procfs_read_functions;


	pfs=create_proc_entry("create_part", S_IWUSR, hal_procfs_root);
	pfs->write_proc = hal_procfs_write_create_part;


	pfs=create_proc_entry("remove_part", S_IWUSR, hal_procfs_root);
	pfs->write_proc = hal_procfs_write_remove_part;


	pfs=create_proc_entry("create_signal", S_IWUSR, hal_procfs_root);
	pfs->write_proc = hal_procfs_write_create_signal;

	pfs=create_proc_entry("remove_signal", S_IWUSR, hal_procfs_root);
	pfs->write_proc = hal_procfs_write_remove_signal;

	pfs=create_proc_entry("create_thread", S_IWUSR, hal_procfs_root);
	pfs->write_proc = hal_procfs_write_create_thread;

return 0;
}

void hal_shutdown_procfs(void)
{
	remove_proc_entry("create_part", hal_procfs_root);
	remove_proc_entry("remove_part", hal_procfs_root);
	remove_proc_entry("create_signal", hal_procfs_root);
	remove_proc_entry("remove_signal", hal_procfs_root);
	remove_proc_entry("create_thread", hal_procfs_root);
	remove_proc_entry("remove_thread", hal_procfs_root);
	remove_proc_entry("show_parts", hal_procfs_root);
	remove_proc_entry("show_modules", hal_procfs_root);
	remove_proc_entry("show_signals", hal_procfs_root);
	remove_proc_entry("show_threads", hal_procfs_root);
	remove_proc_entry("show_functions", hal_procfs_root);
	remove_proc_entry("hal", NULL);
}


#endif // PROCFS

