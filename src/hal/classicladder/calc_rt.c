/* Classic Ladder Project */
/* Copyright (C) 2001 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* February 2001 */
/* Last update : 19 Ocotober 2002 */
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

#include "rtapi.h"
#include "arithm_eval.h"
#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "manager.h"
#ifdef SEQUENTIAL_SUPPORT
#include "calc_sequential_rt.h"
#endif
#include "calc_rt.h"


static char StateOnLeft(int x,int y,StrRung * TheRung)
{
    char State = 0;
    int PosY;
    char StillConnected;
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
static char CalcTypeInput(int x,int y,StrRung * UpdateRung,char IsNot,char OnlyFronts)
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
static char CalcTypeConnection(int x,int y,StrRung * UpdateRung)
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
static char CalcTypeOutput(int x,int y,StrRung * UpdateRung,char IsNot)
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
static char CalcTypeOutputSetReset(int x,int y,StrRung * UpdateRung,char IsReset)
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
static int CalcTypeOutputJump(int x,int y,StrRung * UpdateRung)
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
static int CalcTypeOutputCall(int x,int y,StrRung * UpdateRung)
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
static void CalcTypeTimer(int x,int y,StrRung * UpdateRung)
{
    StrTimer * Timer;
    Timer = &TimerArray[UpdateRung->Element[x][y].VarNum];
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
            Timer->Value -= InfosGene->TimeSinceLastScan;
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
static void CalcTypeMonostable(int x,int y,StrRung * UpdateRung)
{
    StrMonostable * Monostable;
    Monostable = &MonostableArray[UpdateRung->Element[x][y].VarNum];
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
        Monostable->Value -= InfosGene->TimeSinceLastScan;
    else
        Monostable->OutputRunning = 0;
    Monostable->InputBak = Monostable->Input;
    UpdateRung->Element[x][y].DynamicOutput = Monostable->OutputRunning;
}

/* Element : Compar (3 Horizontal Blocks) */
static char CalcTypeCompar(int x,int y,StrRung * UpdateRung)
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
static char CalcTypeOutputOperate(int x,int y,StrRung * UpdateRung)
{
    char State;
    State = StateOnLeft(x-2,y,UpdateRung);
    if (State)
        MakeCalc(ArithmExpr[UpdateRung->Element[x][y].VarNum].Expr,FALSE /* verify mode */);
    UpdateRung->Element[x][y].DynamicInput = State;
    UpdateRung->Element[x][y].DynamicState = State;
    return State;
}

static int RefreshRung(StrRung * Rung, int * JumpTo, int * CallTo)
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
void RefreshAllRungs(long NsSinceLastScan)
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

    long long int StartTime = rtapi_get_time();

    InfosGene->NsSinceLastScan += NsSinceLastScan;
    InfosGene->TimeSinceLastScan = InfosGene->NsSinceLastScan / 1000000;
    InfosGene->NsSinceLastScan = InfosGene->NsSinceLastScan % 1000000;

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
                        }
                        else
                        {
                            rtapi_print_msg(RTAPI_MSG_INFO, "Refresh rungs aborted - too much sub-routine calls in series !?");
                        }
                    }
                    else
                    {
                        rtapi_print_msg(RTAPI_MSG_INFO, "Refresh rungs aborted - call to a sub-routine undefined or programmed as main !!!");
                    }
                }
                else if ( Goto!=-1 )
                {
                    if (!RungArray[Goto].Used)
                    {
                        Done = TRUE;
                        rtapi_print_msg(RTAPI_MSG_INFO, "Refresh rungs aborted - jump to an undefined rung found in rung No%d...\n",Goto);
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
                                    }
                                    else
                                    {
                                        // the previous main section was already finished...
                                        if ( SectionArray[ SubRoutinesStack[ SubRoutineDepth ] ].SubRoutineNumber==0 )
                                        {
                                            Done = TRUE;
                                            ReturnFound = TRUE;
                                        }

                                    }
                                    SubRoutineDepth--;
                                }
                                else
                                {
                                    rtapi_print_msg(RTAPI_MSG_INFO, "Refresh rungs aborted - return of sub-routine without call before !?");
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

    InfosGene->DurationOfLastScan = rtapi_get_time() - StartTime;
}
