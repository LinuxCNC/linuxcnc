/* ClassicLadder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* June 2006 */
/* ---------------------------------- */
/* Printer output (using Gnome-print) */
/* ---------------------------------- */

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

#include <locale.h>
#include <libintl.h>
#define _(x) gettext(x)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libgnomeprintui/gnome-print-dialog.h>

#include "classicladder.h"
#include "global.h"
#include "drawing.h"
#ifdef SEQUENTIAL_SUPPORT
#include "drawing_sequential.h"
#endif
#include "print_gnome.h"

/* Notes : */
/* - Troubles (freeze for gnome_print_dialog_new) if gdk_threads_enter() & gdk_threads_leave() between gtk_main()
*/

GnomePrintContext *gpc = NULL;
GnomePrintJob *job = NULL;

void DrawPrint( GnomePrintContext *gpc )
{
	int bl_width = 32;
	int bl_height = 32;
	int seq_size = 32;
	int the_width = RUNG_WIDTH*bl_width;
	int the_height = RUNG_HEIGHT*bl_height;
	int iCurrentLanguage = SectionArray[ InfosGene->CurrentSection ].Language;
	GdkPixmap *pixmap_for_print;
#ifdef SEQUENTIAL_SUPPORT
	if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
	{
		the_width = SEQ_PAGE_WIDTH*seq_size;
		the_height = SEQ_PAGE_HEIGHT*seq_size;
	}
#endif

	pixmap_for_print = gdk_pixmap_new( drawing_area->window/*drawable*/,                                          the_width, the_height, -1/*depth*/ );
	if ( pixmap_for_print )
	{

		char TheEnd = FALSE;
		char NewPage = TRUE;
		int SizePageOffset = 800;
		int ScanRung = InfosGene->FirstRung;
		int PageNumber = 1;

		do
		{
			char Buffer[ LGT_LABEL+LGT_COMMENT+20 ];
			GdkPixbuf* pixbuf_for_print;

			if ( NewPage==TRUE )
			{
				gnome_print_beginpage( gpc, (guchar *)"1" );
				NewPage = FALSE;
				SizePageOffset = 800;

				sprintf(Buffer, CL_PRODUCT_NAME CL_RELEASE_VER_STRING ".  Section:%s  -  Page:%d", SectionArray[ InfosGene->CurrentSection ].Name, PageNumber );
				gnome_print_moveto( gpc, 50, 20 );
				gnome_print_show( gpc, (guchar *)Buffer );

			}

			gdk_draw_rectangle (pixmap_for_print, drawing_area->style->white_gc, TRUE, 0, 0, the_width, the_height);

			if ( iCurrentLanguage==SECTION_IN_LADDER )
				DrawRung( pixmap_for_print, &RungArray[ ScanRung ], 0/*PosiY*/, bl_width, bl_height, DRAW_FOR_PRINT );
#ifdef SEQUENTIAL_SUPPORT
			if ( iCurrentLanguage==SECTION_IN_SEQUENTIAL )
			{
				DrawSequentialPage( pixmap_for_print, SectionArray[ InfosGene->CurrentSection ].SequentialPage, DRAW_FOR_PRINT );
				TheEnd = TRUE;
			}
#endif

			pixbuf_for_print = gdk_pixbuf_get_from_drawable( NULL /*GdkPixbuf *dest*/,
                                             pixmap_for_print,
                                             NULL /*cmap*/,
                                             0 /*src_x*/,
                                             0 /*src_y*/,
                                             0 /*dest_x*/,
                                             0 /*dest_y*/,
                                             the_width,
                                             the_height);
			if ( pixbuf_for_print )
			{

				guchar * raw_image;
				gint rowstride;
				if ( iCurrentLanguage==SECTION_IN_LADDER )
				{
					char BuffFormat[10] = "%s (%s)";
					if ( RungArray[ ScanRung ].Comment[0]=='\0' )
						strcpy( BuffFormat, "%s %s" );
					sprintf( Buffer, BuffFormat, RungArray[ ScanRung ].Label, RungArray[ ScanRung ].Comment );
					gnome_print_moveto( gpc, 50, SizePageOffset );
					SizePageOffset = SizePageOffset-20;
					gnome_print_show( gpc, (guchar *)Buffer );
				}

				raw_image = gdk_pixbuf_get_pixels( pixbuf_for_print );
				rowstride = gdk_pixbuf_get_rowstride( pixbuf_for_print );

				gnome_print_gsave( gpc );
				gnome_print_translate( gpc, 20, SizePageOffset-the_height+15 );
				gnome_print_scale( gpc, the_width, the_height );
				gnome_print_rgbimage( gpc, raw_image, the_width, the_height, rowstride);
				gnome_print_grestore( gpc );
				SizePageOffset = SizePageOffset-the_height-20;
				g_object_unref( pixbuf_for_print );

				if ( iCurrentLanguage==SECTION_IN_LADDER )
				{
					if ( ScanRung!=InfosGene->LastRung )
						ScanRung = RungArray[ ScanRung ].NextRung;
					else
						TheEnd = TRUE;
				}

				if ( SizePageOffset<200 || TheEnd )
				{
					NewPage = TRUE;
					PageNumber++;
					gnome_print_showpage ( gpc );
				}
			}
			else
			{
				printf( _("Failed to create pixbuf_for_print\n") );
			}
		}
		while( !TheEnd );

		gdk_pixmap_unref( pixmap_for_print );

	}
	else
	{
		printf( _("Failed to create pixmap_for_print\n") );
	}
}

void PrintInit( )
{
	if ( job==NULL )
		job = gnome_print_job_new( NULL );
	if ( gpc==NULL )
		gpc = gnome_print_job_get_context( job );
}
void PrintFree( )
{
	if ( gpc )
	{
		g_object_unref( gpc );
		gpc = NULL;
	}
	if ( job )
	{
		g_object_unref( job );
		job = NULL;
	}
}

void PrintPreviewGnome( void )
{
	GtkWidget *preview;

	PrintInit( );

	DrawPrint(gpc);	
	gnome_print_job_close (job);

	preview = gnome_print_job_preview_new (job, (guchar*)_("classicladder-preview"));
//	g_signal_connect (G_OBJECT (preview), "unrealize",
//			  G_CALLBACK (gtk_main_quit), NULL);
//    g_signal_connect (G_OBJECT (preview), "unrealize",
//                               GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT(preview));

	gtk_window_set_modal( GTK_WINDOW(preview), TRUE );
	gtk_widget_show (preview);

	PrintFree( );
}

void PrintGnome( void )
{
	GtkWidget *dialog;
	gint ret;

	PrintInit( );

	dialog = gnome_print_dialog_new( job, (guchar*)_("ClassicLadder"), 0 );
	ret = gtk_dialog_run( GTK_DIALOG( dialog ) );
	gtk_widget_destroy( dialog );

	switch( ret )
	{
		case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
			DrawPrint(gpc);
gnome_print_job_close (job);
			gnome_print_job_print( job );
			break;

		case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
			PrintPreviewGnome();
			break;
	}

	PrintFree( );
}

