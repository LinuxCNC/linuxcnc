/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */
/* ---------------------------- */
/* GTK Interface & Main */
/* Inspired (at the start) from the scribble example. */
/* ---------------------------------- */
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
#include <gdk/gdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "classicladder.h"
#include "global.h"
#include "classicladder_gtk.h"
// #include "hardware.h"

#define NBR_BOOLS_VAR_SPY 15
#define NBR_TYPE_BOOLS_SPY 3
#define NBR_FREE_VAR_SPY 5

GdkPixmap *pixmap = NULL;
GtkWidget *drawing_area = NULL;
GtkWidget *offsetboolvar[ NBR_TYPE_BOOLS_SPY ];
int ValOffsetBoolVar[ NBR_TYPE_BOOLS_SPY ] = { 0, 0, 0 };
GtkWidget *chkvar[ NBR_TYPE_BOOLS_SPY ][ NBR_BOOLS_VAR_SPY ];
GtkWidget *EntryVarSpy[NBR_FREE_VAR_SPY*2];
int VarSpy[NBR_FREE_VAR_SPY][2] = { {VAR_MEM_WORD,0}, {VAR_MEM_WORD,1}, {VAR_MEM_WORD,2}, {VAR_MEM_WORD,3}, {VAR_MEM_WORD,4} }; /* defaults vars to spy */
GtkWidget *DisplayFormatVarSpy[NBR_FREE_VAR_SPY];
GtkWidget *entrylabel,*entrycomment;
GtkWidget *CheckDispSymbols;
#if defined( RT_SUPPORT ) || defined( __XENO__ )
GtkWidget *DurationOfLastScan;
#endif
GtkWidget *ButtonRunStop;
GtkWidget *VScrollBar;
GtkWidget *HScrollBar;
GtkAdjustment * AdjustVScrollBar;
GtkAdjustment * AdjustHScrollBar;
GtkWidget *FileSelector;
GtkWidget *ConfirmDialog;
GtkWidget *RungWindow;

#include "drawing.h"
#include "vars_access.h"
#include "calc.h"
#include "files.h"
#include "edit.h"
#include "edit_gtk.h"
#include "editproperties_gtk.h"
#include "manager_gtk.h"
#include "config_gtk.h"
// #include "socket_server.h"
// #include "socket_modbus_master.h"
#ifdef SEQUENTIAL_SUPPORT
#include "calc_sequential.h"
#endif
#ifdef GNOME_PRINT_USE
#include "print_gnome.h"
#endif
#include "symbols_gtk.h"

/* Create a new backing pixmap of the appropriate size */
static gint configure_event( GtkWidget         *widget,
                            GdkEventConfigure *event )
{
    if (pixmap)
        gdk_pixmap_unref(pixmap);

    pixmap = gdk_pixmap_new(widget->window,
                            widget->allocation.width,
                            widget->allocation.height,
                            -1);
    gdk_draw_rectangle (pixmap,
                        widget->style->white_gc,
                        TRUE,
                        0, 0,
                        widget->allocation.width,
                        widget->allocation.height);
    return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gint expose_event( GtkWidget      *widget,
                        GdkEventExpose *event )
{
    gdk_draw_pixmap(widget->window,
                    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                    pixmap,
                    event->area.x, event->area.y,
                    event->area.x, event->area.y,
                    event->area.width, event->area.height);

    return FALSE;
}

void UpdateVScrollBar()
{
	int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
	if ( iCurrentLanguage==SECTION_IN_LADDER )
	{
		int NbrRungs = 1;
		int ScanRung = InfosGene->FirstRung;
		int NumCurrentRung = 0;
		while ( ScanRung!=InfosGene->LastRung )
		{
			NbrRungs++;
			ScanRung = RungArray[ ScanRung ].NextRung;
		}
		ScanRung = InfosGene->FirstRung;
		while ( ScanRung!=InfosGene->CurrentRung )
		{
			NumCurrentRung++;
			ScanRung = RungArray[ ScanRung ].NextRung;
		}
//printf("Nbr rungs=%d , NumRung=%d\n", NbrRungs, NumCurrentRung);
		AdjustVScrollBar->lower = 0;
		AdjustVScrollBar->upper = NbrRungs * InfosGene->BlockHeight*RUNG_HEIGHT;
		AdjustVScrollBar->value = NumCurrentRung *  InfosGene->BlockHeight*RUNG_HEIGHT;
		while( AdjustVScrollBar->value+InfosGene->PageHeight > AdjustVScrollBar->upper )
		{
			AdjustVScrollBar->value = AdjustVScrollBar->value - InfosGene->BlockHeight*RUNG_HEIGHT;
		}
		AdjustVScrollBar->step_increment = InfosGene->BlockHeight;
		AdjustVScrollBar->page_increment = InfosGene->BlockHeight*RUNG_HEIGHT;
		AdjustVScrollBar->page_size = InfosGene->PageHeight;
		gtk_adjustment_changed( AdjustVScrollBar );
		gtk_adjustment_value_changed( AdjustVScrollBar );
		gtk_widget_hide( HScrollBar );
//        gtk_widget_show( entrylabel );
//        gtk_widget_show( entrycomment );
	}
#ifdef SEQUENTIAL_SUPPORT
	if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
	{
		gtk_widget_show( HScrollBar );
//        gtk_widget_hide( entrylabel );
//        gtk_widget_hide( entrycomment );
		refresh_label_comment( );
		AdjustVScrollBar->lower = 0;
		AdjustVScrollBar->upper = SEQ_PAGE_HEIGHT * SEQ_SIZE_DEF;
		AdjustVScrollBar->value = 0;
		AdjustVScrollBar->step_increment = SEQ_SIZE_DEF;
		AdjustVScrollBar->page_increment = InfosGene->PageHeight;
		AdjustVScrollBar->page_size = InfosGene->PageHeight;
		gtk_adjustment_changed( AdjustVScrollBar );
		gtk_adjustment_value_changed( AdjustVScrollBar );
		AdjustHScrollBar->lower = 0;
		AdjustHScrollBar->upper = SEQ_PAGE_WIDTH * SEQ_SIZE_DEF;
		AdjustHScrollBar->value = 0;
		AdjustHScrollBar->step_increment = SEQ_SIZE_DEF;
		AdjustHScrollBar->page_increment = InfosGene->PageWidth;
		AdjustHScrollBar->page_size = InfosGene->PageWidth;
		gtk_adjustment_changed( AdjustHScrollBar );
		gtk_adjustment_value_changed( AdjustHScrollBar );
	}
#endif
}

void ChoiceOfTheCurrentRung( int NbrOfRungsAfterTopRung )
{
	int DecptNbrRungs = NbrOfRungsAfterTopRung;

//printf("OffsetHiddenTopRungDisplayed=%d\n", InfosGene->OffsetHiddenTopRungDisplayed);
	// per default, the current rung, is the top one displayed...
	InfosGene->CurrentRung = InfosGene->TopRungDisplayed;
	// if OffsetHiddenTopRungDisplayed==0, vertical shift of the current rung is 0,
	// else this is the y value to substract to have the vertical shift... (we will add many full rungs heights after!)
	InfosGene->OffsetCurrentRungDisplayed = -1*InfosGene->OffsetHiddenTopRungDisplayed;

//printf("=> In choice of current rung, %d passes...(CurrentRung=%d)\n",NbrOfRungsAfterTopRung, InfosGene->CurrentRung);
	while( DecptNbrRungs>0 && InfosGene->CurrentRung!=InfosGene->LastRung )
	{
		InfosGene->CurrentRung = RungArray[ InfosGene->CurrentRung ].NextRung;
		InfosGene->OffsetCurrentRungDisplayed += InfosGene->BlockHeight*RUNG_HEIGHT;
		DecptNbrRungs--;
	}

//printf("-> CurrentRung=%d , OffsetCurrentRungDisplayed=%d\n", InfosGene->CurrentRung, InfosGene->OffsetCurrentRungDisplayed);
	if ( InfosGene->OffsetCurrentRungDisplayed<0 )
		printf( "Error in ChoiceOfTheCurrentRung( %d ) with OffsetCurrentRungDisplayed=%d\n", NbrOfRungsAfterTopRung, InfosGene->OffsetCurrentRungDisplayed );
	refresh_label_comment( );
}

static gint VScrollBar_value_changed_event( GtkAdjustment * ScrollBar, void * not_used )
{
	int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
	if ( iCurrentLanguage==SECTION_IN_LADDER )
	{
		int NumRung = ((int)ScrollBar->value)/(InfosGene->BlockHeight*RUNG_HEIGHT);
		int ScanRung = 0;
		InfosGene->TopRungDisplayed = InfosGene->FirstRung;
		if ( NumRung<0 )
			NumRung = 0;
		while( ScanRung!=NumRung )
		{
			InfosGene->TopRungDisplayed = RungArray[ InfosGene->TopRungDisplayed ].NextRung;
			ScanRung++;
		}
		InfosGene->OffsetHiddenTopRungDisplayed = ((int)ScrollBar->value)%(InfosGene->BlockHeight*RUNG_HEIGHT);

		// if top rung displayed entirely (no vertical offset), it's the current rung => give '0'.
		// else, search the next one => give '1'.
		ChoiceOfTheCurrentRung( (InfosGene->OffsetHiddenTopRungDisplayed>0)?1:0 );
	}
	InfosGene->VScrollValue = (int)ScrollBar->value;
	return TRUE;
}
static gint HScrollBar_value_changed_event( GtkAdjustment * ScrollBar, void * not_used )
{
	InfosGene->HScrollValue = (int)ScrollBar->value;
	return TRUE;
}

static gint button_press_event( GtkWidget *widget, GdkEventButton *event )
{
	if (event->button == 1 && pixmap != NULL)
	{
		if (EditDatas.ModeEdit)
		{
			EditElementInThePage(event->x,event->y);
		}
		else
		{
			// we can select the current rung by clicking on one.
			// the current rung is the one that will be modified...
			int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
			if ( iCurrentLanguage==SECTION_IN_LADDER )
			{
				char DoSelection = TRUE;
				if ( InfosGene->OffsetHiddenTopRungDisplayed>0 )
				{
					if ( event->y<InfosGene->BlockHeight*RUNG_HEIGHT-InfosGene->OffsetHiddenTopRungDisplayed )
						DoSelection = FALSE;
				}
				if ( DoSelection )
				{
					int NbrRungsShift =  (event->y+InfosGene->OffsetHiddenTopRungDisplayed)/(InfosGene->BlockHeight*RUNG_HEIGHT);
printf("Select the current rung, with a shift of rungs=%d\n", NbrRungsShift );
					ChoiceOfTheCurrentRung( NbrRungsShift );
				}
			}
		}
	}
	return TRUE;
}

/* Draw a rectangle on the screen */
/*static void draw_brush( GtkWidget *widget,
                        gdouble    x,
                        gdouble    y)
{
    GdkRectangle update_rect;

    update_rect.x = x - 5;
    update_rect.y = y - 5;
    update_rect.width = 100;
    update_rect.height = 100;
    gdk_draw_rectangle (pixmap,
                        widget->style->black_gc,
                        TRUE,
                        update_rect.x, update_rect.y,
                        update_rect.width, update_rect.height);
    gtk_widget_draw (widget, &update_rect);
}*/

static gint chkvar_press_event( GtkWidget      *widget, void * numcheck )
{
	int NumCheckWidget = (int)numcheck;
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

static gint EntryVarSpy_activate_event(GtkWidget *widget, int * NumVarSpy)
{
	int NewVarType,NewVarOffset;
	char BufferVar[30];
	strcpy(BufferVar, gtk_entry_get_text((GtkEntry *)widget) );
	if (TextParserForAVar(BufferVar , &NewVarType, &NewVarOffset, NULL, FALSE/*PartialNames*/))
	{
		*NumVarSpy++ = NewVarType;
		*NumVarSpy = NewVarOffset;
	}
	else
	{
		int OldType,OldOffset;
		/* Error Message */
printf("ici...\n");
		if (ErrorMessageVarParser)
			ShowMessageBox("Error",ErrorMessageVarParser,"Ok");
		else
			ShowMessageBox( "Error", "Unknown variable...", "Ok" );
		OldType = *NumVarSpy++;
		OldOffset = *NumVarSpy;
		/* put back old correct var */
		gtk_entry_set_text((GtkEntry *)widget,DisplayInfo(OldType,OldOffset));
	}
	return TRUE;
}

static gint OffsetBoolVar_activate_event(GtkWidget *widget, void * NumVarSpy)
{
	int Maxi = 0;
	int NumType = (int)NumVarSpy;
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

void refresh_label_comment( void )
{
    StrRung * RfhRung;
    if ( SectionArray[ InfosGene->CurrentSection ].Language==SECTION_IN_LADDER )
    {
        RfhRung = &RungArray[InfosGene->CurrentRung];
        gtk_entry_set_text((GtkEntry *)entrylabel,RfhRung->Label);
        gtk_entry_set_text((GtkEntry *)entrycomment,RfhRung->Comment);
    }
    else
    {
        gtk_entry_set_text((GtkEntry *)entrylabel,"");
        gtk_entry_set_text((GtkEntry *)entrycomment,"");
    }
}
void clear_label_comment()
{
    gtk_entry_set_text((GtkEntry *)entrylabel,"");
    gtk_entry_set_text((GtkEntry *)entrycomment,"");
}

void save_label_comment_edited()
{
    strcpy(EditDatas.Rung.Label,gtk_entry_get_text((GtkEntry *)entrylabel));
    strcpy(EditDatas.Rung.Comment,gtk_entry_get_text((GtkEntry *)entrycomment));
}

void autorize_prevnext_buttons(int Yes)
{
    if (Yes)
    {
        gtk_widget_set_sensitive(VScrollBar, TRUE);
        gtk_widget_set_sensitive(entrylabel, FALSE);
        gtk_widget_set_sensitive(entrycomment, FALSE);
    }
    else
    {
        gtk_widget_set_sensitive(VScrollBar, FALSE);
        gtk_widget_set_sensitive(entrylabel, TRUE);
        gtk_widget_set_sensitive(entrycomment, TRUE);
    }
}

void ButtonRunStop_click()
{
    if (InfosGene->LadderState==STATE_RUN)
    {
        InfosGene->LadderState = STATE_STOP;
        gtk_label_set_text(GTK_LABEL(GTK_BIN(ButtonRunStop)->child),"Run");
    }
    else
    {
        InfosGene->LadderState = STATE_RUN;
        gtk_label_set_text(GTK_LABEL(GTK_BIN(ButtonRunStop)->child),"Stop");
    }
}

void CheckDispSymbols_toggled( )
{
	InfosGene->DisplaySymbols = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CheckDispSymbols ) );
}


void StoreDirectorySelected(
	#ifndef GTK2
GtkFileSelection *selector, 
	#else
GtkFileChooser *selector,	
#endif
char cForLoadingProject)
{
    char * TempDir;
	
    TempDir = 
	#ifndef GTK2
	gtk_file_selection_get_filename (GTK_FILE_SELECTION(FileSelector));
	#else
	gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(FileSelector));
	#endif
    if ( cForLoadingProject )
        VerifyDirectorySelected( TempDir );
    else
        strcpy( LadderDirectory, TempDir );
}


void LoadNewLadder()
{
    StoreDirectorySelected(
	#ifndef GTK2
	GTK_FILE_SELECTION(FileSelector)
	#else
	GTK_FILE_CHOOSER(FileSelector)
	#endif
	
, TRUE/*cForLoadingProject*/);
	
    if (InfosGene->LadderState==STATE_RUN)
        ButtonRunStop_click();
    InfosGene->LadderState = STATE_LOADING;
////    LoadAllLadderDatas(LadderDirectory);
	if ( !LoadProjectFiles( LadderDirectory ) )
		ShowMessageBox( "Load Error", "Failed to load the project file...", "Ok" );

    UpdateGtkAfterLoading( TRUE/*cCreateTimer*/ );
#ifndef RT_SUPPORT
        OpenHardware( 0 );
        ConfigHardware( );
#endif
    InfosGene->LadderState = STATE_STOP;
}
void ButtonSave_click()
{
////    SaveAllLadderDatas(LadderDirectory);
	if ( !SaveProjectFiles( LadderDirectory ) )
		ShowMessageBox( "Save Error", "Failed to save the project file...", "Ok" );
}

void SaveAsLadder(void)
{
    StoreDirectorySelected(
	#ifndef GTK2
	GTK_FILE_SELECTION(FileSelector)
	#else
	GTK_FILE_CHOOSER(FileSelector)
	#endif
	
	, FALSE/*cForLoadingProject*/);
////    SaveAllLadderDatas(LadderDirectory);
	if ( !SaveProjectFiles( LadderDirectory ) )
		ShowMessageBox( "Save Error", "Failed to save the project file...", "Ok" );
}



#ifdef GTK2
void
on_filechooserdialog_save_response(GtkDialog  *dialog,gint response_id,gpointer user_data)
{
	printf("SAVE %s %d\n",gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(FileSelector)),response_id);	

if(response_id==GTK_RESPONSE_ACCEPT || response_id==GTK_RESPONSE_OK)SaveAsLadder();
gtk_widget_destroy(GTK_WIDGET(dialog));
}
void
on_filechooserdialog_load_response(GtkDialog  *dialog,gint response_id,gpointer user_data)
{
printf("LOAD %s %d\n",gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(FileSelector)),response_id);	

if(response_id==-5)LoadNewLadder();
gtk_widget_destroy(GTK_WIDGET(dialog));
}
#endif


void CreateFileSelection(char * Prompt,int Save)
{
	
    /* From the example in gtkfileselection help */
    /* Create the selector */
    
	#ifndef GTK2
	FileSelector = gtk_file_selection_new(Prompt);
	if (Save)
        gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(FileSelector)->ok_button),
                            "clicked", GTK_SIGNAL_FUNC (SaveAsLadder), NULL);
    else
        gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(FileSelector)->ok_button),
                           "clicked", GTK_SIGNAL_FUNC (LoadNewLadder), NULL);
    /* Ensure that the dialog box is destroyed when the user clicks a button. */
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(FileSelector)->ok_button),
                                           "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
                                           (gpointer) FileSelector);
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(FileSelector)->cancel_button),
                                           "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
                                           (gpointer) FileSelector);
	
	#else
	if(Save) {
  FileSelector = gtk_file_chooser_dialog_new (Prompt, NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	} else {
	FileSelector = gtk_file_chooser_dialog_new (Prompt, NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
}

  gtk_window_set_type_hint (GTK_WINDOW (FileSelector), GDK_WINDOW_TYPE_HINT_DIALOG);
/*
  g_signal_connect ((gpointer) filechooserdialog, "file_activated",
                    G_CALLBACK (on_filechooserdialog_file_activated),
                    NULL);
					*/
					
		if(Save)				
  g_signal_connect ((gpointer) FileSelector, "response",
                    G_CALLBACK (on_filechooserdialog_save_response),
                    NULL);
		else
	  g_signal_connect ((gpointer) FileSelector, "response",
                    G_CALLBACK (on_filechooserdialog_load_response),
                    NULL);

		
  g_signal_connect_swapped ((gpointer) FileSelector, "close",
                            G_CALLBACK (gtk_widget_destroy),
                            GTK_OBJECT (FileSelector));

	#endif
    
   /* Display that dialog */
   gtk_widget_show (FileSelector);
}

void ButtonNew_click()
{
    ShowConfirmationBox("New","Do you really want to clear all datas ?",InitAllLadderDatas);
}
void ButtonLoad_click()
{
    CreateFileSelection("Please select the project to load",FALSE);
}

void ButtonSaveAs_click()
{
    CreateFileSelection("Please select the project to save",TRUE);
}

void ButtonReset_click()
{
	int StateBefore = InfosGene->LadderState;
	InfosGene->LadderState = STATE_STOP;
	// wait, to be sure calcs have ended...
	usleep( 100000 );
	PrepareRungs( );
	PrepareTimers( );
	PrepareMonostables( );
	PrepareCounters( );
#ifdef SEQUENTIAL_SUPPORT
	PrepareSequential( );
#endif
	if ( StateBefore==STATE_RUN )
		InfosGene->LadderState = STATE_RUN;
}

void ButtonConfig_click()
{
    OpenConfigWindowGtk( );
}

void ButtonAbout_click()
{
	/* From the example in gtkdialog help */
	GtkWidget *dialog, *label, *okay_button;
	/* Create the widgets */
	dialog = gtk_dialog_new();
	label = gtk_label_new ("ClassicLadder v" RELEASE_VER_STRING "\n" RELEASE_DATE_STRING "\n"
						"Copyright (C) 2001-2006 Marc Le Douarain\nmarc . le - douarain /At\\ laposte \\DoT/ net\n"
						"http://www.sourceforge.net/projects/classicladder\n"
						"http://www.multimania.com/mavati/classicladder\n"
						"Released under the terms of the\nGNU Lesser General Public License v2.1");
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER );
	okay_button = gtk_button_new_with_label("Okay");
	/* Ensure that the dialog box is destroyed when the user clicks ok. */
	gtk_signal_connect_object (GTK_OBJECT (okay_button), "clicked",
							GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT(dialog));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
					okay_button);
	gtk_widget_grab_focus(okay_button);
	/* Add the label, and show everything we've added to the dialog. */
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
					label);
	gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_CENTER);
	gtk_widget_show_all (dialog);
}

void ShowMessageBox(char * title,char * text,char * button)
{
	/* From the example in gtkdialog help */
	GtkWidget *dialog, *label, *okay_button;
	/* Create the widgets */
	dialog = gtk_dialog_new();
	label = gtk_label_new (text);
	okay_button = gtk_button_new_with_label(button);
	/* Ensure that the dialog box is destroyed when the user clicks ok. */
	gtk_signal_connect_object (GTK_OBJECT (okay_button), "clicked",
							GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT(dialog));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
					okay_button);
	gtk_widget_grab_focus(okay_button);
	/* Add the label, and show everything we've added to the dialog. */
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
					label);
	gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);
	gtk_window_set_title(GTK_WINDOW(dialog),title);
	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_CENTER);
	gtk_widget_show_all (dialog);
}

void DoFunctionOfConfirmationBox(void * (*function_to_do)(void *))
{
	gtk_widget_destroy(ConfirmDialog);
	(function_to_do)(NULL);
}
void ShowConfirmationBox(char * title,char * text,void * function_if_yes)
{
	/* From the example in gtkdialog help */
	GtkWidget *label, *yes_button, *no_button;
	/* Create the widgets */
	ConfirmDialog = gtk_dialog_new();
	label = gtk_label_new (text);
	yes_button = gtk_button_new_with_label("Yes");
	no_button = gtk_button_new_with_label("No");
	/* Ensure that the dialog box is destroyed when the user clicks ok. */
	gtk_signal_connect_object (GTK_OBJECT (no_button), "clicked",
							GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT(ConfirmDialog));
	gtk_signal_connect_object (GTK_OBJECT (yes_button), "clicked",
							GTK_SIGNAL_FUNC (DoFunctionOfConfirmationBox), function_if_yes);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(ConfirmDialog)->action_area),
					yes_button);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(ConfirmDialog)->action_area),
					no_button);
	gtk_widget_grab_focus(no_button);
	/* Add the label, and show everything we've added to the dialog. */
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(ConfirmDialog)->vbox),
					label);
	gtk_window_set_modal(GTK_WINDOW(ConfirmDialog),TRUE);
	gtk_window_set_title(GTK_WINDOW(ConfirmDialog),title);
	gtk_window_set_position(GTK_WINDOW(ConfirmDialog),GTK_WIN_POS_CENTER);
	gtk_widget_show_all (ConfirmDialog);
}

void DoQuit( void )
{
	gtk_widget_destroy( RungWindow );
}
void ConfirmQuit( void )
{
	if ( InfosGene->AskConfirmationToQuit )
		ShowConfirmationBox( "Sure?", "Do you really want to quit ?\nIf not saved, all modifications will be lost  \n", DoQuit );
	else
		DoQuit( );
}
gint RungWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
	ConfirmQuit( );
	// we do not want that the window be destroyed.
	return TRUE;
}
void OpenEditWindow( void )
{
	gtk_widget_show (EditWindow);
#ifdef GTK2
	gtk_window_present( GTK_WINDOW(EditWindow) );
#endif
}

void RungWindowInitGtk()
{
    GtkWidget *vbox,*hboxtop,*hboxbottom,*hboxbottom2;
    GtkWidget *hboxmiddle;
    GtkWidget *ButtonQuit;
    GtkWidget *ButtonNew,*ButtonLoad,*ButtonSave,*ButtonSaveAs,*ButtonReset,*ButtonConfig,*ButtonAbout;
    GtkWidget *ButtonEdit,*ButtonSymbols;
#ifdef GNOME_PRINT_USE
    GtkWidget *ButtonPrint,*ButtonPrintPreview;
#endif

    RungWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title ((GtkWindow *)RungWindow, "Section Display");

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (RungWindow), vbox);
    gtk_widget_show (vbox);

    gtk_signal_connect (GTK_OBJECT (RungWindow), "destroy",
                        GTK_SIGNAL_FUNC (quit_appli), NULL);

    hboxtop = gtk_hbox_new (FALSE,0);
    gtk_container_add (GTK_CONTAINER (vbox), hboxtop);
    gtk_widget_show(hboxtop);
    gtk_box_set_child_packing(GTK_BOX(vbox), hboxtop,
        /*expand*/ FALSE, /*fill*/ FALSE, /*pad*/ 0, GTK_PACK_START);

    entrylabel = gtk_entry_new();
    gtk_widget_set_usize((GtkWidget *)entrylabel,80,0);
    gtk_entry_set_max_length((GtkEntry *)entrylabel,LGT_LABEL-1);
    gtk_entry_prepend_text((GtkEntry *)entrylabel,"");
    gtk_box_pack_start (GTK_BOX (hboxtop), entrylabel, FALSE, FALSE, 0);
    gtk_widget_show(entrylabel);
    entrycomment = gtk_entry_new();
    gtk_entry_set_max_length((GtkEntry *)entrycomment,LGT_COMMENT-1);
    gtk_entry_prepend_text((GtkEntry *)entrycomment,"");
    gtk_box_pack_start (GTK_BOX (hboxtop), entrycomment, TRUE, TRUE, 0);
    gtk_widget_show(entrycomment);

	CheckDispSymbols = gtk_check_button_new_with_label("Display symbols");
	gtk_box_pack_start( GTK_BOX (hboxtop), CheckDispSymbols, FALSE, FALSE, 0 );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CheckDispSymbols ), InfosGene->DisplaySymbols );
	gtk_signal_connect( GTK_OBJECT(CheckDispSymbols), "toggled",
				(GtkSignalFunc)CheckDispSymbols_toggled, NULL );
	gtk_widget_show( CheckDispSymbols );

#if defined( RT_SUPPORT ) || defined( __XENO__ )
    DurationOfLastScan = gtk_entry_new();
    gtk_widget_set_usize((GtkWidget *)DurationOfLastScan,60,0);
//    gtk_entry_set_max_length((GtkEntry *)DurationOfLastScan,LGT_COMMENT-1);
//    gtk_entry_set_max_length((GtkEntry *)DurationOfLastScan,20);
    gtk_entry_prepend_text((GtkEntry *)DurationOfLastScan,"");
    gtk_box_pack_start (GTK_BOX (hboxtop), DurationOfLastScan, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(DurationOfLastScan, FALSE);
    gtk_widget_show(DurationOfLastScan);
#endif


    hboxmiddle = gtk_hbox_new (FALSE,0);
    gtk_container_add (GTK_CONTAINER (vbox), hboxmiddle);
    gtk_widget_show(hboxmiddle);
    gtk_box_set_child_packing(GTK_BOX(vbox), hboxmiddle,
        /*expand*/ TRUE, /*fill*/ TRUE, /*pad*/ 0, GTK_PACK_START);

    /* Create the drawing area */
    drawing_area = gtk_drawing_area_new ();
    gtk_drawing_area_size (GTK_DRAWING_AREA (drawing_area) ,
                            BLOCK_WIDTH_DEF*RUNG_WIDTH+OFFSET_X+5 ,
                            BLOCK_HEIGHT_DEF*RUNG_HEIGHT+OFFSET_Y+30);
    gtk_box_pack_start (GTK_BOX (hboxmiddle), drawing_area, TRUE, TRUE, 0);
    gtk_widget_show (drawing_area);

    AdjustVScrollBar = (GtkAdjustment *)gtk_adjustment_new( 0, 0, 0, 0, 0, 0);
    VScrollBar = gtk_vscrollbar_new( AdjustVScrollBar );
    gtk_box_pack_start (GTK_BOX (hboxmiddle), VScrollBar, FALSE, FALSE, 0);
    gtk_widget_show (VScrollBar);

    AdjustHScrollBar = (GtkAdjustment *)gtk_adjustment_new( 0, 0, 0, 0, 0, 0);
    HScrollBar = gtk_hscrollbar_new( AdjustHScrollBar );
    gtk_box_pack_start (GTK_BOX (vbox), HScrollBar, FALSE, FALSE, 0);
    gtk_widget_show (HScrollBar);
    UpdateVScrollBar();

    gtk_signal_connect(GTK_OBJECT (AdjustVScrollBar), "value-changed",
                        (GtkSignalFunc) VScrollBar_value_changed_event, 0);
    gtk_signal_connect(GTK_OBJECT (AdjustHScrollBar), "value-changed",
                        (GtkSignalFunc) HScrollBar_value_changed_event, 0);

    hboxbottom = gtk_hbox_new (FALSE,0);
    gtk_container_add (GTK_CONTAINER (vbox), hboxbottom);
    gtk_widget_show(hboxbottom);
    gtk_box_set_child_packing(GTK_BOX(vbox), hboxbottom,
        /*expand*/ FALSE, /*fill*/ FALSE, /*pad*/ 0, GTK_PACK_START);

    hboxbottom2 = gtk_hbox_new (FALSE,0);
    gtk_container_add (GTK_CONTAINER (vbox), hboxbottom2);
    gtk_widget_show(hboxbottom2);
    gtk_box_set_child_packing(GTK_BOX(vbox), hboxbottom2,
        /*expand*/ FALSE, /*fill*/ FALSE, /*pad*/ 0, GTK_PACK_START);

    ButtonNew = gtk_button_new_with_label ("New");
    gtk_box_pack_start (GTK_BOX (hboxbottom), ButtonNew, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonNew), "clicked",
                        (GtkSignalFunc) ButtonNew_click, 0);
    gtk_widget_show (ButtonNew);
    ButtonLoad = gtk_button_new_with_label ("Load");
    gtk_box_pack_start (GTK_BOX (hboxbottom), ButtonLoad, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonLoad), "clicked",
                        (GtkSignalFunc) ButtonLoad_click, 0);
    gtk_widget_show (ButtonLoad);
    ButtonSave = gtk_button_new_with_label ("Save");
    gtk_box_pack_start (GTK_BOX (hboxbottom), ButtonSave, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonSave), "clicked",
                        (GtkSignalFunc) ButtonSave_click, 0);
    gtk_widget_show (ButtonSave);
    ButtonSaveAs = gtk_button_new_with_label ("Save As");
    gtk_box_pack_start (GTK_BOX (hboxbottom), ButtonSaveAs, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonSaveAs), "clicked",
                        (GtkSignalFunc) ButtonSaveAs_click, 0);
    gtk_widget_show (ButtonSaveAs);
    ButtonReset = gtk_button_new_with_label ("Reset");
    gtk_box_pack_start (GTK_BOX (hboxbottom), ButtonReset, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonReset), "clicked",
                        (GtkSignalFunc) ButtonReset_click, 0);
    gtk_widget_show (ButtonReset);
    ButtonRunStop = gtk_button_new_with_label ("Stop");
    gtk_box_pack_start (GTK_BOX (hboxbottom), ButtonRunStop, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonRunStop), "clicked",
                        (GtkSignalFunc) ButtonRunStop_click, 0);
    gtk_widget_show (ButtonRunStop);

    ButtonEdit = gtk_button_new_with_label ("Editor");
    gtk_box_pack_start (GTK_BOX (hboxbottom2), ButtonEdit, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonEdit), "clicked",
                        (GtkSignalFunc) OpenEditWindow, 0);
    gtk_widget_show (ButtonEdit);
    ButtonSymbols = gtk_button_new_with_label ("Symbols");
    gtk_box_pack_start (GTK_BOX (hboxbottom2), ButtonSymbols, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonSymbols), "clicked",
                        (GtkSignalFunc) OpenSymbolsWindow, 0);
    gtk_widget_show (ButtonSymbols);
    ButtonConfig = gtk_button_new_with_label ("Config");
    gtk_box_pack_start (GTK_BOX (hboxbottom2), ButtonConfig, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonConfig), "clicked",
                        (GtkSignalFunc) ButtonConfig_click, 0);
    gtk_widget_show (ButtonConfig);
#ifdef GNOME_PRINT_USE
    ButtonPrintPreview = gtk_button_new_with_label ("Preview");
    gtk_box_pack_start (GTK_BOX (hboxbottom2), ButtonPrintPreview, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonPrintPreview), "clicked",
                        (GtkSignalFunc) PrintPreviewGnome, 0);
    gtk_widget_show (ButtonPrintPreview);
    ButtonPrint = gtk_button_new_with_label ("Print");
    gtk_box_pack_start (GTK_BOX (hboxbottom2), ButtonPrint, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonPrint), "clicked",
                        (GtkSignalFunc) PrintGnome, 0);
    gtk_widget_show (ButtonPrint);
#endif
    ButtonAbout = gtk_button_new_with_label ("About");
    gtk_box_pack_start (GTK_BOX (hboxbottom2), ButtonAbout, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT (ButtonAbout), "clicked",
                        (GtkSignalFunc) ButtonAbout_click, 0);
    gtk_widget_show (ButtonAbout);
    ButtonQuit = gtk_button_new_with_label ("Quit");
    gtk_box_pack_start (GTK_BOX (hboxbottom2), ButtonQuit, TRUE, TRUE, 0);
//    gtk_signal_connect_object (GTK_OBJECT (ButtonQuit), "clicked",
//                                GTK_SIGNAL_FUNC (gtk_widget_destroy),
//                                GTK_OBJECT (RungWindow));
    gtk_signal_connect_object (GTK_OBJECT (ButtonQuit), "clicked",
                                ConfirmQuit, NULL);
    gtk_widget_show (ButtonQuit);


    /* Signals used to handle backing pixmap */
    gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
                        (GtkSignalFunc) expose_event, NULL);
    gtk_signal_connect (GTK_OBJECT(drawing_area),"configure_event",
                        (GtkSignalFunc) configure_event, NULL);

    /* Event signals */
    gtk_signal_connect (GTK_OBJECT (drawing_area), "button_press_event",
                        (GtkSignalFunc) button_press_event, NULL);

    gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
                            | GDK_LEAVE_NOTIFY_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_POINTER_MOTION_HINT_MASK);

    gtk_signal_connect( GTK_OBJECT(RungWindow), "delete_event",
        (GtkSignalFunc)RungWindowDeleteEvent, 0 );
    gtk_widget_show (RungWindow);

    GetTheSizesForRung();
}

gint VarsWindowDeleteEvent( GtkWidget * widget, GdkEvent * event, gpointer data )
{
	// we do not want that the window be destroyed.
	return TRUE;
}

void VarsWindowInitGtk()
{
	GtkWidget *windowvars;
	GtkWidget *vboxboolvars[ NBR_TYPE_BOOLS_SPY ],*vboxmain,*hboxvars,*hboxvars2;
	GtkWidget * hboxfreevars[ NBR_FREE_VAR_SPY ];
	int NumCheckWidget,ColumnVar,NumVarSpy,NumEntry;
	GList *DisplayFormatItems = NULL;
	
	windowvars = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *)windowvars, "Vars");
	vboxmain = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (windowvars), vboxmain);
	gtk_widget_show (vboxmain);
	hboxvars = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vboxmain), hboxvars);
	gtk_widget_show (hboxvars);
	hboxvars2 = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vboxmain), hboxvars2);
	gtk_widget_show (hboxvars2);

	for( ColumnVar=0; ColumnVar<NBR_TYPE_BOOLS_SPY; ColumnVar++ )
	{
		vboxboolvars[ ColumnVar ] = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (hboxvars), vboxboolvars[ ColumnVar ]);
		gtk_widget_show (vboxboolvars[ ColumnVar ]);
	}

	DisplayFormatItems = g_list_append(DisplayFormatItems,"Dec");
	DisplayFormatItems = g_list_append(DisplayFormatItems,"Hex");
	DisplayFormatItems = g_list_append(DisplayFormatItems,"Bin");
	
	NumCheckWidget = 0;
	for(ColumnVar=0; ColumnVar<NBR_TYPE_BOOLS_SPY; ColumnVar++)
	{
		int OffVar;
		offsetboolvar[ ColumnVar ]  = gtk_entry_new();
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

	for(NumVarSpy=0; NumVarSpy<NBR_FREE_VAR_SPY; NumVarSpy++)
	{
		hboxfreevars[ NumVarSpy ] = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vboxmain), hboxfreevars[ NumVarSpy ]);
		gtk_widget_show (hboxfreevars[ NumVarSpy ]);
		for(ColumnVar=0; ColumnVar<2; ColumnVar++)
		{
			NumEntry = NumVarSpy+ColumnVar*NBR_FREE_VAR_SPY;
			EntryVarSpy[ NumEntry ] = gtk_entry_new();
			gtk_widget_set_usize((GtkWidget *)EntryVarSpy[ NumEntry ],(ColumnVar==0)?50:80,0);
			gtk_box_pack_start (GTK_BOX( hboxfreevars[ NumVarSpy ] ), EntryVarSpy[ NumEntry ], TRUE, TRUE, 0);
			gtk_widget_show(EntryVarSpy[NumEntry]);
			if ( ColumnVar==0 )
			{
				gtk_entry_set_text((GtkEntry *)EntryVarSpy[ NumEntry ],DisplayInfo(VarSpy[NumVarSpy][0],VarSpy[NumVarSpy][1]));
				gtk_signal_connect(GTK_OBJECT (EntryVarSpy[ NumEntry ]), "activate",
                                        (GtkSignalFunc) EntryVarSpy_activate_event, &VarSpy[NumVarSpy][0]);
			}
		}

		DisplayFormatVarSpy[NumVarSpy] = gtk_combo_new();
		gtk_combo_set_value_in_list(GTK_COMBO(DisplayFormatVarSpy[NumVarSpy]), TRUE /*val*/, FALSE /*ok_if_empty*/);
		gtk_combo_set_popdown_strings(GTK_COMBO(DisplayFormatVarSpy[NumVarSpy]), DisplayFormatItems);
		gtk_widget_set_usize((GtkWidget *)DisplayFormatVarSpy[NumVarSpy],65,0);
		gtk_box_pack_start (GTK_BOX(hboxfreevars[ NumVarSpy ]), DisplayFormatVarSpy[NumVarSpy], FALSE, FALSE, 0);
		gtk_widget_show(DisplayFormatVarSpy[NumVarSpy]);
	}
	
	gtk_signal_connect( GTK_OBJECT(windowvars), "delete_event",
		(GtkSignalFunc)VarsWindowDeleteEvent, 0 );
	gtk_widget_show (windowvars);
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

void DisplayFreeVarSpy()
{
	int NumVarSpy;
	int Value;
	char BufferValue[50];
	char DisplayFormat[10];
	for (NumVarSpy=0; NumVarSpy<NBR_FREE_VAR_SPY; NumVarSpy++)
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
		gtk_entry_set_text((GtkEntry *)EntryVarSpy[NBR_FREE_VAR_SPY+NumVarSpy],BufferValue);
	}
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
			char BufNumVar[20];
			switch( ColumnVar )
			{
				case 0: sprintf(BufNumVar, "%cB%d",'%', OffVar+ValOffsetBoolVar[ ColumnVar ]); break;
				case 1: sprintf(BufNumVar, "%cI%d",'%', OffVar+ValOffsetBoolVar[ ColumnVar ]); break;
				case 2: sprintf(BufNumVar, "%cQ%d",'%', OffVar+ValOffsetBoolVar[ ColumnVar ]); break;
			}
			gtk_label_set_text(GTK_LABEL(GTK_BIN( chkvar[ ColumnVar ][ OffVar ] )->child),BufNumVar);
		}
	}
}

void quit_appli()
{
	InfosGene->LadderState = STATE_LOADING;
#ifdef HAL_SUPPORT
        hal_exit(compId);
#else
	CloseSocketModbusMaster( );
	CloseSocketServer( );
#endif
	ClassicLadderFreeAll();
	gtk_exit (0);
}

static gint PeriodicUpdateDisplay(gpointer data)
{
	if (InfosGene->LadderState==STATE_RUN)
	{
#if defined( RT_SUPPORT ) || defined( __XENO__ )
		char TextBuffer[ 20 ];
		sprintf(TextBuffer , "%d us", InfosGene->DurationOfLastScan/1000);
		gtk_entry_set_text(GTK_ENTRY(DurationOfLastScan),TextBuffer);
#endif
		if (InfosGene->CmdRefreshVarsBits)
		{
			RefreshAllBoolsVars();
			InfosGene->CmdRefreshVarsBits = FALSE;
		}
		DisplayFreeVarSpy();
	}
//    DumpRung(&RungArray[0]);
	if (InfosGene->LadderState!=STATE_LOADING )
		DrawCurrentSection( );
	return 1;
}


void InitGtkWindows( int argc, char *argv[] )
{
	printf( "Your GTK+ version is %d.%d.%d\n", gtk_major_version, gtk_minor_version,
			gtk_micro_version );
//ProblemWithPrint	g_thread_init (NULL);
//ProblemWithPrint	gdk_threads_init ();
    gtk_init (&argc, &argv);

	RungWindowInitGtk();
	VarsWindowInitGtk();
	EditorInitGtk();
	PropertiesInitGtk();
	ManagerInitGtk( );
	SymbolsInitGtk( );
}

void UpdateGtkAfterLoading( char cCreateTimer )
{
	DrawCurrentSection( );
	refresh_label_comment( );
	autorize_prevnext_buttons( TRUE );
	UpdateVScrollBar();

	if ( cCreateTimer )
	{
////#if !defined( RT_SUPPORT ) && !defined( __XENO__ )
////		printf("Init. cyclic refresh of rungs with GTK timer !\n");
////#endif
		gtk_timeout_add( TIME_UPDATE_GTK_DISPLAY_MS, PeriodicUpdateDisplay, NULL );
	}
}
