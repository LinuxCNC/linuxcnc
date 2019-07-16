/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* June 2007 (but only old code moved here!) */
/* --------------------------------------- */
/* Variable names strings for display/edit */
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
/* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include <locale.h>
#include <libintl.h>
#define _(x) gettext(x)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "classicladder.h"
#include "global.h"

#include "symbols.h"
#include "vars_names.h"

// include the var name table (not a header!)
#include "vars_names_list.c"



//old code with names directly in it...
//to be deleted, but some precise errors codes aren't present in the new function...
#ifdef AAAAAAAAA
char * CreateVarName(int Type, int Offset)
{
	static char Buffer[20];
	switch(Type)
	{
		case VAR_MEM_BIT:
			sprintf(Buffer,"%cB%d",'%',Offset);
			break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case VAR_TIMER_DONE:
			sprintf(Buffer,"%cT%d.D",'%',Offset);
			break;
		case VAR_TIMER_RUNNING:
			sprintf(Buffer,"%cT%d.R",'%',Offset);
			break;
		case VAR_MONOSTABLE_RUNNING:
			sprintf(Buffer,"%cM%d.R",'%',Offset);
			break;
#endif
		case VAR_TIMER_IEC_DONE:
			sprintf(Buffer,"%cTM%d.Q",'%',Offset);
			break;
		case VAR_COUNTER_DONE:
			sprintf(Buffer,"%cC%d.D",'%',Offset);
			break;
		case VAR_COUNTER_EMPTY:
			sprintf(Buffer,"%cC%d.E",'%',Offset);
			break;
		case VAR_COUNTER_FULL:
			sprintf(Buffer,"%cC%d.F",'%',Offset);
			break;
		case VAR_STEP_ACTIVITY:
			sprintf(Buffer,"%cX%d",'%',Offset);
			break;
		case VAR_PHYS_INPUT:
			sprintf(Buffer,"%cI%d",'%',Offset);
			break;
		case VAR_PHYS_OUTPUT:
			sprintf(Buffer,"%cQ%d",'%',Offset);
			break;
		case VAR_MEM_WORD:
			sprintf(Buffer,"%cW%d",'%',Offset);
			break;
		case VAR_STEP_TIME:
			sprintf(Buffer,"%cX%d.V",'%',Offset);
			break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case VAR_TIMER_PRESET:
			sprintf(Buffer,"%cT%d.P",'%',Offset);
			break;
		case VAR_TIMER_VALUE:
			sprintf(Buffer,"%cT%d.V",'%',Offset);
			break;
		case VAR_MONOSTABLE_PRESET:
			sprintf(Buffer,"%cM%d.P",'%',Offset);
			break;
		case VAR_MONOSTABLE_VALUE:
			sprintf(Buffer,"%cM%d.V",'%',Offset);
			break;
#endif
		case VAR_TIMER_IEC_PRESET:
			sprintf(Buffer,"%cTM%d.P",'%',Offset);
			break;
		case VAR_TIMER_IEC_VALUE:
			sprintf(Buffer,"%cTM%d.V",'%',Offset);
			break;
		case VAR_COUNTER_PRESET:
			sprintf(Buffer,"%cC%d.P",'%',Offset);
			break;
		case VAR_COUNTER_VALUE:
			sprintf(Buffer,"%cC%d.V",'%',Offset);
			break;
		default:
			sprintf(Buffer,"???");
			break;
	}
printf(_("infogene display symbols=&i\n"),InfosGene->DisplaySymbols);
	if ( InfosGene->DisplaySymbols )
	{ 
		// verify if a symbol has been defined for the variable...
		char * Symbol = ConvVarNameToSymbol( Buffer );
		if ( (Symbol!=NULL )||(Symbol[0]!=' ')||(Symbol[0]!='\0')){
			return Symbol;}
	}
    return Buffer;
}


/* Convert a string to a number if >=Min and <=Max */
/* return TRUE if okay */
/* (pointer advance !) */
int TextToNumberAndAdvance(char ** text,int ValMin,int ValMaxi,int *ValFound)
{
	int IsOk = TRUE;
	int Value = 0;
	char * pScan = *text;
	char NumberFound = FALSE;
	while( *pScan>='0' && *pScan<='9' )
	{
		Value = Value*10+(*pScan-'0');
		pScan++;
		NumberFound = TRUE;
	}
	if ( !NumberFound || (Value<ValMin) || (Value>ValMaxi) )
		IsOk = FALSE;
	if (IsOk)
	{
		*ValFound = Value;
		*text = pScan;
	}
	return IsOk;
}

/* Convert a string to variable (type/offset) */
/* if pNumberOfChars not NULL, give length of the string found */
/* return TRUE if okay */
char TextParserForAVar( char * text,int * VarTypeFound,int * VarOffsetFound, int * pNumberOfChars, char PartialNames )
{
	int TypeFound = 0;
	int OffsetFound;
	int IsOk = TRUE;
	char TimerIEC = FALSE;
	char * pScanPos = text; // content will be modified when advancing!
	char * * pPtrScanPos = &pScanPos;
	char * VarNameFromSymbol = NULL;
	int MaxNbr = 0;

	// if not commencing per '%', search the corresponding symbol !
	if ( *pScanPos!='%' )
	{
		VarNameFromSymbol = ConvSymbolToVarName( pScanPos );
		if ( VarNameFromSymbol==NULL )
		{
			IsOk = FALSE;
			ErrorMessageVarParser = _("Unknown symbol for variable name");
		}
		else
		{
			pScanPos = VarNameFromSymbol;
		}
	}

	if ( IsOk )
	{
		// pass the first character '%'
		(*pPtrScanPos)++;
		switch(**pPtrScanPos)
		{
			case 'B':
				TypeFound = VAR_MEM_BIT;
				(*pPtrScanPos)++;
				if (!TextToNumberAndAdvance(pPtrScanPos,0,NBR_BITS-1,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				break;
	
			case 'T':
				(*pPtrScanPos)++;
				// New IEC Timer ?
				if ( **pPtrScanPos=='M' )
				{
					TimerIEC = TRUE;
					(*pPtrScanPos)++;
				}
				MaxNbr = NBR_TIMERS_IEC-1;
#ifdef OLD_TIMERS_MONOS_SUPPORT
				if ( TimerIEC==FALSE )
					MaxNbr = NBR_TIMERS-1;
#else
				TimerIEC = TRUE;
#endif
				if (!TextToNumberAndAdvance(pPtrScanPos,0,MaxNbr,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				else
				{
					if ( PartialNames==FALSE )
					{
						if ( **pPtrScanPos!=VAR_ATTRIBUTE_SEP )
						{
							IsOk = FALSE;
							ErrorMessageVarParser = _("Unknown variable (missing '.' character before attribute)");
						}
						else
						{
							(*pPtrScanPos)++;
							switch(**pPtrScanPos)
							{
								case 'D':
									if ( TimerIEC==FALSE )
									{
										TypeFound = VAR_TIMER_DONE;
									}
									else
									{
										IsOk = FALSE;
										ErrorMessageVarParser = _("Unknown variable (unknown attribute)");
									}
									break;
								case 'Q':
									if ( TimerIEC==TRUE )
									{
										TypeFound = VAR_TIMER_IEC_DONE;
									}
									else
									{
										IsOk = FALSE;
										ErrorMessageVarParser = _("Unknown variable (unknown attribute)");
									}
									break;
								case 'R':
									if ( TimerIEC==FALSE )
									{
										TypeFound = VAR_TIMER_RUNNING;
									}
									else
									{
										IsOk = FALSE;
										ErrorMessageVarParser = _("Unknown variable (unknown attribute)");
									}
									break;
								case 'P':
									TypeFound = (TimerIEC==FALSE)?VAR_TIMER_PRESET:VAR_TIMER_IEC_PRESET;
									break;
								case 'V':
									TypeFound = (TimerIEC==FALSE)?VAR_TIMER_VALUE:VAR_TIMER_IEC_VALUE;
									break;
								default:
									IsOk = FALSE;
									ErrorMessageVarParser = _("Unknown variable (unknown attribute)");
									break;
							}
							(*pPtrScanPos)++;
						}
					}
				}
				break;
	
#ifdef OLD_TIMERS_MONOS_SUPPORT
			case 'M':
				(*pPtrScanPos)++;
				if (!TextToNumberAndAdvance(pPtrScanPos,0,NBR_MONOSTABLES-1,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				else
				{
					if ( PartialNames==FALSE )
					{
						if ( **pPtrScanPos!=VAR_ATTRIBUTE_SEP )
						{
							IsOk = FALSE;
							ErrorMessageVarParser = _("Unknown variable (missing '.' character before attribute)");
						}
						else
						{
							(*pPtrScanPos)++;
							switch(**pPtrScanPos)
							{
								case 'R':
									TypeFound = VAR_MONOSTABLE_RUNNING;
									break;
								case 'P':
									TypeFound = VAR_MONOSTABLE_PRESET;
									break;
								case 'V':
									TypeFound = VAR_MONOSTABLE_VALUE;
									break;
								default:
									IsOk = FALSE;
									ErrorMessageVarParser = _("Unknown variable (unknown attribute)");
									break;
							}
							(*pPtrScanPos)++;
						}
					}
				}
				break;
#endif
	
			case 'C':
				(*pPtrScanPos)++;
				if (!TextToNumberAndAdvance(pPtrScanPos,0,NBR_COUNTERS-1,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				else
				{
					if ( PartialNames==FALSE )
					{
						if ( **pPtrScanPos!=VAR_ATTRIBUTE_SEP )
						{
							IsOk = FALSE;
							ErrorMessageVarParser = _("Unknown variable (missing '.' character before attribute)");
						}
						else
						{
							(*pPtrScanPos)++;
							switch(**pPtrScanPos)
							{
								case 'D':
									TypeFound = VAR_COUNTER_DONE;
									break;
								case 'E':
									TypeFound = VAR_COUNTER_EMPTY;
									break;
								case 'F':
									TypeFound = VAR_COUNTER_FULL;
									break;
								case 'P':
									TypeFound = VAR_COUNTER_PRESET;
									break;
								case 'V':
									TypeFound = VAR_COUNTER_VALUE;
									break;
								default:
									IsOk = FALSE;
									ErrorMessageVarParser = _("Unknown variable (unknown attribute)");
									break;
							}
							(*pPtrScanPos)++;
						}
					}
				}
				break;
	
			case 'I':
				TypeFound = VAR_PHYS_INPUT;
				(*pPtrScanPos)++;
				if (!TextToNumberAndAdvance(pPtrScanPos,0,NBR_PHYS_INPUTS-1,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				break;
	
			case 'Q':
				TypeFound = VAR_PHYS_OUTPUT;
				(*pPtrScanPos)++;
				if (!TextToNumberAndAdvance(pPtrScanPos,0,NBR_PHYS_OUTPUTS-1,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				break;
	
			case 'W':
				TypeFound = VAR_MEM_WORD;
				(*pPtrScanPos)++;
				if (!TextToNumberAndAdvance(pPtrScanPos,0,NBR_WORDS-1,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				break;
	
	#ifdef SEQUENTIAL_SUPPORT
			case 'X':
				TypeFound = VAR_STEP_ACTIVITY; //per default, but it could change later...
				(*pPtrScanPos)++;
				if (!TextToNumberAndAdvance(pPtrScanPos,0,NBR_STEPS-1,&OffsetFound))
				{
					IsOk = FALSE;
					ErrorMessageVarParser = _("Unknown variable (number value out of bound)");
				}
				else
				{
					// verify if there is a '.' after number so that it must be the Xyyy.V form
					// instead of Xyyy form.
					if ( **pPtrScanPos==VAR_ATTRIBUTE_SEP )
					{
						(*pPtrScanPos)++;
						if ( **pPtrScanPos=='V' )
						{
							TypeFound = VAR_STEP_TIME;
						}
						else
						{
							IsOk = FALSE;
							ErrorMessageVarParser = _("Unknown variable (unknown attribute)");
						}
					}
				}
				break;
	#endif
	
			default:
				IsOk = FALSE;
				ErrorMessageVarParser = _("Unknown variable (on first character following %)");
		}
		if (IsOk)
		{
			if ( VarTypeFound )
				*VarTypeFound = TypeFound;
			if ( VarOffsetFound ) 
				*VarOffsetFound = OffsetFound;
			if ( pNumberOfChars!=NULL )
				*pNumberOfChars = (VarNameFromSymbol==NULL)?*pPtrScanPos-text : *pPtrScanPos-VarNameFromSymbol;
		}
		else
		{
			if ( ErrorMessageVarParser==NULL )
				ErrorMessageVarParser = _("Unknown variable (global error)");
		}
	}
	return IsOk;
}
#endif



/* return 0 if bad or the number of characters to pass... */
int ComparStaticPart(char * pSearchedString, char * pComparString)
{
	int iNbrCaracs = 0;
	do
	{
		iNbrCaracs++;
	}
	while( (pComparString[ iNbrCaracs ]!='%') && (pComparString[ iNbrCaracs ]!='\0') );
	if (strncmp(pSearchedString,pComparString,iNbrCaracs)==0)
		return iNbrCaracs;
	return 0;
}

/* Convert variable name -> internal ID of the corresponding var */
/* a little parser... */
/* return -1 if not found, else the corresponding Variable ID */
char TextParserForAVar( char * TextToParse, int * VarTypeFound,int * VarOffsetFound, int * pNumberOfChars, char PartialNames )
{
	int iBalayTable = 0;
	char bFound = FALSE, bEchecCompar,cOneAttributPassed ;
	int iIdVar = -1,iBalayProf,iTaillePartieStatique;
	StrConvIdVarName * pConv;
	char * pVarCherch = TextToParse;
	char * pBalayVarCherch;
	int iScruteCarac;
	int iOffset1;
	int iOffset2;
	int iOffset3;
//printf( "TextParserForAVar:start...\n" );
	if (*TextToParse=='\0' )
		return FALSE;
	// if not commencing per '%', search the corresponding symbol !
	if (*pVarCherch!='%')
	{
		pVarCherch = ConvSymbolToVarName( TextToParse );
		if ( pVarCherch==NULL )
			return FALSE;
	}
	else
	{
		// seems too much short for a base name !
		if ( strlen(TextToParse)<3 )
			return FALSE;
	}
	// pass the start '%' before parsing
	pVarCherch++;

	// scanning the base name variables table
	do
	{
		pBalayVarCherch = pVarCherch;
		bEchecCompar = FALSE;
		iScruteCarac = 0;
		iBalayProf = 0;
		iOffset1 = 0;
		iOffset2 = 0;
		iOffset3 = 0;
		cOneAttributPassed = FALSE;
		// pointer on the table list
		pConv = &TableConvIdVarName[ iBalayTable ];
		do
		{
			// start number mark ?
			if ( pConv->StringBaseVarName[ iScruteCarac ]=='%' )
			{
				// searching the end number mark in the base variable string
				do
				{
					iScruteCarac++;
				}
				while ( (pConv->StringBaseVarName[ iScruteCarac ]!='d')
						&& (pConv->StringBaseVarName[ iScruteCarac ]!='\0') );
				iScruteCarac++;
				// number value in the string searched
				if ( *pBalayVarCherch>='0' && *pBalayVarCherch<='9' )
				{
					int iNumFound = 0;
					do
					{
						iNumFound = iNumFound*10;
				        iNumFound = iNumFound + *pBalayVarCherch-'0';
						pBalayVarCherch++;
					}
					while( *pBalayVarCherch>='0' && *pBalayVarCherch<='9');
					switch( iBalayProf )
					{
						case 0: iOffset1 = iNumFound; break;
						case 1:	iOffset2 = iNumFound; break;
						case 2: iOffset3 = iNumFound; break;
					}
					iBalayProf++;
				}
				else
				{
					bEchecCompar = TRUE;
				}
			}
			else
			{
				if ( pConv->StringBaseVarName[ iScruteCarac ]=='.' )
					cOneAttributPassed = TRUE;
				// comparing static part...
				iTaillePartieStatique = ComparStaticPart(pBalayVarCherch,&pConv->StringBaseVarName[ iScruteCarac ]);
				if (iTaillePartieStatique>0)
				{
                	iScruteCarac = iScruteCarac+iTaillePartieStatique;
					pBalayVarCherch = pBalayVarCherch+iTaillePartieStatique;
				}
				else
				{
					bEchecCompar = TRUE;
					if ( PartialNames && cOneAttributPassed )
						return TRUE;
				}
			}

			// at the end of the reference string ?
			// if yes it means we have found what we were searching for (else perhaps another one exists starting by the same string)
			if ( pConv->StringBaseVarName[ iScruteCarac ]=='\0' )
			{
				// verify that there isn't too much characters after !
				// if yes, dropped...
				// Put in comment because it is used in the arithmetic parser !!!!!
				if ( !bEchecCompar /*   && *pBalayVarCherch=='\0'  */ )
					bFound = TRUE;
				else
					bEchecCompar = TRUE;
			}
		}
		while( !bEchecCompar && !bFound );
		iBalayTable++;

	}
	while( !bFound && TableConvIdVarName[ iBalayTable ].StringBaseVarName );

	if (bFound)
	{
		char bVerifCoherenceOk = TRUE;

		// verify if offsets given are in
        if ( iOffset1>=pConv->iSize1+pConv->iFirstVal1 )
			bVerifCoherenceOk = FALSE;
        if ( iOffset1<pConv->iFirstVal1 )
			bVerifCoherenceOk = FALSE;
		if ( pConv->iSize2>0 )
		{
	        if ( iOffset2>=pConv->iSize2+pConv->iFirstVal2 )
				bVerifCoherenceOk = FALSE;
	        if ( iOffset2<pConv->iFirstVal2 )
				bVerifCoherenceOk = FALSE;
		}
		if ( pConv->iSize3>0 )
		{
	        if ( iOffset3>=pConv->iSize3+pConv->iFirstVal3 )
				bVerifCoherenceOk = FALSE;
	        if ( iOffset3<pConv->iFirstVal3 )
				bVerifCoherenceOk = FALSE;
		}
		if ( bVerifCoherenceOk==FALSE )
			ErrorMessageVarParser = _("Unknown variable (number value out of bound)");

		if (bVerifCoherenceOk)
		{
			// calc final Id
			if ( pConv->iSize3<=0 && pConv->iSize2<=0 )
			{
				iIdVar = pConv->iIdVar + iOffset1-pConv->iFirstVal1;
			}
			else
			{
				if ( pConv->iSize3<=0 )
					iIdVar = pConv->iIdVar + (iOffset1-pConv->iFirstVal1)*pConv->iSize2 + iOffset2-pConv->iFirstVal2;
				else
					iIdVar = pConv->iIdVar + (iOffset1-pConv->iFirstVal1)*pConv->iSize2*pConv->iSize3 + (iOffset2-pConv->iFirstVal2)*pConv->iSize3 + iOffset3-pConv->iFirstVal3;
			}
			if ( VarTypeFound!=NULL )
			{
#ifdef IDVAR_IS_TYPE_AND_OFFSET
				*VarTypeFound = pConv->iTypeVar;
#else
				*VarTypeFound = VAR_DEFAULT_TYPE;
#endif
			}
			if ( VarOffsetFound!=NULL )
				*VarOffsetFound = iIdVar;
			if ( pNumberOfChars!=NULL )
				*pNumberOfChars = pBalayVarCherch-TextToParse;
		}
		else
		{
			bFound = FALSE;
		}
	}

	return bFound;
}


StrConvIdVarName * ConvIdVarEnPtrSurEleConv( int iTypeVarToSearch, int iIdVarCherchee )
{
	int iBalayTable = 0;
	char bFound = FALSE;
	int iIdFound = 0;
	int iSizeTotal = 0;
	StrConvIdVarName * pConv;
	// research in the table list
	do
	{
		pConv = &TableConvIdVarName[ iBalayTable ];
		// same type ?
#ifdef IDVAR_IS_TYPE_AND_OFFSET
		if ( pConv->iTypeVar==iTypeVarToSearch )
#endif
		{
			iIdFound = pConv->iIdVar;
			iSizeTotal = (pConv->iSize2<=0)?pConv->iSize1:pConv->iSize1*pConv->iSize2;
			if ( pConv->iSize3>0 )
				iSizeTotal *= pConv->iSize3;
			/* Offset value between start and end values ? */
			if ( (iIdFound<=iIdVarCherchee) && (iIdVarCherchee<(iIdFound+iSizeTotal)) )
				bFound = TRUE;
			else
				iBalayTable++;
		}
#ifdef IDVAR_IS_TYPE_AND_OFFSET
		else
		{
			iBalayTable++;
		}
#endif
	}
	while (!bFound && TableConvIdVarName[ iBalayTable].StringBaseVarName);
	if (bFound)
		return pConv;
	else
		return NULL;
}

/* Convert a Type/Id variable into a default base name */
/* return the string character or ??? if not found */
char * CreateVarName( int TypeVarSearched, int OffsetVarSearched )
{
	static char tcBuffer[ 100 ];
	static char tcBuffer2[ 100 ];
	int iIdFound;
	StrConvIdVarName * pConv;
	// research in the table list
	pConv = ConvIdVarEnPtrSurEleConv( TypeVarSearched, OffsetVarSearched );
	if (pConv)
	{
		strcpy(tcBuffer,"%");
		iIdFound = pConv->iIdVar;
		if (pConv->iSize2<=0 && pConv->iSize3<=0)
		{
			sprintf(tcBuffer2, pConv->StringBaseVarName, OffsetVarSearched-iIdFound +pConv->iFirstVal1 );
		}
		else
		{
			if (pConv->iSize3<=0)
				sprintf(tcBuffer2, pConv->StringBaseVarName, (OffsetVarSearched-iIdFound)/pConv->iSize2 +pConv->iFirstVal1,
					(OffsetVarSearched-iIdFound)%pConv->iSize2 +pConv->iFirstVal2 );
			else
				sprintf(tcBuffer2, pConv->StringBaseVarName, (OffsetVarSearched-iIdFound)/(pConv->iSize2*pConv->iSize3) +pConv->iFirstVal1,
					((OffsetVarSearched-iIdFound)/pConv->iSize3)%pConv->iSize2 +pConv->iFirstVal2, (OffsetVarSearched-iIdFound)%pConv->iSize3 +pConv->iFirstVal3 );
		}
		strcat(tcBuffer,tcBuffer2);

		if ( InfosGene->DisplaySymbols )
		{
			// verify if a symbol has been defined for the variable...
			char * Symbol = ConvVarNameToSymbol( tcBuffer );
			if ( (Symbol!=NULL ) && ((Symbol[0]!=' ') || (Symbol[0]!='\0'))) {return Symbol;}
		}
		return tcBuffer;
	}
	else
	{
		return "???";
	}
}

char TestVarIsReadWrite( int TypeVarTested, int OffsetVarTested )
{
	StrConvIdVarName * pConv;
	// research in the table list
	pConv = ConvIdVarEnPtrSurEleConv( TypeVarTested, OffsetVarTested );
	if (pConv)
	{
		return pConv->cReadWriteAccess;
	}
	return FALSE;
}
