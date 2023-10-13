/* Classic Ladder Project */
/* Copyright (C) 2001-2010 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* October 2002 */
/* ---------------------------------------- */
/* Draw sequential pages                    */
/* ======================================== */
/* Switched to CAIRO library in august 2008 */
/* ---------------------------------------- */
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
/* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <pango/pango.h>

#include "classicladder.h"
#include "global.h"
#include "drawing.h"
#include "vars_names.h"
#include "drawing_sequential.h"

#include <rtapi_string.h>

void DrawSeqStep( cairo_t * cr,int x,int y,int Size,StrStep * pStep,char DrawingOption )
{
	char BufTxt[50];
	int PxOffsetActiveStep = Size/6;

	/* State with color */
	if ( (DrawingOption==DRAW_NORMAL) && (!EditDatas.ModeEdit) && (pStep->Activated) )
		cairo_set_source_rgb( cr, 1.0, 0.13, 1.0 );
	else
		cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );

	cairo_rectangle(cr,
			x+2, y+2,
			Size-4, Size-4);
	cairo_stroke( cr );
	// init step ?
	if ( pStep->InitStep )
	{
		cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );
		cairo_rectangle(cr,
				x+PxOffsetActiveStep, y+PxOffsetActiveStep,
				Size-2*PxOffsetActiveStep, Size-2*PxOffsetActiveStep);
		cairo_stroke( cr );
	}
	// step number
	snprintf(BufTxt, sizeof(BufTxt),"%d",pStep->StepNumber);
	DrawPangoText( cr, x, y, Size, Size, BufTxt );
}

void DrawSeqTransition( cairo_t * cr,int x,int y,int Size,StrTransition * pTransi,char DrawingOption )
{
	int SizeDiv2 = Size/2;
	int SizeDiv3 = Size/3;
	int SizeDiv4 = Size/4;
	int SizeLittle = Size/16;
	char BufTxt[50];

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

	/* State with color */
	if ( (DrawingOption==DRAW_NORMAL) && (!EditDatas.ModeEdit) && (pTransi->Activated) )
		cairo_set_source_rgb( cr, 1.0, 0.13, 1.0 );
	else
		cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );

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
	my_cairo_draw_color_line(cr, 0,
			x+SizeDiv2, (cNoDirectStepOnTop?y+SizeDiv3:y)  -1,
			x+SizeDiv2, cNoDirectStepOnBottom?y+Size-SizeDiv3:y+Size+1);

	// -
	my_cairo_draw_line(cr,
			x+SizeDiv3, y+SizeDiv2,
			x+Size-SizeDiv3, y+SizeDiv2);

	// variable for transition
	rtapi_strxcpy(BufTxt, CreateVarName( pTransi->VarTypeCondi, pTransi->VarNumCondi, InfosGene->DisplaySymbols ) );
	DrawPangoText( cr, x+3*SizeDiv4, y+1, -1, Size, BufTxt );

	// multiple steps activation ?
	for ( ScanSwitch=1; ScanSwitch<NBR_SWITCHS_MAX; ScanSwitch++ )
	{
		int NumStep;
		NumStep = pTransi->NumStepToActiv[ ScanSwitch ];
		if ( NumStep!=-1 )
		{
			StepX = Seq->Step[ NumStep ].PosiX * Size - ShiftX;
			// ===
			my_cairo_draw_color_line(cr, 0,
				x+SizeDiv3, y+Size-SizeDiv4+SizeLittle, StepX+Size-SizeDiv3, y+Size-SizeDiv4+SizeLittle);
			my_cairo_draw_color_line(cr, 0,
				x+SizeDiv3, y+Size-SizeDiv4+2*SizeLittle+1, StepX+Size-SizeDiv3, y+Size-SizeDiv4+2*SizeLittle+1);
//			my_cairo_draw_color_line(cr, 1,
//				x+SizeDiv3, y+Size-SizeDiv4+3, StepX+Size-SizeDiv3, y+Size-SizeDiv4+3);
			// | with the step
			my_cairo_draw_color_line(cr, 0,
				StepX+SizeDiv2, y+Size-SizeDiv4+2*SizeLittle, StepX+SizeDiv2, y+Size+1);
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
			my_cairo_draw_color_line(cr, 0,
				x+SizeDiv3, y+SizeDiv4-SizeLittle, StepX+Size-SizeDiv3, y+SizeDiv4-SizeLittle);
			my_cairo_draw_color_line(cr, 0,
				x+SizeDiv3, y+SizeDiv4-2*SizeLittle-1, StepX+Size-SizeDiv3, y+SizeDiv4-2*SizeLittle-1);
//			my_cairo_draw_color_line(cr, 1,
//				x+SizeDiv3, y+SizeDiv4-3, StepX+Size-SizeDiv3, y+SizeDiv4-3);
			// | with the step
			my_cairo_draw_color_line(cr, 0,
				StepX+SizeDiv2, y  -1, StepX+SizeDiv2, y+SizeDiv4-2*SizeLittle);
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
				my_cairo_draw_color_line(cr, 0,
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
				my_cairo_draw_color_line(cr, 0,
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
				my_cairo_draw_color_line(cr, 0,
					x+SizeDiv3, y+Size-SizeDiv3, x+SizeDiv2, y+Size-SizeDiv3+SizeDiv4);
				my_cairo_draw_color_line(cr, 0,
					x+Size-SizeDiv3, y+Size-SizeDiv3, x+SizeDiv2, y+Size-SizeDiv3+SizeDiv4);
				my_cairo_draw_color_line(cr, 0,
					x+SizeDiv2, y+Size-SizeDiv3, x+SizeDiv2, y+Size-SizeDiv3+SizeDiv4);
				snprintf( BufTxt, sizeof(BufTxt), "%d", pStep->StepNumber );
				cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );
				DrawPangoText( cr, x,y+Size+SizeDiv2, Size, -1, BufTxt );


				// draw step number with v (above the cross step)
				StepX = StepX*Size - ShiftX;
				StepY = StepY*Size - ShiftY;
				my_cairo_draw_color_line(cr, 0,
					StepX+SizeDiv3, StepY-SizeDiv2, StepX+SizeDiv2, StepY-SizeDiv4);
				my_cairo_draw_color_line(cr, 0,
					StepX+Size-SizeDiv3, StepY-SizeDiv2, StepX+SizeDiv2, StepY-SizeDiv4);
				my_cairo_draw_color_line(cr, 0,
					StepX+SizeDiv2, StepY-SizeDiv4, StepX+SizeDiv2, StepY+1);
				if ( pStep->OffDrawCrossStep==0 )
					snprintf( BufTxt, sizeof(BufTxt), "%d", pStep2->StepNumber );
				else
					snprintf( BufTxt, sizeof(BufTxt), ";%d", pStep2->StepNumber );
				DrawPangoText( cr, StepX+pStep->OffDrawCrossStep,StepY-SizeDiv2, Size, -1, BufTxt );
//printf("CrossStep nbr=%d, offX=%d\n", pStep->StepNumber, pStep->OffDrawCrossStep );
				pStep->OffDrawCrossStep = pStep->OffDrawCrossStep+((pStep->OffDrawCrossStep!=0)?3:0)+15; // TODO: add length in pixels of the text written
			}
		}
	}
//TODO: cross step for pTransi->NumStepToDesactiv[ 0 ]

}

void DrawSeqComment( cairo_t * cr,int x,int y,int Size,StrSeqComment * pSeqComment )
{
	char * BufTxt = pSeqComment->Comment;
	int Margin = Size/16;
	int Oblique = Size/5;
	cairo_set_source_rgb( cr, 0.58, 0.58, 0.95 );
	my_cairo_draw_line(cr, x+Margin +Oblique, y+Margin, x+Margin, y+Margin +Oblique);
	my_cairo_draw_line(cr, x+4*Size-Oblique, y+Size, x+4*Size, y+Size-Oblique);
	cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );
	cairo_rectangle(cr,
			x+Margin, y+Margin,
			4*Size-Margin, Size-Margin);
	cairo_stroke( cr );
//	DrawPangoTextWithOffset( cr, x, y, 4*Size, Size, BufTxt, 3 );
	DrawPangoText( cr, x, y, 4*Size, Size, BufTxt );
}

void DrawSequentialGrid( cairo_t * cr, int SeqPxSize )
{
	int x,y,z;
	cairo_set_source_rgb( cr, 0.96, 0.96, 0.96 );
	z = 0;
	for(y=0; y<=SeqPxSize*SEQ_PAGE_HEIGHT; y=y+SeqPxSize)
	{
		my_cairo_draw_line(cr, 0, y-InfosGene->VScrollValue, SeqPxSize*NBR_STEPS, y-InfosGene->VScrollValue );
		if ( z&1 )
		{
			for(x=0; x<=SeqPxSize*SEQ_PAGE_WIDTH; x=x+SeqPxSize)
				my_cairo_draw_line(cr, x-InfosGene->HScrollValue, y-InfosGene->VScrollValue, x-InfosGene->HScrollValue, y-InfosGene->VScrollValue+SeqPxSize );
		}
		z++;
	}
}

void DrawSequentialCurrentElementEdited( cairo_t * cr, int SeqPxSize, int OffsetX, int OffsetY )
{
	if ( EditDatas.CurrentElementSizeX>0 && EditDatas.CurrentElementSizeY>0 )
	{
		cairo_set_source_rgb( cr, 0.99, 0.19, 0.19 );
		cairo_rectangle( cr, EditDatas.CurrentElementPosiX*SeqPxSize-OffsetX, EditDatas.CurrentElementPosiY*SeqPxSize-OffsetY, EditDatas.CurrentElementSizeX*SeqPxSize, EditDatas.CurrentElementSizeY*SeqPxSize );
		cairo_clip( cr );
		cairo_paint_with_alpha( cr, 0.3 );
		cairo_reset_clip( cr );
	}
}

void DrawSequentialPage( cairo_t * cr, int PageNbr, int SeqPxSize, char DrawingOption )
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
	CreateFontPangoLayout( cr, SeqPxSize, DrawingOption );
	// clean all
//	gdk_draw_rectangle(pixmap, drawing_area->style->white_gc, TRUE /*filled*/,
//		0, 0, GTK_WIDGET(drawing_area)->allocation.width,GTK_WIDGET(drawing_area)->allocation.height);
	if ( EditDatas.ModeEdit )
		DrawSequentialGrid( cr, SeqPxSize );
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
			DrawSeqStep( cr, pStep->PosiX*SeqPxSize-ShiftX, pStep->PosiY*SeqPxSize-ShiftY, SeqPxSize, pStep, DrawingOption );
	}
	// draw the transitions of this page
	for( ScanTransi=0; ScanTransi<NBR_TRANSITIONS; ScanTransi++ )
	{
		pTransi = &Seq->Transition[ ScanTransi ];
		if ( pTransi->NumPage==PageNbr )
			DrawSeqTransition( cr, pTransi->PosiX*SeqPxSize-ShiftX, pTransi->PosiY*SeqPxSize-ShiftY, SeqPxSize, pTransi, DrawingOption );
	}
	// draw the comments
	for( ScanSeqComment=0; ScanSeqComment<NBR_SEQ_COMMENTS; ScanSeqComment++ )
	{
		StrSeqComment * pSeqComment = &Seq->SeqComment[ ScanSeqComment ];
		if ( pSeqComment->NumPage==PageNbr )
			DrawSeqComment( cr, pSeqComment->PosiX*SeqPxSize-ShiftX,
							pSeqComment->PosiY*SeqPxSize-ShiftY, SeqPxSize, pSeqComment);
	}
	if ( EditDatas.ModeEdit )
		DrawSequentialCurrentElementEdited( cr, SeqPxSize, ShiftX, ShiftY );
}

void DrawSeqElementForToolBar( cairo_t * cr, int x, int y, int Size, int NumElement )
{
	int SizeDiv2 = Size/2;
	int SizeDiv3 = Size/3;
	int SizeDiv4 = Size/4;
	cairo_set_source_rgb( cr, 0.0, 0.0, 0.0 );
	switch( NumElement )
	{
		case ELE_SEQ_STEP:
		case EDIT_SEQ_INIT_STEP:
			cairo_rectangle(cr,
				x+SizeDiv4, y+SizeDiv4,
				Size-2*SizeDiv4, Size-2*SizeDiv4);
			if( NumElement==EDIT_SEQ_INIT_STEP )
				cairo_rectangle(cr,
					x+SizeDiv4+2, y+SizeDiv4+2,
					Size-2*SizeDiv4-4, Size-2*SizeDiv4-4);
			cairo_stroke( cr );
			break;
		case ELE_SEQ_TRANSITION:
			my_cairo_draw_line(cr,
				x+SizeDiv2,y+SizeDiv4, x+SizeDiv2,y+Size-SizeDiv4);
			my_cairo_draw_line(cr,
				x+SizeDiv3,y+SizeDiv2, x+Size-SizeDiv3,y+SizeDiv2);
			break;
		case EDIT_SEQ_STEP_AND_TRANS:
			cairo_rectangle(cr,
				x+SizeDiv4, y+2,
				Size-2*SizeDiv4, SizeDiv2-2);
			cairo_stroke( cr );
			my_cairo_draw_line(cr,
				x+SizeDiv2,y+SizeDiv2, x+SizeDiv2,y+Size-2);
			my_cairo_draw_line(cr,
				x+SizeDiv3,y+Size-SizeDiv4, x+Size-SizeDiv3,y+Size-SizeDiv4);
			break;
		case EDIT_SEQ_START_MANY_TRANS:
			my_cairo_draw_line(cr,
				x+2,y+SizeDiv4, x+Size-2,y+SizeDiv4);
			my_cairo_draw_line(cr,
				x+SizeDiv4,y+SizeDiv4-4, x+SizeDiv4,y+SizeDiv4+4);
			my_cairo_draw_line(cr,
				x+Size-SizeDiv4,y+SizeDiv4, x+Size-SizeDiv4,y+SizeDiv4+4);
			break;
		case EDIT_SEQ_END_MANY_TRANS:
			my_cairo_draw_line(cr,
				x+2,y+Size-SizeDiv4, x+Size-2,y+Size-SizeDiv4);
			my_cairo_draw_line(cr,
				x+SizeDiv4,y+Size-SizeDiv4-4, x+SizeDiv4,y+Size-SizeDiv4+4);
			my_cairo_draw_line(cr,
				x+Size-SizeDiv4,y+Size-SizeDiv4, x+Size-SizeDiv4,y+Size-SizeDiv4-4);
			break;
		case EDIT_SEQ_START_MANY_STEPS:
			my_cairo_draw_line(cr,
				x+2,y+SizeDiv4, x+Size-2,y+SizeDiv4);
			my_cairo_draw_line(cr,
				x+2,y+SizeDiv4+2, x+Size-2,y+SizeDiv4+2);
			my_cairo_draw_line(cr,
				x+SizeDiv4,y+SizeDiv4-4, x+SizeDiv4,y+SizeDiv4+4);
			my_cairo_draw_line(cr,
				x+Size-SizeDiv4,y+SizeDiv4+2, x+Size-SizeDiv4,y+SizeDiv4+4);
			break;
		case EDIT_SEQ_END_MANY_STEPS:
			my_cairo_draw_line(cr,
				x+2,y+Size-SizeDiv4, x+Size-2,y+Size-SizeDiv4);
			my_cairo_draw_line(cr,
				x+2,y+Size-SizeDiv4-2, x+Size-2,y+Size-SizeDiv4-2);
			my_cairo_draw_line(cr,
				x+SizeDiv4,y+Size-SizeDiv4-4, x+SizeDiv4,y+Size-SizeDiv4+4);
			my_cairo_draw_line(cr,
				x+Size-SizeDiv4,y+Size-SizeDiv4-2, x+Size-SizeDiv4,y+Size-SizeDiv4-4);
			break;
		case EDIT_SEQ_LINK:
			my_cairo_draw_line(cr,
				x+SizeDiv3,y+SizeDiv4, x+Size-SizeDiv3,y+Size-SizeDiv4);
//Cairo			gdk_draw_arc(DrawPixmap, drawing_area->style->black_gc, FALSE/*filled*/,
//				x+SizeDiv3-2,y+SizeDiv4-2, 4, 4, 0/*angle1*/, 23040/*angle2*/);
//			gdk_draw_arc(DrawPixmap, drawing_area->style->black_gc, FALSE/*filled*/,
//				x+Size-SizeDiv3-2,y+Size-SizeDiv4-2, 4, 4, 0/*angle1*/, 23040/*angle2*/);
			break;
		case ELE_SEQ_COMMENT:
			cairo_rectangle(cr,
				x+SizeDiv4, y+SizeDiv4,
				Size-2*SizeDiv4, Size-2*SizeDiv4);
			cairo_stroke( cr );
			DrawPangoText( cr, x, y, Size, Size, "xx" );
			break;
		default:
			DrawCommonElementForToolbar( cr, x, y, Size, NumElement );
			break;
	}
}
