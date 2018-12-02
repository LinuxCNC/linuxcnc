/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* August 2002 */
/* -------------------------- */
/* Sections manager (GTK part)*/
/* -------------------------- */
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
#include <stdlib.h>
#include "classicladder.h"
#include "global.h"
#include "manager.h"
#include "classicladder_gtk.h"
#include "manager_gtk.h"
#include "edit_gtk.h"

GtkWidget *ManagerWindow;
GtkWidget *SectionsList;
GtkWidget *ButtonAddSection;
GtkWidget *ButtonDelSection;
GtkWidget *ButtonMoveUpSection;
GtkWidget *ButtonMoveDownSection;

GtkWidget *AddSectionWindow;
GtkWidget * EditName;
GtkWidget * CycleLanguage;
GtkWidget * CycleSubRoutineNbr;

char * pNameSectionSelected;
int RowSectionSelected;

void ManagerDisplaySections( )
{
	StrSection * pSection;
	int NumSec;
	char * RowList[ ] = {"---", "---", "---", "---" };
	char BufferForSRx[ 10 ];
	int OneSectionExists = FALSE;
char buffer_debug[ 50 ];
	pNameSectionSelected = NULL;
	gtk_clist_clear( GTK_CLIST(SectionsList) );
	for ( NumSec=0; NumSec<NBR_SECTIONS; NumSec++ )
	{
		pSection = &SectionArray[ NumSec ];
		if ( pSection->Used )
		{
			RowList[ 0 ] = pSection->Name;
			if ( pSection->Language == SECTION_IN_LADDER )
			{
				RowList[ 1 ] = _("Ladder");
				RowList[ 2 ] = _("Main");
			}
			if ( pSection->Language == SECTION_IN_SEQUENTIAL )
			{
				RowList[ 1 ] = _("Sequential");
				RowList[ 2 ] = "---";
			}
			if ( pSection->SubRoutineNumber>=0 )
			{
				sprintf( BufferForSRx, "SR%d", pSection->SubRoutineNumber );
				RowList[ 2 ] = BufferForSRx;
			}
sprintf( buffer_debug, "F=%d, L=%d, P=%d", pSection->FirstRung, pSection->LastRung, pSection->SequentialPage );
RowList[ 3 ] = buffer_debug;
			gtk_clist_append( GTK_CLIST(SectionsList), RowList );
			OneSectionExists = TRUE;
		}
	}
	if ( OneSectionExists )
		gtk_clist_select_row( GTK_CLIST(SectionsList), 0, 0 );
}

void SelectRowSignal( GtkCList *clist, gint row, gint column, GdkEventButton *event, gpointer user_data)
{
	if ( gtk_clist_get_text( GTK_CLIST(SectionsList), row, 0, (gchar **)&pNameSectionSelected ) )
	{
		RowSectionSelected = row;
		SectionSelected( pNameSectionSelected );
		EditorButtonsAccordingSectionType( );
		UpdateVScrollBar( );
	}
}

void ButtonAddClickSignal( )
{
	// we open the requester to add a new section...
	gtk_entry_set_text( GTK_ENTRY(EditName), "" );
	gtk_widget_grab_focus( EditName );
	gtk_widget_show( AddSectionWindow );
}
void ButtonAddSectionDoneClickSignal( )
{
	char SubNbrValue[ 10 ];
	int SubNbr = -1;
	char BuffLanguage[ 30 ];
	int Language = SECTION_IN_LADDER;
	// get language type
	strcpy( BuffLanguage , (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)CycleLanguage)->entry) );
	if ( strcmp( BuffLanguage, _("Sequential") )==0 )
		Language = SECTION_IN_SEQUENTIAL;
	// get if main or sub-routine (and which number if sub, used in the 'C'all coils)
	strcpy( SubNbrValue , (char *)gtk_entry_get_text((GtkEntry *)((GtkCombo *)CycleSubRoutineNbr)->entry) );
	if ( SubNbrValue[ 0 ]=='S' && SubNbrValue[ 1 ]=='R' )
		SubNbr = atoi( &SubNbrValue[2] );

	// verify if name already exist...
	if (VerifyIfSectionNameAlreadyExist(   (char *)gtk_entry_get_text( GTK_ENTRY(EditName) )   ) )
	{
		ShowMessageBox( _("Error"), _("This section name already exist or is incorrect !!!"), _("Ok") );
	}
	else
	{
		if ( SubNbr>=0 && VerifyIfSubRoutineNumberExist( SubNbr ))
		{
			ShowMessageBox( _("Error"), _("This sub-routine number for calls is already defined !!!"), _("Ok") );
		}
		else
		{
			// create the new section
			if ( !AddSection( (char *)gtk_entry_get_text( GTK_ENTRY(EditName) ) , Language , SubNbr ) )
				ShowMessageBox( _("Error"), _("Failed to add a new section. Full?"), _("Ok") );
			gtk_widget_hide( AddSectionWindow );

			ManagerDisplaySections( );
		}
	}
}
gint AddSectionWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
	// we just want to hide the window
	gtk_widget_hide( AddSectionWindow );
	// we do not want that the window be destroyed.
	return TRUE;
}

void DeleteCurrentSection( )
{
	DelSection( pNameSectionSelected );
	ManagerDisplaySections( );
}

void ButtonDelClickSignal( )
{
	if (pNameSectionSelected )
	{
		if ( NbrSectionsDefined( )>1 )
		{
			ShowConfirmationBox(_("New"),_("Do you really want to delete the section ?"), DeleteCurrentSection);	
		}
		else
		{
			ShowMessageBox( _("Error"), _("You can not delete the last section..."), _("Ok") );
		}
	}
}

void ButtonMoveUpClickSignal( )
{
	char *pNameSectionToSwapWith;
	if ( RowSectionSelected>0 )
	{
		if ( gtk_clist_get_text( GTK_CLIST(SectionsList), RowSectionSelected-1, 0, (gchar **)&pNameSectionToSwapWith ) )
		{
			SwapSections( pNameSectionSelected, pNameSectionToSwapWith );
		}
	}
	else
	{
		ShowMessageBox( _("Error"), _("This section is already executed the first !"), _("Ok") );
	}
	ManagerDisplaySections( );
}
void ButtonMoveDownClickSignal( )
{
	char *pNameSectionToSwapWith;
//	if ( RowSectionSelected<   )
	{
		if ( gtk_clist_get_text( GTK_CLIST(SectionsList), RowSectionSelected+1, 0, (gchar **)&pNameSectionToSwapWith ) )
		{
			SwapSections( pNameSectionSelected, pNameSectionToSwapWith );
		}
	}
//	else
//	{
//		ShowMessageBox( "Error", "This section is already executed the last !", "Ok" );
//	}
	ManagerDisplaySections( );
}

gint ManagerWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
	// we do not want that the window be destroyed.
	return TRUE;
}

void AddSectionWindowInit( )
{
	GtkWidget *vbox;
	GtkWidget * ButtonOk;
	GtkWidget * hbox[ 3 ];
	GtkWidget * Lbl[ 3 ];
	GList *LangageItems = NULL;
	GList *SubRoutinesNbrItems = NULL;
	int NumSub;
	char * ArrayNumSub[ ] = { "SR0", "SR1", "SR2", "SR3", "SR4", "SR5", "SR6", "SR7", "SR8", "SR9" };
	int Line;
	LangageItems = g_list_append( LangageItems, _("Ladder") );
#ifdef SEQUENTIAL_SUPPORT
	LangageItems = g_list_append( LangageItems, _("Sequential") );
#endif

	SubRoutinesNbrItems = g_list_append( SubRoutinesNbrItems, _("Main") );
	for ( NumSub=0; NumSub<10; NumSub++ )
	{
		SubRoutinesNbrItems = g_list_append( SubRoutinesNbrItems, ArrayNumSub[ NumSub ] );
	}

	AddSectionWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *)AddSectionWindow, _("Add a section..."));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (AddSectionWindow), vbox);
	gtk_widget_show (vbox);

	for ( Line = 0; Line<3; Line++ )
	{
		char * text;
		hbox[ Line ] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox[ Line ]);
		gtk_widget_show (hbox[ Line ]);

		switch( Line )
		{
			case 1 : text = _("Language"); break;
			case 2 : text = _("Main/Sub-Routine"); break;
			default: text = _("Name"); break;
		}
		Lbl[ Line ] = gtk_label_new( text );
		gtk_box_pack_start (GTK_BOX (hbox[ Line ]), Lbl[ Line ], FALSE, FALSE, 0);
		gtk_widget_show ( Lbl[ Line ]);

		switch( Line )
		{
			case 0:
				EditName = gtk_entry_new();
				gtk_entry_set_max_length( GTK_ENTRY(EditName),LGT_SECTION_NAME-1 );
				gtk_box_pack_start (GTK_BOX (hbox[Line]), EditName, TRUE, TRUE, 0);
				gtk_widget_show( EditName );
				break;
			case 1:
				CycleLanguage = gtk_combo_new();
				gtk_combo_set_value_in_list(GTK_COMBO(CycleLanguage), TRUE /*val*/, FALSE /*ok_if_empty*/);
				gtk_combo_set_popdown_strings(GTK_COMBO(CycleLanguage),LangageItems);
				gtk_box_pack_start (GTK_BOX (hbox[Line]), CycleLanguage, TRUE, TRUE, 0);
				gtk_widget_show( CycleLanguage );
				break;
			case 2:
				CycleSubRoutineNbr = gtk_combo_new();
				gtk_combo_set_value_in_list(GTK_COMBO(CycleSubRoutineNbr), TRUE /*val*/, FALSE /*ok_if_empty*/);
				gtk_combo_set_popdown_strings(GTK_COMBO(CycleSubRoutineNbr),SubRoutinesNbrItems);
				gtk_box_pack_start (GTK_BOX (hbox[Line]), CycleSubRoutineNbr, TRUE, TRUE, 0);
				gtk_widget_show( CycleSubRoutineNbr );
				break;
		}
	}

	ButtonOk = gtk_button_new_with_label(_("Ok"));
	gtk_box_pack_start (GTK_BOX (vbox), ButtonOk, TRUE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT (ButtonOk), "clicked",
		(GtkSignalFunc)ButtonAddSectionDoneClickSignal, 0);
	gtk_widget_show (ButtonOk);
	gtk_window_set_modal(GTK_WINDOW(AddSectionWindow),TRUE);
	gtk_window_set_position(GTK_WINDOW(AddSectionWindow),GTK_WIN_POS_CENTER);
	gtk_signal_connect( GTK_OBJECT(AddSectionWindow), "delete_event",
		(GtkSignalFunc)AddSectionWindowDeleteEvent, 0 );
}

void ToggleManagerWindow()
{
	if (InfosGene->HideGuiState == GTK_WIDGET_VISIBLE( ManagerWindow ))
	{
		if ( GTK_WIDGET_VISIBLE( ManagerWindow ) )
		{	gtk_widget_hide (ManagerWindow);
		}else{	gtk_widget_show (ManagerWindow);
		}
	}
}
void ManagerInitGtk()
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	char * List[ ] = {_("Section Name   "), _("Language    "), _("Type   "), _("debug") };

	pNameSectionSelected = NULL;

	ManagerWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *)ManagerWindow,_( "Sections Manager"));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (ManagerWindow), vbox);
	gtk_widget_show (vbox);

	SectionsList = gtk_clist_new_with_titles( /*3*/ 4, List );
	gtk_box_pack_start (GTK_BOX(vbox), SectionsList, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT (SectionsList), "select-row",
		(GtkSignalFunc) SelectRowSignal, 0);
	gtk_widget_show( SectionsList );

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show (hbox);

	ButtonAddSection = gtk_button_new_with_label(_("Add section"));
	gtk_box_pack_start (GTK_BOX (hbox), ButtonAddSection, TRUE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT (ButtonAddSection), "clicked",
		(GtkSignalFunc) ButtonAddClickSignal, 0);
	gtk_widget_show (ButtonAddSection);
	ButtonDelSection = gtk_button_new_with_label(_("Delete section"));
	gtk_box_pack_start (GTK_BOX (hbox), ButtonDelSection, TRUE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT (ButtonDelSection), "clicked",
		(GtkSignalFunc) ButtonDelClickSignal, 0);
	gtk_widget_show (ButtonDelSection);
	ButtonMoveUpSection = gtk_button_new_with_label(_("Move Up"));
	gtk_box_pack_start (GTK_BOX (hbox), ButtonMoveUpSection, TRUE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT (ButtonMoveUpSection), "clicked",
		(GtkSignalFunc) ButtonMoveUpClickSignal, 0);
	gtk_widget_show (ButtonMoveUpSection);
	ButtonMoveDownSection = gtk_button_new_with_label(_("Move Down"));
	gtk_box_pack_start (GTK_BOX (hbox), ButtonMoveDownSection, TRUE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT (ButtonMoveDownSection), "clicked",
		(GtkSignalFunc) ButtonMoveDownClickSignal, 0);
	gtk_widget_show (ButtonMoveDownSection);

	ManagerDisplaySections( );
	gtk_signal_connect( GTK_OBJECT(ManagerWindow), "delete_event",
		(GtkSignalFunc)ManagerWindowDeleteEvent, 0 );
	gtk_widget_show (ManagerWindow);

	AddSectionWindowInit( );
}

