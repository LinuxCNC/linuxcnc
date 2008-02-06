// classicladder v7.124 adaptation for emc2 January 08
/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */

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

// this is a collection of completely new functions added for the adaptation to EMC2
// it is easier to keep them all together in one place --I hope!
// please add all new functions here!

#include "hal.h"
#include "hal/hal_priv.h"

#include <stdio.h>
#include "classicladder.h"
#include "global.h"
#include "symbols.h"
#include "vars_names.h"
#include "emc_mods.h"

// This function is a development of Jeff's orginal work for ver 7.100
// It is called from the GetElementPropertiesForStatusBar function in edit.c 
// it checks to see if the VarsNameParam is a symbol, if it is, converts it to a variable name
// if not I, Q, or W then exports that signal name is not possible for B variable
// then checks for what the pinname would be if it is a I, Q, or W  variable (these are the only variable that connect to the outside world)
// then checks to see if a signal is connected to that pin
// if there is a signal exports that name with an arrow to show if the signal is coming in or going out
// if there is such a pin but there is no signal connected export that fact
// anything else should export an error
 
char * ConvVarNameToHalSigName( char * VarNameParam ) 
{
	
	        
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
                break;
            case 'Q':
                sscanf(VarNameParam+2, "%d", &idx);
                snprintf(pin_name, 100, "classicladder.0.out-%02d", idx);
                arrowside = 0;
                break;
            case 'W':
                sscanf(VarNameParam+2, "%d", &idx);
                if(idx > InfosGene->GeneralParams.SizesInfos.nbr_s32in) {
                    snprintf(pin_name, 100, "classicladder.0.s32out-%02d",
                            idx - InfosGene->GeneralParams.SizesInfos.nbr_s32in);
                    arrowside = 0;
                } else {
                    snprintf(pin_name, 100, "classicladder.0.s32in-%02d", idx);
                    arrowside = 1;
                	}
                break;
	   case 'B':
		return "no HAL signal possible";
		break;
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
			if (!pin->signal) {return "no HAL signal connected";  }
			}
}
                
            
		
return "ERROR";
}
