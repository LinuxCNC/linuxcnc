

#ifndef CLASSICLADDER_MENU_AND_TOOLBAR_GTK_H
#define CLASSICLADDER_MENU_AND_TOOLBAR_GTK_H

#include <gtk/gtk.h>

GtkUIManager * InitMenusAndToolBar( GtkWidget *vbox );
void SetToogleMenuForSectionsManagerWindow( gboolean OpenedWin );
void SetToggleMenuForEditorWindow( gboolean OpenedWin );
void SetToggleMenuForSymbolsWindow( gboolean OpenedWin );
void SetToggleMenuForBoolVarsWindow( gboolean OpenedWin );
void SetToggleMenuForFreeVarsWindow( gboolean OpenedWin );
void SetToggleMenuForLogWindow( gboolean OpenedWin );
void SetMenuStateForRunStopSwitch( gboolean Running );


#endif
