/* Classic Ladder Project */
/* Copyright (C) 2001 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* February 2001 */
/* Last update : 17 August 2002 */
/* ------------------------------------------------------------------------------------- */
/* Load/Save Rungs , Timers , Monostables , Arithmetic expressions & Sections parameters */
/* ------------------------------------------------------------------------------------- */
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
// for mkdir( ) Linux
#include <sys/stat.h>
#include <sys/types.h>

#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "manager.h"
#include "calc.h"
#include "edit.h"
#include "files.h"

#ifdef debug
#define dbg_printf printf
#else
static inline int dbg_printf(char *f, ...) {return 0;}
#endif

char TmpDirectory[ 400 ];

StrDatasForBase CorresDatasForBase[3] = { {BASE_MINS , TIME_BASE_MINS , "%.1fmn" , "Mins" } ,
                                   {BASE_SECS , TIME_BASE_SECS , "%.1fs" , "Secs" } ,
                                   {BASE_100MS , TIME_BASE_100MS , "%.0f00ms" , "100msecs" } };


char ConvRawLineOfElements(char * RawLine,int y,StrRung * StorageRung)
{
    char * StartOfValue;
    char * EndOfValue;
    int x = 0;

    char EndOfLine;

    StartOfValue = RawLine;
    EndOfValue = RawLine;

    do
    {
        /* Extract Element Type */
        StartOfValue = EndOfValue;
        do
        {
            EndOfValue++;
        }
        while(*EndOfValue!='-');
        *EndOfValue++ = '\0';
        StorageRung->Element[x][y].Type = atoi(StartOfValue);

        /* Extract ConnectedWithTop */
        StartOfValue = EndOfValue;
        do
        {
            EndOfValue++;
        }
        while(*EndOfValue!='-');
        *EndOfValue++ = '\0';
        StorageRung->Element[x][y].ConnectedWithTop = atoi(StartOfValue);

        /* Extract Var Type */
        StartOfValue = EndOfValue;
        do
        {
            EndOfValue++;
        }
        while(*EndOfValue!='/');
        *EndOfValue++ = '\0';
        StorageRung->Element[x][y].VarType = atoi(StartOfValue);

        /* Extract Var Offset in the type table */
        StartOfValue = EndOfValue;
        do
        {
            EndOfValue++;
        }
        while( (*EndOfValue!=',') && (*EndOfValue!=10) );
        EndOfLine = TRUE;
        if (*EndOfValue==',')
            EndOfLine = FALSE;
        *EndOfValue++ = '\0';
        StorageRung->Element[x][y].VarNum = atoi(StartOfValue);

        /* Next Element */
        x++;

    }
    while(!EndOfLine);
    return (x);
}

char LoadRung(char * FileName,StrRung * BufRung)
{
    FILE * File;
    char Okay = FALSE;
    char Line[300];
    char * LineOk;
    int y = 0;
    File = fopen(FileName,"rt");
    if (File)
    {
        do
        {
            LineOk = fgets(Line,300,File);
            if (LineOk)
            {
                char FndElements;
                switch(Line[0])
                {
                    case ';':
                        break;
                    case '#':
                        if(strncmp(&Line[1],"VER=",4)==0)
                        {
                            if (atoi(&Line[5])>2)
                            {
                                printf("Rung version not supported...\n");
                                LineOk = FALSE;
                            }
                        }
                        if(strncmp(&Line[1],"LABEL=",6)==0)
                        {
                            strcpy(BufRung->Label,&Line[7]);
                            if (strlen( BufRung->Label )>0 && BufRung->Label[strlen(BufRung->Label)-1]=='\n')
                                BufRung->Label[strlen(BufRung->Label)-1]='\0';
                            if (strlen( BufRung->Label )>0 && BufRung->Label[strlen(BufRung->Label)-1]=='\r')
                                BufRung->Label[strlen(BufRung->Label)-1]='\0';
                            if (strlen( BufRung->Label )>0 && BufRung->Label[strlen(BufRung->Label)-1]=='\r')
                                BufRung->Label[strlen(BufRung->Label)-1]='\0';
                        }
                        if(strncmp(&Line[1],"COMMENT=",8)==0)
                        {
                            strcpy(BufRung->Comment,&Line[9]);
                            if (strlen( BufRung->Comment )>0 && BufRung->Comment[strlen(BufRung->Comment)-1]=='\n')
                                BufRung->Comment[strlen(BufRung->Comment)-1]='\0';
                            if (strlen( BufRung->Comment )>0 && BufRung->Comment[strlen(BufRung->Comment)-1]=='\r')
                                BufRung->Comment[strlen(BufRung->Comment)-1]='\0';
                            if (strlen( BufRung->Comment )>0 && BufRung->Comment[strlen(BufRung->Comment)-1]=='\r')
                                BufRung->Comment[strlen(BufRung->Comment)-1]='\0';
                        }
                        if(strncmp(&Line[1],"PREVRUNG=",9)==0)
                            BufRung->PrevRung = atoi( &Line[10] );
                        if(strncmp(&Line[1],"NEXTRUNG=",9)==0)
                            BufRung->NextRung = atoi( &Line[10] );
                        break;
                    default:
                    FndElements = ConvRawLineOfElements(Line,y,BufRung);
                    y++;
                }
            }
        }
        while(LineOk);
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}
char SaveRung(char * FileName,StrRung * BufRung)
{
    FILE * File;
    char Okay = FALSE;
    int x,y;
    File = fopen(FileName,"wt");
    if (File)
    {
        fprintf(File,"; Rung :\n");
        fprintf(File,"; all the blocks with the following format :\n");
        fprintf(File,"; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset\n");
        fprintf(File,"#VER=2.0\n");
        fprintf(File,"#LABEL=%s\n",BufRung->Label);
        fprintf(File,"#COMMENT=%s\n",BufRung->Comment);
        fprintf(File,"#PREVRUNG=%d\n",BufRung->PrevRung);
        fprintf(File,"#NEXTRUNG=%d\n",BufRung->NextRung);
        for (y=0;y<RUNG_HEIGHT;y++)
        {
            for(x=0;x<RUNG_WIDTH;x++)
            {
                fprintf(File,"%d-%d-%d/%d",BufRung->Element[x][y].Type, BufRung->Element[x][y].ConnectedWithTop ,
                                    BufRung->Element[x][y].VarType , BufRung->Element[x][y].VarNum);
                if (x<RUNG_WIDTH-1)
                    fprintf(File," , ");
            }
            fprintf(File,"\n");
        }
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

void LoadAllRungs_V1(char * BaseName,StrRung * Rungs,int * TheFirst,int * TheLast,int * TheCurrent)
{
    int NumRung;
    StrRung * PrevRung = NULL;
    int PrevNumRung = 0;
    *TheFirst = -1;
    *TheCurrent = -1;
    for(NumRung=0; NumRung<NBR_RUNGS; NumRung++)
    {
        char RungFile[400];
        sprintf(RungFile,"%s%d.csv",BaseName,NumRung);
        dbg_printf("Loading file : %s",RungFile);
        if (LoadRung(RungFile,Rungs))
        {
            if (*TheFirst==-1)
            {
                *TheFirst = NumRung;
                *TheCurrent = NumRung;
            }
            if (PrevRung)
            {
                PrevRung->NextRung = NumRung;
            }
            Rungs->Used = TRUE;
            Rungs->PrevRung = PrevNumRung;
            *TheLast = NumRung;
            PrevNumRung = NumRung;
            PrevRung = Rungs;
            dbg_printf(" - ok.\n");
        }
        else
            dbg_printf(" - not found.\n");
        //DumpRung(Rungs);
        Rungs++;
    }
    /* no rungs loaded ? */
    /* we must keep at least one empty ! */
    if (*TheFirst<0)
    {
        *TheFirst = 0;
        *TheCurrent = 0;
    }
}
void LoadAllRungs(char * BaseName,StrRung * Rungs)
{
    int NumRung;
    for(NumRung=0; NumRung<NBR_RUNGS; NumRung++)
    {
        char RungFile[400];
        sprintf(RungFile,"%s%d.csv",BaseName,NumRung);
        dbg_printf("Loading file : %s",RungFile);
        if (LoadRung(RungFile,Rungs))
        {
            Rungs->Used = TRUE;
            dbg_printf(" - ok.\n");
        }
        else
            dbg_printf(" - not found.\n");
        //DumpRung(Rungs);
        Rungs++;
    }
}

void SaveAllRungs(char * BaseName)
{
    int NumRung;
    char RungFile[400];
    /* delete all before */
    for(NumRung=0;NumRung<NBR_RUNGS;NumRung++)
    {
        sprintf(RungFile,"%s%d.csv",BaseName,NumRung);
        unlink(RungFile);
    }
    /* save rungs (only defined ones are saved) */
    /* Since v0.5.5, the number of the rungs do not change : */
    /* it's easier, and with the sections added it's indispensable */
    for(NumRung=0;NumRung<NBR_RUNGS;NumRung++)
    {
        if ( RungArray[ NumRung ].Used )
        {
            sprintf(RungFile,"%s%d.csv",BaseName,NumRung);
            dbg_printf("Saving file : %s",RungFile);
            if (SaveRung(RungFile,&RungArray[NumRung]))
                dbg_printf(" - ok.\n");
            else
                dbg_printf(" - failed.\n");
        }
    }
}

void DumpRung(StrRung * TheRung)
{
    int x,y;
    printf("Used=%d\n",TheRung->Used);
    for (y=0;y<RUNG_HEIGHT;y++)
    {
        for(x=0;x<RUNG_WIDTH;x++)
        {
            printf("%d:%d:%d=%d , ",TheRung->Element[x][y].Type,TheRung->Element[x][y].ConnectedWithTop,TheRung->Element[x][y].VarNum,TheRung->Element[x][y].DynamicOutput);
        }
        printf("\n");
    }
}

static void LoadAllLadderDatas(char * DatasDirectory)
{
    char FileName[500];
    InitAllLadderDatas();

    sprintf(FileName,"%s/timers.csv",DatasDirectory);
    printf("Loading timers datas from %s\n",FileName);
    LoadTimersParams(FileName,TimerArray);
    sprintf(FileName,"%s/monostables.csv",DatasDirectory);
    printf("Loading monostables datas from %s\n",FileName);
    LoadMonostablesParams(FileName,MonostableArray);
    PrepareTimers();
    PrepareMonostables();

    sprintf(FileName,"%s/arithmetic_expressions.csv",DatasDirectory);
    printf("Loading arithmetic expressions from %s\n",FileName);
    LoadArithmeticExpr(FileName);

    // Sections added since v0.5.5, the format of files has a little changed :
    // before the prev/next rungs were not saved in each rung...
    // and the nmber of rungs changed when saved...
    sprintf(FileName,"%s/sections.csv",DatasDirectory);
    printf("Loading sections datas from %s\n",FileName);
    if ( LoadSectionsParams(FileName) )
    {
        sprintf(FileName,"%s/rung_",DatasDirectory);
        LoadAllRungs(FileName,RungArray);
    }
    else
    {
        printf("Rungs with old format found (no sections)\n");
        sprintf(FileName,"%s/rung_",DatasDirectory);
        LoadAllRungs_V1(FileName,RungArray,&InfosGene->FirstRung,&InfosGene->LastRung,&InfosGene->CurrentRung);
        // if we load old format files, sections wasn't created, so we must write theses infos...
        SectionArray[ 0 ].FirstRung = InfosGene->FirstRung;
        SectionArray[ 0 ].LastRung = InfosGene->LastRung;
    }
#ifdef SEQUENTIAL_SUPPORT
    sprintf(FileName,"%s/sequential.csv",DatasDirectory);
    printf("Loading sequential datas from %s\n",FileName);
    LoadSequential( FileName );
#endif

    PrepareRungs();
#ifdef SEQUENTIAL_SUPPORT
    PrepareSequential();
#endif
}

static void SaveAllLadderDatas(char * DatasDirectory)
{
    char FileName[500];
    sprintf(FileName,"%s/timers.csv",DatasDirectory);
    SaveTimersParams(FileName,TimerArray);
    sprintf(FileName,"%s/monostables.csv",DatasDirectory);
    SaveMonostablesParams(FileName,MonostableArray);
    sprintf(FileName,"%s/arithmetic_expressions.csv",DatasDirectory);
    SaveArithmeticExpr(FileName);
    sprintf(FileName,"%s/rung_",DatasDirectory);
    SaveAllRungs(FileName);
    sprintf(FileName,"%s/sections.csv",DatasDirectory);
    SaveSectionsParams( FileName );
#ifdef SEQUENTIAL_SUPPORT
    sprintf(FileName,"%s/sequential.csv",DatasDirectory);
    SaveSequential( FileName );
#endif
}


void ConvRawLineOfNumbers(char * RawLine,char NbrParams,int * ValuesFnd)
{
    char * StartOfValue;
    char * EndOfValue;
    char Num = 0;

    char EndOfLine;

    StartOfValue = RawLine;
    EndOfValue = RawLine;
    EndOfLine = FALSE;

    do
    {
        /* Extract Value */
        StartOfValue = EndOfValue;
        do
        {
            EndOfValue++;
        }
        while( (*EndOfValue!=',') && (*EndOfValue!=10) );
        if (*EndOfValue==10)
            EndOfLine = TRUE;
            *EndOfValue++ = '\0';
            *ValuesFnd++ = atoi(StartOfValue);
            Num++;
            StartOfValue = EndOfValue;
    }
    while( (!EndOfLine) && (Num<NbrParams) );
}

int ConvBaseInMilliSecsToId(int NbrMilliSecs)
{
    switch(NbrMilliSecs)
    {
        case TIME_BASE_MINS:
            return BASE_MINS;
        case TIME_BASE_SECS:
            return BASE_SECS;
        case TIME_BASE_100MS:
            return BASE_100MS;
        default:
            printf("!!!Error in ConvBaseInMilliSecsToInt()\n");
            return BASE_SECS;
    }
}

char LoadTimersParams(char * FileName,StrTimer * BufTimers)
{
    FILE * File;
    char Okay = FALSE;
    char Line[300];
    char * LineOk;
    int Params[3];
    File = fopen(FileName,"rt");
    if (File)
    {
        do
        {
            LineOk = fgets(Line,300,File);
            if (LineOk)
            {
                if (Line[0]!=';')
                {
                    ConvRawLineOfNumbers(Line,2,Params);
                    switch(Params[0])
                    {
                        case BASE_MINS:
                        case BASE_SECS:
                        case BASE_100MS:
                            BufTimers->Base = CorresDatasForBase[Params[0]].ValueInMS;
                            BufTimers->Preset = Params[1] * BufTimers->Base;
                            strcpy(BufTimers->DisplayFormat,CorresDatasForBase[Params[0]].DisplayFormat);
                            break;
                        default:
                            BufTimers->Base = 1;
                            BufTimers->Preset = 10;
                            strcpy(BufTimers->DisplayFormat,"%f?");
                            printf("!!! Error loading parameter base in %s\n",FileName);
                            break;
                    }
dbg_printf("Timer => Base = %d , Preset = %d\n",BufTimers->Base,BufTimers->Preset);
                    BufTimers++;
                }
            }
        }
        while(LineOk);
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

char SaveTimersParams(char * FileName,StrTimer * BufTimers)
{
    FILE * File;
    char Okay = FALSE;
    int NumTimer = 0;
printf( "Saving timer params file=%s", FileName );
    File = fopen(FileName,"wt");
    if (File)
    {
        fprintf(File,"; Timers :\n");
        fprintf(File,"; Base(see classicladder.h),Preset\n");
        do
        {
            fprintf(File,"%d,%d\n",ConvBaseInMilliSecsToId(BufTimers->Base),BufTimers->Preset/BufTimers->Base);
            BufTimers++;
            NumTimer++;
        }
        while(NumTimer<NBR_TIMERS);
        fclose(File);
        Okay = TRUE;
    }
printf( " - result=%d\n", Okay );
    return (Okay);
}

char LoadMonostablesParams(char * FileName,StrMonostable * BufMonostables)
{
    FILE * File;
    char Okay = FALSE;
    char Line[300];
    char * LineOk;
    int Params[3];
    File = fopen(FileName,"rt");
    if (File)
    {
        do
        {
            LineOk = fgets(Line,300,File);
            if (LineOk)
            {
                if (Line[0]!=';')
                {
                    ConvRawLineOfNumbers(Line,2,Params);
                    switch(Params[0])
                    {
                        case BASE_MINS:
                        case BASE_SECS:
                        case BASE_100MS:
                            BufMonostables->Base = CorresDatasForBase[Params[0]].ValueInMS;
                            BufMonostables->Preset = Params[1] * BufMonostables->Base;
                            strcpy(BufMonostables->DisplayFormat,CorresDatasForBase[Params[0]].DisplayFormat);
                            break;
                        default:
                            BufMonostables->Base = 1;
                            BufMonostables->Preset = 10;
                            strcpy(BufMonostables->DisplayFormat,"%f?");
                            printf("!!! Error loading parameter base in %s\n",FileName);
                            break;
                    }
dbg_printf("Monostable => Base = %d , Preset = %d\n",BufMonostables->Base,BufMonostables->Preset);
                    BufMonostables++;
                }
            }
        }
        while(LineOk);
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

char SaveMonostablesParams(char * FileName,StrMonostable * BufMonostables)
{
    FILE * File;
    char Okay = FALSE;
    int NumMonostable = 0;
    File = fopen(FileName,"wt");
    if (File)
    {
        fprintf(File,"; Monostables :\n");
        fprintf(File,"; Base(see classicladder.h),Preset\n");
        do
        {
            fprintf(File,"%d,%d\n",ConvBaseInMilliSecsToId(BufMonostables->Base),BufMonostables->Preset/BufMonostables->Base);
            BufMonostables++;
            NumMonostable++;
        }
        while(NumMonostable<NBR_MONOSTABLES);
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

char LoadArithmeticExpr(char * FileName)
{
    FILE * File;
    char Okay = FALSE;
    char Line[300];
    char * LineOk;
    int NumExpr = 0;
    File = fopen(FileName,"rt");
    if (File)
    {
        do
        {
            LineOk = fgets(Line,300,File);
            if (LineOk)
            {
                if (Line[0]!=';')
                {
                    if (strlen( Line )>0 && Line[strlen(Line)-1]=='\n')
                        Line[strlen(Line)-1]='\0';
                    if (strlen( Line )>0 && Line[strlen(Line)-1]=='\r')
                        Line[strlen(Line)-1]='\0';
                    if (strlen( Line )>0 && Line[strlen(Line)-1]=='\r')
                        Line[strlen(Line)-1]='\0';
                    strcpy(ArithmExpr[NumExpr].Expr,Line);
                    NumExpr++;
                }
            }
        }
        while(LineOk);
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

char SaveArithmeticExpr(char * FileName)
{
    FILE * File;
    char Okay = FALSE;
    int NumExpr;
    File = fopen(FileName,"wt");
    if (File)
    {
        fprintf(File,"; Arithmetic expressions :\n");
        fprintf(File,"; Compare or Operate ones\n");
        for(NumExpr=0; NumExpr<NBR_ARITHM_EXPR; NumExpr++)
        {
            fprintf(File,"%s\n",ArithmExpr[NumExpr].Expr);
        }
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

char LoadSectionsParams(char * FileName)
{
    FILE * File;
    char Okay = FALSE;
    char Line[300];
    char * LineOk;
    int NumSection;
    StrSection * pSection;
    int Params[10];
    File = fopen(FileName,"rt");
    if (File)
    {
        do
        {
            LineOk = fgets(Line,300,File);
            if (LineOk)
            {
                switch(Line[0])
                {
                    case ';':
                        break;
                    case '#':
                        if(strncmp(&Line[1],"VER=",4)==0)
                        {
                            if (atoi(&Line[5])>1)
                            {
                                printf("Sections file version not supported...\n");
                                LineOk = FALSE;
                            }
                        }
                        // #NAMExxx=....
                        if(strncmp(&Line[1],"NAME",4)==0)
                        {
                            char * Name;
                            Line[ 8 ] = '\0';
                            NumSection = atoi( &Line[5] );
                            strcpy(SectionArray[ NumSection ].Name, &Line[9]);
                            Name = SectionArray[ NumSection ].Name;
                    		if (strlen( Name )>0 && Name[strlen(Name)-1]=='\n')
                       			Name[strlen(Name)-1]='\0';
                		if (strlen( Name )>0 && Name[strlen(Name)-1]=='\r')
                        		Name[strlen(Name)-1]='\0';
                    		if (strlen( Name )>0 && Name[strlen(Name)-1]=='\r')
                        		Name[strlen(Name)-1]='\0';
                        }
                        break;
                    default:
                        ConvRawLineOfNumbers(Line,6,Params);
                        NumSection = Params[ 0 ];
                        pSection = &SectionArray[ NumSection ];
                        pSection->Used = TRUE;
                        pSection->Language = Params[ 1 ];
                        pSection->SubRoutineNumber = Params[ 2 ];
                        pSection->FirstRung = Params[ 3 ];
                        pSection->LastRung = Params[ 4 ];
                        pSection->SequentialPage = Params[ 5 ];
dbg_printf("Section %d => Name=%s, Language=%d, SRnbr=%d, FirstRung=%d, LastRung=%d, SequentialPage=%d\n", NumSection,
pSection->Name, pSection->Language, pSection->SubRoutineNumber, pSection->FirstRung, pSection->LastRung, pSection->SequentialPage);
                        break;
                }
            }
        }
        while(LineOk);
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

char SaveSectionsParams(char * FileName)
{
    FILE * File;
    char Okay = FALSE;
    int NumSection;
    StrSection * pSection;
    File = fopen(FileName,"wt");
    if (File)
    {
        fprintf(File,"; Sections\n");
        fprintf(File,"#VER=1.0\n");
        NumSection = 0;
        do
        {
            pSection = &SectionArray[ NumSection ];
            if ( pSection->Used )
                fprintf(File,"#NAME%03d=%s\n", NumSection, pSection->Name);
            NumSection++;
        }
        while(NumSection<NBR_SECTIONS);
        NumSection = 0;
        do
        {
            pSection = &SectionArray[ NumSection ];
            if ( pSection->Used )
                fprintf(File,"%03d,%d,%d,%d,%d,%d\n", NumSection, pSection->Language, pSection->SubRoutineNumber, pSection->FirstRung, pSection->LastRung, pSection->SequentialPage );
            NumSection++;
        }
        while(NumSection<NBR_SECTIONS);
        fclose(File);
        Okay = TRUE;
    }
    return (Okay);
}

char InitTempDir( void )
{
	// get a temp directory
	char * TmpEnv = getenv("TMP");
	sprintf( TmpDirectory, "%s/classicladder_tmp", TmpEnv!=NULL?TmpEnv:"/tmp" );
	mkdir( TmpDirectory, S_IRWXU );
	return TRUE;
}

char LoadProjectFiles( char * FileProject )
{
	char Result = FALSE; /* FIXME: actually unused... */
	char OldProjectFound = TRUE;
	InitTempDir( );
	printf("Init tmp dir=%s\n", TmpDirectory);
	CleanTmpDirectory( TmpDirectory, FALSE );
	/* if it is an old project, read directly from the directory selected... */
	if ( strcmp( &FileProject[ strlen( FileProject ) -4 ], ".clp" )==0 )
		OldProjectFound = FALSE;
	if ( OldProjectFound )
	{
		printf("Loading an old project (many files in a directory) !\n");
		LoadAllLadderDatas( FileProject );
	}
	else
	{
		// split files of the project in the temp directory
		SplitFiles( FileProject, TmpDirectory );
		printf("Load datas in tmp dir=%s\n", TmpDirectory);
		LoadAllLadderDatas( TmpDirectory );
	}
	return Result;
}

char SaveProjectFiles( char * FileProject )
{
	InitTempDir( );
	CleanTmpDirectory( TmpDirectory, FALSE );
	printf("Save datas in tmp dir=%s\n", TmpDirectory);
	SaveAllLadderDatas( TmpDirectory );
	if ( strcmp( &FileProject[ strlen( FileProject ) -4 ], ".clp" )!=0 )
		strcat( FileProject, ".clp" );
	// join files for the project in one file
	return JoinFiles( FileProject, TmpDirectory );
}

/* clean the tmp directory of the parameters files */
void CleanTmpDirectory( char * Directory, char DestroyDir )
{
	DIR *pDir;
	struct dirent *pEnt;
	char Buff[400];

	if (Directory[0]!='\0')
	{
		pDir = opendir(Directory);
		if (pDir)
		{
			while ((pEnt = readdir(pDir)) != NULL)
			{
				if ( strcmp(pEnt->d_name,".") && strcmp(pEnt->d_name,"..") )
				{
					sprintf(Buff, "%s/%s", Directory,pEnt->d_name);
					remove(Buff);
				}
			}
		}
		closedir(pDir);
		/* delete the temp directory if wanted */
		if ( DestroyDir )
			rmdir(Directory); // _rmdir() for Win32 ?
	}
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
					sprintf(Buff, "%s/%s", TmpDirectoryFiles,pEnt->d_name);
					pParametersFile = fopen( Buff, "rt" );
					if (pParametersFile)
					{
						sprintf( BuffTemp, FILE_HEAD "%s\n", pEnt->d_name );
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
						sprintf( BuffTemp, "_/FILE-%s\n", pEnt->d_name );
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
	strcpy(ParametersFile,"");

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
						sprintf(ParametersFile, "%s/%s", TmpDirectoryFiles, &Buff[STR_LEN_FILE_HEAD]);
						ParametersFile[ strlen( ParametersFile )-1 ] = '\0';
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

void InitAllLadderDatas(void)
{
    InitVars();
    InitTimers();
    InitMonostables();
    InitArithmExpr();
    InitRungs();
    InitSections( );
#ifdef SEQUENTIAL_SUPPORT
    InitSequential( );
#endif
}


#define CAR_SEP '/'

int VerifyDirectorySelected( char *Directory, char * NewDir )
{
	strcpy(Directory,NewDir);
	if (strlen(Directory)>1)
	{
		if ( strcmp( &NewDir[ strlen( NewDir ) -4 ], ".clp" )!=0 )
		{
			// verify if path given is really a directory (not a file in it)
			DIR *pDir;
			pDir = opendir(Directory);
			if (pDir==NULL && errno==ENOTDIR)
			{
				int Lgt = strlen(Directory);
				char * End = &Directory[Lgt-1];
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
					printf("ERROR whith path directory given for project !!!\n");
					Directory[ 0 ] = '\0';
					return(0);
				}
			}
			else
			{
				if (Directory[strlen(Directory)-1]!=CAR_SEP)
					strcat( Directory, "/" );
			}
		}
	}
	printf("DIRECTORY = %s\n",Directory);
	return(1);
}
