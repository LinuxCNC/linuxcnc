/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */
/* --------------------------------------- */
/* Access a variable for reading / writing */
/* --------------------------------------- */
/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, write to the Free Software */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef GTK_INTERFACE
#include <gtk/gtk.h>
#include "classicladder_gtk.h"
#endif

#include "config.h"



#if defined(MODULE) 
#include <linux/kernel.h>
#include <linux/module.h>
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#include "classicladder.h"
#include "global.h"


void InitVars(void)
{
    int NumVar;
    for (NumVar=0; NumVar<SIZE_VAR_ARRAY; NumVar++)
        VarArray[NumVar] = FALSE;
    for (NumVar=0; NumVar<SIZE_VAR_WORD_ARRAY; NumVar++)
        VarWordArray[NumVar] = 0;
    for (NumVar=0; NumVar<SIZE_VAR_FLOAT_ARRAY; NumVar++)
        VarFloatArray[NumVar] = 0;
    /* to tell the GTK application to refresh the bits */
    InfosGene->CmdRefreshVarsBits = TRUE;
	InfosGene->HideGuiState = FALSE;
}

int ReadVar(int TypeVar,int Offset)
{
	switch(TypeVar)
	{
		case VAR_MEM_BIT:
			return VarArray[Offset];
                case VAR_ERROR_BIT:
			return VarArray[NBR_STEPS+NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS+Offset];
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case VAR_TIMER_DONE:
			return TimerArray[Offset].OutputDone;
		case VAR_TIMER_RUNNING:
			return TimerArray[Offset].OutputRunning;
		case VAR_MONOSTABLE_RUNNING:
			return MonostableArray[Offset].OutputRunning;
#endif
		case VAR_COUNTER_DONE:
			return CounterArray[Offset].OutputDone;
		case VAR_COUNTER_EMPTY:
			return CounterArray[Offset].OutputEmpty;
		case VAR_COUNTER_FULL:
			return CounterArray[Offset].OutputFull;
		case VAR_TIMER_IEC_DONE:
			return NewTimerArray[Offset].Output;
                case VAR_PHYS_WORD_INPUT:
			return VarWordArray[NBR_WORDS+Offset];
		case VAR_PHYS_WORD_OUTPUT:
			return VarWordArray[NBR_WORDS+NBR_PHYS_WORDS_INPUTS+Offset];
#ifdef SEQUENTIAL_SUPPORT
		case VAR_STEP_ACTIVITY:
//            return Sequential->Step[ Offset ].Activated;
			return VarArray[NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS+Offset];
#endif
		case VAR_PHYS_INPUT:
			return VarArray[NBR_BITS+Offset];
		case VAR_PHYS_OUTPUT:
			return VarArray[NBR_BITS+NBR_PHYS_INPUTS+Offset];
                case VAR_PHYS_FLOAT_INPUT:
			return VarFloatArray[Offset];
		case VAR_PHYS_FLOAT_OUTPUT:
			return VarFloatArray[NBR_PHYS_FLOAT_INPUTS+Offset];
		case VAR_MEM_WORD:
			return VarWordArray[Offset];
#ifdef SEQUENTIAL_SUPPORT
		case VAR_STEP_TIME:
//            return Sequential->Step[ Offset ].TimeActivated/1000;
			return VarWordArray[NBR_WORDS+Offset];
#endif
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case VAR_TIMER_PRESET:
			return TimerArray[Offset].Preset/TimerArray[Offset].Base;
		case VAR_TIMER_VALUE:
			return TimerArray[Offset].Value/TimerArray[Offset].Base;
		case VAR_MONOSTABLE_PRESET:
			return MonostableArray[Offset].Preset/MonostableArray[Offset].Base;
		case VAR_MONOSTABLE_VALUE:
			return MonostableArray[Offset].Value/MonostableArray[Offset].Base;
#endif
		case VAR_COUNTER_PRESET:
			return CounterArray[Offset].Preset;
		case VAR_COUNTER_VALUE:
			return CounterArray[Offset].Value;
		case VAR_TIMER_IEC_PRESET:
			return NewTimerArray[Offset].Preset;
		case VAR_TIMER_IEC_VALUE:
			return NewTimerArray[Offset].Value;
        default:
            debug_printf("!!! Error : Type (=%d) not found in ReadVar(%d,%d)\n", TypeVar, TypeVar, Offset);
    }
    return 0;
}

void WriteVar(int TypeVar,int NumVar,int Value)
{
	switch(TypeVar)
	{
		case VAR_MEM_BIT:
			VarArray[NumVar] = Value;
			break;
                case VAR_ERROR_BIT:
			VarArray[NBR_STEPS+NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS+NumVar] = Value;
			break;
		case VAR_COUNTER_DONE:
			CounterArray[NumVar].OutputDone = Value;
			break;
		case VAR_COUNTER_EMPTY:
			CounterArray[NumVar].OutputEmpty = Value;
			break;
		case VAR_COUNTER_FULL:
			CounterArray[NumVar].OutputFull = Value;
			break;
		case VAR_TIMER_IEC_DONE:
			NewTimerArray[NumVar].Output = Value;
			break;
#ifdef SEQUENTIAL_SUPPORT
		case VAR_STEP_ACTIVITY:
			VarArray[NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS+NumVar] = Value;
			break;
#endif
		case VAR_PHYS_INPUT:
			VarArray[NBR_BITS+NumVar] = Value;
			break;
		case VAR_PHYS_OUTPUT:
			VarArray[NBR_BITS+NBR_PHYS_INPUTS+NumVar] = Value;
			break;
		case VAR_MEM_WORD:
			VarWordArray[NumVar] = Value;
			break;
                case VAR_PHYS_WORD_INPUT:
			VarWordArray[NBR_WORDS+NumVar] = Value;
			break;
		case VAR_PHYS_WORD_OUTPUT:
			VarWordArray[NBR_WORDS+NBR_PHYS_WORDS_INPUTS+NumVar] = Value;
			break;
                case VAR_PHYS_FLOAT_INPUT:
			VarFloatArray[NumVar] = Value;
			break;
		case VAR_PHYS_FLOAT_OUTPUT:
			VarFloatArray[NBR_PHYS_FLOAT_INPUTS+NumVar] = Value;
			break;
#ifdef SEQUENTIAL_SUPPORT
	        case VAR_STEP_TIME:
			VarWordArray[NBR_WORDS+NumVar] = Value;
			break;
#endif
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case VAR_TIMER_PRESET:
			TimerArray[NumVar].Preset = Value * TimerArray[NumVar].Base;
            break;
		case VAR_MONOSTABLE_PRESET:
			MonostableArray[NumVar].Preset = Value * MonostableArray[NumVar].Base;
            break;
#endif
		case VAR_COUNTER_PRESET:
			CounterArray[NumVar].Preset = Value;
			break;
		case VAR_COUNTER_VALUE:
			CounterArray[NumVar].Value = Value;
			break;
		case VAR_TIMER_IEC_PRESET:
			NewTimerArray[NumVar].Preset = Value;
            break;
		case VAR_TIMER_IEC_VALUE:
			NewTimerArray[NumVar].Value = Value;
			break;
		default:
			debug_printf("!!! Error : Type (=%d) not found in WriteVar(%d,%d)\n", TypeVar, TypeVar, NumVar);
			break;
	}

	switch(TypeVar)
	{
		case VAR_MEM_BIT:
		case VAR_PHYS_INPUT:
		case VAR_PHYS_OUTPUT:
			InfosGene->CmdRefreshVarsBits = TRUE;
			break;
	}
}

/* these are only useful for the MAT-connected version */
void DoneVars(void) {}
void CycleStart(void) {}
void CycleEnd(void) {}

