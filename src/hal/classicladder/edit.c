/* Classic Ladder Project */
/* Copyright (C) 2001-2008 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* May 2001 */
/* ------ */
/* Editor */
/* ------ */
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
#include "calc.h"
#include "files.h"
#include "arithm_eval.h"
#include "classicladder_gtk.h"
#include "manager.h"
#ifdef SEQUENTIAL_SUPPORT
#include "edit_sequential.h"
#endif
#include "symbols.h"
#include "vars_names.h"

/* This array give for each special elements the size used */
#define TYPEELERULE 0
#define XSIZEELERULE 1
#define YSIZEELERULE 2
static short int RulesForSpecialElements[][3] =
            { {ELE_TIMER , 2, 2 } ,
              {ELE_MONOSTABLE, 2, 2 },
              {ELE_COUNTER, 2, 4 },
              {ELE_TIMER_IEC, 2, 2 },
              {ELE_COMPAR, 3, 1 },
              {ELE_OUTPUT_OPERATE, 3, 1 },
              {-1, -1, -1}/*end*/ };

// pointer on a string error explanation...
char * ErrorMessageVarParser = NULL;

// used to mark the functions blocs already used
#ifdef OLD_TIMERS_MONOS_SUPPORT
char NumTimersUsedInRung[ 256 ];
char NumMonostablesUsedInRung[ 256 ];
#endif
char NumCountersUsedInRung[ 256 ];
char NumNewTimersUsedInRung[ 256 ];


int ConvBaseInTextToId(char * text)
{
	int ScanBase = 0;
	int BaseFound = BASE_SECS;
	do
	{
		if (strcmp(text,CorresDatasForBase[ScanBase].ParamSelect)==0)
			BaseFound = ScanBase;
		ScanBase++;
	}
	while(ScanBase<NBR_TIMEBASES);
	return BaseFound;
}
int ConvTimerModeInTextToId(char * text)
{
	int ScanMode = 0;
	int TimerModeFound = TIMER_IEC_MODE_ON;
	do
	{
		if (strcmp(text,TimersModesStrings[ ScanMode ])==0)
			TimerModeFound = ScanMode;
		ScanMode++;
	}
	while(ScanMode<NBR_TIMERSMODES);
	return TimerModeFound;
}

int ConvLabelToNumRung(char * LabelName)
{
	int ScanRung = 0;
	int RungFound = -1;
	int Done = FALSE;
	ScanRung = InfosGene->FirstRung;
	do
	{
		if (strcmp(RungArray[ScanRung].Label,LabelName)==0)
			RungFound = ScanRung;
		if (ScanRung == InfosGene->LastRung)
			Done = TRUE;
		else
			ScanRung = RungArray[ScanRung].NextRung;
	}
	while(!Done);
	return RungFound;
}

void LoadElementProperties(StrElement * Element)
{
	char TextToWrite[100];
	int NumParam;
#ifdef OLD_TIMERS_MONOS_SUPPORT
	StrTimer * Timer = NULL;
	StrMonostable * Monostable = NULL;
#endif
	StrCounter * Counter = NULL;
	StrTimerIEC * TimerIEC = NULL;
	for(NumParam=0;NumParam<NBR_PARAMS_PER_OBJ;NumParam++)
		SetProperty(NumParam,"---","");
	if (Element)
	{
		switch(Element->Type)
		{
			case ELE_INPUT:
				
			case ELE_INPUT_NOT:
			case ELE_RISING_INPUT:
			case ELE_FALLING_INPUT:
			case ELE_OUTPUT:
			case ELE_OUTPUT_NOT:
			case ELE_OUTPUT_SET:
			case ELE_OUTPUT_RESET:
				strcpy(TextToWrite,CreateVarName(Element->VarType,Element->VarNum));
				SetProperty(0,"Variable",TextToWrite);
				break;
			case ELE_OUTPUT_JUMP:
				SetProperty(0,"JumpToLabel",RungArray[Element->VarNum].Label);
				break;
			case ELE_OUTPUT_CALL:
				sprintf(TextToWrite,"%d",Element->VarNum);
				SetProperty(0,"Sub-Routine",TextToWrite);
				break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
			case ELE_TIMER:
				Timer = &TimerArray[Element->VarNum];
				sprintf(TextToWrite,"%d",Element->VarNum);
				SetProperty(0,"TimerNbr",TextToWrite);
				sprintf(TextToWrite,"%s",CorresDatasForBase[ ConvBaseInMilliSecsToId(Timer->Base) ].ParamSelect);
				SetProperty(1,"Base",TextToWrite);
				sprintf(TextToWrite,"%d",Timer->Preset/Timer->Base);
				SetProperty(2,"Preset",TextToWrite);
				break;
			case ELE_MONOSTABLE:
				Monostable = &MonostableArray[Element->VarNum];
				sprintf(TextToWrite,"%d",Element->VarNum);
				SetProperty(0,"MonostNbr",TextToWrite);
				sprintf(TextToWrite,"%s",CorresDatasForBase[ ConvBaseInMilliSecsToId(Monostable->Base) ].ParamSelect);
				SetProperty(1,"Base",TextToWrite);
				sprintf(TextToWrite,"%d",Monostable->Preset/Monostable->Base);
				SetProperty(2,"Preset",TextToWrite);
				break;
#endif
			case ELE_COUNTER:
				Counter = &CounterArray[Element->VarNum];
				sprintf(TextToWrite,"%d",Element->VarNum);
				SetProperty(0,"CounterNbr",TextToWrite);
				sprintf(TextToWrite,"%d",Counter->Preset);
				SetProperty(1,"Preset",TextToWrite);
				break;
			case ELE_TIMER_IEC:
				TimerIEC = &NewTimerArray[Element->VarNum];
				sprintf(TextToWrite,"%d",Element->VarNum);
				SetProperty(0,"TimerNbr",TextToWrite);
				sprintf(TextToWrite,"%s",CorresDatasForBase[ ConvBaseInMilliSecsToId(TimerIEC->Base) ].ParamSelect);
				SetProperty(1,"Base",TextToWrite);
				sprintf(TextToWrite,"%d",TimerIEC->Preset);
				SetProperty(2,"Preset",TextToWrite);
				sprintf(TextToWrite,"%s",TimersModesStrings[ (int)TimerIEC->TimerMode ]);
				SetProperty(3,"TimerMode",TextToWrite);
				break;
			case ELE_COMPAR:
			case ELE_OUTPUT_OPERATE:
				strcpy(TextToWrite,DisplayArithmExpr(EditArithmExpr[Element->VarNum].Expr,0));
				SetProperty(0,"Expression",TextToWrite);
				break;
		}
	}
}

char * GetElementPropertiesForStatusBar(StrElement * Element)
{
	static char PropertiesText[100];
#ifdef OLD_TIMERS_MONOS_SUPPORT
	StrTimer * Timer = NULL;
	StrMonostable * Monostable = NULL;
#endif
	StrCounter * Counter = NULL;
	StrTimerIEC * TimerIEC = NULL;

	strcpy( PropertiesText, "" );

	if (Element)
	{
		switch(Element->Type)
		{
			case ELE_INPUT:                             	
			case ELE_INPUT_NOT:
			case ELE_RISING_INPUT:
			case ELE_FALLING_INPUT:
			case ELE_OUTPUT:				
			case ELE_OUTPUT_NOT:
			case ELE_OUTPUT_SET:
			case ELE_OUTPUT_RESET:				
				sprintf(PropertiesText, "Variable: %s    Hal sig: %s",CreateVarName(Element->VarType,Element->VarNum),ConvVarNameToHalSigName(CreateVarName(Element->VarType,Element->VarNum)));
				break;
			case ELE_OUTPUT_JUMP:
				sprintf(PropertiesText, "Label: %s", RungArray[Element->VarNum].Label);
				break;
			case ELE_OUTPUT_CALL:
				sprintf(PropertiesText, "Sub-Routine: %d", Element->VarNum);
				break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
			case ELE_TIMER:
				Timer = &TimerArray[Element->VarNum];
				sprintf(PropertiesText, "%cT%d: Preset=%d Base=%s", '%', Element->VarNum, Timer->Preset/Timer->Base, CorresDatasForBase[ ConvBaseInMilliSecsToId(Timer->Base) ].ParamSelect);
				break;
			case ELE_MONOSTABLE:
				Monostable = &MonostableArray[Element->VarNum];
				sprintf(PropertiesText, "%cM%d: Preset=%d Base=%s", '%', Element->VarNum, Monostable->Preset/Monostable->Base, CorresDatasForBase[ ConvBaseInMilliSecsToId(Monostable->Base) ].ParamSelect);
				break;
#endif
			case ELE_COUNTER:
				Counter = &CounterArray[Element->VarNum];
				sprintf(PropertiesText, "%cC%d: Preset=%d", '%', Element->VarNum, Counter->Preset);
				break;
			case ELE_TIMER_IEC:
				TimerIEC = &NewTimerArray[Element->VarNum];
				sprintf(PropertiesText, "%cTM%d: Preset=%d Base=%s Mode=%s", '%', Element->VarNum, TimerIEC->Preset, CorresDatasForBase[ ConvBaseInMilliSecsToId(TimerIEC->Base) ].ParamSelect, TimersModesStrings[ (int)TimerIEC->TimerMode ]);
				break;
			case ELE_COMPAR:
			case ELE_OUTPUT_OPERATE:
				sprintf(PropertiesText,"HAL sig: %s",FirstVariableInArithm(DisplayArithmExpr(ArithmExpr[Element->VarNum].Expr,0)));
							
				break;
		}
	}
	return PropertiesText;
}
/* Convert a string to a number if >=Min and <=Max */
/* return TRUE if okay */
int TextToNumber(char * text,int ValMin,int ValMaxi,int *ValFound)
{
	int IsOk = TRUE;
	int Value;
	Value = atoi(text);
	if ( (Value<ValMin) || (Value>ValMaxi) )
		IsOk = FALSE;
	if (IsOk)
		*ValFound = Value;
	return IsOk;
}

/* Convert a string of arithmetic expression for the arithm_eval format : */
/* Variables becomes @type/offset@ or @type/offset[indextype/indexoffset]@*/
/* Then verify all the expression using arithm_eval */
/* return pointer on the new string converted and verified if okay, else NULL */
char * TextParserForArithmExpr(char * text, int TypeElement)
{
	static char NewExpr[100];
	char Buffer[20];
	int ItIsOk = TRUE;
	int VarType;
	int VarOffset;
	char * ptr = text;
	int StringLength;
	char VarIndexedIsFound = FALSE;
	ErrorMessageVarParser = NULL;

	if ( text[0]=='\0' )
		return NULL;

	strcpy(NewExpr,"");
	/* Convert expression for variables authorized */
	do
	{
		char SimpleCharCopy = FALSE;

		char SymbolBuff[ LGT_SYMBOL_STRING+10 ];
		char SymbolFound = FALSE;
		int SymbolLength = 1;

//printf("current ptr = '%c'\n", *ptr );

		// verify if it seems to be a symbol name...
		if ( ( *ptr>='A' && *ptr<='Z' ) || ( *ptr>='a' && * ptr<='z' ) )
		{
			char * End = ptr;
			char CharIn = FALSE;
			// possible end here ?
			do
			{
				CharIn = FALSE;
				End++;
				if ( *End==VAR_ATTRIBUTE_SEP || ( *End>='A' && *End<='Z' ) || ( *End>='a' && *End<='z' ) || ( *End>='0' && *End<='9' ) )
					CharIn = TRUE;
				if ( CharIn )
					SymbolLength++;
//printf("--> %c / lgt=%d / In=%d\n", *End, SymbolLength, CharIn );
			}
			while ( (*End!='\0' ) && CharIn );
			strncpy( SymbolBuff, ptr, SymbolLength );
			SymbolBuff[ SymbolLength ] = '\0'; 
//printf("symbol search on this string : %s - lgt=%d (text=%s)\n", SymbolBuff, SymbolLength, ptr );
			if ( ConvSymbolToVarName( SymbolBuff )!=NULL )
				SymbolFound = TRUE;
		}

		/* Verify if it is a variable (character and number) (or a symbol already found) */
//		if ( SymbolFound==TRUE || ( *ptr=='%' && ( ( *(ptr+2)>='0' && *(ptr+2)<='9' ) || ( *(ptr+3)>='0' && *(ptr+3)<='9' ) ) ) )
		// Call variable parser if symbol found -or- '%' character
		if ( SymbolFound==TRUE || *ptr=='%' )
		{
//printf("calling parser var...\n");
			if (TextParserForAVar( SymbolFound==TRUE?SymbolBuff:ptr,&VarType,&VarOffset,&StringLength,FALSE/*PartialNames*/))
			{
//printf( "parser var give => %d/%d length=%d\n", VarType, VarOffset, StringLength );
				if ( VarIndexedIsFound==FALSE )
					strcat( NewExpr, "@" );
				sprintf(Buffer,"%d/%d",VarType,VarOffset);
				strcat(NewExpr,Buffer);
				ptr = ptr + (SymbolFound==TRUE?SymbolLength:StringLength);
				if ( *ptr!='[' && VarIndexedIsFound==FALSE )
					strcat( NewExpr, "@" );

				if ( TEST_VAR_IS_A_BOOL( VarType,VarOffset ) )

				{
					ItIsOk = FALSE;
					ErrorMessageVarParser = "Incompatible type of variable (must be an integer!)";
				}
			}
			else
			{
//printf("parser var give FALSE!\n");
				SimpleCharCopy = TRUE;
//				break;
			}
		}
		else
		{
			SimpleCharCopy = TRUE;
		}

		if ( SimpleCharCopy )
		{
//printf("copy:'%c'.\n", *ptr);
			sprintf(Buffer,"%c",*ptr);
			strcat(NewExpr, Buffer);
			// end of variable mark to add now after an indexed one !
			if ( *ptr==']' )
			{
				strcat( NewExpr, "@" );
				VarIndexedIsFound = FALSE;
			}
			if ( *ptr=='[' )
				VarIndexedIsFound = TRUE;
			ptr++;
		}
	}
	while( (*ptr) && ItIsOk );

	/* Verify length of the expression */
	if (ItIsOk)
	{
		if (strlen(NewExpr)>=ARITHM_EXPR_SIZE-1)
		{
			NewExpr[ ARITHM_EXPR_SIZE-1 ] = '\0';
			printf("New expr=%s\n", NewExpr );
			ItIsOk = FALSE;
			ErrorMessageVarParser = "Expression too long";
		}
	}
//printf("Parser Arithm ; ItIsOk=%d ; OriExpr=%s ; NewExpr=%s\n",ItIsOk, text, NewExpr);
	/* Verify expression converted */
	if (ItIsOk)
	{
		if (TypeElement==ELE_OUTPUT_OPERATE)
			ErrorMessageVarParser = VerifySyntaxForMakeCalc(NewExpr);
		else
			ErrorMessageVarParser = VerifySyntaxForEvalCompare(NewExpr);
		if (ErrorMessageVarParser)
			ItIsOk = FALSE;
	}
	/* Error Message */
	if (ErrorMessageVarParser)
	{
		ShowMessageBox("Error",ErrorMessageVarParser,"Ok");
	}
	/* Give result */
	if (ItIsOk)
		return NewExpr;
	else
		return NULL;
}

void SaveElementProperties()
{
	int IdBase;
	int Preset;
	int NumRungToJump;
#ifdef OLD_TIMERS_MONOS_SUPPORT
	StrTimer * Timer = NULL;
	StrMonostable * Monostable = NULL;
#endif
	StrCounter * Counter = NULL;
	StrTimerIEC * TimerIEC = NULL;
	char * NewArithmExpr;
	int SubRoutineToCall;
	int VarTypeEntered,VarNumEntered;

#ifdef SEQUENTIAL_SUPPORT
	int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
	if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
	{
		SaveSeqElementProperties( );
		return;
	}
#endif

	if (EditDatas.ElementUnderEdit)
	{
		switch(EditDatas.ElementUnderEdit->Type)
		{
			case ELE_INPUT:
			case ELE_INPUT_NOT:
			case ELE_RISING_INPUT:
			case ELE_FALLING_INPUT:
			case ELE_OUTPUT:
			case ELE_OUTPUT_NOT:
			case ELE_OUTPUT_SET:
			case ELE_OUTPUT_RESET:
				if ( TextParserForAVar(GetProperty(0),&VarTypeEntered,&VarNumEntered,NULL,FALSE/*PartialNames*/) )
				{
					if ( !TEST_VAR_IS_A_BOOL( VarTypeEntered,VarNumEntered ) )

					{
						ShowMessageBox("Error","You must select a boolean variable !","Ok");
					}
					else
					{
						EditDatas.ElementUnderEdit->VarType = VarTypeEntered;
						EditDatas.ElementUnderEdit->VarNum = VarNumEntered;
					}
				}
				else
				{
					if (ErrorMessageVarParser)
						ShowMessageBox("Error",ErrorMessageVarParser,"Ok");
					else
						ShowMessageBox( "Error", "Unknown variable...", "Ok" );
				}
				break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
			case ELE_TIMER:
				TextToNumber(GetProperty(0),0,NBR_TIMERS-1,&EditDatas.ElementUnderEdit->VarNum);
				Timer = &TimerArray[EditDatas.ElementUnderEdit->VarNum];
				IdBase = ConvBaseInTextToId(GetProperty(1));
				Timer->Base = CorresDatasForBase[IdBase].ValueInMS;
				strcpy(Timer->DisplayFormat,CorresDatasForBase[IdBase].DisplayFormat);
				if (TextToNumber(GetProperty(2),0,999,&Preset))
					Timer->Preset = Preset * Timer->Base;
				break;
			case ELE_MONOSTABLE:
				TextToNumber(GetProperty(0),0,NBR_MONOSTABLES-1,&EditDatas.ElementUnderEdit->VarNum);
				Monostable = &MonostableArray[EditDatas.ElementUnderEdit->VarNum];
				IdBase = ConvBaseInTextToId(GetProperty(1));
				Monostable->Base = CorresDatasForBase[IdBase].ValueInMS;
				strcpy(Monostable->DisplayFormat,CorresDatasForBase[IdBase].DisplayFormat);
				if (TextToNumber(GetProperty(2),0,999,&Preset))
					Monostable->Preset = Preset * Monostable->Base;
				break;
#endif
			case ELE_COUNTER:
				TextToNumber(GetProperty(0),0,NBR_COUNTERS-1,&EditDatas.ElementUnderEdit->VarNum);
				Counter = &CounterArray[EditDatas.ElementUnderEdit->VarNum];
				if (TextToNumber(GetProperty(1),0,9999,&Preset))
					Counter->Preset = Preset;
				break;
			case ELE_TIMER_IEC:
				TextToNumber(GetProperty(0),0,NBR_TIMERS_IEC-1,&EditDatas.ElementUnderEdit->VarNum);
				TimerIEC = &NewTimerArray[EditDatas.ElementUnderEdit->VarNum];
				IdBase = ConvBaseInTextToId(GetProperty(1));
				TimerIEC->Base = CorresDatasForBase[IdBase].ValueInMS;
				strcpy(TimerIEC->DisplayFormat,CorresDatasForBase[IdBase].DisplayFormat);
				if (TextToNumber(GetProperty(2),0,999,&Preset))
					TimerIEC->Preset = Preset;
				TimerIEC->TimerMode = ConvTimerModeInTextToId( GetProperty(3) );
				break;
			case ELE_OUTPUT_JUMP:
				NumRungToJump = ConvLabelToNumRung(GetProperty(0));
				if (NumRungToJump>=0)
					EditDatas.ElementUnderEdit->VarNum = NumRungToJump;
				break;
			case ELE_OUTPUT_CALL:
				if ( TextToNumber(GetProperty(0),0,NBR_SECTIONS-1,&SubRoutineToCall) )
				{
					if ( VerifyIfSubRoutineNumberExist( SubRoutineToCall ) )
						EditDatas.ElementUnderEdit->VarNum = SubRoutineToCall;
				}
				break;
			case ELE_COMPAR:
				NewArithmExpr = TextParserForArithmExpr(GetProperty(0), ELE_COMPAR);
				if (NewArithmExpr)
					strcpy(EditArithmExpr[EditDatas.ElementUnderEdit->VarNum].Expr,NewArithmExpr);
				else
					strcpy(EditArithmExpr[EditDatas.ElementUnderEdit->VarNum].Expr,"#"); //used but invalid!
				break;
			case ELE_OUTPUT_OPERATE:
				NewArithmExpr = TextParserForArithmExpr(GetProperty(0), ELE_OUTPUT_OPERATE);
				if (NewArithmExpr)
					strcpy(EditArithmExpr[EditDatas.ElementUnderEdit->VarNum].Expr,NewArithmExpr);
				else
					strcpy(EditArithmExpr[EditDatas.ElementUnderEdit->VarNum].Expr,"#"); //used but invalid!
				break;
		}
		/* display back to show what we have really understand... */

		LoadElementProperties (EditDatas.ElementUnderEdit);
	}
}

/* For editing, we do not touch directly the current arithm
expressions. We work on the edit ones. It is only when the
edited rung is applyed that we copy the expressions edited */
void CopyCurrentArithmExpr()
{
	int NumExpr;
	for (NumExpr=0; NumExpr<NBR_ARITHM_EXPR; NumExpr++)
		strcpy(EditArithmExpr[NumExpr].Expr,ArithmExpr[NumExpr].Expr);
}
void ApplyNewArithmExpr()
{
	int NumExpr;
	for (NumExpr=0; NumExpr<NBR_ARITHM_EXPR; NumExpr++)
		strcpy(ArithmExpr[NumExpr].Expr,EditArithmExpr[NumExpr].Expr);
}
void CheckForFreeingArithmExpr(int PosiX,int PosiY)
{
	int TypeElement = EditDatas.Rung.Element[PosiX][PosiY].Type;
	if ( (TypeElement==ELE_COMPAR) || (TypeElement==ELE_OUTPUT_OPERATE) )
	{
		/* Freeing Expr */
		EditArithmExpr[ EditDatas.Rung.Element[PosiX][PosiY].VarNum ].Expr[0] = '\0';
	}
}
void CheckForAllocatingArithmExpr(int PosiX,int PosiY)
{
	int TypeElement = EditDatas.Rung.Element[PosiX][PosiY].Type;
	int NumExpr = 0;
	int Found = FALSE;
	if ( (TypeElement==ELE_COMPAR) || (TypeElement==ELE_OUTPUT_OPERATE) )
	{
		do
		{
			/* Expr free ? */
			if (EditArithmExpr[ NumExpr ].Expr[0]=='\0')
			{
				Found = TRUE;
				/* Allocate this expr for the operate/compar block ! */
				EditDatas.Rung.Element[PosiX][PosiY].VarNum = NumExpr;
				strcpy(EditArithmExpr[NumExpr].Expr,"#"); //used but invalid!
			}
			NumExpr++;
		}
		while( (NumExpr<NBR_ARITHM_EXPR) && (!Found) );
	}
}

void SetUsedStateFunctionBlock( int Type, int Num, char Val )
{
	switch( Type )
	{
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case ELE_TIMER:
			NumTimersUsedInRung[Num] = Val;
			break;
		case ELE_MONOSTABLE:
			NumMonostablesUsedInRung[Num] = Val;
			break;
#endif
		case ELE_COUNTER:
			NumCountersUsedInRung[Num] = Val;
			break;
		case ELE_TIMER_IEC:
			NumNewTimersUsedInRung[Num] = Val;
			break;
	}
}
// update the lists with the functions blocks number already used in the rungs defined
void CopyUsedFonctionBlockBeforeEdit( )
{
	int ScanBlock;
	int ScanRung;
#ifdef OLD_TIMERS_MONOS_SUPPORT
	for( ScanBlock=0; ScanBlock<NBR_TIMERS; ScanBlock++ )
		NumTimersUsedInRung[ ScanBlock ] = FALSE;
	for( ScanBlock=0; ScanBlock<NBR_MONOSTABLES; ScanBlock++ )
		NumMonostablesUsedInRung[ ScanBlock ] = FALSE;
#endif
	for( ScanBlock=0; ScanBlock<NBR_COUNTERS; ScanBlock++ )
		NumCountersUsedInRung[ ScanBlock ] = FALSE;
	for( ScanBlock=0; ScanBlock<NBR_TIMERS_IEC; ScanBlock++ )
		NumNewTimersUsedInRung[ ScanBlock ] = FALSE;
	// now search in the rungs defined
	for (ScanRung=0;ScanRung<NBR_RUNGS;ScanRung++)
	{
		int x,y;
		if ( RungArray[ScanRung].Used )
		{
			for (y=0;y<RUNG_HEIGHT;y++)
			{
				for(x=0;x<RUNG_WIDTH;x++)
				{
					if ( RungArray[ScanRung].Element[x][y].Type!=ELE_FREE )
						SetUsedStateFunctionBlock( RungArray[ScanRung].Element[x][y].Type, RungArray[ScanRung].Element[x][y].VarNum, TRUE );
				}
			}
		}
	}
}
// if return -1: for a function block: no more available...
int GetFreeNumberFunctionBlock( int Type )
{
	int Result = -1;
	int ScanArray = 0;
	char IsFunctionBlock = FALSE;
	switch( Type )
	{
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case ELE_TIMER:
			IsFunctionBlock = TRUE;
			do
			{
				if ( NumTimersUsedInRung[ScanArray]==FALSE )
					Result = ScanArray;
				ScanArray++;
			}
			while( ScanArray<NBR_TIMERS && Result==-1 );
			break;
		case ELE_MONOSTABLE:
			IsFunctionBlock = TRUE;
			do
			{
				if ( NumMonostablesUsedInRung[ScanArray]==FALSE )
					Result = ScanArray;
				ScanArray++;
			}
			while( ScanArray<NBR_MONOSTABLES && Result==-1 );
			break;
#endif
		case ELE_COUNTER:
			IsFunctionBlock = TRUE;
			do
			{
				if ( NumCountersUsedInRung[ScanArray]==FALSE )
					Result = ScanArray;
				ScanArray++;
			}
			while( ScanArray<NBR_COUNTERS && Result==-1 );
			break;
		case ELE_TIMER_IEC:
			IsFunctionBlock = TRUE;
			do
			{
				if ( NumNewTimersUsedInRung[ScanArray]==FALSE )
					Result = ScanArray;
				ScanArray++;
			}
			while( ScanArray<NBR_TIMERS_IEC && Result==-1 );
			break;
	}
	return (IsFunctionBlock)?Result:0;
}

void InitBufferRungEdited( StrRung * pRung )
{
	int x,y;
	for (y=0;y<RUNG_HEIGHT;y++)
	{
		for(x=0;x<RUNG_WIDTH;x++)
		{
			pRung->Element[x][y].Type = ELE_FREE;
			pRung->Element[x][y].ConnectedWithTop = 0;
			pRung->Element[x][y].VarType = 0;
			pRung->Element[x][y].VarNum = 0;
			pRung->Element[x][y].DynamicState = 0;
		}
	}
	pRung->Used = TRUE;
	pRung->PrevRung = -1;
	pRung->NextRung = -1;
}

int GetNbrRungsDefined( void )
{
	int NbrRungsUsed = 0;
	int ScanRung;
	for( ScanRung=0; ScanRung<NBR_RUNGS; ScanRung++ )
	{
		if (RungArray[ScanRung].Used)
			NbrRungsUsed++;
	}
	return NbrRungsUsed;
}

int FindFreeRung()
{
	int NumFree = -1;
	int ScanRung = 0;
	do
	{
		if (!RungArray[ScanRung].Used)
			NumFree = ScanRung;
		ScanRung++;
	}
	while( (NumFree==-1)&&(ScanRung<NBR_RUNGS) );
	return NumFree;
}

void AddRung()
{
	InitBufferRungEdited( &EditDatas.Rung );
	EditDatas.DoBeforeFinalCopy = MODE_ADD;
	EditDatas.NumRung = FindFreeRung();
	/* we have found a free rung ? */
	if (EditDatas.NumRung>=0)
	{
		EditDatas.ModeEdit = TRUE;
		EditDatas.ElementUnderEdit = NULL;
		autorize_prevnext_buttons(FALSE);
		clear_label_comment();
		CopyCurrentArithmExpr();
		CopyUsedFonctionBlockBeforeEdit( );
	}
}

void InsertRung()
{
	/* not really different than adding a rung... */
	AddRung();
	EditDatas.DoBeforeFinalCopy = MODE_INSERT;
}

void ModifyCurrentRung()
{
	CopyRungToRung(&RungArray[InfosGene->CurrentRung],&EditDatas.Rung);
	EditDatas.DoBeforeFinalCopy = MODE_MODIFY;
	EditDatas.NumRung = InfosGene->CurrentRung;
	EditDatas.ModeEdit = TRUE;
	EditDatas.ElementUnderEdit = NULL;
	autorize_prevnext_buttons(FALSE);
	CopyCurrentArithmExpr();
	CopyUsedFonctionBlockBeforeEdit( );
}

void DeleteCurrentRung()
{
	int NextDeleted;
	int PrevDeleted;
	int NewCurrent;
	int OldCurrent;
	int x,y;
	/* do not destroy the last one ! */
	if ( !((InfosGene->CurrentRung==InfosGene->FirstRung) && (InfosGene->CurrentRung==InfosGene->LastRung)) )
	{
		PrevDeleted = RungArray[InfosGene->CurrentRung].PrevRung;
		NextDeleted = RungArray[InfosGene->CurrentRung].NextRung;
		NewCurrent = NextDeleted;
		/* deleting first rung ?*/
		if (InfosGene->CurrentRung==InfosGene->FirstRung)
		{
			InfosGene->FirstRung = NextDeleted;
		}
		else
		{
			RungArray[PrevDeleted].NextRung = NextDeleted;
		}
		/* deleting last rung ? */
		if (InfosGene->CurrentRung==InfosGene->LastRung)
		{
			InfosGene->LastRung = PrevDeleted;
			NewCurrent = InfosGene->LastRung;
		}
		else
		{
			RungArray[NextDeleted].PrevRung = PrevDeleted;
		}
		/* the rung is now free ! */
		OldCurrent = InfosGene->CurrentRung;
		RungArray[InfosGene->CurrentRung].Used = FALSE;
		InfosGene->CurrentRung = NewCurrent;
		DrawRungs();
		UpdateVScrollBar();
		refresh_label_comment( );
		/* save infos for the current section */
		SectionArray[ InfosGene->CurrentSection ].FirstRung = InfosGene->FirstRung;
		SectionArray[ InfosGene->CurrentSection ].LastRung = InfosGene->LastRung;
		/* kill the expressions used on the rung deleted */
		for (y=0;y<RUNG_HEIGHT;y++)
		{
			for(x=0;x<RUNG_WIDTH;x++)
			{
				if ( (RungArray[OldCurrent].Element[x][y].Type == ELE_COMPAR)
				|| (RungArray[OldCurrent].Element[x][y].Type == ELE_OUTPUT_OPERATE) )
				{
					strcpy(ArithmExpr[ RungArray[OldCurrent].Element[x][y].VarNum ].Expr,"");
				}
			}
		}
	}
}

void CancelRungEdited()
{
	EditDatas.ModeEdit = FALSE;
	EditDatas.ElementUnderEdit = NULL;
	EditDatas.NumElementSelectedInToolBar = -1;
	LoadElementProperties(NULL);
	DrawRungs();
	refresh_label_comment( );
	autorize_prevnext_buttons(TRUE);
}

void ApplyRungEdited()
{
	int PrevNew;
	int NextNew;
	save_label_comment_edited();
	CopyRungToRung(&EditDatas.Rung,&RungArray[EditDatas.NumRung]);
	ApplyNewArithmExpr();

	/* if we have added or inserted, we will have to */
	/* modify the links between rungs */
	/* order is important when setting all this, if multi-tasking : */
	/* calc.c is using only .NextRung to updates all the rungs. */
	/* and also global FirstRung & LastRung to start and end */
	switch(EditDatas.DoBeforeFinalCopy)
	{
		/* Add rung edited after current rung */
		case MODE_ADD:
			PrevNew = InfosGene->CurrentRung;
			NextNew = RungArray[InfosGene->CurrentRung].NextRung;
			RungArray[EditDatas.NumRung].PrevRung = PrevNew;
			/* new added is not the last now ? */
			if (InfosGene->CurrentRung!=InfosGene->LastRung)
			{
				RungArray[EditDatas.NumRung].NextRung = NextNew;
				RungArray[NextNew].PrevRung = EditDatas.NumRung;
			}
			RungArray[PrevNew].NextRung = EditDatas.NumRung;
			/* we can now update LastRung */
			if (InfosGene->CurrentRung==InfosGene->LastRung)
			{
				InfosGene->LastRung = EditDatas.NumRung;
			}
			/* Here is the new current ! */
			InfosGene->CurrentRung = EditDatas.NumRung;
			UpdateVScrollBar();
			break;

		/* Insert rung edited before current rung */
		case MODE_INSERT:
			PrevNew = RungArray[InfosGene->CurrentRung].PrevRung;
			NextNew = InfosGene->CurrentRung;
			RungArray[EditDatas.NumRung].NextRung = NextNew;
			/* new added is not the first now ? */
			if (InfosGene->CurrentRung!=InfosGene->FirstRung)
			{
				RungArray[EditDatas.NumRung].PrevRung = PrevNew;
				RungArray[PrevNew].NextRung = EditDatas.NumRung;
			}
			else
			{
				InfosGene->FirstRung = EditDatas.NumRung;
			}
			RungArray[NextNew].PrevRung = EditDatas.NumRung;
			/* Here is the new current ! */
			InfosGene->CurrentRung = EditDatas.NumRung;
			UpdateVScrollBar();
			break;
	}

	/* save infos for the current section */
	SectionArray[ InfosGene->CurrentSection ].FirstRung = InfosGene->FirstRung;
	SectionArray[ InfosGene->CurrentSection ].LastRung = InfosGene->LastRung;

	EditDatas.ModeEdit = FALSE;
	EditDatas.ElementUnderEdit = NULL;
	EditDatas.NumElementSelectedInToolBar = -1;
	LoadElementProperties(NULL);
	DrawRungs();
	autorize_prevnext_buttons(TRUE);
	InfosGene->AskConfirmationToQuit = TRUE;
}



/* When we fall on an "unusable" element of a big one,
we return new posix and posiy of the "alive" block */
void CheckForBlocksOfBigElement(StrRung *pRungToCheck, int * PosiX,int * PosiY )
{
	int ScanX,ScanY;
	int ScanForRule;
	/* on an "unusable" ? */
	if (pRungToCheck->Element[*PosiX][*PosiY].Type==ELE_UNUSABLE)
	{
		/* we now will have to check for all the "alive" block of
		bigs elements */
		for (ScanY=0;ScanY<RUNG_HEIGHT;ScanY++)
		{
			for(ScanX=0;ScanX<RUNG_WIDTH;ScanX++)
			{
				ScanForRule = 0;
				do
				{
					/* Is is an element with a rule ? */
					if (pRungToCheck->Element[ScanX][ScanY].Type == RulesForSpecialElements[ScanForRule][TYPEELERULE])
					{
						/* Have we clicked in it ? */
						if ( (*PosiX>=ScanX-RulesForSpecialElements[ScanForRule][XSIZEELERULE]-1)
							&& (*PosiX<=ScanX)
							&& (*PosiY>=ScanY)
							&& (*PosiY<=ScanY+RulesForSpecialElements[ScanForRule][YSIZEELERULE]-1) )
						{
							/* We've got it ! */
							/* We make as we have clicked on the "alive" block ! */
							*PosiX = ScanX;
							*PosiY = ScanY;
							return;
						}
					}
					ScanForRule++;
				}
				while(RulesForSpecialElements[ScanForRule][0]!=-1);
			}
		}
	}
}

int VerifyRulesForElement(short int NumEle,int PosiX,int PosiY)
{
	int RulePass;
	int ItIsOk = TRUE;
	int ItIsAnOutputEle = FALSE;

	if ( (NumEle==ELE_OUTPUT) || (NumEle==ELE_OUTPUT_NOT)
			|| (NumEle==ELE_OUTPUT_SET) || (NumEle==ELE_OUTPUT_RESET)
			|| (NumEle==ELE_OUTPUT_JUMP) || (NumEle==ELE_OUTPUT_CALL)
			|| (NumEle==ELE_OUTPUT_OPERATE) )
		ItIsAnOutputEle = TRUE;

	/* verify for outputs if we are under output zone (right column) */
	if ( (PosiX==RUNG_WIDTH-1) && !ItIsAnOutputEle )
	{
			ItIsOk = FALSE;
	}
	/* verify for inputs if we are under input zone (not right column) */
	if ( (PosiX<RUNG_WIDTH-1) && ItIsAnOutputEle )
	{
			ItIsOk = FALSE;
	}

	/* verify if for elements bigger than one block it will fit */
	RulePass = 0;
	do
	{
		if ( RulesForSpecialElements[RulePass][TYPEELERULE] == NumEle )
		{
			if ( (PosiX-RulesForSpecialElements[RulePass][XSIZEELERULE]+1 < 0)
				|| (PosiY+RulesForSpecialElements[RulePass][YSIZEELERULE]-1 >= RUNG_HEIGHT) )
				ItIsOk = FALSE;
		}
		RulePass++;
	}
	while(RulesForSpecialElements[RulePass][0]!=-1);

	/* verify if we are not on another part of a big element ! */
	if (EditDatas.Rung.Element[PosiX][PosiY].Type==ELE_UNUSABLE)
		ItIsOk = FALSE;

	return ItIsOk;
}

/* used for destroying or adding a big element */
/* when destroying : filling all the blocks with ELE_FREE */
/* when adding : filling all the blocks with ELE_UNUSABLE */
/* the block "alive" is written elsewhere */
void CleanForBigElement(short int NumEle,int PosiX,int PosiY,short int FillWithThis)
{
	int RulePass;
	int PassX,PassY;
	RulePass = 0;
	do
	{
		if (RulesForSpecialElements[RulePass][TYPEELERULE] == NumEle )
		{
			for (PassX = PosiX - RulesForSpecialElements[RulePass][XSIZEELERULE] +1 ; PassX<=PosiX ; PassX++)
			{
				for (PassY = PosiY ; PassY<=PosiY + RulesForSpecialElements[RulePass][YSIZEELERULE] -1 ; PassY++)
				{
					CheckForFreeingArithmExpr(PassX,PassY);

					SetUsedStateFunctionBlock( EditDatas.Rung.Element[PassX][PassY].Type, EditDatas.Rung.Element[PassX][PassY].VarNum, FALSE );

					EditDatas.Rung.Element[PassX][PassY].Type = FillWithThis;
					EditDatas.Rung.Element[PassX][PassY].ConnectedWithTop = 0;
					EditDatas.Rung.Element[PassX][PassY].DynamicOutput = 0;
				}
			}
		}
		RulePass++;
	}
	while(RulesForSpecialElements[RulePass][0]!=-1);
}

void CheckBigElementTopLeft(short int NumEle,int * PosiX)
{
	int RulePass;
	RulePass = 0;
	do
	{
		if (RulesForSpecialElements[RulePass][TYPEELERULE] == NumEle )
		{
			*PosiX = *PosiX + RulesForSpecialElements[RulePass][XSIZEELERULE] - 1;
			if ( *PosiX>=RUNG_WIDTH )
			{
				*PosiX = RUNG_WIDTH-1;
			}
		}
		RulePass++;
	}
	while(RulesForSpecialElements[RulePass][0]!=-1);
}

void GetSizesOfAnElement(short int NumEle,int * pSizeX, int * pSizeY)
{
	int RulePass;
	// default size of an element	
	*pSizeX = 1;
	*pSizeY = 1;
	RulePass = 0;
	do
	{
		if (RulesForSpecialElements[RulePass][TYPEELERULE] == NumEle )
		{
			*pSizeX = RulesForSpecialElements[RulePass][XSIZEELERULE];
			*pSizeY = RulesForSpecialElements[RulePass][YSIZEELERULE];
		}
		RulePass++;
	}
	while( RulesForSpecialElements[RulePass][0]!=-1 && *pSizeX==1 && *pSizeY==1 );
	if ( NumEle==ELE_FREE || NumEle==ELE_CONNECTION || NumEle==ELE_UNUSABLE )
	{
		*pSizeX = 0;
		*pSizeY = 0;
	}
	if ( NumEle>=EDIT_CNX_WITH_TOP )
	{
		printf("!!!Abnormal current type=%d in rung...(file %s,line %d)\n", NumEle, __FILE__, __LINE__ );
		*pSizeX = 0;
		*pSizeY = 0;
	}
}

/* return TRUE if contact or coil element */
int IsASimpleElement(int Element)
{
	int Res = FALSE;
	if ( (Element==ELE_INPUT) || (Element==ELE_INPUT_NOT)
		|| (Element==ELE_RISING_INPUT) || (Element==ELE_FALLING_INPUT)
		|| (Element==ELE_OUTPUT) || (Element==ELE_OUTPUT_NOT)
		|| (Element==ELE_OUTPUT_SET) || (Element==ELE_OUTPUT_RESET)
		|| (Element==ELE_OUTPUT_JUMP) || (Element==ELE_OUTPUT_CALL) )
		Res = TRUE;
	return Res;
}


char ConvertDoublesToRungCoor( double coorx, double coory, int * pRungX, int * pRungY )
{
	char cOk = FALSE;
	/* correspond to which block ? */
	*pRungX = coorx/InfosGene->BlockWidth;
	*pRungY = coory/InfosGene->BlockHeight;
	if ( (*pRungX<RUNG_WIDTH) && (*pRungY<RUNG_HEIGHT) )
	{
		cOk = TRUE;
	}
	return cOk;
}

/* click with the mouse in x and y pixels of the rung */
void EditElementInRung(double x,double y)
{
	int RungX,RungY;
	short int NumElement;
	if ( ConvertDoublesToRungCoor( x, y, &RungX, &RungY )
		&& (EditDatas.NumElementSelectedInToolBar!=-1) )
	{
                 //printf ("x:%f Y:%f xx%f yy%f rungX=%d rungY=%d\n",x,y,x-RungX*InfosGene->BlockWidth, y-RungY*InfosGene->BlockHeight, RungX, RungY);
		/* check for "unusable" blocks */
		if (EditDatas.NumElementSelectedInToolBar==EDIT_POINTER || EditDatas.NumElementSelectedInToolBar==EDIT_ERASER )
			CheckForBlocksOfBigElement( &EditDatas.Rung, &RungX,&RungY );
		if (EditDatas.NumElementSelectedInToolBar!=EDIT_CNX_WITH_TOP
				&& EditDatas.NumElementSelectedInToolBar!=EDIT_POINTER
				&& EditDatas.NumElementSelectedInToolBar!=EDIT_LONG_CONNECTION )
		{
			NumElement = EditDatas.NumElementSelectedInToolBar;
			/* For big element, click insert top-left of the element */
			CheckBigElementTopLeft(NumElement,&RungX);
			/* already the same element ? => if yes kill it! */
			if (NumElement==EditDatas.Rung.Element[RungX][RungY].Type)
			{
				NumElement = EDIT_ERASER;
			}
			/* apply the new element */
			if (NumElement==EDIT_ERASER)
			{
				/* the blocks other than the "alive" are now free... */
				CleanForBigElement(EditDatas.Rung.Element[RungX][RungY].Type,RungX,RungY,ELE_FREE);
			        EditDatas.Rung.Element[RungX][RungY].DynamicOutput = 0;
                                if (((x-RungX*InfosGene->BlockWidth)>=0) && ((x-RungX*InfosGene->BlockWidth)<=15) 
                                   && (EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop))
				      {			                   
				            EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop = 0;
			              }else{
				           EditDatas.Rung.Element[RungX][RungY].Type = ELE_FREE;
			                   }
			}else{
				if (VerifyRulesForElement(NumElement,RungX,RungY))
				{
					/* the blocks other than the "alive" are now free... */
					CleanForBigElement(EditDatas.Rung.Element[RungX][RungY].Type,RungX,RungY,ELE_FREE);
					/* for big element with only one block "alive" we must mark the others */
					/* blocks as unusable */
					CleanForBigElement(NumElement,RungX,RungY,ELE_UNUSABLE);
					// if not a simple element (contact/coil) replaced by a simple element, clean VarNum
					if ( !(IsASimpleElement(NumElement) && IsASimpleElement(EditDatas.Rung.Element[RungX][RungY].Type)) )
					{
						EditDatas.Rung.Element[RungX][RungY].VarNum = 0;
					}
					// function block element, set directly a free number one...
					if ( !(IsASimpleElement(NumElement) ) )
					{
						EditDatas.Rung.Element[RungX][RungY].VarNum = GetFreeNumberFunctionBlock( NumElement );
						//TODO: better error handling here (not creating element after...)
						if ( EditDatas.Rung.Element[RungX][RungY].VarNum==-1 )
						{
							ShowMessageBox( "Error", "No more free function block available...please delete!", "Ok" );
							EditDatas.Rung.Element[RungX][RungY].VarNum = 0;
						}
						SetUsedStateFunctionBlock( NumElement, EditDatas.Rung.Element[RungX][RungY].VarNum, TRUE );
					}
					// if the area was free before then set the default variable letter based on edit type
					if (EditDatas.Rung.Element[RungX][RungY].Type == ELE_FREE)
					{    int defaultvarname= SetDefaultVariableType(NumElement); 
  						if (defaultvarname!=ELE_NO_DEFAULT_NAME)
						{   EditDatas.Rung.Element[RungX][RungY].VarType =defaultvarname ;   }
					}

					EditDatas.Rung.Element[RungX][RungY].Type = NumElement;
					CheckForAllocatingArithmExpr(RungX,RungY);
				}
			}
		}
		if (EditDatas.NumElementSelectedInToolBar==EDIT_CNX_WITH_TOP)
		{
			if (EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop)
			{
				EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop = 0;
			}
			else
			{
				if (RungY>0)
					EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop = 1;
			}
		}
		if (EditDatas.NumElementSelectedInToolBar==EDIT_LONG_CONNECTION)
		{
			int ScanX = RungX;
			while(EditDatas.Rung.Element[ScanX][RungY].Type==ELE_FREE && ScanX<RUNG_WIDTH-1 )
			{
				EditDatas.Rung.Element[ScanX++][RungY].Type = ELE_CONNECTION;
			}
		}
// printf("current type = %d\n",EditDatas.Rung.Element[RungX][RungY].Type);
// if (EditDatas.Rung.Element[RungX][RungY].Type == ELE_FREE)  {printf("is free\n");}
		LoadElementProperties(&EditDatas.Rung.Element[RungX][RungY]);
		EditDatas.ElementUnderEdit = &EditDatas.Rung.Element[RungX][RungY];
		// infos used to display the "selected element" box
		EditDatas.CurrentElementPosiX = RungX;
		EditDatas.CurrentElementPosiY = RungY;
		GetSizesOfAnElement( EditDatas.Rung.Element[RungX][RungY].Type, &EditDatas.CurrentElementSizeX, &EditDatas.CurrentElementSizeY);
	}
}

/* click with the mouse in x and y pixels of the section display */
void EditElementInThePage(double x,double y)
{
	int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
	if ( iCurrentLanguage==SECTION_IN_LADDER )
	{
		if ( ( y >= InfosGene->OffsetCurrentRungDisplayed ) && ( y < InfosGene->OffsetCurrentRungDisplayed+InfosGene->BlockHeight*RUNG_HEIGHT ) )
			EditElementInRung( x, y - InfosGene->OffsetCurrentRungDisplayed );
	}
#ifdef SEQUENTIAL_SUPPORT
	if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
		EditElementInSeqPage( x+InfosGene->HScrollValue, y+InfosGene->VScrollValue );
#endif
}

char * GetLadderElePropertiesForStatusBar(double x,double y)
{
	if ( ( y >= InfosGene->OffsetCurrentRungDisplayed ) && ( y < InfosGene->OffsetCurrentRungDisplayed+InfosGene->BlockHeight*RUNG_HEIGHT ) )
	{
		int RungX,RungY;
		if ( ConvertDoublesToRungCoor( x, y - InfosGene->OffsetCurrentRungDisplayed, &RungX, &RungY ) )
		{
			StrElement * Element;
			CheckForBlocksOfBigElement( &RungArray[InfosGene->CurrentRung], &RungX,&RungY );
			Element = &RungArray[InfosGene->CurrentRung].Element[RungX][RungY];
			return GetElementPropertiesForStatusBar( Element );
		}
	}
	return "";
}
// this sets the variable type based on the type of edit-element (NumElement) being used  
int SetDefaultVariableType(int NumElement)
{
	switch  ( NumElement)
	{
			case ELE_INPUT:
			case ELE_INPUT_NOT:
			case ELE_RISING_INPUT:
			case ELE_FALLING_INPUT:
					 return 50; //letter I
			break;
			case ELE_OUTPUT:
			case ELE_OUTPUT_NOT:
			case ELE_OUTPUT_SET:
			case ELE_OUTPUT_RESET:
			 		return 60; //letter Q
		break;
		default:;
	}
return ELE_NO_DEFAULT_NAME;
}
