#ifndef HALSC_USR_H
#define HALSC_USR_H
/** This file, 'halsc_usr.h', contains declarations used by
    'halscope.c' and other source files to implement the user
    space portion of the HAL oscilliscope.  Other declarations
    used by both user and realtime code are in 'halsc_shm.h', and
    those used only by realtime code are in 'halsc_rt.h'.
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

#if ( !defined ULAPI )
#error This file needs ULAPI!
#endif

/* import the shared declarations */
#include "halsc_shm.h"

/***********************************************************************
*                         TYPEDEFS AND DEFINES                         *
************************************************************************/

/* this struct holds control data related to horizontal control */
/* it lives in user space (as part of the master control struct */

typedef struct {
    /* general data */
    gchar *thread_name;		/* name of thread that does sampling */
    long thread_period_ns;	/* period of thread in nano-secs */
    long sample_period_ns;	/* sample period in nano-secs */
    float sample_period;	/* sample period as a float */
    float disp_scale;		/* display scale (sec/div) */
    int zoom_setting;		/* setting of zoom slider (1-9) */
    float pos_setting;		/* setting of position slider (0.0-1.0) */
    /* widgets for main window */
    GtkWidget *disp_area;
    GdkGC *disp_context;
    GtkWidget *state_label;
    GtkWidget *record_label;
    GtkWidget *zoom_slider;
    GtkObject *zoom_adj;
    GtkWidget *pos_slider;
    GtkObject *pos_adj;
    GtkWidget *scale_label;
    /* widgets for thread selection dialog */
    GtkWidget *thread_list;
    GtkWidget *thread_name_label;
    GtkWidget *sample_rate_label;
    GtkWidget *sample_period_label;
    GtkObject *mult_adj;
    GtkWidget *mult_spinbutton;
} scope_horiz_t;

/* this struct holds control data related to a single channel */
/* it lives in user space (as part of the master control struct) */

typedef struct {
    char *name;			/* name of pin/sig/parameter */
    hal_type_t data_type;	/* data type */
    void *data_addr;		/* data address (user mapping) */
    int data_len;		/* data length */
    float vert_offset;		/* offset to be applied */
    int scale_index;		/* scaling (slider setting) */
    float scale;		/* scaling (units/div) */
    float position;		/* vertical pos (0.0-1.0) */
} scope_chan_t;

/* this struct holds control data related to vertical control */
/* it lives in user space (as part of the master control struct) */

typedef struct {
    /* general data */
    int chan_enabled[16];	/* chans user wants to display */
    int data_offset[16];	/* offset within sample, -1 if no data */
    int selected;		/* channel user has selected */
    /* widgets for main window */
    GtkWidget *chan_sel_buttons[16];
    GtkWidget *chan_num_label;
    GtkWidget *source_name_label;
    GtkWidget *scale_slider;
    GtkObject *scale_adj;
    GtkWidget *scale_label;
    GtkWidget *pos_slider;
    GtkObject *pos_adj;
    GtkWidget *offset_entry;
    GtkWidget *offset_spinbutton;
    GtkObject *offset_adj;

    /* widgets for source selection dialog */
    GtkWidget *lists[3];	/* lists for pins, signals, and params */
} scope_vert_t;

/* this struct holds control data related to the display */
/* it lives in user space (as part of the master control struct) */

typedef struct {
    /* general data */
    int width;			/* height in pixels */
    int height;			/* width in pixels */
    float pixels_per_sample;	/* horizontal scaling */
    float horiz_offset;		/* offset in pixels */
    int start_sample;		/* first displayable sample */
    int end_sample;		/* last displayable sample */
    /* widgets */
    GtkWidget *drawing;		/* drawing area for display */
    /* drawing objects (GDK) */
    GdkDrawable *win;		/* the window */
    GdkColormap *map;		/* the colormap for the window */
    GdkColor color_bg;		/* background color */
    GdkColor color_grid;	/* the grid color */
    GdkColor color_normal;	/* the color for normal waveforms */
    GdkColor color_selected;	/* the color for selected waveforms */

    GdkGC *context;		/* graphics context for drawing */
} scope_disp_t;

/* this is the master user space control structure */

typedef enum { STOP = 0, NORMAL, SINGLE } scope_run_mode_t;

typedef struct {
    /* general data */
    scope_data_t *buffer;	/* ptr to shmem buffer (user mapping) */
    scope_data_t *disp_buf;	/* ptr to user buffer for display */
    int samples;		/* number of samples in display buffer */
    int display_refresh_timer;	/* flag for display refresh */
    scope_run_mode_t run_mode;	/* current run mode */
    /* top level windows */
    GtkWidget *main_win;
    GtkWidget *horiz_info_win;
    GtkWidget *chan_sel_win;
    GtkWidget *chan_info_win;
    GtkWidget *vert_info_win;
    GtkWidget *waveform_win;
    GtkWidget *run_mode_win;
    GtkWidget *trig_info_win;
    GtkWidget *trig_mode_win;
    /* top level controls */
    GtkWidget *rm_normal_button;
    GtkWidget *rm_single_button;
    GtkWidget *rm_roll_button;
    GtkWidget *rm_stop_button;
    GtkWidget *tm_normal_button;
    GtkWidget *tm_auto_button;
    GtkWidget *tm_force_button;
    /* subsection control data */
    scope_chan_t chan[16];	/* channel specific data */
    scope_horiz_t horiz;	/* horizontal control data */
    scope_vert_t vert;		/* vertical control data */
    scope_disp_t disp;		/* display data */
} scope_usr_control_t;

/***********************************************************************
*                              GLOBALS                                 *
************************************************************************/

extern scope_usr_control_t *ctrl_usr;	/* main user control structure */
extern scope_shm_control_t *ctrl_shm;	/* shared mem control struct */

/***********************************************************************
*                          FUNCTIONS                                   *
************************************************************************/

void init_horiz(void);
void init_vert(void);
void init_display(void);

void handle_watchdog_timeout(void);
void refresh_state_info(void);
void capture_complete(void);
void start_capture(void);
void request_display_refresh(int delay);
void refresh_display(void);
void invalidate_channel(int chan);
void invalidate_all_channels(void);

#endif /* HALSC_USR_H */
