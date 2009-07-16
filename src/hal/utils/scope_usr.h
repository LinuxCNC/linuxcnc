#ifndef HALSC_USR_H
#define HALSC_USR_H
/** This file, 'scope_usr.h', contains declarations used by
    'halscope.c' and other source files to implement the user
    space portion of the HAL oscilloscope.  Other declarations
    used by both user and realtime code are in 'halsc_shm.h', and
    those used only by realtime code are in 'halsc_rt.h'.
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

/* import the shared declarations */
#include "scope_shm.h"

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
    double sample_period;	/* sample period as a double */
    double disp_scale;		/* display scale (sec/div) */
    int zoom_setting;		/* setting of zoom slider (1-9) */
    double pos_setting;		/* setting of position slider (0.0-1.0) */
    long x0;
    /* widgets for main window */
    GtkWidget *disp_area;
    GdkGC *disp_context;
    GtkWidget *state_label;
    GtkWidget *record_button;
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
    int data_source_type;	/* 0 = pin, 1 = signal, 2 = param,
				   -1 = no source assigned */
    int data_source;		/* points to pin/param/signal struct */
    char *name;			/* name of pin/sig/parameter */
    hal_type_t data_type;	/* data type */
    int data_len;		/* data length */
    double vert_offset;		/* offset to be applied */
    int ac_offset;              /* TRUE if the signal should be AC-coupled */
    int scale_index;		/* scaling (slider setting) */
    int max_index;		/* limits of scale slider */
    int min_index;
    double scale;		/* scaling (units/div) */
    double position;		/* vertical pos (0.0-1.0) */
} scope_chan_t;

/* this struct holds control data related to vertical control */
/* it lives in user space (as part of the master control struct) */

typedef struct {
    /* general data */
    int chan_enabled[16];	/* chans user wants to display */
    int data_offset[16];	/* offset within sample, -1 if no data */
    int selected;		/* channel user has selected */
    /* widgets for chan sel window */
    GtkWidget *chan_sel_buttons[16];
    /* widgets for chan info window */
    GtkWidget *chan_num_label;
    GtkWidget *source_name_label;
    GtkWidget *source_name_button;
    /* widgets for vert info window */
    GtkWidget *scale_slider;
    GtkObject *scale_adj;
    GtkWidget *scale_label;
    GtkWidget *pos_slider;
    GtkObject *pos_adj;
    GtkWidget *offset_button;
    GtkWidget *offset_label;
    GtkWidget *readout_label;
    /* widgets for offset dialog */
    GtkWidget *offset_entry;
    GtkWidget *offset_ac;
    /* widgets for source selection dialog */
    GtkWidget *lists[3];	/* lists for pins, signals, and params */
    GtkWidget *windows[3];	/* scrolled windows for above lists */
    GtkAdjustment *adjs[3];	/* scrollbars associated with above */
} scope_vert_t;

/* this struct holds control data related to triggering */
/* it lives in user space (as part of the master control struct) */

typedef struct {
    /* general data */
    double position;		/* horiz position of trigger (0.0-1.0) */
    double level;		/* setting of level slider (0.0-1.0) */
    /* widgets for trigger mode window */
    GtkWidget *normal_button;
    GtkWidget *auto_button;
    GtkWidget *force_button;
    /* widgets for trigger info window */
    GtkWidget *source_button;
    GtkWidget *source_label;
    GtkWidget *edge_button;
    GtkWidget *edge_label;
    GtkWidget *level_slider;
    GtkObject *level_adj;
    GtkWidget *level_label;
    GtkWidget *pos_slider;
    GtkObject *pos_adj;
} scope_trig_t;



/* this struct holds control data related to the display */
/* it lives in user space (as part of the master control struct) */

typedef struct {
    /* general data */
    int width;			/* height in pixels */
    int height;			/* width in pixels */
    double pixels_per_sample;	/* horizontal scaling */
    double horiz_offset;		/* offset in pixels */
    int start_sample;		/* first displayable sample */
    int end_sample;		/* last displayable sample */
    /* widgets */
    GtkWidget *drawing;		/* drawing area for display */
    GtkTooltips *tip;		/* drawing area for display */
    /* drawing objects (GDK) */
    GdkDrawable *win;		/* the window */
    GdkColormap *map;		/* the colormap for the window */
    GdkColor color_bg;		/* background color */
    GdkColor color_grid;	/* the grid color */
    GdkColor color_normal[16];	/* the color for normal waveforms */
    GdkColor color_selected[16];	/* the color for selected waveforms */
    GdkColor color_baseline;    /* The baseline color */

    GdkGC *context;		/* graphics context for drawing */
    int selected_part;
} scope_disp_t;

/* this struct holds data relating to logging */ 

typedef enum { INTERLACED, NOT_INTERLACED } log_order_t;
typedef enum { OVERWRITE, APPEND } log_append_t;
typedef struct {
	/* logging preferences */
	log_order_t order; /* order that fields are written */
	int auto_save; /* save log every trigger */
	char *filename, *default_filename;
	log_append_t append;
	GtkWidget *log_win;
	GtkWidget *log_prefs_button;
	GtkWidget *log_prefs_label;
} scope_log_t;

/* this is the master user space control structure */

typedef enum { STOP = 0, NORMAL, SINGLE, ROLL } scope_run_mode_t;

typedef struct {
    /* general data */
    scope_data_t *buffer;	/* ptr to shmem buffer (user mapping) */
    scope_data_t *disp_buf;	/* ptr to user buffer for display */
    int samples;		/* number of samples in display buffer */
    int display_refresh_timer;	/* flag for display refresh */
    scope_run_mode_t run_mode;	/* current run mode */
    scope_run_mode_t old_run_mode;	/* run mode to restore*/
    int pending_restart;        /* nonzero if run mode to be restored */
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
    /* subsection control data */
    scope_chan_t chan[16];	/* channel specific data */
    scope_horiz_t horiz;	/* horizontal control data */
    scope_vert_t vert;		/* vertical control data */
    scope_trig_t trig;		/* triggering data */
    scope_disp_t disp;		/* display data */
	scope_log_t log;  		/* logging preferences */
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
void init_trig(void);
void init_display(void);

void handle_watchdog_timeout(void);
void refresh_state_info(void);
void capture_complete(void);
void capture_cont(void);
void start_capture(void);
void request_display_refresh(int delay);
void refresh_display(void);
void refresh_trigger(void);
void invalidate_channel(int chan);
void invalidate_all_channels(void);
void channel_changed(void);


void format_signal_value(char *buf, int buflen, double value);

int read_config_file (char *filename);
void write_config_file (char *filename);
void write_horiz_config(FILE *fp);
void write_vert_config(FILE *fp);
void write_trig_config(FILE *fp);
void write_log_file (char *filename);
void write_sample(FILE *fp, char *label, scope_data_t *dptr, hal_type_t type);

/* the following functions set various parameters, they are normally
   called by the GUI, but can also be called by code reading a file
   that contains a saved front panel setup
*/

int set_sample_thread(char *name);
int set_rec_len(int setting);
int set_horiz_mult(int setting);
int set_horiz_zoom(int setting);
int set_horiz_pos(double setting);
int set_active_channel(int chan_num);
int set_channel_source(int chan, int type, char *name);
int set_channel_off(int chan_num);
int set_vert_scale(int setting);
void format_scale_value(char *buf, int buflen, double value);
int set_vert_pos(double setting);
int set_vert_offset(double setting, int ac_coupled);
int set_trigger_source(int chan);
int set_trigger_level(double setting);
int set_trigger_pos(double setting);
int set_trigger_polarity(int setting);
int set_trigger_mode(int mode);
int set_run_mode(int mode);
void prepare_scope_restart(void);
void log_popup(int);
#endif /* HALSC_USR_H */
