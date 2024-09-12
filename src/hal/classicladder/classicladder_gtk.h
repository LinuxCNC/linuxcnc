//    Copyright 2005-2008, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifndef _CLASSICLADDER_GTK_H
#define _CLASSICLADDER_GTK_H

//For i18n
#define _(STRING) gettext(STRING)
#define gettext_noop(STRING) (STRING)
#define N_(STRING) gettext_noop(STRING)

// macros used to be compatible with GTK v2.16 (used on Windows), perhaps a little before big switch to GTK3...
#if ( GTK_MAJOR_VERSION>=2 && GTK_MINOR_VERSION>=18 ) || (GTK_MAJOR_VERSION>=3)
#define MY_GTK_WIDGET_VISIBLE(widget) gtk_widget_get_visible(widget)
#else
#define MY_GTK_WIDGET_VISIBLE(widget) GTK_WIDGET_VISIBLE(widget)
#endif
#if GTK_MAJOR_VERSION>=3
#define GTK_OBJECT(o) G_OBJECT(o)
#define GTK_SIGNAL_FUNC(f)	    G_CALLBACK(f)
#define gtk_signal_connect(instance, detailed_signal, c_handler, data) g_signal_connect(instance, detailed_signal, c_handler, data)
#define gtk_signal_disconnect(instance, handler_id) g_signal_handler_disconnect(instance, handler_id)
#endif
#if ( GTK_MAJOR_VERSION>=2 && GTK_MINOR_VERSION>=24 ) || (GTK_MAJOR_VERSION>=3)
#define MyGtkComboBox GtkComboBoxText
#define MY_GTK_COMBO_BOX GTK_COMBO_BOX_TEXT
#define gtk_combo_box_new_text() gtk_combo_box_text_new()
#define gtk_combo_box_append_text(c,t) gtk_combo_box_text_append_text(c,t)
#define gtk_combo_box_get_active_text(c) gtk_combo_box_text_get_active_text(c)
#define gtk_combo_box_remove_text(c,p) gtk_combo_box_text_remove(c,p)
#else
#define MyGtkComboBox GtkComboBox
#define MY_GTK_COMBO_BOX GTK_COMBO_BOX
#endif


void GetCurrentNumAndNbrRungsForASection( int * pCurrNumRung, int * pNbrRungs );
void GetTheSizesForRung( void );
void UpdateVScrollBar();
void save_label_comment_edited();
void refresh_label_comment( void );
void clear_label_comment();
void autorize_prevnext_buttons(int Yes);
void DoActionSave( );
void DoActionConfirmNewProject( );
void DoActionLoadProject( );
void DoActionSaveAs( );
void DoActionResetAndConfirmIfRunning( );
void ButtonConfig_click();
void DoActionAboutClassicLadder();
void DoActionExportSvg( void );
void DoActionExportPng( void );
void DoActionCopyToClipboard( void );
void ShowMessageBox(const char * title, const char * text, const char * button);
void ShowMessageBoxError( const char * text );
void ShowConfirmationBoxWithChoiceOrNot(const char * title, const char * text, void * function_if_yes, char HaveTheChoice);
void ShowConfirmationBox(const char * title, const char * text, void * function_if_yes);
void RefreshOneBoolVar( int Type, int Num, int Val );
void RefreshAllBoolsVars( );
void UpdateAllLabelsBoolsVars( );
void DoQuitGtkApplication( void );
void DoReset( );
void ConfirmQuit( );
void MessageInStatusBar( char * msg );
void RedrawSignalDrawingArea( void );
void InitGtkWindows( int argc, char *argv[] );
void UpdateAllGtkWindows( void );
void UpdateWindowTitleWithProjectName( void );
void ShowErrorMessage(const char * title, const char * text, const char * button);
void CheckForErrors (void);

// Included for print_gtk.c
void GetCurrentNumAndNbrRungsForCurrentSection( int * NumCurrentRung, int * NbrRungs );
#endif
