/* Classic Ladder Project */
/* Copyright (C) 2001-2004 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* October 2002 */
/* ---------------------------------- */
/* Sequential language - Refresh page */
/* ---------------------------------- */
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

#include "rtapi.h"
#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "calc_sequential.h"


void InitSequential( void )
{
	int NumStep;
	int NumTrans;
	int NumSwitch;
	for( NumStep=0; NumStep<NBR_STEPS; NumStep++ )
	{
		Sequential->Step[ NumStep ].InitStep = FALSE;
		Sequential->Step[ NumStep ].StepNumber = 0;
		Sequential->Step[ NumStep ].NumPage = -1;
		Sequential->Step[ NumStep ].PosiX = 0;
		Sequential->Step[ NumStep ].PosiY = 0;
		Sequential->Step[ NumStep ].Activated = FALSE;
		Sequential->Step[ NumStep ].TimeActivated = 0;
		Sequential->Step[ NumStep ].OffDrawCrossStep = 0;
	}
	for( NumTrans=0; NumTrans<NBR_TRANSITIONS; NumTrans++ )
	{
		Sequential->Transition[ NumTrans ].VarTypeCondi = 0;
		Sequential->Transition[ NumTrans ].VarNumCondi = 0;
		for( NumSwitch=0; NumSwitch<NBR_SWITCHS_MAX; NumSwitch++ )
		{
			Sequential->Transition[ NumTrans ].NumStepToActiv[ NumSwitch ] = -1;
			Sequential->Transition[ NumTrans ].NumStepToDesactiv[ NumSwitch ] = -1;
			Sequential->Transition[ NumTrans ].NumTransLinkedForStart[ NumSwitch ] = -1;
			Sequential->Transition[ NumTrans ].NumTransLinkedForEnd[ NumSwitch ] = -1;
		}
		Sequential->Transition[ NumTrans ].NumPage = -1;
		Sequential->Transition[ NumTrans ].PosiX = 0;
		Sequential->Transition[ NumTrans ].PosiY = 0;
		Sequential->Transition[ NumTrans ].Activated = FALSE;
	}

}

/* set active all the init steps (called at start and after modification with the editor) */
void PrepareSequential( void )
{
	int NumStep,NumTrans;
	for( NumStep=0; NumStep<NBR_STEPS; NumStep++ )
	{
		Sequential->Step[ NumStep ].Activated = FALSE;
		Sequential->Step[ NumStep ].TimeActivated = 0;
		if ( Sequential->Step[ NumStep ].InitStep )
			Sequential->Step[ NumStep ].Activated = TRUE;
	}
	for( NumTrans=0; NumTrans<NBR_TRANSITIONS; NumTrans++ )
		Sequential->Transition[ NumTrans ].Activated = FALSE;
}
