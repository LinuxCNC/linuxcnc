// classicladder v7.124 adaptation for emc2 January 08

// this is a collection of completely new functions added for the adaptation to EMC2
// it is easier to keep them all together in one place --I hope!
// please add all new functions here!

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


#include "hal.h"
#include "hal_priv.h"

#include <stdio.h>
#include <string.h>
#include "classicladder.h"
#include "global.h"
#include "symbols.h"
#include "vars_names.h"
#include "emc_mods.h"



// This function is a development of Jeff's orginal work for ver 7.100
// it converts variables or symbols to HAL signal names (if present)

// It is called from the GetElementPropertiesForStatusBar function in edit.c 
// it checks to see if the VarsNameParam is a symbol, if it is, converts it to a variable name
// if Variable is B or W then exports that it is a memory variable 
//also checks to see if in the range of max number of word variables loaded by realtime
// if not I, Q, QW, or IW  then returns an error message 
// then checks for what the pinname would be if it is a I, Q, IW or QW  variable 
// (these are the only variables that can connect to the outside world)
// if I or Q checks to see it is really QW or QI then checks
// if in the range of number of HAL pins loaded by realtime module
// then checks to see if a signal is connected to that pin
// if there is a signal returns that name with an arrow to show if the signal is coming in or going out
// if there is no signal connected returns that fact
// anything else should return an error
// edit.c uses this function 
// Chris Morley NOV 08


char * ConvVarNameToHalSigName( char * VarNameParam ) 
{
    if( (VarNameParam[0]=='\0') ||  (VarNameParam[0]==' ')) {return "Error blank symbol name";}
    if( VarNameParam[0] != '%') {VarNameParam= ConvSymbolToVarName(VarNameParam);}
    if( VarNameParam[0] == '%'){
        char pin_name[100] = {0};
        int arrowside=0;
        int idx;

        switch(VarNameParam[1]) {
            case 'I':
                switch(VarNameParam[2]) {
                    case 'W':
                        sscanf(VarNameParam+3, "%d", &idx);
                        if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_phys_words_inputs) {return "out of bounds variable number";}
                        snprintf(pin_name, 100, "classicladder.0.s32in-%02d", idx);
                        arrowside = 1;
                        break;
                    case 'F':
                        sscanf(VarNameParam+3, "%d", &idx);
                        if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_phys_float_inputs) {return "out of bounds variable number";}
                        snprintf(pin_name, 100, "classicladder.0.floatin-%02d", idx);
                        arrowside = 1;
                        break;
                    default:
                        sscanf(VarNameParam+2, "%d", &idx);
                        snprintf(pin_name, 100, "classicladder.0.in-%02d", idx);
                        arrowside = 1;
                        if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_phys_inputs) {return "out of bounds variable number";}
                        break;
                }
                break;
            case 'Q':
                switch(VarNameParam[2]) {
                    case 'W':
                        sscanf(VarNameParam+3, "%d", &idx);
                        if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_phys_words_outputs) {return "out of bounds variable number";}
                        snprintf(pin_name, 100, "classicladder.0.s32out-%02d", idx);
                        arrowside = 0;
                        break;
                    case 'F':
                        sscanf(VarNameParam+3, "%d", &idx);
                        if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_phys_float_outputs) {return "out of bounds variable number";}
                        snprintf(pin_name, 100, "classicladder.0.floatout-%02d", idx);
                        arrowside = 0;
                        break;
                    default:
                        sscanf(VarNameParam+2, "%d", &idx);
                        snprintf(pin_name, 100, "classicladder.0.out-%02d", idx);
                        arrowside = 0;
                        if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_phys_outputs) {return "out of bounds variable number";}
                        break;
                }
                break;
            case 'W':
                sscanf(VarNameParam+2, "%d", &idx);
                if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_words) {return "out of bounds variable number";}
                return "None -Internal Memory";
            case 'B':
                sscanf(VarNameParam+2, "%d", &idx);
                if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_bits) {return "out of bounds variable number";}
                return "None -Internal Memory";
            case 'E':
                sscanf(VarNameParam+2, "%d", &idx);
                if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_error_bits) {return "out of bounds variable number";}
                return "None -Internal Error Status";
            case 'T':
            case 'C':
            case 'M':
            case 'X':
                return "None";
            default:
                return "error";
        }

        if(*pin_name) {
            hal_pin_t *pin = halpr_find_pin_by_name(pin_name);
            if(pin && pin->signal) {
                hal_sig_t *sig = SHMPTR(pin->signal);
                if(sig->name) {
                    static char sig_name[100];
                    // char *arrow = "\xe2\x86\x90";
                    char *arrow = "\xe2\x87\x92";

                    if(arrowside == 0) {
                        snprintf(sig_name, 100, "%s%s", sig->name, arrow);
                    } else {
                        snprintf(sig_name, 100, "%s%s", arrow, sig->name);
                    }

                    return sig_name;
                }
            }
            if (pin && !pin->signal) {return "no signal connected";  }
        }
    }

    return "Conv. HAL signal ERROR";
}

// function to check for first Variable in an arithmetic expression
// ultimately so we can check for a HAL signal name
// It finds the first Variable or symbol name
// sends that to ConvVarNameToHalSigName() which returns status or
// name of a HAL signal connected .
// then we piece together the signal name, first variable/symbolname
// and  expression so it can be returned to be printed in the status bar
// Edit.c uses this function
// Chris Morley Feb 08

char * FirstVariableInArithm(char * Expr)
{
		static char Buffer[100];
		static char Tempbuf[100];
		char * Ptr = Expr;
		int i;
		Buffer[0] = '\0';

	if (Expr[0]=='\0' || Expr[0]=='#')
		return "No expression";
	
//parse expression till we find a symbol that marks the end of a variable or symbol name , or find the end of expression

	for (i=0;i<100;i++)
	{

		switch (Ptr[i])
		{
			case ':' :
			case '=' :
			case '\0':
			case '&' :
			case '!' :
			case '|' :
			case '[' :
			case '(' :

			snprintf(Buffer, i+1, "%s", Expr);
			snprintf(Tempbuf, 100, " %s (for %s)  Exprsn~ %s",ConvVarNameToHalSigName(Buffer),Buffer,Expr);
			return Tempbuf;
			break;

			default:;
		}
	}
	return "first var. expression error";
}

//auto assignment of names for symbols of s32in,s32out, bitin 
//and bitout if not already assigned a name
// you must assign s32in before s32out unless there are no s32in
void SymbolsAutoAssign (void)
{
       
	enum pinnames
		{BITINS=0,BITOUTS,BITS,WORDS,S32INS,S32OUTS,FLOATINS,FLOATOUTS,TIMERS,IEC_TIMERS,MONOS,COUNTERS,ERRORS};
#define NUMVARTYPES 13
	int scansymb,found = FALSE,i,v,numofvariable;
	char Buffer[30],SymbolBuf[5],CommentBuf[40]="";
	
        for (v=0;v<NUMVARTYPES;v++)
        {
	        switch(v)
	        {
			case BITINS:
                                    numofvariable= NBR_PHYS_INPUTS ;
                                    strcpy(SymbolBuf,"I");
                                    break;
			case BITOUTS:
                                    numofvariable= NBR_PHYS_OUTPUTS;
                                    strcpy(SymbolBuf,"Q");
                                    break;
                        case BITS:
                                    numofvariable= NBR_BITS;
                                    strcpy(SymbolBuf,"B");
                                    break;
                        case WORDS:
                                    numofvariable= NBR_WORDS;
                                    strcpy(SymbolBuf,"W");
                                    break;
			case S32INS:
                                    numofvariable= NBR_PHYS_WORDS_INPUTS ;
                                    strcpy(SymbolBuf,"IW");
                                    break;
			case S32OUTS:
                                    numofvariable= NBR_PHYS_WORDS_OUTPUTS;
                                    strcpy(SymbolBuf,"QW");
                                    break;
                        case FLOATINS:
                                    numofvariable= NBR_PHYS_FLOAT_INPUTS ;
                                    strcpy(SymbolBuf,"IF");
                                    break;
			case FLOATOUTS:
                                    numofvariable= NBR_PHYS_FLOAT_OUTPUTS;
                                    strcpy(SymbolBuf,"QF");
                                    break;
		        case TIMERS:
                                    numofvariable= NBR_TIMERS;
                                    strcpy(SymbolBuf,"T");
                                    strcpy(CommentBuf,"Old Timer");
                                    break;
                        case IEC_TIMERS:
                                    numofvariable= NBR_TIMERS_IEC ;
                                    strcpy(SymbolBuf,"TM");
                                    strcpy(CommentBuf,"New Timer");
                                    break;
                        case MONOS:
                                    numofvariable= NBR_MONOSTABLES;
                                    strcpy(SymbolBuf,"M");
                                    strcpy(CommentBuf,"One-shot");
                                    break;
			case COUNTERS:
                                    numofvariable= NBR_COUNTERS;
                                    strcpy(SymbolBuf,"C");
                                    strcpy(CommentBuf,"Counter");
                                    break;
		        case ERRORS:
                                    numofvariable= NBR_ERROR_BITS;
                                    strcpy(SymbolBuf,"E");
                                    strcpy(CommentBuf,"Error Flag Bit");
                                    break;
			default : 
                                 rtapi_print_msg(RTAPI_MSG_ERR, "Cannot auto assign symbol names-wrong variable name");
                                 return;
	        }
         //printf("symbol-%s number-%i\n",SymbolBuf,v);
	        for (i=0;i<numofvariable;i++)
	
                 {
   	                  found = FALSE;
	                //set buffer to variable to check variable name
    	                strcpy(Buffer,"");
   	                 sprintf(Buffer,"%%%s%d",SymbolBuf,i);  

	               // printf("%s\n",Buffer);
		        scansymb=0;	
	                // scan all symbol variables
		        while ( scansymb<NBR_SYMBOLS  ) 
		                { 
	                        // check for exsisting variable/symbol name
		                if (strcmp(SymbolArray[ scansymb ].VarName, Buffer) == FALSE)
			                {
				                found = TRUE ; // already a symbol for this variable
				                break;// stop looking then
			                }
	                        scansymb ++;// check the rest
	                        }
	
	                  scansymb=0;
                         // this assigns a symbol to an unassigned variable
                         // while there is no symbol already assigned and we are not at the
                         // end of the symbol data..
	                 while ( found == FALSE && scansymb<NBR_SYMBOLS)
	                      {
	                            // look for an empty spot...
		                    if (SymbolArray[ scansymb ].VarName[ 0 ] == '\0')
		                       { 
                                           //copy variable name already in Buffer to VarName array
			                  strcpy( SymbolArray[scansymb].VarName, Buffer ); 
	
		                           //put a symbol name (and it's number) in buffer
		        	          strcpy( SymbolArray[scansymb].Symbol, Buffer );
	    	        	        //printf("%s\n",Buffer);
                                        // copy a comment if there is one...
		        	        strcpy(Buffer,"");
		        	        sprintf(Buffer,"%s",CommentBuf);
		        	        strcpy( SymbolArray[scansymb].Comment, Buffer );
	
		       	                 break;// we are done looking
		                         }	
		                scansymb ++;// keep looking for empty spot if not done
	                         }	
                }
          }
         return;
}
