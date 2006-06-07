/** This file, 'halsc_disp.c', contains the portion of halscope
    that actually displays waveforms.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
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

#include <math.h>
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

#if GTK_CHECK_VERSION(2,0,0)
#define DOUBLE_BUFFER
#define MARKUP
#define SCROLL
#endif

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

/***********************************************************************
*                   LOCAL FUNCTION PROTOTYPES                          *
************************************************************************/

static void init_display_window(void);
static void clear_display_window(void);
static void update_readout(void);
static void draw_grid(void);
static void draw_baseline(int chan_num, int highlight);
static void draw_waveform(int chan_num, int highlight);
static void handle_window_expose(GtkWidget * widget, gpointer data);
static int handle_click(GtkWidget *widget, GdkEventButton *event, gpointer data);
static int handle_release(GtkWidget *widget, GdkEventButton *event, gpointer data);
static int handle_motion(GtkWidget *widget, GdkEventButton *event, gpointer data);
#ifdef SCROLL
static int handle_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data);
#endif

/***********************************************************************
*                       PUBLIC FUNCTIONS                               *
************************************************************************/

static int DRAWING = 0;

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
    /* request a refresh after approximately delay/10 seconds of idleness */
    ctrl_usr->display_refresh_timer = delay;
}

static int motion_x = -1;

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

#ifdef DOUBLE_BUFFER
    {
        GdkRectangle rect = {0, 0, disp->width, disp->height};
        GdkRegion *region = gdk_region_rectangle(&rect);
        gdk_window_begin_paint_region(disp->drawing->window, region);
        gdk_region_destroy(region);
    }
#endif
    DRAWING = 1;
    clear_display_window();
    draw_grid();

    /* draw baselines first */
    for (n = 0; n < 16; n++) {
	if ((vert->chan_enabled[n]) && (vert->data_offset[n] >= 0)
	    && (n + 1 != vert->selected)) {
	    draw_baseline(n + 1, FALSE);
	}
    }
    if ((vert->chan_enabled[vert->selected - 1])
            && (vert->data_offset[vert->selected - 1] >= 0)) {
        draw_baseline(vert->selected, TRUE);
    }

    /* draw non-highlighted waveforms next */
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

    update_readout();

#ifdef DOUBLE_BUFFER
    gdk_window_end_paint(disp->drawing->window);
#endif
}

/***********************************************************************
*                       LOCAL FUNCTIONS                                *
************************************************************************/

gboolean alloc_color(GdkColor * color, GdkColormap * map,
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

int normal_colors[16][3] = {
	{204,   0,   0},
	{  0, 204, 204},
	{102, 204,   0},
	{102,   0, 204},
	{204, 153,   0},
	{  0, 204,  51},
	{  0,  51, 204},
	{204,   0, 153},
	{153,  95,  61},
	{141, 153,  61},
	{ 72, 153,  61},
	{ 61, 153, 118},
	{ 61, 118, 153},
	{ 72,  61, 153},
	{141,  61, 153},
	{153,  61,  95},
};

int selected_colors[16][3] = {
	{255, 204, 204},
	{204, 255, 255},
	{229, 255, 204},
	{229, 204, 255},
	{255, 242, 204},
	{204, 255, 216},
	{204, 216, 255},
	{255, 204, 242},
	{229, 186, 160},
	{220, 229, 160},
	{169, 229, 160},
	{160, 229, 203},
	{160, 203, 229},
	{169, 160, 229},
	{220, 160, 229},
	{229, 160, 186},
};



static void init_display_window(void)
{
    scope_disp_t *disp;
    int i;

    disp = &(ctrl_usr->disp);

    /* allocate a drawing area */
    disp->drawing = gtk_drawing_area_new();
    /* put it into the display window */
    gtk_box_pack_start(GTK_BOX(ctrl_usr->waveform_win), disp->drawing, TRUE,
	TRUE, 0);
    /* hook up a function to handle expose events */
    gtk_signal_connect(GTK_OBJECT(disp->drawing), "expose_event",
	GTK_SIGNAL_FUNC(handle_window_expose), NULL);
    gtk_signal_connect(GTK_OBJECT(disp->drawing), "button_release_event",
        GTK_SIGNAL_FUNC(handle_release), NULL);
    gtk_signal_connect(GTK_OBJECT(disp->drawing), "button_press_event",
        GTK_SIGNAL_FUNC(handle_click), NULL);
    gtk_signal_connect(GTK_OBJECT(disp->drawing), "motion_notify_event",
        GTK_SIGNAL_FUNC(handle_motion), NULL);
#if defined(SCROLL)
    gtk_signal_connect(GTK_OBJECT(disp->drawing), "scroll_event",
                GTK_SIGNAL_FUNC(handle_scroll), NULL);
    gtk_widget_add_events(GTK_WIDGET(disp->drawing), GDK_SCROLL_MASK);
#endif
    gtk_widget_add_events(GTK_WIDGET(disp->drawing),
            GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
            | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK
            | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK
            | GDK_BUTTON2_MOTION_MASK);
    gtk_widget_show(disp->drawing);
    /* get color map */
    disp->map = gtk_widget_get_colormap(disp->drawing);
    /* allocate colors */
    alloc_color(&(disp->color_bg), disp->map, 0, 0, 0);
    alloc_color(&(disp->color_grid), disp->map, 255, 255, 255);
    alloc_color(&disp->color_baseline, disp->map, 128, 128, 128);
    for(i = 0; i<16; i++) {
        alloc_color(&(disp->color_normal[i]), disp->map, normal_colors[i][0], normal_colors[i][1], normal_colors[i][2]);
        alloc_color(&(disp->color_selected[i]), disp->map, selected_colors[i][0], selected_colors[i][1], selected_colors[i][2]);
    }

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

static int select_x, select_y, target;
static double min_dist;
static int select_trace(int x, int y) {
    int n;
    scope_disp_t *disp = &(ctrl_usr->disp);

    min_dist = hypot(disp->width, disp->height) / 100.;
    if(min_dist < 5) min_dist = 5;
    target = -1;

    DRAWING = 0;
    select_x = x;
    select_y = y;
    for(n=0; n<16; n++) {
        scope_vert_t *vert = &(ctrl_usr->vert);
        if((vert->chan_enabled[n]) && (vert->data_offset[n] >= 0)) {
            draw_baseline(n+1, FALSE);
            draw_waveform(n+1, FALSE);
        }
    }
    return target;
}

static int handle_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    return 1;
}

static void change_zoom(int dir, int x) {
    scope_horiz_t *horiz = &(ctrl_usr->horiz);
    scope_disp_t *disp = &(ctrl_usr->disp);

    double old_pixels_per_sample, pixels_per_div,
           pixels_per_sec, new_pixels_per_sample, old_fraction, new_fraction,
           overall_record_length;

    old_pixels_per_sample = disp->pixels_per_sample;
    overall_record_length = horiz->sample_period * ctrl_shm->rec_len;

    set_horiz_zoom(horiz->zoom_setting + dir);

    /* calculate horizontal params that depend on width */
    pixels_per_div = disp->width * 0.1;
    pixels_per_sec = pixels_per_div / horiz->disp_scale;
    disp->pixels_per_sample = new_pixels_per_sample = 
        pixels_per_sec * horiz->sample_period;

    // how many samples away from the center of the window is this
    // pixel?
    old_fraction = (x - disp->width / 2) / old_pixels_per_sample / ctrl_shm->rec_len;
    new_fraction = (x - disp->width / 2) / new_pixels_per_sample / ctrl_shm->rec_len;
    // how many samples per pixel 
    printf("zoom: %f %f  %f\n", old_fraction, horiz->pos_setting, new_fraction);
    set_horiz_pos( horiz->pos_setting - new_fraction + old_fraction );
}

static int handle_click(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    scope_vert_t *vert = &(ctrl_usr->vert);
    if(event->button == 4) { // zoom in
        change_zoom(1, event->x);
    } else if(event->button == 5) { // zoom out
        change_zoom(-1, event->x);
    } else {
        int z = select_trace(event->x, event->y);
        if(z != -1 && z != vert->selected) {
            vert->selected = z;
            channel_changed();
        }
    }
    return 1;
}

static int get_sample_info(int chan_num, int x, double *t, double *v) {
    scope_horiz_t *horiz = &(ctrl_usr->horiz);
    scope_vert_t *vert = &(ctrl_usr->vert);
    scope_data_t *dptr;
    int start, end, n, sample_len;
    scope_disp_t *disp;
    scope_chan_t *chan;
    double xscale, xoffset;
    double fy;
    hal_type_t type;

    disp = &(ctrl_usr->disp);
    chan = &(ctrl_usr->chan[chan_num - 1]);
    /* calculate a bunch of local vars */
    sample_len = ctrl_shm->sample_len;
    xscale = disp->pixels_per_sample;
    xoffset = disp->horiz_offset;
    type = chan->data_type;
    /* point to first sample in the record for this channel */
    dptr = ctrl_usr->disp_buf + ctrl_usr->vert.data_offset[chan_num - 1];
    /* point to first one that gets displayed */
    start = disp->start_sample;
    end = disp->end_sample;

    n = (x + xoffset + xscale/2) / xscale;
    *t = (n - ctrl_shm->pre_trig) / horiz->sample_period;

    if(!vert->chan_enabled[chan_num - 1]
		    || vert->data_offset[chan_num - 1] < 0) {
	    return -1;
    }
    if(n < 0 || n > ctrl_shm->rec_len) return 0;
    dptr += n * sample_len;

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
    *v = fy;
    return 1;

}

#ifdef SCROLL
static int handle_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data) {
    change_zoom(event->direction ? -1 : 1, event->x);
    return TRUE;
}
#endif

static void drag(int dx) {
    scope_disp_t *disp = &(ctrl_usr->disp);
    scope_horiz_t *horiz = &(ctrl_usr->horiz);
    float dt = (dx / disp->pixels_per_sample) / ctrl_shm->rec_len;
    set_horiz_pos(horiz->pos_setting + 5 * dt);
    refresh_display();
}

static int handle_motion(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    scope_disp_t *disp = &(ctrl_usr->disp);
    GdkModifierType mod;

    if(event) {
            int x, y;
	    gdk_window_get_pointer(disp->drawing->window, &x, &y, &mod);
            if(mod & GDK_BUTTON2_MASK) {
                drag(motion_x - x);
                motion_x = x;
                return TRUE;
            }
            motion_x = x;
            refresh_display();
            return TRUE;
    }
    return TRUE;
}

static void update_readout(void) {
    scope_vert_t *vert = &(ctrl_usr->vert);
    char tip[512];
    GdkRectangle r = {vert->readout_label->allocation.x,
            vert->readout_label->allocation.y,
            vert->readout_label->allocation.width,
            vert->readout_label->allocation.height};
#ifdef MARKUP
    char tip2[sizeof(tip) + 10];
#endif
    if(vert->selected != -1) {
        double t, v;
        int result = get_sample_info(vert->selected, motion_x, &t, &v);
        t = t * 1e-6;
        if(result) { 
            snprintf(tip, sizeof(tip),
                    "f(% 8.5f) = % 8.3f", t, v);
        } else { 
	    strcpy(tip, "");
        }

    } else {
            strcpy(tip, "");
    }

#ifdef MARKUP
    snprintf(tip2, sizeof(tip2), "<tt>%s</tt>", tip);
    gtk_label_set_markup(GTK_LABEL(vert->readout_label), tip2);
#else
    gtk_label_set_text(GTK_LABEL(vert->readout_label), tip);
#endif

    gtk_widget_draw(vert->readout_label, &r);

}

struct pt { double x, y; };
static double dot(struct pt *a, struct pt *b) {
    return a->x * b->x + a->y * b->y;
}

static double mag(struct pt *p) {
    return hypot(p->x, p->y);
}

static double distance_point_line(x, y, x1, y1, x2, y2) {
    struct pt M = {x2-x1, x2-y1},
           Q = {x-x1, y-y1},
           R;

    double t0 = dot(&M, &Q) / dot(&M, &M);
    if(t0 < 0) t0 = 0;
    if(t0 > 1) t0 = 1;
    R.x = x - x1 + t0 * M.x;
    R.y = y - y1 + t0 * M.y;
    return mag(&R);
}

void line(int chan_num, int x1, int y1, int x2, int y2) {
    scope_disp_t *disp = &(ctrl_usr->disp);
    if(DRAWING) {
        gdk_draw_line(disp->win, disp->context, x1, y1, x2, y2);
    } else {
        double dist = distance_point_line(select_x, select_y, x1, y1, x2, y2);
        if(dist < min_dist) {
            min_dist = dist;
            target = chan_num;
        }
    }
}

void draw_baseline(int chan_num, int highlight) {
    scope_disp_t *disp = &(ctrl_usr->disp);
    scope_chan_t *chan = &(ctrl_usr->chan[chan_num - 1]);
    double yfoffset = chan->vert_offset;
    double ypoffset = chan->position * disp->height;
    double yscale = disp->height / (-10.0 * chan->scale);
    int y1 = -yfoffset * yscale + ypoffset;;
    if(highlight) {
        gdk_gc_set_foreground(disp->context, &(disp->color_grid));
    } else {
        gdk_gc_set_foreground(disp->context, &(disp->color_baseline));
    }
    line(chan_num, 0, y1, disp->width, y1);
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
    int first=1;

    disp = &(ctrl_usr->disp);
    chan = &(ctrl_usr->chan[chan_num - 1]);
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


    /* set color to draw */
    if (highlight) {
	gdk_gc_set_foreground(disp->context, &(disp->color_selected[chan_num-1]));
    } else {
	gdk_gc_set_foreground(disp->context, &(disp->color_normal[chan_num-1]));
    }

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
            line(chan_num, x1, y1, x2, y2);
#else
#ifdef DRAW_STEPPED
	    /* this is a stepped one */
            line(chan_num, x1, y1, x1, y2);
            line(chan_num, x1, y2, x2, y2)
#else
	    /* this is halfway between the two extremes */
	    midx = (x1 + x2) / 2;
	    line(chan_num, x1, y1, midx, y2);
	    line(chan_num, midx, y2, x2, y2);
#endif
#endif
	    if(first && highlight && DRAWING && x2 >= motion_x) {
		    first = 0;
		    gdk_draw_arc(disp->win, disp->context, TRUE,
				x2-3, y2-3, 7, 7, 0, 360*64);
	    }
	}
	/* end of this segment is start of next one */
	x1 = x2;
	y1 = y2;
	/* point to next sample */
	dptr += sample_len;
	n++;
    }
}

// vim:sts=4:sw=4:et
