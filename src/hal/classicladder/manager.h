int SearchSubRoutineWithItsNumber( int SubRoutineNbrToFind );
#ifndef MODULE
void InitSections( void );
void SectionSelected( char * SectionName );
int AddSection( char * NewSectionName, int TypeLangageSection, int SubRoutineNbr );
int NbrSectionsDefined( void );
int VerifyIfSectionNameAlreadyExist( char * Name );
int VerifyIfSubRoutineNumberExist( int SubRoutineNbr );
void DelSection( char * SectionNameToErase );
void SwapSections( char * SectionName1, char * SectionName2 );
int FindFreeSequentialPage( void );
#endif
