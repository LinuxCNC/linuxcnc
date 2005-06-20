/* Classic Ladder Project */
/* Copyright (C) 2001-2003 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "classicladder.h"
#include "global.h"
#include "manager.h"
#include "edit.h"
#include "config_gtk.h"


#define NBR_OBJECTS 9
GtkWidget *LabelParam[ NBR_OBJECTS ],*ValueParam[ NBR_OBJECTS ];
GtkWidget *ConfigWindow;


GtkWidget * CreateSizesPage( void )
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
				InfoUsed = GetNbrRungsDefined( )*100/InfosGene->SizesInfos.nbr_rungs;
				sprintf( BuffLabel, "Nbr.rungs (%d%c used)", InfoUsed,'%' );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_rungs );
				break;
			case 1:
				sprintf( BuffLabel, "Nbr.Bits" );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_bits );
				break;
			case 2:
				sprintf( BuffLabel, "Nbr.Words" );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_words );
				break;
			case 3:
				sprintf( BuffLabel, "Nbr.Timers" );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_timers );
				break;
			case 4:
				sprintf( BuffLabel, "Nbr.Monostables" );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_monostables );
				break;
			case 5:
				sprintf( BuffLabel, "Nbr.Phys.Inputs" );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_phys_inputs );
				break;
			case 6:
				sprintf( BuffLabel, "Nbr.Phys.Oututs" );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_phys_outputs );
				break;
			case 7:
				sprintf( BuffLabel, "Nbr.Arithm.Expr." );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_arithm_expr );
				break;
			case 8:
				InfoUsed = NbrSectionsDefined( )*100/InfosGene->SizesInfos.nbr_sections;
				sprintf( BuffLabel, "Nbr.Sections (%d%c used)", InfoUsed,'%' );
				sprintf( BuffValue, "%d", InfosGene->SizesInfos.nbr_sections );
				break;
			default:
				sprintf( BuffLabel, "???" );
				sprintf( BuffValue, "???" );
				break;
		}

		LabelParam[NumObj] = gtk_label_new(BuffLabel);
		gtk_widget_set_usize((GtkWidget *)LabelParam[NumObj],150,0);
		gtk_box_pack_start (GTK_BOX (hbox[NumObj]), LabelParam[NumObj], FALSE, FALSE, 0);
		gtk_widget_show (LabelParam[NumObj]);

		/* For numbers */
		ValueParam[NumObj] = gtk_entry_new();
		gtk_widget_set_usize((GtkWidget *)ValueParam[NumObj],50,0);
		gtk_box_pack_start (GTK_BOX (hbox[NumObj]), ValueParam[NumObj], FALSE, FALSE, 0);
		gtk_widget_show (ValueParam[NumObj]);
		gtk_entry_set_text( GTK_ENTRY(ValueParam[NumObj]), BuffValue);
		gtk_editable_set_editable( GTK_EDITABLE(ValueParam[NumObj]), FALSE);
	}
	return vbox;
}

void OpenConfigWindowGtk()
{
	GtkWidget *nbook;

	ConfigWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title( GTK_WINDOW(ConfigWindow), "Config" );
	gtk_window_set_modal( GTK_WINDOW(ConfigWindow), TRUE );

	nbook = gtk_notebook_new( );
	gtk_notebook_append_page( GTK_NOTEBOOK(nbook), CreateSizesPage(),
				 gtk_label_new ("Sizes") );

	gtk_container_add( GTK_CONTAINER (ConfigWindow), nbook );
	gtk_widget_show( nbook );

	gtk_window_set_position( GTK_WINDOW(ConfigWindow), GTK_WIN_POS_CENTER );
	gtk_window_set_policy( GTK_WINDOW(ConfigWindow), FALSE, FALSE, TRUE );

	gtk_widget_show( ConfigWindow );
}


