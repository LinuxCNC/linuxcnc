/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* December 2003 */
/* --------------------------- */
/* Editor for Sequential Pages */
/* --------------------------- */
/* This part of the editor is the one who will not change even if if we use */
/* another gui instead of gtk... who know? */
/* ------------------------------------------------------------- */
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


#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "classicladder.h"
#include "global.h"
#include "drawing.h"
#include "edit.h"
#include "editproperties_gtk.h"
#include "classicladder_gtk.h"
#include "calc_sequential.h"
#include "vars_names.h"
#include "edit_sequential.h"

/* We modify the datas in this variable. It is only
   after clicking on apply that they are used */
StrSequential EditSeqDatas;

int TypeSeqEleEdited = -1;
int OffsetSeqEleEdited = 0;
char TopSeqEleEdited = FALSE;

/* for elements requiring many clicks to be done (link, multi-steps, 'Or' in transitions) */
int CptNbrClicksDone = 0;
int NumElementSelectedInToolBarBak = -1;

void LoadSeqElementProperties( void )
{
	char TextToWrite[100];
	int NumParam;
	for(NumParam=0;NumParam<NBR_PARAMS_PER_OBJ;NumParam++)
		SetProperty(NumParam,"---","");
	if ( TypeSeqEleEdited!=-1 )
	{
		switch( TypeSeqEleEdited )
		{
			case ELE_SEQ_STEP:
				sprintf( TextToWrite, "%d", EditSeqDatas.Step[ OffsetSeqEleEdited ].StepNumber );
				SetProperty(0,"Step Nbr",TextToWrite);
				break;
			case ELE_SEQ_TRANSITION:
				strcpy( TextToWrite, CreateVarName( EditSeqDatas.Transition[ OffsetSeqEleEdited ].VarTypeCondi,
					EditSeqDatas.Transition[ OffsetSeqEleEdited ].VarNumCondi ) );
				SetProperty(0,"Variable",TextToWrite);
				break;
			case ELE_SEQ_COMMENT:
				SetProperty(0, "Comment", EditSeqDatas.SeqComment[ OffsetSeqEleEdited ].Comment );
				break;
		}
	}
}

void SaveSeqElementProperties( void )
{
	int StepNbr;
	if ( TypeSeqEleEdited!=-1 )
	{
		switch( TypeSeqEleEdited )
		{
			case ELE_SEQ_STEP:
				if ( TextToNumber( GetProperty(0),0,9999,&StepNbr ) )
					EditSeqDatas.Step[ OffsetSeqEleEdited ].StepNumber = StepNbr;
				break;
			case ELE_SEQ_TRANSITION:
				if ( TextParserForAVar( GetProperty(0),&EditSeqDatas.Transition[ OffsetSeqEleEdited ].VarTypeCondi,
							&EditSeqDatas.Transition[ OffsetSeqEleEdited ].VarNumCondi, NULL, FALSE/*PartialNames*/ )==FALSE )
				{
					if (ErrorMessageVarParser)
						ShowMessageBox("Error",ErrorMessageVarParser,"Ok");
					else
						ShowMessageBox( "Error", "Unknown variable...", "Ok" );
				}
				break;
			case ELE_SEQ_COMMENT:
				strncpy( EditSeqDatas.SeqComment[ OffsetSeqEleEdited ].Comment, GetProperty(0), SEQ_COMMENT_LGT );
				EditSeqDatas.SeqComment[ OffsetSeqEleEdited ].Comment[ SEQ_COMMENT_LGT-1 ] = '\0';
				break;
		}
		/* display back to show what we have really understand... */
		LoadSeqElementProperties( );
	}
}

void ModifyCurrentSeqPage()
{
	memcpy( &EditSeqDatas, Sequential, sizeof( StrSequential ) );
	EditDatas.ModeEdit = TRUE;
//	autorize_prevnext_buttons(FALSE);
}

void CancelSeqPageEdited()
{
	EditDatas.ModeEdit = FALSE;
	EditDatas.NumElementSelectedInToolBar = -1;
	TypeSeqEleEdited = -1;
	LoadSeqElementProperties( );
//	autorize_prevnext_buttons(TRUE);
}

void ApplySeqPageEdited()
{
	// passing in STOP and waiting not under calc...
	StopRunIfRunning( );
	memcpy( Sequential, &EditSeqDatas, sizeof( StrSequential ) );
	// passing in RUN now...
	RunBackIfStopped( );

	EditDatas.ModeEdit = FALSE;
	EditDatas.NumElementSelectedInToolBar = -1;
	TypeSeqEleEdited = -1;
	LoadSeqElementProperties( );
//	autorize_prevnext_buttons(TRUE);
	PrepareSequential( );
	InfosGene->AskConfirmationToQuit = TRUE;
}

int SearchStepElement( int PageNumber, int PositionX, int PositionY )
{
	int ScanStep;
	StrStep * pStep;
	int Result = -1;
printf("step search posiX=%d, posiY=%d ; ", PositionX, PositionY );
	for( ScanStep=0; ScanStep<NBR_STEPS; ScanStep++ )
	{
		pStep = &EditSeqDatas.Step[ ScanStep ];
		if ( pStep->NumPage==PageNumber )
		{
			if ( pStep->PosiX==PositionX && pStep->PosiY==PositionY )
				Result = ScanStep;
		}
	}
printf("found=%d!!!\n", Result );
	return Result;
}
int SearchTransiElement( int PageNumber, int PositionX, int PositionY )
{
	int ScanTransi;
	StrTransition * pTransi;
	int Result = -1;
printf("transi search posiX=%d, posiY=%d ; ", PositionX, PositionY );
	for( ScanTransi=0; ScanTransi<NBR_TRANSITIONS; ScanTransi++ )
	{
		pTransi = &EditSeqDatas.Transition[ ScanTransi ];
		if ( pTransi->NumPage==PageNumber )
		{
			if ( pTransi->PosiX==PositionX && pTransi->PosiY==PositionY )
				Result = ScanTransi;
		}
	}
printf("found=%d!!!\n", Result );
	return Result;
}
int SearchCommentElement( int PageNumber, int PositionX, int PositionY )
{
	int ScanComment;
	StrSeqComment * pComment;
	int Result = -1;
printf("comment search posiX=%d, posiY=%d ; ", PositionX, PositionY );
	for( ScanComment=0; ScanComment<NBR_SEQ_COMMENTS; ScanComment++ )
	{
		pComment = &EditSeqDatas.SeqComment[ ScanComment ];
		if ( pComment->NumPage==PageNumber )
		{
			if ( ( pComment->PosiX<=PositionX && PositionX<pComment->PosiX+4 ) && pComment->PosiY==PositionY )
				Result = ScanComment;
		}
	}
printf("found=%d!!!\n", Result );
	return Result;
}

/* -1 if not found */
int FindFreeStep( void )
{
	int ScanStep = 0;
	StrStep * pStep;
	int Result = -1;
	do
	{
		pStep = &EditSeqDatas.Step[ ScanStep ];
		if ( pStep->NumPage==-1 )
			Result = ScanStep;
		else
			ScanStep++;
	}
	while( Result==-1 && ScanStep<NBR_STEPS );
printf("found free step=%d!!!\n", Result );
	return Result;
}
/* -1 if not found */
int FindFreeTransi( void )
{
	int ScanTransi = 0;
	StrTransition * pTransi;
	int Result = -1;
	do
	{
		pTransi = &EditSeqDatas.Transition[ ScanTransi ];
		if ( pTransi->NumPage==-1 )
			Result = ScanTransi;
		else
			ScanTransi++;
	}
	while( Result==-1 && ScanTransi<NBR_TRANSITIONS );
printf("found free transi=%d!!!\n", Result );
	return Result;
}
/* -1 if not found */
int FindFreeComment( void )
{
	int ScanComment = 0;
	StrSeqComment * pComment;
	int Result = -1;
	do
	{
		pComment = &EditSeqDatas.SeqComment[ ScanComment ];
//printf("scan comment %d = page:%d\n", ScanComment, pComment->NumPage );
		if ( pComment->NumPage==-1 )
			Result = ScanComment;
		else
			ScanComment++;
	}
	while( Result==-1 && ScanComment<NBR_SEQ_COMMENTS );
printf("found free comment=%d!!!\n", Result );
	return Result;
}

void DestroyStep( int Offset )
{
	StrStep * pStep = &EditSeqDatas.Step[ Offset ];
	pStep->NumPage = -1;
}
/* -1 if not created */
int CreateStep( int page, int x, int y, char init )
{
	int TransiAssoc;
	int OffsetStepCreated = FindFreeStep( );
	int TopStepForAutoNumber = -1;
	int NumStepForAutoNumber = 0;
	if ( OffsetStepCreated!=-1 )
	{
		StrStep * pStep = &EditSeqDatas.Step[ OffsetStepCreated ];
		pStep->NumPage = page;
		pStep->PosiX = x;
		pStep->PosiY = y;
		pStep->InitStep = init;
		// look if there is a step on the top, to directly give next step number
		TopStepForAutoNumber = SearchStepElement( page, x, y-2 );
		if ( TopStepForAutoNumber==-1 )
		{
			// search next step number available...
			int ScanStep = 0;
			for( ScanStep=0; ScanStep<NBR_STEPS; ScanStep++ )
			{
				StrStep * pScanStep = &EditSeqDatas.Step[ ScanStep ];
				if ( pScanStep->NumPage!=-1 )
				{
					// already used ?
					if ( NumStepForAutoNumber==pScanStep->StepNumber )
						NumStepForAutoNumber = pScanStep->StepNumber+1;
				}
			}
		}
		else
		{
			NumStepForAutoNumber = EditSeqDatas.Step[ TopStepForAutoNumber ].StepNumber+1;
		}
		pStep->StepNumber = NumStepForAutoNumber;
		// search top transi to connect with
		TransiAssoc = SearchTransiElement( page, x, y-1 );
		if ( TransiAssoc!=-1 )
			EditSeqDatas.Transition[ TransiAssoc ].NumStepToActiv[ 0 ] = OffsetStepCreated;
		// search bottom transi to connect with
		TransiAssoc = SearchTransiElement( page, x, y+1 );
		if ( TransiAssoc!=-1 )
			EditSeqDatas.Transition[ TransiAssoc ].NumStepToDesactiv[ 0 ] = OffsetStepCreated;
	}
	return OffsetStepCreated;
}

void DestroyTransi( int Offset )
{
	StrTransition * pTransi = &EditSeqDatas.Transition[ Offset ];
	pTransi->NumPage = -1;
}
/* -1 if not created */
int CreateTransi( int page, int x, int y )
{
	int StepAssoc;
	int NumSwitch;
	int OffsetCreated = FindFreeTransi( );
	int TopTransiForAutoVar = -1;
	if ( OffsetCreated!=-1 )
	{
		StrTransition * pTransi = &EditSeqDatas.Transition[ OffsetCreated ];
		pTransi->NumPage = page;
		pTransi->PosiX = x;
		pTransi->PosiY = y;
		pTransi->VarTypeCondi = VAR_MEM_BIT;
		pTransi->VarNumCondi = 0;
		// look if there is a transition on the top, to directly give next variable number
		TopTransiForAutoVar = SearchTransiElement( page, x, y-2 );
		if ( TopTransiForAutoVar!=-1 )
		{
			pTransi->VarTypeCondi = EditSeqDatas.Transition[ TopTransiForAutoVar ].VarTypeCondi;
			pTransi->VarNumCondi = EditSeqDatas.Transition[ TopTransiForAutoVar ].VarNumCondi+1;
		}
		for( NumSwitch=0; NumSwitch<NBR_SWITCHS_MAX; NumSwitch++ )
		{
			pTransi->NumStepToActiv[ NumSwitch ] = -1;
			pTransi->NumStepToDesactiv[ NumSwitch ] = -1;
			pTransi->NumTransLinkedForStart[ NumSwitch ] = -1;
			pTransi->NumTransLinkedForEnd[ NumSwitch ] = -1;
		}
		// search top step to connect with
		if ( y>0 )
		{
			StepAssoc = SearchStepElement( page, x, y-1 );
			if ( StepAssoc!=-1 )
				pTransi->NumStepToDesactiv[ 0 ] = StepAssoc;
		}
		// search bottom step to connect with
		if ( y<SEQ_PAGE_HEIGHT-1 )
		{
			StepAssoc = SearchStepElement( page, x, y+1 );
			if ( StepAssoc!=-1 )
				pTransi->NumStepToActiv[ 0 ] = StepAssoc;
		}
	}
	return OffsetCreated;
}

void DestroyComment( int Offset )
{
	StrSeqComment * pComment = &EditSeqDatas.SeqComment[ Offset ];
	pComment->NumPage = -1;
}

/* -1 if could not be created */
int CreateComment( int page, int x, int y )
{
	int OffsetCommentCreated = FindFreeComment( );
	if ( OffsetCommentCreated!=-1 )
	{
		StrSeqComment * pComment = &EditSeqDatas.SeqComment[ OffsetCommentCreated ];
		pComment->Comment[ 0 ] = '\0';
		pComment->NumPage = page;
		pComment->PosiX = x;
		pComment->PosiY = y;
	}
	return OffsetCommentCreated;
}

void DoLinkTransitionAndStep( int OffsetTransi, char TopOfTransi, int OffsetStep )
{
	StrTransition * pTransi = &EditSeqDatas.Transition[ OffsetTransi ];
printf("Do link : transi=%d (top=%d), step=%d\n", OffsetTransi, TopOfTransi, OffsetStep);
	if ( TopOfTransi )
		pTransi->NumStepToDesactiv[ 0 ] = OffsetStep;
	else
		pTransi->NumStepToActiv[ 0 ] = OffsetStep;
}

/* return TRUE is okay */
char CommonSearchForManyStepsOrTransi( char ForManySteps, int TypeEle1, int OffEle1, int TypeEle2, int OffEle2,
					int * pOffsetTransiFound, int * pStepsBaseY, int * pTransitionsBaseY, int * pLeftX, int * pRightX )
{
	int OffsetTransiFound = -1;
	int StepsBaseY = -1;
	int TransitionsBaseY = -1;
	int Ele1X,Ele2X;
	int LeftX,RightX;
	if ( !ForManySteps && (TypeEle1==ELE_SEQ_STEP || TypeEle2==ELE_SEQ_STEP ) )
	{
		ShowMessageBox("Error","Not selected first and last transitions to be joined !!??","Ok");
		return FALSE;
	}
	if ( TypeEle1==ELE_SEQ_STEP )
	{
		Ele1X = EditSeqDatas.Step[ OffEle1 ].PosiX;
		StepsBaseY = EditSeqDatas.Step[ OffEle1 ].PosiY;
	}
	else if ( TypeEle1==ELE_SEQ_TRANSITION )
	{
		// search transition corresponding... directly clicked on it!
		OffsetTransiFound = OffEle1;
		Ele1X = EditSeqDatas.Transition[ OffEle1 ].PosiX;
		TransitionsBaseY = EditSeqDatas.Transition[ OffEle1 ].PosiY;
	}
	else
	{
		ShowMessageBox("Error","Unknown element type for Ele1","Ok");
		return FALSE;
	}
	if ( TypeEle2==ELE_SEQ_STEP )
	{
		Ele2X = EditSeqDatas.Step[ OffEle2 ].PosiX;
		if ( StepsBaseY==-1 )
		{
			StepsBaseY = EditSeqDatas.Step[ OffEle1 ].PosiY;
		}
		else
		{
			if ( StepsBaseY!=EditSeqDatas.Step[ OffEle1 ].PosiY )
			{
				ShowMessageBox("Error","First and last steps selected are not on the same line !!??","Ok");
				return FALSE;
			}
		}
	}
	else if ( TypeEle2==ELE_SEQ_TRANSITION )
	{
		OffsetTransiFound = OffEle2;
		Ele2X = EditSeqDatas.Transition[ OffEle2 ].PosiX;
		if ( TransitionsBaseY==-1 )
		{
			TransitionsBaseY = EditSeqDatas.Transition[ OffEle1 ].PosiY;
		}
		else
		{
			if ( TransitionsBaseY!=EditSeqDatas.Transition[ OffEle1 ].PosiY )
			{
				ShowMessageBox("Error","First and last transitions selected are not on the same line !!??","Ok");
				return FALSE;
			}
		}
	}
	else
	{
		ShowMessageBox("Error","Unknown element type for Ele2","Ok");
		return FALSE;
	}

	LeftX = Ele1X;
	RightX = Ele1X;
	if ( LeftX>Ele2X )
		LeftX = Ele2X;
	if ( RightX<Ele2X )
		RightX = Ele2X;

	if ( pOffsetTransiFound!=NULL )
	*pOffsetTransiFound = OffsetTransiFound;
	if ( pStepsBaseY!=NULL )
		*pStepsBaseY = StepsBaseY;
	if ( pTransitionsBaseY!=NULL )
		*pTransitionsBaseY = TransitionsBaseY;
	*pLeftX = LeftX;
	*pRightX = RightX;
printf("commonsearch: leftX=%d, rightX=%d, OffTransi=%d, StepsY=%d, TransiY=%d\n", LeftX, RightX, OffsetTransiFound, StepsBaseY, TransitionsBaseY );
	return TRUE;
}

void DoManyStepsActOrDesact( int ForPage, int FlagStart, int TypeEle1, int OffEle1, int TypeEle2, int OffEle2 )
{
	int OffsetTransiFound = -1;
	int StepsBaseY = -1;
	int LeftX,RightX;

	if ( !CommonSearchForManyStepsOrTransi( TRUE, TypeEle1, OffEle1, TypeEle2, OffEle2 ,
					&OffsetTransiFound, &StepsBaseY, NULL, &LeftX, &RightX ) )
		return; /* search failed ! */

	// try to find the transition associated to the steps...
	if ( OffsetTransiFound==-1 && StepsBaseY!=-1 )
	{
		int ScanX;
		// searching in line behind or above...
		int TransiPosiY = StepsBaseY+(FlagStart?-1:1);
		int TransiSearch;
		for( ScanX=LeftX; ScanX<=RightX; ScanX++ )
		{
			TransiSearch = SearchTransiElement( ForPage, ScanX, TransiPosiY );
			if ( TransiSearch!=-1 )
				OffsetTransiFound = TransiSearch;
		}
	}
	if ( OffsetTransiFound==-1 || StepsBaseY==-1 )
	{
			ShowMessageBox("Error","Error in selection or not possible...","Ok");
	}
	else
	{
		int ScanX;
		int CptStep = 0;
		int StepSearch;
		int ScanStep;
printf("DO ACT/DESACT STEPS x1=%d, x2=%d, y=%d\n", LeftX, RightX, StepsBaseY );
		// init all
		for( ScanStep=0; ScanStep<NBR_SWITCHS_MAX; ScanStep++ )
		{
			if ( FlagStart )
				EditSeqDatas.Transition[ OffsetTransiFound ].NumStepToActiv[ ScanStep ] = -1;
			else
				EditSeqDatas.Transition[ OffsetTransiFound ].NumStepToDesactiv[ ScanStep ] = -1;
		}
		// find all the steps to set for the transition
		for( ScanX=LeftX; ScanX<=RightX; ScanX++ )
		{
			StepSearch = SearchStepElement( ForPage, ScanX, StepsBaseY );
			if ( StepSearch!=-1 && CptStep<NBR_SWITCHS_MAX )
			{
				if ( FlagStart )
					EditSeqDatas.Transition[ OffsetTransiFound ].NumStepToActiv[ CptStep ] = StepSearch;
				else
					EditSeqDatas.Transition[ OffsetTransiFound ].NumStepToDesactiv[ CptStep ] = StepSearch;
				CptStep++;
printf("StepActDesact++=%d\n", StepSearch );
			}
		}
	}
}

void DoManyTransitionsLinked( int ForPage, int FlagStart, int TypeEle1, int OffEle1, int TypeEle2, int OffEle2 )
{
	int LeftX,RightX;
	int TransisBaseY;

	CommonSearchForManyStepsOrTransi( TRUE, TypeEle1, OffEle1, TypeEle2, OffEle2 ,
					NULL, NULL, &TransisBaseY, &LeftX, &RightX );

	if ( TransisBaseY==-1 )
	{
			ShowMessageBox("Error","Error in selection or not possible...","Ok");
	}
	else
	{
		int NbrTransisLinked = 0;
		int ArrayNumTransiLinked[ NBR_SWITCHS_MAX ];
		int ScanX;
		int CptTransi = 0;
		int TransiSearch;
		int ScanTransi;
		int ScanTransiArray, ScanTransiArray2;
		// find all the transitions which are linked together
		for( ScanX=LeftX; ScanX<=RightX; ScanX++ )
		{
			TransiSearch = SearchTransiElement( ForPage, ScanX, TransisBaseY );
			if ( TransiSearch!=-1 && CptTransi<NBR_SWITCHS_MAX )
				ArrayNumTransiLinked[ NbrTransisLinked++ ] = TransiSearch;
		}

		if ( NbrTransisLinked>=2 )
		{
			for( ScanTransiArray=0; ScanTransiArray<NbrTransisLinked; ScanTransiArray++ )
			{
				int TheTransi = ArrayNumTransiLinked[ ScanTransiArray ];
				int StepToAct = -1;
				int StepToDesact = -1;
				StrTransition * pTheTransi = &EditSeqDatas.Transition[ TheTransi ];
				// init all
				for( ScanTransi=0; ScanTransi<NBR_SWITCHS_MAX; ScanTransi++ )
				{
					if ( FlagStart )
						pTheTransi->NumTransLinkedForStart[ ScanTransi ] = -1;
					else
						pTheTransi->NumTransLinkedForEnd[ ScanTransi ] = -1;
				}

				// put the others transitions than itself
				ScanTransi = 0;
				for( ScanTransiArray2=0; ScanTransiArray2<NbrTransisLinked; ScanTransiArray2++ )
				{
					int NumTransi = ArrayNumTransiLinked[ ScanTransiArray2 ];
printf("having num transi linked=%d for transi=%d\n", NumTransi, TheTransi );
					if ( NumTransi!=TheTransi )
					{
printf("->storing num transi linked=%d for transi=%d\n", NumTransi, TheTransi );
						if ( FlagStart )
						{
							pTheTransi->NumTransLinkedForStart[ ScanTransi++ ] = NumTransi;
							if ( EditSeqDatas.Transition[ NumTransi ].NumStepToDesactiv[ 0 ]!=-1 )
								StepToDesact = EditSeqDatas.Transition[ NumTransi ].NumStepToDesactiv[ 0 ];
						}
						else
						{
							pTheTransi->NumTransLinkedForEnd[ ScanTransi++ ] = NumTransi;
							if ( EditSeqDatas.Transition[ NumTransi ].NumStepToActiv[ 0 ]!=-1 )
								StepToAct = EditSeqDatas.Transition[ NumTransi ].NumStepToActiv[ 0 ];
						}
					}
				}
				// step to activate / descativate
printf("=>step to activ=%d, step to desactiv=%d\n", StepToAct, StepToDesact );
				if ( StepToAct!=-1 )
					pTheTransi->NumStepToActiv[ 0 ] = StepToAct;
				if ( StepToDesact!=-1 )
					pTheTransi->NumStepToDesactiv[ 0 ] = StepToDesact;
			}
		}
		else
		{
			ShowMessageBox("Error","Not found at least 2 transitions linked...","Ok");
		}


	}
}

void DestroyIt( int Type, int Offset )
{
	switch( Type )
	{
		case ELE_SEQ_STEP: DestroyStep( Offset ); break;
		case ELE_SEQ_TRANSITION: DestroyTransi( Offset ); break;
		case ELE_SEQ_COMMENT: DestroyComment( Offset ); break;
	}
}
void SearchIt( int PageNbr, int PosX, int PosY, int * Type, int * Offset )
{
	int TypeFound = -1;
	int OffsetFound = -1;

	/* steps are on odd lines, transitions on even ones */
	if ( PosY & 1 )
	{
		OffsetFound = SearchStepElement( PageNbr, PosX, PosY );
		if ( OffsetFound!=-1 )
		{
			TypeFound = ELE_SEQ_STEP;
			TopSeqEleEdited = TypeFound;
		}
	}
	else
	{
		OffsetFound = SearchTransiElement( PageNbr, PosX, PosY );
		if ( OffsetFound!=-1 )
		{
			TypeFound = ELE_SEQ_TRANSITION;
			TopSeqEleEdited = TypeFound;
		}
	}
	/* comments can be on any lines */
	if ( OffsetFound==-1 )
	{
		OffsetFound = SearchCommentElement( PageNbr, PosX, PosY );
		if ( OffsetFound!=-1 )
		{
			TypeFound = ELE_SEQ_COMMENT;
			TopSeqEleEdited = TypeFound;
		}
	}
	*Type = TypeFound;
	*Offset = OffsetFound;
}

/* click with the mouse in x and y pixels of the sequential page */
void EditElementInSeqPage(double x,double y)
{
	int PosX,PosY;
	/* correspond to which block ? */
	PosX = x/SEQ_SIZE_DEF;
	PosY = y/SEQ_SIZE_DEF;
	if ( (PosX<SEQ_PAGE_WIDTH) && (PosY<SEQ_PAGE_HEIGHT)
		&& (EditDatas.NumElementSelectedInToolBar!=-1) )
	{
		int TypeFound = -1;
		int OffsetFound = -1;
		char TopFound = y>PosY*SEQ_SIZE_DEF && y<PosY*SEQ_SIZE_DEF+SEQ_SIZE_DEF/2;

		/* save what was selected just before */
		int TypeSeqEleEditedBak = TypeSeqEleEdited;
		int OffsetSeqEleEditedBak = OffsetSeqEleEdited;
		char TopSeqEleEditedBak = TopSeqEleEdited;
		int CurrentSeqPage = SectionArray[ InfosGene->CurrentSection ].SequentialPage;

		// for comments, verify 4 horizontal blocks

		if ( EditDatas.NumElementSelectedInToolBar!=NumElementSelectedInToolBarBak )
			CptNbrClicksDone = 0;
		NumElementSelectedInToolBarBak = EditDatas.NumElementSelectedInToolBar;

//printf("top clicked= %d\n", TopFound );
		TypeSeqEleEdited = -1;

		/* search element selected */
		SearchIt( CurrentSeqPage, PosX, PosY, &TypeFound, &OffsetFound );

		switch( EditDatas.NumElementSelectedInToolBar )
		{
			case EDIT_POINTER:
				TypeSeqEleEdited = TypeFound;
				OffsetSeqEleEdited = OffsetFound;
				break;
			case ELE_SEQ_STEP:
			case EDIT_SEQ_INIT_STEP:
			case ELE_SEQ_TRANSITION:
			case EDIT_SEQ_STEP_AND_TRANS:
				if ( EditDatas.NumElementSelectedInToolBar==ELE_SEQ_STEP
					|| EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_INIT_STEP
					|| EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_STEP_AND_TRANS )
				{
					if ( PosY & 1 )
					{
						if ( TypeFound==ELE_SEQ_STEP )
						{
							DestroyStep( OffsetFound );
						}
						else
						{
							if ( TypeFound==-1 )
							{
								OffsetFound = CreateStep( CurrentSeqPage, PosX, PosY,
											(EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_INIT_STEP)?TRUE:FALSE );
								if ( OffsetFound!=-1 )
								{
									TypeSeqEleEdited = ELE_SEQ_STEP;
									OffsetSeqEleEdited = OffsetFound;
								}
								else
								{
									ShowMessageBox("Error","Sequential memory full for steps","Ok");
								}
							}
							else
							{
								ShowMessageBox("Error","There is already an element!","Ok");
							}
						}
					}
					else
					{
						ShowMessageBox("Error","A step can't be placed on even lines","Ok");
					}
				}
				if ( EditDatas.NumElementSelectedInToolBar==ELE_SEQ_TRANSITION
					|| EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_STEP_AND_TRANS )
				{
					if ( EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_STEP_AND_TRANS )
						PosY++;
					if ( (PosY & 1)==0 )
					{
						if ( TypeFound==ELE_SEQ_TRANSITION )
						{
							DestroyTransi( OffsetFound );
						}
						else
						{
							if ( TypeFound==-1 )
							{
								OffsetFound = CreateTransi( CurrentSeqPage, PosX, PosY );
								if ( OffsetFound!=-1 )
								{
									TypeSeqEleEdited = ELE_SEQ_TRANSITION;
									OffsetSeqEleEdited = OffsetFound;
								}
								else
								{
									ShowMessageBox("Error","Sequential memory full for transition","Ok");
								}
							}
							else
							{
								ShowMessageBox("Error","There is already an element!","Ok");
							}
						}
					}
					else
					{
						ShowMessageBox("Error","A transition can't be placed on odd lines","Ok");
					}
				}
				break;
			case EDIT_SEQ_LINK:
				if ( TypeFound!=-1 )
					CptNbrClicksDone++;
				if ( CptNbrClicksDone==1 )
				{
printf("nbr clicks=1!!! (posi=%s), wait next point to link...\n", (TopFound==1)?"top":"bottom" );
					TypeSeqEleEdited = TypeFound;
					OffsetSeqEleEdited = OffsetFound;
					TopSeqEleEdited = TopFound;
					//TODO: modify cursor so that it is a little more explicit...?
				}
				if ( CptNbrClicksDone==2 )
				{
printf("nbr clicks=2!!! (posi=%s), TypeBak=%d, TypeNow=%d\n", (TopFound==1)?"top":"bottom", TypeSeqEleEditedBak, TypeFound );
					if ( TypeSeqEleEditedBak==ELE_SEQ_TRANSITION && TypeFound==ELE_SEQ_STEP )
						DoLinkTransitionAndStep( OffsetSeqEleEditedBak, TopSeqEleEditedBak, OffsetFound );
					if ( TypeSeqEleEditedBak==ELE_SEQ_STEP && TypeFound==ELE_SEQ_TRANSITION )
						DoLinkTransitionAndStep( OffsetFound, TopFound, OffsetSeqEleEditedBak );
					CptNbrClicksDone = 0;
				}
				break;

			case EDIT_ERASER:
				if ( TypeFound!=-1 )
					DestroyIt( TypeFound, OffsetFound );
				break;

			case ELE_SEQ_COMMENT:
				// a comment takes 4 horizontal blocks
				if ( PosX<=SEQ_PAGE_WIDTH-4 )
				{
					// verify if elements that would be under it...
					int ScanHoriX;
					char SomethingHere = FALSE;
					for ( ScanHoriX=PosX; ScanHoriX<PosX+4; ScanHoriX++ )
					{
						SearchIt( CurrentSeqPage, ScanHoriX, PosY, &TypeFound, &OffsetFound );
						if ( TypeFound!=-1 )
							SomethingHere = TRUE;
//							DestroyIt( TypeFound, OffsetFound );
					}
					if ( SomethingHere==FALSE )
					{
						OffsetFound = CreateComment( CurrentSeqPage, PosX, PosY );
						if ( OffsetFound!=-1 )
						{
							TypeSeqEleEdited = ELE_SEQ_COMMENT;
							OffsetSeqEleEdited = OffsetFound;
						}
						else
						{
							ShowMessageBox("Error","Sequential memory full for comments","Ok");
						}
					}
					else
					{
						ShowMessageBox("Error","There is already an element on 4 horizontal blocks required!","Ok");
					}
				}
				else
				{
					ShowMessageBox("Error","Not enough room on the right here...","Ok");
				}
				break;

			case EDIT_SEQ_START_MANY_STEPS:
			case EDIT_SEQ_END_MANY_STEPS:
			case EDIT_SEQ_START_MANY_TRANS:
			case EDIT_SEQ_END_MANY_TRANS:
				if ( TypeFound!=-1 )
					CptNbrClicksDone++;
				if ( CptNbrClicksDone==1 )
				{
					TypeSeqEleEdited = TypeFound;
					OffsetSeqEleEdited = OffsetFound;
					TopSeqEleEdited = TopFound;
					//TODO: modify cursor so that it is a little more explicit...?
				}
				if ( CptNbrClicksDone==2 )
				{
					if ( EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_START_MANY_STEPS
						|| EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_END_MANY_STEPS )
					{
printf("DO_MANY_STEPS:nbr clicks=2!!!, TypeBak=%d, TypeNow=%d\n", TypeSeqEleEditedBak, TypeFound );
						DoManyStepsActOrDesact( CurrentSeqPage, EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_START_MANY_STEPS,
									 TypeSeqEleEditedBak, OffsetSeqEleEditedBak, TypeFound, OffsetFound );
					}
					else
					{
printf("DO_MANY_TRANSITIONS:nbr clicks=2!!!, TypeBak=%d, TypeNow=%d\n", TypeSeqEleEditedBak, TypeFound );
						DoManyTransitionsLinked( CurrentSeqPage, EditDatas.NumElementSelectedInToolBar==EDIT_SEQ_START_MANY_TRANS,
									 TypeSeqEleEditedBak, OffsetSeqEleEditedBak, TypeFound, OffsetFound );
					}
					CptNbrClicksDone = 0;
				}
				break;

		}
		LoadSeqElementProperties( );
	}
}

