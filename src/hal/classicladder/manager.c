/* Classic Ladder Project */
/* Copyright (C) 2001-2008 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* August 2002 */
/* ---------------------------- */
/* Sections functions utilities */
/* ---------------------------- */
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
#ifdef HAL_SUPPORT
#include "rtapi.h"
#include "rtapi_string.h"
#else
#include <string.h>
#endif


#include "classicladder.h"
#include "global.h"
#ifdef GTK_INTERFACE
#include "edit.h"
#endif
#include "manager.h"

void InitSections( void )
{
	StrSection * pSection;
	int NumSec;
	for ( NumSec=0; NumSec<NBR_SECTIONS; NumSec++ )
	{
		pSection = &SectionArray[ NumSec ];
		pSection->Used = FALSE;
		rtapi_strxcpy( pSection->Name, "" );
		pSection->Language = SECTION_IN_LADDER;
		pSection->SubRoutineNumber = -1;
		pSection->FirstRung = 0;
		pSection->LastRung = 0;
		pSection->SequentialPage = 0;
	}

	// We directly create one section in ladder...
	// at least for compatibility with our old progs !
	// The rung associated is defined in InitRungs() of calc.c
	pSection = &SectionArray[ 0 ];
	pSection->Used = TRUE;
	rtapi_strxcpy( pSection->Name, "Prog1" );
	pSection->Language = SECTION_IN_LADDER;
	pSection->SubRoutineNumber = -1;
	pSection->FirstRung = 0;
	pSection->LastRung = 0;
	pSection->SequentialPage = 0;
}

int SearchSubRoutineWithItsNumber( int SubRoutineNbrToFind )
{
	StrSection * pSection;
	int NumSec;
	int Found = FALSE;
	NumSec = 0;
	do
	{
		pSection = &SectionArray[ NumSec ];
		if ( pSection->Used )
		{
			if ( pSection->SubRoutineNumber==SubRoutineNbrToFind )
				Found = TRUE;
			else
				NumSec++;
		}
		else
		{
			NumSec++;
		}
	}
	while( NumSec<NBR_SECTIONS && !Found );
	if ( Found )
		return NumSec;
	else
		return -1;
}


/* All the following stuff is not used for real-time working and embedded version... */
#if !defined( __RTL__ ) && defined( GTK_INTERFACE )
int SearchSectionWithName( char * SectionNameToFind )
{
	StrSection * pSection;
	int NumSec;
	int Found = FALSE;
	NumSec = 0;
	do
	{
		pSection = &SectionArray[ NumSec ];
		if ( pSection->Used )
		{
			if ( strcmp( pSection->Name, SectionNameToFind )==0 )
				Found = TRUE;
			else
				NumSec++;
		}
		else
		{
			NumSec++;
		}
	}
	while( NumSec<NBR_SECTIONS && !Found );
	if ( Found )
		return NumSec;
	else
		return -1;
}

void SetSectionSelected( int NumSec /*char * SectionName*/ )
{
	StrSection * pSection;
//	int NumSec = SearchSectionWithName( SectionName );
//	if ( NumSec>=0 )
	{
		pSection = &SectionArray[ NumSec ];
		InfosGene->FirstRung = pSection->FirstRung;
		InfosGene->LastRung = pSection->LastRung;
		InfosGene->CurrentRung = InfosGene->FirstRung;

		InfosGene->CurrentSection = NumSec;
		
// added in v0.9.7
		InfosGene->TopRungDisplayed = InfosGene->FirstRung;
	}
}

int AddSection( char * NewSectionName, int TypeLanguageSection, int SubRoutineNbr )
{
	StrSection * pSection;
	int NumSec;
	int FreeFound = FALSE;
	// Search a free available section
	NumSec = 0;
	do
	{
		pSection = &SectionArray[ NumSec ];
		if ( !pSection->Used )
			FreeFound = TRUE;
		else
			NumSec++;
	}
	while( NumSec<NBR_SECTIONS && !FreeFound );
	if ( FreeFound )
	{
		int NumFreeRung;
		rtapi_strxcpy( pSection->Name, NewSectionName );
		pSection->Language = TypeLanguageSection;
		pSection->FirstRung = 0;
		pSection->LastRung = 0;
		pSection->SequentialPage = 0;
		if ( TypeLanguageSection==SECTION_IN_LADDER )
		{
			pSection->SubRoutineNumber = SubRoutineNbr;
			NumFreeRung = FindFreeRung( );
			if ( NumFreeRung>=0 )
			{
				pSection->Used = TRUE;
				pSection->FirstRung = NumFreeRung;
				pSection->LastRung = NumFreeRung;
				InitBufferRungEdited( &RungArray[ NumFreeRung ] );
			}
			else
			{
				FreeFound = FALSE;
			}
		}
#ifdef SEQUENTIAL_SUPPORT
		if ( TypeLanguageSection==SECTION_IN_SEQUENTIAL )
		{
			int NumFreePage;
			NumFreePage = FindFreeSequentialPage( );
			if ( NumFreePage>=0 )
			{
				pSection->Used = TRUE;
				pSection->SequentialPage = NumFreePage;
			}
			else
			{
				FreeFound = FALSE;
			}
		}
#endif
		InfosGene->AskConfirmationToQuit = TRUE;
		InfosGene->HasBeenModifiedForExitCode = TRUE;
	}
	return FreeFound;
}
void ModifySectionProperties( int NumSec /*char * OriginalSectionName*/, char * NewSectionName )
{
	StrSection * pSection;
//	int NumSec = SearchSectionWithName( OriginalSectionName );
//	if ( NumSec>=0 )
	{
		pSection = &SectionArray[ NumSec ];
		rtapi_strxcpy( pSection->Name, NewSectionName );
	}
}

int NbrSectionsDefined( void )
{
	int NumSec;
	int NbrSectionsDefined = 0;
	for ( NumSec=0; NumSec<NBR_SECTIONS; NumSec++ )
	{
		StrSection * pSection = &SectionArray[ NumSec ];
		if ( pSection->Used )
			NbrSectionsDefined++;
	}
	return NbrSectionsDefined;
}

int VerifyIfSectionNameAlreadyExist( char * Name )
{
	int NumSec;
	if ( Name[0]=='\0' )
		return TRUE;
	NumSec = SearchSectionWithName( Name );
	return( NumSec>=0?TRUE:FALSE );
}

int VerifyIfSubRoutineNumberExist( int SubRoutineNbr )
{
	int NumSec;
	NumSec = SearchSubRoutineWithItsNumber( SubRoutineNbr );
	return( NumSec>=0?TRUE:FALSE );
}

void DelSection( int NumSec /*char * SectionNameToErase*/ )
{
	StrSection * pSection;
//	int NumSec;
//	NumSec = SearchSectionWithName( SectionNameToErase );
//	if ( NumSec>=0 )
	{
		pSection = &SectionArray[ NumSec ];
		pSection->Used = FALSE;
		// free the rungs used in this section
		if ( pSection->Language==SECTION_IN_LADDER )
		{
			int ScanRung = InfosGene->FirstRung;
			while ( ScanRung!=InfosGene->LastRung )
			{
				RungArray[ ScanRung ].Used = FALSE;
				ScanRung = RungArray[ ScanRung ].NextRung;
			}
			RungArray[ InfosGene->LastRung ].Used = FALSE;
		}
		InfosGene->AskConfirmationToQuit = TRUE;
		InfosGene->HasBeenModifiedForExitCode = TRUE;
	}
}

int GetPrevNextSection( int RefSectionNbr, char NextSearch )
{
	int ScanSect = RefSectionNbr;
//printf("Search sect base = %d\n", ScanSect );
	int ScanFound = -1;
	do
	{
		ScanSect = ScanSect + ( NextSearch?1:-1 );
//printf("Search sect %d\n", ScanSect );
		if ( SectionArray[ ScanSect ].Used )
			ScanFound = ScanSect;
	}
	while( ( NextSearch && ScanSect<NBR_SECTIONS-1 ) && ( !NextSearch && ScanSect>0 ) );
	return ScanFound;
}

void SwapSections( int SectionNbr1, int SectionNbr2 /*char * SectionName1, char * SectionName2*/ )
{
	StrSection * pSection1;
	StrSection * pSection2;
	StrSection SectionTemp;
	StrSection * pSectionTemp = &SectionTemp;
//	int NumSec1;
//	int NumSec2;
//	NumSec1 = SearchSectionWithName( SectionName1 );
//	NumSec2 = SearchSectionWithName( SectionName2 );
//	if ( NumSec1>=0 && NumSec2>=0 )
	{
		pSection1 = &SectionArray[ SectionNbr1 /*NumSec1*/ ];
		pSection2 = &SectionArray[ SectionNbr2 /*NumSec2*/ ];

		memcpy( pSectionTemp, pSection1, sizeof(StrSection) );
		memcpy( pSection1, pSection2, sizeof(StrSection) );
		memcpy( pSection2, pSectionTemp, sizeof(StrSection) );
		
		InfosGene->AskConfirmationToQuit = TRUE;
		InfosGene->HasBeenModifiedForExitCode = TRUE;
	}
}

#ifdef SEQUENTIAL_SUPPORT
int FindFreeSequentialPage( void )
{
	int FreePage = -1;
	// to mark the pages used
	char PageUsed[ NBR_SEQUENTIAL_PAGES ];
	int ScanPage;
	int ScanSection;
	StrSection * pSection;
	for ( ScanPage=0; ScanPage<NBR_SEQUENTIAL_PAGES; ScanPage++ )
		PageUsed[ ScanPage ] = FALSE;
	// scan each section and mark the pages used
	for ( ScanSection=0; ScanSection<NBR_SECTIONS; ScanSection++ )
	{
		pSection = &SectionArray[ ScanSection ];
		if ( pSection->Used )
		{
			if ( pSection->Language==SECTION_IN_SEQUENTIAL )
				PageUsed[ pSection->SequentialPage ] = TRUE;
		}
	}
	// find the first free page
	ScanPage = 0;
	do
	{
		if ( !PageUsed[ ScanPage ] )
			FreePage = ScanPage;
		ScanPage++;
	}
	while( ScanPage<NBR_SEQUENTIAL_PAGES && FreePage==-1 );
	return FreePage;
}
#endif

#endif

