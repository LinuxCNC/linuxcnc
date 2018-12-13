/* Classic Ladder Project */
/* Copyright (C) 2001-2008 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* July 2003 */
/* ----------------------------- */
/* Editor Config - GTK interface */
/* ----------------------------- */
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
/* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */
//Chris Morley July 08

#include <locale.h>
#include <libintl.h>
#define _(x) gettext(x)
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "classicladder.h"
#include "classicladder_gtk.h"
#include "manager.h"
#include "edit.h"
//#include "hardware.h"
#include "global.h"
#include "config_gtk.h"

#ifdef OLD_TIMERS_MONOS_SUPPORT
#define NBR_OBJECTS 19
#else
#define NBR_OBJECTS 17
#endif
GtkWidget *LabelParam[ NBR_OBJECTS ],*ValueParam[ NBR_OBJECTS ];



#define NBR_IO_PARAMS 6
GtkWidget *InputParamEntry[ NBR_INPUTS_CONF ][ NBR_IO_PARAMS ];
GtkWidget *InputDeviceParam[ NBR_INPUTS_CONF ];
GtkWidget *InputFlagParam[ NBR_INPUTS_CONF ];

GtkWidget *OutputParamEntry[ NBR_OUTPUTS_CONF ][ NBR_IO_PARAMS ];
GtkWidget *OutputDeviceParam[ NBR_OUTPUTS_CONF ];
GtkWidget *OutputFlagParam[ NBR_OUTPUTS_CONF ];

//for modbus input/output page
#ifdef MODBUS_IO_MASTER
// ModbusReqType must be in the same order as MODBUS_REQ_ in protocol_modbus_master.h
static char * ModbusReqType[] = {"Read_discrete_INPUTS  fnctn- 2", "Write_COIL(S)           fnctn-5/15", "Read_Input_REGS     fnctn- 4", "Write_hold_REG(S)    fnctn-6/16", "Read_COILS              fnctn- 1","Read_HOLD_REG      fnctn- 3","Slave_echo              fnctn- 8",NULL };
#define NBR_MODBUS_PARAMS 6
static char * SerialSpeed[] = { "300", "600", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", NULL };
#define NBR_SERIAL_SPEED 9
static char * PortName[] = {"IP port","/dev/ttyS0","/dev/ttyS1","/dev/ttyS2","/dev/ttyS3","/dev/ttyUSB0","/dev/ttyUSB1",NULL };
#define NBR_PORT_NAME 6
GtkWidget *ModbusParamEntry[ NBR_MODBUS_MASTER_REQ ][ NBR_MODBUS_PARAMS ];
GtkWidget *SerialPortEntry;
GtkWidget *SerialSpeedEntry;
GtkWidget *PauseInterFrameEntry;
GtkWidget *DebugLevelEntry;

//for modbus configure window
int MapCoilRead;
int MapCoilWrite;
#define NBR_COM_PARAMS 12
GtkWidget *EntryComParam[ NBR_COM_PARAMS ];
GtkWidget *ComboComParam[2];
GtkWidget *ConfigWindow;
GtkWidget *DebugButton [ 4 ];
GtkWidget *OffsetButton[ 2 ];
GtkWidget *RtsButton   [ 2 ];
GtkWidget *MapButton   [ 10 ];
GSList *group;
#endif

GtkWidget * CreateGeneralParametersPage( void )
{
	GtkWidget *vbox;
	GtkWidget *hbox[ NBR_OBJECTS ];
	int NumObj;
         
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
        
	for (NumObj=0; NumObj<NBR_OBJECTS; NumObj++)
	{
		char BuffLabel[ 50 ];
		char BuffValue[ 200 ];
                
		int InfoUsed = 0;
		hbox[NumObj] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumObj]);
		gtk_widget_show (hbox[NumObj]);
             
		switch( NumObj )
		{ 
			case 0:
				sprintf( BuffLabel, _("Rung Refresh Rate (milliseconds)") );
				sprintf( BuffValue, "%d", InfosGene->GeneralParams.PeriodicRefreshMilliSecs );
				break;
			case 1:
				InfoUsed = GetNbrRungsDefined( )*100/InfosGene->GeneralParams.SizesInfos.nbr_rungs;
				sprintf( BuffLabel, _("Number of rungs (%d%c used      "), InfoUsed,'%' );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_rungs );
				break;
			case 2:
				sprintf( BuffLabel, _("Number of Bits                  ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_bits );
				break;
                        case 3:
				sprintf( BuffLabel, _("Number of Error Bits                  ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_error_bits );
				break;
			case 4:
				sprintf( BuffLabel, _("Number of Words                 ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_words );
				break;
			case 5:
				sprintf( BuffLabel, _("Number of Counters              ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_counters );
				break;
			case 6:
				sprintf( BuffLabel, _("Number of Timers IEC            ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_timers_iec );
				break;
			case 7:
				sprintf( BuffLabel, _("Number of Arithmetic Expressions ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_arithm_expr );
				break;
			case 8:
				InfoUsed = NbrSectionsDefined( )*100/InfosGene->GeneralParams.SizesInfos.nbr_sections;
				sprintf( BuffLabel, _("Number of Sections (%d%c used)   "), InfoUsed,'%' );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_sections );
				break;
			case 9:
				sprintf( BuffLabel, _("Number of Symbols                ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_symbols );
				break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
			case 10:
				sprintf( BuffLabel, _("Number of Timers                 ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_timers );
				break;
			case 11:
				sprintf( BuffLabel, _("Number of Monostables            ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_monostables );
				break;
#endif
                        case 12:
				sprintf( BuffLabel, _("Number of BIT Inputs HAL pins           ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_inputs );
				break;
			case 13:
				sprintf( BuffLabel, _("Number of BIT Outputs HAL pins          ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_outputs );
				break;
			case 14:
				sprintf( BuffLabel, _("Number of S32in HAL pins             ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_words_inputs );
				break;
			case 15:
				sprintf( BuffLabel, _("Number of S32out HAL pins            ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_words_outputs );
				break;
                        case 16:
				sprintf( BuffLabel, _("Number of floatin HAL pins             ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_float_inputs );
				break;
			case 17:
				sprintf( BuffLabel, _("Number of floatout HAL pins            ") );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_float_outputs );
				break;
                        case 18:
				sprintf( BuffLabel, _("Current path/filename") );
                                sprintf( BuffValue, "%s",InfosGene->CurrentProjectFileName);
                                //sprintf( BuffValue, "Not available yet" );
				break;                                
			default:
				sprintf( BuffLabel, "???" );
				sprintf( BuffValue, "???" );
				break;
		}

		LabelParam[NumObj] = gtk_label_new(BuffLabel);
		gtk_widget_set_usize((GtkWidget *)LabelParam[NumObj],300,0);
		gtk_box_pack_start (GTK_BOX (hbox[NumObj]), LabelParam[NumObj], FALSE, FALSE, 0);
		gtk_widget_show (LabelParam[NumObj]);

		/* For numbers */
		ValueParam[NumObj] = gtk_entry_new();
		if (NumObj==18) {   gtk_widget_set_usize((GtkWidget *)ValueParam[NumObj],200,0);
                           }else{  
                                    gtk_widget_set_usize((GtkWidget *)ValueParam[NumObj],50,0);  
                                }
		gtk_box_pack_start (GTK_BOX (hbox[NumObj]), ValueParam[NumObj], FALSE, FALSE, 0);
		gtk_widget_show (ValueParam[NumObj]);
                gtk_entry_set_text( GTK_ENTRY(ValueParam[NumObj]), BuffValue);
                // make all the entries non editable for EMC
		gtk_editable_set_editable( GTK_EDITABLE(ValueParam[NumObj]),FALSE);
	}
	return vbox;
}
int GetOneGeneralInfo( int iNumber )
{
	char text[ 10 ];
	int value;
	strncpy( text, (char *)gtk_entry_get_text((GtkEntry *)ValueParam[ iNumber ]), 10 );
	text[ 9 ] = '\0';
	value = atoi( text );
	return value;
}
void GetGeneralParameters( void )
{
	int TheValue;
	TheValue = GetOneGeneralInfo( 0 );
	if ( TheValue<1 || TheValue>1000 )
		TheValue = PERIODIC_REFRESH_MS_DEF;
	InfosGene->GeneralParams.PeriodicRefreshMilliSecs = TheValue;
	GeneralParamsMirror.PeriodicRefreshMilliSecs = TheValue;

	TheValue = GetOneGeneralInfo( 1 );
	GeneralParamsMirror.SizesInfos.nbr_rungs = TheValue;
	TheValue = GetOneGeneralInfo( 2 );
	GeneralParamsMirror.SizesInfos.nbr_bits = TheValue;
	TheValue = GetOneGeneralInfo( 3 );
	GeneralParamsMirror.SizesInfos.nbr_words = TheValue;
	TheValue = GetOneGeneralInfo( 4 );
	GeneralParamsMirror.SizesInfos.nbr_counters = TheValue;
	TheValue = GetOneGeneralInfo( 5 );
	GeneralParamsMirror.SizesInfos.nbr_timers_iec = TheValue;
	TheValue = GetOneGeneralInfo( 6 );
	GeneralParamsMirror.SizesInfos.nbr_phys_inputs = TheValue;
	TheValue = GetOneGeneralInfo( 7 );
	GeneralParamsMirror.SizesInfos.nbr_phys_outputs = TheValue;
	TheValue = GetOneGeneralInfo( 8 );
	GeneralParamsMirror.SizesInfos.nbr_arithm_expr = TheValue;
	TheValue = GetOneGeneralInfo( 9 );
	GeneralParamsMirror.SizesInfos.nbr_sections = TheValue;
	TheValue = GetOneGeneralInfo( 10 );
	GeneralParamsMirror.SizesInfos.nbr_symbols = TheValue;
#ifdef OLD_TIMERS_MONOS_SUPPORT
	TheValue = GetOneGeneralInfo( 11 );
	GeneralParamsMirror.SizesInfos.nbr_timers = TheValue;
	TheValue = GetOneGeneralInfo( 12 );
	GeneralParamsMirror.SizesInfos.nbr_monostables = TheValue;
#endif
}


int ConvComboToNum( char * text, char ** list )
{
	int Value = 0;
	char Found = FALSE;
	while( !Found && list[ Value ]!=NULL )
	{
		if ( strcmp( list[ Value ], text )==0 )
			Found = TRUE;
		else
			Value++;
	}
	return Value;
}

char* ConvNumToString( int num, char ** list )
{
	int Value = 0;
	char Found = FALSE;
	while( !Found && list[ Value ]!=NULL )
	{
		if ( atoi( list[ Value ] )== num)
			Found = TRUE;
		else
			Value++;
	}
	return list[ Value ];
}

#ifdef MODBUS_IO_MASTER
GtkWidget * CreateModbusModulesIO( void )
{
	static char * Labels[] = { "Slave Address", "Request Type", "1st Modbus Ele.", "# of Ele", "Logic", "1st Variable mapped" };
	GtkWidget *vbox;
	GtkWidget *hbox[ NBR_MODBUS_MASTER_REQ+2 ];
	int NumObj;
	int NumLine;
	GList * ItemsDevices = NULL;
	int ScanDev = 0;
	StrModbusMasterReq * pConf;
	char BuffValue[ 40 ];
	GtkWidget *ModbusParamLabel[ NBR_MODBUS_PARAMS];	

 	if(modmaster==FALSE) 
            {
             vbox = gtk_vbox_new (FALSE, 0);
              gtk_widget_show (vbox);
              ModbusParamLabel[0] = gtk_label_new( "\n  To use modbus you must specify a modbus configure file\n"
		"                        when loading classicladder use: \n \n loadusr classicladder --modmaster myprogram.clp  " );
              gtk_box_pack_start(GTK_BOX (vbox), ModbusParamLabel[0], FALSE, FALSE, 0);
              gtk_widget_show( ModbusParamLabel[0] );     
              return vbox;
            }

	do
	{
		ItemsDevices = g_list_append( ItemsDevices, ModbusReqType[ ScanDev++ ] );
	}
	while( ModbusReqType[ ScanDev ] );

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);

	for (NumLine=-1; NumLine<NBR_MODBUS_MASTER_REQ; NumLine++ )
	{
		hbox[NumLine+1] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumLine+1]);
		gtk_widget_show (hbox[NumLine+1]);

		for (NumObj=0; NumObj<NBR_MODBUS_PARAMS; NumObj++)
		{
			switch( NumLine )
			{

				case -1:
				{
					int PixelsLength = 100;
					GtkWidget **IOParamLabel = &ModbusParamLabel[ NumObj ];
					switch( NumObj )
					{
						case 0:
							PixelsLength=120;
							break;
						case 1:
							PixelsLength=230;
							break;
                                                case 3:
							PixelsLength=70;
							break;
                                                case 5:
							PixelsLength=140;
							break;
					}
					*IOParamLabel = gtk_label_new( Labels[ NumObj ] );
					gtk_widget_set_usize(*IOParamLabel,PixelsLength,0);
					gtk_box_pack_start(GTK_BOX (hbox[ NumLine+1 ]), *IOParamLabel, FALSE, FALSE, 0);
					gtk_widget_show( *IOParamLabel );
					break;
				}
				default:
				{
					pConf = &ModbusMasterReq[ NumLine ];
					switch( NumObj )
					{
						/* For req type (combo-list) */
						case 1:
						{
							int ValueToDisplay = pConf->TypeReq;
							GtkWidget **IOParamDevice = &ModbusParamEntry[ NumLine ][ NumObj ];
							*IOParamDevice = gtk_combo_new( );
							gtk_combo_set_value_in_list( GTK_COMBO(*IOParamDevice), TRUE /*val*/, FALSE /*ok_if_empty*/ );
							gtk_combo_set_popdown_strings( GTK_COMBO(*IOParamDevice), ItemsDevices );
							gtk_widget_set_usize( *IOParamDevice,230,0 );
							gtk_box_pack_start ( GTK_BOX (hbox[NumLine+1]), *IOParamDevice, FALSE, FALSE, 0 );
							gtk_widget_show ( *IOParamDevice );
					        	gtk_entry_set_text((GtkEntry*)((GtkCombo *)*IOParamDevice)->entry, ModbusReqType[ ValueToDisplay ]);
							gtk_editable_set_editable( GTK_EDITABLE((GtkEntry*)((GtkCombo *)*IOParamDevice)->entry),FALSE);
							break;
						}
						/* For flags (checkbutton)*/
						case 4:
						{
							GtkWidget **IOParamFlag = &ModbusParamEntry[ NumLine ][ NumObj ];
							*IOParamFlag = gtk_check_button_new_with_label( _("Inverted") );
							gtk_widget_set_usize( *IOParamFlag,100,0 );
							gtk_box_pack_start( GTK_BOX (hbox[NumLine+1]), *IOParamFlag, FALSE, FALSE, 0 );
							gtk_widget_show ( *IOParamFlag );
							if ( pConf->LogicInverted )
								gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( *IOParamFlag ), TRUE );
							break;
						}
						/* For numbers/strings (edit widgets)*/
						default:
						{
							int PixelsLength = 100;
							switch( NumObj )
							{
								case 0:
									strcpy( BuffValue, pConf->SlaveAdr );
									PixelsLength = 120;
									break;
								case 2:
									sprintf( BuffValue, "%d", pConf->FirstModbusElement );
									break;
								case 3:
									sprintf( BuffValue, "%d", pConf->NbrModbusElements );
                                                                        PixelsLength = 70;
									break;
								case 5:
									sprintf( BuffValue, "%d", pConf->OffsetVarMapped );
									PixelsLength = 140;
									break;
							}
							{
								GtkWidget **IOParamEntry = &ModbusParamEntry[ NumLine ][ NumObj ];
								*IOParamEntry = gtk_entry_new( );
								gtk_widget_set_usize( *IOParamEntry,PixelsLength,0 );
								gtk_box_pack_start( GTK_BOX (hbox[NumLine+1]), *IOParamEntry, FALSE, FALSE, 0 );
								gtk_widget_show ( *IOParamEntry );
								gtk_entry_set_text( GTK_ENTRY(*IOParamEntry), BuffValue );
							}
							break;
						}
					}
				}//default:
			}
		}
	}
	return vbox;
}
void GetModbusModulesIOSettings( void )
{
	int NumObj;
	int NumLine;
	StrModbusMasterReq * pConf;
	GtkWidget **IOParamEntry;
	char * text;
	char BuffValue[ 40 ];
	for (NumLine=0; NumLine<NBR_MODBUS_MASTER_REQ; NumLine++ )
	{
		int MaxVars = 0;
		char DoVerify = FALSE;
		pConf = &ModbusMasterReq[ NumLine ];
		strcpy( pConf->SlaveAdr, "" );
		pConf->LogicInverted = 0;

		for (NumObj=0; NumObj<NBR_IO_PARAMS; NumObj++)
		{
			IOParamEntry = &ModbusParamEntry[ NumLine ][ NumObj ];
			switch( NumObj )
			{
				case 0://slave address
					text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
					strcpy( BuffValue, text );
					break;
				case 1://type of request
					pConf->TypeReq = ConvComboToNum( (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)*IOParamEntry)->entry), ModbusReqType );
					break;
				case 2://first element address
					text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
					pConf->FirstModbusElement = atoi( text );
					break;
				case 3://number of requested items
					text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
					pConf->NbrModbusElements = atoi( text );
					break;
				case 4://invert logic (instead of thinking of that everywhere later...)
					if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( *IOParamEntry ) ) )
						pConf->LogicInverted = 1;
					break;
				case 5:// first classicladder variable map location
					text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
					pConf->OffsetVarMapped = atoi( text );
					break;
			}
		}//for (NumObj=0; 
		/* verify if not overflowing */
		//TODO much more error checking for word variable. 
switch( pConf->TypeReq )
		{
			case MODBUS_REQ_INPUTS_READ: 
			case MODBUS_REQ_COILS_READ: 
                                                   if (MapCoilRead==B_VAR)  {   MaxVars = NBR_BITS ; DoVerify = TRUE; break;
                                                                       }else{   MaxVars = NBR_PHYS_OUTPUTS; DoVerify = TRUE; break;    }
                        case MODBUS_REQ_COILS_WRITE: 
                                                   if (MapCoilWrite==B_VAR) {   MaxVars = NBR_BITS ; DoVerify = TRUE; break;   }
                                                   if (MapCoilWrite==I_VAR) {   MaxVars = NBR_PHYS_INPUTS; DoVerify = TRUE; break;   
                                                                      }else {   MaxVars = NBR_PHYS_OUTPUTS; DoVerify = TRUE; break;   }
			case MODBUS_REQ_REGISTERS_READ: 
                        case MODBUS_REQ_HOLD_READ:
                                                   if (MapRegisterRead==W_VAR) {   MaxVars = NBR_WORDS; DoVerify = TRUE; break;
                                                                     }else{   MaxVars = NBR_PHYS_WORDS_OUTPUTS; DoVerify = TRUE; break;    }

			case MODBUS_REQ_REGISTERS_WRITE: 
                                                   if (MapRegisterWrite==W_VAR) {   MaxVars = NBR_WORDS; DoVerify = TRUE; break;    }
                                                   if (MapRegisterWrite==IW_VAR) {   MaxVars = NBR_PHYS_WORDS_INPUTS; DoVerify = TRUE; break;
                                                                     }else{  MaxVars = NBR_PHYS_WORDS_OUTPUTS; DoVerify = TRUE; break;    }
			
		}
		if ( DoVerify )
		{
			if ( pConf->OffsetVarMapped+pConf->NbrModbusElements>MaxVars )
			{
				printf(_("Error in I/O modbus conf: overflow for I,B,Q,IQ or WQ mapping detected...ASKED=%i,MAX=%i\n"),  pConf->OffsetVarMapped+pConf->NbrModbusElements,MaxVars);
				strcpy( BuffValue, "" );
				ShowMessageBox(_("Error"),_("Overflow error for I,B,Q,IQ or WQ mapping detected..."),_("Ok"));
			}
		}
		
		/* done at the end, do not forget multi-task ! */
		/* the first char is tested to determine a valid request => paranoia mode ;-) */
		//printf("buffvalue1=%s buffvalue0=%d \n",&BuffValue[ 1 ],BuffValue[ 0 ]);
		strcpy( &pConf->SlaveAdr[ 1 ], &BuffValue[ 1 ] );
		pConf->SlaveAdr[ 0 ] = BuffValue[ 0 ];
	}//for (NumLine=0; 
}
#endif

// These 7 callback functions will change the global variable as soon as the radio button is changed
// you don't have to close the window to update them
 void debug_button_callback (GtkWidget *widget, gint data)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[0])))  { ModbusDebugLevel = 0; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[1])))  { ModbusDebugLevel = 1; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[2])))  { ModbusDebugLevel = 2; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[3])))  { ModbusDebugLevel = 3; } 
}

void map_CoilR_button_callback (GtkWidget *widget, gint data)
{
   int i;

   for (i=0; i<2; i++)
	{
           if (widget==MapButton[i]) {break;}
        }
   MapCoilRead=data;
}

void map_CoilW_button_callback (GtkWidget *widget, gint data)
{
   int i;

   for (i=2; i<5; i++)
	{
           if (widget==MapButton[i]) {break;}
        }
   MapCoilWrite=data;
}


void map_RegsR_button_callback (GtkWidget *widget, gint data)
{
   int i;

   for (i=5; i<8; i++)
	{
           if (widget==MapButton[i]) {break;}
        }
   MapRegisterRead=data;
}

void map_RegsW_button_callback (GtkWidget *widget, gint data)
{
   int i;

   for (i=8; i<10; i++)
	{
           if (widget==MapButton[i]) {break;}
        }
   MapRegisterWrite=data;
}

static void offset_button_callback (GtkWidget *widget, gint data)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (OffsetButton[0])))  { ModbusEleOffset = 0; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (OffsetButton[1])))  { ModbusEleOffset = 1; } 
}

static void rts_button_callback (GtkWidget *widget, gint data)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (RtsButton[0])))  {  ModbusSerialUseRtsToSend = 0; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (RtsButton[1])))  {  ModbusSerialUseRtsToSend = 1; } 
}

GtkWidget * CreateModbusComParametersPage( void )
{
	GtkWidget *vbox;
	GtkWidget *hbox[ NBR_COM_PARAMS ];
	GtkWidget *LabelComParam[ NBR_COM_PARAMS ];
        GtkWidget **ComboPortName = &ComboComParam[0];
        GtkWidget **ComboSerialSpeed = &ComboComParam[1];
        GSList *group;
        GList * SpeedItemsDevices = NULL;
        GList * PortItemsDevices = NULL;
	int NumLine,ScanDev = 0;
	char BuffLabel[ 50 ];
	char BuffValue[ 20 ];

        if(modmaster==FALSE) 
            {
             vbox = gtk_vbox_new (FALSE, 0);
              gtk_widget_show (vbox);
              LabelComParam[ 0 ] = gtk_label_new( "\n  To use modbus you must specify a modbus configure file\n"
		"                        when loading classicladder use: \n \n loadusr classicladder --modmaster myprogram.clp   " );
              gtk_box_pack_start(GTK_BOX (vbox),LabelComParam[ 0 ] , FALSE, FALSE, 0);
              gtk_widget_show( LabelComParam[ 0 ]  );     
              return vbox;
             }
        do
	{
		SpeedItemsDevices = g_list_append( SpeedItemsDevices, SerialSpeed[ ScanDev++ ] );
	}
	while( SerialSpeed[ ScanDev ] );
        ScanDev = 0;
        do
	{
		PortItemsDevices = g_list_append( PortItemsDevices, PortName[ ScanDev++ ] );
	}
	while( PortName[ ScanDev ] );
	vbox = gtk_vbox_new (FALSE/*homogeneous*/, 0);
	gtk_widget_show (vbox);

	for( NumLine=0; NumLine<NBR_COM_PARAMS; NumLine++ )
	{
		hbox[NumLine] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumLine]);
		gtk_widget_show (hbox[NumLine]);
		switch( NumLine )
		{
			case 0:
				sprintf( BuffLabel, _("Serial port (blank = IP mode)") );
				strcpy( BuffValue, ModbusSerialPortNameUsed );
				break;
			case 1:
				sprintf( BuffLabel, _("Serial baud rate") );
				sprintf( BuffValue, "%d", ModbusSerialSpeed );
                                
				break;
                        case 2:
				sprintf( BuffLabel, _("After transmit pause - milliseconds") );
				sprintf( BuffValue, "%d", ModbusTimeAfterTransmit );
				break;
			
			case 3:
				sprintf( BuffLabel, _("After receive pause - milliseconds") );
				sprintf( BuffValue, "%d", ModbusTimeInterFrame );
				break;
			case 4:
				sprintf( BuffLabel, _("Request Timeout length - milliseconds") );
				sprintf( BuffValue, "%d", ModbusTimeOutReceipt );
				break;
			case 5:
				sprintf( BuffLabel, _("Use RTS to send") );
				sprintf( BuffValue, "%d", ModbusSerialUseRtsToSend );
				break;
			case 6:
				sprintf( BuffLabel, _("Modbus element offset") );
				sprintf( BuffValue, "%d", ModbusEleOffset );
				break;
			case 7:
				sprintf( BuffLabel, _("Debug level") );
				sprintf( BuffValue, "%d", ModbusDebugLevel );
				break;
                        case 8:
				sprintf( BuffLabel, _("Read Coils/inputs map to") );
				//sprintf( BuffValue, "%d", ModbusDebugLevel );
				break;
                        case 9:
				sprintf( BuffLabel, _("Write Coils map from") );
				//sprintf( BuffValue, "%d", ModbusDebugLevel );
				break;
                        case 10:
				sprintf( BuffLabel, _("Read register/holding map to") );
				//sprintf( BuffValue, "%d", ModbusDebugLevel );
				break;
                        case 11:
				sprintf( BuffLabel, _("Write registers map from") );
				//sprintf( BuffValue, "%d", ModbusDebugLevel );
				break;
		}
            switch( NumLine )
		{
                        case 0:
                               //port name label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
                                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] );

                                // combo box				
				*ComboPortName = gtk_combo_new( );
				gtk_combo_set_value_in_list( GTK_COMBO(*ComboPortName), TRUE /*val*/, FALSE /*ok_if_empty*/ );
				gtk_combo_set_popdown_strings( GTK_COMBO(*ComboPortName), PortItemsDevices );
				gtk_widget_set_usize( *ComboPortName,125,0 );
				gtk_box_pack_start ( GTK_BOX (hbox[NumLine]), *ComboPortName, FALSE, FALSE, 0 );
				gtk_widget_show ( *ComboPortName );
                                if ( strncmp( ModbusSerialPortNameUsed, "",strlen( ModbusSerialPortNameUsed) )==0 )           
                                   {
			            gtk_entry_set_text((GtkEntry*)((GtkCombo *)*ComboPortName)->entry,"IP port");
                                   }else{
                                         gtk_entry_set_text((GtkEntry*)((GtkCombo *)*ComboPortName)->entry, ModbusSerialPortNameUsed);
                                        }
                        	gtk_editable_set_editable( GTK_EDITABLE((GtkEntry*)((GtkCombo *)*ComboPortName)->entry),FALSE);
                                break;
                        case 1:
                                // serial speed label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
                                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] );

                                // combo box				
				*ComboSerialSpeed = gtk_combo_new( );
				gtk_combo_set_value_in_list( GTK_COMBO(*ComboSerialSpeed), TRUE /*val*/, FALSE /*ok_if_empty*/ );
				gtk_combo_set_popdown_strings( GTK_COMBO(*ComboSerialSpeed), SpeedItemsDevices );
				gtk_widget_set_usize( *ComboSerialSpeed,125,0 );
				gtk_box_pack_start ( GTK_BOX (hbox[NumLine]), *ComboSerialSpeed, FALSE, FALSE, 0 );
				gtk_widget_show ( *ComboSerialSpeed );
			        gtk_entry_set_text((GtkEntry*)((GtkCombo *)*ComboSerialSpeed)->entry,
                                                    ConvNumToString( ModbusSerialSpeed,SerialSpeed  ));
				gtk_editable_set_editable( GTK_EDITABLE((GtkEntry*)((GtkCombo *)*ComboSerialSpeed)->entry),FALSE);
                                break;
                         case 5:
                                //RTS label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                RtsButton[0]= gtk_radio_button_new_with_label (NULL, _("NO"));
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), RtsButton[0], FALSE, TRUE, 0);
                                gtk_widget_show (RtsButton[0]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (RtsButton[0]));

                                RtsButton[1]= gtk_radio_button_new_with_label (group, _("YES"));
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), RtsButton[1], FALSE, TRUE, 0);
                                gtk_widget_show (RtsButton[1]);

                                //set the active button by the current stste of  ModbusSerialUseRtsToSend
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (RtsButton[ModbusSerialUseRtsToSend]), TRUE);

                                g_signal_connect (G_OBJECT (RtsButton[0]), "toggled",
 	                   	                  G_CALLBACK (rts_button_callback), GINT_TO_POINTER ( 0 ));
                                g_signal_connect (G_OBJECT (RtsButton[1]), "toggled",
		                                  G_CALLBACK (rts_button_callback), GINT_TO_POINTER ( 1 ));
                                break;

                         case 6:
                                //offset label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                OffsetButton[0]= gtk_radio_button_new_with_label (NULL, "0");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), OffsetButton[0], FALSE, TRUE, 0);
                                gtk_widget_show (OffsetButton[0]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (OffsetButton[0]));

                                OffsetButton[1]= gtk_radio_button_new_with_label (group, "1");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), OffsetButton[1], FALSE, TRUE, 0);
                                gtk_widget_show (OffsetButton[1]);

                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (OffsetButton[ModbusEleOffset ]), TRUE);

                                g_signal_connect (G_OBJECT (OffsetButton[0]), "toggled",
 	                   	                  G_CALLBACK (offset_button_callback), GINT_TO_POINTER ( 0 ));
                                g_signal_connect (G_OBJECT (OffsetButton[1]), "toggled",
		                                  G_CALLBACK (offset_button_callback), GINT_TO_POINTER ( 1 ));
                                break;
                        
			case 7: 
                                //Debug label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                DebugButton[0]= gtk_radio_button_new_with_label (NULL, _("QUIET"));
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), DebugButton[0], FALSE, TRUE, 0);
                                gtk_widget_show (DebugButton[0]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (DebugButton[0]));

                                DebugButton[1]= gtk_radio_button_new_with_label (group, _("LEVEL 1"));
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), DebugButton[1], FALSE, TRUE, 0);
                                gtk_widget_show (DebugButton[1]);
 
                                DebugButton[2]= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (DebugButton[0]),_("LEVEL 2"));
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), DebugButton[2], FALSE, TRUE, 0);
                                gtk_widget_show (DebugButton[2]);

                                DebugButton[3]= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (DebugButton[0]),_("LEVEL 3"));
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), DebugButton[3], FALSE, TRUE, 0);
                                gtk_widget_show (DebugButton[3]);

                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (DebugButton[ModbusDebugLevel]), TRUE);

                                g_signal_connect (G_OBJECT (DebugButton[0]), "toggled",
 	                   	                  G_CALLBACK (debug_button_callback), GINT_TO_POINTER ( 0 ));
                                g_signal_connect (G_OBJECT (DebugButton[1]), "toggled",
		                                  G_CALLBACK (debug_button_callback), GINT_TO_POINTER ( 1 ));
                                g_signal_connect (G_OBJECT (DebugButton[2]), "toggled",
		                                  G_CALLBACK (debug_button_callback), GINT_TO_POINTER ( 2 ));
                                g_signal_connect (G_OBJECT (DebugButton[3]), "toggled",
		                                  G_CALLBACK (debug_button_callback), GINT_TO_POINTER ( 3 ));
                           break;
                   case 8:
                                //read coil map label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                MapButton[0]= gtk_radio_button_new_with_label (NULL, "%B");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[0], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[0]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (MapButton[0]));

                                MapButton[1]= gtk_radio_button_new_with_label (group, "%Q");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[1], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[1]);
 
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (MapButton[MapCoilRead]), TRUE);

                                g_signal_connect (G_OBJECT (MapButton[0]), "toggled",
 	                   	                  G_CALLBACK (map_CoilR_button_callback), GINT_TO_POINTER ( 0 ));
                                g_signal_connect (G_OBJECT (MapButton[1]), "toggled",
		                                  G_CALLBACK (map_CoilR_button_callback), GINT_TO_POINTER ( 1 ));
                          break;
                   case 9:
                                //Write coil map label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                MapButton[2]= gtk_radio_button_new_with_label (NULL, "%B");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[2], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[2]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (MapButton[2]));

                                MapButton[3]= gtk_radio_button_new_with_label (group, "%Q ");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[3], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[3]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (MapButton[3]));

                                MapButton[4]= gtk_radio_button_new_with_label (group, "%I");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[4], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[4]);
 
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (MapButton[2+MapCoilWrite]), TRUE);

                                g_signal_connect (G_OBJECT (MapButton[2]), "toggled",
 	                   	                  G_CALLBACK (map_CoilW_button_callback), GINT_TO_POINTER ( 0 ));
                                g_signal_connect (G_OBJECT (MapButton[3]), "toggled",
		                                  G_CALLBACK (map_CoilW_button_callback), GINT_TO_POINTER ( 1 ));
                                g_signal_connect (G_OBJECT (MapButton[4]), "toggled",
		                                  G_CALLBACK (map_CoilW_button_callback), GINT_TO_POINTER ( 2 ));
                          break;
                  case 10:
                                //read register/holding map label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                MapButton[5]= gtk_radio_button_new_with_label (NULL, "%W");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[5], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[5]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (MapButton[5]));

                                MapButton[6]= gtk_radio_button_new_with_label (group, "%QW");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[6], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[6]);
 
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (MapButton[5+MapRegisterRead]), TRUE);

                                g_signal_connect (G_OBJECT (MapButton[5]), "toggled",
 	                   	                  G_CALLBACK (map_RegsR_button_callback), GINT_TO_POINTER ( 0 ));
                                g_signal_connect (G_OBJECT (MapButton[6]), "toggled",
		                                  G_CALLBACK (map_RegsR_button_callback), GINT_TO_POINTER ( 1 ));
                          break;
                  case 11:
                                //Write register map label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                MapButton[7]= gtk_radio_button_new_with_label (NULL, "%W");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[7], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[7]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (MapButton[7]));

                                MapButton[8]= gtk_radio_button_new_with_label (group, "%QW");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[8], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[8]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (MapButton[8]));

                                MapButton[9]= gtk_radio_button_new_with_label (group, "%IW");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), MapButton[9], FALSE, TRUE, 0);
                                gtk_widget_show (MapButton[9]);
 
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (MapButton[7+MapRegisterWrite]), TRUE);

                                g_signal_connect (G_OBJECT (MapButton[7]), "toggled",
 	                   	                  G_CALLBACK (map_RegsW_button_callback), GINT_TO_POINTER ( 0 ));
                                g_signal_connect (G_OBJECT (MapButton[8]), "toggled",
		                                  G_CALLBACK (map_RegsW_button_callback), GINT_TO_POINTER ( 1 ));
                                g_signal_connect (G_OBJECT (MapButton[9]), "toggled",
		                                  G_CALLBACK (map_RegsW_button_callback), GINT_TO_POINTER ( 2 ));
                          break;

                default:
		/* Labels */
		LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		gtk_widget_set_usize( LabelComParam[NumLine],250,0 );
		gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		gtk_widget_show( LabelComParam[NumLine] );

		/* Values */
		EntryComParam[NumLine] = gtk_entry_new();
		gtk_widget_set_usize( EntryComParam[NumLine],125,0 );
		gtk_box_pack_start( GTK_BOX(hbox[NumLine]), EntryComParam[NumLine], FALSE, FALSE, 0 );
		gtk_widget_show( EntryComParam[NumLine] );
		gtk_entry_set_text( GTK_ENTRY(EntryComParam[NumLine]), BuffValue );
                }
	}
	return vbox;
}

void GetModbusComParameters( void )
{
        GtkWidget **ComboText;
        int update=0;
        gchar *string;

        ComboText = &ComboComParam[0];
        string = (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)*ComboText)->entry) ;
       // printf("BEFORE port name: ->%s<-->%s<-\n",ModbusSerialPortNameUsed, string);
        if ( strncmp( string, "IP port", strlen("IP port") )==0 )           
             {    
                   if ( strncmp( ModbusSerialPortNameUsed, " ", strlen(" ") )!=0 )  
                      {   update=TRUE;   strcpy( ModbusSerialPortNameUsed,"" );  }
             }else{
                    if ( strncmp( ModbusSerialPortNameUsed, string, strlen(string) )!=0 ) 
                       {
       	                strcpy( ModbusSerialPortNameUsed,string );update=TRUE;
                       }
                  }
      // printf("port name: ->%s<-->%s<-\n",ModbusSerialPortNameUsed, string);
        ComboText = &ComboComParam[1];
        string = (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)*ComboText)->entry) ;
        if ( ModbusSerialSpeed != atoi(string)) {   update=TRUE;   }
	ModbusSerialSpeed = atoi( string );
	ModbusTimeAfterTransmit = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 2 ] )) );
	ModbusTimeInterFrame = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 3 ] )) );
	ModbusTimeOutReceipt = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 4 ] )) );
	if ( (update) && (modmaster) ) 
           {
            MessageInStatusBar( _(" To change Modbus port settings, save and reload ladder GUI"));
           // PrepareModbusMaster( );
            }else{
                  MessageInStatusBar(_("Updated Modbus configuration"));
                 }
}
void GetSettings( void )
{	
#ifdef MODBUS_IO_MASTER
if(modmaster) { 
                GetModbusComParameters( );   
                GetModbusModulesIOSettings( ); 
              }  
#endif
#ifndef RT_SUPPORT
        GetGeneralParameters( );
	ConfigHardware( );
	InfosGene->AskToConfHard = TRUE;
#endif
}

void OpenConfigWindowGtk( void )
{
	if ( !GTK_WIDGET_VISIBLE( ConfigWindow ) )
	{ 
		gtk_widget_show (ConfigWindow);
		MessageInStatusBar(_("Opened Configuration window. Press again to update changes and close"));
#ifdef GTK2
		gtk_window_present( GTK_WINDOW(ConfigWindow) );
#endif
	}
	else
	{
                GetSettings();
		gtk_widget_hide( ConfigWindow );
		
	}
}
gint ConfigWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
        GetSettings();
	gtk_widget_hide( ConfigWindow );
	// we do not want that the window be destroyed.
	return TRUE;
}

void destroyConfigWindow()
{
        gtk_widget_destroy ( ConfigWindow );
}

void IntConfigWindowGtk()
{ 	
        GtkWidget *nbook;
	ConfigWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);        
	gtk_window_set_title( GTK_WINDOW(ConfigWindow), _("Config") );       
	nbook = gtk_notebook_new( );
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateGeneralParametersPage( ),
				 gtk_label_new (_("Period/object info")) );
#ifdef MODBUS_IO_MASTER
        gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateModbusComParametersPage( ),
				 gtk_label_new (_("Modbus communication setup")) );
       
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateModbusModulesIO(  ),
				 gtk_label_new (_("Modbus  I/O register setup ")) );       
#endif
	gtk_container_add( GTK_CONTAINER (ConfigWindow), nbook );
	gtk_widget_show( nbook );
	gtk_window_set_position( GTK_WINDOW(ConfigWindow), GTK_WIN_POS_CENTER );
	gtk_signal_connect ( GTK_OBJECT(ConfigWindow), "delete_event",
                        GTK_SIGNAL_FUNC(ConfigWindowDeleteEvent), NULL );
}


