//    Copyright 2005-2008, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
void InitSections( void );
int SearchSubRoutineWithItsNumber( int SubRoutineNbrToFind );
int SearchSectionWithName( char * SectionNameToFind );
void SetSectionSelected( int NumSec /*char * SectionName*/ );
int AddSection( char * NewSectionName, int TypeLangageSection, int SubRoutineNbr );
void ModifySectionProperties( int NumSec /*char * OriginalSectionName*/, char * NewSectionName );
int NbrSectionsDefined( void );
int VerifyIfSectionNameAlreadyExist( char * Name );
int VerifyIfSubRoutineNumberExist( int SubRoutineNbr );
void DelSection( int NumSec /*char * SectionNameToErase*/ );
int GetPrevNextSection( int RefSectionNbr, char NextSearch );
void SwapSections( int SectionNbr1, int SectionNbr2 /*char * SectionName1, char * SectionName2*/ );
int FindFreeSequentialPage( void );

