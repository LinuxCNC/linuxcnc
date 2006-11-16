/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */
/* -------------- */
/* Refresh a rung */
/* -------------- */
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
#ifdef RTAI
#include "rtai.h"
#include "rtai_sched.h"
#endif
#else
#include <stdio.h>
#include <string.h>
#endif
#ifdef __RTL__
#include <rtlinux_signal.h>
#endif

#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "arithm_eval.h"
#include "manager.h"
#ifdef SEQUENTIAL_SUPPORT
#include "calc_sequential.h"
#endif
#include "calc.h"

void InitRungs()
{
    int NumRung;
    int x,y;
    for (NumRung=0;NumRung<NBR_RUNGS;NumRung++)
    {
        RungArray[NumRung].Used = FALSE;
        strcpy(RungArray[NumRung].Label,"");
        strcpy(RungArray[NumRung].Comment,"");
        for (y=0;y<RUNG_HEIGHT;y++)
        {
            for(x=0;x<RUNG_WIDTH;x++)
            {
                RungArray[NumRung].Element[x][y].Type = ELE_FREE;
                RungArray[NumRung].Element[x][y].ConnectedWithTop = 0;
                RungArray[NumRung].Element[x][y].VarType = 0;
                RungArray[NumRung].Element[x][y].VarNum = 0;
                RungArray[NumRung].Element[x][y].DynamicInput = 0;
                RungArray[NumRung].Element[x][y].DynamicState = 0;
                RungArray[NumRung].Element[x][y].DynamicVarBak = 0;
                RungArray[NumRung].Element[x][y].DynamicOutput = 0;
            }
        }
    }
    InfosGene->FirstRung = 0;
    InfosGene->LastRung = 0;
    InfosGene->CurrentRung = 0;
    RungArray[0].Used = TRUE;
}
/* Set DynamicVarBak (Element) to the right value before calculating the rungs */
/* for detecting rising/falling edges used in some elements */
void PrepareRungs()
{
    int NumRung;
    int x,y;
    char StateElement;
    for (NumRung=0;NumRung<NBR_RUNGS;NumRung++)
    {
        for (y=0;y<RUNG_HEIGHT;y++)
        {
            for(x=0;x<RUNG_WIDTH;x++)
            {
                if ( (RungArray[NumRung].Element[x][y].Type==ELE_RISING_INPUT)
                    || (RungArray[NumRung].Element[x][y].Type==ELE_FALLING_INPUT) )
                {
                    StateElement = ReadVar(RungArray[NumRung].Element[x][y].VarType,RungArray[NumRung].Element[x][y].VarNum);
                    if (RungArray[NumRung].Element[x][y].Type==ELE_FALLING_INPUT)
                        StateElement = !StateElement;
                    RungArray[NumRung].Element[x][y].DynamicVarBak = StateElement;
                }
            }
        }
    }
}
void InitTimers()
{
    int NumTimer;
    for (NumTimer=0; NumTimer<NBR_TIMERS; NumTimer++)
    {
        TimerArray[NumTimer].Preset = 0;
        TimerArray[NumTimer].Base = TIME_BASE_SECS;
        strcpy( TimerArray[NumTimer].DisplayFormat, "%.1fs" );
    }
}
void PrepareTimers()
{
    int NumTimer;
    for (NumTimer=0; NumTimer<NBR_TIMERS; NumTimer++)
    {
        TimerArray[NumTimer].Value = TimerArray[NumTimer].Preset;
        TimerArray[NumTimer].InputEnable = 0;
        TimerArray[NumTimer].OutputDone = 0;
        TimerArray[NumTimer].OutputRunning = 0;
    }
}
void InitMonostables()
{
    int NumMonostable;
    for (NumMonostable=0; NumMonostable<NBR_MONOSTABLES; NumMonostable++)
    {
        MonostableArray[NumMonostable].Preset = 0;
        MonostableArray[NumMonostable].Base = TIME_BASE_SECS;
        strcpy( MonostableArray[NumMonostable].DisplayFormat, "%.1fs" );
    }
}
void PrepareMonostables()
{
    int NumMonostable;
    for (NumMonostable=0; NumMonostable<NBR_MONOSTABLES; NumMonostable++)
    {
        MonostableArray[NumMonostable].Value = 0;
        MonostableArray[NumMonostable].Input = 0;
        MonostableArray[NumMonostable].OutputRunning = 0;
        MonostableArray[NumMonostable].InputBak = 0;
    }
}
void InitCounters()
{
    int NumCounter;
    for (NumCounter=0; NumCounter<NBR_COUNTERS; NumCounter++)
    {
        CounterArray[NumCounter].Preset = 0;
    }
}
void PrepareCounters()
{
    int NumCounter;
    for (NumCounter=0; NumCounter<NBR_COUNTERS; NumCounter++)
    {
        CounterArray[NumCounter].Value = 0;
        CounterArray[NumCounter].ValueBak = 0;
        CounterArray[NumCounter].InputReset = 0;
        CounterArray[NumCounter].InputPreset = 0;
        CounterArray[NumCounter].InputCountUp = 0;
        CounterArray[NumCounter].InputCountUpBak = 0;
        CounterArray[NumCounter].InputCountDown = 0;
        CounterArray[NumCounter].InputCountDownBak = 0;
        CounterArray[NumCounter].OutputDone = 0;
        CounterArray[NumCounter].OutputEmpty = 0;
        CounterArray[NumCounter].OutputFull = 0;
    }
}
void InitArithmExpr()
{
    int NumExpr;
    for (NumExpr=0; NumExpr<NBR_ARITHM_EXPR; NumExpr++)
        strcpy(ArithmExpr[NumExpr].Expr,"");
}
void InitIOConf( )
{
	int NumConf;
	int NbrConf;
	int Pass;
	StrIOConf * pConf;
	for( Pass=0; Pass<2; Pass++)
	{
		NbrConf = (Pass==0)?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF;
		for( NumConf=0; NumConf<NbrConf; NumConf++ )
		{
			pConf = (Pass==0)?&InfosGene->InputsConf[ NumConf ]:&InfosGene->OutputsConf[ NumConf ];
			pConf->FirstClassicLadderIO = -1;
			pConf->DeviceType = 0;
			pConf->SubDevOrAdr = 0;
			pConf->FirstChannel = 0;
			pConf->NbrConsecutivesChannels = 1;
			pConf->FlagInverted = 0;
		}
	}
}

char StateOnLeft(int x,int y,StrRung * TheRung)
{
    char State = 0;
    int PosY;
    char StillConnected;
    // directly connected to the "left"? if yes, ON !
    if (x==0)
        return 1;
    /* Direct on left */
    if (TheRung->Element[x-1][y].DynamicOutput)
        State = 1;
    /* Up */
    PosY = y;
    StillConnected = TheRung->Element[x][PosY].ConnectedWithTop;
    while( (PosY>0) && StillConnected)
    {
        PosY--;
        if (TheRung->Element[x-1][PosY].DynamicOutput)
            State = 1;
        if ( !(TheRung->Element[x][PosY].ConnectedWithTop) )
            StillConnected = FALSE;
    }
    /* Down */
    if (y<RUNG_HEIGHT-1)
    {
        PosY = y+1;
        StillConnected = TheRung->Element[x][PosY].ConnectedWithTop;
        while( (PosY<RUNG_HEIGHT) && StillConnected)
        {
            if (TheRung->Element[x-1][PosY].DynamicOutput)
                State = 1;
            PosY++;
            if (PosY<RUNG_HEIGHT)
            {
                if ( !(TheRung->Element[x][PosY].ConnectedWithTop) )
                    StillConnected = FALSE;
            }
        }
    }
    return State;
}

/* Elements : -| |- and -|/|- */
char CalcTypeInput(int x,int y,StrRung * UpdateRung,char IsNot,char OnlyFronts)
{
    char State;
    char StateElement;
    char StateVar;

    StateElement = ReadVar(UpdateRung->Element[x][y].VarType,UpdateRung->Element[x][y].VarNum);
    if (IsNot)
        StateElement = !StateElement;
    StateVar = StateElement;
    if (OnlyFronts)
    {
        if (StateElement && UpdateRung->Element[x][y].DynamicVarBak)
            StateElement = 0;
    }
    UpdateRung->Element[x][y].DynamicState = StateElement;
    if (x==0)
    {
        State = StateElement;
    }
    else
    {
        UpdateRung->Element[x][y].DynamicInput = StateOnLeft(x,y,UpdateRung);
        State = StateElement && UpdateRung->Element[x][y].DynamicInput;
    }
    UpdateRung->Element[x][y].DynamicOutput = State;
    UpdateRung->Element[x][y].DynamicVarBak = StateVar;
    return State;
}
/* Element : --- */
char CalcTypeConnection(int x,int y,StrRung * UpdateRung)
{
    char State;
    char StateElement;
    StateElement = 1;
    if (x==0)
    {
        State = StateElement;
    }
    else
    {
        UpdateRung->Element[x][y].DynamicInput = StateOnLeft(x,y,UpdateRung);
        State = StateElement && UpdateRung->Element[x][y].DynamicInput;
    }
    UpdateRung->Element[x][y].DynamicState = State;
    UpdateRung->Element[x][y].DynamicOutput = State;
    return State;
}
/* Elements : -( )- and -(/)- */
char CalcTypeOutput(int x,int y,StrRung * UpdateRung,char IsNot)
{
    char State;
    State = StateOnLeft(x,y,UpdateRung);
    UpdateRung->Element[x][y].DynamicInput = State;
    UpdateRung->Element[x][y].DynamicState = State;
    if (IsNot)
        State = !State;
    WriteVar(UpdateRung->Element[x][y].VarType,UpdateRung->Element[x][y].VarNum,State);
    return State;
}
/* Elements : -(S)- and -(R)- */
char CalcTypeOutputSetReset(int x,int y,StrRung * UpdateRung,char IsReset)
{
    char State;
    UpdateRung->Element[x][y].DynamicInput = StateOnLeft(x,y,UpdateRung);
    State = UpdateRung->Element[x][y].DynamicInput;
    UpdateRung->Element[x][y].DynamicState = State;
    if (State)
    {
        if (IsReset)
            State = 0;  /* reset */
        else
            State = 1;  /* set */
        WriteVar(UpdateRung->Element[x][y].VarType,UpdateRung->Element[x][y].VarNum,State);
    }
    return State;
}
/* Element : -(J)- */
int CalcTypeOutputJump(int x,int y,StrRung * UpdateRung)
{
    char State;
    int Goto = -1;
    State = StateOnLeft(x,y,UpdateRung);
    if (State)
        Goto = UpdateRung->Element[x][y].VarNum;
    UpdateRung->Element[x][y].DynamicInput = State;
    UpdateRung->Element[x][y].DynamicState = State;
    return Goto;
}
/* Element : -(C)- */
int CalcTypeOutputCall(int x,int y,StrRung * UpdateRung)
{
    char State;
    int CallSrSection = -1;
    State = StateOnLeft(x,y,UpdateRung);
    if (State)
        CallSrSection = SearchSubRoutineWithItsNumber( UpdateRung->Element[x][y].VarNum );
    UpdateRung->Element[x][y].DynamicInput = State;
    UpdateRung->Element[x][y].DynamicState = State;
    return CallSrSection;
}
/* Element : Timer (2x2 Blocks) */
void CalcTypeTimer(int x,int y,StrRung * UpdateRung)
{
    StrTimer * Timer;
    Timer = &TimerArray[UpdateRung->Element[x][y].VarNum];
    // directly connected to the "left"? if yes, ON !
    if (x==0)
    {
        Timer->InputEnable = 1;
    }
    else
    {
        Timer->InputEnable = StateOnLeft(x-1,y,UpdateRung);
    }
    if (!Timer->InputEnable)
    {
        Timer->OutputRunning = 0;
        Timer->OutputDone = 0;
        Timer->Value = Timer->Preset;
    }
    else
    {
        if (Timer->Value>0)
        {
            Timer->Value = Timer->Value - TIME_REFRESH_RUNG_MS;
            Timer->OutputRunning = 1;
            Timer->OutputDone = 0;
        }
        else
        {
            Timer->OutputRunning = 0;
            Timer->OutputDone = 1;
        }
    }
    UpdateRung->Element[x][y].DynamicOutput = Timer->OutputDone;
    UpdateRung->Element[x][y+1].DynamicOutput = Timer->OutputRunning;
}
/* Element : Monostable (2x2 Blocks) */
void CalcTypeMonostable(int x,int y,StrRung * UpdateRung)
{
    StrMonostable * Monostable;
    Monostable = &MonostableArray[UpdateRung->Element[x][y].VarNum];
    // directly connected to the "left"? if yes, ON !
    if (x==0)
    {
        Monostable->Input = 1;
    }
    else
    {
        Monostable->Input = StateOnLeft(x-1,y,UpdateRung);
    }
    /* detecting impulse on input, the monostable is not retriggerable */
    if (Monostable->Input && !Monostable->InputBak && (Monostable->Value==0) )
    {
        Monostable->OutputRunning = 1;
        Monostable->Value = Monostable->Preset;
    }
    if (Monostable->Value>0)
        Monostable->Value = Monostable->Value - TIME_REFRESH_RUNG_MS;
    else
        Monostable->OutputRunning = 0;
    Monostable->InputBak = Monostable->Input;
    UpdateRung->Element[x][y].DynamicOutput = Monostable->OutputRunning;
}
/* Element : Counter (2x4 Blocks) */
void CalcTypeCounter(int x,int y,StrRung * UpdateRung)
{
	StrCounter * Counter = &CounterArray[ UpdateRung->Element[x][y].VarNum ];
	// directly connected to the "left"? if yes, ON !
	if ( x==0 )
	{
		Counter->InputReset = 1;
		Counter->InputPreset = 1;
		Counter->InputCountUp = 1;
		Counter->InputCountDown = 1;
	}
	else
	{
		Counter->InputReset = StateOnLeft(x-1,y,UpdateRung);
		Counter->InputPreset = StateOnLeft(x-1,y+1,UpdateRung);
		Counter->InputCountUp = StateOnLeft(x-1,y+2,UpdateRung);
		Counter->InputCountDown = StateOnLeft(x-1,y+3,UpdateRung);
	}
	if ( Counter->InputCountUp && Counter->InputCountUpBak==0 )
	{
		Counter->ValueBak = Counter->Value;
		Counter->Value++;
		if ( Counter->Value>9999 )
			Counter->Value = 0;
	}
	if ( Counter->InputCountDown && Counter->InputCountDownBak==0 )
	{
		Counter->ValueBak = Counter->Value;
		Counter->Value--;
		if ( Counter->Value<0 )
			Counter->Value = 9999;
	}
	if ( Counter->InputPreset )
	{
		Counter->ValueBak = Counter->Value;
		Counter->Value = Counter->Preset;
	}
	if ( Counter->InputReset )
	{
		Counter->ValueBak = Counter->Value;
		Counter->Value = 0;
	}
	Counter->InputCountUpBak = Counter->InputCountUp;
	Counter->InputCountDownBak = Counter->InputCountDown;
	Counter->OutputDone = ( Counter->Value==Counter->Preset )?1:0;
	Counter->OutputEmpty = ( Counter->Value==9999 && Counter->ValueBak==0 )?1:0;
	Counter->OutputFull = ( Counter->Value==0 && Counter->ValueBak==9999 )?1:0;

	UpdateRung->Element[x][y].DynamicOutput = Counter->OutputEmpty;
	UpdateRung->Element[x][y + 1].DynamicOutput = Counter->OutputDone;
	UpdateRung->Element[x][y + 2].DynamicOutput = Counter->OutputFull;
}

/* Element : Compar (3 Horizontal Blocks) */
char CalcTypeCompar(int x,int y,StrRung * UpdateRung)
{
    char State;
    char StateElement;

    StateElement = EvalCompare(ArithmExpr[UpdateRung->Element[x][y].VarNum].Expr);
    UpdateRung->Element[x][y].DynamicState = StateElement;
    if (x==2)
    {
        State = StateElement;
    }
    else
    {
        UpdateRung->Element[x-2][y].DynamicInput = StateOnLeft(x-2,y,UpdateRung);
        State = StateElement && UpdateRung->Element[x-2][y].DynamicInput;
    }
    UpdateRung->Element[x][y].DynamicOutput = State;
    return State;
}

/* Element : Operate (3 Horizontal Blocks) */
char CalcTypeOutputOperate(int x,int y,StrRung * UpdateRung)
{
    char State;
    State = StateOnLeft(x-2,y,UpdateRung);
    if (State)
        MakeCalc(ArithmExpr[UpdateRung->Element[x][y].VarNum].Expr,FALSE /* verify mode */);
    UpdateRung->Element[x][y].DynamicInput = State;
    UpdateRung->Element[x][y].DynamicState = State;
    return State;
}


int RefreshRung(StrRung * Rung, int * JumpTo, int * CallTo)
{
    int x,y;
    int JumpToRung = -1;
    int SectionToCall = -1;

    for (x=0;x<RUNG_WIDTH;x++)
    {
        for(y=0;y<RUNG_HEIGHT;y++)
        {
            switch(Rung->Element[x][y].Type)
            {
                /* MLD,16/5/2001,V0.2.8 , fixed for drawing */
                case ELE_FREE:
                case ELE_UNUSABLE:
                    if (StateOnLeft(x,y,Rung))
                        Rung->Element[x][y].DynamicInput = 1;
                    else
                        Rung->Element[x][y].DynamicInput = 0;
                    break;
                /* End fix */
                case ELE_INPUT:
                    CalcTypeInput(x,y,Rung,FALSE,FALSE);
                    break;
                case ELE_INPUT_NOT:
                    CalcTypeInput(x,y,Rung,TRUE,FALSE);
                    break;
                case ELE_RISING_INPUT:
                    CalcTypeInput(x,y,Rung,FALSE,TRUE);
                    break;
                case ELE_FALLING_INPUT:
                    CalcTypeInput(x,y,Rung,TRUE,TRUE);
                    break;
                case ELE_CONNECTION:
                    CalcTypeConnection(x,y,Rung);
                    break;
                case ELE_TIMER:
                    CalcTypeTimer(x,y,Rung);
                    break;
                case ELE_MONOSTABLE:
                    CalcTypeMonostable(x,y,Rung);
                    break;
                case ELE_COUNTER:
                    CalcTypeCounter(x,y,Rung);
                    break;
                case ELE_COMPAR:
                    CalcTypeCompar(x,y,Rung);
                    break;
                case ELE_OUTPUT:
                    CalcTypeOutput(x,y,Rung,FALSE);
                    break;
                case ELE_OUTPUT_NOT:
                    CalcTypeOutput(x,y,Rung,TRUE);
                    break;
                case ELE_OUTPUT_SET:
                    CalcTypeOutputSetReset(x,y,Rung,FALSE);
                    break;
                case ELE_OUTPUT_RESET:
                    CalcTypeOutputSetReset(x,y,Rung,TRUE);
                    break;
                case ELE_OUTPUT_JUMP:
                    JumpToRung = CalcTypeOutputJump(x,y,Rung);
                    // TODO : abort the refresh of the rung immediately...
                    break;
                case ELE_OUTPUT_CALL:
                    SectionToCall = CalcTypeOutputCall(x,y,Rung);
                    break;
                case ELE_OUTPUT_OPERATE:
                    CalcTypeOutputOperate(x,y,Rung);
                    break;
            }
        }
    }

    *JumpTo = JumpToRung;
    *CallTo = SectionToCall;
    // TODO : tell if the rung isn't finished : if found Call, immediately abort
    // and return here to finish the end, after the sub-routine has been called...
    return TRUE;
}

// we refresh all the rungs of one section.
// we can (J)ump to another rung in the same section.
// we can (C)all a sub-routine (a section) and at the end of it, returns to the
// next rung.
// All the sections 'main' are refreshed in the order defined.
// TODO : if we have a mad Jump (infinite loop) we must make something !
#define SR_STACK 25
void RefreshAllRungs()
{
    int NumRung;
    int Goto;
    int SectionForCallSR;
    int Done;
    int ScanMainSection;
    int CurrentSection;
    StrSection * pSection;

    int SubRoutinesStack[ SR_STACK ];
    int RungsStack[ SR_STACK ];
    int SubRoutineDepth = 0;

//TODO: times measures should be moved directly in the module task
#ifdef __RTL__
    long StartTime = gethrtime();
#endif
#if defined( RTAI ) && defined( MODULE )
    long StartTime = rt_get_cpu_time_ns();
#endif
    CycleStart();

    for ( ScanMainSection=0; ScanMainSection<NBR_SECTIONS; ScanMainSection++ )
    {

        CurrentSection = ScanMainSection;
        pSection = &SectionArray[ CurrentSection ];

        // current section defined and is a main-section (not a sub-routine)
        // and in Ladder language ?
        if ( pSection->Used && pSection->SubRoutineNumber==-1 && pSection->Language==SECTION_IN_LADDER )
        {
            Done = FALSE;
            NumRung = pSection->FirstRung;
            do
            {
                RefreshRung(&RungArray[NumRung], &Goto, &SectionForCallSR);
                if ( SectionForCallSR!=-1 )
                {
                    if ( SectionArray[ SectionForCallSR ].Used && SectionArray[ SectionForCallSR ].SubRoutineNumber>=0 )
                    {
                        if ( SubRoutineDepth<SR_STACK-1 )
                        {
                            // saving current section context
                            SubRoutineDepth++;
                            SubRoutinesStack[ SubRoutineDepth ] = CurrentSection;
                            if ( NumRung!=pSection->LastRung )
                            {
                                RungsStack[ SubRoutineDepth ] = RungArray[NumRung].NextRung;
                            }
                            else
                            {
                                // at the end of the sub-routine, this section is already finished
                                RungsStack[ SubRoutineDepth ] = -1;
                            }
                            // now starting the sub-routine section...
                            CurrentSection = SectionForCallSR;
                            pSection = &SectionArray[ CurrentSection ];
                            NumRung = pSection->FirstRung;
//printf("-> sub-routine called : SR : Sec=%d, Rung =%d\n", CurrentSection, NumRung );
                        }
                        else
                        {
                            debug_printf("Refresh rungs aborted - too much sub-routine calls in series !?");
                        }
                    }
                    else
                    {
                        debug_printf("Refresh rungs aborted - call to a sub-routine undefined or programmed as main !!!");
                    }
                }
                else if ( Goto!=-1 )
                {
                    if (!RungArray[Goto].Used)
                    {
                        Done = TRUE;
                        debug_printf("Refresh rungs aborted - jump to an undefined rung found in rung No%d...\n",Goto);
                    }
                    NumRung = Goto;
                }
                else
                {
                    if (NumRung == pSection->LastRung)
                    {
                        // if this section is a sub-routine, we make a return...
                        if ( pSection->SubRoutineNumber>=0 )
                        {
                            int ReturnFound = FALSE;
                            do
                            {
                                if ( SubRoutineDepth>0 )
                                {

                                    // previous section already finished ?
                                    if ( RungsStack[ SubRoutineDepth ]!=-1 )
                                    {
                                        CurrentSection = SubRoutinesStack[ SubRoutineDepth ];
                                        pSection = &SectionArray[ CurrentSection ];
                                        NumRung = RungsStack[ SubRoutineDepth ];
                                        ReturnFound = TRUE;
//printf( "-> return from sub-routine to : Sec=%d, Rung =%d\n", CurrentSection, NumRung );
                                    }
                                    else
                                    {
                                        // the previous main section was already finished...
                                        if ( SectionArray[ SubRoutinesStack[ SubRoutineDepth ] ].SubRoutineNumber==0 )
                                        {
                                            Done = TRUE;
                                            ReturnFound = TRUE;
//printf( "-> return from sub-routine with previous main section done...\n" );
                                        }
                                        else
                                        {
//printf( "-> return from sub-routine with sub-routine already done... searching again...\n" );
                                        }

                                    }
                                    SubRoutineDepth--;
                                }
                                else
                                {
                                    debug_printf("Refresh rungs aborted - return of sub-routine without call before !?");
                                }
                            }
                            while( !ReturnFound );
                        }
                        else
                        {
                            // last rung found for a main section
                            Done = TRUE;
                        }
                    }
                    else
                        NumRung = RungArray[NumRung].NextRung;
                }
            }
            while(!Done);
        }

#ifdef SEQUENTIAL_SUPPORT
        // current section defined and is in sequential language
        if ( pSection->Used && pSection->Language==SECTION_IN_SEQUENTIAL )
        {
            RefreshSequentialPage( pSection->SequentialPage );
        }
#endif

    }// for( )

    CycleEnd();
//TODO: times measures should be moved directly in the module task
#ifdef __RTL__
    InfosGene->DurationOfLastScan = gethrtime() - StartTime;
//    rtl_printf("Time elapsed for refresh = %u nsec\n",gethrtime()-StartTime);
#endif
#if defined( RTAI ) && defined ( MODULE )
    InfosGene->DurationOfLastScan = rt_get_cpu_time_ns() - StartTime;
#endif
}

void CopyRungToRung(StrRung * RungSrc,StrRung * RungDest)
{
    memcpy(RungDest,RungSrc,sizeof(StrRung));
}

