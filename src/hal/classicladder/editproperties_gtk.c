/* Classic Ladder Project */
/* Copyright (C) 2001 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* May 2001 */
/* Last update : 27 December 2001 */
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
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "classicladder.h"
#include "global.h"
#include "editproperties_gtk.h"
#include "edit.h"

GtkWidget *PropertiesWindow;
GtkWidget *PropLabelParam[NBR_PARAMS_PER_OBJ],
    *PropEntryParam[NBR_PARAMS_PER_OBJ];
GtkWidget *PropEntryBaseParam[NBR_PARAMS_PER_OBJ];
GtkWidget *ButtonApplyProperties;

void SetProperty(int NumParam, char *LblParam, char *ValParam)
{
    gtk_label_set_text((GtkLabel *) PropLabelParam[NumParam], LblParam);
    if (strcmp(LblParam, "Base") == 0) {
	gtk_widget_show(PropEntryBaseParam[NumParam]);
	gtk_widget_hide(PropEntryParam[NumParam]);
	gtk_entry_set_text((GtkEntry *) ((GtkCombo *)
		PropEntryBaseParam[NumParam])->entry, ValParam);
	if (NumParam == 0) {
	    gtk_widget_grab_focus(PropEntryBaseParam[0]);
	}
    } else {
	gtk_widget_hide(PropEntryBaseParam[NumParam]);
	gtk_widget_show(PropEntryParam[NumParam]);
	gtk_entry_set_text(GTK_ENTRY(PropEntryParam[NumParam]), ValParam);
	if (NumParam == 0) {
	    gtk_widget_grab_focus(PropEntryParam[0]);
	}
    }
    /* if no first param, means no params at all */
    /* so no sense to need to validate ! */
    if (NumParam == 0) {
	if (strcmp(LblParam, "---") == 0) {
	    gtk_widget_hide(ButtonApplyProperties);
	} else {
	    gtk_widget_show(ButtonApplyProperties);
//FIXME: no gtk_window_present() function available with GTK1.2...?
//not beautiful but it works...
	    gtk_widget_hide(PropertiesWindow);
	    gtk_widget_show(PropertiesWindow);
	}
    }
}

char *GetProperty(int NumParam)
{
    static char ValTxtParameter[40];
    gchar *TxtParameter;
    /* Convert to a number if it is a base */
    gtk_label_get(GTK_LABEL(PropLabelParam[NumParam]), &TxtParameter);
    if (strcmp(TxtParameter, "Base") == 0) {
	strcpy(ValTxtParameter,
	    (char *) gtk_entry_get_text((GtkEntry *) ((GtkCombo *)
		    PropEntryBaseParam[NumParam])->entry));
    } else {
	strcpy(ValTxtParameter, (char *) gtk_entry_get_text((GtkEntry *)
		PropEntryParam[NumParam]));
    }
    return ValTxtParameter;
}

gint PropertiesWindowDeleteEvent(GtkWidget * widget, GdkEvent * event,
    gpointer data)
{
    // we do not want that the window be destroyed.
    return TRUE;
}

void ShowPropertiesWindow(int Visible)
{
    if (Visible)
	gtk_widget_show(PropertiesWindow);
    else
	gtk_widget_hide(PropertiesWindow);
}

void PropertiesInitGtk()
{
    GtkWidget *vbox;
    GtkWidget *hbox[NBR_PARAMS_PER_OBJ + 1];
    int NumParam;
    GList *BaseItems = NULL;
    int ScanBase = 0;

    do {
	BaseItems =
	    g_list_append(BaseItems,
	    CorresDatasForBase[ScanBase++].ParamSelect);
    }
    while (ScanBase < NBR_TIMEBASES);

    PropertiesWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title((GtkWindow *) PropertiesWindow, "Properties");

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(PropertiesWindow), vbox);
    gtk_widget_show(vbox);

    for (NumParam = 0; NumParam < NBR_PARAMS_PER_OBJ; NumParam++) {
	hbox[NumParam] = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox[NumParam]);
	gtk_widget_show(hbox[NumParam]);

	PropLabelParam[NumParam] = gtk_label_new("Parameter");
	gtk_widget_set_usize((GtkWidget *) PropLabelParam[NumParam], 85, 0);
	gtk_box_pack_start(GTK_BOX(hbox[NumParam]), PropLabelParam[NumParam],
	    FALSE, FALSE, 0);
	gtk_widget_show(PropLabelParam[NumParam]);

	/* For numbers */
	PropEntryParam[NumParam] = gtk_entry_new();
	gtk_widget_set_usize((GtkWidget *) PropEntryParam[NumParam], 85, 0);
	gtk_box_pack_start(GTK_BOX(hbox[NumParam]), PropEntryParam[NumParam],
	    FALSE, FALSE, 0);
	gtk_widget_show(PropEntryParam[NumParam]);
	gtk_signal_connect(GTK_OBJECT(PropEntryParam[NumParam]), "activate",
	    (GtkSignalFunc) SaveElementProperties, NULL);

	/* For time base */
	PropEntryBaseParam[NumParam] = gtk_combo_new();
	gtk_combo_set_value_in_list(GTK_COMBO(PropEntryBaseParam[NumParam]), TRUE	/* val 
	     */ , FALSE /* ok_if_empty */ );
	gtk_combo_set_popdown_strings(GTK_COMBO(PropEntryBaseParam[NumParam]),
	    BaseItems);
	gtk_widget_set_usize((GtkWidget *) PropEntryBaseParam[NumParam], 85,
	    0);
	gtk_box_pack_start(GTK_BOX(hbox[NumParam]),
	    PropEntryBaseParam[NumParam], FALSE, FALSE, 0);
    }

    /* for validate button... */
    hbox[NumParam] = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(vbox), hbox[NumParam]);
    gtk_widget_show(hbox[NumParam]);

    ButtonApplyProperties = gtk_button_new_with_label("Apply");
    gtk_box_pack_start(GTK_BOX(hbox[NumParam]), ButtonApplyProperties,
	TRUE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(ButtonApplyProperties), "clicked",
	(GtkSignalFunc) SaveElementProperties, 0);

//    gtk_widget_show (PropertiesWindow);

    gtk_signal_connect(GTK_OBJECT(PropertiesWindow), "delete_event",
	(GtkSignalFunc) PropertiesWindowDeleteEvent, 0);
}
