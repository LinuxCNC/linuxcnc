/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */

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

/* if GTK not included before */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define RELEASE_VER_STRING "0.7.100"
#define RELEASE_DATE_STRING "4 November 2006"

// defaults values
#define NBR_RUNGS_DEF 100
#define NBR_BITS_DEF 500
#define NBR_WORDS_DEF 100
#define NBR_TIMERS_DEF 10
#define NBR_MONOSTABLES_DEF 10
#define NBR_COUNTERS_DEF 10
#define NBR_PHYS_INPUTS_DEF 50
#define NBR_PHYS_OUTPUTS_DEF 50
#define NBR_ARITHM_EXPR_DEF 100
#define NBR_SECTIONS_DEF 10
#define NBR_SYMBOLS_DEF 100

#define NBR_INPUTS_CONF 5
#define NBR_OUTPUTS_CONF 5

#ifdef DYNAMIC_PLCSIZE
typedef struct plc_sizeinfo_s {
	int	nbr_rungs;
	int	nbr_bits;
	int	nbr_words;
	int	nbr_timers;
	int	nbr_monostables;
	int	nbr_counters;
	int	nbr_phys_inputs;
	int	nbr_phys_outputs;
	int	nbr_arithm_expr;
	int	nbr_sections;
	int nbr_symbols;
}plc_sizeinfo_s;

#define NBR_RUNGS 	 InfosGene->SizesInfos.nbr_rungs
#define NBR_BITS 	 InfosGene->SizesInfos.nbr_bits
#define NBR_WORDS	 InfosGene->SizesInfos.nbr_words
#define NBR_TIMERS 	 InfosGene->SizesInfos.nbr_timers
#define NBR_MONOSTABLES  InfosGene->SizesInfos.nbr_monostables
#define NBR_COUNTERS	InfosGene->SizesInfos.nbr_counters
#define NBR_PHYS_INPUTS  InfosGene->SizesInfos.nbr_phys_inputs
#define NBR_PHYS_OUTPUTS InfosGene->SizesInfos.nbr_phys_outputs
#define NBR_ARITHM_EXPR  InfosGene->SizesInfos.nbr_arithm_expr
#define NBR_SECTIONS 	 InfosGene->SizesInfos.nbr_sections
#define NBR_SYMBOLS		InfosGene->SizesInfos.nbr_symbols

#else
/* static size */
#define NBR_RUNGS NBR_RUNGS_DEF
#define NBR_BITS NBR_BITS_DEF
#define NBR_WORDS NBR_WORDS_DEF
#define NBR_TIMERS NBR_TIMERS_DEF
#define NBR_MONOSTABLES NBR_MONOSTABLES_DEF
#define NBR_COUNTERS NBR_COUNTERS_DEF
#define NBR_PHYS_INPUTS NBR_PHYS_INPUTS_DEF
#define NBR_PHYS_OUTPUTS NBR_PHYS_OUTPUTS_DEF
#define NBR_ARITHM_EXPR NBR_ARITHM_EXPR_DEF
#define NBR_SECTIONS NBR_SECTIONS_DEF
#define NBR_SYMBOLS NBR_SYMBOLS_DEF
#endif

#define ARITHM_EXPR_SIZE 32

#ifdef MAT_CONNECTION
#define TYPE_FOR_BOOL_VAR plc_pt_t
#else
#define TYPE_FOR_BOOL_VAR char
#endif

#define TIME_REFRESH_RUNG_MS 100
#define TIME_BASE_MINS 60000
#define TIME_BASE_SECS 1000
#define TIME_BASE_100MS 100

/* numbers of blocks in a rung */
#define RUNG_WIDTH 10
#define RUNG_HEIGHT 6

/* size in pixels of rungs blocks (default) */
//#define BLOCK_WIDTH_DEF 32
#define BLOCK_WIDTH_DEF 48
#define BLOCK_HEIGHT_DEF 32

/* offsets in pixels */
#define OFFSET_X 4
#define OFFSET_Y 6
/* size of lines activated (comment to not use) */
#define THICK_LINE_ELE_ACTIVATED 3

/* elements in the rungs */
#define ELE_FREE 0
#define ELE_INPUT 1
#define ELE_INPUT_NOT 2
#define ELE_RISING_INPUT 3
#define ELE_FALLING_INPUT 4
#define ELE_CONNECTION 9
#define ELE_TIMER 10
#define ELE_MONOSTABLE 11
#define ELE_COUNTER 12
#define ELE_COMPAR 20
#define ELE_OUTPUT 50
#define ELE_OUTPUT_NOT 51
#define ELE_OUTPUT_SET 52
#define ELE_OUTPUT_RESET 53
#define ELE_OUTPUT_JUMP 54
#define ELE_OUTPUT_CALL 55
#define ELE_OUTPUT_OPERATE 60
/* for complex elements using many blocks : only one block
   is "alive", others are marked as UNUSABLE */
#define ELE_UNUSABLE 99


/* used only for edit */
#define EDIT_CNX_WITH_TOP 100
#define EDIT_POINTER 101
#define EDIT_LONG_CONNECTION 102
#define EDIT_ERASER 103


/* booleans */
#define VAR_MEM_BIT 00
#define VAR_TIMER_DONE 10
#define VAR_TIMER_RUNNING 11
#define VAR_MONOSTABLE_RUNNING 20
#define VAR_COUNTER_DONE 25
#define VAR_COUNTER_EMPTY 26
#define VAR_COUNTER_FULL 27
#define VAR_STEP_ACTIVITY 30
#define VAR_PHYS_INPUT 50
#define VAR_PHYS_OUTPUT 60
#define VAR_ARE_WORD 199    /* after it, all vars are no more booleans */
/* integers */
#define VAR_MEM_WORD 200
#define VAR_STEP_TIME 220
#define VAR_TIMER_PRESET 230
#define VAR_TIMER_VALUE 231
#define VAR_MONOSTABLE_PRESET 240
#define VAR_MONOSTABLE_VALUE 241
#define VAR_COUNTER_PRESET 250
#define VAR_COUNTER_VALUE 251

#define BASE_MINS 0
#define BASE_SECS 1
#define BASE_100MS 2

#define TIME_UPDATE_GTK_DISPLAY_MS 100

// attribute separator for variable names.
//#define VAR_ATTRIBUTE_SEP ','
#define VAR_ATTRIBUTE_SEP '.'

typedef struct StrElement
{
    short int Type;
    char ConnectedWithTop;
    int VarType;
    int VarNum;     /* or NumRung (for jump), NumTimer, NumMonostable,... */

    char DynamicInput;
    char DynamicState;
    char DynamicVarBak; /* used for rising/falling edges */
    char DynamicOutput;
}StrElement;

#define LGT_LABEL 10
#define LGT_COMMENT 30
typedef struct StrRung
{
    int Used;
    int PrevRung;
    int NextRung;
    char Label[LGT_LABEL];
    char Comment[LGT_COMMENT];
    StrElement Element[RUNG_WIDTH][RUNG_HEIGHT];
}StrRung;

typedef struct StrTimer
{
    int Preset;
    int Value;
    int Base;
    char DisplayFormat[10];
    char InputEnable;
    char OutputDone;
    char OutputRunning;
}StrTimer;

typedef struct StrMonostable
{
    int Preset;
    int Value;
    int Base;
    char DisplayFormat[10];
    char Input;
    char InputBak;
    char OutputRunning;
}StrMonostable;

typedef struct StrCounter
{
    int Preset;
    int Value;
    int ValueBak;
    char InputReset;
    char InputPreset;
    char InputCountUp;
    char InputCountUpBak;
    char InputCountDown;
    char InputCountDownBak;
    char OutputDone;
    char OutputEmpty;
    char OutputFull;
}StrCounter;

typedef struct StrArithmExpr
{
    char Expr[ARITHM_EXPR_SIZE];
}StrArithmExpr;

#define DEVICE_TYPE_DIRECT_ACCESS 0	/* used inb( ) and outb( ) calls */
#define DEVICE_TYPE_COMEDI 100	/* /dev/comedi0 and following */

typedef struct StrIOConf
{
    int FirstClassicLadderIO;	/* -1 : not used */
    int DeviceType;		/* a comedi device or type direct I/O access */
    int SubDevOrAdr;	/* comedi sub-device or I/O port address */
    int FirstChannel;
    int NbrConsecutivesChannels;
    int FlagInverted;
}StrIOConf;

typedef struct StrInfosGene
{
    int FirstRung;
    int CurrentRung;
    int LastRung;
    int LadderState;
    char CmdRefreshVarsBits;

    int BlockWidth;
    int BlockHeight;
    int PageWidth;
    int PageHeight;
    int TopRungDisplayed;
    int OffsetHiddenTopRungDisplayed;
    int OffsetCurrentRungDisplayed;
    int HScrollValue;
    int VScrollValue;

    /* how time for the last scan of the rungs in ns (if calc on RTLinux side) */
    int DurationOfLastScan;

    int CurrentSection;

#ifdef DYNAMIC_PLCSIZE
    plc_sizeinfo_s SizesInfos;
#endif

    StrIOConf InputsConf[ NBR_INPUTS_CONF ];
    StrIOConf OutputsConf[ NBR_OUTPUTS_CONF ];

	char AskConfirmationToQuit;
	char DisplaySymbols;
}StrInfosGene;

/* Differents states of Ladder */
#define STATE_LOADING 0
#define STATE_STOP 1
#define STATE_RUN 2

typedef struct StrEditRung
{
    StrRung Rung;
    char ModeEdit;
    int NumRung;
    char DoBeforeFinalCopy;
    short int NumElementSelectedInToolBar;
    StrElement * ElementUnderEdit;
}StrEditRung;

#define NBR_PARAMS_PER_OBJ 3

#define NBR_TIMEBASES 3
typedef struct StrDatasForBases
{
    int Id;
    int ValueInMS;
    char * DisplayFormat;
    char * ParamSelect;
}StrDatasForBase;

#define SECTION_IN_LADDER 0
#define SECTION_IN_SEQUENTIAL 1

#define LGT_SECTION_NAME 20
typedef struct StrSection
{
    char Used;
    char Name[ LGT_SECTION_NAME ];
    int Language; /* SECTION_IN_ */
    /* -1 if not a sub-routine, else sub-routine number used for the calls */
    int SubRoutineNumber;
    /* if section is in Ladder */
    int FirstRung;
    int LastRung;
    /* if section is in Sequential */
    int SequentialPage;
}StrSection;

#define LGT_VAR_NAME 10
#define LGT_SYMBOL_STRING 10
#define LGT_SYMBOL_COMMENT 30
typedef struct StrSymbol
{
	char VarName[ LGT_VAR_NAME ];
	char Symbol[ LGT_SYMBOL_STRING ];
	char Comment[ LGT_SYMBOL_COMMENT ];
}StrSymbol;

#ifdef SEQUENTIAL_SUPPORT
#include "sequential.h"
#define SIZE_VAR_ARRAY (NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS+NBR_STEPS)
#define SIZE_VAR_WORD_ARRAY (NBR_WORDS+NBR_STEPS)
#else
#define SIZE_VAR_ARRAY (NBR_BITS+NBR_PHYS_INPUTS+NBR_PHYS_OUTPUTS)
#define SIZE_VAR_WORD_ARRAY (NBR_WORDS)
#endif

void DoPauseMilliSecs( int Time );

void InitAllLadderDatas( char NoScreenRefresh );
int ClassicLadderAllocAll(void);
void ClassicLadderFreeAll(void);

#ifdef __RTL__
#include <rtl_printf.h>
#define debug_printf rtl_printf
#endif

#if defined( RTAI ) && defined( MODULE )
#define debug_printf rt_printk
#endif

#ifndef MODULE
#define debug_printf printf
#endif

#ifdef HAL_SUPPORT
#include "rtapi.h"
#include "hal.h"
extern int compId;
#endif
extern int nogui;
