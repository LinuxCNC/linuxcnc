/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* October 2006 */
/* ------- */
/* Symbols */
/* ------- */
/* Utilities functions to convert between variables names (starting with % character) */
/* and symbols names */
/* A symbol name can be set for a complete variable (ex %B45) or for a partial variable */
/* (ex: "zzz"=%T5 -> partial variable name before "." character : after this symbols will */
/* be available zzz.D zzz.V zzz.P etc...) */
/* ------- */
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

#ifndef MODULE
#include <stdio.h>
#include <string.h>
#endif

#include "classicladder.h"
#include "global.h"
#include "symbols.h"
#include "hal/hal_priv.h"

void InitSymbols( void )
{
	int ScanSymb;
	for ( ScanSymb=0; ScanSymb<NBR_SYMBOLS; ScanSymb++ )
	{
		SymbolArray[ ScanSymb ].VarName[ 0 ] = '\0';
		SymbolArray[ ScanSymb ].Symbol[ 0 ] = '\0';
		SymbolArray[ ScanSymb ].Comment[ 0 ] = '\0';
	}
}


// search symbol (partial or complete) for the var name parameter
StrSymbol * ConvVarNameInSymbolPtr( char * tcVarNameVar )
{
	StrSymbol * pFoundSymbol = NULL;
	if ( tcVarNameVar[0]=='%' )
	{
		int ScanSymbol = 0;
		StrSymbol *pCurrentSymbol;
		char bFound = FALSE;
		char tcVarNamePartial[ LGT_VAR_NAME ];
		// creating partial var name (before '.' attribute)
		char * pVarNameSrc = tcVarNameVar;
		char * pVarNameDest = tcVarNamePartial;
		do
		{
			*pVarNameDest++ = *pVarNameSrc++;
		}
		while( *pVarNameSrc!=VAR_ATTRIBUTE_SEP && *pVarNameSrc!='\0' );
		*pVarNameDest = '\0';
		// searching complete or partial in symbols list
		do
		{
			pCurrentSymbol = &SymbolArray[ ScanSymbol ];
			// verify strings for complete & partial
			if ( strcmp( tcVarNameVar, pCurrentSymbol->VarName )==0
				|| strcmp(tcVarNamePartial, pCurrentSymbol->VarName )==0 )
			{
				bFound = TRUE;
				pFoundSymbol = pCurrentSymbol;
			}
			ScanSymbol++;
		}
		while( !bFound && ScanSymbol<NBR_SYMBOLS );
	}
	return pFoundSymbol;
}

// search symbol (partial or complete) for the symbol parameter (partial or complete)
StrSymbol * ConvSymbolParamInSymbolPtr( char * tcMnemoSaisi )//, char bPartiel )
{
	StrSymbol * pFoundSymbol = NULL;
	int ScanSymbol = 0;
	StrSymbol *pCurrentSymbol;
	char bFound = FALSE;
	char tcPartialSymbolString[ LGT_SYMBOL_STRING ];
	// creating partial var name (before '.' attribute)
	char * pVarNameSrc = tcMnemoSaisi;
	char * pVarNameDest = tcPartialSymbolString;
	do
	{
		*pVarNameDest++ = *pVarNameSrc++;
	}
	while( *pVarNameSrc!=VAR_ATTRIBUTE_SEP && *pVarNameSrc!='\0' );
	*pVarNameDest = '\0';
	// searching complete or partial in symbols list
	do
	{
		pCurrentSymbol = &SymbolArray[ ScanSymbol ];
		// not an empty symbol ?
		if ( pCurrentSymbol->Symbol[ 0 ]!='\0' )
		{
			// verify strings for complete & partial
			if ( strcmp( tcMnemoSaisi, pCurrentSymbol->Symbol )==0
				|| strcmp(tcPartialSymbolString, pCurrentSymbol->Symbol )==0 )
			{
				bFound = TRUE;
				pFoundSymbol = pCurrentSymbol;
			}
		}
		ScanSymbol++;
	}
	while( !bFound && ScanSymbol<NBR_SYMBOLS );
	return pFoundSymbol;
}


char tcBufferResult[ 100 ];
// return a symbol (completed with attribute if it is a partial symbol)
// for the var name parameter.
char * ConvVarNameToSymbol( char * VarNameParam )
{
	StrSymbol * pSymbol = ConvVarNameInSymbolPtr( VarNameParam );
        if( VarNameParam[0] == '%') {
            char pin_name[100] = {0};
            int arrowside=0;
            int idx;

            switch(VarNameParam[1]) {
            case 'I':
                sscanf(VarNameParam+2, "%d", &idx);
                snprintf(pin_name, 100, "classicladder.0.in-%02d", idx);
                arrowside = 1;
                break;
            case 'Q':
                sscanf(VarNameParam+2, "%d", &idx);
                snprintf(pin_name, 100, "classicladder.0.out-%02d", idx);
                arrowside = 0;
                break;
            case 'W':
                sscanf(VarNameParam+2, "%d", &idx);
                if(idx > InfosGene->SizesInfos.nbr_s32in) {
                    snprintf(pin_name, 100, "classicladder.0.s32out-%02d",
                            idx - InfosGene->SizesInfos.nbr_s32in);
                    arrowside = 0;
                } else {
                    snprintf(pin_name, 100, "classicladder.0.s32in-%02d", idx);
                    arrowside = 1;
                }
                break;
            }
            if(*pin_name) {
                hal_pin_t *pin = halpr_find_pin_by_name(pin_name);
                if(pin && pin->signal) {
                    hal_sig_t *sig = SHMPTR(pin->signal);
                    if(sig->name) {
                        static char sig_name[100];
                        // char *arrow = "\xe2\x86\x90";
                        char *arrow = "\xe2\x87\x92";

                        if(arrowside == 0) {
                            snprintf(sig_name, 100, "%s%s", sig->name, arrow);
                        } else {
                            snprintf(sig_name, 100, "%s%s", arrow, sig->name);
                        }
                        return sig_name;
                    }
                }
            }
        }

	if ( pSymbol )
	{
		// if partial symbol, add the attribute taken from the var name
		if ( strcmp( VarNameParam, pSymbol->VarName )!=0 )
		{
			char * pAttrStart = VarNameParam;
			// copy partial symbol
			strcpy( tcBufferResult, pSymbol->Symbol );
			// search the '.' separator
			do
			{
				pAttrStart++;
			}
			while( *pAttrStart!=VAR_ATTRIBUTE_SEP && *pAttrStart!='\0' );
			// add the attribute to the symbol
			if ( *pAttrStart==VAR_ATTRIBUTE_SEP )
				strcpy( &tcBufferResult[ strlen( pSymbol->Symbol ) ], pAttrStart );
		}
		else
		{
			// simple copy (a symbol = a var name directly)
			strcpy( tcBufferResult, pSymbol->Symbol );
		}
		return tcBufferResult;
	}
	else
	{
		return NULL;
	}
}

// return a var name for the symbol parameter (partial or complete symbol)
char * ConvSymbolToVarName( char * SymbolParam )
{
	StrSymbol * pSymbol = ConvSymbolParamInSymbolPtr( SymbolParam );
	if ( pSymbol )
	{
		// if partial symbol, add the attribute following the Symbol parameter
		if ( strcmp( SymbolParam, pSymbol->Symbol )!=0 )
		{
			char * pAttrStart = SymbolParam;
			// copy partial symbol
			strcpy( tcBufferResult, pSymbol->VarName );
			// search the '.' separator
			do
			{
				pAttrStart++;
			}
			while( *pAttrStart!=VAR_ATTRIBUTE_SEP && *pAttrStart!='\0' );
			// add the attribute to the var name
			if ( *pAttrStart==VAR_ATTRIBUTE_SEP )
				strcpy( &tcBufferResult[ strlen( pSymbol->VarName ) ], pAttrStart );
		}
		else
		{
			// simple copy (a symbol = a var name directly)
			strcpy( tcBufferResult, pSymbol->VarName );
		}
		return tcBufferResult;
	}
	else
	{
		return NULL;
	}
}

