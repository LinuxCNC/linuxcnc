/* Classic Ladder Project */
/* Copyright (C) 2001-2003 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* February 2001 */
/* Last update : 14 April 2003 */
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

#if !defined(MODULE) && defined(GTK_INTERFACE)
#include <gtk/gtk.h>
#include "classicladder_gtk.h"
#endif

#include "rtapi.h"
#include "classicladder.h"
#include "global.h"


#ifndef MODULE
void InitVars(void)
{
    int NumVar;
    for (NumVar=0; NumVar<SIZE_VAR_ARRAY; NumVar++)
        VarArray[NumVar] = FALSE;
    for (NumVar=0; NumVar<SIZE_VAR_WORD_ARRAY; NumVar++)
        VarWordArray[NumVar] = 0;
    /* to tell the GTK application to refresh the bits */
    InfosGene->CmdRefreshVarsBits = TRUE;
}
#endif

int ReadVar(int TypeVar,int Offset)
{
    switch(TypeVar)
    {
	case VAR_MEM_BIT:
	    return VarArray[Offset];
	case VAR_TIMER_DONE:
	    return TimerArray[Offset].OutputDone;
	case VAR_TIMER_RUNNING:
	    return TimerArray[Offset].OutputRunning;
	case VAR_MONOSTABLE_RUNNING:
	    return MonostableArray[Offset].OutputRunning;
#ifdef SEQUENTIAL_SUPPORT
	case VAR_STEP_ACTIVITY:
//		return Sequential->Step[ Offset ].Activated;
	    return VarArray[NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS+Offset];
#endif
	case VAR_PHYS_INPUT:
	    return VarArray[NBR_BITS+Offset];
	case VAR_PHYS_OUTPUT:
	    return VarArray[NBR_BITS+NBR_PHYS_INPUTS+Offset];
	case VAR_MEM_WORD:
	    return VarWordArray[Offset];
#ifdef SEQUENTIAL_SUPPORT
	case VAR_STEP_TIME:
//		return Sequential->Step[ Offset ].TimeActivated/1000;
	    return VarWordArray[NBR_WORDS+Offset];
#endif
	default:
	    rtapi_print_msg(RTAPI_MSG_ERR, "!!! Error : Type not found in ReadVar()\n");
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
	case VAR_PHYS_INPUT:
	    VarArray[NBR_BITS+NumVar] = Value;
	    break;
	case VAR_PHYS_OUTPUT:
	    VarArray[NBR_BITS+NBR_PHYS_INPUTS+NumVar] = Value;
	    break;
#ifdef SEQUENTIAL_SUPPORT
	case VAR_STEP_ACTIVITY:
	    VarArray[NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS+NumVar] = Value;
	    break;
#endif
	case VAR_MEM_WORD:
	    VarWordArray[NumVar] = Value;
	    break;
#ifdef SEQUENTIAL_SUPPORT
	case VAR_STEP_TIME:
	    VarWordArray[NBR_WORDS+NumVar] = Value;
	    break;
#endif
	default:
	    rtapi_print_msg(RTAPI_MSG_ERR, "!!! Error : Type not found in WriteVar()\n");
	    break;
    }
    switch(TypeVar)
    {
	case VAR_MEM_BIT:
	case VAR_PHYS_INPUT:
	case VAR_PHYS_OUTPUT:
#ifdef GTK_INTERFACE
	    RefreshOneBoolVar( TypeVar, NumVar, Value );
#else
	    /* to tell the GTK application to refresh the bits */
	    InfosGene->CmdRefreshVarsBits = TRUE;
#endif
	    break;
    }
}
