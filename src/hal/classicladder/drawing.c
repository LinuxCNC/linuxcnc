/* Classic Ladder Project */
/* Copyright (C) 2001-2008 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
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
#include <stdlib.h>
#include "classicladder.h"
#include "global.h"
#include "arithm_eval.h"
#include "classicladder_gtk.h"
#include "edit.h"
#ifdef SEQUENTIAL_SUPPORT
#include "drawing_sequential.h"
#endif
#include "symbols.h"
#include "vars_names.h"
#include "drawing.h"

#ifdef GTK2
#include <pango/pango.h>
#endif

char * DisplayArithmExpr(char * Expr,int NumCarMax)
{
	static char Buffer[ARITHM_EXPR_SIZE+30];
	char * Ptr = Expr;
	int Fill = 0;
	Buffer[0] = '\0';
	/* null expression ? */
	if (Expr[0]=='\0')
		return Buffer;
	do
	{
		/* start of a variable ? */
		if (*Ptr=='@')
		{
			int NumVar,TypeVar;
			int IndexNumVar,IndexTypeVar;
			char VarBuffer[20];
			char VarIndexBuffer[20];
			if ( IdentifyVarIndexedOrNot( Ptr, &TypeVar, &NumVar, &IndexTypeVar, &IndexNumVar ) )
			{
				if ( IndexTypeVar!=-1 && IndexNumVar!=-1 )
				{
					// buffer for index required as CreateVarName() returns on a static buffer !
					strcpy( VarIndexBuffer, CreateVarName(IndexTypeVar,IndexNumVar) );
					sprintf(VarBuffer, "%s[%s]", CreateVarName(TypeVar,NumVar), VarIndexBuffer );
				}
				else
				{
					strcpy(VarBuffer,CreateVarName(TypeVar,NumVar));
				}
			}
			else
				strcpy(VarBuffer,"??");
			strcpy(&Buffer[Fill],VarBuffer);
			/* flush until end of a variable */
			do
			{
				Ptr++;
			}
			while(*Ptr!='@');
			Ptr++;
			Fill = Fill+strlen(VarBuffer);
		}
		else
		{
			Buffer[Fill++] = *Ptr++;
		}
	}
	while(*Ptr!='\0');
	Buffer[Fill] = '\0';
	/* size limited ? */
	if (NumCarMax>0)
	{
		if (strlen(Buffer)>NumCarMax)
		{
			Buffer[NumCarMax-1] = '.';
			Buffer[NumCarMax] = '.';
			Buffer[NumCarMax+1] = '\0';
		}
	}
	return Buffer;
}

#ifdef GTK2
/* Drawing text centered with GTK2. */
/* if Height is -1, then drawing on top of BaseY given... */
void DrawTextWithOffsetGTK2( GdkPixmap * DrawPixmap, GdkGC * GcRef, int BaseX, int BaseY, int Width, int Height, char * Text, int BorderOffset ) 
{
	int SizeX, SizeY, OffsetX,PosiY;
	PangoFontDescription *FontDesc;
	PangoLayout * playout = gtk_widget_create_pango_layout ( GTK_WIDGET(drawing_area), Text );

	pango_layout_set_width( playout, Width*PANGO_SCALE );

//	FontDesc = pango_font_description_from_string( "Andale Mono 8" );
//	FontDesc = pango_font_description_from_string( "Courier New 8" );
//	FontDesc = pango_font_description_from_string( "Lucida Bright 8" );
	FontDesc = pango_font_description_from_string( "Lucida Sans 10" );
	pango_layout_set_font_description( playout, FontDesc );
	pango_font_description_free( FontDesc );

	pango_layout_get_pixel_size( playout, &SizeX, &SizeY );
	OffsetX = 0;
	if ( Width>0 )
	{
		OffsetX = (Width-SizeX -BorderOffset*2)/2;
		if ( OffsetX<=BorderOffset )
			OffsetX = BorderOffset;
	}
	PosiY = BaseY - SizeY; //BaseY is the bottom if Height is -1.
	if ( Height>0 )
	{
		PosiY = BaseY + (Height-SizeY)/2;
		// too much text ?
		if ( SizeY>= Height )
		{
			int NbrLines = pango_layout_get_line_count( playout );
			int HeightOneLine = SizeY/NbrLines;
			int NbrLinesToDisplay = Height/HeightOneLine;
//printf("overflow text display: NbrLines=%d, HeightOneLine=%d, NbrLinesToDisplay=%d\n", NbrLines, HeightOneLine, NbrLinesToDisplay); 
			if ( NbrLinesToDisplay>1 )
			{
				PangoLayoutLine * playout_line = pango_layout_get_line( playout, NbrLinesToDisplay-1 );
				if ( playout_line!=NULL )
				{
					char * BuffTmp = malloc( strlen(Text)+1 );
					if ( BuffTmp )
					{
						char * pScanText;
						strcpy( BuffTmp, Text );
//printf("playout_line start=%d length=%d\n", playout_line->start_index, playout_line->length );
//printf("playout_line string=%s\n", &Text[ playout_line->start_index+playout_line->length ]);
						pScanText = &BuffTmp[ playout_line->start_index+playout_line->length-1 ];
						*pScanText--='\0';
						*pScanText--='.';
						*pScanText--='.';
						*pScanText--='.';
						pango_layout_set_text( playout, BuffTmp, -1 );
						pango_layout_get_pixel_size( playout, &SizeX, &SizeY );
						PosiY = BaseY - SizeY; //BaseY is the bottom if Height is -1.
						if ( Height>0 )
							PosiY = BaseY + (Height-SizeY)/2;
						free( BuffTmp );
					}
				}
			}
		}
	}
	gdk_draw_layout (DrawPixmap, GcRef, BaseX+OffsetX,PosiY, playout);
	g_object_unref (playout);
}
void DrawTextGTK2( GdkPixmap * DrawPixmap, GdkGC * GcRef, int BaseX, int BaseY, int Width, int Height, char * Text ) 
{
	DrawTextWithOffsetGTK2( DrawPixmap, GcRef, BaseX, BaseY, Width, Height, Text, 0/*BorderOffset*/ );
}
#endif

void DrawCommonElementForToolbar(GdkPixmap * DrawPixmap,int x,int y,int Size,int NumElement)
{
	int SizeDiv2 = Size/2;
	int SizeDiv3 = Size/3;
	int SizeDiv4 = Size/4;
	switch(NumElement)
	{
		case EDIT_POINTER:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+SizeDiv4, x+Size-SizeDiv4,y+Size-SizeDiv4); /* \ */
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+SizeDiv4, x+SizeDiv3,y+SizeDiv2);  /* | */
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+SizeDiv4, x+SizeDiv2,y+SizeDiv3);  /* _ */
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv3,y+SizeDiv2, x+SizeDiv2,y+SizeDiv3);
			break;
		case EDIT_ERASER:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv3,y+SizeDiv4, x+Size-SizeDiv4,y+SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+Size-SizeDiv4, x+Size-SizeDiv3,y+Size-SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv3,y+SizeDiv4, x+SizeDiv4,y+Size-SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+Size-SizeDiv4,y+SizeDiv4, x+Size-SizeDiv3,y+Size-SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4 +2,y+Size-SizeDiv4 +2, x+Size-SizeDiv3 +2,y+Size-SizeDiv4 +2);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+Size-SizeDiv4 +2,y+SizeDiv4 +2, x+Size-SizeDiv3 +2,y+Size-SizeDiv4 +2);
			break;
	}
}

void DrawElement(GdkPixmap * DrawPixmap,int x,int y,int Width,int Height,StrElement Element,char DrawingOption)
{
	char BufTxt[50];
#ifdef OLD_TIMERS_MONOS_SUPPORT
	StrTimer * Timer;
	StrMonostable * Monostable;
#endif
	StrCounter * Counter;
	StrTimerIEC * TimerIEC;
	int WidDiv2 = Width/2;
	int WidDiv3 = Width/3;
	int WidDiv4 = Width/4;
	int HeiDiv2 = Height/2;
	int HeiDiv3 = Height/3;
	int HeiDiv4 = Height/4;
	GdkGC * DynaGcOff;
	GdkGC * TheGc;
	
	GdkColor DynaGdkColor;
        GdkColor DynaGdkColor1;
  	GdkColor DynaGdkColor2;
        GdkColor DynaGdkColor3;
	GdkGC * DynaGcOn;

// This is black:
 	DynaGdkColor3.pixel = 0x000000;
	DynaGdkColor3.red = 0x00;
	DynaGdkColor3.green = 0x00;
	DynaGdkColor3.blue = 0x00;

// Red colour for text of input objects
// to make them standout better
        DynaGdkColor1.pixel = 0xFF0000;
	DynaGdkColor1.red = 0xFF;
	DynaGdkColor1.green = 0x00;
	DynaGdkColor1.blue = 0x00;

// Blue colour for text of output objects	
        DynaGdkColor2.pixel = 0x0000FF;
	DynaGdkColor2.red = 0x00;
	DynaGdkColor2.green = 0x00;
	DynaGdkColor2.blue = 0xFF;

// Pink for 'power on' lines 	
	DynaGdkColor.pixel = 0xFF22FF;
	DynaGdkColor.red = 0xFF;
	DynaGdkColor.green = 0x22;
	DynaGdkColor.blue = 0xFF;
	
	DynaGcOn = gdk_gc_new(DrawPixmap);

	gdk_gc_set_foreground(DynaGcOn,&DynaGdkColor);
	#ifdef THICK_LINE_ELE_ACTIVATED
	gdk_gc_set_line_attributes(DynaGcOn, THICK_LINE_ELE_ACTIVATED,
		GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
	#endif

// for 'power off' lines
	DynaGcOff = drawing_area->style->black_gc;

	/* State with color */
	TheGc = drawing_area->style->black_gc;
	if ( (DrawingOption==DRAW_NORMAL) && (!EditDatas.ModeEdit) && (Element.DynamicState) )
		TheGc = DynaGcOn;
	if (EditDatas.ModeEdit || DrawingOption==DRAW_FOR_PRINT)
	{
		gdk_gc_unref(DynaGcOn);
		DynaGcOn = drawing_area->style->black_gc;
	}
	/* Drawing - - */
	switch(Element.Type)
	{
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
								x,y+HeiDiv2, x+WidDiv3,y+HeiDiv2);
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3*2,y+HeiDiv2, x+Width,y+HeiDiv2);
	}
	/* Drawing || or () or --- */
	switch(Element.Type)
	{
		case ELE_INPUT:
		case ELE_INPUT_NOT:
		case ELE_RISING_INPUT:
		case ELE_FALLING_INPUT:
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3,y+HeiDiv4, x+WidDiv3,y+Height-HeiDiv4);
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3*2,y+HeiDiv4, x+WidDiv3*2,y+Height-HeiDiv4);
				break;
		case ELE_CONNECTION:
				gdk_draw_line(DrawPixmap, TheGc,
								x,y+HeiDiv2, x+Width,y+HeiDiv2);


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
				gdk_draw_rectangle(DrawPixmap, (DrawingOption!=DRAW_FOR_TOOLBAR)?drawing_area->style->white_gc:drawing_area->style->bg_gc[0], TRUE,
								x+WidDiv4,y+HeiDiv2-1, WidDiv2,3);
				/* draw the 2 arcs of the outputs */
				gdk_draw_arc (DrawPixmap, TheGc, FALSE,
								x+WidDiv4, y+HeiDiv4, WidDiv2, HeiDiv2, (90+20)*64, 150*64);
				gdk_draw_arc (DrawPixmap, TheGc, FALSE,
								x+WidDiv4, y+HeiDiv4, WidDiv2, HeiDiv2, (270+20)*64, 150*64);
						break;
	}
	/* Drawing / */
	switch(Element.Type)
	{
		case ELE_INPUT_NOT:
		case ELE_OUTPUT_NOT:
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3,y+Height-HeiDiv4, x+WidDiv3*2,y+HeiDiv4);
	}
	/* Drawing ^ or \/  */
	switch(Element.Type)
	{
		case ELE_RISING_INPUT:
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3,y+HeiDiv3*2, x+WidDiv4*2,y+HeiDiv3);
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv4*2,y+HeiDiv3, x+WidDiv3*2,y+HeiDiv3*2);
				break;
		case ELE_FALLING_INPUT:
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3,y+HeiDiv3, x+WidDiv4*2,y+HeiDiv3*2);
				gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv4*2,y+HeiDiv3*2, x+WidDiv3*2,y+HeiDiv3);
				break;
	}

	/* Drawing 'S'et or 'R'eset or 'J'ump or 'C'all for outputs */
	switch(Element.Type)
	{
		case ELE_OUTPUT_SET:
#ifndef GTK2
		gdk_draw_text(DrawPixmap, drawing_area->style->font, TheGc,
						x+WidDiv3+2,y+HeiDiv3*2,"S",1);
#else
			DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, "S" );
#endif
			break;
		case ELE_OUTPUT_RESET:
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, TheGc,
						x+WidDiv3+2,y+HeiDiv3*2,"R",1);
#else
			DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, "R" );
#endif
			break;
		case ELE_OUTPUT_JUMP:
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, TheGc,
						x+WidDiv3+2,y+HeiDiv3*2,"J",1);
#else
			DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, "J" );
#endif
			break;
		case ELE_OUTPUT_CALL:
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, TheGc,
						x+WidDiv3+2,y+HeiDiv3*2,"C",1);
#else
			DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, "C" );
#endif
			break;
	}

	/* Drawing complex ones : Timer, Monostable, Compar, Operate */
	switch(Element.Type)
	{
#ifdef OLD_TIMERS_MONOS_SUPPORT
		case ELE_TIMER:
			if (DrawingOption==DRAW_FOR_TOOLBAR)
				break;
			Timer = &TimerArray[Element.VarNum];
			/* the box */
			gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, Height+1*HeiDiv3);
			gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, Height+1*HeiDiv3);
			/* input : enable */
			gdk_draw_line(DrawPixmap, (Timer->InputEnable)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2, x-Width+WidDiv3,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width+2,y+HeiDiv2-1,"E",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width+2, y+HeiDiv2-1, -1, -1, "E" );
#endif
			/* input : control */
			gdk_draw_line(DrawPixmap, (Timer->InputControl)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2+Height, x-Width+WidDiv3,y+HeiDiv2+Height);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width+2,y+HeiDiv2-1+Height,"C",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width+2, y+HeiDiv2-1+Height, -1, -1, "C" );
#endif
			/* output : done */
			gdk_draw_line(DrawPixmap, (Timer->OutputDone)?DynaGcOn:DynaGcOff,
								x+WidDiv3*2,y+HeiDiv2, x+Width,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+Width-WidDiv4,y+HeiDiv2-1,"D",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+Width-WidDiv4,y+HeiDiv2-1, -1, -1, "D" );
#endif
			/* output : running */
			gdk_draw_line(DrawPixmap, (Timer->OutputRunning)?DynaGcOn:DynaGcOff,
								x+WidDiv3*2,y+HeiDiv2+Height, x+Width,y+HeiDiv2+Height);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+Width-WidDiv4,y+HeiDiv2-1+Height,"R",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+Width-WidDiv4,y+HeiDiv2-1+Height, -1, -1, "R" );
#endif
			/* Timer Number */
			sprintf( BufTxt,"%cT%d", '%', Element.VarNum );
			if ( InfosGene->DisplaySymbols )
			{
				StrSymbol * SymbolName = ConvVarNameInSymbolPtr( BufTxt );
				if ( SymbolName )
					strcpy( BufTxt, SymbolName->Symbol );
			}
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv2-Width,y+HeiDiv4-2,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv3-Width,y+HeiDiv4+2, (Width-WidDiv3)*2, -1, BufTxt );
#endif
			/* Current Value (or Preset if print/edit) */
			if ( DrawingOption!=DRAW_FOR_PRINT && !EditDatas.ModeEdit )
				sprintf(BufTxt,Timer->DisplayFormat,(double)Timer->Value/(double)Timer->Base);
			else
				sprintf(BufTxt,Timer->DisplayFormat,(double)Timer->Preset/(double)Timer->Base);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
							x+WidDiv2-2-Width,y+Height,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y, Width*2, Height*2, BufTxt );
#endif
			break;

		case ELE_MONOSTABLE:
			if (DrawingOption==DRAW_FOR_TOOLBAR)
				break;
			Monostable = &MonostableArray[Element.VarNum];
			/* the box */
			gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, Height+1*HeiDiv3);
			gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, Height+1*HeiDiv3);
			/* input */
			gdk_draw_line(DrawPixmap, (Monostable->Input)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2, x-Width+WidDiv3,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width,y+HeiDiv2-1,"I^",2);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width,y+HeiDiv2-1, -1, -1, "I^" );
#endif
			/* output : running */
			gdk_draw_line(DrawPixmap, (Monostable->OutputRunning)?DynaGcOn:DynaGcOff,
								x+WidDiv3*2,y+HeiDiv2, x+Width,y+HeiDiv2);

#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+Width-WidDiv4,y+HeiDiv2-1,"R",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+Width-WidDiv4,y+HeiDiv2-1, -1, -1, "R" );
#endif
			/* Monostable Number */
			sprintf( BufTxt,"%cM%d", '%', Element.VarNum );
			if ( InfosGene->DisplaySymbols )
			{
				StrSymbol * SymbolName = ConvVarNameInSymbolPtr( BufTxt );
				if ( SymbolName )
					strcpy( BufTxt, SymbolName->Symbol );
			}
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv2-Width,y+HeiDiv4-2,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv3-Width,y+HeiDiv4+2, (Width-WidDiv3)*2, -1, BufTxt );
#endif
			/* Current Value (or Preset if print/edit) */
			if ( DrawingOption!=DRAW_FOR_PRINT && !EditDatas.ModeEdit )
				sprintf(BufTxt,Monostable->DisplayFormat,(double)Monostable->Value/(double)Monostable->Base);
			else
				sprintf(BufTxt,Monostable->DisplayFormat,(double)Monostable->Preset/(double)Monostable->Base);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
							x+WidDiv2-2-Width,y+Height,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y, Width*2, Height*2, BufTxt );
#endif
			break;
#endif

		case ELE_COUNTER:
			if (DrawingOption==DRAW_FOR_TOOLBAR)
				break;
			Counter = &CounterArray[Element.VarNum];
			/* the box */
			gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, 3*Height+1*HeiDiv3);
			gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, 3*Height+1*HeiDiv3);
			/* input : reset */
			gdk_draw_line(DrawPixmap, (Counter->InputReset)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2, x-Width+WidDiv3,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width,y+HeiDiv2-1,"R",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y+HeiDiv2-1, -1, -1, "R" );
#endif
			/* input : preset */
			gdk_draw_line(DrawPixmap, (Counter->InputPreset)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2+Height, x-Width+WidDiv3,y+HeiDiv2+Height);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width,y+HeiDiv2-1+Height,"P",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y+HeiDiv2-1+Height, -1, -1, "P" );
#endif
			/* input : count up */
			gdk_draw_line(DrawPixmap, (Counter->InputCountUp)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2+Height*2, x-Width+WidDiv3,y+HeiDiv2+Height*2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width,y+HeiDiv2-1+Height*2,"U",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y+HeiDiv2-1+Height*2, -1, -1, "U" );
#endif
			/* input : count down */
			gdk_draw_line(DrawPixmap, (Counter->InputCountDown)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2+Height*3, x-Width+WidDiv3,y+HeiDiv2+Height*3);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width,y+HeiDiv2-1+Height*3,"D",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y+HeiDiv2-1+Height*3, -1, -1, "D" );
#endif
			/* output : empty */
			gdk_draw_line(DrawPixmap, (Counter->OutputEmpty)?DynaGcOn:DynaGcOff,
								x+WidDiv3*2,y+HeiDiv2, x+Width,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+Width-WidDiv4,y+HeiDiv2-1,"E",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+Width-WidDiv4,y+HeiDiv2-1, -1, -1, "E" );
#endif
			/* output : done */
			gdk_draw_line(DrawPixmap, (Counter->OutputDone)?DynaGcOn:DynaGcOff,
								x+WidDiv3*2,y+HeiDiv2+Height, x+Width,y+HeiDiv2+Height);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+Width-WidDiv4,y+HeiDiv2-1+Height,"D",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+Width-WidDiv4,y+HeiDiv2-1+Height, -1, -1, "D" );
#endif
			/* output : full */
			gdk_draw_line(DrawPixmap, (Counter->OutputFull)?DynaGcOn:DynaGcOff,
								x+WidDiv3*2,y+HeiDiv2+Height*2, x+Width,y+HeiDiv2+Height*2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+Width-WidDiv4,y+HeiDiv2-1+Height*2,"F",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+Width-WidDiv4,y+HeiDiv2-1+Height*2, -1, -1, "F" );
#endif
			/* Counter Number */
			sprintf( BufTxt,"%cC%d", '%', Element.VarNum );
			if ( InfosGene->DisplaySymbols )
			{
				StrSymbol * SymbolName = ConvVarNameInSymbolPtr( BufTxt );
				if ( SymbolName )
					strcpy( BufTxt, SymbolName->Symbol );
			}
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv2-Width,y+HeiDiv4-2,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv3-Width,y+HeiDiv4+2, (Width-WidDiv3)*2, -1, BufTxt );
#endif
			/* Current Value (or Preset if print/edit) */
			if ( DrawingOption!=DRAW_FOR_PRINT && !EditDatas.ModeEdit )
				sprintf(BufTxt,"%d",Counter->Value);
			else
				sprintf(BufTxt,"%d",Counter->Preset);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
							x+WidDiv2-2-Width,y+Height,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y, Width*2, Height*2, BufTxt );
#endif
			break;

		case ELE_TIMER_IEC:
			if (DrawingOption==DRAW_FOR_TOOLBAR)
				break;
			TimerIEC = &NewTimerArray[Element.VarNum];
			/* the box */
			gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, Height+1*HeiDiv3);
			gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
								x+WidDiv3-Width, y+HeiDiv3,
								Width+1*WidDiv3, Height+1*HeiDiv3);
			/* input : enable */
			gdk_draw_line(DrawPixmap, (TimerIEC->Input)?DynaGcOn:DynaGcOff,
								x-Width,y+HeiDiv2, x-Width+WidDiv3,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x-Width,y+HeiDiv2-1,"I",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y+HeiDiv2-1, -1, -1, "I" );
#endif
			/* output : done */
			gdk_draw_line(DrawPixmap, (TimerIEC->Output)?DynaGcOn:DynaGcOff,
								x+WidDiv3*2,y+HeiDiv2, x+Width,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+Width-WidDiv4,y+HeiDiv2-1,"Q",1);
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+Width-WidDiv4,y+HeiDiv2-1, -1, -1, "Q" );
#endif
			/* Timer IEC Number */
			sprintf( BufTxt,"%cTM%d", '%', Element.VarNum );
			if ( InfosGene->DisplaySymbols )
			{
				StrSymbol * SymbolName = ConvVarNameInSymbolPtr( BufTxt );
				if ( SymbolName )
					strcpy( BufTxt, SymbolName->Symbol );
			}
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv2-Width,y+HeiDiv4-2,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv3-Width,y+HeiDiv4+2, (Width-WidDiv3)*2, -1, BufTxt );
#endif
			/* Timer mode */
			sprintf( BufTxt, "%s", TimersModesStrings[ (int)TimerIEC->TimerMode ] );
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
							x+WidDiv2-2-Width,y+Height,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y+HeiDiv3, Width*2, Height-HeiDiv3, BufTxt );
#endif

			/* Current Value (or Preset if print/edit) */
			if ( DrawingOption!=DRAW_FOR_PRINT && !EditDatas.ModeEdit )
				sprintf(BufTxt,/*TimerIEC->DisplayFormat*/"%d", TimerIEC->Value);
			else
				sprintf(BufTxt,/*TimerIEC->DisplayFormat*/"%d", TimerIEC->Preset);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
							x+WidDiv2-2-Width,y+Height,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x-Width, y+Height+HeiDiv2, Width*2, -1/*Height-HeiDiv3*/, BufTxt );
#endif
			break;

		case ELE_COMPAR:
			if (DrawingOption==DRAW_FOR_TOOLBAR)
				break;
			/* the box */
			gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
								x+WidDiv3-(Width*2), y+HeiDiv4,
								Width*2+1*WidDiv3, 2*HeiDiv4);
			gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
								x+WidDiv3-(Width*2), y+HeiDiv4,
								Width*2+1*WidDiv3, 2*HeiDiv4);
			/* input */
			gdk_draw_line(DrawPixmap, TheGc,
								x-Width*2,y+HeiDiv2, x-Width*2+WidDiv3,y+HeiDiv2);
			/* output */
			gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3*2,y+HeiDiv2, x+Width,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv4-(Width*2)+3,y+HeiDiv4-1,"COMPARISON",strlen("COMPARISON"));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv4-(Width*2)+3, y+HeiDiv4+1, -1, -1, "COMPARISON" );
#endif
			/* arithmetic expression */
			if (!EditDatas.ModeEdit)
				strcpy(BufTxt,DisplayArithmExpr(ArithmExpr[Element.VarNum].Expr,(Width*2+1*WidDiv3)/8));
			else
				strcpy(BufTxt,DisplayArithmExpr(EditArithmExpr[Element.VarNum].Expr,(Width*2+1*WidDiv3)/8));
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv3-(Width*2)+2,y+HeiDiv2+4,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv3-(Width*2)+2,y, -1, Height, BufTxt );
#endif
			break;
		case ELE_OUTPUT_OPERATE:
			if (DrawingOption==DRAW_FOR_TOOLBAR)
				break;
			/* the box */
			gdk_draw_rectangle(DrawPixmap, drawing_area->style->white_gc, TRUE,
								x+WidDiv3-(Width*2), y+HeiDiv4,
								Width*2+1*WidDiv3, 2*HeiDiv4);
			gdk_draw_rectangle(DrawPixmap, DynaGcOff, FALSE,
								x+WidDiv3-(Width*2), y+HeiDiv4,
								Width*2+1*WidDiv3, 2*HeiDiv4);
			/* input */
			gdk_draw_line(DrawPixmap, TheGc,
								x-Width*2,y+HeiDiv2, x-Width*2+WidDiv3,y+HeiDiv2);
			/* output */
			gdk_draw_line(DrawPixmap, TheGc,
								x+WidDiv3*2,y+HeiDiv2, x+Width,y+HeiDiv2);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv4-(Width*2)+3,y+HeiDiv4-1,"ASSIGNMENT",strlen("ASSIGNMENT"));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv4-(Width*2)+3, y+HeiDiv4+1, -1, -1, "ASSIGNMENT" );
#endif
			/* arithmetic expression */
			if (!EditDatas.ModeEdit)
				strcpy(BufTxt,DisplayArithmExpr(ArithmExpr[Element.VarNum].Expr,(Width*2+1*WidDiv3)/8));
			else
				strcpy(BufTxt,DisplayArithmExpr(EditArithmExpr[Element.VarNum].Expr,(Width*2+1*WidDiv3)/8));
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
						x+WidDiv3-(Width*2)+2,y+HeiDiv2+4,BufTxt,strlen(BufTxt));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+WidDiv3-(Width*2)+2,y, -1, Height, BufTxt );
#endif
			break;    }

	/* Drawing Var */
	if (DrawingOption!=DRAW_FOR_TOOLBAR)
	{
		switch(Element.Type)
		{
			case ELE_INPUT:
			case ELE_INPUT_NOT:				
			case ELE_RISING_INPUT:
			case ELE_FALLING_INPUT:
				strcpy(BufTxt,CreateVarName(Element.VarType,Element.VarNum));
				gdk_gc_set_foreground(DynaGcOff,&DynaGdkColor1);//color this text red
				DrawTextGTK2( DrawPixmap, DynaGcOff, x, y+HeiDiv4+1, Width, -1, BufTxt );
				gdk_gc_set_foreground(DynaGcOff,&DynaGdkColor3);//change color back to black
	
				break;
			case ELE_OUTPUT:
			case ELE_OUTPUT_NOT:
			case ELE_OUTPUT_SET:
			case ELE_OUTPUT_RESET:
				
				strcpy(BufTxt,CreateVarName(Element.VarType,Element.VarNum));
				
#ifndef GTK2
				gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
								x+WidDiv4,y+HeiDiv4-2,BufTxt,strlen(BufTxt));
#else
				gdk_gc_set_foreground(DynaGcOff,&DynaGdkColor2); //color text blue
				DrawTextGTK2( DrawPixmap, DynaGcOff, x, y+HeiDiv4+1, Width, -1, BufTxt );
				gdk_gc_set_foreground(DynaGcOff,&DynaGdkColor3);//back to black
	
#endif
				break;
			case ELE_OUTPUT_JUMP:
#ifndef GTK2
				gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
								x+1,y+HeiDiv4-2,RungArray[Element.VarNum].Label,
								strlen(RungArray[Element.VarNum].Label));
#else
				DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x, y+HeiDiv4+1, Width, -1, RungArray[Element.VarNum].Label );
#endif
				break;
			case ELE_OUTPUT_CALL:
				sprintf( BufTxt, "SR%d", Element.VarNum );
#ifndef GTK2
				gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
								x+1,y+HeiDiv4-2,BufTxt,strlen(BufTxt));
#else
				DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x, y+HeiDiv4+1, Width, -1, BufTxt );
#endif
				break;
		}
	}
	/* Drawing cnx with top */
	if (Element.ConnectedWithTop)
	{
		if (Element.DynamicInput)
			gdk_draw_line(DrawPixmap, DynaGcOn,
				x,y+HeiDiv2 +1, x,y-HeiDiv2);
		else
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x,y+HeiDiv2, x,y-HeiDiv2);
	}

	/* specials used for Editor */
	if (DrawingOption==DRAW_FOR_TOOLBAR)
	{
		switch(Element.Type)
		{
			case EDIT_CNX_WITH_TOP:
				gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
					x+WidDiv2,y+HeiDiv4, x+WidDiv2,y+Height-HeiDiv4);
				break;
			case EDIT_LONG_CONNECTION:
				gdk_draw_line(DrawPixmap, TheGc,
								x,y+HeiDiv2, x+Width-1,y+HeiDiv2);
				gdk_draw_line(DrawPixmap, TheGc,
								x+3*WidDiv4-1,y+HeiDiv4, x+Width-1,y+HeiDiv2);
				gdk_draw_line(DrawPixmap, TheGc,
								x+3*WidDiv4-1,y+3*HeiDiv4, x+Width-1,y+HeiDiv2);
				gdk_draw_line(DrawPixmap, TheGc,
								x+3*WidDiv4-1,y+HeiDiv4, x+3*WidDiv4 +3,y+HeiDiv2);
				gdk_draw_line(DrawPixmap, TheGc,
								x+3*WidDiv4 +3,y+HeiDiv2, x+3*WidDiv4-1,y+3*HeiDiv4);
				break;
			/* little display used for the toolbar */
			case ELE_TIMER:
			case ELE_MONOSTABLE:
			case ELE_COUNTER:
			case ELE_TIMER_IEC:
				{
					char * Letter = "T";
					if ( Element.Type==ELE_MONOSTABLE )
						Letter = "M";
					if ( Element.Type==ELE_COUNTER )
						Letter = "C";
					if ( Element.Type==ELE_TIMER_IEC )
						Letter = "TM";
					gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc, FALSE,
									x/*+WidDiv4*/, y+HeiDiv4,
									Width-2/**WidDiv4*/, Height-2*HeiDiv4);
#ifndef GTK2
					gdk_draw_text(DrawPixmap, drawing_area->style->font, TheGc,
							x+WidDiv3+2,y+HeiDiv3*2,Letter,1);
#else
					DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, Letter );
#endif
				}
				break;
			case ELE_COMPAR:
				gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc, FALSE,
									x+WidDiv4, y+HeiDiv4,
									Width-2*WidDiv4, Height-2*HeiDiv4);
#ifndef GTK2
				gdk_draw_text(DrawPixmap, drawing_area->style->font, TheGc,
							x+WidDiv3+2,y+HeiDiv3*2,">",1);
#else
				DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, ">" );
#endif
				break;
			case ELE_OUTPUT_OPERATE:
				gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc, FALSE,
									x+WidDiv4, y+HeiDiv4,
									Width-2*WidDiv4, Height-2*HeiDiv4);
#ifndef GTK2
				gdk_draw_text(DrawPixmap, drawing_area->style->font, TheGc,
							x+WidDiv3+2,y+HeiDiv3*2,"=",1);
#else
				DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, "=" );
#endif
				break;
			default:
				DrawCommonElementForToolbar(DrawPixmap, x, y, Width, Element.Type);
				break;
		}
	}
	else
	{
		// only usefull to see abnormal elements in a rung (eraser bug in versions < 0.7.124)
		if ( Element.Type>=EDIT_CNX_WITH_TOP )
		{
			sprintf( BufTxt, "(%d)", Element.Type );
			DrawTextGTK2( DrawPixmap, TheGc, x, y, Width, Height, BufTxt );
		}
	}
	if (!EditDatas.ModeEdit && DrawingOption!=DRAW_FOR_PRINT )
		gdk_gc_unref(DynaGcOn);
}

void DrawBars( int PosiY, int IsTheCurrentRung )
{
	GdkColor DynaGdkColor;
	GdkGC * DynaGcBarsColor;
	DynaGdkColor.pixel = 0xEE5555;
	DynaGdkColor.red = 0xEE;
	DynaGdkColor.green = 0x55;
	DynaGdkColor.blue = 0x55;
	if (!IsTheCurrentRung )
	{
		DynaGcBarsColor = drawing_area->style->black_gc;
	}
	else
	{
		DynaGcBarsColor = gdk_gc_new(pixmap);
		gdk_gc_set_foreground(DynaGcBarsColor,&DynaGdkColor);
	}
	gdk_draw_rectangle(pixmap, DynaGcBarsColor, TRUE,
								1, 1+PosiY,
								3, InfosGene->BlockHeight*RUNG_HEIGHT);
	gdk_draw_rectangle(pixmap, DynaGcBarsColor, TRUE,
								InfosGene->BlockWidth*RUNG_WIDTH+OFFSET_X, 1+PosiY,
								3, InfosGene->BlockHeight*RUNG_HEIGHT);
	if (IsTheCurrentRung )
		gdk_gc_unref(DynaGcBarsColor);
}

void DrawGrid( int PosiY )
{
	int x,y;
	GdkColor DynaGdkColor;
	GdkGC * DynaGcOn;
	DynaGdkColor.pixel = 0xA0D0D0;
	DynaGdkColor.red = 0xA0;
	DynaGdkColor.green = 0xD0;
	DynaGdkColor.blue = 0xD0;

	DynaGcOn = gdk_gc_new(pixmap);
	gdk_gc_set_foreground(DynaGcOn,&DynaGdkColor);
	
	for(x=InfosGene->BlockWidth; x<RUNG_WIDTH*InfosGene->BlockWidth; x=x+InfosGene->BlockWidth)
	{
		gdk_draw_line(pixmap, DynaGcOn,
				x+OFFSET_X, OFFSET_Y+PosiY, x+OFFSET_X, RUNG_HEIGHT*InfosGene->BlockHeight+PosiY);
	}
	for(y=InfosGene->BlockHeight; y<RUNG_HEIGHT*InfosGene->BlockHeight; y=y+InfosGene->BlockHeight)
	{
		gdk_draw_line(pixmap, DynaGcOn,
				OFFSET_X, y+OFFSET_Y+PosiY, RUNG_WIDTH*InfosGene->BlockWidth+OFFSET_X, y+OFFSET_Y+PosiY);
	}
	gdk_gc_unref(DynaGcOn);
}

void DrawRungPartition( int PosiY )
{
	GdkColor DynaGdkColor;
	GdkGC * DynaGcOn;
	DynaGdkColor.pixel = 0xCCCCCC;
	DynaGdkColor.red = 0xCC;
	DynaGdkColor.green = 0xCC;
	DynaGdkColor.blue = 0xCC;

	DynaGcOn = gdk_gc_new(pixmap);
	gdk_gc_set_foreground(DynaGcOn,&DynaGdkColor);
	gdk_draw_line(pixmap, DynaGcOn,
			OFFSET_X,OFFSET_Y+PosiY, RUNG_WIDTH*InfosGene->BlockWidth+OFFSET_X,OFFSET_Y+PosiY);
	gdk_gc_unref(DynaGcOn);
}

void DrawCurrentElementEdited( int AddPosiY )
{
	if ( EditDatas.CurrentElementSizeX>0 && EditDatas.CurrentElementSizeY>0 )
	{
		int Left = (EditDatas.CurrentElementPosiX-EditDatas.CurrentElementSizeX+1)*InfosGene->BlockWidth +OFFSET_X;
		int Top = EditDatas.CurrentElementPosiY*InfosGene->BlockHeight +AddPosiY +OFFSET_Y;
		int Width = EditDatas.CurrentElementSizeX*InfosGene->BlockWidth;
		int Height = EditDatas.CurrentElementSizeY*InfosGene->BlockHeight;
		GdkColor DynaGdkColor;
		GdkGC * DynaGcColor;
		DynaGdkColor.pixel = 0xFF3030;
		DynaGdkColor.red = 0xFF;
		DynaGdkColor.green = 0x30;
		DynaGdkColor.blue = 0x30;
		DynaGcColor = gdk_gc_new( pixmap );
		gdk_gc_set_foreground( DynaGcColor, &DynaGdkColor );
		gdk_draw_rectangle( pixmap, DynaGcColor, FALSE, Left, Top, Width, Height );
		gdk_gc_unref( DynaGcColor );
	}
}

void GetTheSizesForRung()
{
	static int PageHeightBak = 0;
	static int BlockHeightBak = 0;
	InfosGene->BlockWidth = (GTK_WIDGET(drawing_area)->allocation.width - OFFSET_X -5) / RUNG_WIDTH;
	// keep ratio aspect (if defaults values of size block not square)
	InfosGene->BlockHeight = InfosGene->BlockWidth*BLOCK_HEIGHT_DEF/BLOCK_WIDTH_DEF;

	InfosGene->PageHeight = GTK_WIDGET(drawing_area)->allocation.height - OFFSET_Y;
	// used for sequential
	InfosGene->PageWidth = GTK_WIDGET(drawing_area)->allocation.width - OFFSET_X;

	// size of the page or block changed ?
	if ( InfosGene->PageHeight!=PageHeightBak || InfosGene->BlockHeight!=BlockHeightBak )
		UpdateVScrollBar();
	PageHeightBak = InfosGene->PageHeight;
	BlockHeightBak = InfosGene->BlockHeight;
}

void DrawRung(GdkPixmap * DrawPixmap, StrRung * Rung, int PosiY, int BlockWidth, int BlockHeight, char DrawingOption)
{
	int x,y;
	for (y=0;y<RUNG_HEIGHT;y++)
	{
		for(x=0;x<RUNG_WIDTH;x++)
		{
			DrawElement(DrawPixmap, x*BlockWidth+OFFSET_X, y*BlockHeight+OFFSET_Y+PosiY, BlockWidth,BlockHeight, Rung->Element[x][y], DrawingOption);
		}
	}
}

void DrawRungs()
{
	int ScanRung = InfosGene->TopRungDisplayed;
	int ScanY;
	StrRung * PtrRung;
	int FlagAddOrInsertRung = FALSE;
	int TheEnd = FALSE;

	// If we are editing and adding a rung, we set a flag to indicate
	// that there is an extra rung to add (before or after current rung)
	if ( EditDatas.ModeEdit && EditDatas.DoBeforeFinalCopy == MODE_ADD )
	{
		// beware not the first rung displayed at the top...
		if ( InfosGene->FirstRung!=InfosGene->CurrentRung )
		{
			if ( ScanRung!=InfosGene->LastRung )
				ScanRung = RungArray[ ScanRung ].NextRung;
			FlagAddOrInsertRung = TRUE;
		}
	}
	if ( EditDatas.ModeEdit && EditDatas.DoBeforeFinalCopy == MODE_INSERT )
		FlagAddOrInsertRung = TRUE;

	// Clean all
	gdk_draw_rectangle(pixmap, drawing_area->style->white_gc, TRUE,
						0, 0, InfosGene->BlockWidth*RUNG_WIDTH+50, InfosGene->PageHeight+50);

	for (ScanY = InfosGene->OffsetHiddenTopRungDisplayed*-1; ScanY<InfosGene->PageHeight && !TheEnd; ScanY=ScanY+(InfosGene->BlockHeight*RUNG_HEIGHT))
	{
		PtrRung = &RungArray[ ScanRung ];
		/* displaying the current rung - in edit ? */
		if ( ScanRung==InfosGene->CurrentRung && EditDatas.ModeEdit )
		{
			if ( ( FlagAddOrInsertRung && EditDatas.DoBeforeFinalCopy==MODE_INSERT )
				|| ( !FlagAddOrInsertRung && EditDatas.DoBeforeFinalCopy==MODE_ADD )
				|| EditDatas.DoBeforeFinalCopy==MODE_MODIFY )
			{
				/* grid for edit mode and display the rung under edition */
				DrawGrid( ScanY );
				PtrRung = &EditDatas.Rung;
				// draw the current element.
				if ( EditDatas.ElementUnderEdit!=NULL )
					DrawCurrentElementEdited( ScanY );
			}
		}
		DrawBars( ScanY, ScanRung==InfosGene->CurrentRung );
		DrawRung( pixmap, PtrRung, ScanY, InfosGene->BlockWidth, InfosGene->BlockHeight, DRAW_NORMAL );

		// if we are adding or inserting a rung, it is as if we have 2 current rungs...
		// If inserting : display edited one, then the current.
		// if adding : display current, then the edited one.
		// Edited one is displayed at the current position (current before... well I hope
		// you still follow, because I'm not sure for me ;-) )
		if ( ! (ScanRung==InfosGene->CurrentRung  && FlagAddOrInsertRung) )
		{
			if ( ScanRung!=InfosGene->LastRung )
				ScanRung = RungArray[ ScanRung ].NextRung;
			else
				TheEnd = TRUE;
		}
		else
		{
			FlagAddOrInsertRung = FALSE;
		}
		DrawRungPartition( ScanY + InfosGene->BlockHeight*RUNG_HEIGHT -5 );
	}
}

void DrawCurrentSection( void )
{
	GdkRectangle update_rect;
	int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
	GetTheSizesForRung();
	if ( iCurrentLanguage==SECTION_IN_LADDER )
		DrawRungs( );
#ifdef SEQUENTIAL_SUPPORT
	if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
		DrawSequentialPage( pixmap, SectionArray[ InfosGene->CurrentSection ].SequentialPage, 0 );
#endif
	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = GTK_WIDGET(drawing_area)->allocation.width;
	update_rect.height = GTK_WIDGET(drawing_area)->allocation.height;
	gtk_widget_draw (drawing_area, &update_rect);
}

