/********************************************************************
* Description:  watch.c
*
* This file, 'watch.c', is a HAL instantiated component that,
* reads the value of a pin and waits until it reaches a 
* specified value and sets a flag.
* It then either ceases to track the pin value or continues to do so.
*
* watch can be passed all parameters required via the argc/argv mechanism
* built into instantiated components.
*
* Passing strings via module params to instantiated components is
* inherently risky, because the kernel module param is only created once, and all 
* the instantiations of the component reuse it, irrespective of whether they 
* have longer strings and thus overrun the allocated buffer.
*
* Example:
* 	newinst watch w1 --- pin_name=component.pin preset_name=component.pin \
*		preset_type=0 target_value=100.00 forever=1 iterations=0
*
* Where:
* 	pin_name = Name of the pin value to watch (string)
* 	preset_name = Name of pin to preset with target value (string) 
*		(could be panel pin or heater temp etc)
*	preset_type = Is it a pin or a signal (0 pin, 1 sig)
* 	target value = Value required (float)
* 	forever =  Keep watching pin after target value reached (bit)
* 	iterations = Watch for XX iterations only (s32) 
*	       - remember this is thread polls, so may go very quickly
*
* Pins:
* 	trigger (bit in) - starts the component watching 'pin_name'
* 	reset (bit in) - stops the component watching 'pin_name' 
*		(& zeros started, stopped, triggered.)
* 	value-out (float out) - value of the pin at that poll
* 	target-reached (bit out) - target value has been reached
* 	target-value (float in) - value to check for
*	target-value-int (s32 in) - value of (int)target-value
* 	forever - (bit in) - keep watching the pin after target reached
*	iterations (s32 in) - ignore target-value and run for X thread polls
*
* Bit flags:
*	started (bit, out) - value tracking underway and target not reached
*	stopped (bit, out) - value tracking not underway
*	triggered (bit, out) - the component has been started and not reset
*       (these are just debug flags or possible logic triggers for panel widgets)
*
* Author: ArcEye <arceyeATmgwareDOTcoDOTuk>
* License: GPL Version 2
*
* Copyright (c) 2018 All rights reserved.
*
* Last change:.
********************************************************************/
    
/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public
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

    This code was written as part of the Machinekit project...
*/


#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_accessor.h"
#include "hal_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>   /* Standard types */
#include <signal.h>
#include <math.h>
#include <unistd.h>   /* UNIX standard function definitions */
#include <fcntl.h>    /* File control definitions */
#include <errno.h>    /* Error number definitions */
#include <ctype.h>

// forward declaration
//int get_pin_value(char *name, char *value);
//int get_common(hal_type_t type, void *d_ptr, char *value);

static int comp_id;

static char *compname = "watch";

#ifdef MODULE_INFO
MODULE_INFO(machinekit, "component:watch:Watch the value of a HAL pin with options");
MODULE_INFO(machinekit, "pin:#.trigger:bit:pincount:in::None");
MODULE_INFO(machinekit, "pin:#.reset:bit:pincount:in::None");
MODULE_INFO(machinekit, "pin:#.target_reached:bit:pincount:out::None");
MODULE_INFO(machinekit, "pin:#.value_out:s32:pincount:out::None");
MODULE_INFO(machinekit, "pin:#.target-value:float:pincount:in::None");
MODULE_INFO(machinekit, "pin:#.target-value-int:s32:pincount:in::None");
MODULE_INFO(machinekit, "pin:#.forever:bit:pincount:in::None");
MODULE_INFO(machinekit, "pin:#.iterations:s32:pincount:in::None");
MODULE_INFO(machinekit, "pin:#.started:bit:pincount:out:in::None");
MODULE_INFO(machinekit, "pin:#.stopped:bit:pincount:out:in::None");
MODULE_INFO(machinekit, "pin:#.triggered:bit:pincount:out::None");
MODULE_INFO(machinekit, "instanceparam:pincount:int::1");
MODULE_INFO(machinekit, "license:GPL2");
MODULE_INFO(machinekit, "author:Arceye");
MODULE_INFO(machinekit, "funct:watch_:1:");
MODULE_LICENSE("GPL2");
#endif // MODULE_INFO
RTAPI_TAG(HAL,HC_INSTANTIABLE);

// no singleton in instantiated components, just restrict to 1
// you could create another one to view another pin
#define MAXCOUNT 1
#define DEFAULTCOUNT 1

static int pincount = 1;
RTAPI_IP_INT(pincount, "");

struct inst_data
    {
// pins
    hal_bit_t *_trigger;
    hal_bit_t *_reset;
    hal_bit_t *_target_reached;
    hal_float_t *_value_out;
    hal_bit_t *_forever;
    hal_s32_t *_iterations;
    hal_s32_t *_target_value_int;
    hal_float_t *_target_value;
// flags for use in panels etc
    hal_bit_t *_started;
    hal_bit_t *_stopped;
    hal_bit_t *_triggered;
// local copies
    int local_pincount;
    };

static int maxpins __attribute__((unused)) = 1;

static int watch_(void *arg, const hal_funct_args_t *fa);

static int instantiate(const int argc, char* const *argv);

static int extra_inst_setup(struct inst_data* ip, const char *name, int argc,
                            char* const *argv);

// var to take pin names passed to newinst
char target_pin_name[HAL_NAME_LEN];
char preset_name[HAL_NAME_LEN];
int preset_type;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int export_halobjs(struct inst_data *ip, int owner_id, const char *name,
                          const int argc, char * const *argv)
{
char buf[HAL_NAME_LEN + 1];
int r = 0;
    // pins
    r = hal_pin_bit_newf(HAL_OUT, &(ip->_started), owner_id, "%s.started", name);
    if(r != 0) return r;

    r = hal_pin_bit_newf(HAL_OUT, &(ip->_stopped), owner_id, "%s.stopped", name);
    if(r != 0) return r;

    r = hal_pin_bit_newf(HAL_IN, &(ip->_trigger), owner_id, "%s.trigger", name);
    if(r != 0) return r;

    r = hal_pin_bit_newf(HAL_OUT, &(ip->_triggered), owner_id, "%s.triggered", name);
    if(r != 0) return r;

    r = hal_pin_bit_newf(HAL_IN, &(ip->_reset), owner_id, "%s.reset", name);
    if(r != 0) return r;

    r = hal_pin_bit_newf(HAL_OUT, &(ip->_target_reached), owner_id, "%s.target-reached", name);
    if(r != 0) return r;

    r = hal_pin_float_newf(HAL_OUT, &(ip->_value_out), owner_id, "%s.value-out", name);
    if(r != 0) return r;

    // param pins
    r = hal_pin_bit_newf(HAL_IN, &(ip->_forever), owner_id, "%s.forever", name);
    if(r != 0) return r;

    r = hal_pin_s32_newf(HAL_IN, &(ip->_iterations), owner_id, "%s.iterations", name);
    if(r != 0) return r;

    r = hal_pin_s32_newf(HAL_IN, &(ip->_target_value_int), owner_id, "%s.target-value-int", name);
    if(r != 0) return r;

    r = hal_pin_float_newf(HAL_IN, &(ip->_target_value), owner_id, "%s.target-value", name);
    if(r != 0) return r;

    // local copies
    if(! ip->local_pincount || ip->local_pincount == -1)
         ip->local_pincount = DEFAULTCOUNT;

    hal_print_msg(RTAPI_MSG_DBG,"export_halobjs() ip->local_pincount set to %d", ip->local_pincount);

    hal_export_xfunct_args_t __xf = 
        {
        .type = FS_XTHREADFUNC,
        .funct.x = watch_,
        .arg = ip,
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = owner_id
        };

    rtapi_snprintf(buf, sizeof(buf),"%s.funct", name);
    r = hal_export_xfunctf(&__xf, buf, name);
    if(r != 0)
        return r;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int instantiate(const int argc, char* const *argv)
{
struct inst_data *ip;
// argv[0]: component name argv[1]: instance
const char *name = argv[1];
int r, k;
int inst_id;
    
    inst_id = hal_inst_create(name, comp_id, sizeof(struct inst_data), (void **)&ip);
    if (inst_id < 0)
        return -1;

    hal_print_msg(RTAPI_MSG_DBG,"%s inst=%s argc=%d",__FUNCTION__, name, argc);

    hal_print_msg(RTAPI_MSG_DBG,"%s: int instance param: %s=%d",__FUNCTION__,"pincount", pincount);
    int pin_param_value = pincount;
    if((pin_param_value == -1) || (pin_param_value == 0))
        pin_param_value = DEFAULTCOUNT;
    else if((pin_param_value > 0) && (pin_param_value > MAXCOUNT))
        pin_param_value = MAXCOUNT;
    ip->local_pincount = pincount = pin_param_value;
    hal_print_msg(RTAPI_MSG_DBG,"ip->local_pincount set to %d", pin_param_value);

    r = export_halobjs(ip, inst_id, name, argc, argv);
    // if the extra_inst_setup returns non zero will abort module creation
    // we could just do it inside instantiate(), but this is cleaner
    k = extra_inst_setup(ip, name, argc, argv);
    if(k != 0)
        return k;

    pincount = -1;

    return r;
}

int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate, NULL, compname);

    if (comp_id < 0)

        return -1;

    hal_ready(comp_id);

    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

///////////////////////////////////////////////////////////////////////

int get_common(hal_type_t type, void *d_ptr, char *value)
{
// This function assumes that the mutex is held
int retval = 0;
int bitval = 0;
double fval = 0;
long lval = 0;
unsigned long ulval = 0;

    switch (type)
        {
        case HAL_BIT:
                    bitval = *(hal_bit_t *) (d_ptr);
                    sprintf(value, "%d", bitval);
                    break;
        case HAL_FLOAT:
                    fval = *((hal_float_t *) (d_ptr));
                    sprintf(value, "%f", fval);
                    break;
        case HAL_S32:
                    lval =  *((hal_s32_t *) (d_ptr));
                    sprintf(value, "%ld", lval);
                    break;
        case HAL_U32:
                    ulval = *((hal_u32_t *) (d_ptr));
                    sprintf(value, "%lu", ulval);
                    break;
        default:
                    // Shouldn't get here, but just in case...
                    retval = -EINVAL;
        }

    return retval;
}




int get_pin_value(char *name, char *value)
{
int retval;
hal_pin_t *pin;
hal_type_t type;
void *d_ptr;

    // get mutex before accessing shared data
    rtapi_mutex_get(&(hal_data->mutex));

    pin = halpr_find_pin_by_name(name);
    if(pin == 0)
        {
        rtapi_mutex_give(&(hal_data->mutex));
        return -EINVAL;
        }
    else // pin
        {
        type = pin->type;
        d_ptr = (void*)&pin->dummysig;
        }

    retval = get_common(type, d_ptr, value);

    rtapi_mutex_give(&(hal_data->mutex));
    if (retval != 0)
	hal_print_msg(RTAPI_MSG_DBG, "Error getting value of pin: %s\n", name);
    return retval;
}


int get_pin_type(char *name, hal_type_t *type)
{
int retval = 0;
hal_pin_t *pin;

    // get mutex before accessing shared data
    rtapi_mutex_get(&(hal_data->mutex));

    pin = halpr_find_pin_by_name(name);
    if(pin == 0)
        {
        rtapi_mutex_give(&(hal_data->mutex));
        retval = -EINVAL;
        }
    else // pin
        {
        *(type) = pin->type;
        }

    rtapi_mutex_give(&(hal_data->mutex));
    if (retval != 0)
	hal_print_msg(RTAPI_MSG_DBG, "Error getting value of pin: %s\n", name);
    return retval;
}


int get_sig_type(char *name, hal_type_t *type)
{
int retval = 0;
hal_sig_t *sig;

    // get mutex before accessing shared data
    rtapi_mutex_get(&(hal_data->mutex));

    sig = halpr_find_sig_by_name(name);
    if(sig == 0)
        {
        rtapi_mutex_give(&(hal_data->mutex));
        retval = -EINVAL;
        }
    else // sig
        {
        *(type) = sig->type;
        }

    rtapi_mutex_give(&(hal_data->mutex));
    if (retval != 0)
	hal_print_msg(RTAPI_MSG_DBG, "Error getting value of signal: %s\n", name);
    return retval;
}


/////////////////////////////////////////////////////////////////////////////////////////

int set_common(hal_type_t type, void *d_ptr, char *value)
{   
// This function assumes that the mutex is held
int retval = 0;
double fval;
long lval;
unsigned long ulval;
char *cp = value;

    switch (type) {
        case HAL_BIT:
            if ((strcmp("1", value) == 0) || (strcasecmp("TRUE", value) == 0))
                *(hal_bit_t *) (d_ptr) = 1;
            else if ((strcmp("0", value) == 0)|| (strcasecmp("FALSE", value)) == 0)
                *(hal_bit_t *) (d_ptr) = 0;
            else
                {
                hal_print_msg(RTAPI_MSG_DBG,"value '%s' invalid for bit\n", value);
                retval = -EINVAL;
                }
            break;
        case HAL_FLOAT:
            fval = strtod ( value, &cp );
            if ((*cp != '\0') && (!isspace(*cp)))
                {
                // invalid character(s) in string 
                hal_print_msg(RTAPI_MSG_DBG,"value '%s' invalid for float\n", value);
                retval = -EINVAL;
                }
            else
                *((hal_float_t *) (d_ptr)) = fval;
            break;
        case HAL_S32:
            lval = strtol(value, &cp, 0);
            if ((*cp != '\0') && (!isspace(*cp)))
                {
                // invalid chars in string 
                hal_print_msg(RTAPI_MSG_DBG,"value '%s' invalid for S32\n", value);
                retval = -EINVAL;
                }
            else
                *((hal_s32_t *) (d_ptr)) = lval;
            break;
        case HAL_U32:
            ulval = strtoul(value, &cp, 0);
            if ((*cp != '\0') && (!isspace(*cp)))
                {
                // invalid chars in string 
                hal_print_msg(RTAPI_MSG_DBG,"value '%s' invalid for U32\n", value);
                retval = -EINVAL;
                }
            else
                *((hal_u32_t *) (d_ptr)) = ulval;
            break;
        default:
            // Shouldn't get here, but just in case... 
            hal_print_msg(RTAPI_MSG_DBG,"bad type %d\n", type);
            retval = -EINVAL;
        }
    return retval;
}


int set_sig_value(char *name, char *value)
{
int retval;
hal_sig_t *sig;
hal_type_t type;
void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "setting signal '%s'\n", name);
    // get mutex before accessing shared data 
    rtapi_mutex_get(&(hal_data->mutex));
    // search signal list for name 
    sig = halpr_find_sig_by_name(name);
    if (sig == 0) 
	{
        rtapi_mutex_give(&(hal_data->mutex));
        hal_print_msg(RTAPI_MSG_DBG,"signal '%s' not found\n", name);
        return -EINVAL;
	}
    // found it - does it have a writer? 
    if (sig->writers > 0) 
	{
        rtapi_mutex_give(&(hal_data->mutex));
        hal_print_msg(RTAPI_MSG_DBG,"signal '%s' already has writer(s)\n", name);
        return -EINVAL;
	}
    // no writer, so we can safely set it 
    type = sig->type;
    d_ptr = sig_value(sig);
    retval = set_common(type, d_ptr, value);
    rtapi_mutex_give(&(hal_data->mutex));
    if (retval == 0) 
        hal_print_msg(RTAPI_MSG_DBG,"Signal '%s' set to %s\n", name, value);
     else 
        hal_print_msg(RTAPI_MSG_DBG,"sets failed\n");
    
    return retval;
}


int set_pin_value(char *name, char *value)
{
int retval;
hal_pin_t *pin;
hal_type_t type;
void *d_ptr;

    // get mutex before accessing shared data
    rtapi_mutex_get(&(hal_data->mutex));

    pin = halpr_find_pin_by_name(name);
    if(pin == 0)
        {
        rtapi_mutex_give(&(hal_data->mutex));
        return -EINVAL;
        }
    else // pin
        {
        type = pin->type;
        d_ptr = (void*)&pin->dummysig;
        }

    retval = set_common(type, d_ptr, value);

    rtapi_mutex_give(&(hal_data->mutex));
    if (retval != 0)
	hal_print_msg(RTAPI_MSG_DBG, "Error setting value of pin: %s\n", name);
    return retval;
}

/////////////////  FUNCTION  ///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Multi-part logic, not all going to be used
//
// Setting just pin name to watch and target value to watch for
// will see function run until target reached and stop
//
// Setting as above with iterations set, will run function until
// either the target is reached or number of iterations is reached
// whichever is first
// NB iteration rate depends upon thread speed
//
// Setting as above with forever flag set, will cause the target-value
// pin to continue to be updated after the target is reached and the
// target-reached flag is set, to give a live view of the pin value
////////////////////////////////////////////////////////////////////

static int watch_(void *arg, const hal_funct_args_t *fa)
{
long period __attribute__((unused)) = fa_period(fa);
struct inst_data *ip __attribute__((unused)) = arg;

hal_float_t valuel = 0.0;
char value[16];
static int counter = 0;
static hal_float_t valuef = 0.0;
static hal_bit_t latched = 0;

    if(*(ip->_reset))
	{
	*(ip->_started) = 0;
	*(ip->_stopped) = 0;
	*(ip->_triggered) = latched = 0;
	*(ip->_trigger) = 0;
	*(ip->_reset) = 0;
	*(ip->_value_out) = 0.0;
	// come back next poll and wait for trigger
	return 0;
	}

    if(*(ip->_trigger) && !latched)
	{
	*(ip->_started) = 1;
	*(ip->_stopped) = 0;
	*(ip->_triggered) = latched = 1;
	*(ip->_trigger) = 0;
	}
    
    if( latched ) // we are running
	{
	if(! *(ip->_target_reached)) // OK to continue running
	    {
	    get_pin_value(target_pin_name, value);
	    valuel = atof(value);
	    if(valuef != valuel) // no duplicate tests or prints if unchanged
		{
		*(ip->_value_out) = valuef = valuel;
		// set flag and print message just latched
		if( valuef >= *(ip->_target_value) && (! *(ip->_target_reached)) )
    		    {
        	    hal_print_msg(RTAPI_MSG_DBG, "Target reached:  Value = %f Target = %f",  valuef, *(ip->_target_value) );
		    *(ip->_target_reached) = 1;
		    *(ip->_stopped) = 1;
		    *(ip->_started) = 0;
		    latched = 0;
		    }
		// if still below target continue counter
    		else if( valuef < *(ip->_target_value) )
		    {
		    counter++;
		    hal_print_msg(RTAPI_MSG_DBG,"Counter: %d Last value: %f Target = %f", counter, valuef, *(ip->_target_value) );
		    }
		// if only running for NN iterations
		// may reach iterations limit before value matches target
		if(*(ip->_iterations) && (*(ip->_iterations) <= counter) )
		    {
		    *(ip->_stopped) = 1;
		    *(ip->_started) = 0;
		    latched = 0;
		    }
		}
	    }
	}
    // carry on showing pin value after target reached, but not until triggered if not latched
    else if(*(ip->_forever) && *(ip->_triggered)) 
	{
	get_pin_value(target_pin_name, value);
	*(ip->_value_out) = atof(value);
	}
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int extra_inst_setup(struct inst_data *ip, const char *name,
                            int argc, char* const *argv)
{
int x;

    target_pin_name[0] = '\0';
    preset_name[0] = '\0';
    *(ip->_target_value) = -10;
    *(ip->_target_value_int) = -1;
    *(ip->_iterations) = 0;
    *(ip->_forever) = 0;
    preset_type = 0;

    // parse args here
    for(x = 2; x < argc; x++)
	{
	const char *c = argv[x];
	if((strstr(c, "pin_name=")) != NULL)
	    strcpy(target_pin_name, &c[9]);
	else if((strstr(c, "preset_name=")) != NULL)
	    strcpy(preset_name, &c[12]);
	else if((strstr(c, "target_value=")) != NULL)
	    {
	    *(ip->_target_value) = atof(&c[13]);
	    *(ip->_target_value_int) = atoi(&c[13]);
	    }
	else if((strstr(c, "forever=")) != NULL)
	    *(ip->_forever) = atoi(&c[8]);
	else if((strstr(c, "iterations=")) != NULL)
	    *(ip->_iterations) = atoi(&c[11]);
	else if((strstr(c, "preset_type=")) != NULL)
	    preset_type = atoi(&c[12]);
	}

    hal_print_msg(RTAPI_MSG_DBG,"pin_name = %s, preset_pin_name = %s, preset_type = %d, target_value = %f, forever = %d, iterations = %d", 
		    target_pin_name, preset_name, preset_type, *(ip->_target_value), *(ip->_forever), *(ip->_iterations) );

    char buff[16];
    hal_type_t type;
    int ret = 0;

    // This section is to preset a pin or signal with the target_value passed to the watch component
    // Uses for this include setting the heater component to the temperature required or
    // setting a panel widget to the value required to initialise it.
    // Wherever possible this should be a pin, the signal option arises from horrible FDM configs
    // where signals are created with "meaningful names" and net'd to the pin, instead of just using 
    // the actual pin name to do stuff.

    if(strlen(preset_name))
	{
	if(preset_type == 0) // pin
	    ret = get_pin_type(preset_name, &type);	    
	else
	    ret = get_sig_type(preset_name, &type);

	if(ret != 0)
	    return ret;

	if(type == HAL_FLOAT)
	    sprintf(buff,"%f", *(ip->_target_value));
	else
	    sprintf(buff,"%d", *(ip->_target_value_int));

	if(preset_type == 0) // pin
	    ret = set_pin_value(preset_name, buff);
	else
	    ret = set_sig_value(preset_name, buff);
	}

    return ret;
}


