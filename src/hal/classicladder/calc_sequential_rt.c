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
#include "calc_sequential_rt.h"


static int RefreshTransi( StrTransition * pTransi )
{
	int HasChanged = FALSE;

	pTransi->Activated = ReadVar( pTransi->VarTypeCondi, pTransi->VarNumCondi );
	/* Condi associated true ? */
	if ( pTransi->Activated )
	{
		int AllStepsOn = TRUE;
		int ScanStep = 0;
		/* Steps to desactivate are activated ? */
		while( ScanStep<NBR_SWITCHS_MAX && pTransi->NumStepToDesactiv[ ScanStep ]!=-1 && AllStepsOn )
		{
			if ( !Sequential->Step[ pTransi->NumStepToDesactiv[ ScanStep ] ].Activated )
				AllStepsOn = FALSE;
			ScanStep++;
		}

		/* Transition is on ? */
		if ( AllStepsOn )
		{
			HasChanged = TRUE;
			/* Reset all the steps to desactivate */
			ScanStep = 0;
			while( ScanStep<NBR_SWITCHS_MAX && pTransi->NumStepToDesactiv[ ScanStep ]!=-1 )
			{
				Sequential->Step[ pTransi->NumStepToDesactiv[ ScanStep ] ].Activated = FALSE;
				ScanStep++;
			}

			/* Set all the steps to activate */
			ScanStep = 0;
			while( ScanStep<NBR_SWITCHS_MAX && pTransi->NumStepToActiv[ ScanStep ]!=-1 )
			{
				Sequential->Step[ pTransi->NumStepToActiv[ ScanStep ] ].Activated = TRUE;
				ScanStep++;
			}
		}

	}
	return HasChanged;
}

/* refresh vars of the steps (activity+time) */
static void RefreshStepsVars( void )
{
	int NumStep;
	for( NumStep=0; NumStep<NBR_STEPS; NumStep++ )
	{
		if ( Sequential->Step[ NumStep ].Activated )
			Sequential->Step[ NumStep ].TimeActivated += TIME_REFRESH_RUNG_MS;
		else
			Sequential->Step[ NumStep ].TimeActivated = 0;

		/* refresh the vars for the step number associated to this step */
		WriteVar( VAR_STEP_ACTIVITY, Sequential->Step[ NumStep ].StepNumber, Sequential->Step[ NumStep ].Activated );
		WriteVar( VAR_STEP_TIME, Sequential->Step[ NumStep ].StepNumber, Sequential->Step[ NumStep ].TimeActivated/1000 );
	}
}


void RefreshSequentialPage( int PageNbr )
{
	int ScanTransi;
	StrTransition * pTransi;
	int StateChanged;
	int LoopSecurity = 0;
	/* we loop here while some transitions have changed of state */
	do
	{
		StateChanged = FALSE;
		for( ScanTransi=0; ScanTransi<NBR_TRANSITIONS; ScanTransi++ )
		{
			pTransi = &Sequential->Transition[ ScanTransi ];
			/* transition for the page under refresh ? */
			if( pTransi->NumPage==PageNbr )
			{
				if ( RefreshTransi( pTransi ) )
					StateChanged = TRUE;
			}
		}
		LoopSecurity++;
	}
	while( StateChanged && LoopSecurity<50 );
	RefreshStepsVars( );
}
