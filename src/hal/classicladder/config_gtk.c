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
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
//Chris Morley July 08

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
#define NBR_OBJECTS 16
#else
#define NBR_OBJECTS 14
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
static char * ModbusReqType[] = {"Read_INPUTS  fnct- 2", "Write_COIL(S)  fnct-5/15", "Read_REGS     fnct- 4", "Write_REG(S)  fnct-6/16", "Read_COILS  fnct- 1","Read_HOLD    fnct- 3","Slave_echo    fnct- 8",NULL };
#define NBR_MODBUS_PARAMS 6
GtkWidget *ModbusParamEntry[ NBR_MODBUS_MASTER_REQ ][ NBR_MODBUS_PARAMS ];
GtkWidget *SerialPortEntry;
GtkWidget *SerialSpeedEntry;
GtkWidget *PauseInterFrameEntry;
GtkWidget *DebugLevelEntry;

//for modbus configure window
#define NBR_COM_PARAMS 8
GtkWidget *EntryComParam[ NBR_COM_PARAMS ];
GtkWidget *ConfigWindow;
GtkWidget *DebugButton [ 4 ];
GtkWidget *OffsetButton[ 1 ];
GtkWidget *RtsButton   [ 1 ];
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
		char BuffValue[ 20 ];
		int InfoUsed = 0;
		hbox[NumObj] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumObj]);
		gtk_widget_show (hbox[NumObj]);

		switch( NumObj )
		{
			case 0:
				sprintf( BuffLabel, "Rung Refresh Rate (milliseconds)" );
				sprintf( BuffValue, "%d", InfosGene->GeneralParams.PeriodicRefreshMilliSecs );
				break;
			case 1:
				InfoUsed = GetNbrRungsDefined( )*100/InfosGene->GeneralParams.SizesInfos.nbr_rungs;
				sprintf( BuffLabel, "Number of rungs (%d%c used      ", InfoUsed,'%' );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_rungs );
				break;
			case 2:
				sprintf( BuffLabel, "Number of Bits                  " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_bits );
				break;
			case 3:
				sprintf( BuffLabel, "Number of Words                 " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_words );
				break;
			case 4:
				sprintf( BuffLabel, "Number of Counters              " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_counters );
				break;
			case 5:
				sprintf( BuffLabel, "Number of Timers IEC            " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_timers_iec );
				break;
			case 6:
				sprintf( BuffLabel, "Number of BIT Inputs HAL pins           " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_inputs );
				break;
			case 7:
				sprintf( BuffLabel, "Number of BIT Outputs HAL pins          " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_outputs );
				break;
			case 8:
				sprintf( BuffLabel, "Number of Arithmetic Expresions " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_arithm_expr );
				break;
			case 9:
				InfoUsed = NbrSectionsDefined( )*100/InfosGene->GeneralParams.SizesInfos.nbr_sections;
				sprintf( BuffLabel, "Number of Sections (%d%c used)   ", InfoUsed,'%' );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_sections );
				break;
			case 10:
				sprintf( BuffLabel, "Number of Symbols                " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_symbols );
				break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
			case 11:
				sprintf( BuffLabel, "Number of Timers                 " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_timers );
				break;
			case 12:
				sprintf( BuffLabel, "Number of Monostables            " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_monostables );
				break;
#endif
			case 13:
				sprintf( BuffLabel, "Number of S32in HAL pins             " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_inputs );
				break;
			case 14:
				sprintf( BuffLabel, "Number of S32out HAL pins            " );
				sprintf( BuffValue, "%d", GeneralParamsMirror.SizesInfos.nbr_phys_outputs );
				break;
                        case 15:
				sprintf( BuffLabel, "Current path/filename" );
                                sprintf( BuffValue, "Not available yet");
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
		if (NumObj==15) {   gtk_widget_set_usize((GtkWidget *)ValueParam[NumObj],150,0);
                           }else{  
                                    gtk_widget_set_usize((GtkWidget *)ValueParam[NumObj],50,0);  
                                }
		gtk_box_pack_start (GTK_BOX (hbox[NumObj]), ValueParam[NumObj], FALSE, FALSE, 0);
		gtk_widget_show (ValueParam[NumObj]);
		/*if (NumObj==15) {gtk_entry_set_text( GTK_ENTRY(ValueParam[NumObj]),InfosGene->FileName);
                }else{*/
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

#ifdef MODBUS_IO_MASTER
GtkWidget * CreateModbusModulesIO( page )
{
	static char * Labels[] = { "Slave Address", "TypeAccess", "1st Register #", "# of Regs", "Logic", "1st I/Q/W Mapped" };
	GtkWidget *vbox;
	GtkWidget *hbox[ NBR_MODBUS_MASTER_REQ+2 ];
	int NumObj, NumLine, i,ScanDev = 0;
	GList * ItemsDevices = NULL;
	StrModbusMasterReq * pConf;
	char BuffValue[ 40 ];
	GtkWidget *ModbusParamLabel[ NBR_MODBUS_PARAMS];	
	GtkWidget *SerialPortLabel;
	

 	if(modmaster==FALSE) 
            {
             vbox = gtk_vbox_new (FALSE, 0);
              gtk_widget_show (vbox);
              SerialPortLabel = gtk_label_new( "\n  To use modbus you must specify a modbus configure file\n"
		"                        when loading classicladder use: \n \n loadusr classicladder --modmaster myprogram.clp  " );
              gtk_box_pack_start(GTK_BOX (vbox), SerialPortLabel, FALSE, FALSE, 0);
              gtk_widget_show( SerialPortLabel );     
              return vbox;
            }

	do
	{
		ItemsDevices = g_list_append( ItemsDevices, ModbusReqType[ ScanDev++ ] );
	}
	while( ModbusReqType[ ScanDev ] );

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);

	for (i=0; i<(NBR_MODBUS_MASTER_REQ/2+1); i++ )
	{
                NumLine=i+(page*16)+page;
		hbox[NumLine] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumLine]);
		gtk_widget_show (hbox[NumLine]);

		for (NumObj=0; NumObj<NBR_MODBUS_PARAMS; NumObj++)
		{
			switch( NumLine )
			{
				case 0:
                                case 17:
				{
					int length;
					GtkWidget **IOParamLabel = &ModbusParamLabel[ NumObj ];
					switch( NumObj )
							{ 
					
								case 0:
								case 5:
									length=120;
									break;
								case 1:
									length=185;
									break;
								default:
									length=100;
							}
					
					*IOParamLabel = gtk_label_new( Labels[ NumObj ] );

					gtk_widget_set_usize(*IOParamLabel,length,0);
					gtk_box_pack_start(GTK_BOX (hbox[ NumLine]), *IOParamLabel, FALSE, FALSE, 0);
					gtk_widget_show( *IOParamLabel );
					break;
				}
				default:
				{
					pConf = &ModbusMasterReq[ NumLine-1-page ];
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
							gtk_widget_set_usize( *IOParamDevice,185,0 );
							gtk_box_pack_start ( GTK_BOX (hbox[NumLine]), *IOParamDevice, FALSE, FALSE, 0 );
							gtk_widget_show ( *IOParamDevice );
					        	gtk_entry_set_text((GtkEntry*)((GtkCombo *)*IOParamDevice)->entry, ModbusReqType[ ValueToDisplay ]);
							gtk_editable_set_editable( GTK_EDITABLE((GtkEntry*)((GtkCombo *)*IOParamDevice)->entry),FALSE);
							
							break;
						}
						/* For flags */
						case 4:
						{
							GtkWidget **IOParamFlag = &ModbusParamEntry[ NumLine ][ NumObj ];
							*IOParamFlag = gtk_check_button_new_with_label( "Inverted" );
							gtk_widget_set_usize( *IOParamFlag,100,0 );
							gtk_box_pack_start( GTK_BOX (hbox[NumLine]), *IOParamFlag, FALSE, FALSE, 0 );
							gtk_widget_show ( *IOParamFlag );
							if ( pConf->LogicInverted )
								gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( *IOParamFlag ), TRUE );
							break;
						}
						/* For numbers/strings */
						default:
						{int length=100;
							switch( NumObj )
							{ 
								case 0:
									strcpy( BuffValue, pConf->SlaveAdr );
									length=120;
									break;
								case 2:
									sprintf( BuffValue, "%d", pConf->FirstModbusElement );
									length=100;
									break;
								case 3:
									sprintf( BuffValue, "%d", pConf->NbrModbusElements );
									length=100;
									break;
								case 5:
									sprintf( BuffValue, "%d", pConf->OffsetVarMapped );
									length=120;
									break;
							}
							{
								GtkWidget **IOParamEntry = &ModbusParamEntry[ NumLine ][ NumObj ];
								*IOParamEntry = gtk_entry_new( );
								gtk_widget_set_usize( *IOParamEntry,length,0 );
								gtk_box_pack_start( GTK_BOX (hbox[NumLine]), *IOParamEntry, FALSE, FALSE, 0 );
								gtk_widget_show ( *IOParamEntry );
								gtk_entry_set_text( GTK_ENTRY(*IOParamEntry), BuffValue );
							}
							break;
						}
					}
				}//default: bracket
			}
		}
	}
	return vbox;
}
// Have to adjust the numbering system because  &ModbusMasterReq starts at 0 ends at 31
// but in &ModbusParamEntry 0 and 17 are header labels and it ends at 33 of course
void GetModbusModulesIOSettings( void )
{
	int NumObj;
	int NumLine,temp=-1;
	StrModbusMasterReq * pConf;
	GtkWidget **IOParamEntry;
	char * text;
	char BuffValue[ 40 ];

	for (NumLine=1; NumLine<NBR_MODBUS_MASTER_REQ; NumLine++ )// start at line 1 to miss the header label at line 0
	{
            if (NumLine==17)  {    temp--; continue;    }// line 17 is the header label of modbus io register page 2. (set temp to -2)
		pConf = &ModbusMasterReq[ NumLine+temp ];
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
				//	printf("buff-%s\n",BuffValue);
					break;
				case 1://type of request
					pConf->TypeReq = ConvComboToNum( (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)*IOParamEntry)->entry), ModbusReqType );
				//	printf("buff-%s\n",(char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)*IOParamEntry)->entry));
					break;
				case 2://first element address
					text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
					pConf->FirstModbusElement = atoi( text );
					break;
				case 3://number of reqested items
					text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
					pConf->NbrModbusElements = atoi( text );
					break;
				case 4://invert logic?
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
		//TODO much more error checking for word variable. (s32 in and out are mapped on
		//     top of word variables, in first, then out, then regular words).
		switch( pConf->TypeReq )
		{
			case	MODBUS_REQ_INPUTS_READ :
				if ( pConf->OffsetVarMapped+pConf->NbrModbusElements>= NBR_PHYS_INPUTS )
				{
					printf("Error in I/O modbus configure table: Asking to map more modbus inputs then there are I Variables.\n" );
					strcpy( BuffValue, "" );
				}
				break;

			case MODBUS_REQ_COILS_WRITE :
				if ( pConf->OffsetVarMapped+pConf->NbrModbusElements>= NBR_PHYS_OUTPUTS )
				{
					printf("Error in I/O modbus configure table: Asking to map more modbus coils then there are Q Variables.\n" );
					strcpy( BuffValue, "" );
				}
				break;

			case	MODBUS_REQ_REGISTERS_READ :
			case    MODBUS_REQ_HOLD_READ :
				if ( pConf->OffsetVarMapped+pConf->NbrModbusElements>= NBR_WORDS )
				{
					printf("Error in I/O modbus configure table: Asking to map more modbus registers then there are W Variables.\n" );
					strcpy( BuffValue, "" );
				}
/*				if ( pConf->OffsetVarMapped< NBR_S32IN )
				{
					printf("Error in I/O modbus configure table: Cannot map modbus READ registers onto S32 in Variables.\n" );
					strcpy( BuffValue, "" );
				}
				break;
*/
			case	 MODBUS_REQ_REGISTERS_WRITE :
				if ( pConf->OffsetVarMapped+pConf->NbrModbusElements>= NBR_WORDS )
				{
					printf("Error in I/O modbus configure table: Asking to map more modbus registers then there are W Variables.\n" );
					strcpy( BuffValue, "" );
				}
				break;
                       case     MODBUS_REQ_DIAGNOSTICS :
                                break;
                       case     MODBUS_REQ_COILS_READ :
                                if ( pConf->OffsetVarMapped+pConf->NbrModbusElements>= NBR_PHYS_OUTPUTS )
				{
					printf("Error in I/O modbus configure table: Asking to map more modbus coils then there are Q Variables.\n" );
					strcpy( BuffValue, "" );
				}
                                break;
                       default:
                                printf("Error in I/O modbus configure table: Modbus function not recognized");
                                break;

		}
		/* done at the end, do not forget multi-task ! */
		/* the first char is tested to determine a valid request => paranoia mode ;-) */
		//printf("buffvalue1=%s buffvalue0=%d \n",&BuffValue[ 1 ],BuffValue[ 0 ]);
		strcpy( &pConf->SlaveAdr[ 1 ], &BuffValue[ 1 ] );
		pConf->SlaveAdr[ 0 ] = BuffValue[ 0 ];
	}//for (NumLine=0; 
}
#endif

// These three callback functions will change the global variable as soon as the radio button is changed
// you don't have to close the window to update them
 void debug_button_callback (GtkWidget *widget, gint data)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[0])))  { ModbusDebugLevel = 0; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[1])))  { ModbusDebugLevel = 1; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[2])))  { ModbusDebugLevel = 2; } 
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (DebugButton[3])))  { ModbusDebugLevel = 3; } 
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
        GSList *group;
	int NumLine;
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
				sprintf( BuffLabel, "Serial port (blank = IP mode)" );
				strcpy( BuffValue, ModbusSerialPortNameUsed );
				break;
			case 1:
				sprintf( BuffLabel, "Serial baud rate" );
				sprintf( BuffValue, "%d", ModbusSerialSpeed );
				break;
                        case 2:
				sprintf( BuffLabel, "After transmit pause - milliseconds" );
				sprintf( BuffValue, "%d", ModbusTimeAfterTransmit );
				break;
			
			case 3:
				sprintf( BuffLabel, "After receive pause - milliseconds" );
				sprintf( BuffValue, "%d", ModbusTimeInterFrame );
				break;
			case 4:
				sprintf( BuffLabel, "Request Timeout length - milliseconds" );
				sprintf( BuffValue, "%d", ModbusTimeOutReceipt );
				break;
			case 5:
				sprintf( BuffLabel, "Use RTS to send" );
				sprintf( BuffValue, "%d", ModbusSerialUseRtsToSend );
				break;
			case 6:
				sprintf( BuffLabel, "Modbus element offset" );
				sprintf( BuffValue, "%d", ModbusEleOffset );
				break;
			case 7:
				sprintf( BuffLabel, "Debug level" );
				sprintf( BuffValue, "%d", ModbusDebugLevel );
				break;
		}
            switch( NumLine )
		{
                         case 5:
                                //RTS label
                                LabelComParam[NumLine] = gtk_label_new(BuffLabel);
		                gtk_widget_set_usize( LabelComParam[NumLine],200,0 );
		                gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		                gtk_widget_show( LabelComParam[NumLine] ); 

                                //radio buttons
                                RtsButton[0]= gtk_radio_button_new_with_label (NULL, "NO");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), RtsButton[0], FALSE, TRUE, 0);
                                gtk_widget_show (RtsButton[0]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (RtsButton[0]));

                                RtsButton[1]= gtk_radio_button_new_with_label (group, "YES");
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
                                DebugButton[0]= gtk_radio_button_new_with_label (NULL, "QUIET");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), DebugButton[0], FALSE, TRUE, 0);
                                gtk_widget_show (DebugButton[0]);
                                group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (DebugButton[0]));

                                DebugButton[1]= gtk_radio_button_new_with_label (group, "LEVEL 1");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), DebugButton[1], FALSE, TRUE, 0);
                                gtk_widget_show (DebugButton[1]);
 
                                DebugButton[2]= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (DebugButton[0]),"LEVEL 2");
                                gtk_box_pack_start (GTK_BOX (hbox[NumLine]), DebugButton[2], FALSE, TRUE, 0);
                                gtk_widget_show (DebugButton[2]);

                                DebugButton[3]= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (DebugButton[0]),"LEVEL 3");
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
        int update=0;
        if ( strncmp( ModbusSerialPortNameUsed, gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 0 ] )),
            strlen( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 0 ] ))) )!=0 )               {   update=TRUE;   }
        if ( ModbusSerialSpeed != atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 1 ] )) ) ) {   update=TRUE;   }

	strcpy( ModbusSerialPortNameUsed, gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 0 ] )));
	ModbusSerialSpeed = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 1 ] )) );
	ModbusTimeAfterTransmit = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 2 ] )) );
	ModbusTimeInterFrame = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 3 ] )) );
	ModbusTimeOutReceipt = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 4 ] )) );
	if ( (update) && (modmaster) ) {    PrepareModbusMaster( );    }
}
void GetSettings( void )
{	
#ifdef MODBUS_IO_MASTER
if(modmaster) {  GetModbusComParameters( );   
                 GetModbusModulesIOSettings( );   }
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
		MessageInStatusBar("Openned Configuration window. Press again to update changes and close");
#ifdef GTK2
		gtk_window_present( GTK_WINDOW(ConfigWindow) );
#endif
	}
	else
	{
                GetSettings();
		gtk_widget_hide( ConfigWindow );
		MessageInStatusBar("Updated Modbus configuration");
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
	gtk_window_set_title( GTK_WINDOW(ConfigWindow), "Config" );

	nbook = gtk_notebook_new( );
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateGeneralParametersPage( ),
				 gtk_label_new ("Period/object info") );
	//gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateIOConfPage( 1/*ForInputs*/ ),
	//			 gtk_label_new ("Physical Inputs") );
	//gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateIOConfPage( 0/*ForInputs*/ ),
	//			 gtk_label_new ("Physical Outputs") );
#ifdef MODBUS_IO_MASTER
        gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateModbusComParametersPage( ),
				 gtk_label_new ("Modbus communication setup") );
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateModbusModulesIO( 0 ),
				 gtk_label_new ("Modbus  I/O register setup 1") );
        gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateModbusModulesIO( 1 ),
				 gtk_label_new ("Modbus  I/O register setup 2") );
#endif

	gtk_container_add( GTK_CONTAINER (ConfigWindow), nbook );
	gtk_widget_show( nbook );

	gtk_window_set_position( GTK_WINDOW(ConfigWindow), GTK_WIN_POS_CENTER );
	gtk_signal_connect ( GTK_OBJECT(ConfigWindow), "delete_event",
                        GTK_SIGNAL_FUNC(ConfigWindowDeleteEvent), NULL );
}


