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
void DrawSeqStep( cairo_t * cr,int x,int y,int Size,StrStep * pStep,char DrawingOption );
void DrawSeqTransition( cairo_t * cr,int x,int y,int Size,StrTransition * pTransi,char DrawingOption );

void DrawSequentialPage( cairo_t * cr, int PageNbr, int SeqPxSize, char DrawingOption );

void DrawSeqElementForToolBar( cairo_t * cr, int x, int y, int Size, int NumElement );
