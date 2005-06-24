/* Classic Ladder Project */
/* Copyright (C) 2001-2004 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* May 2001 */
/* --------------------------- */
/* Editor - GTK interface part */
/* --------------------------- */
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
#include "classicladder.h"
#include "global.h"
#include "drawing.h"
#include "edit.h"
#include "classicladder_gtk.h"
#include "edit_gtk.h"
#include "editproperties_gtk.h"

static GdkPixmap *EditorPixmap = NULL;
GtkWidget *EditorDrawingArea;
GtkWidget *EditorButtonValid, *EditorButtonCancel;
GtkWidget *EditorButtonAdd, *EditorButtonIns, *EditorButtonDel;
GtkWidget *EditorButtonModify;
#define NBR_ELE_TOOLBAR_Y 10
static short int ToolBarElementsLadder[NBR_ELE_TOOLBAR_Y + 1][2] =
    { {ELE_INPUT, ELE_INPUT_NOT},
{ELE_RISING_INPUT, ELE_FALLING_INPUT},
{ELE_CONNECTION, EDIT_CNX_WITH_TOP},
{EDIT_LONG_CONNECTION, ELE_COMPAR},
{ELE_TIMER, ELE_MONOSTABLE},
{ELE_OUTPUT, ELE_OUTPUT_NOT},
{ELE_OUTPUT_SET, ELE_OUTPUT_RESET},
{ELE_OUTPUT_JUMP, ELE_OUTPUT_CALL},
{ELE_OUTPUT_OPERATE, 0},
{0, EDIT_POINTER},
{-1, -1}			/* end */
};

#ifdef SEQUENTIAL_SUPPORT
#include "drawing_sequential.h"
#include "edit_sequential.h"
static short int ToolBarElementsSequential[][2] =
    { {ELE_SEQ_STEP, EDIT_SEQ_INIT_STEP},
{ELE_SEQ_TRANSITION, EDIT_SEQ_STEP_AND_TRANS},
{EDIT_SEQ_START_MANY_TRANS, EDIT_SEQ_END_MANY_TRANS},
{EDIT_SEQ_START_MANY_STEPS, EDIT_SEQ_END_MANY_STEPS},
{EDIT_SEQ_LINK, 0},
{0, EDIT_POINTER},
{-1, -1}			/* end */
};
#endif

static short int ToolBarYMaxi = 0;
static short int (*PtrToolBarElementsList)[2] = NULL;	// ptr for array with 
							// 
							// 2 dimensions, is
							// hard... :-(
static int ToolBarX = -1;
static int ToolBarY = -1;

/* Draw a rectangle on the screen */
static void draw_brush(GtkWidget * widget,
    gdouble x, gdouble y, GdkGC * color)
{
    GdkRectangle update_rect;

    update_rect.x = x;
    update_rect.y = y;
    update_rect.width = BLOCK_WIDTH_DEF + 1;
    update_rect.height = BLOCK_HEIGHT_DEF + 1;
    if (color == NULL)
	color = widget->style->black_gc;
    gdk_draw_rectangle(EditorPixmap,
	color,
	FALSE,
	update_rect.x, update_rect.y,
	update_rect.width - 1, update_rect.height - 1);
    gtk_widget_draw(widget, &update_rect);
}

/* Draw the tool bar of elements */
void DrawToolBarElements(short int PtrOnToolBarElementsList[][2])
{
    StrElement ToolBarEle;
    int ScanToolBarX, ScanToolBarY;
    ScanToolBarX = 0;
    ScanToolBarY = 0;
    do {
	ToolBarYMaxi = ScanToolBarY;
	ToolBarEle.Type =
	    PtrOnToolBarElementsList[ScanToolBarY][ScanToolBarX];
	ToolBarEle.ConnectedWithTop = 0;
#ifdef SEQUENTIAL_SUPPORT
	if (PtrOnToolBarElementsList == ToolBarElementsSequential)
	    DrawSeqElementForToolBar(EditorPixmap,
		ScanToolBarX * (BLOCK_WIDTH_DEF + 2) + 2,
		ScanToolBarY * (BLOCK_HEIGHT_DEF + 2) + 2, BLOCK_WIDTH_DEF,
		ToolBarEle.Type);
	else
#endif
	    DrawElement(EditorPixmap,
		ScanToolBarX * (BLOCK_WIDTH_DEF + 2) + 2,
		ScanToolBarY * (BLOCK_HEIGHT_DEF + 2) + 2, BLOCK_WIDTH_DEF,
		BLOCK_HEIGHT_DEF, ToolBarEle, TRUE);
	/* draw current element selected in the toolbar (required to display
	   default pointer selected at startup) */
	if (ToolBarEle.Type == EditDatas.NumElementSelectedInToolBar) {
	    draw_brush(GTK_WIDGET(EditorDrawingArea),
		ScanToolBarX * (BLOCK_WIDTH_DEF + 2) + 2,
		ScanToolBarY * (BLOCK_HEIGHT_DEF + 2) + 2, NULL);
	    ToolBarX = ScanToolBarX;
	    ToolBarY = ScanToolBarY;
	}

	ScanToolBarX++;
	if (ScanToolBarX > 1) {
	    ScanToolBarX = 0;
	    ScanToolBarY++;
	}
    }
    while (PtrOnToolBarElementsList[ScanToolBarY][ScanToolBarX] != -1);
    PtrToolBarElementsList = PtrOnToolBarElementsList;
}
void ReDrawToolBarElements(void)
{
    if (PtrToolBarElementsList != NULL)
	DrawToolBarElements(PtrToolBarElementsList);
}

/* Create a new backing pixmap of the appropriate size */
static gint EditorConfigureEvent(GtkWidget * widget,
    GdkEventConfigure * event)
{
    if (EditorPixmap)
	gdk_pixmap_unref(EditorPixmap);

    EditorPixmap = gdk_pixmap_new(widget->window,
	widget->allocation.width, widget->allocation.height, -1);
    gdk_draw_rectangle(EditorPixmap,
	widget->style->white_gc,
	TRUE, 0, 0, widget->allocation.width, widget->allocation.height);
    ReDrawToolBarElements();
    return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gint EditorExposeEvent(GtkWidget * widget, GdkEventExpose * event)
{
    gdk_draw_pixmap(widget->window,
	widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
	EditorPixmap,
	event->area.x, event->area.y,
	event->area.x, event->area.y, event->area.width, event->area.height);

    return FALSE;
}

static gint EditorSelectElement(GtkWidget * widget, GdkEventButton * event)
{
    if (EditDatas.ModeEdit) {
	int ToolBarXBak = ToolBarX;
	int ToolBarYBak = ToolBarY;
	if (event->button == 1 && pixmap != NULL) {
	    ToolBarX = event->x / BLOCK_WIDTH_DEF;
	    ToolBarY = event->y / BLOCK_HEIGHT_DEF;
	    if ((ToolBarXBak != -1) && (ToolBarYBak != -1))
		draw_brush(widget, ToolBarXBak * (BLOCK_WIDTH_DEF + 2) + 2,
		    ToolBarYBak * (BLOCK_HEIGHT_DEF + 2) + 2,
		    widget->style->white_gc);
	    if (ToolBarX > 1)
		ToolBarX = 1;
	    draw_brush(widget, ToolBarX * (BLOCK_WIDTH_DEF + 2) + 2,
		ToolBarY * (BLOCK_HEIGHT_DEF + 2) + 2,
		widget->style->black_gc);
	    if (ToolBarY <= ToolBarYMaxi)
		EditDatas.NumElementSelectedInToolBar =
		    PtrToolBarElementsList[ToolBarY][ToolBarX];
	    else
		EditDatas.NumElementSelectedInToolBar = ELE_FREE;
	}
    }
    return TRUE;
}

void ButtonsForStart()
{
    gtk_widget_hide(EditorButtonAdd);
    gtk_widget_hide(EditorButtonIns);
    gtk_widget_hide(EditorButtonDel);
    gtk_widget_hide(EditorButtonModify);
    gtk_widget_show(EditorButtonValid);
    gtk_widget_show(EditorButtonCancel);
    ShowPropertiesWindow(TRUE);
    /* select directly the pointer in toolbar */
    EditDatas.NumElementSelectedInToolBar = EDIT_POINTER;
}
void ButtonsForEnd(char ForRung)
{
    if (ForRung) {
	gtk_widget_show(EditorButtonAdd);
	gtk_widget_show(EditorButtonIns);
	gtk_widget_show(EditorButtonDel);
    } else {
	gtk_widget_hide(EditorButtonAdd);
	gtk_widget_hide(EditorButtonIns);
	gtk_widget_hide(EditorButtonDel);
    }
    gtk_widget_show(EditorButtonModify);
    gtk_widget_hide(EditorButtonValid);
    gtk_widget_hide(EditorButtonCancel);
    ShowPropertiesWindow(FALSE);
}

void EditorButtonsAccordingSectionType()
{
    int iCurrentLanguage = SectionArray[InfosGene->CurrentSection].Language;
    ButtonsForEnd(iCurrentLanguage == SECTION_IN_LADDER);
#ifdef SEQUENTIAL_SUPPORT
    if (iCurrentLanguage == SECTION_IN_SEQUENTIAL)
	DrawToolBarElements(ToolBarElementsSequential);
    else
#endif
	DrawToolBarElements(ToolBarElementsLadder);
}

void ButtonAddRung()
{
    AddRung();
    ButtonsForStart();
}

void ButtonInsertRung()
{
    InsertRung();
    ButtonsForStart();
}

void ButtonDeleteCurrentRung()
{
    ShowConfirmationBox("Delete",
	"Do you really want to delete the current rung ?", DeleteCurrentRung);
}

void ButtonModifyCurrentRung()
{
    int iCurrentLanguage = SectionArray[InfosGene->CurrentSection].Language;
    if (iCurrentLanguage == SECTION_IN_LADDER) {
	ModifyCurrentRung();
	ButtonsForStart();
    }
#ifdef SEQUENTIAL_SUPPORT
    if (iCurrentLanguage == SECTION_IN_SEQUENTIAL) {
	ModifyCurrentSeqPage();
	ButtonsForStart();
    }
#endif
}
void ButtonValidCurrentRung()
{
    int iCurrentLanguage = SectionArray[InfosGene->CurrentSection].Language;
    if (iCurrentLanguage == SECTION_IN_LADDER)
	ApplyRungEdited();
#ifdef SEQUENTIAL_SUPPORT
    if (iCurrentLanguage == SECTION_IN_SEQUENTIAL)
	ApplySeqPageEdited();
#endif
    ButtonsForEnd(iCurrentLanguage == SECTION_IN_LADDER);
}

void ButtonCancelCurrentRung()
{
    int iCurrentLanguage = SectionArray[InfosGene->CurrentSection].Language;
    if (iCurrentLanguage == SECTION_IN_LADDER)
	CancelRungEdited();
#ifdef SEQUENTIAL_SUPPORT
    if (iCurrentLanguage == SECTION_IN_SEQUENTIAL)
	CancelSeqPageEdited();
#endif
    ButtonsForEnd(iCurrentLanguage == SECTION_IN_LADDER);
}

gint EditorWindowDeleteEvent(GtkWidget * widget, GdkEvent * event,
    gpointer data)
{
    // we do not want that the window be destroyed.
    return TRUE;
}

void EditorInitGtk()
{
    GtkWidget *EditWindow;
    GtkWidget *vbox;

    EditWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title((GtkWindow *) EditWindow, "Editor");

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(EditWindow), vbox);
    gtk_widget_show(vbox);

    EditorButtonAdd = gtk_button_new_with_label("Add");
    gtk_box_pack_start(GTK_BOX(vbox), EditorButtonAdd, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(EditorButtonAdd), "clicked",
	(GtkSignalFunc) ButtonAddRung, 0);
    gtk_widget_show(EditorButtonAdd);
    EditorButtonIns = gtk_button_new_with_label("Insert");
    gtk_box_pack_start(GTK_BOX(vbox), EditorButtonIns, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(EditorButtonIns), "clicked",
	(GtkSignalFunc) ButtonInsertRung, 0);
    gtk_widget_show(EditorButtonIns);
    EditorButtonDel = gtk_button_new_with_label("Delete");
    gtk_box_pack_start(GTK_BOX(vbox), EditorButtonDel, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(EditorButtonDel), "clicked",
	(GtkSignalFunc) ButtonDeleteCurrentRung, 0);
    gtk_widget_show(EditorButtonDel);
    EditorButtonModify = gtk_button_new_with_label("Modify");
    gtk_box_pack_start(GTK_BOX(vbox), EditorButtonModify, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(EditorButtonModify), "clicked",
	(GtkSignalFunc) ButtonModifyCurrentRung, 0);
    gtk_widget_show(EditorButtonModify);
    EditorButtonValid = gtk_button_new_with_label("Valid.");
    gtk_box_pack_start(GTK_BOX(vbox), EditorButtonValid, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(EditorButtonValid), "clicked",
	(GtkSignalFunc) ButtonValidCurrentRung, 0);
    EditorButtonCancel = gtk_button_new_with_label("Cancel");
    gtk_box_pack_start(GTK_BOX(vbox), EditorButtonCancel, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(EditorButtonCancel), "clicked",
	(GtkSignalFunc) ButtonCancelCurrentRung, 0);

    /* Create the drawing area */
    EditorDrawingArea = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(EditorDrawingArea),
	(BLOCK_WIDTH_DEF + 2) * 2 + 2,
	(BLOCK_HEIGHT_DEF + 2) * NBR_ELE_TOOLBAR_Y + 2);
    gtk_box_pack_start(GTK_BOX(vbox), EditorDrawingArea, TRUE, TRUE, 0);
    gtk_widget_show(EditorDrawingArea);

    /* Signals used to handle backing pixmap */
    gtk_signal_connect(GTK_OBJECT(EditorDrawingArea), "expose_event",
	(GtkSignalFunc) EditorExposeEvent, NULL);
    gtk_signal_connect(GTK_OBJECT(EditorDrawingArea), "configure_event",
	(GtkSignalFunc) EditorConfigureEvent, NULL);

    /* Event signals */
    gtk_signal_connect(GTK_OBJECT(EditorDrawingArea), "button_press_event",
	(GtkSignalFunc) EditorSelectElement, NULL);

    gtk_widget_set_events(EditorDrawingArea, GDK_EXPOSURE_MASK
	| GDK_LEAVE_NOTIFY_MASK
	| GDK_BUTTON_PRESS_MASK
	| GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

    gtk_signal_connect(GTK_OBJECT(EditWindow), "delete_event",
	(GtkSignalFunc) EditorWindowDeleteEvent, 0);
    gtk_widget_show(EditWindow);

    EditDatas.NumElementSelectedInToolBar = -1;
}
