/* Classic Ladder Project */
/* Copyright (C) 2001-2009 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */
/* ------------------------ */
/* Load/Save projects files */
/* ------------------------ */
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
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
// for mkdir( ) Linux
#if !defined(__WIN32__)
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "classicladder.h"
#include "global.h"
#include "edit.h"
#include "calc.h"
#include "calc_sequential.h"
#include "files.h"
#include "files_sequential.h"
#include "files_project.h"
#include <rtapi_string.h>


//#ifdef GTK_INTERFACE
//char CurrentProjectFileName[400] = "../src/hal/classicladder/projects_examples/example.clp";
//#else
//char CurrentProjectFileName[400] = "projects_examples/parallel_port_direct.clp";
//#endif


#ifdef __WIN32__
#define CAR_SEP '\\'
#else
#define CAR_SEP '/'
#endif
void VerifyDirectorySelected( char * NewDir )
{
	rtapi_strxcpy( InfosGene->CurrentProjectFileName, NewDir );
	if (strlen(InfosGene->CurrentProjectFileName)>1)
	{
		if ( strcmp( &NewDir[ strlen( NewDir ) -4 ], ".clp" )!=0 )
		{
			// verify if path given is really a directory (not a file in it)
			DIR *pDir;
			pDir = opendir(InfosGene->CurrentProjectFileName);
			if (pDir==NULL && errno==ENOTDIR)
			{
				int Lgt = strlen(InfosGene->CurrentProjectFileName);
				char * End = &InfosGene->CurrentProjectFileName[Lgt-1];
				do
				{
					End--;
				}
				while(*End!=CAR_SEP && --Lgt>0);
				End++;
				if ( Lgt>0 )
				{
					*End = '\0';
				}
				else
				{
					printf(_("ERROR with path directory given for project !!!\n"));
					InfosGene->CurrentProjectFileName[ 0 ] = '\0';
				}
			}
			else
			{
				if (pDir != NULL)
				{
					closedir(pDir);
				}
				if (InfosGene->CurrentProjectFileName[strlen(InfosGene->CurrentProjectFileName)-1]!=CAR_SEP)
					rtapi_strxcat( InfosGene->CurrentProjectFileName, "/" );
			}
			//printf("DIRECTORY PROJECT = %s\n",CurrentProjectFileName);
		}
	}
}



void InitTempDir( void )
{
	char * TmpEnv = getenv("TMP");
	if ( TmpEnv==NULL )
		TmpEnv = "/tmp";

	// get a single name directory
	snprintf(TmpDirectory, sizeof(TmpDirectory), "%s/classicladder_tmp_XXXXXX", TmpEnv );
#ifndef __WIN32__
	if ( mkdtemp( TmpDirectory )==NULL )
#else
	if ( mktemp( TmpDirectory )==NULL )
#endif
	{
		snprintf(TmpDirectory, sizeof(TmpDirectory), "%s/classicladder_tmp", TmpEnv );
#ifndef __WIN32__
		mkdir( TmpDirectory, S_IRWXU );
#else
		mkdir( TmpDirectory );
#endif
	}
#ifdef __WIN32__
	else
	{
		mkdir( TmpDirectory );
	}
#endif
printf("Init tmp dir=%s\n", TmpDirectory);
}

char LoadProjectFiles( char * FileProject )
{
	char Result = FALSE;
	char OldProjectFound = TRUE;
	if ( TmpDirectory[ 0 ]=='\0' )
		InitTempDir( );
	CleanTmpLadderDirectory( FALSE/*DestroyDir*/ );
	/* if it is an old project, read directly from the directory selected... */
	if ( strcmp( &FileProject[ strlen( FileProject ) -4 ], ".clp" )==0 )
		OldProjectFound = FALSE;
	if ( OldProjectFound )
	{
		//printf("Loading an old project (many files in a directory) !\n");
		LoadAllLadderDatas( FileProject );
	}
	else
	{
		// split files of the project in the temp directory
		Result = SplitFiles( FileProject, TmpDirectory );
		//printf("Load project '%s' in tmp dir=%s\n", FileProject, TmpDirectory);
		LoadAllLadderDatas( TmpDirectory );
	}
	return Result;
}

static char FileName[500];
char LoadGeneralParamsOnlyFromProject( char * FileProject )
{
	char Result = FALSE;
	if ( TmpDirectory[ 0 ]=='\0' )
		InitTempDir( );
	CleanTmpLadderDirectory( FALSE/*DestroyDir*/ );
	if ( FileProject[0]!='\0' )
	{
		if ( strcmp( &FileProject[ strlen( FileProject ) -4 ], ".clp" )==0 )
		{
			// split files of the project in the temp directory
			Result = SplitFiles( FileProject, TmpDirectory );
			snprintf(FileName, sizeof(FileName),"%s/general.txt",TmpDirectory);
			LoadGeneralParameters( FileName );
		}
	}
	return Result;
}

char SaveProjectFiles( char * FileProject )
{
	if ( TmpDirectory[ 0 ]=='\0' )
		InitTempDir( );
printf(_("Save project '%s' from tmp dir=%s\n"), FileProject, TmpDirectory);
	SaveAllLadderDatas( TmpDirectory );
	if ( strcmp( &FileProject[ strlen( FileProject ) -4 ], ".clp" )!=0 )
		strcat( FileProject, ".clp" );
	// join files for the project in one file
	return JoinFiles( FileProject, TmpDirectory );
}


#define FILE_HEAD "_FILE-"
#define STR_LEN_FILE_HEAD strlen(FILE_HEAD)
// Join many parameters files in a project file
char JoinFiles( char * DirAndNameOfProject, char * TmpDirectoryFiles )
{
	char ProjectFileOk = FALSE;
	FILE * pProjectFile;
	char Buff[300];
	char BuffTemp[300];
	DIR *pDir;
	struct dirent *pEnt;

	pProjectFile = fopen( DirAndNameOfProject, "wt" );
	if ( pProjectFile )
	{

		/* start line of project */
		fputs( "_FILES_CLASSICLADDER\n", pProjectFile );

		/* read directory of the parameters files */
		pDir = opendir( TmpDirectoryFiles );
		if (pDir)
		{
			while ((pEnt = readdir(pDir)) != NULL)
			{
				if ( strcmp(pEnt->d_name,".") && strcmp(pEnt->d_name,"..") )
				{
					FILE * pParametersFile;
////WIN32PORT added /
					snprintf(Buff, sizeof(Buff), "%s/%s", TmpDirectoryFiles,pEnt->d_name);
					pParametersFile = fopen( Buff, "rt" );
					if (pParametersFile)
					{
						snprintf(BuffTemp, sizeof(BuffTemp), FILE_HEAD "%s\n", pEnt->d_name );
						fputs( BuffTemp, pProjectFile );
						while( !feof( pParametersFile ) )
						{
							char Buff[ 300 ];
							fgets( Buff, 300, pParametersFile );
							if (!feof(pParametersFile))
							{
								fputs( Buff, pProjectFile );
							}
						}
						fclose( pParametersFile );
						snprintf(BuffTemp, sizeof(BuffTemp), "_/FILE-%s\n", pEnt->d_name );
						fputs( BuffTemp, pProjectFile );
					}
				}
			}
			closedir(pDir);

		}

		/* end line of project */
		fputs( "_/FILES_CLASSICLADDER\n", pProjectFile );
		fclose(pProjectFile);

		ProjectFileOk = TRUE;
	}

	return ProjectFileOk;
}

// Split a project file in many parameters files
char SplitFiles( char * DirAndNameOfProject, char * TmpDirectoryFiles )
{
	char ProjectFileOk = TRUE;
	char Buff[ 300 ];
	FILE * pProjectFile;
	FILE * pParametersFile;
	char ParametersFile[300];
	rtapi_strxcpy(ParametersFile,"");

	pProjectFile = fopen( DirAndNameOfProject, "rb" );
	if ( pProjectFile )
	{

		/* start line of project ?*/
		fgets( Buff, 300, pProjectFile );
		if ( strncmp( Buff, "_FILES_CLASSICLADDER", strlen( "_FILES_CLASSICLADDER" ) )==0 )
		{

			while( !feof( pProjectFile ) )
			{
				fgets( Buff, 300, pProjectFile );
				if ( !feof( pProjectFile ) )
				{
					// header line for a file parameter ?
					if (strncmp(Buff,FILE_HEAD,STR_LEN_FILE_HEAD) ==0)
					{
////WIN32PORT added /
						size_t ret = snprintf(ParametersFile, sizeof(ParametersFile), "%s/%s", TmpDirectoryFiles, &Buff[STR_LEN_FILE_HEAD]);
						if (ret >= sizeof(ParametersFile)) ProjectFileOk = FALSE;
						ParametersFile[ strlen( ParametersFile )-1 ] = '\0';
//WIN32PORT
if ( ParametersFile[ strlen(ParametersFile)-1 ]=='\r' )
ParametersFile[ strlen(ParametersFile)-1 ] = '\0';
					}
					else
					{
						/* not end line of project ? */
						if ( ( strncmp( Buff, "_/FILES_CLASSICLADDER", strlen("_/FILES_CLASSICLADDER") )!=0 )
								&& Buff[ 0 ]!='\n' )
						{
							char cEndOfFile = FALSE;
							/* file parameter */

							pParametersFile = fopen( ParametersFile, "wt" );
							if (pParametersFile)
							{
								fputs( Buff, pParametersFile );
								while( !feof( pProjectFile ) && !cEndOfFile )
								{
									fgets( Buff, 300, pProjectFile );
									if (strncmp(Buff,"_/FILE-",strlen("_/FILE-")) !=0)
									{
										if (!feof(pProjectFile))
											fputs( Buff, pParametersFile );
									}
									else
									{
										cEndOfFile = TRUE;
									}
								}
								fclose(pParametersFile);
							}
						}
					}
				}
			}
		}
		else
		{
			ProjectFileOk = FALSE;
		}
		fclose(pProjectFile);
	}
	else
	{
		ProjectFileOk = FALSE;
	}
	return ProjectFileOk;
}
