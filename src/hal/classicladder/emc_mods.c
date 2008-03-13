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
#include "hal/hal_priv.h"

#include <stdio.h>
#include "classicladder.h"
#include "global.h"
#include "symbols.h"
#include "vars_names.h"
#include "emc_mods.h"

// This function is a development of Jeff's orginal work for ver 7.100
// it converts variables or symbols to HAL signal names (if present)

// It is called from the GetElementPropertiesForStatusBar function in edit.c 
// it checks to see if the VarsNameParam is a symbol, if it is, converts it to a variable name
// if Variable is B then exports that signal name is not possible 
// if not I, W, or Q then returns an error message 
// then checks for what the pinname would be if it is a I, Q, or W  variable 
// (these are the only variable that can connect to the outside world)
//
// if W, checks if in the range for hal pins loaded by realtime
// (S32 in and out pins piggy back on top of the regular word-variable array so in the array,
// S32in are first, then S32out, then word variables)
// also checks to see if in the range of max number of word variables loaded by realtime
//
// if I or Q checks to see if in the range of number of HAL pins loaded by realtime module
//
// then checks to see if a signal is connected to that pin
// if there is a signal returns that name with an arrow to show if the signal is coming in or going out
// if there is such a pin but there is no signal connected returns that fact
// anything else should return an error
// edit.c uses this function 
// Chris Morley Feb 08


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
                sscanf(VarNameParam+2, "%d", &idx);
                snprintf(pin_name, 100, "classicladder.0.in-%02d", idx);
                arrowside = 1;
		if((idx+1) > InfosGene->GeneralParams.SizesInfos.nbr_phys_inputs) {return "out of bounds variable number";}
                break;
            case 'Q':
                sscanf(VarNameParam+2, "%d", &idx);
                snprintf(pin_name, 100, "classicladder.0.out-%02d", idx);
                arrowside = 0;
		if((idx+1) > InfosGene->GeneralParams.SizesInfos.nbr_phys_outputs) {return "out of bounds variable number";}
                break;
            case 'W':
                sscanf(VarNameParam+2, "%d", &idx);
		if((idx) >= InfosGene->GeneralParams.SizesInfos.nbr_words) {return "out of bounds variable number";}
		if(idx >= (InfosGene->GeneralParams.SizesInfos.nbr_s32in + InfosGene->GeneralParams.SizesInfos.nbr_s32out)) {return "None -internal memory";}
                if(idx >= InfosGene->GeneralParams.SizesInfos.nbr_s32in) {
                    snprintf(pin_name, 100, "classicladder.0.s32out-%02d",
                            idx - InfosGene->GeneralParams.SizesInfos.nbr_s32in);
                    arrowside = 0;

                } else {
                    snprintf(pin_name, 100, "classicladder.0.s32in-%02d", idx);
                    arrowside = 1;
                	}
                break;
	   case 'B':
		return "No signal for %B variables possible ";
		break;
	default:
                return "error";
		break;
            }
//printf("INFO---PINNAME-%s\n",pin_name);
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
			if (!pin->signal) {return "no signal connected";  }
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
