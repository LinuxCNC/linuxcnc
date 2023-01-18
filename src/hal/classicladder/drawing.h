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

void CreateVarNameForElement( char * pBuffToWrite, StrElement * pElem, char SymbolsVarsNamesIfAvail );
char * DisplayArithmExpr(char * Expr, char SymbolsVarsNamesIfAvail);
void CreateFontPangoLayout( cairo_t *cr, int BlockPxHeight, char DrawingOption );
int DrawPangoText( cairo_t * cr, int BaseX, int BaseY, int Width, int Height, char * Text );
void DrawCommonElementForToolbar( cairo_t * cr,int x,int y,int Size,int NumElement );
void my_cairo_draw_line( cairo_t *cr, double x1, double y1, double x2, double y2 );
void my_cairo_draw_color_line( cairo_t *cr, char cColor, double x1, double y1, double x2, double y2 );
void my_cairo_draw_black_rectangle( cairo_t *cr, double x, double y, double w, double h );
void DrawElement( cairo_t * cr,int x,int y,int Width,int Height,StrElement Element,char DrawingOption );
void DrawLeftRightBars( cairo_t * cr, int OffX, int PosiY, int BlockWidth, int BlockHeight, int HeaderLabelAndCommentHeight, int LeftRightBarsWidth, int IsTheCurrentRung );
void DrawRung( cairo_t * cr, StrRung * Rung, int OffX, int PosiY, int BlockWidth, int BlockHeight, int HeaderLabelAndCommentHeight, char DrawingOption );
void DrawRungs( cairo_t * cr );
void DrawCurrentSection( cairo_t * cr );

