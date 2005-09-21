/* Classic Ladder Project */
/* Copyright (C) 2001-2003 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* February 2001 */
/* ----------- */
/* Draw a rung */
/* ----------- */
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
#include "arithm_eval.h"
#include "classicladder_gtk.h"
#include "edit.h"
#ifdef SEQUENTIAL_SUPPORT
#include "drawing_sequential.h"
#endif
#include "drawing.h"

char *DisplayInfo(int Type, int Offset)
{
    static char Buffer[20];
    switch (Type) {
    case VAR_MEM_BIT:
	sprintf(Buffer, "B%d", Offset);
	break;
    case VAR_TIMER_DONE:
	sprintf(Buffer, "T%d,D", Offset);
	break;
    case VAR_TIMER_RUNNING:
	sprintf(Buffer, "T%d,R", Offset);
	break;
    case VAR_MONOSTABLE_RUNNING:
	sprintf(Buffer, "M%d,R", Offset);
	break;
    case VAR_STEP_ACTIVITY:
	sprintf(Buffer, "X%d", Offset);
	break;
    case VAR_PHYS_INPUT:
	sprintf(Buffer, "I%d", Offset);
	break;
    case VAR_PHYS_OUTPUT:
	sprintf(Buffer, "Q%d", Offset);
	break;
    case VAR_MEM_WORD:
	sprintf(Buffer, "W%d", Offset);
	break;
    case VAR_STEP_TIME:
	sprintf(Buffer, "X%d,V", Offset);
	break;
    default:
	sprintf(Buffer, "???");
	break;
    }
    return Buffer;
}
char *DisplayArithmExpr(char *Expr, int NumCarMax)
{
    static char Buffer[ARITHM_EXPR_SIZE + 30];
    char *Ptr = Expr;
    int Fill = 0;
    Buffer[0] = '\0';
    /* null expression ? */
    if (Expr[0] == '\0')
	return Buffer;
    do {
	/* start of a variable ? */
	if (*Ptr == '@') {
	    int NumVar, TypeVar;
	    char VarBuffer[20];
	    if (IdentifyVariable(Ptr, &NumVar, &TypeVar))
		strcpy(VarBuffer, DisplayInfo(NumVar, TypeVar));
	    else
		strcpy(VarBuffer, "??");
	    strcpy(&Buffer[Fill], VarBuffer);
	    /* flush until end of a variable */
	    do {
		Ptr++;
	    }
	    while (*Ptr != '@');
	    Ptr++;
	    Fill = Fill + strlen(VarBuffer);
	} else {
	    Buffer[Fill++] = *Ptr++;
	}
    }
    while (*Ptr != '\0');
    Buffer[Fill] = '\0';
    /* size limited ? */
    if (NumCarMax > 0) {
	if (strlen(Buffer) > NumCarMax) {
	    Buffer[NumCarMax - 1] = '.';
	    Buffer[NumCarMax] = '.';
	    Buffer[NumCarMax + 1] = '\0';
	}
    }
    return Buffer;
}

void DrawElement(GdkPixmap * DrawPixmap, int x, int y, int Width, int Height,
    StrElement Element, char DrawForToolBar)
{
    char BufTxt[50];
    StrTimer *Timer;
    StrMonostable *Monostable;
    int WidDiv2 = Width / 2;
    int WidDiv3 = Width / 3;
    int WidDiv4 = Width / 4;
    int HeiDiv2 = Height / 2;
    int HeiDiv3 = Height / 3;
    int HeiDiv4 = Height / 4;
    GdkGC *DynaGcOff;
    GdkGC *TheGc;

    GdkColor DynaGdkColor;
    GdkGC *DynaGcOn;
    DynaGdkColor.pixel = 0xFF22FF;
    DynaGdkColor.red = 0xFF;
    DynaGdkColor.green = 0x22;
    DynaGdkColor.blue = 0xFF;

    DynaGcOn = gdk_gc_new(DrawPixmap);
    gdk_gc_set_foreground(DynaGcOn, &DynaGdkColor);
#ifdef THICK_LINE_ELE_ACTIVATED
    gdk_gc_set_line_attributes(DynaGcOn, THICK_LINE_ELE_ACTIVATED,
	GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
#endif
    DynaGcOff = drawing_area->style->black_gc;
    /* State with color */
    TheGc = drawing_area->style->black_gc;
    if ((!DrawForToolBar) && (!EditDatas.ModeEdit) && (Element.DynamicState))
	TheGc = DynaGcOn;
    if (EditDatas.ModeEdit) {
	gdk_gc_unref(DynaGcOn);
	DynaGcOn = drawing_area->style->black_gc;
    }
    /* Drawing - - */
    switch (Element.Type) {
    case ELE_INPUT:
    case ELE_INPUT_NOT:
    case ELE_RISING_INPUT:
    case ELE_FALLING_INPUT:
    case ELE_OUTPUT:
    case ELE_OUTPUT_NOT:
    case ELE_OUTPUT_SET:
    case ELE_OUTPUT_RESET:
    case ELE_OUTPUT_JUMP:
    case ELE_OUTPUT_CALL:
	gdk_draw_line(DrawPixmap, TheGc,
	    x, y + HeiDiv2, x + WidDiv3, y + HeiDiv2);
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3 * 2, y + HeiDiv2, x + Width, y + HeiDiv2);
    }
    /* Drawing || or () or --- */
    switch (Element.Type) {
    case ELE_INPUT:
    case ELE_INPUT_NOT:
    case ELE_RISING_INPUT:
    case ELE_FALLING_INPUT:
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3, y + HeiDiv4, x + WidDiv3, y + Height - HeiDiv4);
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3 * 2, y + HeiDiv4, x + WidDiv3 * 2,
	    y + Height - HeiDiv4);
	break;
    case ELE_CONNECTION:
	gdk_draw_line(DrawPixmap, TheGc,
	    x, y + HeiDiv2, x + Width, y + HeiDiv2);

/*                           gdk_draw_rectangle (DrawPixmap,
                                               drawing_area->style->black_gc,
                                               TRUE,
                                               x, y,
                                               Width,
                                               Height);*/
	break;
    case ELE_OUTPUT:
    case ELE_OUTPUT_NOT:
    case ELE_OUTPUT_SET:
    case ELE_OUTPUT_RESET:
    case ELE_OUTPUT_JUMP:
    case ELE_OUTPUT_CALL:
	/* hide the too much of lines - - before drawing arcs */
	gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
	    x + WidDiv4, y + HeiDiv2 - 1, WidDiv2, 3);
	/* draw the 2 arcs of the outputs */
	gdk_draw_arc(DrawPixmap, TheGc, FALSE,
	    x + WidDiv4, y + WidDiv4, WidDiv2, HeiDiv2, (90 + 20) * 64,
	    150 * 64);
	gdk_draw_arc(DrawPixmap, TheGc, FALSE, x + WidDiv4, y + WidDiv4,
	    WidDiv2, HeiDiv2, (270 + 20) * 64, 150 * 64);
	break;
    }
    /* Drawing / */
    switch (Element.Type) {
    case ELE_INPUT_NOT:
    case ELE_OUTPUT_NOT:
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3, y + Height - HeiDiv4, x + WidDiv3 * 2, y + HeiDiv4);
    }
    /* Drawing ^ or \/ */
    switch (Element.Type) {
    case ELE_RISING_INPUT:
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3, y + HeiDiv3 * 2, x + WidDiv4 * 2, y + HeiDiv3);
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv4 * 2, y + HeiDiv3, x + WidDiv3 * 2, y + HeiDiv3 * 2);
	break;
    case ELE_FALLING_INPUT:
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3, y + HeiDiv3, x + WidDiv4 * 2, y + HeiDiv3 * 2);
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv4 * 2, y + HeiDiv3 * 2, x + WidDiv3 * 2, y + HeiDiv3);
	break;
    }
    /* Drawing 'S'et or 'R'eset or 'J'ump or 'C'all for outputs */
    switch (Element.Type) {
    case ELE_OUTPUT_SET:
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, "S", 1);
	break;
    case ELE_OUTPUT_RESET:
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, "R", 1);
	break;
    case ELE_OUTPUT_JUMP:
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, "J", 1);
	break;
    case ELE_OUTPUT_CALL:
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, "C", 1);
	break;
    }
    /* Drawing complex ones : Timer, Monostable, Compar, Operate */
    switch (Element.Type) {
    case ELE_TIMER:
	if (DrawForToolBar)
	    break;
	Timer = &TimerArray[Element.VarNum];
	/* the box */
	gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
	    x + WidDiv3 - Width, y + HeiDiv3,
	    Width + 1 * WidDiv3, Height + 1 * HeiDiv3);
	gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
	    x + WidDiv3 - Width, y + HeiDiv3,
	    Width + 1 * WidDiv3, Height + 1 * HeiDiv3);
	/* input : enable */
	gdk_draw_line(DrawPixmap, (Timer->InputEnable) ? DynaGcOn : DynaGcOff,
	    x - Width, y + HeiDiv2, x - Width + WidDiv3, y + HeiDiv2);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x - Width, y + HeiDiv2 - 1, "I",
	    1);
	/* output : done */
	gdk_draw_line(DrawPixmap, (Timer->OutputDone) ? DynaGcOn : DynaGcOff,
	    x + WidDiv3 * 2, y + HeiDiv2, x + Width, y + HeiDiv2);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + Width - WidDiv4,
	    y + HeiDiv2 - 1, "D", 1);
	/* output : running */
	gdk_draw_line(DrawPixmap,
	    (Timer->OutputRunning) ? DynaGcOn : DynaGcOff, x + WidDiv3 * 2,
	    y + HeiDiv2 + Height, x + Width, y + HeiDiv2 + Height);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + Width - WidDiv4,
	    y + HeiDiv2 - 1 + Height, "R", 1);
	/* Timer Number */
	sprintf(BufTxt, "T%d", Element.VarNum);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + WidDiv2 - Width,
	    y + HeiDiv4 - 2, BufTxt, strlen(BufTxt));
	/* Current Value */
	if (!EditDatas.ModeEdit) {
	    sprintf(BufTxt, Timer->DisplayFormat,
		(float) Timer->Value / (float) Timer->Base);
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		drawing_area->style->black_gc, x + WidDiv2 - 2 - Width,
		y + Height, BufTxt, strlen(BufTxt));
	}
	break;
    case ELE_MONOSTABLE:
	if (DrawForToolBar)
	    break;
	Monostable = &MonostableArray[Element.VarNum];
	/* the box */
	gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
	    x + WidDiv3 - Width, y + HeiDiv3,
	    Width + 1 * WidDiv3, Height + 1 * HeiDiv3);
	gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
	    x + WidDiv3 - Width, y + HeiDiv3,
	    Width + 1 * WidDiv3, Height + 1 * HeiDiv3);
	/* input */
	gdk_draw_line(DrawPixmap, (Monostable->Input) ? DynaGcOn : DynaGcOff,
	    x - Width, y + HeiDiv2, x - Width + WidDiv3, y + HeiDiv2);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x - Width, y + HeiDiv2 - 1, "I^",
	    2);
	/* output : running */
	gdk_draw_line(DrawPixmap,
	    (Monostable->OutputRunning) ? DynaGcOn : DynaGcOff,
	    x + WidDiv3 * 2, y + HeiDiv2, x + Width, y + HeiDiv2);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + Width - WidDiv4,
	    y + HeiDiv2 - 1, "R", 1);
	/* Monostable Number */
	sprintf(BufTxt, "M%d", Element.VarNum);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + WidDiv2 - Width,
	    y + HeiDiv4 - 2, BufTxt, strlen(BufTxt));
	/* Current Value */
	if (!EditDatas.ModeEdit) {
	    sprintf(BufTxt, Monostable->DisplayFormat,
		(float) Monostable->Value / (float) Monostable->Base);
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		drawing_area->style->black_gc, x + WidDiv2 - 2 - Width,
		y + Height, BufTxt, strlen(BufTxt));
	}
	break;
    case ELE_COMPAR:
	if (DrawForToolBar)
	    break;
	/* the box */
	gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
	    x + WidDiv3 - (Width * 2), y + HeiDiv4,
	    Width * 2 + 1 * WidDiv3, 2 * HeiDiv4);
	gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
	    x + WidDiv3 - (Width * 2), y + HeiDiv4,
	    Width * 2 + 1 * WidDiv3, 2 * HeiDiv4);
	/* input */
	gdk_draw_line(DrawPixmap, TheGc,
	    x - Width * 2, y + HeiDiv2, x - Width * 2 + WidDiv3, y + HeiDiv2);
	/* output */
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3 * 2, y + HeiDiv2, x + Width, y + HeiDiv2);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + WidDiv4 - (Width * 2) + 3,
	    y + HeiDiv4 - 1, "COMPARE", strlen("COMPARE"));
	/* arithmetic expression */
	if (!EditDatas.ModeEdit)
	    strcpy(BufTxt, DisplayArithmExpr(ArithmExpr[Element.VarNum].Expr,
		    (Width * 2 + 1 * WidDiv3) / 8));
	else
	    strcpy(BufTxt,
		DisplayArithmExpr(EditArithmExpr[Element.VarNum].Expr,
		    (Width * 2 + 1 * WidDiv3) / 8));
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + WidDiv3 - (Width * 2) + 2,
	    y + HeiDiv2 + 4, BufTxt, strlen(BufTxt));
	break;
    case ELE_OUTPUT_OPERATE:
	if (DrawForToolBar)
	    break;
	/* the box */
	gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
	    x + WidDiv3 - (Width * 2), y + HeiDiv4,
	    Width * 2 + 1 * WidDiv3, 2 * HeiDiv4);
	gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
	    x + WidDiv3 - (Width * 2), y + HeiDiv4,
	    Width * 2 + 1 * WidDiv3, 2 * HeiDiv4);
	/* input */
	gdk_draw_line(DrawPixmap, TheGc,
	    x - Width * 2, y + HeiDiv2, x - Width * 2 + WidDiv3, y + HeiDiv2);
	/* output */
	gdk_draw_line(DrawPixmap, TheGc,
	    x + WidDiv3 * 2, y + HeiDiv2, x + Width, y + HeiDiv2);
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + WidDiv4 - (Width * 2) + 3,
	    y + HeiDiv4 - 1, "OPERATE", strlen("OPERATE"));
	/* arithmetic expression */
	if (!EditDatas.ModeEdit)
	    strcpy(BufTxt, DisplayArithmExpr(ArithmExpr[Element.VarNum].Expr,
		    (Width * 2 + 1 * WidDiv3) / 8));
	else
	    strcpy(BufTxt,
		DisplayArithmExpr(EditArithmExpr[Element.VarNum].Expr,
		    (Width * 2 + 1 * WidDiv3) / 8));
	gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
	    drawing_area->style->black_gc, x + WidDiv3 - (Width * 2) + 2,
	    y + HeiDiv2 + 4, BufTxt, strlen(BufTxt));
	break;
    }
    /* Drawing Var */
    if (!DrawForToolBar) {
	switch (Element.Type) {
	case ELE_INPUT:
	case ELE_INPUT_NOT:
	case ELE_RISING_INPUT:
	case ELE_FALLING_INPUT:
	case ELE_OUTPUT:
	case ELE_OUTPUT_NOT:
	case ELE_OUTPUT_SET:
	case ELE_OUTPUT_RESET:
	    strcpy(BufTxt, DisplayInfo(Element.VarType, Element.VarNum));
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		drawing_area->style->black_gc, x + WidDiv4, y + HeiDiv4 - 2,
		BufTxt, strlen(BufTxt));
	    break;
	case ELE_OUTPUT_JUMP:
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		drawing_area->style->black_gc, x + 1, y + HeiDiv4 - 2,
		RungArray[Element.VarNum].Label,
		strlen(RungArray[Element.VarNum].Label));
	    break;
	case ELE_OUTPUT_CALL:
	    sprintf(BufTxt, "SR%d", Element.VarNum);
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		drawing_area->style->black_gc, x + 1, y + HeiDiv4 - 2, BufTxt,
		strlen(BufTxt));
	    break;
	}
    }
    /* Drawing cnx with top */
    if (Element.ConnectedWithTop) {
	if (Element.DynamicInput)
	    gdk_draw_line(DrawPixmap, DynaGcOn,
		x, y + HeiDiv2 + 1, x, y - HeiDiv2);
	else
	    gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
		x, y + HeiDiv2, x, y - HeiDiv2);
    }

    /* specials used for Editor */
    if (DrawForToolBar) {
	switch (Element.Type) {
	case EDIT_POINTER:
	    gdk_draw_line(DrawPixmap, drawing_area->style->black_gc, x + WidDiv4, y + HeiDiv4, x + Width - WidDiv4, y + Height - HeiDiv4);	/* \ 
																		 */
	    gdk_draw_line(DrawPixmap, drawing_area->style->black_gc, x + WidDiv4, y + HeiDiv4, x + WidDiv3, y + HeiDiv2);	/* | 
																 */
	    gdk_draw_line(DrawPixmap, drawing_area->style->black_gc, x + WidDiv4, y + HeiDiv4, x + WidDiv2, y + HeiDiv3);	/* _ 
																 */
	    gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
		x + WidDiv3, y + HeiDiv2, x + WidDiv2, y + HeiDiv3);
	    break;
	case EDIT_CNX_WITH_TOP:
	    gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
		x + WidDiv2, y + HeiDiv4, x + WidDiv2, y + Height - HeiDiv4);
	    break;
	case EDIT_LONG_CONNECTION:
	    gdk_draw_line(DrawPixmap, TheGc,
		x, y + HeiDiv2, x + Width - 1, y + HeiDiv2);
	    gdk_draw_line(DrawPixmap, TheGc,
		x + 3 * WidDiv4 - 1, y + HeiDiv4, x + Width - 1, y + HeiDiv2);
	    gdk_draw_line(DrawPixmap, TheGc,
		x + 3 * WidDiv4 - 1, y + 3 * HeiDiv4, x + Width - 1,
		y + HeiDiv2);
	    gdk_draw_line(DrawPixmap, TheGc, x + 3 * WidDiv4 - 1, y + HeiDiv4,
		x + 3 * WidDiv4 + 3, y + HeiDiv2);
	    gdk_draw_line(DrawPixmap, TheGc, x + 3 * WidDiv4 + 3, y + HeiDiv2,
		x + 3 * WidDiv4 - 1, y + 3 * HeiDiv4);
	    break;
	    /* little display used for the toolbar */
	case ELE_TIMER:
	    gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc,
		FALSE, x + WidDiv4, y + HeiDiv4, Width - 2 * WidDiv4,
		Height - 2 * HeiDiv4);
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, "T", 1);
	    break;
	case ELE_MONOSTABLE:
	    gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc,
		FALSE, x + WidDiv4, y + HeiDiv4, Width - 2 * WidDiv4,
		Height - 2 * HeiDiv4);
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, "M", 1);
	    break;
	case ELE_COMPAR:
	    gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc,
		FALSE, x + WidDiv4, y + HeiDiv4, Width - 2 * WidDiv4,
		Height - 2 * HeiDiv4);
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, ">", 1);
	    break;
	case ELE_OUTPUT_OPERATE:
	    gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc,
		FALSE, x + WidDiv4, y + HeiDiv4, Width - 2 * WidDiv4,
		Height - 2 * HeiDiv4);
	    gdk_draw_text(DrawPixmap, gtk_style_get_font(drawing_area->style),
		TheGc, x + WidDiv3 + 2, y + HeiDiv3 * 2, "=", 1);
	    break;
	}
    }
    if (!EditDatas.ModeEdit)
	gdk_gc_unref(DynaGcOn);
}

void DrawBars(int PosiY, int IsTheCurrentRung)
{
    GdkColor DynaGdkColor;
    GdkGC *DynaGcBarsColor;
    DynaGdkColor.pixel = 0xEE5555;
    DynaGdkColor.red = 0xEE;
    DynaGdkColor.green = 0x55;
    DynaGdkColor.blue = 0x55;
    if (!IsTheCurrentRung) {
	DynaGcBarsColor = drawing_area->style->black_gc;
    } else {
	DynaGcBarsColor = gdk_gc_new(pixmap);
	gdk_gc_set_foreground(DynaGcBarsColor, &DynaGdkColor);
    }
    gdk_draw_rectangle(pixmap, DynaGcBarsColor, TRUE,
	1, 1 + PosiY, 3, InfosGene->BlockHeight * RUNG_HEIGHT);
    gdk_draw_rectangle(pixmap, DynaGcBarsColor, TRUE,
	InfosGene->BlockWidth * RUNG_WIDTH + OFFSET_X, 1 + PosiY,
	3, InfosGene->BlockHeight * RUNG_HEIGHT);
    if (IsTheCurrentRung)
	gdk_gc_unref(DynaGcBarsColor);
}

void DrawGrid(int PosiY)
{
    int x, y;
    GdkColor DynaGdkColor;
    GdkGC *DynaGcOn;
    DynaGdkColor.pixel = 0xF4F4F4;
    DynaGdkColor.red = 0xF4;
    DynaGdkColor.green = 0xF4;
    DynaGdkColor.blue = 0xF4;

    DynaGcOn = gdk_gc_new(pixmap);
    gdk_gc_set_foreground(DynaGcOn, &DynaGdkColor);
    for (x = InfosGene->BlockWidth; x < RUNG_WIDTH * InfosGene->BlockWidth;
	x = x + InfosGene->BlockWidth) {
	gdk_draw_line(pixmap, DynaGcOn, x + OFFSET_X, OFFSET_Y + PosiY,
	    x + OFFSET_X, RUNG_HEIGHT * InfosGene->BlockHeight + PosiY);
    }
    for (y = InfosGene->BlockHeight; y < RUNG_HEIGHT * InfosGene->BlockHeight;
	y = y + InfosGene->BlockHeight) {
	gdk_draw_line(pixmap, DynaGcOn, OFFSET_X, y + OFFSET_Y + PosiY,
	    RUNG_WIDTH * InfosGene->BlockWidth + OFFSET_X,
	    y + OFFSET_Y + PosiY);
    }
    gdk_gc_unref(DynaGcOn);
}

void DrawRungPartition(int PosiY)
{
    GdkColor DynaGdkColor;
    GdkGC *DynaGcOn;
    DynaGdkColor.pixel = 0xCCCCCC;
    DynaGdkColor.red = 0xCC;
    DynaGdkColor.green = 0xCC;
    DynaGdkColor.blue = 0xCC;

    DynaGcOn = gdk_gc_new(pixmap);
    gdk_gc_set_foreground(DynaGcOn, &DynaGdkColor);
    gdk_draw_line(pixmap, DynaGcOn,
	OFFSET_X, OFFSET_Y + PosiY,
	RUNG_WIDTH * InfosGene->BlockWidth + OFFSET_X, OFFSET_Y + PosiY);
    gdk_gc_unref(DynaGcOn);
}

void GetTheSizesForRung()
{
    static int PageHeightBak = 0;
    static int BlockHeightBak = 0;
    InfosGene->BlockWidth = InfosGene->BlockHeight =
	(GTK_WIDGET(drawing_area)->allocation.width - OFFSET_X -
	5) / RUNG_WIDTH;
    InfosGene->PageHeight =
	GTK_WIDGET(drawing_area)->allocation.height - OFFSET_Y;
    // used for sequential
    InfosGene->PageWidth =
	GTK_WIDGET(drawing_area)->allocation.width - OFFSET_X;

    // size of the page or block changed ?
    if (InfosGene->PageHeight != PageHeightBak
	|| InfosGene->BlockHeight != BlockHeightBak)
	UpdateVScrollBar();
    PageHeightBak = InfosGene->PageHeight;
    BlockHeightBak = InfosGene->BlockHeight;
}

void DrawRung(StrRung * Rung, int PosiY)
{
    int x, y;

    for (y = 0; y < RUNG_HEIGHT; y++) {
	for (x = 0; x < RUNG_WIDTH; x++) {
	    DrawElement(pixmap, x * InfosGene->BlockWidth + OFFSET_X,
		y * InfosGene->BlockHeight + OFFSET_Y + PosiY,
		InfosGene->BlockWidth, InfosGene->BlockHeight,
		Rung->Element[x][y], FALSE);
	}
    }
}

void DrawRungs()
{
    int ScanRung = InfosGene->TopRungDisplayed;
    int ScanY = InfosGene->OffsetHiddenTopRungDisplayed;
    StrRung *PtrRung;
    int TheEnd = FALSE;

    // Clean all
    gdk_draw_rectangle(pixmap, drawing_area->style->white_gc, TRUE,
	0, 0, InfosGene->BlockWidth * RUNG_WIDTH + 50,
	InfosGene->PageHeight + 50);

    for (ScanY = -InfosGene->OffsetHiddenTopRungDisplayed;
	(ScanY < InfosGene->PageHeight) && !TheEnd;
	ScanY += (InfosGene->BlockHeight * RUNG_HEIGHT)) {
	PtrRung = &RungArray[ScanRung];

	/* displaying the current rung - in edit ? */
	if ((ScanRung == InfosGene->CurrentRung) && EditDatas.ModeEdit) {
	    /* grid for edit mode and display the rung under edition */
	    DrawGrid(ScanY);
	    PtrRung = &EditDatas.Rung;
	}

	DrawBars(ScanY, ScanRung == InfosGene->CurrentRung);
	DrawRung(PtrRung, ScanY);
	DrawRungPartition(ScanY + InfosGene->BlockHeight * RUNG_HEIGHT - 5);

	if (ScanRung != InfosGene->LastRung)
	    ScanRung = RungArray[ScanRung].NextRung;
	else
	    TheEnd = TRUE;
    }
}

void DrawCurrentSection(void)
{
    GdkRectangle update_rect;
    int iCurrentLanguage = SectionArray[InfosGene->CurrentSection].Language;
    GetTheSizesForRung();
    if (iCurrentLanguage == SECTION_IN_LADDER)
	DrawRungs();
#ifdef SEQUENTIAL_SUPPORT
    if (iCurrentLanguage == SECTION_IN_SEQUENTIAL)
	DrawSequentialPage(SectionArray[InfosGene->CurrentSection].
	    SequentialPage);
#endif
    update_rect.x = 0;
    update_rect.y = 0;
    update_rect.width = GTK_WIDGET(drawing_area)->allocation.width;
    update_rect.height = GTK_WIDGET(drawing_area)->allocation.height;
    gtk_widget_draw(drawing_area, &update_rect);
}
