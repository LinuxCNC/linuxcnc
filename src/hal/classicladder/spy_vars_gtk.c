/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */
/* -------------------------------------------------------------------------------------------- */
/* Spy variables windows (booleans with checkboxes, and any with entry widgets) - GTK interface */
/* -------------------------------------------------------------------------------------------- */
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

// modified for EMC
// Chris Morley Feb 08

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "drawing.h"
#include "edit.h"
#include "classicladder_gtk.h"
#include "vars_names.h"
#include "symbols.h"

#include "spy_vars_gtk.h"

#define NBR_BOOLS_VAR_SPY 15
#define NBR_TYPE_BOOLS_SPY 3
#define NBR_FREE_VAR_SPY 15

static int toggle=0;
GtkWidget *SpyBoolVarsWindow;
GtkWidget *SpyFreeVarsWindow;

GtkWidget *offsetboolvar[ NBR_TYPE_BOOLS_SPY ];
int ValOffsetBoolVar[ NBR_TYPE_BOOLS_SPY ] = { 0, 0, 0 };
GtkWidget *chkvar[ NBR_TYPE_BOOLS_SPY ][ NBR_BOOLS_VAR_SPY ];

GtkWidget *EntryVarSpy[NBR_FREE_VAR_SPY*3], *LabelFreeVars[ NBR_FREE_VAR_SPY];

GtkTooltips * TooltipsEntryVarSpy[ NBR_FREE_VAR_SPY ];
/* defaults vars to spy list */
int VarSpy[NBR_FREE_VAR_SPY][2] = { {VAR_MEM_WORD,0}, {VAR_MEM_WORD,1}, {VAR_MEM_WORD,2}, {VAR_MEM_WORD,3}, {VAR_MEM_WORD,4}, {VAR_MEM_WORD,5}, {VAR_MEM_WORD,6}, {VAR_MEM_WORD,7}, {VAR_MEM_WORD,8}, {VAR_MEM_WORD,9}, {VAR_MEM_WORD,10}, {VAR_MEM_WORD,11}, {VAR_MEM_WORD,12}, {VAR_MEM_WORD,13}, {VAR_MEM_WORD,14} }; 
GtkWidget *DisplayFormatVarSpy[NBR_FREE_VAR_SPY];

static gint chkvar_press_event( GtkWidget      *widget, void * numcheck )
{
	long NumCheckWidget = (long)numcheck;
	int Type = VAR_MEM_BIT;
	int Offset = ValOffsetBoolVar[ 0 ];
	int NumCheck = NumCheckWidget;
	if( NumCheckWidget>=NBR_BOOLS_VAR_SPY && NumCheckWidget<2*NBR_BOOLS_VAR_SPY )
	{
		Type = VAR_PHYS_INPUT;
		Offset = ValOffsetBoolVar[ 1 ];
		NumCheck -= NBR_BOOLS_VAR_SPY;
	} 
	if( NumCheckWidget>=2*NBR_BOOLS_VAR_SPY && NumCheckWidget<3*NBR_BOOLS_VAR_SPY )
	{
		Type = VAR_PHYS_OUTPUT;
		Offset = ValOffsetBoolVar[ 2 ];
		NumCheck -= 2*NBR_BOOLS_VAR_SPY;
	} 
	if (gtk_toggle_button_get_active((GtkToggleButton *)widget))
		WriteVar(Type,Offset+NumCheck,1);
	else
		WriteVar(Type,Offset+NumCheck,0);
	return TRUE;
}

void RefreshOneBoolVar( int Type, int Num, int Val )
{
	int Col = 0;
	switch( Type )
	{
		case VAR_PHYS_INPUT: Col = 1; break;
		case VAR_PHYS_OUTPUT: Col = 2; break;
	}
	if ( Num>=ValOffsetBoolVar[ Col ] && Num<ValOffsetBoolVar[ Col ]+NBR_BOOLS_VAR_SPY )
		gtk_toggle_button_set_active((GtkToggleButton *)chkvar[Col][Num-ValOffsetBoolVar[ Col ]],(Val!=0)?TRUE:FALSE);
}

void RefreshAllBoolsVars( )
{
	int NumVar;
	for (NumVar=0; NumVar<NBR_BOOLS_VAR_SPY; NumVar++)
	{
		gtk_toggle_button_set_active( (GtkToggleButton *)chkvar[0][NumVar], ReadVar(VAR_MEM_BIT,NumVar+ValOffsetBoolVar[ 0 ])?TRUE:FALSE );
		gtk_toggle_button_set_active( (GtkToggleButton *)chkvar[1][NumVar], ReadVar(VAR_PHYS_INPUT,NumVar+ValOffsetBoolVar[ 1 ])?TRUE:FALSE );
		gtk_toggle_button_set_active( (GtkToggleButton *)chkvar[2][NumVar], ReadVar(VAR_PHYS_OUTPUT,NumVar+ValOffsetBoolVar[ 2 ])?TRUE:FALSE );
	}
}

void UpdateAllLabelsBoolsVars( )
{
	int ColumnVar, OffVar;
	for(ColumnVar=0; ColumnVar<NBR_TYPE_BOOLS_SPY; ColumnVar++)
	{
		for(OffVar=0; OffVar<NBR_BOOLS_VAR_SPY; OffVar++)
		{
			char BufNumVar[256];
			switch( ColumnVar )
			{
                        case 0:
                            snprintf(BufNumVar, 256, "<span foreground=\"black\" weight=\"bold\">%cB%d</span>",'%', 
                                    OffVar+ValOffsetBoolVar[ ColumnVar ]); 
                            break;
                        case 1:
                            snprintf(BufNumVar, 256, "<span foreground=\"red\" weight=\"bold\">%cI%d</span>",'%',  
                                    OffVar+ValOffsetBoolVar[ ColumnVar ]); 
                            break;
                        case 2:
                            snprintf(BufNumVar, 256, "<span foreground=\"blue\" weight=\"bold\">%cQ%d</span>",'%', 
                                    OffVar+ValOffsetBoolVar[ ColumnVar ]); 
                            break;
			}
			gtk_label_set_markup (GTK_LABEL (GTK_BIN( chkvar[ ColumnVar ][ OffVar ] )->child),BufNumVar);
		}
	}
}

static gint OffsetBoolVar_activate_event(GtkWidget *widget, void * NumVarSpy)
{
	int Maxi = 0;
	long NumType = (long)NumVarSpy;
	int ValOffset = atoi( gtk_entry_get_text((GtkEntry *)widget) );
	switch( NumType )
	{
		case 0: Maxi = NBR_BITS; break;
		case 1: Maxi = NBR_PHYS_INPUTS; break;
		case 2: Maxi = NBR_PHYS_OUTPUTS; break;
	}
	if ( ValOffset+NBR_BOOLS_VAR_SPY>Maxi || ValOffset<0 )
		ValOffset = 0;
	ValOffsetBoolVar[ NumType ] = ValOffset;
	UpdateAllLabelsBoolsVars( );
	RefreshAllBoolsVars( );
	return TRUE;
}

// return true so window is not destroyed
// set toggle to 3 so hitting vars button again wiil hide everything
// unless we were only showing BoolVars window then start from the beginning
gint BoolVarsWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
	gtk_widget_hide( SpyBoolVarsWindow );
	if (toggle==1) {toggle=0;
	}else{toggle=3;}
	return TRUE;
}



void BoolVarsWindowInitGtk()
{
	GtkWidget *vboxboolvars[ NBR_TYPE_BOOLS_SPY ],*vboxmain,*hboxvars;
	long NumCheckWidget,ColumnVar;
	GtkTooltips * WidgetTooltips[ NBR_TYPE_BOOLS_SPY ];
	
	SpyBoolVarsWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *)SpyBoolVarsWindow, "Bit Status Window");
	vboxmain = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (SpyBoolVarsWindow), vboxmain);
	gtk_widget_show (vboxmain);
	hboxvars = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vboxmain), hboxvars);
	gtk_widget_show (hboxvars);

	for( ColumnVar=0; ColumnVar<NBR_TYPE_BOOLS_SPY; ColumnVar++ )
	{
		vboxboolvars[ ColumnVar ] = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (hboxvars), vboxboolvars[ ColumnVar ]);
		gtk_widget_show (vboxboolvars[ ColumnVar ]);
	}

	NumCheckWidget = 0;
	for(ColumnVar=0; ColumnVar<NBR_TYPE_BOOLS_SPY; ColumnVar++)
	{
		int OffVar;
		offsetboolvar[ ColumnVar ]  = gtk_entry_new();
		WidgetTooltips[ ColumnVar ] = gtk_tooltips_new();
		gtk_tooltips_set_tip ( WidgetTooltips[ ColumnVar ], offsetboolvar[ ColumnVar ], "Offset for vars displayed below (return to apply)", NULL );
		gtk_widget_set_usize((GtkWidget *)offsetboolvar[ ColumnVar ],40,0);
		gtk_box_pack_start (GTK_BOX(vboxboolvars[ ColumnVar ]),  offsetboolvar[ ColumnVar ] , FALSE, FALSE, 0);
		gtk_widget_show( offsetboolvar[ ColumnVar ] );
		gtk_entry_set_text((GtkEntry *)offsetboolvar[ ColumnVar ],"0");
		gtk_signal_connect(GTK_OBJECT (offsetboolvar[ ColumnVar ]), "activate",
					(GtkSignalFunc) OffsetBoolVar_activate_event, (void *)ColumnVar);
		
		for(OffVar=0; OffVar<NBR_BOOLS_VAR_SPY; OffVar++)
		{
			chkvar[ ColumnVar ][ OffVar ] = gtk_check_button_new_with_label("xxxx");
			gtk_box_pack_start (GTK_BOX(vboxboolvars[ ColumnVar ]), chkvar[ ColumnVar ][ OffVar ], FALSE, FALSE, 0);
			gtk_widget_show(chkvar[ ColumnVar ][ OffVar ]);
			gtk_signal_connect(GTK_OBJECT (chkvar[ ColumnVar ][ OffVar ]), "toggled",
					(GtkSignalFunc) chkvar_press_event, (void*)NumCheckWidget);
			NumCheckWidget++;
		}
	}
	UpdateAllLabelsBoolsVars( );
	
	gtk_signal_connect( GTK_OBJECT(SpyBoolVarsWindow), "delete_event",
		(GtkSignalFunc)BoolVarsWindowDeleteEvent, 0 );

//	gtk_window_set_policy( GTK_WINDOW(SpyBoolVarsWindow), FALSE/*allow_shrink*/, FALSE/*allow_grow*/, TRUE/*auto_shrink*/ );
}



char * ConvToBin( unsigned int Val )
{
	static char TabBin[ 33 ];
	int Pos;
	unsigned int Mask = 0x80000000;
	char First1 = FALSE;
	strcpy( TabBin, "" );
	for ( Pos = 0; Pos<32; Pos++ )
	{
		if ( Val & Mask )
			First1 = TRUE;
		if ( First1 )
		{
			if ( Val & Mask )
				strcat( TabBin, "1" );
			else
				strcat( TabBin, "0" );
		}
		Mask = Mask>>1;
	}
	if ( Val==0 )
		strcpy( TabBin,"0" );
	return TabBin;
}
// This function updates the signed Integer window
// It checks the display format and displays the number in hex,binary or decimal
// It checks to see what type of word variable it represents (Hal s32 pin in or out or an internal memory variable) and displays that label with colour
// If Displays variable names or symbol names depending on the check box in the section display window
void DisplayFreeVarSpy()
{
	static int LastTime;
	int NumVarSpy,i=NBR_FREE_VAR_SPY;
	int Value;
	char BufferValue[50];
	char DisplayFormat[10];
	char  * VarName;
	if ( NBR_WORDS < NBR_FREE_VAR_SPY) { i=NBR_WORDS ;}
	for (NumVarSpy=0; NumVarSpy<i; NumVarSpy++)
	{
		Value = ReadVar(VarSpy[NumVarSpy][0],VarSpy[NumVarSpy][1]);
		strcpy( DisplayFormat , (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)DisplayFormatVarSpy[NumVarSpy])->entry) );
		strcpy( BufferValue, "" );
		if (strcmp( DisplayFormat,"Dec" )==0 )
			sprintf(BufferValue,"%d",Value);
		if (strcmp( DisplayFormat,"Hex" )==0 )
			sprintf(BufferValue,"%X",Value);
		if (strcmp( DisplayFormat,"Bin" )==0 )
			strcpy( BufferValue, ConvToBin( Value ) );
		gtk_entry_set_text((GtkEntry *)EntryVarSpy[NumVarSpy+(2*NBR_FREE_VAR_SPY)],BufferValue);
                VarName= "<span foreground=\"gray\" weight=\"bold\" >Other       </span>";
               
                switch (VarSpy[NumVarSpy][0])
                     
                      {
                       case VAR_TIMER_VALUE :
                       case VAR_TIMER_PRESET :
                       case VAR_TIMER_RUNNING :
                       case VAR_TIMER_DONE :
                            VarName= "<span foreground=\"brown\" weight=\"bold\" >Timer       </span>";
                            break;
                       case VAR_TIMER_IEC_VALUE :
                       case VAR_TIMER_IEC_PRESET :
                       case VAR_TIMER_IEC_DONE :
                            VarName= "<span foreground=\"brown\" weight=\"bold\" >IEC Timer   </span>";
                            break;
                       case VAR_COUNTER_VALUE :
                       case VAR_COUNTER_PRESET :
                       case VAR_COUNTER_FULL :
                       case VAR_COUNTER_EMPTY :
                       case VAR_COUNTER_DONE :
                            VarName= "<span foreground=\"brown\" weight=\"bold\" >Counter     </span>";
                            break;
                       case VAR_MONOSTABLE_RUNNING :
                       case VAR_MONOSTABLE_PRESET :
                       case VAR_MONOSTABLE_VALUE :
                            VarName= "<span foreground=\"brown\" weight=\"bold\" >Monostable  </span>";
                            break;
                       case VAR_MEM_WORD :
                            VarName= "<span foreground=\"black\" weight=\"bold\" >Memory      </span>";
                            break;
                       case VAR_PHYS_INPUT :
                            VarName= "<span foreground=\"red\" weight=\"bold\" >Bit In Pin  </span>";
                            break;
                       case VAR_PHYS_OUTPUT :
                            VarName= "<span foreground=\"blue\" weight=\"bold\" >Bit Out Pin </span>";
                            break;
                       case VAR_PHYS_FLOAT_INPUT :
                            VarName= "<span foreground=\"red\" weight=\"bold\" >Floatin Pin </span>";
                            break;
                       case VAR_PHYS_FLOAT_OUTPUT :
                            VarName= "<span foreground=\"blue\" weight=\"bold\" >Floatout Pin</span>";
                            break;
                       case VAR_PHYS_WORD_INPUT :
                            VarName= "<span foreground=\"red\" weight=\"bold\" >S32in Pin   </span>";
                            break;
                       case VAR_PHYS_WORD_OUTPUT :
                            VarName= "<span foreground=\"blue\" weight=\"bold\" >S32out Pin  </span>";
                            break;
                       case VAR_MEM_BIT :
                            VarName= "<span foreground=\"black\" weight=\"bold\" >Bit Memory  </span>";
                            break;
                       case VAR_ERROR_BIT :
                            VarName= "<span foreground=\"gold\" weight=\"bold\" >Error Bit   </span>";
                            break;
                       case VAR_STEP_ACTIVITY :
                            VarName= "<span foreground=\"brown\" weight=\"bold\" >Step Active  </span>";
                            break;
                       case VAR_STEP_TIME :
                            VarName= "<span foreground=\"brown\" weight=\"bold\" >Step Run Time</span>";
                            break;
                      }
		gtk_label_set_markup (GTK_LABEL (LabelFreeVars[NumVarSpy]),VarName);

		if (InfosGene->DisplaySymbols!=LastTime) 
		{		 	
		gtk_entry_set_text((GtkEntry *)EntryVarSpy[ NumVarSpy+(1 *NBR_FREE_VAR_SPY)],CreateVarName(VarSpy[NumVarSpy][0],VarSpy[NumVarSpy][1]));
		}
	}
	// we do this check after the FOR loop
    	// so it does not toggle each time thru loop
        // Toggle LastTime to match InfoGene  
	if (InfosGene->DisplaySymbols!=LastTime) 
		{ LastTime=((LastTime-1)*-1); } 		
}

static gint EntryVarSpy_activate_event(GtkWidget *widget, int NumSpy)
{
	int NewVarType,NewVarOffset;
	int * NumVarSpy = &VarSpy[NumSpy][0];
	char BufferVar[30];
	strcpy(BufferVar, gtk_entry_get_text((GtkEntry *)widget) );
	if (TextParserForAVar(BufferVar , &NewVarType, &NewVarOffset, NULL, FALSE/*PartialNames*/))
	{
		char * OtherVarName = NULL;
		*NumVarSpy++ = NewVarType;
		*NumVarSpy = NewVarOffset;

		if ( BufferVar[ 0 ]=='%' ) 
                       {       OtherVarName = ConvVarNameToSymbol( BufferVar );
                       }else{  
                               OtherVarName = ConvSymbolToVarName( BufferVar );
                            }
		if ( OtherVarName ) {    gtk_tooltips_set_tip ( TooltipsEntryVarSpy[ NumSpy ], widget, OtherVarName, NULL );    }
	}else{
		int OldType,OldOffset;
		/* Error Message */
		if (ErrorMessageVarParser)
                       {       ShowMessageBox("Error",ErrorMessageVarParser,"Ok");
		       }else{
			       ShowMessageBox( "Error", "Unknown variable...", "Ok" );
                            }
		OldType = *NumVarSpy++;
		OldOffset = *NumVarSpy;
		/* put back old correct var */
		gtk_entry_set_text((GtkEntry *)widget,CreateVarName(OldType,OldOffset));
	      }
	return TRUE;
}

//return true so window is not destroyed
//set toggle to 3 so hitting vars button again wiil hide everything
//unless we were only showing FreeVars window then start from the beginning
gint FreeVarsWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
	gtk_widget_hide( SpyFreeVarsWindow );
	if (toggle==2) {  toggle=0;  }else{  toggle=3;  }
	return TRUE;
}

// modified this to have 3 columns so we can display variable type
void FreeVarsWindowInitGtk( )
{
	GtkWidget * hboxfreevars[ NBR_FREE_VAR_SPY ], *vboxMain;
	char * VarName= NULL;
	long ColumnVar;
	int NumVarSpy,NumEntry,i=NBR_FREE_VAR_SPY;
	GList *DisplayFormatItems = NULL;

	SpyFreeVarsWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *)SpyFreeVarsWindow, "Watch Window");
	vboxMain = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (SpyFreeVarsWindow), vboxMain);
	gtk_widget_show (vboxMain);
	
	DisplayFormatItems = g_list_append(DisplayFormatItems,"Dec");
	DisplayFormatItems = g_list_append(DisplayFormatItems,"Hex");
	DisplayFormatItems = g_list_append(DisplayFormatItems,"Bin");
	if ( NBR_WORDS < NBR_FREE_VAR_SPY) { i=NBR_WORDS ;}
	for(NumVarSpy=0; NumVarSpy<i; NumVarSpy++)
	{
		hboxfreevars[ NumVarSpy ] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vboxMain), hboxfreevars[ NumVarSpy ]);
		gtk_widget_show (hboxfreevars[ NumVarSpy ]);

		for(ColumnVar=0; ColumnVar<3; ColumnVar++)
		{
			NumEntry = NumVarSpy+ColumnVar*NBR_FREE_VAR_SPY;			
			
			if ( ColumnVar==0)
			{
				LabelFreeVars[NumEntry] = gtk_label_new(NULL);				
				gtk_widget_set_usize((GtkWidget *)LabelFreeVars[NumEntry],100,0);
				gtk_box_pack_start (GTK_BOX (hboxfreevars[ NumVarSpy ]), LabelFreeVars[NumEntry], FALSE, FALSE, 0);
				gtk_widget_show (LabelFreeVars[NumEntry]);
			}
			if ( ColumnVar==1)
			{
				EntryVarSpy[ NumEntry ] = gtk_entry_new();
			        gtk_widget_show(EntryVarSpy[NumEntry]);
				gtk_box_pack_start (GTK_BOX( hboxfreevars[ NumVarSpy ] ), EntryVarSpy[ NumEntry ], TRUE, TRUE, 0);
				gtk_widget_set_usize((GtkWidget *)EntryVarSpy[ NumEntry ],(ColumnVar==1)?80:110,0);	
				VarName = CreateVarName(VarSpy[NumVarSpy][0],VarSpy[NumVarSpy][1]);
				TooltipsEntryVarSpy[ NumVarSpy ] = gtk_tooltips_new();
				gtk_entry_set_text((GtkEntry *)EntryVarSpy[ NumEntry ],VarName);
				gtk_signal_connect(GTK_OBJECT (EntryVarSpy[ NumEntry ]), "activate",
                                (GtkSignalFunc) EntryVarSpy_activate_event, (void *)(intptr_t)NumVarSpy);
			}
			if ( ColumnVar==2)
			{
				EntryVarSpy[ NumEntry ] = gtk_entry_new();
			        gtk_widget_show(EntryVarSpy[NumEntry]);
				gtk_box_pack_start (GTK_BOX( hboxfreevars[ NumVarSpy ] ), EntryVarSpy[ NumEntry ], TRUE, TRUE, 0);
				gtk_widget_set_usize((GtkWidget *)EntryVarSpy[ NumEntry ],(ColumnVar==2)?80:110,0);

			}

		}

		DisplayFormatVarSpy[NumVarSpy] = gtk_combo_new();
		gtk_combo_set_value_in_list(GTK_COMBO(DisplayFormatVarSpy[NumVarSpy]), TRUE /*val*/, FALSE /*ok_if_empty*/);
		gtk_combo_set_popdown_strings(GTK_COMBO(DisplayFormatVarSpy[NumVarSpy]), DisplayFormatItems);
		gtk_widget_set_usize((GtkWidget *)DisplayFormatVarSpy[NumVarSpy],65,0);
		gtk_box_pack_start (GTK_BOX(hboxfreevars[ NumVarSpy ]), DisplayFormatVarSpy[NumVarSpy], FALSE, FALSE, 0);
		gtk_widget_show(DisplayFormatVarSpy[NumVarSpy]);
	}
	gtk_signal_connect( GTK_OBJECT(SpyFreeVarsWindow), "delete_event",
		(GtkSignalFunc)FreeVarsWindowDeleteEvent, 0 );
}

void VarsWindowInitGtk()
{
	FreeVarsWindowInitGtk( );
	BoolVarsWindowInitGtk( );
}

// This is modified to toggle the vars windows
// be click the spyvars button multiple times
// one, the other, both, then none of the windows will be shown
void OpenSpyVarsWindow( )
{
	
	switch (toggle)
	{
	case 0 :	gtk_widget_show( SpyBoolVarsWindow ); gtk_widget_hide( SpyFreeVarsWindow );
			MessageInStatusBar("openned BOOL (bit) variable window. press again for WORD window");	
		break;
	case 1 :        gtk_widget_hide( SpyBoolVarsWindow ); gtk_widget_show( SpyFreeVarsWindow );
			MessageInStatusBar("openned WORD (s32) variable window. press again for both windows");
		break;
	case 2 :	gtk_widget_show( SpyBoolVarsWindow ); gtk_widget_show( SpyFreeVarsWindow );
			MessageInStatusBar("openned BOTH variable windows. press again to close them.");
		break;
	case 3 :	gtk_widget_hide( SpyBoolVarsWindow ); gtk_widget_hide( SpyFreeVarsWindow );
			MessageInStatusBar("");
		break;
	default:;
	}
	toggle++;
	if (toggle==4) {toggle=0;}

}
