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
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

static int comp_id;		/* component ID */
static int shm_id;		/* shared memory ID */
static scope_usr_control_t ctrl_struct;	/* scope control structure */

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

/* init functions */
static void init_usr_control_struct(scope_usr_control_t * ctrl, void *shmem);
static void init_shared_control_struct(scope_shm_control_t * ctrl);

static void define_scope_windows(scope_usr_control_t * ctrl);
static void init_run_mode_window(scope_usr_control_t * ctrl);
static void init_trigger_mode_window(scope_usr_control_t * ctrl);
static void init_menu_window(scope_usr_control_t * ctrl);

static void init_chan_select_window(scope_usr_control_t * ctrl);

/* callback functions */
static void exit_from_hal(void);
static int heartbeat(gpointer data);
static void rm_normal_button_clicked(GtkWidget * widget,
    scope_usr_control_t * ctrl);
static void rm_stop_button_clicked(GtkWidget * widget,
    scope_usr_control_t * ctrl);
static void tm_force_button_clicked(GtkWidget * widget,
    scope_usr_control_t * ctrl);

/***********************************************************************
*                        MAIN() FUNCTION                               *
************************************************************************/

void foo(void)
{
    gtk_main_quit();
}

int main(int argc, gchar * argv[])
{
    scope_usr_control_t *ctrl;
    int retval;
    void *shm_base;

    /* generic widgets */
    GtkWidget *label;

    /* process and remove any GTK specific command line args */
    gtk_init(&argc, &argv);
    /* process my own command line args (if any) here */

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
    ctrl = &ctrl_struct;
    init_usr_control_struct(ctrl, shm_base);

/* FIXME - resume cleanup here */

    /* init watchdog */
    ctrl->shared->watchdog = 10;
    /* set main window */
    define_scope_windows(ctrl);
    /* this makes the application exit when the window is closed */
    gtk_signal_connect(GTK_OBJECT(ctrl->main_win), "destroy",
	GTK_SIGNAL_FUNC(foo), NULL);
    /* define menu windows */
    /* do next level of init */
    init_run_mode_window(ctrl);
    init_trigger_mode_window(ctrl);
    init_menu_window(ctrl);
    init_chan_select_window(ctrl);
    init_horiz(ctrl);
/* test code - make labels to show the size of each box */

    label = gtk_label_new("waveform_win");
    gtk_box_pack_start(GTK_BOX(ctrl->waveform_win), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    /* The interface is completely set up so we show the window and enter the
       gtk_main loop. */
    gtk_widget_show(ctrl->main_win);
    /* arrange for periodic call of heartbeat() */
    gtk_timeout_add(100, heartbeat, ctrl);
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
    scope_usr_control_t *ctrl;

    /* point to control structure */
    ctrl = (scope_usr_control_t *) data;
    refresh_state_info(ctrl);
    /* check watchdog */
    if (ctrl->shared->watchdog < 10) {
	ctrl->shared->watchdog++;
    } else {
	handle_watchdog_timeout(ctrl);
    }
    return 1;
}

#if 0
meter_t *meter_new(void)
{
    meter_t *new;

    /* allocate a meter object for the display */
    new = malloc(sizeof(meter_t));
    if (new == NULL) {
	return NULL;
    }
    /* define a probe for the display item */
    new->probe = probe_new("Select item to display");
    if (new->probe == NULL) {
	free(new);
	return NULL;
    }
    /* create a label widget to hold the value */
    new->value_label = gtk_label_new("----");
    /* center justify text, no wordwrap */
    gtk_label_set_justify(GTK_LABEL(new->value_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(new->value_label), FALSE);

    /* create a label widget to hold the name */
    new->name_label = gtk_label_new("------");
    /* center justify text, no wordwrap */
    gtk_label_set_justify(GTK_LABEL(new->name_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(new->name_label), FALSE);

    return new;
}
#endif

/***********************************************************************
*                      LOCAL INIT FUNCTION CODE                        *
************************************************************************/

static void init_usr_control_struct(scope_usr_control_t * ctrl, void *shmem)
{
    char *cp;
    int n, skip;
    hal_comp_t *comp;

    /* first clear entire struct to all zeros */
    cp = (char *) ctrl;
    for (n = 0; n < sizeof(scope_usr_control_t); n++) {
	cp[n] = 0;
    }
    /* save pointer to shared control structure */
    ctrl->shared = shmem;
    /* round size of shared struct up to a multiple of 4 for alignment */
    skip = (sizeof(scope_shm_control_t) + 3) & !3;
    /* the rest of the shared memory area is the data buffer */
    ctrl->buffer = (scope_data_t *) ((char *) (shmem)) + skip;
    /* is the realtime component loaded already? */
    comp = halpr_find_comp_by_name("scope_rt");
    if (comp == NULL) {
	/* no, must init shared structure */
	init_shared_control_struct(ctrl->shared);
    }
    /* init remainder of local structure */

    /* done */
}

static void init_shared_control_struct(scope_shm_control_t * share)
{
    int skip;

    /* round size of shared struct up to a multiple of 4 for alignment */
    skip = (sizeof(scope_shm_control_t) + 3) & !3;
    /* remainder of shmem area is buffer */
    share->buf_len = (SCOPE_SHM_SIZE - skip) / sizeof(scope_data_t);
    share->watchdog = 0;
    share->mult = 1;
    share->mult_cntr = 0;
    share->rec_len = 0;
    share->sample_len = 0;
    share->pre_trig = 0;
    share->force_trig = 0;
    share->start = 0;
    share->curr = 0;
    share->samples = 0;
}

/** 'define_scope_windows()' defines the overall layout of the main
    window.  It does not connect signals or load content into the
    windows - it only creates the windows (actually each "window" is
    either an hbox or vbox).  The layout is as shown below:

    **************************************************************
    *                hor_disp_win                *               *
    **********************************************  run_mode_win *
    *                                            *               *
    *                                            *               *
    *                                            *****************
    *                                            * trig_mode_win *
    *                                            *               *
    *                waveform_win                *****************
    *                                            *  state_win    *
    *                                            *****************
    *                                            *               *
    *                                            *   menu_win    *
    *                                            *               *
    *                                            *               *
    **********************************************               *
    * hor_zoom_win * hor_scale_win * hor_pos_win *               *
    **************************************************************

    Pointers to each of the windows named in the above diagram are
    saved in the control structure.  There are a few other boxes
    used to build the nested windows, but they are not needed later
    so pointers are not saved.
*/

static void define_scope_windows(scope_usr_control_t * ctrl)
{
    GtkWidget *hbox1, *bar;
    GtkWidget *vbox2l, *vbox2r;
    GtkWidget *hbox3b, *hbox3c;

    /* create main window, set it's size */
    ctrl->main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    /* set the minimum size */
//    gtk_widget_set_usize(GTK_WIDGET(ctrl->main_win), 500, 350);
    /* allow the user to expand it */
    gtk_window_set_policy(GTK_WINDOW(ctrl->main_win), TRUE, TRUE, FALSE);
    /* set main window title */
    gtk_window_set_title(GTK_WINDOW(ctrl->main_win), "HAL Oscilliscope");

    /* top level - one big hbox */
    hbox1 = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 0);
    /* add the hbox to the main window */
    gtk_container_add(GTK_CONTAINER(ctrl->main_win), hbox1);
    gtk_widget_show(hbox1);
    /* end of top level */

    /* start second level */
    /* a vbox on the left */
    vbox2l = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2l), 0);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox2l, TRUE, TRUE, 0);
    gtk_widget_show(vbox2l);
    /* a separator between the left and right vboxes */
    bar = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(hbox1), bar, FALSE, FALSE, 0);
    gtk_widget_show(bar);
    /* a vbox on the right */
    vbox2r = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2r), 0);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox2r, FALSE, FALSE, 5);
    gtk_widget_show(vbox2r);
    /* end of second level */

    /* start of third level - left side */
    /* a vbox at the top */
    ctrl->hor_disp_win =
	gtk_vbox_new_in_box(FALSE, 0, 0, vbox2l, FALSE, FALSE, 1);
    /* separator */
    gtk_hseparator_new_in_box(vbox2l, 0);
    /* another hbox */
    hbox3b = gtk_hbox_new_in_box(FALSE, 0, 0, vbox2l, FALSE, FALSE, 1);
    /* separator */
    gtk_hseparator_new_in_box(vbox2l, 0);
    /* the main waveform window */
    ctrl->waveform_win =
	gtk_vbox_new_in_box(FALSE, 0, 0, vbox2l, TRUE, TRUE, 0);
    /* another separater */
    gtk_hseparator_new_in_box(vbox2l, 0);
    /* an hbox at the bottom */
    hbox3c = gtk_hbox_new_in_box(TRUE, 0, 0, vbox2l, FALSE, FALSE, 1);
    /* end of third level - left side */

    /* start of third level - right side */
    /* run mode window */
    ctrl->run_mode_win = gtk_vbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(ctrl->run_mode_win), 0);
    gtk_box_pack_start(GTK_BOX(vbox2r), ctrl->run_mode_win, FALSE, FALSE, 5);
    gtk_widget_show(ctrl->run_mode_win);
    /* separator */
    bar = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox2r), bar, FALSE, FALSE, 0);
    gtk_widget_show(bar);
    /* trigger mode window */
    ctrl->trig_mode_win = gtk_vbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(ctrl->trig_mode_win), 0);
    gtk_box_pack_start(GTK_BOX(vbox2r), ctrl->trig_mode_win, FALSE, FALSE, 5);
    gtk_widget_show(ctrl->trig_mode_win);
    /* another separator */
    bar = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox2r), bar, FALSE, FALSE, 0);
    gtk_widget_show(bar);
    /* menu window */
    ctrl->menu_win = gtk_vbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(ctrl->menu_win), 0);
    gtk_box_pack_start(GTK_BOX(vbox2r), ctrl->menu_win, TRUE, TRUE, 5);
    gtk_widget_show(ctrl->menu_win);
    /* end of third level - right side */

    /* all windows are now defined */
}

static void init_run_mode_window(scope_usr_control_t * ctrl)
{
    GtkWidget *label;

    /* fill in the run mode window */
    label = gtk_label_new("Run Mode");
    /* center justify text, no wordwrap */
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
    /* add label to window */
    gtk_box_pack_start(GTK_BOX(ctrl->run_mode_win), label, FALSE, FALSE, 0);
    gtk_widget_show(label);
    /* now define the radio buttons */
    ctrl->rm_stop_button = gtk_radio_button_new_with_label(NULL, "Stop");
    ctrl->rm_normal_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl->rm_stop_button)), "Normal");
    ctrl->rm_single_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl->rm_stop_button)), "Single");
    ctrl->rm_roll_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl->rm_stop_button)), "Roll");
    /* now put them into the box */
    gtk_box_pack_start(GTK_BOX(ctrl->run_mode_win), ctrl->rm_normal_button,
	FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl->run_mode_win), ctrl->rm_single_button,
	FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl->run_mode_win), ctrl->rm_roll_button,
	FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl->run_mode_win), ctrl->rm_stop_button,
	FALSE, FALSE, 0);
    /* hook callbacks to buttons */
    gtk_signal_connect(GTK_OBJECT(ctrl->rm_normal_button), "clicked",
	GTK_SIGNAL_FUNC(rm_normal_button_clicked), ctrl);
    gtk_signal_connect(GTK_OBJECT(ctrl->rm_stop_button), "clicked",
	GTK_SIGNAL_FUNC(rm_stop_button_clicked), ctrl);
    /* and make them visible */
    gtk_widget_show(ctrl->rm_normal_button);
    gtk_widget_show(ctrl->rm_single_button);
    gtk_widget_show(ctrl->rm_roll_button);
    gtk_widget_show(ctrl->rm_stop_button);
}

static void init_trigger_mode_window(scope_usr_control_t * ctrl)
{
    GtkWidget *label;

    /* fill in the trigger mode window */
    label = gtk_label_new("Trig Mode");
    /* center justify text, no wordwrap */
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
    /* add label to window */
    gtk_box_pack_start(GTK_BOX(ctrl->trig_mode_win), label, FALSE, FALSE, 0);
    gtk_widget_show(label);
    /* now define the radio buttons */
    ctrl->tm_normal_button = gtk_radio_button_new_with_label(NULL, "Normal");
    ctrl->tm_auto_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(ctrl->tm_normal_button)), "Auto");
    ctrl->tm_force_button = gtk_button_new_with_label("Force");
    /* now put them into the box */
    gtk_box_pack_start(GTK_BOX(ctrl->trig_mode_win), ctrl->tm_normal_button,
	FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl->trig_mode_win), ctrl->tm_auto_button,
	FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl->trig_mode_win), ctrl->tm_force_button,
	FALSE, FALSE, 0);
    /* hook callbacks to buttons */
    gtk_signal_connect(GTK_OBJECT(ctrl->tm_force_button), "clicked",
	GTK_SIGNAL_FUNC(tm_force_button_clicked), ctrl);
    /* and make them visible */
    gtk_widget_show(ctrl->tm_normal_button);
    gtk_widget_show(ctrl->tm_auto_button);
    gtk_widget_show(ctrl->tm_force_button);
}

static void init_menu_window(scope_usr_control_t * ctrl)
{

    GtkWidget *label;

    /* label the window */
    label = gtk_label_new("Menus");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
    gtk_box_pack_start(GTK_BOX(ctrl->menu_win), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

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

static void init_chan_select_window(scope_usr_control_t * ctrl)
{
    GtkWidget *vbox1 /* ,*vbox2, *bar */ ;
    GtkWidget *chk[16] /* , *rad[16] */ ;
    gint n;
    gchar buf[5];

    /* define two vboxes */
    vbox1 = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox1), 0);
    gtk_box_pack_start(GTK_BOX(ctrl->waveform_win), vbox1, FALSE, TRUE, 0);
    gtk_widget_show(vbox1);
#if 0
    vbox2 = gtk_vbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 0);
    gtk_box_pack_start(GTK_BOX(ctrl->waveform_win), vbox2, FALSE, TRUE, 0);
    gtk_widget_show(vbox2);
    /* and a separator */
    bar = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(ctrl->waveform_win), bar, FALSE, FALSE, 0);
    gtk_widget_show(bar);
#endif

    for (n = 0; n < 16; n++) {
	snprintf(buf, 4, "%d", n);
	/* define the check button */
	chk[n] = gtk_button_new_with_label(buf);
	gtk_box_pack_start(GTK_BOX(vbox1), chk[n], TRUE, TRUE, 0);
	gtk_widget_show(chk[n]);
#if 0
	/* now define the radio buttons */
	if (n == 0) {
	    /* define the first radio button */
	    rad[n] = gtk_radio_button_new(NULL);
	} else {
	    /* define an additional radio button */
	    rad[n] =
		gtk_radio_button_new(gtk_radio_button_group(GTK_RADIO_BUTTON
		    (rad[0])));
	}
	gtk_box_pack_start(GTK_BOX(vbox2), rad[n], FALSE, FALSE, 0);
	gtk_widget_show(rad[n]);
#endif
    }
}

/***********************************************************************
*                    LOCAL CALLBACK FUNCTION CODE                      *
************************************************************************/

static void exit_from_hal(void)
{
    rtapi_shmem_delete(shm_id, comp_id);
    hal_exit(comp_id);
}

static void rm_normal_button_clicked(GtkWidget * widget,
    scope_usr_control_t * ctrl)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    /* FIXME temporary, remove after trigger pos is working */
    ctrl->shared->pre_trig = ctrl->shared->rec_len / 2;
    if (ctrl->shared->state == IDLE) {
	ctrl->shared->state = INIT;
    }
}

static void rm_stop_button_clicked(GtkWidget * widget,
    scope_usr_control_t * ctrl)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    ctrl->shared->state = IDLE;
}

static void tm_force_button_clicked(GtkWidget * widget,
    scope_usr_control_t * ctrl)
{
    ctrl->shared->force_trig = 1;
}

#if 0
/* this function refreshes the value display */
static int refresh_value(gpointer data)
{
    meter_t *meter;
    probe_t *probe;
    char *value_str;

    meter = (meter_t *) data;
    probe = meter->probe;

    if (probe->data == NULL) {
	return 1;
    }
    value_str = data_value(probe->type, probe->data);
    gtk_label_set_text(GTK_LABEL(meter->value_label), value_str);
    gtk_label_set_text(GTK_LABEL(meter->name_label), probe->name);
    return 1;
}

/* Switch function to return var value for the print_*_list functions  */
static char *data_value(int type, void *valptr)
{
    char *value_str;
    static char buf[25];

    switch (type) {
    case HAL_BIT:
	if (*((char *) valptr) == 0)
	    value_str = "FALSE";
	else
	    value_str = "TRUE";
	break;
    case HAL_FLOAT:
	snprintf(buf, 24, "%12.5e", *((float *) valptr));
	value_str = buf;
	break;
    case HAL_S8:
	snprintf(buf, 24, "%4d", *((signed char *) valptr));
	value_str = buf;
	break;
    case HAL_U8:
	snprintf(buf, 24, "%3u  (%02X)",
	    *((unsigned char *) valptr), *((unsigned char *) valptr));
	value_str = buf;
	break;
    case HAL_S16:
	snprintf(buf, 24, "%6d", *((signed short *) valptr));
	value_str = buf;
	break;
    case HAL_U16:
	snprintf(buf, 24, "%5u (%04X)",
	    *((unsigned short *) valptr), *((unsigned short *) valptr));
	value_str = buf;
	break;
    case HAL_S32:
	snprintf(buf, 24, "%10ld", *((signed long *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 24, "%10lu (%08lX)", *((unsigned long *) valptr),
	    *((unsigned long *) valptr));
	value_str = buf;
	break;
    default:
	/* Shouldn't get here, but just in case... */
	value_str = "";
    }
    return value_str;
}
#endif
