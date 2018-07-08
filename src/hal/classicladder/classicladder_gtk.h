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
void UpdateVScrollBar();
void save_label_comment_edited();
void refresh_label_comment( void );
void clear_label_comment();
void autorize_prevnext_buttons(int Yes);
void ShowMessageBox(const char * title, const char * text, const char * button);
void ShowConfirmationBoxWithChoiceOrNot(const char * title, const char * text, void * function_if_yes, char HaveTheChoice);
void ShowConfirmationBox(const char * title, const char * text, void * function_if_yes);
void RefreshOneBoolVar( int Type, int Num, int Val );
void RefreshAllBoolsVars( );
void UpdateAllLabelsBoolsVars( );
void DoQuitGtkApplication( void );
void DoReset( );
void MessageInStatusBar( char * msg );
void InitGtkWindows( int argc, char *argv[] );
void UpdateAllGtkWindows( void );
void UpdateWindowTitleWithProjectName( void );
void ShowErrorMessage(const char * title, const char * text, const char * button);
void CheckForErrors (void);
