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
#include "../hal_priv.h"	/* private HAL decls */

#include <gtk/gtk.h>
#include "miscgtk.h"		/* generic GTK stuff */
#include "scope_usr.h"		/* scope related declarations */

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

#define VERT_POS_RESOLUTION 100.0

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

static void init_chan_sel_window(void);
static void init_chan_info_window(void);
static void init_vert_info_window(void);

static gboolean dialog_select_source(int chan_num);
static void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, dialog_generic_t * dptr);
static void change_source_button(GtkWidget * widget, gpointer gdata);
static void channel_off_button(GtkWidget * widget, gpointer gdata);
static void offset_button(GtkWidget * widget, gpointer gdata);
static gboolean dialog_set_offset(int chan_num);
static void channel_changed(void);
static void scale_changed(GtkAdjustment * adj, gpointer gdata);
static void offset_changed(GtkEditable * editable, gchar * buf);
static void pos_changed(GtkAdjustment * adj, gpointer gdata);
static void chan_sel_button(GtkWidget * widget, gpointer gdata);

/* helper functions */
static void format_scale_value(char *buf, int buflen, float value);

/***********************************************************************
*                       PUBLIC FUNCTIONS                               *
************************************************************************/

void init_vert(void)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    int n;

    /* stop sampling */
    ctrl_shm->state = IDLE;
    /* make a pointer to the vert structure */
    vert = &(ctrl_usr->vert);
    /* init non-zero members of the vertical structure */
    invalidate_all_channels();
    /* init non-zero members of the channel structures */
    for (n = 1; n <= 16; n++) {
	chan = &(ctrl_usr->chan[n - 1]);
	chan->position = 0.5;
    }
    /* set up the windows */
    init_chan_sel_window();
    init_chan_info_window();
    init_vert_info_window();
}

void format_signal_value(char *buf, int buflen, float value)
{
    char *units;
    int decimals;
    char sign, symbols[] = "pnum KMGT";

    if (value < 0) {
	value = -value;
	sign = '-';
    } else {
	sign = '+';
    }
    if (value <= 1.0e-24) {
	/* pretty damn small, call it zero */
	snprintf(buf, buflen, "0.000");
	return;
    }
    if (value <= 1.0e-12) {
	/* less than pico units, use scientific notation */
	snprintf(buf, buflen, "%c%10.3e", sign, value);
	return;
    }
    if (value >= 1.0e+12) {
	/* greater than tera-units, use scientific notation */
	snprintf(buf, buflen, "%c%10.3e", sign, value);
	return;
    }
    units = &(symbols[4]);
    while (value < 1.0) {
	value *= 1000.0;
	units--;
    }
    while (value >= 1000.0) {
	value /= 1000.0;
	units++;
    }
    decimals = 3;
    if (value >= 9.999) {
	decimals--;
    }
    if (value >= 99.99) {
	decimals--;
    }
    snprintf(buf, buflen, "%c%0.*f%c", sign, decimals, value, *units);
}

/***********************************************************************
*                       LOCAL FUNCTIONS                                *
************************************************************************/

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

    /* a button to change the source */
    vert->source_name_button = gtk_button_new_with_label("------");
    gtk_box_pack_start(GTK_BOX(ctrl_usr->chan_info_win),
	vert->source_name_button, FALSE, FALSE, 3);

    vert->source_name_label = (GTK_BIN(vert->source_name_button))->child;
    gtk_label_set_justify(GTK_LABEL(vert->source_name_label),
	GTK_JUSTIFY_LEFT);
    gtk_label_size_to_fit(GTK_LABEL(vert->source_name_label),
	"--longest possible source name--");
    /* activate the source selection dialog if button is clicked */
    gtk_signal_connect(GTK_OBJECT(vert->source_name_button), "clicked",
	GTK_SIGNAL_FUNC(change_source_button), NULL);
    gtk_widget_show(vert->source_name_button);
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
    /* connect the slider to a function that re-calcs vertical scale */
    gtk_signal_connect(GTK_OBJECT(vert->scale_adj), "value_changed",
	GTK_SIGNAL_FUNC(scale_changed), NULL);
    gtk_widget_show(vert->scale_slider);
    /* box for the position slider */
    vbox = gtk_vbox_new_in_box(FALSE, 0, 0, hbox, TRUE, TRUE, 0);
    gtk_label_new_in_box("Pos", vbox, FALSE, FALSE, 0);
    vert->pos_adj =
	gtk_adjustment_new(VERT_POS_RESOLUTION / 2, 0, VERT_POS_RESOLUTION, 1,
	1, 0);
    vert->pos_slider = gtk_vscale_new(GTK_ADJUSTMENT(vert->pos_adj));
    gtk_scale_set_digits(GTK_SCALE(vert->pos_slider), 0);
    gtk_scale_set_draw_value(GTK_SCALE(vert->pos_slider), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), vert->pos_slider, TRUE, TRUE, 0);
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
    vert->offset_button = gtk_button_new_with_label("Offset\n----");
    vert->offset_label = (GTK_BIN(vert->offset_button))->child;
    gtk_box_pack_start(GTK_BOX(ctrl_usr->vert_info_win),
	vert->offset_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(vert->offset_button), "clicked",
	GTK_SIGNAL_FUNC(offset_button), NULL);
    gtk_widget_show(vert->offset_button);
    /* a button to turn off the channel */
    button = gtk_button_new_with_label("Chan Off");
    gtk_box_pack_start(GTK_BOX(ctrl_usr->vert_info_win), button, FALSE, FALSE,
	0);
    /* turn off the channel if button is clicked */
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(channel_off_button), NULL);
    gtk_widget_show(button);
}

static void scale_changed(GtkAdjustment * adj, gpointer gdata)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    int chan_num, index;
    float scale;
    gchar buf[BUFFERLEN];

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    chan = &(ctrl_usr->chan[chan_num - 1]);
    chan->scale_index = adj->value;
    scale = 1.0;
    index = chan->scale_index;
    while (index >= 3) {
	scale *= 10.0;
	index -= 3;
    }
    while (index <= -3) {
	scale *= 0.1;
	index += 3;
    }
    switch (index) {
    case 2:
	scale *= 5.0;
	break;
    case 1:
	scale *= 2.0;
	break;
    case -1:
	scale *= 0.5;
	break;
    case -2:
	scale *= 0.2;
	break;
    default:
	break;
    }
    chan->scale = scale;
    format_scale_value(buf, BUFFERLEN - 1, scale);
    gtk_label_set_text_if(vert->scale_label, buf);
    if (chan_num == ctrl_shm->trig_chan) {
	refresh_trigger();
    }
    request_display_refresh(1);
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
    chan = &(ctrl_usr->chan[chan_num - 1]);
    chan->position = adj->value / VERT_POS_RESOLUTION;
    if (chan_num == ctrl_shm->trig_chan) {
	refresh_trigger();
    }
    request_display_refresh(1);
}

static void offset_button(GtkWidget * widget, gpointer gdata)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    int chan_num;

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    chan = &(ctrl_usr->chan[chan_num - 1]);
    if (chan->data_type == HAL_BIT) {
	/* no offset for bits */
	return;
    }
    if (dialog_set_offset(chan_num)) {
	if (chan_num == ctrl_shm->trig_chan) {
	    refresh_trigger();
	}
	channel_changed();
	request_display_refresh(1);
    }
}

static gboolean dialog_set_offset(int chan_num)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    dialog_generic_t dialog;
    gchar *title, msg[BUFFERLEN], buf[BUFFERLEN], *cptr;
    GtkWidget *label, *button;
    float tmp;

    vert = &(ctrl_usr->vert);
    chan = &(ctrl_usr->chan[chan_num - 1]);
    title = "Set Offset";
    snprintf(msg, BUFFERLEN - 1, "Set the vertical offset\n"
	"for channel %d.", chan_num);
    /* create dialog window, disable resizing */
    dialog.retval = 0;
    dialog.window = gtk_dialog_new();
    dialog.app_data = buf;
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
    vert->offset_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->vbox),
	vert->offset_entry, FALSE, TRUE, 0);
    snprintf(buf, BUFFERLEN, "%f", chan->vert_offset);
    gtk_entry_set_text(GTK_ENTRY(vert->offset_entry), buf);
    gtk_widget_show(vert->offset_entry);
    /* connect the offset entry to a function */
    gtk_signal_connect(GTK_OBJECT(vert->offset_entry), "changed",
	GTK_SIGNAL_FUNC(offset_changed), buf);
    /* set up a callback function when the window is destroyed */
    gtk_signal_connect(GTK_OBJECT(dialog.window), "destroy",
	GTK_SIGNAL_FUNC(dialog_generic_destroyed), &dialog);
    /* make OK and Cancel buttons */
    button = gtk_button_new_with_label("OK");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->action_area),
	button, TRUE, TRUE, 4);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(dialog_generic_button1), &dialog);
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
    if ((dialog.retval == 0) || (dialog.retval == 2)) {
	/* user either closed dialog, or hit cancel */
	return FALSE;
    }
    tmp = strtod(buf, &cptr);
    if (cptr == buf) {
	return FALSE;
    }
    chan->vert_offset = tmp;
    return TRUE;
}

static void offset_changed(GtkEditable * editable, gchar * buf)
{
    const char *text;

    text = gtk_entry_get_text(GTK_ENTRY(ctrl_usr->vert.offset_entry));
    strncpy(buf, text, BUFFERLEN);
}

/* FIXME - this global is ugly - it's the result of using toggle
   buttons for the channel select.  I need a better way to show
   not only which channel(s) are enabled, but also which one is
   selected (highlighted) and which one is the trigger source. */
static int ignore_click = 0;

static void chan_sel_button(GtkWidget * widget, gpointer gdata)
{
    int chan_num, n, count;
    scope_vert_t *vert;
    scope_chan_t *chan;
    char *title, *msg;

    vert = &(ctrl_usr->vert);
    chan_num = (int) gdata;
    chan = &(ctrl_usr->chan[chan_num - 1]);

    if (ignore_click != 0) {
	ignore_click = 0;
	return;
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
	/* button was up when clicked */
	/* want to enable the channel */
	if (ctrl_shm->state != IDLE) {
	    /* acquisition in progress, 'push' the stop button */
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl_usr->
		    rm_stop_button), TRUE);
	}
	count = 0;
	for (n = 0; n < 16; n++) {
	    if (vert->chan_enabled[n]) {
		count++;
	    }
	}
	if (count >= ctrl_shm->sample_len) {
	    /* max number of channels already enabled */
	    /* force the button to pop back out */
	    ignore_click = 1;
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
	    if (dialog_select_source(chan_num) != TRUE) {
		/* user failed to assign a source */
		/* force the button to pop back out */
		ignore_click = 1;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		    FALSE);
		return;
	    }
	}
	vert->chan_enabled[chan_num - 1] = 1;
    } else {
	/* button was down when clicked, which means user clicked */
	/* an already enabled channel to highlight it. */
	/* button should stay down, so we force it */
	ignore_click = 1;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
    }
    if (vert->selected != chan_num) {
	/* make chan_num the selected channel */
	vert->selected = chan_num;
	channel_changed();
    }
}

static void channel_off_button(GtkWidget * widget, gpointer gdata)
{
    scope_vert_t *vert;
    int chan_num, n;

    vert = &(ctrl_usr->vert);
    chan_num = vert->selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    vert->chan_enabled[chan_num - 1] = 0;
    /* force the button to pop out */
    ignore_click = 1;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vert->
	    chan_sel_buttons[chan_num - 1]), FALSE);
    /* set new selected channel */
    n = 0;
    vert->selected = 0;
    do {
	chan_num++;
	if (chan_num > 16) {
	    chan_num = 1;
	}
	if (vert->chan_enabled[chan_num - 1] != 0) {
	    vert->selected = chan_num;
	}
    } while ((++n < 16) && (vert->selected == 0));
    channel_changed();
}

static void change_source_button(GtkWidget * widget, gpointer gdata)
{
    int chan_num;

    chan_num = ctrl_usr->vert.selected;
    if ((chan_num < 1) || (chan_num > 16)) {
	return;
    }
    if (ctrl_shm->state != IDLE) {
	/* acquisition in progress, 'push' the stop button */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl_usr->
		rm_stop_button), TRUE);
    }
    invalidate_channel(chan_num);
    dialog_select_source(chan_num);
    channel_changed();
}

static gboolean dialog_select_source(int chan_num)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    dialog_generic_t dialog;
    gchar *title, msg[BUFFERLEN];
    int next, n;
    gchar *tab_label_text[3], *name;
    GtkWidget *hbox, *label, *notebk, *button;
    GtkWidget *scrolled_window;
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;

    vert = &(ctrl_usr->vert);
    chan = &(ctrl_usr->chan[chan_num - 1]);
    title = "Select Channel Source";
    snprintf(msg, BUFFERLEN - 1, "Select a pin, signal, or parameter\n"
	"as the source for channel %d.", chan_num);
    /* create dialog window, disable resizing */
    dialog.retval = 0;
    dialog.window = gtk_dialog_new();
    dialog.app_data = chan;
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
    /* invalidate any data in the buffer for this channel */
    vert->data_offset[chan_num - 1] = -1;
    /* set scale and offset to nominal values */
    chan->vert_offset = 0.0;
    chan->scale_index = 0;
    channel_changed();
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
    chan = dptr->app_data;
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
	chan->data_type = pin->type;
	chan->name = pin->name;
	if (pin->signal == 0) {
	    /* pin is unlinked, get data from dummysig */
	    chan->data_addr = &(pin->dummysig);
	} else {
	    /* pin is linked to a signal */
	    sig = SHMPTR(pin->signal);
	    chan->data_addr = SHMPTR(sig->data_ptr);
	}
    } else if (listnum == 1) {
	/* search the signal list */
	sig = halpr_find_sig_by_name(name);
	if (sig == NULL) {
	    /* signal not found (can happen if signal is deleted after the
	       list was generated) */
	    return;
	}
	chan->data_type = sig->type;
	chan->name = sig->name;
	chan->data_addr = SHMPTR(sig->data_ptr);
    } else if (listnum == 2) {
	/* search the parameter list */
	param = halpr_find_param_by_name(name);
	if (param == NULL) {
	    /* parameter not found (can happen if param is deleted after the
	       list was generated) */
	    return;
	}
	chan->data_type = param->type;
	chan->name = param->name;
	chan->data_addr = SHMPTR(param->data_ptr);
    }
    switch (chan->data_type) {
    case HAL_BIT:
	chan->data_len = sizeof(hal_bit_t);
	break;
    case HAL_FLOAT:
	chan->data_len = sizeof(hal_float_t);
	break;
    case HAL_S8:
	chan->data_len = sizeof(hal_s8_t);
	break;
    case HAL_U8:
	chan->data_len = sizeof(hal_u8_t);
	break;
    case HAL_S16:
	chan->data_len = sizeof(hal_s16_t);
	break;
    case HAL_U16:
	chan->data_len = sizeof(hal_u16_t);
	break;
    case HAL_S32:
	chan->data_len = sizeof(hal_s32_t);
	break;
    case HAL_U32:
	chan->data_len = sizeof(hal_u32_t);
	break;
    default:
	/* Shouldn't get here, but just in case... */
	chan->data_len = 0;
    }
    /* set return value of dialog */
    dptr->retval = 1;
    /* destroy window to cause dialog_generic_destroyed() to be called */
    gtk_widget_destroy(dptr->window);
    return;
}

static void channel_changed(void)
{
    scope_vert_t *vert;
    scope_chan_t *chan;
    GtkAdjustment *adj;
    gchar *name;
    gchar buf1[BUFFERLEN + 1], buf2[BUFFERLEN + 1];

    vert = &(ctrl_usr->vert);
    if ((vert->selected < 1) || (vert->selected > 16)) {
	gtk_label_set_text_if(vert->scale_label, "----");
	gtk_label_set_text_if(vert->chan_num_label, "--");
	gtk_label_set_text_if(vert->source_name_label, "------");
	request_display_refresh(1);
	return;
    }
    chan = &(ctrl_usr->chan[vert->selected - 1]);
    /* set position slider based on new channel */
    gtk_adjustment_set_value(GTK_ADJUSTMENT(vert->pos_adj),
	chan->position * VERT_POS_RESOLUTION);
    adj = GTK_ADJUSTMENT(vert->scale_adj);
    switch (chan->data_type) {
    case HAL_BIT:
	adj->lower = -2;
	adj->upper = 2;
	break;
    case HAL_FLOAT:
	adj->lower = -36;
	adj->upper = 36;
	break;
    case HAL_S8:
	adj->lower = -2;
	adj->upper = 8;
	break;
    case HAL_U8:
	adj->lower = -2;
	adj->upper = 8;
	break;
    case HAL_S16:
	adj->lower = -2;
	adj->upper = 15;
	break;
    case HAL_U16:
	adj->lower = -2;
	adj->upper = 15;
	break;
    case HAL_S32:
	adj->lower = -2;
	adj->upper = 30;
	break;
    case HAL_U32:
	adj->lower = -2;
	adj->upper = 30;
	break;
    default:
	break;
    }
    adj->value = chan->scale_index;
    gtk_adjustment_changed(adj);
    gtk_adjustment_value_changed(adj);
    /* update the channel number and name display */
    snprintf(buf1, BUFFERLEN, "%2d", vert->selected);
    name = chan->name;
    gtk_label_set_text_if(vert->chan_num_label, buf1);
    gtk_label_set_text_if(vert->source_name_label, name);
    /* update the offset display */
    if (chan->data_type == HAL_BIT) {
	snprintf(buf1, BUFFERLEN, "----");
    } else {
	format_signal_value(buf1, BUFFERLEN, chan->vert_offset);
    }
    snprintf(buf2, BUFFERLEN, "Offset\n%s", buf1);
    gtk_label_set_text_if(vert->offset_label, buf2);
    request_display_refresh(1);
}

static void format_scale_value(char *buf, int buflen, float value)
{
    char *units;
    char symbols[] = "pnum KMGT";

    if (value < 0.9e-12) {
	/* less than pico units, shouldn't happen */
	snprintf(buf, buflen, "tiny");
	return;
    }
    if (value > 1.1e+12) {
	/* greater than tera-units, shouldn't happen */
	snprintf(buf, buflen, "huge");
	return;
    }
    units = &(symbols[4]);
    while (value < 1.0) {
	value *= 1000.0;
	units--;
    }
    while (value >= 999.99) {
	value *= 0.001;
	units++;
    }
    snprintf(buf, buflen, "%0.0f%c/div", value, *units);
}
