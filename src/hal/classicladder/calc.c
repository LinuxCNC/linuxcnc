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

#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
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

void InitArithmExpr()
{
    int NumExpr;
    for (NumExpr=0; NumExpr<NBR_ARITHM_EXPR; NumExpr++)
        strcpy(ArithmExpr[NumExpr].Expr,"");
}
