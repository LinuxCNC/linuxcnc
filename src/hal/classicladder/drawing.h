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
#define DRAW_NORMAL 0
#define DRAW_FOR_TOOLBAR 1
#define DRAW_FOR_PRINT 2

char * DisplayArithmExpr(char * Expr,int NumCarMax);
void DrawTextWithOffsetGTK2( GdkPixmap * DrawPixmap, GdkGC * GcRef, int BaseX, int BaseY, int Width, int Height, char * Text, int BorderOffset );
void DrawTextGTK2( GdkPixmap * DrawPixmap, GdkGC * GcRef, int BaseX, int BaseY, int Width, int Height, char * Text );
void DrawCommonElementForToolbar(GdkPixmap * DrawPixmap,int x,int y,int Size,int NumElement);
void DrawElement(GdkPixmap * DrawPixmap,int x,int y,int Width,int Height,StrElement Element,char DrawingOption);
void GetTheSizesForRung();
void DrawRung(GdkPixmap * DrawPixmap, StrRung * Rung, int PosiY, int BlockWidth, int BlockHeight, char DrawingOption);
void DrawRungs();
void DrawCurrentSection( void );

