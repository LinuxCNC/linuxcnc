/** This file, 'halsc_vert.c', contains the portion of halscope
    that deals with vertical stuff - signal sources, scaling,
    position and such.
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
#include "hal_priv.h"		/* private HAL decls */

#include <gtk/gtk.h>
#include "miscgtk.h"		/* generic GTK stuff */
#include "halsc_usr.h"		/* scope related declarations */

#define BUFLEN 80		/* length for sprintf buffers */

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

static void init_chan_sel_window(void);
static void init_chan_info_window(void);
static void init_vert_info_window(void);

static void change_source_button(GtkWidget * widget, gpointer gdata);
static void channel_off_button(GtkWidget * widget, gpointer gdata);
static gboolean dialog_select_source(int chan_num);
static void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, dialog_generic_t * dptr);
static void refresh_chan_info(void);
#if 0
static void refresh_vert_info(void);
#endif

/*
static void init_acquire_function(void);
static void acquire_popup(GtkWidget * widget, gpointer gdata);

static void dialog_realtime_not_linked(void);
static void dialog_realtime_not_running(void);
static void acquire_selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, gpointer gdata);
*/
static void scale_changed(GtkAdjustment * adj, gpointer gdata);
static void offset_changed(GtkAdjustment * adj, gpointer gdata);
static void offset_activated(GtkAdjustment * adj, gpointer gdata);
static void pos_changed(GtkAdjustment * adj, gpointer gdata);
static void chan_sel_button(GtkWidget * widget, gpointer gdata);

/*
static void calc_horiz_scaling(void);

static void refresh_horiz_info(void);
static void refresh_pos_disp(void);

*/

/* helper functions */
#if 0
static void format_signal_value(char *buf, int buflen, float signal);
#endif

/***********************************************************************
*                       PUBLIC FUNCTIONS                               *
************************************************************************/

void init_vert(void)
{
    scope_vert_t *vert;

    /* stop sampling */
    ctrl_shm->state = IDLE;
    /* make a pointer to the horiz structure */
    vert = &(ctrl_usr->vert);
    /* init non-zero members of the vertical structure */
    /* set up the windows */
    init_chan_sel_window();
    init_chan_info_window();
    init_vert_info_window();
#if 0
    /* make sure displays are up to date */
    calc_horiz_scaling();
    refresh_vert_info();
#endif
}

static void init_chan_sel_window(void)
{
    scope_vert_t *vert;
    GtkWidget *button;
    gint n;
    gchar buf[5];

    vert = &(ctrl_usr->vert);
    for (n = 0; n < 16; n++) {
	snprintf(buf, 4, "%d", n + 1);
	/* define the button */
	button = gtk_toggle_button_new_with_label(buf);
	/* put it in the window */
	gtk_box_pack_start(GTK_BOX(ctrl_usr->chan_sel_win), button, TRUE,
	    TRUE, 0);
	gtk_widget_show(button);
	/* hook a callback function to it */
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
	    GTK_SIGNAL_FUNC(chan_sel_button), (gpointer) n + 1);
	/* save the button pointer */
	vert->chan_sel_buttons[n] = button;
    }
}

static void init_chan_info_window(void)
{
    scope_vert_t *vert;

    vert = &(ctrl_usr->vert);

    vert->chan_num_label =
	gtk_label_new_in_box("--", ctrl_usr->chan_info_win, FALSE, FALSE, 5);
    gtk_label_size_to_fit(GTK_LABEL(vert->chan_num_label), "99");
    gtk_vseparator_new_in_box(ctrl_usr->chan_info_win, 3);
    vert->source_name_label =
	gtk_label_new_in_box("------", ctrl_usr->chan_info_win, FALSE, FALSE,
	3);
    gtk_label_set_justify(GTK_LABEL(vert->source_name_label),
	GTK_JUSTIFY_LEFT);
//    gtk_label_size_to_fit(GTK_LABEL(vert->source_name_label), "--longest possible source name--");
}

static void init_vert_info_window(void)
{
    scope_vert_t *vert;
    GtkWidget *hbox, *vbox;
    GtkWidget *button;

    vert = &(ctrl_usr->vert);

    /* box for the two sliders */
    hbox =
	gtk_hbox_new_in_box(TRUE, 0, 0, ctrl_usr->vert_info_win, TRUE, TRUE,
	0);
    /* box for the scale slider */
    vbox = gtk_vbox_new_in_box(FALSE, 0, 0, hbox, TRUE, TRUE, 0);
    gtk_label_new_in_box("Gain", vbox, FALSE, FALSE, 0);
    vert->scale_adj = gtk_adjustment_new(0, -5, 5, 1, 1, 0);
    vert->scale_slider = gtk_vscale_new(GTK_ADJUSTMENT(vert->scale_adj));
    gtk_scale_set_digits(GTK_SCALE(vert->scale_slider), 0);
    gtk_scale_set_draw_value(GTK_SCALE(vert->scale_slider), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), vert->scale_slider, TRUE, TRUE, 0);
    /* store the current value of the slider */
    vert->scale_setting = GTK_ADJUSTMENT(vert->scale_adj)->value;
    /* connect the slider to a function that re-calcs vertical scale */
    gtk_signal_connect(GTK_OBJECT(vert->scale_adj), "value_changed",
	GTK_SIGNAL_FUNC(scale_changed), NULL);
    gtk_widget_show(vert->scale_slider);
    /* box for the position slider */
    vbox = gtk_vbox_new_in_box(FALSE, 0, 0, hbox, TRUE, TRUE, 0);
    gtk_label_new_in_box("Pos", vbox, FALSE, FALSE, 0);
    vert->pos_adj = gtk_adjustment_new(500, 0, 1000, 1, 1, 0);
    vert->pos_slider = gtk_vscale_new(GTK_ADJUSTMENT(vert->pos_adj));
    gtk_scale_set_digits(GTK_SCALE(vert->pos_slider), 0);
    gtk_scale_set_draw_value(GTK_SCALE(vert->pos_slider), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), vert->pos_slider, TRUE, TRUE, 0);
    /* store the current value of the slider */
    vert->pos_setting = GTK_ADJUSTMENT(vert->pos_adj)->value / 1000.0;
    /* connect the slider to a function that re-calcs vertical pos */
    gtk_signal_connect(GTK_OBJECT(vert->pos_adj), "value_changed",
	GTK_SIGNAL_FUNC(pos_changed), NULL);
    gtk_widget_show(vert->pos_slider);
    /* Scale display */
    gtk_hseparator_new_in_box(ctrl_usr->vert_info_win, 3);
    gtk_label_new_in_box("Scale", ctrl_usr->vert_info_win, FALSE, FALSE, 0);
    vert->scale_label =
	gtk_label_new_in_box(" ---- ", ctrl_usr->vert_info_win, FALSE, FALSE,
	0);
    /* Offset control */
    gtk_hseparator_new_in_box(ctrl_usr->vert_info_win, 3);
    gtk_label_new_in_box("Offset  ", ctrl_usr->vert_info_win, FALSE, FALSE,
	0);
/* FIXME - spinbutton doesn't work well with large or small numbers,
           and entry is too wide (messes up layout) */
#define SPINBUTTON
#ifdef ENTRY
    vert->offset_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(ctrl_usr->vert_info_win), vert->offset_entry,
	FALSE, TRUE, 0);
    gtk_widget_show(vert->offset_entry);
    /* connect the offset entry to a function */
    gtk_signal_connect(GTK_OBJECT(vert->offset_entry), "changed",
	GTK_SIGNAL_FUNC(offset_changed), NULL);
    gtk_signal_connect(GTK_OBJECT(vert->offset_entry), "activate",
	GTK_SIGNAL_FUNC(offset_activated), NULL);
#endif
#ifdef SPINBUTTON
    vert->offset_adj = gtk_adjustment_new(0, -100, 100, 1, 1, 0);
    vert->offset_spinbutton =
	gtk_spin_button_new(GTK_ADJUSTMENT(vert->offset_adj), 1, 2);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->vert_info_win),
	vert->offset_spinbutton, FALSE, TRUE, 0);
    gtk_widget_show(vert->offset_spinbutton);
    /* connect the offset spinbutton to a function */
    gtk_signal_connect(GTK_OBJECT(vert->offset_adj), "value_changed",
	GTK_SIGNAL_FUNC(offset_changed), NULL);
#endif

    /* a button to change the source */
    button = gtk_button_new_with_label("Source");
    gtk_box_pack_start(GTK_BOX(ctrl_usr->vert_info_win), button, FALSE, FALSE,
	3);
    /* activate the source selection dialog if button is clicked */
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(change_source_button), NULL);
    gtk_widget_show(button);
    /* a button to turn off the channel */
    button = gtk_button_new_with_label("Chan Off");
    gtk_box_pack_start(GTK_BOX(ctrl_usr->vert_info_win), button, FALSE, FALSE,
	0);
    /* turn off the channel if button is clicked */
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(channel_off_button), NULL);
    gtk_widget_show(button);

#if 0
    vert->chan_num_label =
	gtk_label_new_in_box("Chan --:", ctrl_usr->chan_info_win, FALSE,
	FALSE, 0);
    gtk_label_size_to_fit(GTK_LABEL(vert->chan_num_label), "Chan 99:");
    vert->source_name_label =
	gtk_label_new_in_box("------", ctrl_usr->chan_info_win, FALSE, FALSE,
	0);
    gtk_label_set_justify(GTK_LABEL(vert->source_name_label),
	GTK_JUSTIFY_LEFT);
    gtk_label_size_to_fit(GTK_LABEL(vert->source_name_label),
	"--longest possible source name--");
    gtk_vseparator_new_in_box(ctrl_usr->chan_info_win, 0);
    /* a button to turn off the channel */
    button = gtk_button_new_with_label("OFF");
    gtk_box_pack_end(GTK_BOX(ctrl_usr->chan_info_win), button, FALSE, FALSE,
	0);
    /* turn off the channel if button is clicked */
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(channel_off_button), NULL);
    gtk_widget_show(button);
    /* a button to change things */
    button = gtk_button_new_with_label(" Change Source ");
    gtk_box_pack_end(GTK_BOX(ctrl_usr->chan_info_win), button, FALSE, FALSE,
	0);
    /* activate the source selection dialog if button is clicked */
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(change_source_button), NULL);
    gtk_widget_show(button);
#endif
}

/***********************************************************************
*                       LOCAL FUNCTIONS                                *
************************************************************************/

#if 0
static void dialog_realtime_not_loaded(void)
{
    gchar *title, *msg;
    gint retval;

    title = "Realtime component not loaded";
    msg = "HALSCOPE uses a realtime component called 'halscope_rt.o'\n"
	"to sample signals for display.  It is not currently loaded.\n\n"
	"Please do one of the following:\n\n"
	"Load the component (using insmod), then click 'OK'\n"
	"or\n" "Click 'Cancel' to exit HALSCOPE";
    retval =
	dialog_generic_msg(ctrl_usr->main_win, title, msg, "OK", "Cancel",
	NULL, NULL);
    if ((retval == 0) || (retval == 2)) {
	/* user either closed dialog, or hit cancel - end the program */
	gtk_main_quit();
    }
}
#endif

#if 0
static void dialog_realtime_not_running(void)
{
    gchar *title, *msg;
    gint retval;

    title = "Realtime thread(s) not running";
    msg = "HALSCOPE uses code in a realtime HAL thread to sample\n"
	"signals for display.  The HAL thread(s) are not running.\n"
	"Threads are usually started by the application you are\n"
	"attempting to run, or you can use the 'halcmd start' command.\n\n"
	"Please do one of the following:\n\n"
	"Start the threads, then click 'OK'\n"
	"or\n" "Click 'Cancel' to exit HALSCOPE";
    retval =
	dialog_generic_msg(ctrl_usr->main_win, title, msg, "OK", "Cancel",
	NULL, NULL);
    if ((retval == 0) || (retval == 2)) {
	/* user either closed dialog, or hit cancel - end the program */
	gtk_main_quit();
    }
}

static void acquire_popup(GtkWidget * widget, gpointer gdata)
{
    scope_horiz_t *horiz;

    /** This function doesn't directly cause the acquire menu to
        pop up.  Instead is disconnects the acquire function from
        whatever thread is calling it.  This will result in a
        watchdog timeout, and that in turn will pop up the dialog
        requesting you to reconnect it.
    */
    horiz = &(ctrl_usr->horiz);
    if (horiz->thread_name != NULL) {
	hal_del_funct_from_thread("scope.sample", horiz->thread_name);
    }
    /* presetting the watchdog to 10 avoids the delay that would otherwise
       take place while the watchdog times out. */
    ctrl_shm->watchdog = 10;
    return;
}

static void acquire_selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, gpointer gdata)
{
    scope_horiz_t *horiz;
    GdkEventType type;
    gchar *picked;
    hal_thread_t *thread;
    long max_mult;

    /* get a pointer to the horiz data structure */
    horiz = &(ctrl_usr->horiz);

    if (clist == NULL) {
	/* spurious event, ignore it */
	return;
    }
    type = 4;
    if (event != NULL) {
	type = event->type;
    }
    if (type != 4) {
	/* We get bad callbacks if you drag the mouse across the list with
	   the button held down.  They can be distinguished because their
	   event type is 3, not 4. */
	return;
    }
    if (column < 0) {
	/* this is the initial selection automatically made by GTK */
	/* we don't want to act on it */
	return;
    }
    /* must be a valid user selection or preselection */
    /* Get the text from the list */
    gtk_clist_get_text(GTK_CLIST(clist), row, 0, &picked);
    /* find thread */
    thread = halpr_find_thread_by_name(picked);
    if (thread == NULL) {
	horiz->thread_name = NULL;
	calc_horiz_scaling();
	refresh_horiz_info();
	return;
    }
    horiz->thread_name = thread->name;
    horiz->thread_period_ns = thread->period;
    /* calc max possible mult (to keep sample period <= 1 sec */
    max_mult = (1000000000 / horiz->thread_period_ns);
    if (max_mult > 1000) {
	max_mult = 1000;
    }
    /* update limit on mult spinbox */
    GTK_ADJUSTMENT(horiz->mult_adjustment)->upper = max_mult;
    gtk_adjustment_changed(GTK_ADJUSTMENT(horiz->mult_adjustment));
    if (ctrl_shm->mult > max_mult) {
	ctrl_shm->mult = max_mult;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(horiz->mult_spinbutton),
	    max_mult);
    }
    calc_horiz_scaling();
    refresh_horiz_info();
}
#endif

static void scale_changed(GtkAdjustment * adj, gpointer gdata)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    int chan_num;

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    chan = &(vert->chan[chan_num - 1]);
    printf("Channel %d scale changed\n", chan_num);
#if 0
    vert->scale_setting = adj->value;
    calc_vert_scaling();
    refresh_vert_info();
#endif
}

static void pos_changed(GtkAdjustment * adj, gpointer gdata)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    int chan_num;

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    chan = &(vert->chan[chan_num - 1]);
    printf("Channel %d position changed\n", chan_num);
#if 0
    vert->pos_setting = adj->value;
    calc_vert_scaling();
    refresh_vert_info();
#endif
}

static void offset_changed(GtkAdjustment * adj, gpointer gdata)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    int chan_num;

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    chan = &(vert->chan[chan_num - 1]);
    printf("Channel %d offset changed\n", chan_num);
#if 0
    chan->offset =
	gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(vert->
	    offset_spinbutton));
    calc_vert_scaling();
    refresh_vert_info();
#endif
}

static void offset_activated(GtkAdjustment * adj, gpointer gdata)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    int chan_num;

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    chan = &(vert->chan[chan_num - 1]);
    printf("Channel %d offset activated\n", chan_num);
#if 0
    chan->offset =
	gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(vert->
	    offset_spinbutton));
    calc_vert_scaling();
    refresh_vert_info();
#endif
}

static void chan_sel_button(GtkWidget * widget, gpointer gdata)
{
    int chan_num, n, count, prev;
    scope_vert_t *vert;
    scope_chan_t *chan;
    char *title, *msg;
    short chan_mask, mask;

    vert = &(ctrl_usr->vert);
    chan_num = (int) gdata;
    chan = &(vert->chan[chan_num - 1]);
    chan_mask = 1 << (chan_num - 1);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
	/* button was up when clicked */
	if ((vert->enabled & chan_mask) != 0) {
	    /* channel was enabled, but button was up */
	    /* means click is from a force, ignore it */
	    return;
	}
	/* want to enable the channel */
	count = 0;
	mask = 1;
	for (n = 0; n < 16; n++) {
	    if ((vert->enabled & mask) != 0) {
		count++;
	    }
	    mask <<= 1;
	}
	if (count >= ctrl_shm->sample_len) {
	    /* max number of channels already enabled */
	    /* force the button to pop back out */
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
	    title = "Too many channels";
	    msg = "You cannot add another channel.\n\n"
		"Either turn off one or more channels, or shorten\n"
		"the record length to allow for more channels";
	    dialog_generic_msg(ctrl_usr->main_win, title, msg, "OK", NULL,
		NULL, NULL);
	    return;
	}
	if (chan->name == NULL) {
	    /* need to assign a source */
	    prev = vert->selected;
	    vert->selected = chan_num;
	    if (dialog_select_source(chan_num) != TRUE) {
		/* user failed to assign a source */
		/* force the button to pop back out */
		vert->selected = prev;
		refresh_chan_info();
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		    FALSE);
		return;
	    }
	}
	vert->enabled |= chan_mask;
    } else {
	/* button was down when clicked */
	if ((vert->enabled & chan_mask) == 0) {
	    /* channel was disabled, but button was down */
	    /* means click is from a force, ignore it */
	    return;
	}
	/* user clicked an already enabled channel to highlight it - button
	   should stay down, so we force it */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
    }
    if (vert->selected != chan_num) {
	/* make chan_num the selected channel */
	vert->selected = chan_num;
    }
    refresh_chan_info();
}

static void channel_off_button(GtkWidget * widget, gpointer gdata)
{
    scope_vert_t *vert;
    int chan_num, n;
    short chan_mask;

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    chan_mask = 1 << (chan_num - 1);
    vert->enabled &= ~chan_mask;
    /* force the button to pop out */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vert->
	    chan_sel_buttons[chan_num - 1]), FALSE);
    /* set new selected channel */
    n = 0;
    vert->selected = 0;
    do {
	chan_num++;
	chan_mask <<= 1;
	if (chan_num > 16) {
	    chan_num = 1;
	    chan_mask = 1;
	}
	if ((vert->enabled & chan_mask) != 0) {
	    vert->selected = chan_num;
	}
    } while ((++n < 16) && (vert->selected == 0));
    refresh_chan_info();
}

static void change_source_button(GtkWidget * widget, gpointer gdata)
{
    int chan_num;

    chan_num = ctrl_usr->vert.selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    dialog_select_source(chan_num);
    refresh_chan_info();
}

static gboolean dialog_select_source(int chan_num)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    dialog_generic_t dialog;
    gchar *title, *msg;
    int next, n, len;
    gchar *tab_label_text[3], *name;
    GtkWidget *hbox, *label, *notebk, *button;
    GtkWidget *scrolled_window;
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;

    vert = &(ctrl_usr->vert);
    chan = &(vert->chan[chan_num - 1]);
    title = "Select Channel Source";
    msg = "Select a pin, signal, or parameter\n"
	"as the source for this channel.";
    /* create dialog window, disable resizing */
    dialog.retval = 0;
    dialog.window = gtk_dialog_new();
    /* set initial height of window */
    gtk_widget_set_usize(GTK_WIDGET(dialog.window), -2, 300);
    /* allow user to grow but not shrink the window */
    gtk_window_set_policy(GTK_WINDOW(dialog.window), FALSE, TRUE, FALSE);
    /* window should appear in center of screen */
    gtk_window_set_position(GTK_WINDOW(dialog.window), GTK_WIN_POS_CENTER);
    /* set title */
    gtk_window_set_title(GTK_WINDOW(dialog.window), title);
    /* display message */
    label = gtk_label_new(msg);
    gtk_misc_set_padding(GTK_MISC(label), 15, 5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->vbox), label, FALSE,
	TRUE, 0);

    /* a separator */
    gtk_hseparator_new_in_box(GTK_DIALOG(dialog.window)->vbox, 0);

    /* create a notebook to hold pin, signal, and parameter lists */
    notebk = gtk_notebook_new();
    /* add the notebook to the dialog */
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->vbox), notebk, TRUE,
	TRUE, 0);
    /* set overall notebook parameters */
    gtk_notebook_set_homogeneous_tabs(GTK_NOTEBOOK(notebk), TRUE);
    /* text for tab labels */
    tab_label_text[0] = "Pins";
    tab_label_text[1] = "Signals";
    tab_label_text[2] = "Parameters";
    /* loop to create three identical tabs */
    for (n = 0; n < 3; n++) {
	/* Create a scrolled window to display the list */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
	    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_widget_show(scrolled_window);
	/* create a list to hold the data */
	vert->lists[n] = gtk_clist_new(1);
	/* set up a callback for when the user selects a line */
	gtk_signal_connect(GTK_OBJECT(vert->lists[n]), "select_row",
	    GTK_SIGNAL_FUNC(selection_made), &dialog);
	/* It isn't necessary to shadow the border, but it looks nice :) */
	gtk_clist_set_shadow_type(GTK_CLIST(vert->lists[n]), GTK_SHADOW_OUT);
	/* set list for single selection only */
	gtk_clist_set_selection_mode(GTK_CLIST(vert->lists[n]),
	    GTK_SELECTION_BROWSE);
	/* put the list into the scrolled window */
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
	    (scrolled_window), vert->lists[n]);
	gtk_widget_show(vert->lists[n]);
	/* create a box for the tab label */
	hbox = gtk_hbox_new(TRUE, 0);
	/* create a label for the page */
	gtk_label_new_in_box(tab_label_text[n], hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);
	/* add page to the notebook */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebk), scrolled_window, hbox);
	/* set tab attributes */
	gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK(notebk), hbox,
	    TRUE, TRUE, GTK_PACK_START);
    }
    gtk_widget_show(notebk);

    /* populate the pin, signal, and parameter lists */
    gtk_clist_clear(GTK_CLIST(vert->lists[0]));
    gtk_clist_clear(GTK_CLIST(vert->lists[1]));
    gtk_clist_clear(GTK_CLIST(vert->lists[2]));
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	name = pin->name;
	gtk_clist_append(GTK_CLIST(vert->lists[0]), &name);
	next = pin->next_ptr;
    }
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	name = sig->name;
	gtk_clist_append(GTK_CLIST(vert->lists[1]), &name);
	next = sig->next_ptr;
    }
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	name = param->name;
	gtk_clist_append(GTK_CLIST(vert->lists[2]), &name);
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));

    /* set up a callback function when the window is destroyed */
    gtk_signal_connect(GTK_OBJECT(dialog.window), "destroy",
	GTK_SIGNAL_FUNC(dialog_generic_destroyed), &dialog);
    /* make Cancel button */
    button = gtk_button_new_with_label("Cancel");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->action_area),
	button, TRUE, TRUE, 4);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(dialog_generic_button2), &dialog);
    /* make window transient and modal */
    gtk_window_set_transient_for(GTK_WINDOW(dialog.window),
	GTK_WINDOW(ctrl_usr->main_win));
    gtk_window_set_modal(GTK_WINDOW(dialog.window), TRUE);
    gtk_widget_show_all(dialog.window);
    gtk_main();
    /* we get here when the user makes a selection, hits Cancel, or closes
       the window */
    vert->lists[0] = NULL;
    vert->lists[1] = NULL;
    vert->lists[2] = NULL;
    if ((dialog.retval == 0) || (dialog.retval == 2)) {
	/* user either closed dialog, or hit cancel */
	return FALSE;
    }
    /* user made a selection */
    /* set values in ctrl_shm struct */
    ctrl_shm->data_offset[chan_num - 1] = SHMOFF(chan->addr);
    switch (chan->type) {
    case HAL_BIT:
	len = sizeof(hal_bit_t);
	break;
    case HAL_FLOAT:
	len = sizeof(hal_float_t);
	break;
    case HAL_S8:
	len = sizeof(hal_s8_t);
	break;
    case HAL_U8:
	len = sizeof(hal_u8_t);
	break;
    case HAL_S16:
	len = sizeof(hal_s16_t);
	break;
    case HAL_U16:
	len = sizeof(hal_u16_t);
	break;
    case HAL_S32:
	len = sizeof(hal_s32_t);
	break;
    case HAL_U32:
	len = sizeof(hal_u32_t);
	break;
    default:
	/* Shouldn't get here, but just in case... */
	len = 0;
    }
    ctrl_shm->data_len[chan_num - 1] = len;
    return TRUE;
}

/* If we come here, then the user has selected a row in the list. */
static void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, dialog_generic_t * dptr)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    GdkEventType type;
    gint n, listnum;
    gchar *name;
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;

    if ((event == NULL) || (clist == NULL)) {
	/* We get spurious events when the lists are populated I don't know
	   why.  If either clist or event is null, it's a bad one! */
	return;
    }
    type = event->type;
    if (type != 4) {
	/* We also get bad callbacks if you drag the mouse across the list
	   with the button held down.  They can be distinguished because
	   their event type is 3, not 4. */
	return;
    }
    /* If we get here, it should be a valid selection */
    vert = &(ctrl_usr->vert);
    chan = &(vert->chan[vert->selected - 1]);
    /* figure out which notebook tab it was */
    listnum = -1;
    for (n = 0; n < 3; n++) {
	if (clist == vert->lists[n]) {
	    listnum = n;
	}
    }
    /* Get the text from the list */
    gtk_clist_get_text(GTK_CLIST(clist), row, column, &name);
    /* locate the selected item in the HAL */
    if (listnum == 0) {
	/* search the pin list */
	pin = halpr_find_pin_by_name(name);
	if (pin == NULL) {
	    /* pin not found (can happen if pin is deleted after the list was 
	       generated) */
	    /* error handling leaves a bit to be desired! */
	    return;
	}
	chan->type = pin->type;
	chan->name = pin->name;
	if (pin->signal == 0) {
	    /* pin is unlinked, get data from dummysig */
	    chan->addr = &(pin->dummysig);
	} else {
	    /* pin is linked to a signal */
	    sig = SHMPTR(pin->signal);
	    chan->addr = SHMPTR(sig->data_ptr);
	}
    } else if (listnum == 1) {
	/* search the signal list */
	sig = halpr_find_sig_by_name(name);
	if (sig == NULL) {
	    /* signal not found (can happen if signal is deleted after the
	       list was generated) */
	    return;
	}
	chan->type = sig->type;
	chan->name = sig->name;
	chan->addr = SHMPTR(sig->data_ptr);
    } else if (listnum == 2) {
	/* search the parameter list */
	param = halpr_find_param_by_name(name);
	if (param == NULL) {
	    /* parameter not found (can happen if param is deleted after the
	       list was generated) */
	    return;
	}
	chan->type = param->type;
	chan->name = param->name;
	chan->addr = SHMPTR(param->data_ptr);
    }
    /* set return value of dialog */
    dptr->retval = 1;
    /* destroy window to cause dialog_generic_destroyed() to be called */
    gtk_widget_destroy(dptr->window);
    return;
}

#if 0
static void calc_horiz_scaling(void)
{
    scope_horiz_t *horiz;
    float total_rec_time;
    long int desired_usec_per_div, actual_usec_per_div;
    int n, decade, sub_decade;

    horiz = &(ctrl_usr->horiz);
    if (horiz->thread_name == NULL) {
	horiz->thread_period_ns = 0;
	ctrl_shm->mult = 1;
	horiz->sample_period_ns = 0;
	horiz->sample_period = 0.0;
	horiz->disp_scale = 0.0;
	return;
    }
    horiz->sample_period_ns = horiz->thread_period_ns * ctrl_shm->mult;
    horiz->sample_period = horiz->sample_period_ns / 1000000000.0;
    total_rec_time = ctrl_shm->rec_len * horiz->sample_period;
    if (total_rec_time < 0.000010) {
	/* out of range, set to 1uS per div */
	horiz->disp_scale = 0.000001;
	return;
    }
    desired_usec_per_div = (total_rec_time / 10.0) * 1000000.0;
    /* find scaling to display entire record */
    decade = 1;
    sub_decade = 1;
    actual_usec_per_div = decade * sub_decade;
    while (actual_usec_per_div < desired_usec_per_div) {
	if (sub_decade == 1) {
	    sub_decade = 2;
	} else if (sub_decade == 2) {
	    sub_decade = 5;
	} else {
	    sub_decade = 1;
	    decade *= 10;
	}
	actual_usec_per_div = decade * sub_decade;
    }
    /* now correct for zoom factor */
    for (n = 1; n < horiz->zoom_setting; n++) {
	if (sub_decade == 1) {
	    sub_decade = 5;
	    decade /= 10;
	} else if (sub_decade == 2) {
	    sub_decade = 1;
	} else {		/* sub_decade == 5 */

	    sub_decade = 2;
	}
    }
    if (decade == 0) {
	/* underflow from zoom, set to minimum */
	decade = 1;
	sub_decade = 1;
    }
    actual_usec_per_div = decade * sub_decade;
    /* convert to floating point */
    horiz->disp_scale = actual_usec_per_div / 1000000.0;
}
#endif

static void refresh_chan_info(void)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    gchar *name;
    gchar num[BUFLEN + 1];

    vert = &(ctrl_usr->vert);
    if ((vert->selected < 1) || (vert->selected > 16)) {
	snprintf(num, BUFLEN, "--");
	name = "----";
    } else {
	chan = &(vert->chan[vert->selected - 1]);
	snprintf(num, BUFLEN, "%2d", vert->selected);
	name = chan->name;
    }
    gtk_label_set_text_if(vert->chan_num_label, num);
    gtk_label_set_text_if(vert->source_name_label, name);
}

#if 0
void refresh_state_info(void)
{
    scope_horiz_t *horiz;
    static gchar *state_names[] = { "IDLE",
	"INIT",
	"PRE-TRIG",
	"TRIG?",
	"TRIGGERED",
	"FINISH",
	"DONE"
    };

    horiz = &(ctrl_usr->horiz);
    if (ctrl_shm->state > DONE) {
	ctrl_shm->state = IDLE;
    }
    gtk_label_set_text_if(horiz->state_label, state_names[ctrl_shm->state]);
    refresh_pos_disp();
}

static void refresh_pos_disp(void)
{
    scope_horiz_t *horiz;
    GdkDrawable *w;
    int width, height, depth;
    GdkGC *c;
    float disp_center, disp_start, disp_end;
    float rec_start, rec_curr, rec_end;
    float min, max, span, scale;

    int rec_line_y, rec_line_left, rec_line_right;
    int box_y_off, box_top, box_bot, box_right, box_left;
    int trig_y_off, trig_line_top, trig_line_bot, trig_line_x;
    int rec_curr_x;

    horiz = &(ctrl_usr->horiz);
    /* get window to local var */
    w = horiz->disp_area->window;
    if (w == NULL) {
	/* window isn't visible yet, do nothing */
	return;
    }
    /* create drawing context if needed */
    if (horiz->disp_context == NULL) {
	horiz->disp_context = gdk_gc_new(w);
    }
    /* get context to local var */
    c = horiz->disp_context;
    /* get window dimensions */
    gdk_window_get_geometry(w, NULL, NULL, &width, &height, &depth);

    /* these are based only on window dims */
    rec_line_y = (height - 1) / 2;
    box_y_off = rec_line_y / 2;
    trig_y_off = rec_line_y;
    trig_line_top = rec_line_y - trig_y_off;
    trig_line_bot = rec_line_y + trig_y_off;
    box_top = rec_line_y - box_y_off;
    box_bot = rec_line_y + box_y_off;

    /* these need to be calculated */
    /* times relative to trigger */
    rec_start = -ctrl_shm->pre_trig * horiz->sample_period;
    rec_end = (ctrl_shm->rec_len - ctrl_shm->pre_trig) * horiz->sample_period;
    rec_curr = rec_start + (ctrl_shm->samples * horiz->sample_period);
    disp_center = rec_start + horiz->pos_setting * (rec_end - rec_start);
    disp_start = disp_center - 5.0 * horiz->disp_scale;
    disp_end = disp_center + 5.0 * horiz->disp_scale;
    if (rec_start < disp_start) {
	min = rec_start;
    } else {
	min = disp_start;
    }
    if (rec_end > disp_end) {
	max = rec_end;
    } else {
	max = disp_end;
    }
    span = max - min;
    scale = (width - 1) / span;

    trig_line_x = scale * (0 - min);
    rec_line_left = scale * (rec_start - min);
    rec_line_right = scale * (rec_end - min);
    rec_curr_x = scale * (rec_curr - min);
    box_left = scale * (disp_start - min);
    box_right = scale * (disp_end - min);

    /* draw stuff */
    gdk_window_clear(w);
    gdk_draw_line(w, c, rec_line_left, rec_line_y + 1, rec_line_left,
	rec_line_y - 1);
    gdk_draw_line(w, c, rec_line_right, rec_line_y + 1, rec_line_right,
	rec_line_y - 1);
    gdk_draw_line(w, c, rec_line_left, rec_line_y + 1, rec_line_right,
	rec_line_y + 1);
    gdk_draw_line(w, c, rec_line_left, rec_line_y - 1, rec_line_right,
	rec_line_y - 1);
    gdk_draw_line(w, c, rec_line_left, rec_line_y, rec_curr_x, rec_line_y);
    gdk_draw_line(w, c, trig_line_x, trig_line_top, trig_line_x,
	trig_line_bot);
    gdk_draw_line(w, c, box_left, box_top, box_right, box_top);
    gdk_draw_line(w, c, box_left, box_bot, box_right, box_bot);
    gdk_draw_line(w, c, box_left, box_top, box_left, box_bot);
    gdk_draw_line(w, c, box_right, box_top, box_right, box_bot);
}
#endif

#if 0
static void format_signal_value(char *buf, int buflen, float value)
{
    char *units;
    int decimals;
    char symbols[] = "pnum KMGT";

    if (value < 1.0e-12) {
	/* less than pico units, use scientific notation */
	snprintf(buf, buflen, "%10.3e", value);
	return;
    }
    /* convert to pico-units */
    value *= 1.0e+12;
    units = symbols;
    while ((value > 1000.0) && (*units != '\0')) {
	value /= 1000.0;
	units++;
    }
    if (*units == '\0') {
	/* greatern than tera-units, use scientific notation */
	snprintf(buf, buflen, "%10.3e", value);
	return;
    }
    decimals = 2;
    if (value >= 10.0) {
	decimals = 1;
    }
    if (value >= 100.0) {
	decimals = 0;
    }
    snprintf(buf, buflen, "%0.*f %s", decimals, value, units);
}
#endif
