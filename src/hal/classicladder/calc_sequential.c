/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
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

#ifdef MODULE
#include <linux/string.h>
#else
#include <stdio.h>
#include <string.h>
#endif
#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "calc_sequential.h"

void InitSequential( void )
{
	int NumStep;
	int NumTrans;
	int NumSwitch;
	int NumSeqComment;
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
	for( NumSeqComment=0; NumSeqComment<NBR_SEQ_COMMENTS; NumSeqComment++ )
	{
		Sequential->SeqComment[ NumSeqComment ].NumPage = -1;
		Sequential->SeqComment[ NumSeqComment ].PosiX = 0;
		Sequential->SeqComment[ NumSeqComment ].PosiY = 0;
		Sequential->SeqComment[ NumSeqComment ].Comment[ 0 ] = '\0';
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

int RefreshTransi( StrTransition * pTransi )
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
void RefreshStepsVars( void )
{
	int NumStep;
	for( NumStep=0; NumStep<NBR_STEPS; NumStep++ )
	{
		if ( Sequential->Step[ NumStep ].Activated )
			Sequential->Step[ NumStep ].TimeActivated += InfosGene->GeneralParams.PeriodicRefreshMilliSecs;
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

