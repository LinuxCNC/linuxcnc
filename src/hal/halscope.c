/** This file, 'halscope.c', is a GUI program that together with
    'halscope_rt.c' serves as an oscilliscope to examine HAL pins,
    signals, and parameters.  It is a user space component and
    uses GTK 1.2 for the GUI code.
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

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private API decls */

#include <gtk/gtk.h>
#include "miscgtk.h"		/* generic GTK stuff */
#include "halsc_usr.h"		/* scope related declarations */

/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

scope_usr_control_t *ctrl_usr;	/* ptr to main user control structure */
scope_shm_control_t *ctrl_shm;	/* ptr to shared mem control struct */

/***********************************************************************
*                         LOCAL VARIABLES                              *
************************************************************************/

static int comp_id;		/* component ID */
static int shm_id;		/* shared memory ID */
static scope_usr_control_t ctrl_struct;	/* scope control structure */

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

/* init functions */
static void init_usr_control_struct(void *shmem);
static void init_shared_control_struct(void);

static void define_scope_windows(void);
static void init_run_mode_window(void);
static void init_trigger_mode_window(void);

/* callback functions */
static void exit_from_hal(void);
static int heartbeat(gpointer data);
static void rm_normal_button_clicked(GtkWidget * widget, gpointer * gdata);
static void rm_single_button_clicked(GtkWidget * widget, gpointer * gdata);
static void rm_roll_button_clicked(GtkWidget * widget, gpointer * gdata);
static void rm_stop_button_clicked(GtkWidget * widget, gpointer * gdata);
static void tm_auto_button_clicked(GtkWidget * widget, gpointer * gdata);
static void tm_normal_button_clicked(GtkWidget * widget, gpointer * gdata);
static void tm_force_button_clicked(GtkWidget * widget, gpointer * gdata);

/***********************************************************************
*                        MAIN() FUNCTION                               *
************************************************************************/

int main(int argc, gchar * argv[])
{
    int retval;
    void *shm_base;

    /* process and remove any GTK specific command line args */
    gtk_init(&argc, &argv);
    /* process halscope command line args (if any) here */

    /* connect to the HAL */
    comp_id = hal_init("scope_gui");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "SCOPE: ERROR: hal_init() failed\n");
	return -1;
    }
    /* set up a shared memory region for the scope data */
    shm_id = rtapi_shmem_new(SCOPE_SHM_KEY, comp_id, SCOPE_SHM_SIZE);
    if (shm_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SCOPE: ERROR: failed to get shared memory\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = rtapi_shmem_getptr(shm_id, &shm_base);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SCOPE: ERROR: failed to map shared memory\n");
	rtapi_shmem_delete(shm_id, comp_id);
	hal_exit(comp_id);
	return -1;
    }
    /* register an exit function to cleanup and disconnect from the HAL */
    g_atexit(exit_from_hal);

    /* init control structure */
    ctrl_usr = &ctrl_struct;
    init_usr_control_struct(shm_base);

    /* init watchdog */
    ctrl_shm->watchdog = 10;
    /* set main window */
    define_scope_windows();
    /* this makes the application exit when the window is closed */
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->main_win), "destroy",
	GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
    /* define menu windows */
    /* do next level of init */
    init_horiz();
    init_vert();
    init_run_mode_window();
    init_trigger_mode_window();
    init_display();

    /* The interface is completely set up so we show the window and enter the
       gtk_main loop. */
    gtk_widget_show(ctrl_usr->main_win);
    /* arrange for periodic call of heartbeat() */
    gtk_timeout_add(100, heartbeat, NULL);
    /* main loop */
    gtk_main();

    return (0);
}

/* This function is called approximately 10 times per second.
   It is responsible for updating anything that is supposed to be
   "live", such as the state display.  It also watches for asynchronous
   events that need to be dealt with, such as loss of the realtime
   component of the scope.
*/

static int heartbeat(gpointer data)
{
    refresh_state_info();
    /* check watchdog */
    if (ctrl_shm->watchdog < 10) {
	ctrl_shm->watchdog++;
    } else {
	handle_watchdog_timeout();
    }
    if (ctrl_usr->display_refresh_timer > 0) {
	/* decrement timer, did it time out? */
	if (--ctrl_usr->display_refresh_timer == 0) {
	    /* yes, refresh the display */
	    refresh_display();
	}
    }
    if (ctrl_shm->state == DONE) {
	capture_complete();
    }
    return 1;
}

void start_capture(void)
{
    int n, mask;
    scope_chan_t *chan;

    /* FIXME temporary, remove after trigger pos is working */
    ctrl_shm->pre_trig = ctrl_shm->rec_len / 2;
    /* end of FIXME */

    if (ctrl_shm->state != IDLE) {
	/* already running! */
	return;
    }
    mask = 1;
    for (n = 0; n < 16; n++) {
	chan = &(ctrl_usr->chan[n]);
	ctrl_shm->data_offset[n] = SHMOFF(chan->data_addr);
	ctrl_shm->data_type[n] = chan->data_type;
	if (ctrl_usr->vert.chan_enabled[n]) {
	    ctrl_shm->data_len[n] = chan->data_len;
	} else {
	    ctrl_shm->data_len[n] = 0;
	}
	mask <<= 1;
    }
    ctrl_shm->state = INIT;
}

void capture_complete(void)
{
    int n, offs;
    scope_data_t *src, *dst, *src_end;
    int samp_len, samp_size;

    offs = 0;
    for (n = 0; n < 16; n++) {
	if (ctrl_shm->data_len[n] > 0) {
	    /* this channel has valid data */
	    ctrl_usr->vert.data_offset[n] = offs;
	    offs++;
	} else {
	    /* this channel was not acquired */
	    ctrl_usr->vert.data_offset[n] = -1;
	}
    }
    /* copy data from shared buffer to display buffer */
    ctrl_usr->samples = ctrl_shm->samples;
    samp_len = ctrl_shm->sample_len;
    samp_size = samp_len * sizeof(scope_data_t);
    dst = ctrl_usr->disp_buf;
    src = ctrl_usr->buffer + ctrl_shm->start;
    src_end = ctrl_usr->buffer + (ctrl_shm->rec_len * samp_len);
    n = 0;
    while (n < ctrl_usr->samples) {
	/* copy one sample */
	memcpy(dst, src, samp_size);
	n++;
	dst += samp_len;
	src += samp_len;
	if (src >= src_end) {
	    src = ctrl_usr->buffer;
	}
    }
    ctrl_shm->state = IDLE;
    switch (ctrl_usr->run_mode) {
    case STOP:
	break;
    case NORMAL:
	start_capture();
	break;
    case SINGLE:
	/* 'push' the stop button */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl_usr->
		rm_stop_button), TRUE);
	break;
    default:
	break;
    }
    refresh_display();
}

/***********************************************************************
*                      LOCAL INIT FUNCTION CODE                        *
************************************************************************/

static void init_usr_control_struct(void *shmem)
{
    char *cp;
    int n, skip;
    hal_comp_t *comp;

    /* first clear entire struct to all zeros */
    cp = (char *) ctrl_usr;
    for (n = 0; n < sizeof(scope_usr_control_t); n++) {
	cp[n] = 0;
    }
    /* save pointer to shared control structure */
    ctrl_shm = shmem;
    /* round size of shared struct up to a multiple of 4 for alignment */
    skip = (sizeof(scope_shm_control_t) + 3) & ~3;
    /* the rest of the shared memory area is the data buffer */
    ctrl_usr->buffer = (scope_data_t *) (((char *) (shmem)) + skip);
    /* is the realtime component loaded already? */
    comp = halpr_find_comp_by_name("scope_rt");
    if (comp == NULL) {
	/* no, must init shared structure */
	init_shared_control_struct();
    }
    /* init any non-zero fields */

    /* done */
}

static void init_shared_control_struct(void)
{
    char *cp;
    int skip, n;

    /* first clear entire struct to all zeros */
    cp = (char *) ctrl_shm;
    for (n = 0; n < sizeof(scope_shm_control_t); n++) {
	cp[n] = 0;
    }
    /* round size of shared struct up to a multiple of 4 for alignment */
    skip = (sizeof(scope_shm_control_t) + 3) & ~3;
    /* remainder of shmem area is buffer */
    ctrl_shm->buf_len = (SCOPE_SHM_SIZE - skip) / sizeof(scope_data_t);
    /* init any non-zero fields */
    ctrl_shm->mult = 1;
    ctrl_shm->state = IDLE;
}

/** 'define_scope_windows()' defines the overall layout of the main
    window.  It does not connect signals or load content into the
    windows - it only creates the windows (actually each "window" is
    either an hbox or vbox).  The layout is as shown below:

    **************************************************************
    *                horiz_info_win              *               *
    **********************************************  run_mode_win *
    *                                        * c *               *
    *                                        * h *               *
    *                                        * a *****************
    *                                        * n * trig_mode_win *
    *                                        * _ *               *
    *                waveform_win            * s *****************
    *                                        * e *               *
    *                                        * l *               *
    *                                        * _ *               *
    *                                        * w * vert_info_win *
    *                                        * i *               *
    *                                        * n *               *
    **********************************************               *
    *               chan_info_win                *               *
    **************************************************************

    Pointers to each of the windows named in the above diagram are
    saved in the control structure.  There are a few other boxes
    used to build the nested windows, but they are not needed later
    so pointers are not saved.
*/

static void define_scope_windows(void)
{
    GtkWidget *hbox, *vboxleft, *vboxright;

    /* create main window, set it's size */
    ctrl_usr->main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    /* set the minimum size */
//    gtk_widget_set_usize(GTK_WIDGET(ctrl_usr->main_win), 500, 350);
    /* allow the user to expand it */
    gtk_window_set_policy(GTK_WINDOW(ctrl_usr->main_win), FALSE, TRUE, FALSE);
    /* set main window title */
    gtk_window_set_title(GTK_WINDOW(ctrl_usr->main_win), "HAL Oscilliscope");

    /* top level - one big hbox */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
    /* add the hbox to the main window */
    gtk_container_add(GTK_CONTAINER(ctrl_usr->main_win), hbox);
    gtk_widget_show(hbox);
    /* end of top level */

    /* second level of windows */
    vboxleft = gtk_vbox_new_in_box(FALSE, 0, 0, hbox, TRUE, TRUE, 0);
    vboxright = gtk_vbox_new_in_box(FALSE, 0, 0, hbox, FALSE, FALSE, 5);

    /* third level of windows */
    /* left side */
    ctrl_usr->horiz_info_win =
	gtk_vbox_framed_new_in_box("Horizontal", FALSE, 0, 0, vboxleft, FALSE,
	FALSE, 1);
    /* hbox for waveform and chan sel windows */
    hbox = gtk_hbox_new_in_box(FALSE, 0, 0, vboxleft, TRUE, TRUE, 1);
    ctrl_usr->waveform_win =
	gtk_vbox_new_in_box(FALSE, 0, 0, hbox, TRUE, TRUE, 0);
    ctrl_usr->chan_sel_win =
	gtk_vbox_new_in_box(TRUE, 0, 0, hbox, FALSE, FALSE, 0);
    ctrl_usr->chan_info_win =
	gtk_hbox_framed_new_in_box("Selected Channel", FALSE, 0, 0, vboxleft,
	FALSE, FALSE, 0);
    /* right side */
    ctrl_usr->run_mode_win =
	gtk_vbox_framed_new_in_box("Run Mode", TRUE, 0, 0, vboxright, FALSE,
	FALSE, 0);
    ctrl_usr->trig_mode_win =
	gtk_vbox_framed_new_in_box("Trigger", TRUE, 0, 0, vboxright, FALSE,
	FALSE, 0);
    ctrl_usr->vert_info_win =
	gtk_vbox_framed_new_in_box("Vertical", FALSE, 0, 0, vboxright, TRUE,
	TRUE, 0);
    /* all windows are now defined */
}

static void init_run_mode_window(void)
{
    /* define the radio buttons */
    ctrl_usr->rm_stop_button = gtk_radio_button_new_with_label(NULL, "Stop");
    ctrl_usr->rm_normal_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl_usr->rm_stop_button)), "Normal");
    ctrl_usr->rm_single_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl_usr->rm_stop_button)), "Single");
    ctrl_usr->rm_roll_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl_usr->rm_stop_button)), "Roll");
    /* now put them into the box */
    gtk_box_pack_start(GTK_BOX(ctrl_usr->run_mode_win),
	ctrl_usr->rm_normal_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->run_mode_win),
	ctrl_usr->rm_single_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->run_mode_win),
	ctrl_usr->rm_roll_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->run_mode_win),
	ctrl_usr->rm_stop_button, FALSE, FALSE, 0);
    /* hook callbacks to buttons */
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->rm_normal_button), "clicked",
	GTK_SIGNAL_FUNC(rm_normal_button_clicked), NULL);
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->rm_single_button), "clicked",
	GTK_SIGNAL_FUNC(rm_single_button_clicked), NULL);
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->rm_roll_button), "clicked",
	GTK_SIGNAL_FUNC(rm_roll_button_clicked), NULL);
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->rm_stop_button), "clicked",
	GTK_SIGNAL_FUNC(rm_stop_button_clicked), NULL);
    /* and make them visible */
    gtk_widget_show(ctrl_usr->rm_normal_button);
    gtk_widget_show(ctrl_usr->rm_single_button);
    gtk_widget_show(ctrl_usr->rm_roll_button);
    gtk_widget_show(ctrl_usr->rm_stop_button);
}

static void init_trigger_mode_window(void)
{
    /* define the radio buttons */
    ctrl_usr->tm_normal_button =
	gtk_radio_button_new_with_label(NULL, "Normal");
    ctrl_usr->tm_auto_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl_usr->tm_normal_button)), "Auto");
    /* and a regular button */
    ctrl_usr->tm_force_button = gtk_button_new_with_label("Force");
    /* now put them into the box */
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_mode_win),
	ctrl_usr->tm_normal_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_mode_win),
	ctrl_usr->tm_auto_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_mode_win),
	ctrl_usr->tm_force_button, FALSE, FALSE, 0);
    /* hook callbacks to buttons */
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->tm_normal_button), "clicked",
	GTK_SIGNAL_FUNC(tm_normal_button_clicked), NULL);
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->tm_auto_button), "clicked",
	GTK_SIGNAL_FUNC(tm_auto_button_clicked), NULL);
    gtk_signal_connect(GTK_OBJECT(ctrl_usr->tm_force_button), "clicked",
	GTK_SIGNAL_FUNC(tm_force_button_clicked), NULL);
    /* and make them visible */
    gtk_widget_show(ctrl_usr->tm_normal_button);
    gtk_widget_show(ctrl_usr->tm_auto_button);
    gtk_widget_show(ctrl_usr->tm_force_button);
}

/* POP UP MENUS */

/** source menu - in this menu, the user selects what pin/signal/param
    they wish to assign to a channel
*/

/** color menu - in this menu, the user selects the color for a trace
    or for the background
*/

/* WINDOW REGIONS */

/** channel select area - one button per channel in a row (vert or horiz?)
    if off, click turns on and selects channel
    if on, click selects channel
    "select" means - trace is highlighted (brighter), channel is
    active in vertical menu, cursor displays that channel's value
    that channel's source name is displayed.
*/

/** vertical area - in this menu, the user selects vertical scaling
    and positioning for a channel or channels
    sliders - one for gain, from 1n/div to 500M/div in 1-2-5 steps
                  (54 steps)
            - one for position, from +5div to -5div in 0.1 div steps
	          (100 steps)
    entry - one for offset (default zero)
    uses an untabbed notebook, with different scale and offset
    entries for different data types
    this menu also allows the user to popup the source menu
    this menu also allows the user to popup the color menu

*/

/** trigger area - in this menu, the user selects the triggering.
    this means they pick the trigger source (any vertical channel)
    They also pick the trigger level, and the trigger polarity.
    Level is relative to the screen (with a slider), and the
    actual level is displayed.
*/

/** cursor area = slider for cursor position, two labels, one for
    timevalue, one for signal value, three buttons [1] [2] [d]
    [1] causes labels to display cursor1 data, slider moves
    cursor1, [2] displays cursor 2 data, slider moves cursor2
    [d] displays delta data, slider moves both cursors
    (can we eliminate sliders and let user click on the waveforms?)
*/

/***********************************************************************
*                    LOCAL CALLBACK FUNCTION CODE                      *
************************************************************************/

static void exit_from_hal(void)
{
    rtapi_shmem_delete(shm_id, comp_id);
    hal_exit(comp_id);
}

static void rm_normal_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    ctrl_usr->run_mode = NORMAL;
    if (ctrl_shm->state == IDLE) {
	start_capture();
    }
}

static void rm_single_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    ctrl_usr->run_mode = SINGLE;
    if (ctrl_shm->state == IDLE) {
	start_capture();
    }
}

static void rm_roll_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    printf("Sorry, ROLL mode is not supported yet\n");
    /* 'push' the stop button */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl_usr->rm_stop_button),
	TRUE);
}

static void rm_stop_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    if (ctrl_shm->state != IDLE) {
	/* RT code is sampling, tell it to stop */
	ctrl_shm->state = RESET;
    }
    ctrl_usr->run_mode = STOP;
}

static void tm_normal_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    printf("TM_NORMAL clicked\n");
}

static void tm_auto_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    printf("TM_AUTO clicked\n");
}

static void tm_force_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    ctrl_shm->force_trig = 1;
}
