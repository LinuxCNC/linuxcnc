/* ClassicLadder Realtime module for emc2/hal */
/* This file is based on the module_rtlinux.c by Marc Le Douarain */

/* Classic Ladder Project */
/* Copyright (C) 2001 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* mavati@club-internet.fr */
/* Copyright (C) 2006 Jeff Epler */
/* jepler@unpy.net */

/*
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_errno.h"
#include "hal.h"

#include "classicladder.h"
#include "global.h"
#include "calc.h"
#include "vars_access.h"

MODULE_LICENSE("LGPL");
MODULE_AUTHOR("Marc Le Douarain");
MODULE_DESCRIPTION("ClassicLadder HAL module");

int comedi_to_open_mask;
#ifdef DYNAMIC_PLCSIZE
int numRungs=NBR_RUNGS_DEF, numBits=NBR_BITS_DEF,numWords=NBR_WORDS_DEF, numTimers=NBR_TIMERS_DEF, numMonostables=NBR_MONOSTABLES_DEF;
int numPhysInputs=NBR_PHYS_INPUTS_DEF, numPhysOutputs=NBR_PHYS_OUTPUTS_DEF, numArithmExpr=NBR_ARITHM_EXPR_DEF, numSections=NBR_SECTIONS_DEF;
RTAPI_MP_INT(numRungs, "i");
RTAPI_MP_INT(numBits, "i");
RTAPI_MP_INT(numWords, "i");
RTAPI_MP_INT(numTimers, "i");
RTAPI_MP_INT(numMonostables, "i");
RTAPI_MP_INT(numPhysInputs, "i");
RTAPI_MP_INT(numPhysOutputs, "i");
RTAPI_MP_INT(numArithmExpr, "i");
RTAPI_MP_INT(numSections, "i");
#else
#define numPhysInputs InfosGene->SizesInfos.nbr_phys_inputs
#define numPhysOutputs InfosGene->SizesInfos.nbr_phys_outputs
#endif

hal_bit_t **hal_inputs;
hal_bit_t **hal_outputs;

extern plc_sizeinfo_s sinfo;

#define TIME_REFRESH_RUNG_NS (1000 * 1000 * (TIME_REFRESH_RUNG_MS))

void HalReadPhysicalInputs(void) {
	int i;
	for( i=0; i<InfosGene->SizesInfos.nbr_phys_inputs; i++) {
		WriteVar(VAR_PHYS_INPUT, i, *hal_inputs[i]);
	}
}

void HalWritePhysicalOutputs(void) {
	int i;
	for( i=0; i<InfosGene->SizesInfos.nbr_phys_outputs; i++) {
		*(hal_outputs[i]) = ReadVar(VAR_PHYS_OUTPUT, i);
	}
}

static void hal_task(void *arg, long period) {
	/* XXX: TIME_REFRESH_RUNG_MS is a compile-time constant.  suck it. */
	static long left = TIME_REFRESH_RUNG_NS;
	left = left - period;
	if(left < 0) {
                unsigned long t0 = rtapi_get_time(), t1;
		left += TIME_REFRESH_RUNG_NS;

		if (InfosGene->LadderState==STATE_RUN)
		{

			HalReadPhysicalInputs();

			RefreshAllRungs();

			HalWritePhysicalOutputs();

		}
                t1 = rtapi_get_time();
                InfosGene->DurationOfLastScan = t1 - t0;
	}
}

extern void CopySizesInfosFromModuleParams( void );

int rtapi_app_main(void) {
	int result, i;
        CopySizesInfosFromModuleParams();

	compId = hal_init("classicladder_rt");
	if(compId < 0) return compId;
	result = hal_export_funct("classicladder.0.refresh", hal_task, 0, 1, 0, compId);
	if(result < 0) {
error:
		hal_exit(compId);
		return result;
	}

	hal_inputs = hal_malloc(sizeof(hal_bit_t*) * numPhysInputs);
	if(!hal_inputs) { result = -ENOMEM; goto error; }
	hal_outputs = hal_malloc(sizeof(hal_bit_t*) * numPhysOutputs);
	if(!hal_outputs) { result = -ENOMEM; goto error; }

	for(i=0; i<numPhysInputs; i++) {
		result = hal_pin_bit_newf(HAL_IN, &hal_inputs[i], compId,
				"classicladder.0.in-%02d", i);
		if(result < 0) goto error;
	}

	for(i=0; i<numPhysOutputs; i++) {
		result = hal_pin_bit_newf(HAL_OUT, &hal_outputs[i], compId,
				"classicladder.0.out-%02d", i);
		if(result < 0) goto error;
	}
	hal_ready(compId);

	ClassicLadderAllocAll( );
	return 0;
}

void rtapi_app_exit(void) {
	ClassicLadderFreeAll( );
}

void CopySizesInfosFromModuleParams( void )
{
#ifdef DYNAMIC_PLCSIZE
	if ( numRungs>0 )
		sinfo.nbr_rungs = numRungs;
	if ( numBits>0 )
		sinfo.nbr_bits = numBits;
	if ( numWords>0 )
		sinfo.nbr_words = numWords;
	if ( numTimers>0 )
		sinfo.nbr_timers = numTimers;
	if ( numMonostables>0 )
		sinfo.nbr_monostables = numMonostables;
	if ( numPhysInputs>0 )
		sinfo.nbr_phys_inputs = numPhysInputs;
	if ( numPhysOutputs>0 )
		sinfo.nbr_phys_outputs = numPhysOutputs;
	if ( numArithmExpr>0 )
		sinfo.nbr_arithm_expr = numArithmExpr;
	if ( numSections>0 )
		sinfo.nbr_sections = numSections;
#endif
}
