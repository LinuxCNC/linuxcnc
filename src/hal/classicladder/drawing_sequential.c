/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* October 2002 */
/* --------------------- */
/* Draw sequential pages */
/* --------------------- */
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
#include "drawing.h"
#include "vars_names.h"
#include "drawing_sequential.h"

#ifdef GTK2
#include <pango/pango.h>
#endif

void DrawSeqStep(GdkPixmap * DrawPixmap,int x,int y,int Size,StrStep * pStep,char DrawingOption)
{
	char BufTxt[50];
	GdkGC * TheGc;
	GdkColor DynaGdkColor;
	GdkGC * DynaGcOn;
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
	/* State with color */
	TheGc = drawing_area->style->black_gc;
	if ( (DrawingOption==DRAW_NORMAL) && (!EditDatas.ModeEdit) && (pStep->Activated) )
		TheGc = DynaGcOn;

	gdk_draw_rectangle(DrawPixmap, TheGc, FALSE /*filled*/,
			x+2, y+2,
			Size-4, Size-4);
	// init step ?
	if ( pStep->InitStep )
	{
		gdk_draw_rectangle(DrawPixmap, /*TheGc*/drawing_area->style->black_gc, FALSE /*filled*/,
				x+5, y+5,
				Size-10, Size-10);
	}
	// step number
	sprintf(BufTxt,"%d",pStep->StepNumber);
#ifndef GTK2
	gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
		x+SizeDiv3,y+SizeDiv2+4,BufTxt,strlen(BufTxt));
#else
	DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x, y, Size, Size, BufTxt );
#endif
	gdk_gc_unref(DynaGcOn);
}

void DrawSeqTransition(GdkPixmap * DrawPixmap,int x,int y,int Size,StrTransition * pTransi,char DrawingOption)
{
	int SizeDiv2 = Size/2;
	int SizeDiv3 = Size/3;
	int SizeDiv4 = Size/4;
	char BufTxt[50];
	GdkGC * DynaGcOff;
	GdkGC * TheGc;
	GdkColor DynaGdkColor;
	GdkGC * DynaGcOn;

	int ScanSwitch;
	int StepX,StepY;
	StrStep * pStep, * pStep2;
	int TransiX;
	char cNoDirectStepOnTop = FALSE;
	char cNoDirectStepOnBottom = FALSE;

	StrSequential * Seq = Sequential;
	// lifts positions (not used if for print!)
	int ShiftX = InfosGene->HScrollValue;
	int ShiftY = InfosGene->VScrollValue;
	if ( DrawingOption==DRAW_FOR_PRINT )
	{
		ShiftX = 0;
		ShiftY = 0;
	}
	// if in edit, use datas in edit...
	if ( EditDatas.ModeEdit )
		Seq = &EditSeqDatas;

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
	DynaGcOff = drawing_area->style->black_gc;
	/* State with color */
	TheGc = drawing_area->style->black_gc;
	if ( (DrawingOption==DRAW_NORMAL) && (!EditDatas.ModeEdit) && (pTransi->Activated) )
		TheGc = DynaGcOn;

	// direct step on top ?
	pStep = &Seq->Step[ pTransi->NumStepToDesactiv[ 0 ] ];
	StepX = pStep->PosiX;
	StepY = pStep->PosiY;
	if ( pTransi->PosiX!=StepX || pTransi->PosiY-1!=StepY )
		cNoDirectStepOnTop = TRUE;
	// direct step on bottom ?
	pStep = &Seq->Step[ pTransi->NumStepToActiv[ 0 ] ];
	StepX = pStep->PosiX;
	StepY = pStep->PosiY;
	if ( pTransi->PosiX!=StepX || pTransi->PosiY+1!=StepY )
		cNoDirectStepOnBottom = TRUE;

	// |
	gdk_draw_line(DrawPixmap, DynaGcOff,
			x+SizeDiv2, cNoDirectStepOnTop?y+SizeDiv3:y,
			x+SizeDiv2, cNoDirectStepOnBottom?y+Size-SizeDiv3:y+Size+1);

	// -
	gdk_draw_line(DrawPixmap, TheGc,
			x+SizeDiv3, y+SizeDiv2,
			x+Size-SizeDiv3, y+SizeDiv2);

	// variable for transition
	strcpy(BufTxt, CreateVarName( pTransi->VarTypeCondi, pTransi->VarNumCondi ) );
#ifndef GTK2
	gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
		x+3*SizeDiv4,y+SizeDiv2+5,BufTxt,strlen(BufTxt));
#else
	DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+3*SizeDiv4, y+1, -1, Size, BufTxt );
#endif

	// multiple steps activation ?
	for ( ScanSwitch=1; ScanSwitch<NBR_SWITCHS_MAX; ScanSwitch++ )
	{
		int NumStep;
		NumStep = pTransi->NumStepToActiv[ ScanSwitch ];
		if ( NumStep!=-1 )
		{
			StepX = Seq->Step[ NumStep ].PosiX * Size - ShiftX;
			// ===
			gdk_draw_line(DrawPixmap, DynaGcOff,
				x+SizeDiv3, y+Size-SizeDiv4+2, StepX+Size-SizeDiv3, y+Size-SizeDiv4+2);
			gdk_draw_line(DrawPixmap, DynaGcOff,
				x+SizeDiv3, y+Size-SizeDiv4+4, StepX+Size-SizeDiv3, y+Size-SizeDiv4+4);
			gdk_draw_line(DrawPixmap, drawing_area->style->white_gc,
				x+SizeDiv3, y+Size-SizeDiv4+3, StepX+Size-SizeDiv3, y+Size-SizeDiv4+3);
			// | with the step
			gdk_draw_line(DrawPixmap, DynaGcOff,
				StepX+SizeDiv2, y+Size-SizeDiv4+4, StepX+SizeDiv2, y+Size+1);
		}
	}

	// multiple steps desactivation ?
	for ( ScanSwitch=1; ScanSwitch<NBR_SWITCHS_MAX; ScanSwitch++ )
	{
		int NumStep;
		NumStep = pTransi->NumStepToDesactiv[ ScanSwitch ];
		if ( NumStep!=-1 )
		{
			StepX = Seq->Step[ NumStep ].PosiX * Size - ShiftX;
			// ===
			gdk_draw_line(DrawPixmap, DynaGcOff,
				x+SizeDiv3, y+SizeDiv4-2, StepX+Size-SizeDiv3, y+SizeDiv4-2);
			gdk_draw_line(DrawPixmap, DynaGcOff,
				x+SizeDiv3, y+SizeDiv4-4, StepX+Size-SizeDiv3, y+SizeDiv4-4);
			gdk_draw_line(DrawPixmap, drawing_area->style->white_gc,
				x+SizeDiv3, y+SizeDiv4-3, StepX+Size-SizeDiv3, y+SizeDiv4-3);
			// | with the step
			gdk_draw_line(DrawPixmap, DynaGcOff,
				StepX+SizeDiv2, y, StepX+SizeDiv2, y+SizeDiv4-4);
		}
	}

	// transitions linked (start of OR)
	if ( pTransi->NumTransLinkedForStart[ 0 ]!=-1 )
	{
		for ( ScanSwitch=0; ScanSwitch<NBR_SWITCHS_MAX; ScanSwitch++ )
		{
			int NumTransi;
			NumTransi = pTransi->NumTransLinkedForStart[ ScanSwitch ];
			if ( NumTransi!=-1 )
			{
				TransiX = Seq->Transition[ NumTransi ].PosiX * Size - ShiftX;
				gdk_draw_line(DrawPixmap, DynaGcOff,
					x+SizeDiv2, y+SizeDiv3, TransiX+SizeDiv2, y+SizeDiv3);
			}
		}
	}

	// transitions linked (end of OR)
	if ( pTransi->NumTransLinkedForEnd[ 0 ]!=-1 )
	{
		for ( ScanSwitch=0; ScanSwitch<NBR_SWITCHS_MAX; ScanSwitch++ )
		{
			int NumTransi;
			NumTransi = pTransi->NumTransLinkedForEnd[ ScanSwitch ];
			if ( NumTransi!=-1 )
			{
				TransiX = Seq->Transition[ NumTransi ].PosiX * Size - ShiftX;
				gdk_draw_line(DrawPixmap, DynaGcOff,
					x+SizeDiv2, y+Size-SizeDiv3, TransiX+SizeDiv2, y+Size-SizeDiv3);
			}
		}
	}

	// cross step ?
	if ( pTransi->NumStepToActiv[ 0 ]!=-1 && pTransi->NumStepToDesactiv[ 0 ]!=-1 )
	{
		pStep = &Seq->Step[ pTransi->NumStepToActiv[ 0 ] ];
		pStep2 = &Seq->Step[ pTransi->NumStepToDesactiv[ 0 ] ];
		StepX = pStep->PosiX;
		StepY = pStep->PosiY;
		if ( pTransi->PosiX!=StepX || pTransi->PosiY+1!=StepY )
		{
			if ( pTransi->NumTransLinkedForEnd[ 0 ]==-1 )
			{
				// draw v with step number (below transition)
				gdk_draw_line(DrawPixmap, DynaGcOff,
					x+SizeDiv3, y+Size-SizeDiv3, x+SizeDiv2, y+Size-SizeDiv3+SizeDiv4);
				gdk_draw_line(DrawPixmap, DynaGcOff,
					x+Size-SizeDiv3, y+Size-SizeDiv3, x+SizeDiv2, y+Size-SizeDiv3+SizeDiv4);
				gdk_draw_line(DrawPixmap, DynaGcOff,
					x+SizeDiv2, y+Size-SizeDiv3, x+SizeDiv2, y+Size-SizeDiv3+SizeDiv4);
				sprintf( BufTxt, "%d", pStep->StepNumber );
#ifndef GTK2
				gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
					x+SizeDiv3,y+Size-SizeDiv3+SizeDiv4+11,BufTxt,strlen(BufTxt));
#else
				DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x+SizeDiv3,y+Size-SizeDiv3+SizeDiv4+13, -1, -1, BufTxt );
#endif


				// draw step number with v (above the cross step)
				StepX = StepX*Size - ShiftX;
				StepY = StepY*Size - ShiftY;
				gdk_draw_line(DrawPixmap, DynaGcOff,
					StepX+SizeDiv3, StepY-SizeDiv2, StepX+SizeDiv2, StepY-SizeDiv4);
				gdk_draw_line(DrawPixmap, DynaGcOff,
					StepX+Size-SizeDiv3, StepY-SizeDiv2, StepX+SizeDiv2, StepY-SizeDiv4);
				gdk_draw_line(DrawPixmap, DynaGcOff,
					StepX+SizeDiv2, StepY-SizeDiv4, StepX+SizeDiv2, StepY+1);
				if ( pStep->OffDrawCrossStep==0 )
					sprintf( BufTxt, "%d", pStep2->StepNumber );
				else
					sprintf( BufTxt, ";%d", pStep2->StepNumber );
#ifndef GTK2
				gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
					StepX+SizeDiv3+pStep->OffDrawCrossStep,StepY-SizeDiv4-9,BufTxt,strlen(BufTxt));
#else
				DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, StepX+SizeDiv3+pStep->OffDrawCrossStep,StepY-SizeDiv4-7, -1, -1, BufTxt );
#endif
//printf("CrossStep n°=%d, offX=%d\n", pStep->StepNumber, pStep->OffDrawCrossStep );
				pStep->OffDrawCrossStep = pStep->OffDrawCrossStep+((pStep->OffDrawCrossStep!=0)?3:0)+15; // TODO: add length in pixels of the text written
			}
		}
	}
//TODO: cross step for pTransi->NumStepToDesactiv[ 0 ]

	gdk_gc_unref(DynaGcOn);
}

void DrawSeqComment(GdkPixmap * DrawPixmap,int x,int y,int Size,StrSeqComment * pSeqComment)
{
	char * BufTxt = pSeqComment->Comment;
	GdkColor CornersColor = {0, 38000, 38000, 62000 };
	GdkGC *gcCorners = gdk_gc_new( pixmap );
	gdk_gc_set_rgb_fg_color( gcCorners, &CornersColor );
	gdk_draw_line(DrawPixmap, gcCorners, x+2 +6, y+2, x+2, y+2 +6);
	gdk_draw_line(DrawPixmap, gcCorners, x+2+4*Size-4 -6, y+2+Size-4, x+2+4*Size-4, y+2+Size-4 -6);
	gdk_draw_rectangle(DrawPixmap, drawing_area->style->black_gc, FALSE /*filled*/,
			x+2, y+2,
			4*Size-4, Size-4);
#ifndef GTK2
	gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
		x+SizeDiv3,y+SizeDiv2+4,BufTxt,strlen(BufTxt));
#else
	DrawTextWithOffsetGTK2( DrawPixmap, drawing_area->style->black_gc, x, y, 4*Size, Size, BufTxt, 3 );
#endif
	g_object_unref( G_OBJECT(gcCorners) );
}

void DrawSequentialGrid( )
{
	int x,y,z;
	GdkColor DynaGdkColor;
	GdkGC * DynaGcOn;
	DynaGdkColor.pixel = 0xF4F4F4;
	DynaGdkColor.red = 0xF4;
	DynaGdkColor.green = 0xF4;
	DynaGdkColor.blue = 0xF4;

	DynaGcOn = gdk_gc_new(pixmap);
	gdk_gc_set_foreground(DynaGcOn,&DynaGdkColor);
	z = 0;
	for(y=0; y<=SEQ_SIZE_DEF*SEQ_PAGE_HEIGHT; y=y+SEQ_SIZE_DEF)
	{
		gdk_draw_line(pixmap, DynaGcOn, 0, y-InfosGene->VScrollValue, SEQ_SIZE_DEF*NBR_STEPS, y-InfosGene->VScrollValue );
		if ( z&1 )
		{
			for(x=0; x<=SEQ_SIZE_DEF*SEQ_PAGE_WIDTH; x=x+SEQ_SIZE_DEF)
				gdk_draw_line(pixmap, DynaGcOn, x-InfosGene->HScrollValue, y-InfosGene->VScrollValue, x-InfosGene->HScrollValue, y-InfosGene->VScrollValue+SEQ_SIZE_DEF );
		}
		z++;
	}
	gdk_gc_unref(DynaGcOn);
}

void DrawSequentialPage( GdkPixmap * DrawPixmap, int PageNbr, char DrawingOption )
{
	int ScanStep;
	StrStep * pStep;
	int ScanTransi;
	StrTransition * pTransi;
	int ScanSeqComment;
	StrSequential * Seq = Sequential;
	// lifts positions (not used if for print!)
	int ShiftX = InfosGene->HScrollValue;
	int ShiftY = InfosGene->VScrollValue;
	if ( DrawingOption==DRAW_FOR_PRINT )
	{
		ShiftX = 0;
		ShiftY = 0;
	}
	// if in edit, use datas in edit...
	if ( EditDatas.ModeEdit )
		Seq = &EditSeqDatas;	
	// clean all
	gdk_draw_rectangle(pixmap, drawing_area->style->white_gc, TRUE /*filled*/,
		0, 0, GTK_WIDGET(drawing_area)->allocation.width,GTK_WIDGET(drawing_area)->allocation.height);
	if ( EditDatas.ModeEdit )
		DrawSequentialGrid( );
	for( ScanStep=0; ScanStep<NBR_STEPS; ScanStep++ )
	{
		pStep = &Seq->Step[ ScanStep ];
		if ( pStep->NumPage==PageNbr )
			pStep->OffDrawCrossStep = 0;
	}
	// draw the steps of this page
	for( ScanStep=0; ScanStep<NBR_STEPS; ScanStep++ )
	{
		pStep = &Seq->Step[ ScanStep ];
		if ( pStep->NumPage==PageNbr )
			DrawSeqStep( DrawPixmap, pStep->PosiX*SEQ_SIZE_DEF-ShiftX, pStep->PosiY*SEQ_SIZE_DEF-ShiftY, SEQ_SIZE_DEF, pStep, DrawingOption );
	}
	// draw the transitions of this page
	for( ScanTransi=0; ScanTransi<NBR_TRANSITIONS; ScanTransi++ )
	{
		pTransi = &Seq->Transition[ ScanTransi ];
		if ( pTransi->NumPage==PageNbr )
			DrawSeqTransition( DrawPixmap, pTransi->PosiX*SEQ_SIZE_DEF-ShiftX, pTransi->PosiY*SEQ_SIZE_DEF-ShiftY, SEQ_SIZE_DEF, pTransi, DrawingOption );
	}
	// draw the comments
	for( ScanSeqComment=0; ScanSeqComment<NBR_SEQ_COMMENTS; ScanSeqComment++ )
	{
		StrSeqComment * pSeqComment = &Seq->SeqComment[ ScanSeqComment ];
		if ( pSeqComment->NumPage==PageNbr )
			DrawSeqComment( DrawPixmap, pSeqComment->PosiX*SEQ_SIZE_DEF-ShiftX,
							pSeqComment->PosiY*SEQ_SIZE_DEF-ShiftY, SEQ_SIZE_DEF, pSeqComment);
	}
}

void DrawSeqElementForToolBar( GdkPixmap * DrawPixmap, int x, int y, int Size, int NumElement )
{
	int SizeDiv2 = Size/2;
	int SizeDiv3 = Size/3;
	int SizeDiv4 = Size/4;
	GdkGC * TheGc;
	TheGc = drawing_area->style->black_gc;
	switch( NumElement )
	{
		case ELE_SEQ_STEP:
		case EDIT_SEQ_INIT_STEP:
			gdk_draw_rectangle(DrawPixmap, TheGc, FALSE /*filled*/,
				x+SizeDiv4, y+SizeDiv4,
				Size-2*SizeDiv4, Size-2*SizeDiv4);
			if( NumElement==EDIT_SEQ_INIT_STEP )
				gdk_draw_rectangle(DrawPixmap, TheGc, FALSE /*filled*/,
					x+SizeDiv4+2, y+SizeDiv4+2,
					Size-2*SizeDiv4-4, Size-2*SizeDiv4-4);
			break;
		case ELE_SEQ_TRANSITION:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv2,y+SizeDiv4, x+SizeDiv2,y+Size-SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv3,y+SizeDiv2, x+Size-SizeDiv3,y+SizeDiv2);
			break;
		case EDIT_SEQ_STEP_AND_TRANS:
			gdk_draw_rectangle(DrawPixmap, TheGc, FALSE /*filled*/,
				x+SizeDiv4, y+2,
				Size-2*SizeDiv4, SizeDiv2-2);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv2,y+SizeDiv2, x+SizeDiv2,y+Size-2);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv3,y+Size-SizeDiv4, x+Size-SizeDiv3,y+Size-SizeDiv4);
			break;
		case EDIT_SEQ_START_MANY_TRANS:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+2,y+SizeDiv4, x+Size-2,y+SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+SizeDiv4-4, x+SizeDiv4,y+SizeDiv4+4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+Size-SizeDiv4,y+SizeDiv4, x+Size-SizeDiv4,y+SizeDiv4+4);
			break;
		case EDIT_SEQ_END_MANY_TRANS:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+2,y+Size-SizeDiv4, x+Size-2,y+Size-SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+Size-SizeDiv4-4, x+SizeDiv4,y+Size-SizeDiv4+4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+Size-SizeDiv4,y+Size-SizeDiv4, x+Size-SizeDiv4,y+Size-SizeDiv4-4);
			break;
		case EDIT_SEQ_START_MANY_STEPS:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+2,y+SizeDiv4, x+Size-2,y+SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+2,y+SizeDiv4+2, x+Size-2,y+SizeDiv4+2);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+SizeDiv4-4, x+SizeDiv4,y+SizeDiv4+4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+Size-SizeDiv4,y+SizeDiv4+2, x+Size-SizeDiv4,y+SizeDiv4+4);
			break;
		case EDIT_SEQ_END_MANY_STEPS:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+2,y+Size-SizeDiv4, x+Size-2,y+Size-SizeDiv4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+2,y+Size-SizeDiv4-2, x+Size-2,y+Size-SizeDiv4-2);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv4,y+Size-SizeDiv4-4, x+SizeDiv4,y+Size-SizeDiv4+4);
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+Size-SizeDiv4,y+Size-SizeDiv4-2, x+Size-SizeDiv4,y+Size-SizeDiv4-4);
			break;
		case EDIT_SEQ_LINK:
			gdk_draw_line(DrawPixmap, drawing_area->style->black_gc,
				x+SizeDiv3,y+SizeDiv4, x+Size-SizeDiv3,y+Size-SizeDiv4);
			gdk_draw_arc(DrawPixmap, drawing_area->style->black_gc, FALSE/*filled*/,
				x+SizeDiv3-2,y+SizeDiv4-2, 4, 4, 0/*angle1*/, 23040/*angle2*/);
			gdk_draw_arc(DrawPixmap, drawing_area->style->black_gc, FALSE/*filled*/,
				x+Size-SizeDiv3-2,y+Size-SizeDiv4-2, 4, 4, 0/*angle1*/, 23040/*angle2*/);
			break;
		case ELE_SEQ_COMMENT:
			gdk_draw_rectangle(DrawPixmap, TheGc, FALSE /*filled*/,
				x+SizeDiv4, y+SizeDiv4,
				Size-2*SizeDiv4, Size-2*SizeDiv4);
#ifndef GTK2
			gdk_draw_text(DrawPixmap, drawing_area->style->font, drawing_area->style->black_gc,
				x+SizeDiv3,y+SizeDiv2+4,"xx",strlen("xx"));
#else
			DrawTextGTK2( DrawPixmap, drawing_area->style->black_gc, x, y, Size, Size, "xx" );
#endif
			break;
		default:
			DrawCommonElementForToolbar( DrawPixmap, x, y, Size, NumElement );
			break;
	}
}
