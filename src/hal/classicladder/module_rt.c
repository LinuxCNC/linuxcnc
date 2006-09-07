/******************************************************************************
 *
 * Copyright (C) 2005 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 * $RCSfile$
 * $Author$
 * $Locker$
 * $Revision$
 * $State$
 * $Date$
 *
 * This is a PLC component for the HAL. It is based on Classic Ladder.
 *
 * Installation of the component (realtime only):
 *
 * insmod classicladder_rt <numRungs=10> <numBits=500> <numWords=50>
 *	<numTimers=8> <numMonostables=8> <numPhysInputs=10>
 *	<numPhysOutputs=50> <numArithmExpr=100> <numSections=10>
 *
 *
 * The following items are exported to the HAL. <channel> is
 * formated as "%02d".
 * The following items are exported to the HAL. <plcId> is
 * formated as "%d". <channel> is formated as "%02d".
 *
 *   Pins:
 *	u8	classicladder.<plcId>.status	// 0=load, 1=stop, 2=run
 *	bit	classicladder.<plcId>.in-<channel>
 *	bit	classicladder.<plcId>.out-<channel>
 *
 *   Functions:
 *	void    classicladder.<plcId>.refresh
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU General
 * Public License as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
 *
 * THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 * ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 * TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 * harming persons must have provisions for completely removing power
 * from all motors, etc, before persons enter any danger area.  All
 * machinery must be designed to comply with local and national safety
 * codes, and the authors of this software can not, and do not, take
 * any responsibility for such compliance.
 *
 * This code was written as part of the EMC HAL project.  For more
 * information, go to www.linuxcnc.org.
 *
 ******************************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif


#include "rtapi.h"			// RTAPI realtime OS API.
#include "rtapi_app.h"			// RTAPI realtime module decls.
#include "hal.h"			// HAL public API decls.

#include "classicladder.h"
#include "global.h"
#include "arrays.h"
#include "vars_access.h"
#include "calc_rt.h"


#ifndef MODULE
#define MODULE
#endif


#ifdef MODULE
// Module information.
MODULE_AUTHOR("Pete Vavaroutsos");
MODULE_DESCRIPTION("PLC component for EMC HAL");
MODULE_LICENSE("GPL");
static long				period = 0;	// Thread period (0 = no thread).
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "thread period (nsecs)");
static int numRungs = 10;
MODULE_PARM(numRungs, "i");
MODULE_PARM_DESC(numRungs, "Number of rungs to allocate");
static int numBits = 500;
MODULE_PARM(numBits, "i");
MODULE_PARM_DESC(numBits, "Number of bits to allocate");
static int numWords = 50;
MODULE_PARM(numWords, "i");
MODULE_PARM_DESC(numWords, "Number of words to allocate");
static int numTimers = 8;
MODULE_PARM(numTimers, "i");
MODULE_PARM_DESC(numTimers, "Number of timers to allocate");
static int numMonostables = 8;
MODULE_PARM(numMonostables, "i");
MODULE_PARM_DESC(numMonostables, "Number of monostables to allocate");
static int numPhysInputs = 10;
MODULE_PARM(numPhysInputs, "i");
MODULE_PARM_DESC(numPhysInputs, "Number of inputs to allocate");
static int numPhysOutputs = 50;
MODULE_PARM(numPhysOutputs, "i");
MODULE_PARM_DESC(numPhysOutputs, "Number of outputs to allocate");
static int numArithmExpr = 100;
MODULE_PARM(numArithmExpr, "i");
MODULE_PARM_DESC(numArithmExpr, "Number of arithmetic expressions to allocate");
static int numSections = 10;
MODULE_PARM(numSections, "i");
MODULE_PARM_DESC(numSections, "Number of sections to allocate");
#endif // MODULE


/******************************************************************************
 * PLC OBJECT
 *
 * This object contains the data for one PLC object.
 *
 ******************************************************************************/

typedef struct {
    // Pins.
    hal_bit_t				*pValue;
} DigitalInPinsParams;

typedef struct {
    // Pins.
    hal_bit_t				*pValue;
} DigitalOutPinsParams;

typedef struct {
    hal_u8_t				*pStatus;
    DigitalInPinsParams			*pInPins;
    DigitalOutPinsParams		*pOutPins;
} Plc;


// These methods are used for initialization.
static int Plc_Init(Plc *this, int componentId);
static int Plc_ExportPinsParametersFunctions(Plc *this, int componentId, int boardId);

// These methods are exported to the HAL.
static void Plc_Refresh(void *this, long period);

// Private helper methods.
static void Plc_ReadPhysicalInputs(Plc *this);
static void Plc_WritePhysicalOutputs(Plc *this);


/******************************************************************************
 * COMPONENT OBJECT
 *
 * This object contains all the data for this HAL component.
 *
 ******************************************************************************/

#define MAX_PLCS			1		// Classic Ladder is too
							// broken to support more.

typedef struct {
    int					componentId;	// HAL component ID.
    Plc					*plcTable[MAX_PLCS];
} Component;

static Component			component;


/******************************************************************************
 * INIT AND EXIT CODE
 ******************************************************************************/

int
rtapi_app_main(void)
{
    int					i;
    Plc					*pPlc;

    // Connect to the HAL.
    component.componentId = hal_init("classicladder_rt");
    if (component.componentId < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "CLRT: ERROR: hal_init() failed\n");
	return(-1);
    }

    for(i = 0; i < MAX_PLCS; i++){
	component.plcTable[i] = NULL;
    }

    for(i = 0; i < MAX_PLCS; i++){
	// Allocate memory for plc object.
	pPlc = hal_malloc(sizeof(Plc));

	if (pPlc == NULL) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "CLRT: ERROR: hal_malloc() failed\n");
	    hal_exit(component.componentId);
	    return(-1);
	}

	// Save pointer to plc object.
	component.plcTable[i] = pPlc;

	// Initialize PLC.
	if(Plc_Init(pPlc, component.componentId)){
	   hal_exit(component.componentId);
	   return(-1);
	}

	// Export pins, parameters, and functions.
	if(Plc_ExportPinsParametersFunctions(pPlc, component.componentId, i++)){
	    hal_exit(component.componentId);
	    return(-1);
	}
    }

    // Was 'period' specified in the insmod command?
    if (period > 0) {

	// Create a thread.
	if (hal_create_thread("classicladder.thread", period, 1) < 0){
	    rtapi_print_msg(RTAPI_MSG_ERR, "CLRT: ERROR: hal_create_thread() failed\n");
	    hal_exit(component.componentId);
	    return(-1);
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "CLRT: created %d uS thread\n", period / 1000);
	}
    }

    return(0);
}


void
rtapi_app_exit(void)
{
    int					i;
    Plc					*pPlc;

    ClassicLadderFreeAll(component.componentId);

    hal_exit(component.componentId);

    for(i = 0; i < MAX_PLCS; i++){
	if((pPlc = component.plcTable[i]) != NULL){
#if 0
	    // TODO: Free memory when HAL supports free.
	    if(pPlc->pInPins != NULL)
		hal_free(pPlc->pInPins);

	    if(pPlc->pOutPins != NULL)
		hal_free(pPlc->pOutPins);

	    hal_free(pPlc);
#endif
	}
    }
}


/******************************************************************************
 * PLC OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

/*
 * LOCAL FUNCTIONS
 */

static int
Plc_Init(Plc *this, int componentId)
{
    plc_sizeinfo_s			sizeInfo;

    this->pOutPins = NULL;

    // Allocate data for input pins.
    this->pInPins = hal_malloc(sizeof(DigitalInPinsParams) * numPhysInputs);

    if (this->pInPins == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "CLRT: ERROR: hal_malloc() failed\n");
	return(-1);
    }

    // Allocate data for output pins.
    this->pOutPins = hal_malloc(sizeof(DigitalOutPinsParams) * numPhysOutputs);

    if (this->pOutPins == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "CLRT: ERROR: hal_malloc() failed\n");
	return(-1);
    }

    // Copy module parameters for allocation into Classic Ladder structure.
    sizeInfo.nbr_rungs = numRungs;
    sizeInfo.nbr_bits = numBits;
    sizeInfo.nbr_words = numWords;
    sizeInfo.nbr_timers = numTimers;
    sizeInfo.nbr_monostables = numMonostables;
    sizeInfo.nbr_phys_inputs = numPhysInputs;
    sizeInfo.nbr_phys_outputs = numPhysOutputs;
    sizeInfo.nbr_arithm_expr = numArithmExpr;
    sizeInfo.nbr_sections = numSections;

    if(!ClassicLadderAllocAll(componentId, &sizeInfo)){
	rtapi_print_msg(RTAPI_MSG_ERR, "CLRT: ERROR: ClassicLadderAllocAll() failed\n");
	return(-1);
    }

    return(0);
}


static int
Plc_ExportPinsParametersFunctions(Plc *this, int componentId, int plcId)
{
    int					msgLevel, error, channel;
    char				name[HAL_NAME_LEN + 2];

    // This function exports a lot of stuff, which results in a lot of
    // logging if msg_level is at INFO or ALL. So we save the current value
    // of msg_level and restore it later.  If you actually need to log this
    // function's actions, change the second line below.
    msgLevel = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    // Export pins and parameters.
    rtapi_snprintf(name, HAL_NAME_LEN, "classicladder.%d.status", plcId);
    if((error = hal_pin_u8_new(name, HAL_OUT, &(this->pStatus), componentId)) == 0){

	// Init pin.
	*(this->pStatus) = InfosGene->LadderState;
    }

    for(channel = 0; channel < numPhysInputs && !error; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "classicladder.%d.in-%02d", plcId, channel);
	if((error = hal_pin_bit_new(name, HAL_IN, &(this->pInPins[channel].pValue), componentId)) != 0)
	    break;

	// Init pin.
	*(this->pInPins[channel].pValue) = 0;
    }

    for(channel = 0; channel < numPhysOutputs && !error; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "classicladder.%d.out-%02d", plcId, channel);
	if((error = hal_pin_bit_new(name, HAL_OUT, &(this->pOutPins[channel].pValue), componentId)) != 0)
	    break;

	// Init pin.
	*(this->pOutPins[channel].pValue) = 0;
    }

    // Export functions.
    if(!error){
	rtapi_snprintf(name, HAL_NAME_LEN, "classicladder.%d.refresh", plcId);
	error = hal_export_funct(name, Plc_Refresh, this, 0, 0, componentId);
    }

    if(error){
	rtapi_print_msg(RTAPI_MSG_ERR, "CLRT: ERROR: export digital in failed\n");
    }

    // Restore saved message level.
    rtapi_set_msg_level(msgLevel);

    return(error);
}


/*
 * HAL EXPORTED FUNCTIONS
 */

static void
Plc_Refresh(void *arg, long period)
{
    Plc					*this = (Plc *)arg;

    if((*(this->pStatus) = InfosGene->LadderState) == STATE_RUN){
	Plc_ReadPhysicalInputs(this);
	RefreshAllRungs(period);
	Plc_WritePhysicalOutputs(this);
    }
}


/*
 * PRIVATE HELPER FUNCTIONS
 */

void
Plc_ReadPhysicalInputs(Plc *this)
{
    int					i;

    // Copy HAL pin data to Classic Ladder structure.
    for(i = 0; i < numPhysInputs; i++){
	WriteVar(VAR_PHYS_INPUT, i, *(this->pInPins[i].pValue));
    }
}


void
Plc_WritePhysicalOutputs(Plc *this)
{
    int					i;

    // Copy HAL pin data from Classic Ladder structure.
    for(i = 0; i < numPhysOutputs; i++){
	 *(this->pOutPins[i].pValue) = ReadVar(VAR_PHYS_OUTPUT, i);
    }
}
