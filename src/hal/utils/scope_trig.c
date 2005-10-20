/** This file, 'halsc_trig.c', contains the portion of halscope
    that deals with triggering
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

#define BUFLEN 80		/* length for sprintf buffers */

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

#define TRIG_LEVEL_RESOLUTION 100.0
#define TRIG_POS_RESOLUTION 100.0

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

static void init_trigger_mode_window(void);
static void init_trigger_info_window(void);

static void trigger_selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, dialog_generic_t * dptr);
static void dialog_select_trigger_source(void);

/* callback functions */
static void auto_button_clicked(GtkWidget * widget, gpointer * gdata);
static void normal_button_clicked(GtkWidget * widget, gpointer * gdata);
static void force_button_clicked(GtkWidget * widget, gpointer * gdata);
static void source_button_clicked(GtkWidget * widget, gpointer * gdata);
static void edge_button_clicked(GtkWidget * widget, gpointer * gdata);
static void level_changed(GtkAdjustment * adj, gpointer gdata);
static void pos_changed(GtkAdjustment * adj, gpointer gdata);

/* helper functions */

/***********************************************************************
*                       PUBLIC FUNCTIONS                               *
************************************************************************/

void init_trig(void)
{
    scope_trig_t *trig;

    /* stop sampling */
    ctrl_shm->state = IDLE;
    /* make a pointer to the trig structure */
    trig = &(ctrl_usr->trig);
    /* init non-zero members of the trigger structure */

    /* set up the windows */
    init_trigger_mode_window();
    init_trigger_info_window();
}

void refresh_trigger(void)
{
    scope_trig_t *trig;
    scope_chan_t *chan;
    gchar buf[BUFLEN + 1];
    float fp_level;

    trig = &(ctrl_usr->trig);
    /* display edge */
    if (ctrl_shm->trig_edge == 0) {
	snprintf(buf, BUFLEN, "Falling");
    } else {
	snprintf(buf, BUFLEN, "Rising");
    }
    gtk_label_set_text_if(trig->edge_label, buf);
    /* display source */
    if ((ctrl_shm->trig_chan < 1) || (ctrl_shm->trig_chan > 16)) {
	/* no source */
	ctrl_shm->trig_chan = 0;
	gtk_label_set_text_if(trig->source_label, "Source\nNone");
	gtk_label_set_text_if(trig->level_label, "  ----  ");
	/* nothing left to do */
	return;
    }
    snprintf(buf, BUFLEN, "Source\nChan %2d", ctrl_shm->trig_chan);
    gtk_label_set_text_if(trig->source_label, buf);
    /* point to source channel data */
    chan = &(ctrl_usr->chan[ctrl_shm->trig_chan - 1]);
    /* calculate a preliminary value for trigger level */
    fp_level =
	chan->scale * ((chan->position - trig->level) * 10) -
	chan->vert_offset;
    /* apply type specific tweaks to trigger level */
    switch (chan->data_type) {
    case HAL_FLOAT:
	ctrl_shm->trig_level.d_float = fp_level;
    case HAL_S8:
	if (fp_level > 127.0) {
	    fp_level = 127.0;
	}
	if (fp_level < -128.0) {
	    fp_level = -128.0;
	}
	ctrl_shm->trig_level.d_s8 = fp_level;
	break;
    case HAL_U8:
	if (fp_level > 255.0) {
	    fp_level = 255.0;
	}
	if (fp_level < 0.0) {
	    fp_level = 0.0;
	}
	ctrl_shm->trig_level.d_u8 = fp_level;
	break;
    case HAL_S16:
	if (fp_level > 32767.0) {
	    fp_level = 32767.0;
	}
	if (fp_level < -32768.0) {
	    fp_level = -32768.0;
	}
	ctrl_shm->trig_level.d_s16 = fp_level;
	break;
    case HAL_U16:
	if (fp_level > 65535.0) {
	    fp_level = 65535.0;
	}
	if (fp_level < 0.0) {
	    fp_level = 0.0;
	}
	ctrl_shm->trig_level.d_u16 = fp_level;
	break;
    case HAL_S32:
	if (fp_level > 2147483647.0) {
	    fp_level = 2147483647.0;
	}
	if (fp_level < -2147483648.0) {
	    fp_level = -2147483648.0;
	}
	ctrl_shm->trig_level.d_s32 = fp_level;
	break;
    case HAL_U32:
	if (fp_level > 4294967295.0) {
	    fp_level = 4294967295.0;
	}
	if (fp_level < 0.0) {
	    fp_level = 0.0;
	}
	ctrl_shm->trig_level.d_u32 = fp_level;
	break;
    default:
	break;
    }
    if (chan->data_type == HAL_BIT) {
	snprintf(buf, BUFLEN, "  ----  ");
    } else {
	format_signal_value(buf, BUFLEN, fp_level);
    }
    gtk_label_set_text_if(trig->level_label, buf);
#if 0
    adj->value = chan->level_index;
    gtk_adjustment_changed(adj);
    gtk_adjustment_value_changed(adj);
    /* update the channel number and name display */
    snprintf(num, BUFLEN, "%2d", trig->selected);
    name = chan->name;
    gtk_label_set_text_if(trig->chan_num_label, num);
    gtk_label_set_text_if(trig->source_name_label, name);
    request_display_refresh(1);
#endif
}

void write_trig_config(FILE *fp)
{
    scope_trig_t *trig;
    
    trig = &(ctrl_usr->trig);
    if (ctrl_shm->trig_chan > 0) {
	fprintf(fp, "TSOURCE %d\n", ctrl_shm->trig_chan);
    }
    
    
}




/***********************************************************************
*                       LOCAL FUNCTIONS                                *
************************************************************************/

static void init_trigger_mode_window(void)
{
    scope_trig_t *trig;

    /* make a pointer to the trig structure */
    trig = &(ctrl_usr->trig);
    /* set initial state to normal */
    ctrl_shm->auto_trig = 0;
    /* define the radio buttons */
    trig->normal_button = gtk_radio_button_new_with_label(NULL, "Normal");
    trig->auto_button =
	gtk_radio_button_new_with_label(gtk_radio_button_group
	(GTK_RADIO_BUTTON(trig->normal_button)), "Auto");
    /* and a regular button */
    trig->force_button = gtk_button_new_with_label("Force");
    /* now put them into the box */
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_mode_win),
	trig->normal_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_mode_win),
	trig->auto_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_info_win),
	trig->force_button, FALSE, FALSE, 0);
    /* hook callbacks to buttons */
    gtk_signal_connect(GTK_OBJECT(trig->normal_button), "clicked",
	GTK_SIGNAL_FUNC(normal_button_clicked), NULL);
    gtk_signal_connect(GTK_OBJECT(trig->auto_button), "clicked",
	GTK_SIGNAL_FUNC(auto_button_clicked), NULL);
    gtk_signal_connect(GTK_OBJECT(trig->force_button), "clicked",
	GTK_SIGNAL_FUNC(force_button_clicked), NULL);
    /* and make them visible */
    gtk_widget_show(trig->normal_button);
    gtk_widget_show(trig->auto_button);
    gtk_widget_show(trig->force_button);
}

static void init_trigger_info_window(void)
{
    scope_trig_t *trig;
    GtkWidget *hbox, *vbox;

    /* make a pointer to the trigger structure */
    trig = &(ctrl_usr->trig);
    /* box for the two sliders */
    hbox =
	gtk_hbox_new_in_box(TRUE, 0, 0, ctrl_usr->trig_info_win, TRUE, TRUE,
	0);
    /* box for the level slider */
    vbox = gtk_vbox_new_in_box(FALSE, 0, 0, hbox, TRUE, TRUE, 0);
    gtk_label_new_in_box("Level", vbox, FALSE, FALSE, 0);
    trig->level_adj =
	gtk_adjustment_new(TRIG_LEVEL_RESOLUTION / 2, 0,
	TRIG_LEVEL_RESOLUTION, 1, 1, 0);
    trig->level_slider = gtk_vscale_new(GTK_ADJUSTMENT(trig->level_adj));
    gtk_scale_set_digits(GTK_SCALE(trig->level_slider), 0);
    gtk_scale_set_draw_value(GTK_SCALE(trig->level_slider), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), trig->level_slider, TRUE, TRUE, 0);
    /* set initial trigger level */
    trig->level =
	(GTK_ADJUSTMENT(trig->level_adj))->value / TRIG_LEVEL_RESOLUTION;
    /* connect the slider to a function that re-sets the trigger level */
    gtk_signal_connect(GTK_OBJECT(trig->level_adj), "value_changed",
	GTK_SIGNAL_FUNC(level_changed), NULL);
    gtk_widget_show(trig->level_slider);
    /* box for the position slider */
    vbox = gtk_vbox_new_in_box(FALSE, 0, 0, hbox, TRUE, TRUE, 0);
    gtk_label_new_in_box("Pos", vbox, FALSE, FALSE, 0);
    trig->pos_adj =
	gtk_adjustment_new(TRIG_POS_RESOLUTION / 2, 0, TRIG_POS_RESOLUTION, 1,
	1, 0);
    trig->pos_slider = gtk_vscale_new(GTK_ADJUSTMENT(trig->pos_adj));
    gtk_scale_set_digits(GTK_SCALE(trig->pos_slider), 0);
    gtk_scale_set_draw_value(GTK_SCALE(trig->pos_slider), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), trig->pos_slider, TRUE, TRUE, 0);
    /* set initial trigger position */
    trig->position =
	(GTK_ADJUSTMENT(trig->pos_adj))->value / TRIG_POS_RESOLUTION;
    /* connect the slider to a function that re-sets trigger position */
    gtk_signal_connect(GTK_OBJECT(trig->pos_adj), "value_changed",
	GTK_SIGNAL_FUNC(pos_changed), NULL);
    gtk_widget_show(trig->pos_slider);
    /* level display */
    gtk_hseparator_new_in_box(ctrl_usr->trig_info_win, 3);
    gtk_label_new_in_box("Level", ctrl_usr->trig_info_win, FALSE, FALSE, 0);
    trig->level_label =
	gtk_label_new_in_box(" ---- ", ctrl_usr->trig_info_win, FALSE, FALSE,
	0);
    /* define a button to set the trigger edge */
    ctrl_shm->trig_edge = 1;
    trig->edge_button = gtk_button_new_with_label("Rising");
    trig->edge_label = (GTK_BIN(trig->edge_button))->child;
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_info_win),
	trig->edge_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(trig->edge_button), "clicked",
	GTK_SIGNAL_FUNC(edge_button_clicked), NULL);
    gtk_widget_show(trig->edge_button);
    /* define a button to set the trigger source */
    trig->source_button = gtk_button_new_with_label("Source\nNone");
    trig->source_label = (GTK_BIN(trig->source_button))->child;
    gtk_box_pack_start(GTK_BOX(ctrl_usr->trig_info_win),
	trig->source_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(trig->source_button), "clicked",
	GTK_SIGNAL_FUNC(source_button_clicked), NULL);
    gtk_widget_show(trig->source_button);
}

static void dialog_select_trigger_source(void)
{
    scope_vert_t *vert;
    scope_trig_t *trig;
    dialog_generic_t dialog;
    gchar *title, *msg;
    int n, colwidth;
    gchar *strs[2], *titles[2];
    gchar buf[BUFLEN + 1];
    GtkWidget *label, *button, *scrolled_window, *trig_list;

    /* is acquisition in progress? */
    if (ctrl_shm->state != IDLE) {
	/* yes, 'push' the stop button */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl_usr->
		rm_stop_button), TRUE);
    }
    vert = &(ctrl_usr->vert);
    trig = &(ctrl_usr->trig);
    title = "Trigger Source";
    msg = "Select a channel to use for triggering.";
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
    /* Create a scrolled window to display the list */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
	GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->vbox),
	scrolled_window, TRUE, TRUE, 5);
    gtk_widget_show(scrolled_window);
    /* create a list to hold the data */
    titles[0] = "Chan";
    titles[1] = "Source";
    trig_list = gtk_clist_new_with_titles(2, titles);
    gtk_clist_column_titles_passive(GTK_CLIST(trig_list));
    /* set up a callback for when the user selects a line */
    gtk_signal_connect(GTK_OBJECT(trig_list), "select_row",
	GTK_SIGNAL_FUNC(trigger_selection_made), &dialog);
    /* It isn't necessary to shadow the border, but it looks nice :) */
    gtk_clist_set_shadow_type(GTK_CLIST(trig_list), GTK_SHADOW_OUT);
    /* set list for single selection only */
    gtk_clist_set_selection_mode(GTK_CLIST(trig_list), GTK_SELECTION_BROWSE);
    /* put the list into the scrolled window */
    gtk_container_add(GTK_CONTAINER(scrolled_window), trig_list);
    gtk_widget_show(trig_list);
    /* populate the trigger source list */
    gtk_clist_clear(GTK_CLIST(trig_list));
    for (n = 0; n < 16; n++) {
	snprintf(buf, BUFLEN, "%d", n + 1);
	strs[0] = buf;
	if (ctrl_usr->chan[n].name != NULL) {
	    strs[1] = ctrl_usr->chan[n].name;
	} else {
	    strs[1] = "----";
	}
	gtk_clist_append(GTK_CLIST(trig_list), strs);
    }
    /* set column width */
    colwidth = gtk_clist_optimal_column_width(GTK_CLIST(trig_list), 1);
    gtk_clist_set_column_min_width(GTK_CLIST(trig_list), 1,
	(colwidth * 17) / 16);
    /* was a channel previously selected? */
    if (ctrl_shm->trig_chan > 0) {
	/* yes, preselect appropriate line */
	gtk_clist_select_row(GTK_CLIST(trig_list), ctrl_shm->trig_chan - 1,
	    1);
    }
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
    trig_list = NULL;
    if ((dialog.retval == 0) || (dialog.retval == 2)) {
	/* user either closed dialog, or hit cancel */
	return;
    }
    /* user made a selection */
    if (ctrl_usr->chan[ctrl_shm->trig_chan - 1].name == NULL) {
	/* selected channel has no source */
	ctrl_shm->trig_chan = 0;
    }
    refresh_trigger();
}

/* If we come here, then the user has selected a row in the list. */
static void trigger_selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, dialog_generic_t * dptr)
{
    GdkEventType type;

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
    ctrl_shm->trig_chan = row + 1;
    /* set return value of dialog */
    dptr->retval = 1;
    /* destroy window to cause dialog_generic_destroyed() to be called */
    gtk_widget_destroy(dptr->window);
    return;
}

static void level_changed(GtkAdjustment * adj, gpointer gdata)
{
    scope_trig_t *trig;

    trig = &(ctrl_usr->trig);
    trig->level = adj->value / TRIG_LEVEL_RESOLUTION;
    if ((ctrl_shm->trig_chan < 1) || (ctrl_shm->trig_chan > 16)) {
	return;
    }
    refresh_trigger();
}

static void pos_changed(GtkAdjustment * adj, gpointer gdata)
{
    scope_trig_t *trig;

    trig = &(ctrl_usr->trig);
    /* is acquisition in progress? */
    if (ctrl_shm->state != IDLE) {
	/* yes, 'push' the stop button */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl_usr->
		rm_stop_button), TRUE);
    }
    trig->position = adj->value / TRIG_POS_RESOLUTION;
    refresh_trigger();
}

static void edge_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (ctrl_shm->trig_edge == 0) {
	/* was falling edge, make rising */
	ctrl_shm->trig_edge = 1;
    } else {
	/* was rising edge, make falling */
	ctrl_shm->trig_edge = 0;
    }
    refresh_trigger();
}

static void normal_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    ctrl_shm->auto_trig = 0;
}

static void auto_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != TRUE) {
	/* not pressed, ignore it */
	return;
    }
    ctrl_shm->auto_trig = 1;
}

static void force_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    ctrl_shm->force_trig = 1;
}

static void source_button_clicked(GtkWidget * widget, gpointer * gdata)
{
    dialog_select_trigger_source();
}
