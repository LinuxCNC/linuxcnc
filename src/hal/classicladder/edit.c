/* Classic Ladder Project */
/* Copyright (C) 2001-2004 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* May 2001 */
/* ------ */
/* Editor */
/* ------ */
/* This part of the editor is the one who will not change even if if we use */
/* another gui instead of gtk... who know? */
/* ------------------------------------------------------------- */
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


#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "classicladder.h"
#include "global.h"
#include "drawing.h"
#include "edit.h"
#include "editproperties_gtk.h"
#include "calc.h"
#include "files.h"
#include "arithm_eval.h"
#include "classicladder_gtk.h"
#include "manager.h"
#ifdef SEQUENTIAL_SUPPORT
#include "edit_sequential.h"
#endif

/* This array give for each special elements the size used */
#define TYPEELERULE 0
#define XSIZEELERULE 1
#define YSIZEELERULE 2
static short int RulesForSpecialElements[5][3] =
            { {ELE_TIMER , 2, 2 } ,
              {ELE_MONOSTABLE, 2, 2 },
              {ELE_COMPAR, 3, 1 },
              {ELE_OUTPUT_OPERATE, 3, 1 },
              {-1, -1, -1}/*end*/ };



int ConvBaseInTextToId(char * text)
{
    int ScanBase = 0;
    int BaseFound = BASE_SECS;
    do
    {
        if (strcmp(text,CorresDatasForBase[ScanBase].ParamSelect)==0)
            BaseFound = ScanBase;
        ScanBase++;
    }
    while(ScanBase<NBR_TIMEBASES);
    return BaseFound;
}

int ConvLabelToNumRung(char * LabelName)
{
    int ScanRung = 0;
    int RungFound = -1;
    int Done = FALSE;
    ScanRung = InfosGene->FirstRung;
    do
    {
        if (strcmp(RungArray[ScanRung].Label,LabelName)==0)
            RungFound = ScanRung;
        if (ScanRung == InfosGene->LastRung)
            Done = TRUE;
        else
            ScanRung = RungArray[ScanRung].NextRung;
    }
    while(!Done);
    return RungFound;
}

void LoadElementProperties(StrElement * Element)
{
    char TextToWrite[100];
    int NumParam;
    StrTimer * Timer;
    StrMonostable * Monostable;
    for(NumParam=0;NumParam<NBR_PARAMS_PER_OBJ;NumParam++)
        SetProperty(NumParam,"---","");
    if (Element)
    {
        switch(Element->Type)
        {
            case ELE_INPUT:
            case ELE_INPUT_NOT:
            case ELE_RISING_INPUT:
            case ELE_FALLING_INPUT:
            case ELE_OUTPUT:
            case ELE_OUTPUT_NOT:
            case ELE_OUTPUT_SET:
            case ELE_OUTPUT_RESET:
                strcpy(TextToWrite,DisplayInfo(Element->VarType,Element->VarNum));
                SetProperty(0,"Variable",TextToWrite);
                break;
            case ELE_OUTPUT_JUMP:
                SetProperty(0,"JumpToLabel",RungArray[Element->VarNum].Label);
                break;
            case ELE_OUTPUT_CALL:
                sprintf(TextToWrite,"%d",Element->VarNum);
                SetProperty(0,"Sub-Routine",TextToWrite);
                break;
            case ELE_TIMER:
                Timer = &TimerArray[Element->VarNum];
                sprintf(TextToWrite,"%d",Element->VarNum);
                SetProperty(0,"Number",TextToWrite);
                sprintf(TextToWrite,"%s",CorresDatasForBase[ ConvBaseInMilliSecsToId(Timer->Base) ].ParamSelect);
                SetProperty(1,"Base",TextToWrite);
                sprintf(TextToWrite,"%d",Timer->Preset/Timer->Base);
                SetProperty(2,"Preset",TextToWrite);
                break;
            case ELE_MONOSTABLE:
                Monostable = &MonostableArray[Element->VarNum];
                sprintf(TextToWrite,"%d",Element->VarNum);
                SetProperty(0,"Number",TextToWrite);
                sprintf(TextToWrite,"%s",CorresDatasForBase[ ConvBaseInMilliSecsToId(Monostable->Base) ].ParamSelect);
                SetProperty(1,"Base",TextToWrite);
                sprintf(TextToWrite,"%d",Monostable->Preset/Monostable->Base);
                SetProperty(2,"Preset",TextToWrite);
                break;
            case ELE_COMPAR:
            case ELE_OUTPUT_OPERATE:
                strcpy(TextToWrite,DisplayArithmExpr(EditArithmExpr[Element->VarNum].Expr,0));
                SetProperty(0,"Expression",TextToWrite);
                break;
        }
    }
}

/* Convert a string to a number if >=Min and <=Max */
/* return TRUE if okay */
int TextToNumber(char * text,int ValMin,int ValMaxi,int *ValFound)
{
    int IsOk = TRUE;
    int Value;
    Value = atoi(text);
    if ( (Value<ValMin) || (Value>ValMaxi) )
        IsOk = FALSE;
    if (IsOk)
        *ValFound = Value;
    return IsOk;
}

/* Convert a string to variable (type/offset) */
/* return TRUE if okay */
int TextParserForAVar(char * text,int * VarTypeFound,int * VarOffsetFound)
{
    int TypeFound = -1;	// To avoid warnings.
    int OffsetFound;
    int IsOk = TRUE;
#ifdef SEQUENTIAL_SUPPORT
    char * pSearch;
#endif
    switch(text[0])
    {
        case 'B':
            TypeFound = VAR_MEM_BIT;
            if (!TextToNumber(&text[1],0,NBR_BITS-1,&OffsetFound))
                IsOk = FALSE;
            break;

        case 'T':
            if (text[strlen(text)-2]!=',')
            {
                IsOk = FALSE;
            }
            else
            {
                switch(text[strlen(text)-1])
                {
                    case 'D':
                        TypeFound = VAR_TIMER_DONE;
                        break;
                    case 'R':
                        TypeFound = VAR_TIMER_RUNNING;
                        break;
                    default:
                        IsOk = FALSE;
                        break;
                }
                text[strlen(text)-2] = '\0';
                if(!TextToNumber(&text[1],0,NBR_TIMERS-1,&OffsetFound))
                    IsOk = FALSE;
            }
            break;

        case 'M':
            if (text[strlen(text)-2]!=',')
            {
                IsOk = FALSE;
            }
            else
            {
                switch(text[strlen(text)-1])
                {
                    case 'R':
                        TypeFound = VAR_MONOSTABLE_RUNNING;
                        break;
                    default:
                        IsOk = FALSE;
                        break;
                }
                text[strlen(text)-2] = '\0';
                if(!TextToNumber(&text[1],0,NBR_MONOSTABLES-1,&OffsetFound))
                    IsOk = FALSE;
            }
            break;

        case 'I':
            TypeFound = VAR_PHYS_INPUT;
            if (!TextToNumber(&text[1],0,NBR_PHYS_INPUTS-1,&OffsetFound))
                IsOk = FALSE;
            break;

        case 'Q':
            TypeFound = VAR_PHYS_OUTPUT;
            if (!TextToNumber(&text[1],0,NBR_PHYS_OUTPUTS-1,&OffsetFound))
                IsOk = FALSE;
            break;

        case 'W':
            TypeFound = VAR_MEM_WORD;
            if (!TextToNumber(&text[1],0,NBR_WORDS-1,&OffsetFound))
                IsOk = FALSE;
            break;

#ifdef SEQUENTIAL_SUPPORT
        case 'X':
            // verify if there is a ',' after number so that it must be the X..,V form
            // instead of X... form.
            pSearch = text;
            do
            {
                pSearch++;
            }
            while( *pSearch>='0' && *pSearch<='9' && *pSearch!='\0' );
            if ( *pSearch!=',' )
            {
                TypeFound = VAR_STEP_ACTIVITY;
                if(!TextToNumber(&text[1],0,NBR_STEPS-1,&OffsetFound))
                    IsOk = FALSE;
            }
            else
            {
                *pSearch = '\0';
                pSearch++;
                switch(*pSearch)
                {
                    case 'V':
                        TypeFound = VAR_STEP_TIME;
                        break;
                    default:
                        IsOk = FALSE;
                        break;
                }
                if(!TextToNumber(&text[1],0,NBR_STEPS-1,&OffsetFound))
                    IsOk = FALSE;
            }
            break;
#endif

        default:
            IsOk = FALSE;
    }
    if (IsOk)
    {
        *VarTypeFound = TypeFound;
        *VarOffsetFound = OffsetFound;
    }
    return IsOk;
}

/* Convert a string of arithmetic expression for the arithm_eval : */
/* Variables becomes @type/offset@ */
/* Then verify all the expression using arithm_eval */
/* return pointer on the new string converted and verified if okay, else NULL */
char * TextParserForArithmExpr(char * text, int TypeElement)
{
    static char NewExpr[100];
    char Buffer[20];
    int ItIsOk = TRUE;
    int VarType;
    int VarOffset;
    char * ErrorMessage = NULL;
    char * ptr = text;
    strcpy(NewExpr,"");
    /* Convert expression for variables authorized */
    do
    {
        /* Verify if it is a variable (character and number) */
        if ( *(ptr+1)>='0' && *(ptr+1)<='9' )
        {
        switch(*ptr)
        {
            case 'W':
                if (TextParserForAVar(ptr,&VarType,&VarOffset))
                {
                    sprintf(Buffer,"@%d/%d@",VarType,VarOffset);
                    strcat(NewExpr,Buffer);
                    do
                    {
                        ptr++;
                    }
                    while( (*ptr>='0') && (*ptr<='9') );
                }
                else
                {
                    ItIsOk = FALSE;
                }
                break;
            case 'X':
                if (TextParserForAVar(ptr,&VarType,&VarOffset))
                {
                    if ( VarType==VAR_STEP_TIME )
                    {
                        sprintf(Buffer,"@%d/%d@",VarType,VarOffset);
                        strcat(NewExpr,Buffer);
                        do
                        {
                            ptr++;
                        }
                        while( *ptr!='V' );
                        ptr++;
                    }
                    else
                    {
                        ItIsOk = FALSE;
                        ErrorMessage = "Incompatible type of variable";
                    }
                }
                else
                {
                    ItIsOk = FALSE;
                }
                break;
            case 'B':
            case 'T':
            case 'M':
            case 'I':
            case 'Q':
                ItIsOk = FALSE;
                ErrorMessage = "Incompatible type of variable";
                break;
            case '\0':
                ItIsOk = FALSE;
                ErrorMessage = "Null expression not valid";
                break;
            default:
                sprintf(Buffer,"%c",*ptr);
                strcat(NewExpr, Buffer);
                ptr++;
                break;
        }
        }
        else
        {
            sprintf(Buffer,"%c",*ptr);
            strcat(NewExpr, Buffer);
            ptr++;
        }
    }
    while( (*ptr) && ItIsOk );
    /* Verify length of the expression */
    if (ItIsOk)
    {
        if (strlen(NewExpr)>=ARITHM_EXPR_SIZE)
        {
            ItIsOk = FALSE;
            ErrorMessage = "Expression too long";
        }
    }
//printf("Parser Arithm ; ItIsOk=%d ; OriExpr=%s ; NewExpr=%s\n",ItIsOk, text, NewExpr);
    /* Verify expression converted */
    if (ItIsOk)
    {
        if (TypeElement==ELE_OUTPUT_OPERATE)
            ErrorMessage = VerifySyntaxForMakeCalc(NewExpr);
        else
            ErrorMessage = VerifySyntaxForEvalCompare(NewExpr);
        if (ErrorMessage)
            ItIsOk = FALSE;
    }
    /* Error Message */
    if (ErrorMessage)
    {
        ShowMessageBox("Error",ErrorMessage,"Ok");
    }
    /* Give result */
    if (ItIsOk)
        return NewExpr;
    else
        return NULL;
}

void SaveElementProperties()
{
    int IdBase;
    int Preset;
    int NumRungToJump;
    StrTimer * Timer;
    StrMonostable * Monostable;
    char * NewArithmExpr;
    int SubRoutineToCall;

#ifdef SEQUENTIAL_SUPPORT
    int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
    if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
    {
        SaveSeqElementProperties( );
        return;
    }
#endif

    if (EditDatas.ElementUnderEdit)
    {
        switch(EditDatas.ElementUnderEdit->Type)
        {
            case ELE_INPUT:
            case ELE_INPUT_NOT:
            case ELE_RISING_INPUT:
            case ELE_FALLING_INPUT:
            case ELE_OUTPUT:
            case ELE_OUTPUT_NOT:
            case ELE_OUTPUT_SET:
            case ELE_OUTPUT_RESET:
                TextParserForAVar(GetProperty(0),&EditDatas.ElementUnderEdit->VarType,&EditDatas.ElementUnderEdit->VarNum);
                break;
            case ELE_TIMER:
                TextToNumber(GetProperty(0),0,NBR_TIMERS-1,&EditDatas.ElementUnderEdit->VarNum);
                Timer = &TimerArray[EditDatas.ElementUnderEdit->VarNum];
                IdBase = ConvBaseInTextToId(GetProperty(1));
                Timer->Base = CorresDatasForBase[IdBase].ValueInMS;
                strcpy(Timer->DisplayFormat,CorresDatasForBase[IdBase].DisplayFormat);
                if (TextToNumber(GetProperty(2),0,999,&Preset))
                    Timer->Preset = Preset * Timer->Base;
                break;
            case ELE_MONOSTABLE:
                TextToNumber(GetProperty(0),0,NBR_MONOSTABLES-1,&EditDatas.ElementUnderEdit->VarNum);
                Monostable = &MonostableArray[EditDatas.ElementUnderEdit->VarNum];
                IdBase = ConvBaseInTextToId(GetProperty(1));
                Monostable->Base = CorresDatasForBase[IdBase].ValueInMS;
                strcpy(Monostable->DisplayFormat,CorresDatasForBase[IdBase].DisplayFormat);
                if (TextToNumber(GetProperty(2),0,999,&Preset))
                    Monostable->Preset = Preset * Monostable->Base;
                break;
            case ELE_OUTPUT_JUMP:
                NumRungToJump = ConvLabelToNumRung(GetProperty(0));
                if (NumRungToJump>=0)
                    EditDatas.ElementUnderEdit->VarNum = NumRungToJump;
                break;
            case ELE_OUTPUT_CALL:
                if ( TextToNumber(GetProperty(0),0,NBR_SECTIONS-1,&SubRoutineToCall) )
                {
                    if ( VerifyIfSubRoutineNumberExist( SubRoutineToCall ) )
                        EditDatas.ElementUnderEdit->VarNum = SubRoutineToCall;
                }
                break;
            case ELE_COMPAR:
                NewArithmExpr = TextParserForArithmExpr(GetProperty(0), ELE_COMPAR);
                if (NewArithmExpr)
                    strcpy(EditArithmExpr[EditDatas.ElementUnderEdit->VarNum].Expr,NewArithmExpr);
                break;
            case ELE_OUTPUT_OPERATE:
                NewArithmExpr = TextParserForArithmExpr(GetProperty(0), ELE_OUTPUT_OPERATE);
                if (NewArithmExpr)
                    strcpy(EditArithmExpr[EditDatas.ElementUnderEdit->VarNum].Expr,NewArithmExpr);
                break;
        }
        /* display back to show what we have really understand... */
        LoadElementProperties(EditDatas.ElementUnderEdit);
    }
}

/* For editing, we do not touch directly the current arithm
   expressions. We work on the edit ones. It is only when the
   edited rung is applyed that we copy the expressions edited */
void CopyCurrentArithmExpr()
{
    int NumExpr;
    for (NumExpr=0; NumExpr<NBR_ARITHM_EXPR; NumExpr++)
        strcpy(EditArithmExpr[NumExpr].Expr,ArithmExpr[NumExpr].Expr);
}
void ApplyNewArithmExpr()
{
    int NumExpr;
    for (NumExpr=0; NumExpr<NBR_ARITHM_EXPR; NumExpr++)
        strcpy(ArithmExpr[NumExpr].Expr,EditArithmExpr[NumExpr].Expr);
}
void CheckForFreeingArithmExpr(int PosiX,int PosiY)
{
    int TypeElement = EditDatas.Rung.Element[PosiX][PosiY].Type;
    if ( (TypeElement==ELE_COMPAR) || (TypeElement==ELE_OUTPUT_OPERATE) )
    {
        /* Freeing Expr */
        EditArithmExpr[ EditDatas.Rung.Element[PosiX][PosiY].VarNum ].Expr[0] = '\0';
    }
}
void CheckForAllocatingArithmExpr(int PosiX,int PosiY)
{
    int TypeElement = EditDatas.Rung.Element[PosiX][PosiY].Type;
    int NumExpr = 0;
    int Found = FALSE;
    if ( (TypeElement==ELE_COMPAR) || (TypeElement==ELE_OUTPUT_OPERATE) )
    {
        do
        {
            /* Expr free ? */
            if (EditArithmExpr[ NumExpr ].Expr[0]=='\0')
            {
                Found = TRUE;
                /* Allocate this expr for the operate/compar block ! */
                EditDatas.Rung.Element[PosiX][PosiY].VarNum = NumExpr;
            }
            NumExpr++;
        }
        while( (NumExpr<NBR_ARITHM_EXPR) && (!Found) );
    }
}

void InitBufferRungEdited( StrRung * pRung )
{
    int x,y;
    for (y=0;y<RUNG_HEIGHT;y++)
    {
        for(x=0;x<RUNG_WIDTH;x++)
        {
            pRung->Element[x][y].Type = ELE_FREE;
            pRung->Element[x][y].ConnectedWithTop = 0;
            pRung->Element[x][y].VarType = 0;
            pRung->Element[x][y].VarNum = 0;
            pRung->Element[x][y].DynamicState = 0;
        }
    }
    pRung->Used = TRUE;
    pRung->PrevRung = -1;
    pRung->NextRung = -1;
}

int GetNbrRungsDefined( void )
{
    int NbrRungsUsed = 0;
    int ScanRung;
    for( ScanRung=0; ScanRung<NBR_RUNGS; ScanRung++ )
    {
        if (RungArray[ScanRung].Used)
            NbrRungsUsed++;
    }
    return NbrRungsUsed;
}

int FindFreeRung()
{
    int NumFree = -1;
    int ScanRung = 0;
    do
    {
        if (!RungArray[ScanRung].Used)
            NumFree = ScanRung;
        ScanRung++;
    }
    while( (NumFree==-1)&&(ScanRung<NBR_RUNGS) );
    return NumFree;
}

void AddRung()
{
    InitBufferRungEdited( &EditDatas.Rung );
    EditDatas.DoBeforeFinalCopy = MODE_ADD;
    EditDatas.NumRung = FindFreeRung();
    /* we have found a free rung ? */
    if (EditDatas.NumRung>=0)
    {
        EditDatas.ModeEdit = TRUE;
        EditDatas.ElementUnderEdit = NULL;
        autorize_prevnext_buttons(FALSE);
        clear_label_comment();
        CopyCurrentArithmExpr();
    }
}

void InsertRung()
{
    /* not really different than adding a rung... */
    AddRung();
    EditDatas.DoBeforeFinalCopy = MODE_INSERT;
}

void ModifyCurrentRung()
{
    CopyRungToRung(&RungArray[InfosGene->CurrentRung],&EditDatas.Rung);
    EditDatas.DoBeforeFinalCopy = MODE_MODIFY;
    EditDatas.NumRung = InfosGene->CurrentRung;
    EditDatas.ModeEdit = TRUE;
    EditDatas.ElementUnderEdit = NULL;
    autorize_prevnext_buttons(FALSE);
    CopyCurrentArithmExpr();
}

void DeleteCurrentRung()
{
    int NextDeleted;
    int PrevDeleted;
    int NewCurrent;
    int OldCurrent;
    int x,y;
    /* do not destroy the last one ! */
    if ( !((InfosGene->CurrentRung==InfosGene->FirstRung) && (InfosGene->CurrentRung==InfosGene->LastRung)) )
    {
        PrevDeleted = RungArray[InfosGene->CurrentRung].PrevRung;
        NextDeleted = RungArray[InfosGene->CurrentRung].NextRung;
        NewCurrent = NextDeleted;
        /* deleting first rung ?*/
        if (InfosGene->CurrentRung==InfosGene->FirstRung)
        {
            InfosGene->FirstRung = NextDeleted;
        }
        else
        {
            RungArray[PrevDeleted].NextRung = NextDeleted;
        }
        /* deleting last rung ? */
        if (InfosGene->CurrentRung==InfosGene->LastRung)
        {
            InfosGene->LastRung = PrevDeleted;
            NewCurrent = InfosGene->LastRung;
        }
        else
        {
            RungArray[NextDeleted].PrevRung = PrevDeleted;
        }
        /* the rung is now free ! */
        OldCurrent = InfosGene->CurrentRung;
        RungArray[InfosGene->CurrentRung].Used = FALSE;
        InfosGene->CurrentRung = NewCurrent;
        DrawRungs();
        UpdateVScrollBar();
        refresh_label_comment( );
        /* save infos for the current section */
        SectionArray[ InfosGene->CurrentSection ].FirstRung = InfosGene->FirstRung;
        SectionArray[ InfosGene->CurrentSection ].LastRung = InfosGene->LastRung;
        /* kill the expressions used on the rung deleted */
        for (y=0;y<RUNG_HEIGHT;y++)
        {
            for(x=0;x<RUNG_WIDTH;x++)
            {
                if ( (RungArray[OldCurrent].Element[x][y].Type == ELE_COMPAR)
                   || (RungArray[OldCurrent].Element[x][y].Type == ELE_OUTPUT_OPERATE) )
                {
                    strcpy(ArithmExpr[ RungArray[OldCurrent].Element[x][y].VarNum ].Expr,"");
                }
            }
        }
    }
}

void CancelRungEdited()
{
    EditDatas.ModeEdit = FALSE;
    EditDatas.ElementUnderEdit = NULL;
    EditDatas.NumElementSelectedInToolBar = -1;
    LoadElementProperties(NULL);
    DrawRungs();
    refresh_label_comment( );
    autorize_prevnext_buttons(TRUE);
}

void ApplyRungEdited()
{
    int PrevNew;
    int NextNew;
    save_label_comment_edited();
    CopyRungToRung(&EditDatas.Rung,&RungArray[EditDatas.NumRung]);
    ApplyNewArithmExpr();

    /* if we have added or inserted, we will have to */
    /* modify the links between rungs */
    /* order is important when setting all this, if multi-tasking : */
    /* calc.c is using only .NextRung to updates all the rungs. */
    /* and also global FirstRung & LastRung to start and end */
    switch(EditDatas.DoBeforeFinalCopy)
    {
        /* Add rung edited after current rung */
        case MODE_ADD:
            PrevNew = InfosGene->CurrentRung;
            NextNew = RungArray[InfosGene->CurrentRung].NextRung;
            RungArray[EditDatas.NumRung].PrevRung = PrevNew;
            /* new added is not the last now ? */
            if (InfosGene->CurrentRung!=InfosGene->LastRung)
            {
                RungArray[EditDatas.NumRung].NextRung = NextNew;
                RungArray[NextNew].PrevRung = EditDatas.NumRung;
            }
            RungArray[PrevNew].NextRung = EditDatas.NumRung;
            /* we can now update LastRung */
            if (InfosGene->CurrentRung==InfosGene->LastRung)
            {
                InfosGene->LastRung = EditDatas.NumRung;
            }
            /* Here is the new current ! */
            InfosGene->CurrentRung = EditDatas.NumRung;
            UpdateVScrollBar();
            break;

        /* Insert rung edited before current rung */
        case MODE_INSERT:
            PrevNew = RungArray[InfosGene->CurrentRung].PrevRung;
            NextNew = InfosGene->CurrentRung;
            RungArray[EditDatas.NumRung].NextRung = NextNew;
            /* new added is not the first now ? */
            if (InfosGene->CurrentRung!=InfosGene->FirstRung)
            {
                RungArray[EditDatas.NumRung].PrevRung = PrevNew;
                RungArray[PrevNew].NextRung = EditDatas.NumRung;
            }
            else
            {
                InfosGene->FirstRung = EditDatas.NumRung;
            }
            RungArray[NextNew].PrevRung = EditDatas.NumRung;
            /* Here is the new current ! */
            InfosGene->CurrentRung = EditDatas.NumRung;
            UpdateVScrollBar();
            break;
    }

    /* save infos for the current section */
    SectionArray[ InfosGene->CurrentSection ].FirstRung = InfosGene->FirstRung;
    SectionArray[ InfosGene->CurrentSection ].LastRung = InfosGene->LastRung;

    EditDatas.ModeEdit = FALSE;
    EditDatas.ElementUnderEdit = NULL;
    EditDatas.NumElementSelectedInToolBar = -1;
    LoadElementProperties(NULL);
    DrawRungs();
    autorize_prevnext_buttons(TRUE);
}

/* When we fall on an "unusable" element of a big one,
   we return new posix and posiy of the "alive" block */
void CheckForBlocksOfBigElement(int * PosiX,int * PosiY)
{
    int ScanX,ScanY;
    int ScanForRule;
    /* on an "unusable" ? */
    if (EditDatas.Rung.Element[*PosiX][*PosiY].Type==ELE_UNUSABLE)
    {
        /* we now will have to check for all the "alive" block of
           bigs elements */
        for (ScanY=0;ScanY<RUNG_HEIGHT;ScanY++)
        {
            for(ScanX=0;ScanX<RUNG_WIDTH;ScanX++)
            {
                ScanForRule = 0;
                do
                {
                    /* Is is an element with a rule ? */
                    if (EditDatas.Rung.Element[ScanX][ScanY].Type == RulesForSpecialElements[ScanForRule][TYPEELERULE])
                    {
                        /* Have we clicked in it ? */
                        if ( (*PosiX>=ScanX-RulesForSpecialElements[ScanForRule][XSIZEELERULE]-1)
                            && (*PosiX<=ScanX)
                            && (*PosiY>=ScanY)
                            && (*PosiY<=ScanY+RulesForSpecialElements[ScanForRule][YSIZEELERULE]-1) )
                        {
                            /* We've got it ! */
                            /* We make as we have clicked on the "alive" block ! */
                            *PosiX = ScanX;
                            *PosiY = ScanY;
                            return;
                        }
                    }
                    ScanForRule++;
                }
                while(RulesForSpecialElements[ScanForRule][0]!=-1);
            }
        }
    }
}

int VerifyRulesForElement(short int NumEle,int PosiX,int PosiY)
{
    int RulePass;
    int ItIsOk = TRUE;
    int ItIsAnOutputEle = FALSE;

    if ( (NumEle==ELE_OUTPUT) || (NumEle==ELE_OUTPUT_NOT)
            || (NumEle==ELE_OUTPUT_SET) || (NumEle==ELE_OUTPUT_RESET)
            || (NumEle==ELE_OUTPUT_JUMP) || (NumEle==ELE_OUTPUT_CALL)
            || (NumEle==ELE_OUTPUT_OPERATE) )
        ItIsAnOutputEle = TRUE;

    /* verify for outputs if we are under output zone (right column) */
    if ( (PosiX==RUNG_WIDTH-1) && !ItIsAnOutputEle )
    {
            ItIsOk = FALSE;
    }
    /* verify for inputs if we are under input zone (not right column) */
    if ( (PosiX<RUNG_WIDTH-1) && ItIsAnOutputEle )
    {
            ItIsOk = FALSE;
    }

    /* verify if for elements bigger than one block it will fit */
    RulePass = 0;
    do
    {
        if ( RulesForSpecialElements[RulePass][TYPEELERULE] == NumEle )
        {
            if ( (PosiX-RulesForSpecialElements[RulePass][XSIZEELERULE]+1 < 0)
                || (PosiY+RulesForSpecialElements[RulePass][YSIZEELERULE]-1 >= RUNG_HEIGHT) )
                ItIsOk = FALSE;
        }
        RulePass++;
    }
    while(RulesForSpecialElements[RulePass][0]!=-1);

    /* verify if we are not on another part of a big element ! */
    if (EditDatas.Rung.Element[PosiX][PosiY].Type==ELE_UNUSABLE)
        ItIsOk = FALSE;

    return ItIsOk;
}

/* used for destroying or adding a big element */
/* when destroying : filling all the blocks with ELE_FREE */
/* when adding : filling all the blocks with ELE_UNUSABLE */
/* the block "alive" is written elsewhere */
void CleanForBigElement(short int NumEle,int PosiX,int PosiY,short int FillWithThis)
{
    int RulePass;
    int PassX,PassY;
    RulePass = 0;
    do
    {
        if (RulesForSpecialElements[RulePass][TYPEELERULE] == NumEle )
        {
            for (PassX = PosiX - RulesForSpecialElements[RulePass][XSIZEELERULE] +1 ; PassX<=PosiX ; PassX++)
            {
                for (PassY = PosiY ; PassY<=PosiY + RulesForSpecialElements[RulePass][YSIZEELERULE] -1 ; PassY++)
                {
                    CheckForFreeingArithmExpr(PassX,PassY);
                    EditDatas.Rung.Element[PassX][PassY].Type = FillWithThis;
                    EditDatas.Rung.Element[PassX][PassY].ConnectedWithTop = 0;
                    EditDatas.Rung.Element[PassX][PassY].DynamicOutput = 0;
                }
            }
        }
        RulePass++;
    }
    while(RulesForSpecialElements[RulePass][0]!=-1);
}

void CheckBigElementTopLeft(short int NumEle,int * PosiX)
{
    int RulePass;
    RulePass = 0;
    do
    {
        if (RulesForSpecialElements[RulePass][TYPEELERULE] == NumEle )
        {
            *PosiX = *PosiX + RulesForSpecialElements[RulePass][XSIZEELERULE] - 1;
        }
        RulePass++;
    }
    while(RulesForSpecialElements[RulePass][0]!=-1);
}

/* return TRUE if contact or coil element */
int IsASimpleElement(int Element)
{
    int Res = FALSE;
    if ( (Element==ELE_INPUT) || (Element==ELE_INPUT_NOT)
          || (Element==ELE_RISING_INPUT) || (Element==ELE_FALLING_INPUT)
          || (Element==ELE_OUTPUT) || (Element==ELE_OUTPUT_NOT)
          || (Element==ELE_OUTPUT_SET) || (Element==ELE_OUTPUT_RESET)
          || (Element==ELE_OUTPUT_JUMP) || (Element==ELE_OUTPUT_CALL) )
        Res = TRUE;
    return Res;
}


/* click with the mouse in x and y pixels of the rung */
void EditElementInRung(double x,double y)
{
    int RungX,RungY;
    short int NumElement;
    /* correspond to which block ? */
    RungX = x/InfosGene->BlockWidth;
    RungY = y/InfosGene->BlockHeight;
    if ( (RungX<RUNG_WIDTH) && (RungY<RUNG_HEIGHT)
        && (EditDatas.NumElementSelectedInToolBar!=-1) )
    {
        /* check for "unusable" blocks */
        if (EditDatas.NumElementSelectedInToolBar==EDIT_POINTER)
            CheckForBlocksOfBigElement(&RungX,&RungY);
        if (EditDatas.NumElementSelectedInToolBar<ELE_UNUSABLE)
        {
            NumElement = EditDatas.NumElementSelectedInToolBar;
            /* For big element, click insert top-left of the element */
            CheckBigElementTopLeft(NumElement,&RungX);
            /* already the same element ? => if yes kill it! */
            if (NumElement==EditDatas.Rung.Element[RungX][RungY].Type)
            {
                NumElement = ELE_FREE;
            }
            /* apply the new element */
            if (NumElement==ELE_FREE)
            {
                /* the blocks other than the "alive" are now free... */
                CleanForBigElement(EditDatas.Rung.Element[RungX][RungY].Type,RungX,RungY,ELE_FREE);
                EditDatas.Rung.Element[RungX][RungY].DynamicOutput = 0;
                EditDatas.Rung.Element[RungX][RungY].Type = NumElement;
            }
            else
            {
                if (VerifyRulesForElement(NumElement,RungX,RungY))
                {
                    /* the blocks other than the "alive" are now free... */
                    CleanForBigElement(EditDatas.Rung.Element[RungX][RungY].Type,RungX,RungY,ELE_FREE);
                    /* for big element with only one block "alive" we must mark the others */
                    /* blocks as unusable */
                    CleanForBigElement(NumElement,RungX,RungY,ELE_UNUSABLE);
                    // if not a simple element (contact/coil) replaced by a simple element, clean VarNum
                    if ( !(IsASimpleElement(NumElement) && IsASimpleElement(EditDatas.Rung.Element[RungX][RungY].Type)) )
                    {
                        EditDatas.Rung.Element[RungX][RungY].VarNum = 0;
                    }
                    EditDatas.Rung.Element[RungX][RungY].Type = NumElement;
                    CheckForAllocatingArithmExpr(RungX,RungY);
                }
            }
        }
        if (EditDatas.NumElementSelectedInToolBar==EDIT_CNX_WITH_TOP)
        {
            if (EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop)
            {
                EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop = 0;
            }
            else
            {
                if (RungY>0)
                    EditDatas.Rung.Element[RungX][RungY].ConnectedWithTop = 1;
            }
        }
        if (EditDatas.NumElementSelectedInToolBar==EDIT_LONG_CONNECTION)
        {
            int ScanX = RungX;
            while(EditDatas.Rung.Element[ScanX][RungY].Type==ELE_FREE && ScanX<RUNG_WIDTH-1 )
            {
                EditDatas.Rung.Element[ScanX++][RungY].Type = ELE_CONNECTION;
            }
        }
        LoadElementProperties(&EditDatas.Rung.Element[RungX][RungY]);
        EditDatas.ElementUnderEdit = &EditDatas.Rung.Element[RungX][RungY];
    }
}

/* click with the mouse in x and y pixels of the section display */
void EditElementInThePage(double x,double y)
{
    int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
    if ( iCurrentLanguage==SECTION_IN_LADDER )
    {
        if ( ( y >= InfosGene->OffsetCurrentRungDisplayed ) && ( y < InfosGene->OffsetCurrentRungDisplayed+InfosGene->BlockHeight*RUNG_HEIGHT ) )
            EditElementInRung( x, y - InfosGene->OffsetCurrentRungDisplayed );
    }
#ifdef SEQUENTIAL_SUPPORT
    if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
        EditElementInSeqPage( x+InfosGene->HScrollValue, y+InfosGene->VScrollValue );
#endif
}

void CopyRungToRung(StrRung * RungSrc,StrRung * RungDest)
{
    memcpy(RungDest,RungSrc,sizeof(StrRung));
}
