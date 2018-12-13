/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* May 2001 */
/* --------------------------------- */
/* Editor properties - GTK interface */
/* --------------------------------- */
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

#include <locale.h>
#include <libintl.h>
#define _(x) gettext(x)
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "classicladder.h"
#include "global.h"
#include "editproperties_gtk.h"
#include "edit.h"

GtkWidget *PropertiesWindow;
GtkWidget *PropLabelParam[NBR_PARAMS_PER_OBJ],*PropEntryParam[NBR_PARAMS_PER_OBJ];
GtkWidget *PropEntryBaseParam[NBR_PARAMS_PER_OBJ];
GtkWidget *PropEntryTimerModeParam[NBR_PARAMS_PER_OBJ];
GtkWidget *ButtonApplyProperties;
int SavePosX = -1;
int SavePosY = -1;

void SetProperty(int NumParam,char * LblParam,char * ValParam)
{
	gtk_label_set_text((GtkLabel *)PropLabelParam[NumParam],LblParam);
	if (strcmp(LblParam,_("Base"))==0)
	{
		gtk_widget_hide(PropEntryParam[NumParam]);
		gtk_widget_show(PropEntryBaseParam[NumParam]);
		gtk_widget_hide(PropEntryTimerModeParam[NumParam]);
		gtk_entry_set_text((GtkEntry*)((GtkCombo *)PropEntryBaseParam[NumParam])->entry,ValParam);
		if (NumParam==0)
			gtk_widget_grab_focus( PropEntryBaseParam[0] );
	}
	else
	{
		if (strcmp(LblParam,_("TimerMode"))==0)
		{
			gtk_widget_hide(PropEntryParam[NumParam]);
			gtk_widget_hide(PropEntryBaseParam[NumParam]);
			gtk_widget_show(PropEntryTimerModeParam[NumParam]);
			gtk_entry_set_text((GtkEntry*)((GtkCombo *)PropEntryTimerModeParam[NumParam])->entry,ValParam);
			if (NumParam==0)
				gtk_widget_grab_focus( PropEntryTimerModeParam[0] );
		}
		else
		{
			// others standard parameters
			gtk_widget_show(PropEntryParam[NumParam]);
			gtk_widget_hide(PropEntryBaseParam[NumParam]);
			gtk_widget_hide(PropEntryTimerModeParam[NumParam]);
			gtk_entry_set_text(GTK_ENTRY(PropEntryParam[NumParam]),ValParam);
			if (NumParam==0)
				gtk_widget_grab_focus( PropEntryParam[0] );
		}
	}
	/* if no first param, means no params at all */
	/* so no sense to need to apply ! */
	if (NumParam==0)
	{
		if (strcmp(LblParam,"---")==0)
		{
			gtk_widget_hide(ButtonApplyProperties);
		}
		else
		{
			gtk_widget_show(ButtonApplyProperties);
#ifndef GTK2
			// no gtk_window_present() function available with GTK1.2
			//not beautiful but it works...
			gtk_widget_hide(PropertiesWindow);
			gtk_widget_show(PropertiesWindow);
#else
			gtk_window_present( GTK_WINDOW(PropertiesWindow) );
#endif
		}
	}
}

char * GetProperty(int NumParam)
{
	static char ValTxtParameter[61];
	gchar * TxtParameter;
	/* Convert to a number if it is a base */
	gtk_label_get(GTK_LABEL(PropLabelParam[NumParam]),&TxtParameter);
	if (strcmp(TxtParameter,_("Base"))==0)
	{
		strcpy( ValTxtParameter , (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)PropEntryBaseParam[NumParam])->entry) );
	}
	else
	{
		if (strcmp(TxtParameter,_("TimerMode"))==0)
		{
			strcpy( ValTxtParameter , (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)PropEntryTimerModeParam[NumParam])->entry) );
		}
		else
		{
			// others standard parameters
			strncpy( ValTxtParameter , (char *)gtk_entry_get_text((GtkEntry *)PropEntryParam[NumParam]), 60 );
			ValTxtParameter[ 60 ] = '\0';
		}
	}
	return ValTxtParameter;
}

gint PropertiesWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
	// we do not want that the window be destroyed.
	return TRUE;
}

void ShowPropertiesWindow( int Visible )
{
	if ( Visible )
	{
		gtk_widget_show(PropertiesWindow);
		if ( SavePosX!=-1 && SavePosY!=-1 )
			gtk_window_move( GTK_WINDOW(PropertiesWindow), SavePosX, SavePosY );
	}
	else
	{
		gtk_window_get_position( GTK_WINDOW(PropertiesWindow), &SavePosX, &SavePosY );
		gtk_widget_hide(PropertiesWindow);
	}
}

void PropertiesInitGtk()
{
	GtkWidget *vbox;
	GtkWidget *hbox[NBR_PARAMS_PER_OBJ + 1];
	int NumParam;
	GList *BaseItems = NULL;
	GList *TimersModesItems = NULL;
	int ScanBase = 0;
	int ScanTimerMode = 0;
	do
	{
		BaseItems = g_list_append(BaseItems,CorresDatasForBase[ScanBase++].ParamSelect);
	}
	while(ScanBase<NBR_TIMEBASES);
	do
	{
		TimersModesItems = g_list_append(TimersModesItems,TimersModesStrings[ScanTimerMode++]);
	}
	while(ScanTimerMode<NBR_TIMERSMODES);

	PropertiesWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *)PropertiesWindow, _("Properties"));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (PropertiesWindow), vbox);
	gtk_widget_show (vbox);

	for (NumParam = 0 ; NumParam<NBR_PARAMS_PER_OBJ ; NumParam++)
	{
		hbox[NumParam] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[NumParam]);
		gtk_widget_show (hbox[NumParam]);

		PropLabelParam[NumParam] = gtk_label_new(_("Parameter"));
		gtk_widget_set_usize((GtkWidget *)PropLabelParam[NumParam],85,0);
		gtk_box_pack_start (GTK_BOX (hbox[NumParam]), PropLabelParam[NumParam], FALSE, FALSE, 0);
		gtk_widget_show (PropLabelParam[NumParam]);

		/* For numbers */
		PropEntryParam[NumParam] = gtk_entry_new();
//        gtk_widget_set_usize((GtkWidget *)PropEntryParam[NumParam],85,0);
		gtk_box_pack_start (GTK_BOX (hbox[NumParam]), PropEntryParam[NumParam], TRUE, TRUE, 0);
		gtk_widget_show (PropEntryParam[NumParam]);
		gtk_signal_connect(GTK_OBJECT (PropEntryParam[NumParam]), "activate",
							(GtkSignalFunc) SaveElementProperties, NULL);

		/* For time base */
		PropEntryBaseParam[NumParam] = gtk_combo_new();
		gtk_combo_set_value_in_list(GTK_COMBO(PropEntryBaseParam[NumParam]), TRUE /*val*/, FALSE /*ok_if_empty*/);
		gtk_combo_set_popdown_strings(GTK_COMBO(PropEntryBaseParam[NumParam]),BaseItems);
//        gtk_widget_set_usize((GtkWidget *)PropEntryBaseParam[NumParam],85,0);
		gtk_box_pack_start (GTK_BOX (hbox[NumParam]), PropEntryBaseParam[NumParam], FALSE, FALSE, 0);

		/* For timer mode */
		PropEntryTimerModeParam[NumParam] = gtk_combo_new();
		gtk_combo_set_value_in_list(GTK_COMBO(PropEntryTimerModeParam[NumParam]), TRUE /*val*/, FALSE /*ok_if_empty*/);
		gtk_combo_set_popdown_strings(GTK_COMBO(PropEntryTimerModeParam[NumParam]),TimersModesItems);
//        gtk_widget_set_usize((GtkWidget *)PropEntryTimerModeParam[NumParam],85,0);
		gtk_box_pack_start (GTK_BOX (hbox[NumParam]), PropEntryTimerModeParam[NumParam], FALSE, FALSE, 0);
	}

	/* for apply button... */
	hbox[NumParam] = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox[NumParam]);
	gtk_widget_show (hbox[NumParam]);

	ButtonApplyProperties = gtk_button_new_with_label(_("Apply"));
	gtk_box_pack_start (GTK_BOX (hbox[NumParam]), ButtonApplyProperties, TRUE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT (ButtonApplyProperties), "clicked",
						(GtkSignalFunc) SaveElementProperties, 0);

//    gtk_widget_show (PropertiesWindow);

	gtk_signal_connect( GTK_OBJECT(PropertiesWindow), "delete_event",
		(GtkSignalFunc)PropertiesWindowDeleteEvent, 0 );
}

