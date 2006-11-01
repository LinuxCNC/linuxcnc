/** This file, 'halsc_disp.c', contains the portion of halscope
    that actually displays waveforms.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/
#ifndef ULAPI
#error This is a user mode component only!
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "../hal_priv.h"	/* private HAL decls */

#include <gtk/gtk.h>
#include "miscgtk.h"		/* generic GTK stuff */
#include "scope_usr.h"		/* scope related declarations */

#define BUFLEN 80		/* length for sprintf buffers */

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

/***********************************************************************
*                   LOCAL FUNCTION PROTOTYPES                          *
************************************************************************/

static void init_display_window(void);
static void clear_display_window(void);
static void draw_grid(void);
static void draw_waveform(int chan_num, int highlight);
static void handle_window_expose(GtkWidget * widget, gpointer data);

/***********************************************************************
*                       PUBLIC FUNCTIONS                               *
************************************************************************/

void init_display(void)
{

    /* allocate a user space buffer */
    ctrl_usr->disp_buf = g_malloc(sizeof(scope_data_t) * ctrl_shm->buf_len);
    if (ctrl_usr->disp_buf == 0) {
	/* malloc failed */
	/* should never get here - gmalloc checks its return value */
	exit(-1);
    }
    /* initialize waveform window */
    init_display_window();

    invalidate_all_channels();
}

void invalidate_channel(int chan)
{
    ctrl_usr->vert.data_offset[chan - 1] = -1;
}

void invalidate_all_channels(void)
{
    int n;

    for (n = 0; n < 16; n++) {
	ctrl_usr->vert.data_offset[n] = -1;
    }
}

void request_display_refresh(int delay)
{
    if (delay > 5) {
	delay = 5;
    }
    if (delay < 1) {
	delay = 1;
    }
    /* request a refresh after 0.2 seconds of idleness */
    ctrl_usr->display_refresh_timer = delay;
}

void refresh_display(void)
{
    int n;
    scope_disp_t *disp;
    scope_vert_t *vert;
    scope_horiz_t *horiz;
    int depth;
    float pixels_per_div, pixels_per_sec, overall_record_length;
    float screen_center_time, screen_start_time, screen_end_time;

    /* cancel any pending refresh request */
    ctrl_usr->display_refresh_timer = 0;
    /* set pointers */
    disp = &(ctrl_usr->disp);
    vert = &(ctrl_usr->vert);
    horiz = &(ctrl_usr->horiz);
    /* get window pointer */
    disp->win = disp->drawing->window;
    if (disp->win == NULL) {
	/* window isn't visible yet, do nothing */
	printf("refresh_display(): win = NULL, bailing!\n");
	return;
    }
    /* create drawing context if needed */
    if (disp->context == NULL) {
	disp->context = gdk_gc_new(disp->win);
    }

    /* get window dimensions */
    gdk_window_get_geometry(disp->win, NULL, NULL, &(disp->width),
	&(disp->height), &depth);
    /* calculate horizontal params that depend on width */
    pixels_per_div = disp->width * 0.1;
    pixels_per_sec = pixels_per_div / horiz->disp_scale;
    disp->pixels_per_sample = pixels_per_sec * horiz->sample_period;
    overall_record_length = horiz->sample_period * ctrl_shm->rec_len;
    screen_center_time = overall_record_length * horiz->pos_setting;
    screen_start_time = screen_center_time - (5.0 * horiz->disp_scale);
    disp->horiz_offset = screen_start_time * pixels_per_sec;
    disp->start_sample = screen_start_time / horiz->sample_period;
    if (disp->start_sample < 0) {
	disp->start_sample = 0;
    }
    screen_end_time = screen_center_time + (5.0 * horiz->disp_scale);
    disp->end_sample = (screen_end_time / horiz->sample_period) + 1;
    if (disp->end_sample > ctrl_shm->rec_len - 1) {
	disp->end_sample = ctrl_shm->rec_len - 1;
    }

    clear_display_window();
    draw_grid();

    /* draw non-highlighted waveforms first */
    for (n = 0; n < 16; n++) {
	if ((vert->chan_enabled[n]) && (vert->data_offset[n] >= 0)
	    && (n + 1 != vert->selected)) {
	    draw_waveform(n + 1, FALSE);
	}
    }
    /* draw highlighted waveform last */
    if ((vert->chan_enabled[vert->selected - 1])
	&& (vert->data_offset[vert->selected - 1] >= 0)) {
	draw_waveform(vert->selected, TRUE);
    }
}

/***********************************************************************
*                       LOCAL FUNCTIONS                                *
************************************************************************/

static gboolean alloc_color(GdkColor * color, GdkColormap * map,
    unsigned char red, unsigned char green, unsigned char blue)
{
    int retval;

    color->red = ((unsigned long) red) << 8;
    color->green = ((unsigned long) green) << 8;
    color->blue = ((unsigned long) blue) << 8;
    color->pixel =
	((unsigned long) red) << 16 | ((unsigned long) green) << 8 |
	((unsigned long) blue);
    retval = gdk_colormap_alloc_color(map, color, FALSE, TRUE);
    if (retval == 0) {
	printf("alloc_color( %d, %d, %d ) failed\n", red, green, blue);
    }
    return retval;
}

#if 0 /* this will be needed if/when I allow user defined colors */
static void free_color(GdkColor * color, GdkColormap * map)
{
    gdk_colormap_free_colors(map, color, 1);
}
#endif

static void init_display_window(void)
{
    scope_disp_t *disp;

    disp = &(ctrl_usr->disp);

    /* allocate a drawing area */
    disp->drawing = gtk_drawing_area_new();
    /* put it into the display window */
    gtk_box_pack_start(GTK_BOX(ctrl_usr->waveform_win), disp->drawing, TRUE,
	TRUE, 0);
    /* hook up a function to handle expose events */
    gtk_signal_connect(GTK_OBJECT(disp->drawing), "expose_event",
	GTK_SIGNAL_FUNC(handle_window_expose), NULL);
    gtk_widget_show(disp->drawing);
    /* get color map */
    disp->map = gtk_widget_get_colormap(disp->drawing);
    /* allocate colors */
    alloc_color(&(disp->color_bg), disp->map, 0, 0, 0);
    alloc_color(&(disp->color_grid), disp->map, 255, 255, 255);
    alloc_color(&(disp->color_normal), disp->map, 255, 0, 0);
    alloc_color(&(disp->color_selected), disp->map, 0, 255, 0);
}

static void handle_window_expose(GtkWidget * widget, gpointer data)
{
    /* we don't want to react immediately - sometime we get a burst of expose 
       events - instead we request a refresh for later */
    request_display_refresh(2);
}

void clear_display_window(void)
{
    scope_disp_t *disp;

    disp = &(ctrl_usr->disp);
    /* set color to draw */
    gdk_gc_set_foreground(disp->context, &(disp->color_bg));
    /* draw a big rectangle to clear the screen */
    gdk_draw_rectangle(disp->win, disp->context, TRUE, 0, 0, disp->width,
	disp->height);
}

void draw_grid(void)
{
    scope_disp_t *disp;
    float xscale, yscale;
    int xmajor, xminor, ymajor, yminor;
    int nx, ny, m;
    float fx, fy;
    int x, y;

    disp = &(ctrl_usr->disp);
    /* set color to draw */
    gdk_gc_set_foreground(disp->context, &(disp->color_grid));
    /* calculate scale factors */
    xscale = disp->width - 1.0;
    yscale = disp->height - 1.0;
    /* calculate grid spacings */
    xmajor = xscale * 0.1;
    if (xmajor >= 40) {
	xminor = 10;
    } else if (xmajor >= 20) {
	xminor = 5;
    } else {
	xminor = xmajor / 4;
    }
    ymajor = yscale * 0.1;
    if (ymajor >= 40) {
	yminor = 10;
    } else if (ymajor >= 20) {
	yminor = 5;
    } else {
	yminor = ymajor / 4;
    }
    /* draw the vertical lines */
    for (nx = 0; nx <= 10; nx++) {
	/* calc the major division x coordinate */
	fx = nx * 0.1;
	for (ny = 0; ny <= 10; ny++) {
	    /* calc the major division y coordinate */
	    fy = ny * 0.1;
	    /* draw the major division point */
	    x = fx * xscale;
	    y = fy * yscale;
	    gdk_draw_point(disp->win, disp->context, x, y);
	    /* draw minor divisions (vertical) */
	    if (ny < 10) {
		for (m = 1; m < yminor; m++) {
		    y = (((0.1 * m) / yminor) + fy) * yscale;
		    gdk_draw_point(disp->win, disp->context, x, y);
		}
	    }
	    /* draw minor divisions (horizontal) */
	    if (nx < 10) {
		y = fy * yscale;
		for (m = 1; m < xminor; m++) {
		    x = (((0.1 * m) / xminor) + fx) * xscale;
		    gdk_draw_point(disp->win, disp->context, x, y);
		}
	    }
	}
    }
}

/* waveform styles: if neither is defined, an intermediate style is used */
// #define DRAW_STEPPED
// #define DRAW_SMOOTH

void draw_waveform(int chan_num, int highlight)
{
    scope_data_t *dptr;
    int start, end, n, sample_len;
    scope_disp_t *disp;
    scope_chan_t *chan;
    double xscale, xoffset;
    double yscale, yfoffset, ypoffset, fy;
    hal_type_t type;
    int x1, y1, x2, y2, miny, maxy, midx;

    disp = &(ctrl_usr->disp);
    chan = &(ctrl_usr->chan[chan_num - 1]);
    /* set color to draw */
    if (highlight) {
	gdk_gc_set_foreground(disp->context, &(disp->color_selected));
    } else {
	gdk_gc_set_foreground(disp->context, &(disp->color_normal));
    }
    /* calculate a bunch of local vars */
    sample_len = ctrl_shm->sample_len;
    xscale = disp->pixels_per_sample;
    xoffset = disp->horiz_offset;
    miny = -disp->height;
    maxy = 2 * disp->height;
    type = chan->data_type;
    yscale = disp->height / (-10.0 * chan->scale);
    yfoffset = chan->vert_offset;
    ypoffset = chan->position * disp->height;
    /* point to first sample in the record for this channel */
    dptr = ctrl_usr->disp_buf + ctrl_usr->vert.data_offset[chan_num - 1];
    /* point to first one that gets displayed */
    start = disp->start_sample;
    end = disp->end_sample;
    n = start;
    dptr += n * sample_len;
    x1 = y1 = 0;
    while (n <= end) {
	/* calc x coordinate of this point */
	x2 = (n * xscale) - xoffset;
	/* calc y coordinate of this point */
	switch (type) {
	case HAL_BIT:
	    if (dptr->d_u8) {
		fy = 1.0;
	    } else {
		fy = 0.0;
	    };
	    break;
	case HAL_FLOAT:
	    fy = dptr->d_float;
	    break;
	case HAL_S8:
	    fy = dptr->d_s8;
	    break;
	case HAL_U8:
	    fy = dptr->d_u8;
	    break;
	case HAL_S16:
	    fy = dptr->d_s16;
	    break;
	case HAL_U16:
	    fy = dptr->d_u16;
	    break;
	case HAL_S32:
	    fy = dptr->d_s32;
	    break;
	case HAL_U32:
	    fy = dptr->d_u32;
	    break;
	default:
	    fy = 0.0;
	    break;
	}
	y2 = ((fy - yfoffset) * yscale) + ypoffset;
	if (y2 < miny) {
	    y2 = miny;
	} else if (y2 > maxy) {
	    y2 = maxy;
	}
	/* don't draw segment ending at first point */
	if (n > start) {
#ifdef DRAW_SMOOTH
	    /* this is a smoothed line display */
	    gdk_draw_line(disp->win, disp->context, x1, y1, x2, y2);
#else
#ifdef DRAW_STEPPED
	    /* this is a stepped one */
	    gdk_draw_line(disp->win, disp->context, x1, y1, x1, y2);
	    gdk_draw_line(disp->win, disp->context, x1, y2, x2, y2);
#else
	    /* this is halfway between the two extremes */
	    midx = (x1 + x2) / 2;
	    gdk_draw_line(disp->win, disp->context, x1, y1, midx, y2);
	    gdk_draw_line(disp->win, disp->context, midx, y2, x2, y2);
#endif
#endif
	}
	/* end of this segment is start of next one */
	x1 = x2;
	y1 = y2;
	/* point to next sample */
	dptr += sample_len;
	n++;
    }
}
