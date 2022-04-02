/* Classic Ladder Project */
/* Copyright (C) 2001-2009 Marc Le Douarain */
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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h> // i18n
#include <locale.h> // i18n
#include "classicladder.h"
#include "manager.h"
#include "edit.h"
//#include "hardware.h"
#include "global.h"
#include "classicladder_gtk.h"
#include "vars_names.h"
//#include "log.h"
#include "socket_modbus_master.h"
#include "config_gtk.h"
#include <rtapi_string.h>

#ifdef OLD_TIMERS_MONOS_SUPPORT
#define NBR_OBJECTS 19
#else
#define NBR_OBJECTS 17
#endif
#define NBR_SUBS_VBOX 3
GtkWidget *LabelParam[ NBR_OBJECTS ],*ValueParam[ NBR_OBJECTS ];

static char * Devices[] = { N_("None"), N_("DirectPortAccess"),
#ifdef COMEDI_SUPPORT
"/dev/comedi0", "/dev/comedi1", "/dev/comedi2", "/dev/comedi3",
#endif
 NULL };

#define NBR_IO_PARAMS 6
GtkWidget *InputParamEntry[ NBR_INPUTS_CONF ][ NBR_IO_PARAMS ];
GtkWidget *InputDeviceParam[ NBR_INPUTS_CONF ];
GtkWidget *InputFlagParam[ NBR_INPUTS_CONF ];

GtkWidget *OutputParamEntry[ NBR_OUTPUTS_CONF ][ NBR_IO_PARAMS ];
GtkWidget *OutputDeviceParam[ NBR_OUTPUTS_CONF ];
GtkWidget *OutputFlagParam[ NBR_OUTPUTS_CONF ];

#ifdef MODBUS_IO_MASTER
// ModbusReqType must be in the same order as MODBUS_REQ_ in protocol_modbus_master.h
static char * ModbusReqType[] = { N_("ReadInputs (to %I)"), N_("WriteCoils (from %Q)"), N_("ReadInputRegs (to %IW)"), N_("WriteHoldRegs (from %QW)"), N_("ReadCoils (to %Q)"), N_("ReadHoldRegs (to %QW)"), N_("ReadStatus (to %IW)"), N_("Diagnostic (from %IW/to %QW - 1stEle=sub-code used)"), NULL };
#define NBR_MODBUS_PARAMS 6
GtkWidget *ModbusParamEntry[ NBR_MODBUS_MASTER_REQ ][ NBR_MODBUS_PARAMS ];
//GtkWidget *SerialPortEntry;
//GtkWidget *SerialSpeedEntry;
//GtkWidget *PauseInterFrameEntry;
//GtkWidget *DebugLevelEntry;
#define NBR_COM_PARAMS 17
#define NBR_RADIO_BUT_COM_PARAMS 5
GtkWidget *EntryComParam[ NBR_COM_PARAMS ];
GtkWidget *RadioButComParams[ NBR_COM_PARAMS ][ NBR_RADIO_BUT_COM_PARAMS ];
#endif

#define NBR_CONFIG_EVENTS_PARAMS 5
//GtkWidget *EventConfigParamEntry[ NBR_CONFIG_EVENTS_LOG ][ NBR_CONFIG_EVENTS_PARAMS ];

GtkWidget *ConfigWindow;

GtkWidget * CreateGeneralParametersPage( void )
{
	GtkWidget *vbox_main;
	GtkWidget *hbox[ NBR_OBJECTS ];
	GtkWidget *hbox_for_subs;
	GtkWidget *vbox_sub[ NBR_SUBS_VBOX ];
	GtkWidget *vbox_whitespace;
	int NumObj,ScanSub;
	int CurrentSub = 0;

	vbox_main = gtk_vbox_new (FALSE, 0);

	hbox_for_subs = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(vbox_main), hbox_for_subs);
	for( ScanSub=0; ScanSub<NBR_SUBS_VBOX; ScanSub++ )
	{
		vbox_sub[ ScanSub ] = gtk_vbox_new (FALSE, 0);
		if ( ScanSub==1 )
		{
			GtkWidget * VertSep = gtk_vseparator_new( );
			gtk_widget_set_size_request( VertSep, 10, -1 );
			gtk_box_pack_start (GTK_BOX(vbox_sub[ScanSub]), VertSep, TRUE/*expand*/, TRUE/*fill*/, 0);
		}
		gtk_container_add (GTK_CONTAINER(hbox_for_subs), vbox_sub[ScanSub]);
	}

	for (NumObj=0; NumObj<NBR_OBJECTS; NumObj++)
	{
		char BuffLabel[ 100 ];
		char BuffValue[ 200 ];
                
		int InfoUsed = 0;
		hbox[NumObj] = gtk_hbox_new (FALSE, 0);

		switch( NumObj )
		{
			case 0:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Rung Refresh Rate (milliseconds)") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", InfosGene->GeneralParams.PeriodicRefreshMilliSecs );
				break;
			case 1:
				InfoUsed = GetNbrRungsDefined( )*100/InfosGene->GeneralParams.SizesInfos.nbr_rungs;
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of rungs (%d%c used)"), InfoUsed,'%' );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_rungs );
				break;
			case 2:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Bits") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_bits );
				break;
			case 3:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Words") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_words );
				break;
			case 4:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Counters") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_counters );
				break;
			case 5:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Timers IEC") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_timers_iec );
				break;
			case 6:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of BIT Inputs HAL pins") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_phys_inputs );
				break;
			case 7:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of BIT Outputs HAL pins") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_phys_outputs );
				break;
			case 8:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of S32in HAL pins") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_phys_words_inputs );
				break;
			case 9:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of S32out HAL pins") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_phys_words_outputs );
				break;
			case 10:
				CurrentSub += 2;
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Arithmetic Expressions") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_arithm_expr );
				break;
			case 11:
				InfoUsed = NbrSectionsDefined( )*100/InfosGene->GeneralParams.SizesInfos.nbr_sections;
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Sections (%d%c used)"), InfoUsed,'%' );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_sections );
				break;
//#endif
			case 12:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Symbols") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_symbols );
				break;
#ifdef OLD_TIMERS_MONOS_SUPPORT
			case 13:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Timers") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_timers );
				break;
			case 14:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Monostables") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_monostables );
				break;
#endif
			case 15:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of Error Bits") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_error_bits );
				break;
			case 16:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of floatin HAL pins") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_phys_float_inputs );
				break;
			case 17:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Number of floatout HAL pins") );
				snprintf(BuffValue, sizeof(BuffValue), "%d", GeneralParamsMirror.SizesInfos.nbr_phys_float_outputs );
				break;
			case 18:
				snprintf(BuffLabel, sizeof(BuffLabel), _("Current path/filename:") );
				size_t ret = snprintf(BuffValue, sizeof(BuffValue), "%s",InfosGene->CurrentProjectFileName);
				if (ret >= sizeof(BuffLabel)) snprintf(BuffValue, sizeof(BuffValue), "<path too long>");
                                //snprintf(BuffValue, sizeof(BuffValue), "Not available yet" );
				break;
			default:
				snprintf(BuffLabel, sizeof(BuffLabel), "???" );
				snprintf(BuffValue, sizeof(BuffValue), "???" );
				break;
		}

			LabelParam[NumObj] = gtk_label_new(BuffLabel);
//ForGTK3			gtk_widget_set_usize(/*(GtkWidget *)*/LabelParam[NumObj],(NumObj<NBR_OBJECTS_GENERAL-2)?420:180,0);
			if( NumObj<NBR_OBJECTS-1 )
				gtk_widget_set_size_request(LabelParam[NumObj],350,-1);
			gtk_misc_set_alignment(GTK_MISC(LabelParam[NumObj]), 0.0f, 0.5f);
			gtk_box_pack_start (GTK_BOX(hbox[NumObj]), LabelParam[NumObj], FALSE, FALSE, 5);

			/* For numbers */
			ValueParam[NumObj] = gtk_entry_new();
//ForGTK3			gtk_widget_set_usize(/*(GtkWidget *)*/ValueParam[NumObj],(NumObj<NBR_OBJECTS_GENERAL-2)?50:450,0);
			if (NumObj<NBR_OBJECTS-1)
				gtk_widget_set_size_request(/*(GtkWidget *)*/ValueParam[NumObj],45,-1);
			gtk_box_pack_start (GTK_BOX(hbox[NumObj]), ValueParam[NumObj], NumObj>=NBR_OBJECTS-1 /*expand*/, NumObj>=NBR_OBJECTS-1 /*fill*/, 5);
			gtk_entry_set_text( GTK_ENTRY(ValueParam[NumObj]), BuffValue );
			gtk_widget_set_sensitive( ValueParam[NumObj],FALSE);

			if ( NumObj<NBR_OBJECTS-1 )
				gtk_container_add (GTK_CONTAINER(vbox_sub[CurrentSub]), hbox[NumObj]);
			else {
				vbox_whitespace = gtk_vbox_new(FALSE, 0);
				gtk_box_pack_start(GTK_BOX(vbox_main), vbox_whitespace, TRUE, TRUE, 0);
				gtk_container_add (GTK_CONTAINER(vbox_main), hbox[NumObj]);
			}

	}
	gtk_widget_show_all(vbox_main);
	return vbox_main;
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
	GeneralParamsMirror.SizesInfos.nbr_phys_words_inputs = TheValue;
	TheValue = GetOneGeneralInfo( 9 );
	GeneralParamsMirror.SizesInfos.nbr_phys_words_outputs = TheValue;
	TheValue = GetOneGeneralInfo( 10 );
	GeneralParamsMirror.SizesInfos.nbr_arithm_expr = TheValue;
	TheValue = GetOneGeneralInfo( 11 );
	GeneralParamsMirror.SizesInfos.nbr_sections = TheValue;
	TheValue = GetOneGeneralInfo( 12 );
	GeneralParamsMirror.SizesInfos.nbr_symbols = TheValue;
#ifdef OLD_TIMERS_MONOS_SUPPORT
	TheValue = GetOneGeneralInfo( 13 );
	GeneralParamsMirror.SizesInfos.nbr_timers = TheValue;
	TheValue = GetOneGeneralInfo( 14 );
	GeneralParamsMirror.SizesInfos.nbr_monostables = TheValue;
#endif
}

void AddDevicesListToComboBox( MyGtkComboBox * pComboBox )
{
	int ScanDev = 0;
	do
	{
		gtk_combo_box_append_text( pComboBox, gettext(Devices[ ScanDev++ ]) );
	}
	while( Devices[ ScanDev ] );
}

GtkWidget * CreateIOConfPage( char ForInputs )
{
	static char * Labels[] = { N_("First %"), N_("Type"), N_("PortAdr(0x)/SubDev"), N_("First Channel"), N_("Nbr Channels"), N_("Logic") };
	static int LabelsSize[] = { 105, 130, 130, 120, 130, 105 };
	GtkWidget *scrolled_win;
	GtkWidget *table;
	int NumObj;
	int NumLine;
	StrIOConf * pConf;
	GtkWidget *InputParamLabel[ NBR_IO_PARAMS];
	GtkWidget *OutputParamLabel[ NBR_IO_PARAMS ];
	table = gtk_table_new( NBR_IO_PARAMS, 1+(ForInputs?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF), FALSE );

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolled_win),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (scrolled_win), table );

	for (NumLine=-1; NumLine<(ForInputs?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF); NumLine++ )
	{
		
		for (NumObj=0; NumObj<NBR_IO_PARAMS; NumObj++)
		{
			if ( NumLine==-1 )
			{
				GtkWidget **IOParamLabel = ForInputs?&InputParamLabel[ NumObj ]:&OutputParamLabel[ NumObj ];
				if ( NumObj==0 )
					*IOParamLabel = gtk_label_new( ForInputs?(_("1st %I mapped")):(_("1st %Q mapped")) );
				else
					*IOParamLabel = gtk_label_new( gettext(Labels[ NumObj ]) );
				gtk_widget_set_size_request( *IOParamLabel, LabelsSize[NumObj],-1 );
				gtk_table_attach_defaults (GTK_TABLE (table), *IOParamLabel, NumObj, NumObj+1, NumLine+1, NumLine+2);
			}
			else
			{
				char BuffValue[ 30 ];
				if ( ForInputs )
					pConf = &InfosGene->InputsConf[ NumLine ];
				else
					pConf = &InfosGene->OutputsConf[ NumLine ];

				switch( NumObj )
				{
					/* For devices */
					case 1:
					{
						int ComboValueToDisplay = 0;
						{
							if ( pConf->DeviceType==DEVICE_TYPE_DIRECT_ACCESS )
								ComboValueToDisplay = 1;
							else if ( pConf->DeviceType>=DEVICE_TYPE_COMEDI )
								ComboValueToDisplay = pConf->DeviceType-DEVICE_TYPE_COMEDI+4;
						}
						{
							GtkWidget **IOParamDevice = ForInputs?&InputDeviceParam[ NumLine ]:&OutputDeviceParam[ NumLine ];
							*IOParamDevice = gtk_combo_box_new_text();
							AddDevicesListToComboBox( MY_GTK_COMBO_BOX( *IOParamDevice ) );
							gtk_widget_set_size_request( *IOParamDevice,LabelsSize[NumObj],-1 );
							gtk_table_attach_defaults (GTK_TABLE (table), *IOParamDevice, NumObj, NumObj+1, NumLine+1, NumLine+2);
							gtk_combo_box_set_active( GTK_COMBO_BOX( *IOParamDevice ), ComboValueToDisplay );
						}
						break;
					}
					/* For flags */
					case 5:
					{
						GtkWidget **IOParamFlag = ForInputs?&InputFlagParam[ NumLine ]:&OutputFlagParam[ NumLine ];
						*IOParamFlag = gtk_check_button_new_with_label( _("Inverted") );
						gtk_widget_set_size_request( *IOParamFlag,LabelsSize[NumObj],-1 );
						gtk_table_attach_defaults (GTK_TABLE (table), *IOParamFlag, NumObj, NumObj+1, NumLine+1, NumLine+2);
						if ( pConf->FlagInverted )
							gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( *IOParamFlag ), TRUE );
						break;
					}
					/* For numbers */
					default:
					{
						switch( NumObj )
						{
							case 0:
								if ( pConf->FirstClassicLadderIO==-1 )
									rtapi_strxcpy( BuffValue, "" );
								else
									snprintf( BuffValue, sizeof(BuffValue), "%d", pConf->FirstClassicLadderIO );
								break;
							case 2:
								if ( pConf->DeviceType==DEVICE_TYPE_DIRECT_ACCESS )
									snprintf( BuffValue, sizeof(BuffValue), "%X", pConf->SubDevOrAdr );
								else
									snprintf( BuffValue, sizeof(BuffValue), "%d", pConf->SubDevOrAdr );
								break;
							case 3:
								snprintf( BuffValue, sizeof(BuffValue), "%d", pConf->FirstChannel ); break;
							case 4:
								snprintf( BuffValue, sizeof(BuffValue), "%d", pConf->NbrConsecutivesChannels ); break;
						}
						{
							GtkWidget **IOParamEntry = ForInputs?&InputParamEntry[ NumLine ][ NumObj ]:&OutputParamEntry[ NumLine ][ NumObj ];
							*IOParamEntry = gtk_entry_new( );
							gtk_widget_set_size_request( *IOParamEntry,LabelsSize[NumObj],-1 );
							gtk_table_attach_defaults (GTK_TABLE (table), *IOParamEntry, NumObj, NumObj+1, NumLine+1, NumLine+2);
							gtk_entry_set_text( GTK_ENTRY(*IOParamEntry), BuffValue );
						}
						break;
					}
				}
			}
		}//for (NumObj=0;
	}
	gtk_widget_show_all( scrolled_win );

//TODO: I've not found how to not have all the hbox vertically expanded...?
/*for(testpack=0; testpack<30; testpack++)
{		
		hbox[(ForInputs?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF)+1+testpack] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[(ForInputs?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF)+1+testpack]);
//gtk_box_pack_start(GTK_BOX(vbox), hbox[ (ForInputs?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF)+1+testpack ], TRUE, TRUE, 0);
		gtk_widget_show (hbox[(ForInputs?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF)+1+testpack]);
}
*/
	
	return scrolled_win;
}

void GetIOSettings( char ForInputs )
{
	int NumObj;
	int NumLine;
	StrIOConf * pConf;
	int ComboVal;
	GtkWidget **IOParamDevice;
	GtkWidget **IOParamEntry;
	GtkWidget **IOParamFlag;
	char * text;
	for (NumLine=0; NumLine<(ForInputs?NBR_INPUTS_CONF:NBR_OUTPUTS_CONF); NumLine++ )
	{
		if ( ForInputs )
			pConf = &InfosGene->InputsConf[ NumLine ];
		else
			pConf = &InfosGene->OutputsConf[ NumLine ];

		pConf->FirstClassicLadderIO = -1;
		pConf->FlagInverted = 0;

		IOParamDevice = ForInputs?&InputDeviceParam[ NumLine ]:&OutputDeviceParam[ NumLine ];
		ComboVal = gtk_combo_box_get_active( GTK_COMBO_BOX( *IOParamDevice ) );
		if ( ComboVal>0 )
		{
			int FirstIO = -1;
			int DeviceTypeValue = DEVICE_TYPE_NONE;
			if ( ComboVal==1 )
				DeviceTypeValue = DEVICE_TYPE_DIRECT_ACCESS;
			else
				DeviceTypeValue = DEVICE_TYPE_COMEDI+ComboVal-4;
			IOParamFlag = ForInputs?&InputFlagParam[ NumLine ]:&OutputFlagParam[ NumLine ];
			if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( *IOParamFlag ) ) )
				pConf->FlagInverted = 1;
			for (NumObj=0; NumObj<NBR_IO_PARAMS; NumObj++)
			{
				IOParamEntry = ForInputs?&InputParamEntry[ NumLine ][ NumObj ]:&OutputParamEntry[ NumLine ][ NumObj ];
				switch( NumObj )
				{
					case 0:
						text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
						if ( text[0]!='\0' )
							FirstIO = atoi( text );
						break;
					case 2:
						text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
						if ( DeviceTypeValue==DEVICE_TYPE_DIRECT_ACCESS )
							sscanf( text, "%X", &pConf->SubDevOrAdr );
						else
							pConf->SubDevOrAdr = atoi( text );
						break;
					case 3:
						text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
						pConf->FirstChannel = atoi( text );
						break;
					case 4:
						text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
						pConf->NbrConsecutivesChannels = atoi( text );
						break;
				}
			}
			/* verify if not overflowing */
			if ( FirstIO+pConf->NbrConsecutivesChannels>( ForInputs?NBR_PHYS_INPUTS:NBR_PHYS_OUTPUTS ) )
			{
				printf("Error in I/O conf: overflow for Ixx or Qxx mapping detected...\n" );
				FirstIO = -1;
			}
			/* done at the end, do not forget multi-task ! */
			pConf->FirstClassicLadderIO = FirstIO;
			pConf->DeviceType = DeviceTypeValue;
		}//if ( ComboVal>0 )
	}
}

#ifdef MODBUS_IO_MASTER
void FillComboBoxReqType( MyGtkComboBox * pComboBox, char * ListTextsCombo[] )
{
	int ScanDev = 0;
	do
	{
		gtk_combo_box_append_text( pComboBox, gettext(ListTextsCombo[ ScanDev++ ]) );
	}
	while( ListTextsCombo[ ScanDev ] );
}
GtkWidget * CreateModbusModulesIO( void )
{
	const char * Labels[] = { N_("Slave Address"), N_("Request Type"), N_("1st Modbus Ele."), N_("# of Ele"), N_("Logic"), N_("1st Variable mapped") };
	GtkWidget *table;
	GtkWidget *scrolled_win;
	int NumObj;
	int NumLine;
	GtkWidget *ModbusParamLabel[ NBR_MODBUS_PARAMS];

    if(modmaster==FALSE)
    {
        GtkWidget *vbox;
        vbox = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox);
        ModbusParamLabel[0] = gtk_label_new(
                "\nTo use modbus you must specify a modbus configure file\n"
                "when loading classicladder use:\n\n"
                "loadusr classicladder --modmaster myprogram.clp" );
        gtk_label_set_justify(GTK_LABEL(ModbusParamLabel[0]), GTK_JUSTIFY_CENTER);
        gtk_box_pack_start(GTK_BOX (vbox), ModbusParamLabel[0], FALSE, FALSE, 0);
        gtk_widget_show( ModbusParamLabel[0] );
        return vbox;
    }

	table = gtk_table_new( NBR_MODBUS_PARAMS, 1+NBR_MODBUS_MASTER_REQ, FALSE );
	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolled_win),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (scrolled_win), table );

	for (NumLine=-1; NumLine<NBR_MODBUS_MASTER_REQ; NumLine++ )
	{

		for (NumObj=0; NumObj<NBR_MODBUS_PARAMS; NumObj++)
		{
			GtkWidget **CurrentWidget;
			if ( NumLine==-1 )
			{
				CurrentWidget = &ModbusParamLabel[ NumObj ];
				*CurrentWidget = gtk_label_new( gettext(Labels[ NumObj ]) );
			}
			else
			{
				StrModbusMasterReq * pConf = &ModbusMasterReq[ NumLine ];
				switch( NumObj )
				{
					/* For slave index + req type (combo-list) */
					case 1:
					{
						CurrentWidget = &ModbusParamEntry[ NumLine ][ NumObj ];
						*CurrentWidget = gtk_combo_box_new_text( );
						FillComboBoxReqType( MY_GTK_COMBO_BOX( *CurrentWidget ), ModbusReqType );
						gtk_combo_box_set_active( GTK_COMBO_BOX( *CurrentWidget ), pConf->TypeReq );
						break;
					}
					/* For flags (checkbutton)*/
					case 4:
					{
						CurrentWidget = &ModbusParamEntry[ NumLine ][ NumObj ];
						*CurrentWidget = gtk_check_button_new_with_label( _("Inverted") );
						if ( pConf->LogicInverted )
							gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( *CurrentWidget ), TRUE );
						break;
					}
					/* For numbers/strings (edit widgets)*/
					default:
					{
						char BuffValue[ 30 ];
						switch( NumObj )
						{
							case 0:
								rtapi_strxcpy( BuffValue, pConf->SlaveAdr );
								break;
							case 2:
								snprintf( BuffValue, sizeof(BuffValue), "%d", pConf->FirstModbusElement );
								break;
							case 3:
								snprintf( BuffValue, sizeof(BuffValue), "%d", pConf->NbrModbusElements );
								break;
							case 5:
								snprintf( BuffValue, sizeof(BuffValue), "%d", pConf->OffsetVarMapped );
								break;
						}
						CurrentWidget = &ModbusParamEntry[ NumLine ][ NumObj ];
						*CurrentWidget = gtk_entry_new( );
						gtk_entry_set_text( GTK_ENTRY(*CurrentWidget), BuffValue );
						break;
					}
				}//switch( NumObj )
			}//!if ( NumLine==-1 )
			gtk_table_attach_defaults (GTK_TABLE (table), *CurrentWidget, NumObj, NumObj+1, NumLine+1, NumLine+2);
		}//for (NumObj
	}//for (NumLine
	gtk_widget_show_all( scrolled_win );
	return scrolled_win;
}
void GetModbusModulesIOSettings( void )
{
	int NumObj;
	int NumLine;
	char BuffValue[ 40 ];
	for (NumLine=0; NumLine<NBR_MODBUS_MASTER_REQ; NumLine++ )
	{
		int MaxVars = 0;
		char DoVerify = FALSE;
		StrModbusMasterReq * pConf = &ModbusMasterReq[ NumLine ];
		rtapi_strxcpy( pConf->SlaveAdr, "" );

		for (NumObj=0; NumObj<NBR_MODBUS_PARAMS; NumObj++)
		{
			GtkWidget **IOParamEntry;
			char * text;
			IOParamEntry = &ModbusParamEntry[ NumLine ][ NumObj ];
			switch( NumObj )
			{
				case 0://slave address
                    text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
                    rtapi_strxcpy( BuffValue, text );
					break;
				case 1://type of request
					pConf->TypeReq = gtk_combo_box_get_active( GTK_COMBO_BOX(*IOParamEntry) );
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
					pConf->LogicInverted = ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( *IOParamEntry ) ) )?1:0;
					break;
				case 5:// first classicladder variable map location
					text = (char *)gtk_entry_get_text((GtkEntry *)*IOParamEntry);
					pConf->OffsetVarMapped = atoi( text );
					break;
			}
		}//for (NumObj=0;
		/* a slave is defined ? */
		/* verify if not overflowing */
		//TODO much more error checking for word variable. 
			switch( pConf->TypeReq )
			{
				case MODBUS_REQ_INPUTS_READ: MaxVars = GetSizeVarsForTypeVar( ModbusConfig.MapTypeForReadInputs ); DoVerify = TRUE; break;
				case MODBUS_REQ_COILS_WRITE: MaxVars = GetSizeVarsForTypeVar( ModbusConfig.MapTypeForWriteCoils ); DoVerify = TRUE; break;
				case MODBUS_REQ_COILS_READ: MaxVars = GetSizeVarsForTypeVar( ModbusConfig.MapTypeForReadCoils ); DoVerify = TRUE; break;
				case MODBUS_REQ_INPUT_REGS_READ: MaxVars = GetSizeVarsForTypeVar( ModbusConfig.MapTypeForReadInputRegs ); DoVerify = TRUE; break;
				case MODBUS_REQ_HOLD_REGS_WRITE: MaxVars = GetSizeVarsForTypeVar( ModbusConfig.MapTypeForWriteHoldRegs ); DoVerify = TRUE; break;
				case MODBUS_REQ_HOLD_REGS_READ: MaxVars = GetSizeVarsForTypeVar( ModbusConfig.MapTypeForReadHoldRegs ); DoVerify = TRUE; break;
			}
			if ( DoVerify )
			{
				if ( pConf->OffsetVarMapped+pConf->NbrModbusElements>MaxVars )
				{
					printf(_("Error in I/O modbus conf: overflow for I,Q,B,IQ, or WQ mapping detected...ASKED=%i,MAX=%i\n"),  pConf->OffsetVarMapped+pConf->NbrModbusElements,MaxVars);
					rtapi_strxcpy( BuffValue, "" );
					ShowMessageBoxError( _("Overflow error for I,Q,B,IQ,WQ or W mapping detected...") );
				}
			}
		/* done at the end, do not forget multi-task ! */
		/* the first char is tested to determine a valid request => paranoia mode ;-) */
		//printf("buffvalue1=%s buffvalue0=%d \n",&BuffValue[ 1 ],BuffValue[ 0 ]);
		strcpy( &pConf->SlaveAdr[ 1 ], &BuffValue[ 1 ] );
		pConf->SlaveAdr[ 0 ] = BuffValue[ 0 ];
	}//for (NumLine=0; 
}

// return nbr of fields found
int SplitCommasFieldsInPointersArray( char * LineDatas, char * PtrFieldsDatasFound[], int NbrMaxFields )
{
	int ScanField;
	for( ScanField=0; ScanField<NbrMaxFields; ScanField++ )
		PtrFieldsDatasFound[ ScanField ] = NULL;
	ScanField = 0;
	PtrFieldsDatasFound[ ScanField++ ] = LineDatas;
	do
	{
		do
		{
			// comma ?
			if ( *LineDatas==',' && *(LineDatas+1)!='\0' )
			{
				// test if not an empty field...
				if ( *(LineDatas+1)!=',' )
				{
					PtrFieldsDatasFound[ ScanField ] = LineDatas+1;
					*LineDatas = '\0';
				}
				ScanField++;
			}
			LineDatas++;
		}
		while( ScanField<NbrMaxFields-1 && *LineDatas!='\0' );
	}
	while( ScanField<NbrMaxFields-1 && *LineDatas!='\0' );
	return ScanField;
}
#define MODBUS_COM_PARAMS_SIZE_X_TABLE 5
GtkWidget * CreateModbusComParametersPage( void )
{
//	GtkWidget *vbox;
//	GtkWidget *hbox[ NBR_COM_PARAMS ];
	GtkWidget *hbox;
	GtkWidget *table;
	GtkWidget * LabelComParam[ NBR_COM_PARAMS ];
	int NumLine;
	char BuffLabel[ 50 ];
	char BuffValue[ 100 ];

    if(modmaster==FALSE)
    {
        GtkWidget *vbox;
        vbox = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox);
        LabelComParam[0] = gtk_label_new(
                "\nTo use modbus you must specify a modbus configure file\n"
                "when loading classicladder use:\n\n"
                "loadusr classicladder --modmaster myprogram.clp" );
        gtk_label_set_justify(GTK_LABEL(LabelComParam[0]), GTK_JUSTIFY_CENTER);
        gtk_box_pack_start(GTK_BOX (vbox), LabelComParam[0], FALSE, FALSE, 0);
        gtk_widget_show( LabelComParam[0] );
        return vbox;
    }

//	vbox = gtk_vbox_new (FALSE/*homogeneous*/, 0);
	hbox = gtk_hbox_new (FALSE/*homogeneous*/, 0);
//	gtk_widget_show (vbox);
	table = gtk_table_new( MODBUS_COM_PARAMS_SIZE_X_TABLE, NBR_COM_PARAMS, FALSE );
	gtk_box_pack_start (GTK_BOX(hbox), table, TRUE/*expand*/, FALSE/*fill*/, 0);
	for( NumLine=0; NumLine<NBR_COM_PARAMS; NumLine++ )
	{
//		hbox[NumLine] = gtk_hbox_new (FALSE, 0);
//		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumLine]);
//		gtk_widget_show (hbox[NumLine]);
		switch( NumLine )
		{
			case 0:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Serial port (blank = IP mode)") );
				rtapi_strxcpy( BuffValue, ModbusConfig.ModbusSerialPortNameUsed );
				break;
			case 1:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Serial baud rate") );
				snprintf( BuffValue, sizeof(BuffValue), "%d", ModbusConfig.ModbusSerialSpeed );
				break;
			case 2:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Serial data bits") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,5,6,7,8", ModbusConfig.ModbusSerialDataBits-5 );
				break;
			case 3:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Serial parity") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,None,Odd,Even", ModbusConfig.ModbusSerialParity );
				break;
			case 4:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Serial stop bits") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,1,2", ModbusConfig.ModbusSerialStopBits-1 );
				break;
			case 5:
				snprintf( BuffLabel, sizeof(BuffLabel), _("After transmit pause - milliseconds") );
				snprintf( BuffValue, sizeof(BuffValue), "%d", ModbusConfig.ModbusTimeAfterTransmit );
				break;
			case 6:
				snprintf( BuffLabel, sizeof(BuffLabel), _("After receive pause - milliseconds") );
				snprintf( BuffValue, sizeof(BuffValue), "%d", ModbusConfig.ModbusTimeInterFrame );
				break;
			case 7:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Request Timeout length - milliseconds") );
				snprintf( BuffValue, sizeof(BuffValue), "%d", ModbusConfig.ModbusTimeOutReceipt );
				break;
			case 8:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Use RTS to send") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,NO,YES", ModbusConfig.ModbusSerialUseRtsToSend );
				break;
			case 9:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Modbus element offset") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,0,1", ModbusConfig.ModbusEleOffset );
				break;
			case 10:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Debug level") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,%s,%s 1,%s 2,%s", ModbusConfig.ModbusDebugLevel, _("QUIET"), _("LEVEL"), _("LEVEL"), _("VERBOSE") );
				break;
			case 11:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Read inputs map to") );
				snprintf( BuffValue, sizeof(BuffValue),  "%d,\%%B,\%%Q", ModbusConfig.MapTypeForReadInputs==VAR_MEM_BIT?0:(ModbusConfig.MapTypeForReadInputs==VAR_PHYS_OUTPUT?1:2) );
				break;
			case 12:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Read Coils/inputs map to") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,\%%B,\%%Q", ModbusConfig.MapTypeForReadCoils==VAR_MEM_BIT?0:(ModbusConfig.MapTypeForReadCoils==VAR_PHYS_OUTPUT?1:2) );
				break;
			case 13:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Write Coils map from") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,\%%B,\%%Q,\%%I", ModbusConfig.MapTypeForWriteCoils==VAR_MEM_BIT?0:(ModbusConfig.MapTypeForWriteCoils==VAR_PHYS_OUTPUT?1:2) );
				break;
			case 14:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Read input registers map to") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,\%%W,\%%QW", ModbusConfig.MapTypeForReadInputRegs==VAR_MEM_WORD?0:(ModbusConfig.MapTypeForReadInputRegs==VAR_PHYS_WORD_OUTPUT?1:2) );
				break;
			case 15:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Read register/holding map to") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,\%%W,\%%QW", ModbusConfig.MapTypeForReadHoldRegs==VAR_MEM_WORD?0:(ModbusConfig.MapTypeForReadHoldRegs==VAR_PHYS_WORD_OUTPUT?1:2) );
				break;
			case 16:
				snprintf( BuffLabel, sizeof(BuffLabel), _("Write hold registers map from") );
				snprintf( BuffValue, sizeof(BuffValue), "%d,\%%W,\%%QW,\%%IW", ModbusConfig.MapTypeForWriteHoldRegs==VAR_MEM_WORD?0:(ModbusConfig.MapTypeForWriteHoldRegs==VAR_PHYS_WORD_OUTPUT?1:2) );
				break;
		}

		/* Labels */
		LabelComParam[NumLine] = gtk_label_new(BuffLabel);
//GTK3		gtk_widget_set_usize( LabelComParam[NumLine],320,0 );
//		gtk_box_pack_start( GTK_BOX(hbox[NumLine]), LabelComParam[NumLine], FALSE, FALSE, 0 );
		gtk_table_attach_defaults (GTK_TABLE (table), LabelComParam[ NumLine ], 0, 1, NumLine, NumLine+1);
//		gtk_widget_show( LabelComParam[NumLine] );

		if ( NumLine<=1 || ( NumLine>=5 && NumLine<=7 ) )
		{
			/* Simple Integer Values */
			EntryComParam[NumLine] = gtk_entry_new();
//GTK3			gtk_widget_set_usize( EntryComParam[NumLine],125,0 );
//			gtk_box_pack_start( GTK_BOX(hbox[NumLine]), EntryComParam[NumLine], FALSE, FALSE, 0 );
			gtk_table_attach_defaults (GTK_TABLE (table), EntryComParam[ NumLine ], 1, MODBUS_COM_PARAMS_SIZE_X_TABLE, NumLine, NumLine+1);
//			gtk_widget_show( EntryComParam[NumLine] );
			gtk_entry_set_text( GTK_ENTRY(EntryComParam[NumLine]), BuffValue );
		}
		else
		{
			/* Radio buttons Values */
			/* BuffValue: first=n° selected , others=labels for each radio button */
			char * PtrArraysCsv[10];
			int CreateRadioBut;
			for( CreateRadioBut=0; CreateRadioBut<NBR_RADIO_BUT_COM_PARAMS; CreateRadioBut++ )
				RadioButComParams[ NumLine ][ CreateRadioBut ] = NULL;
			int NbrInfos = SplitCommasFieldsInPointersArray( BuffValue, PtrArraysCsv, 10 );
			if ( NbrInfos>2 )
			{
				int ValueSelected = atoi( PtrArraysCsv[0] );
				for( CreateRadioBut=0; CreateRadioBut<NbrInfos-1; CreateRadioBut++ )
				{
					char * label = PtrArraysCsv[1+CreateRadioBut];
					if ( CreateRadioBut==0 )
						RadioButComParams[ NumLine ][ CreateRadioBut ]= gtk_radio_button_new_with_label( NULL, label );
					else
						RadioButComParams[ NumLine ][ CreateRadioBut ]= gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(RadioButComParams[NumLine][0]), label );
//					gtk_box_pack_start (GTK_BOX (hbox[NumLine]), RadioButComParams[ NumLine ][ CreateRadioBut ], FALSE, TRUE, 0);
					gtk_table_attach_defaults (GTK_TABLE (table), RadioButComParams[ NumLine ][ CreateRadioBut ], 1+CreateRadioBut, 2+CreateRadioBut, NumLine, NumLine+1);
//					gtk_widget_show( RadioButComParams[ NumLine ][ CreateRadioBut ] );
					if ( CreateRadioBut==ValueSelected )
						gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RadioButComParams[ NumLine ][ CreateRadioBut ] ), TRUE);
				}
			}
		}
	}
	gtk_widget_show_all(hbox);
//	return vbox;
	return hbox;
}
int GetRadioButValueSelected( int NumLineToSee )
{
	int Sel = 0;
	int ScanRadioBut;
	for( ScanRadioBut=0; ScanRadioBut<NBR_RADIO_BUT_COM_PARAMS; ScanRadioBut++ )
	{
		if ( RadioButComParams[ NumLineToSee ][ ScanRadioBut ]!=NULL )
		{
			if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RadioButComParams[ NumLineToSee ][ ScanRadioBut ] ) ) )
				Sel = ScanRadioBut;
		}
	}
	return Sel;	
}
void GetModbusComParameters( void )
{
	rtapi_strxcpy( ModbusConfig.ModbusSerialPortNameUsed, gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 0 ] )));
	ModbusConfig.ModbusSerialSpeed = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 1 ] )) );
	ModbusConfig.ModbusSerialDataBits = GetRadioButValueSelected( 2 )+5;
	ModbusConfig.ModbusSerialParity = GetRadioButValueSelected( 3 ); 
	ModbusConfig.ModbusSerialStopBits = GetRadioButValueSelected( 4 )+1;
	ModbusConfig.ModbusTimeAfterTransmit = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 5 ] )) );
	ModbusConfig.ModbusTimeInterFrame = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 6 ] )) );
	ModbusConfig.ModbusTimeOutReceipt = atoi( gtk_entry_get_text(GTK_ENTRY( EntryComParam[ 7 ] )) );
	ModbusConfig.ModbusSerialUseRtsToSend = GetRadioButValueSelected( 8 );
	ModbusConfig.ModbusEleOffset = GetRadioButValueSelected( 9 );
	ModbusConfig.ModbusDebugLevel = GetRadioButValueSelected( 10 );
	// ! after here, 2 tests per line... ( if a parameter is added before ! ;-) )
	ModbusConfig.MapTypeForReadInputs = GetRadioButValueSelected( 11 )==0?VAR_MEM_BIT:(GetRadioButValueSelected( 11 )==1?VAR_PHYS_OUTPUT:VAR_PHYS_INPUT);
	ModbusConfig.MapTypeForReadCoils = GetRadioButValueSelected( 12 )==0?VAR_MEM_BIT:(GetRadioButValueSelected( 12 )==1?VAR_PHYS_OUTPUT:VAR_PHYS_INPUT);
	ModbusConfig.MapTypeForWriteCoils = GetRadioButValueSelected( 13 )==0?VAR_MEM_BIT:(GetRadioButValueSelected( 13 )==1?VAR_PHYS_OUTPUT:VAR_PHYS_INPUT);
	ModbusConfig.MapTypeForReadInputRegs = GetRadioButValueSelected( 14 )==0?VAR_MEM_WORD:(GetRadioButValueSelected( 14 )==1?VAR_PHYS_WORD_OUTPUT:VAR_PHYS_WORD_INPUT);
	ModbusConfig.MapTypeForReadHoldRegs = GetRadioButValueSelected( 15 )==0?VAR_MEM_WORD:(GetRadioButValueSelected( 15 )==1?VAR_PHYS_WORD_OUTPUT:VAR_PHYS_WORD_INPUT);
	ModbusConfig.MapTypeForWriteHoldRegs = GetRadioButValueSelected( 16 )==0?VAR_MEM_WORD:(GetRadioButValueSelected( 16 )==1?VAR_PHYS_WORD_OUTPUT:VAR_PHYS_WORD_INPUT);
}
#endif


//XXX log functionality not implemented.
/*
GtkWidget * CreateConfigEventsPage( void )
{
	static char * Labels[] = { "1st %MBxxxx", "Nbr Of %MB", "Symbol", "Text event", "Tag(>0=Def)" };
	GtkWidget *vbox;
	GtkWidget *hbox[ NBR_CONFIG_EVENTS_LOG+2 ];
	int NumObj;
	int NumLine;
	StrConfigEventLog * pCfgEvtLog;
	char BuffValue[ 40 ];

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);

	for (NumLine=-1; NumLine<NBR_CONFIG_EVENTS_LOG; NumLine++ )
	{
		hbox[NumLine+1] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumLine+1]);
		gtk_widget_show (hbox[NumLine+1]);

		for (NumObj=0; NumObj<NBR_CONFIG_EVENTS_PARAMS; NumObj++)
		{
			switch( NumLine )
			{
				case -1:
				{
					int PixelsLength = 100;
					GtkWidget * ParamTitleLabel;
					switch( NumObj )
					{
						case 3:
							PixelsLength = 220;
							break;
					}
					ParamTitleLabel = gtk_label_new( Labels[ NumObj ] );
					gtk_widget_set_usize(ParamTitleLabel,PixelsLength,0);
					gtk_box_pack_start(GTK_BOX (hbox[ NumLine+1 ]), ParamTitleLabel, FALSE, FALSE, 0);
					gtk_widget_show( ParamTitleLabel );
					break;
				}
				default:
				{
					pCfgEvtLog = &ConfigEventLog[ NumLine ];
					int PixelsLength = 100;
					int MaxChars = 0;
					switch( NumObj )
					{
						case 0:
							if ( pCfgEvtLog->FirstVarNum!=-1 )
								sprintf( BuffValue, "%d", pCfgEvtLog->FirstVarNum );
							else
								BuffValue[ 0 ] = '\0';
							break;
						case 1:
							sprintf( BuffValue, "%d", pCfgEvtLog->NbrVars );
							break;
						case 2:
							strcpy( BuffValue, pCfgEvtLog->Symbol );
							MaxChars = EVENT_SYMBOL_LGT-1;
							break;
						case 3:
							strcpy( BuffValue, pCfgEvtLog->Text );
							PixelsLength = 220;
							MaxChars = EVENT_TEXT_LGT-1;
							break;
						case 4:
							sprintf( BuffValue, "%d", pCfgEvtLog->EventType );
							break;
					}
					{
						GtkWidget **ParamEntry = &EventConfigParamEntry[ NumLine ][ NumObj ];
						*ParamEntry = gtk_entry_new( );
						gtk_widget_set_usize( *ParamEntry,PixelsLength,0 );
						if ( MaxChars>0 )
							gtk_entry_set_max_length( GTK_ENTRY(*ParamEntry), MaxChars );
						gtk_box_pack_start( GTK_BOX (hbox[NumLine+1]), *ParamEntry, FALSE, FALSE, 0 );
						gtk_widget_show ( *ParamEntry );
						gtk_entry_set_text( GTK_ENTRY(*ParamEntry), BuffValue );
					}
				}//default:
			}
		}
	}
	return vbox;
}
void GetConfigEventsSettings( void )
{
	int NumObj;
	int NumLine;
	StrConfigEventLog * pCfgEvtLog;
	GtkWidget *ParamEntry;
	char * text;
	for (NumLine=0; NumLine<NBR_CONFIG_EVENTS_LOG; NumLine++ )
	{
		int FirstVarEntered = -1;
		int NbrVarsEntered = 0;
		pCfgEvtLog = &ConfigEventLog[ NumLine ];

		for (NumObj=0; NumObj<NBR_CONFIG_EVENTS_PARAMS; NumObj++)
		{
			ParamEntry = EventConfigParamEntry[ NumLine ][ NumObj ];
			switch( NumObj )
			{
				case 0:
					text = (char *)gtk_entry_get_text(GTK_ENTRY(ParamEntry));
					if ( text[0]!='\0' )
						FirstVarEntered = atoi( text );
					break;
				case 1:
					text = (char *)gtk_entry_get_text(GTK_ENTRY(ParamEntry));
					NbrVarsEntered = atoi( text );
					break;
				case 2:
					text = (char *)gtk_entry_get_text(GTK_ENTRY(ParamEntry));
					strcpy( pCfgEvtLog->Symbol, text );
					break;
				case 3:
					text = (char *)gtk_entry_get_text(GTK_ENTRY(ParamEntry));
					strcpy( pCfgEvtLog->Text, text );
					break;
				case 4:
					text = (char *)gtk_entry_get_text(GTK_ENTRY(ParamEntry));
					pCfgEvtLog->EventType = atoi( text );
					break;
			}
		}//for (NumObj=0;
		pCfgEvtLog->FirstVarNum = -1;
		if ( FirstVarEntered+NbrVarsEntered>GetSizeVarsForTypeVar( VAR_MEM_BIT ) )
		{
			ShowMessageBoxError( _("Overflow error for first/nbrs detected...") );
		}
		else
		{
			pCfgEvtLog->NbrVars = NbrVarsEntered;
			pCfgEvtLog->FirstVarNum = FirstVarEntered;
		}
	}//for (NumLine=0; 
	// update the tags list of the variables that the user want to log !
    //XXX LinuxCNC log functionality is not implemented.
	//InitVarsArrayLogTags( );
}
*/


void GetSettings( void )
{
	GetGeneralParameters( );
	//GetIOSettings( 1/*ForInputs*/ );
	//GetIOSettings( 0/*ForInputs*/ );
#ifdef MODBUS_IO_MASTER
if(modmaster) {
    GetModbusModulesIOSettings( );
    GetModbusComParameters( );
}
#endif
	//GetConfigEventsSettings( );
#ifndef RT_SUPPORT
//	ConfigHardware( );
	InfosGene->AskToConfHard = TRUE;
#endif
ConfigSerialModbusMaster( );
}

void OpenConfigWindowGtk()
{
	GtkWidget *nbook;

	ConfigWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title( GTK_WINDOW(ConfigWindow), _("Config") );
	gtk_window_set_modal( GTK_WINDOW(ConfigWindow), TRUE );

	nbook = gtk_notebook_new( );
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateGeneralParametersPage( ),
				 gtk_label_new (_("Period/object info")) );
	//gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateConfigEventsPage( ),
				 //gtk_label_new (_("Events Config")) );
	//gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateIOConfPage( 1/*ForInputs*/ ),
				 //gtk_label_new (_("Physical Inputs")) );
	//gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateIOConfPage( 0/*ForInputs*/ ),
				 //gtk_label_new (_("Physical Outputs")) );
#ifdef MODBUS_IO_MASTER
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateModbusComParametersPage( ),
				 gtk_label_new (_("Modbus communication setup")) );
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateModbusModulesIO( ),
				 gtk_label_new (_("Modbus I/O register setup")) );
#endif

	gtk_container_add( GTK_CONTAINER (ConfigWindow), nbook );
	gtk_widget_show( nbook );

	gtk_window_set_position( GTK_WINDOW(ConfigWindow), GTK_WIN_POS_CENTER );
//	gtk_window_set_policy( GTK_WINDOW(ConfigWindow), FALSE, FALSE, TRUE );
	gtk_signal_connect ( GTK_OBJECT(ConfigWindow), "destroy",
                        GTK_SIGNAL_FUNC(GetSettings), NULL );

	gtk_widget_show( ConfigWindow );
}


