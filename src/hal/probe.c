/** This file, 'set_p[robe.c', implements a dialog that allows you to
    select a signal, pin, or parameter.  It is used by both halmeter
    and halscope to select the item(s) to be observed.
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
#include "halgtk.h"		/* HAL related GTK stuff */

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

static void create_probe_window(probe_t * probe);
static void accept_selection(GtkWidget * widget, gpointer data);
static void accept_selection_and_close(GtkWidget * widget, gpointer data);
static void close_selection(GtkWidget * widget, gpointer data);
static void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, gpointer data);

/***********************************************************************
*                        MAIN() FUNCTION                               *
************************************************************************/

probe_t *probe_new(char *probe_name)
{
    probe_t *new;

    if (probe_name != NULL) {
	/* no name specified, fake it */
	probe_name = "Select Item to Probe";
    }
    /* allocate a new probe structure */
    new = malloc(sizeof(probe_t));
    if (new == NULL) {
	return NULL;
    }
    /* init the fields */
    new->pickname = NULL;
    new->name = NULL;
    new->type = 0;
    new->data = NULL;
    new->listnum = -1;
    strncpy(new->probe_name, probe_name, HAL_NAME_LEN);
    /* window will be created just before it is displayed */
    new->window = NULL;
    /* done */
    return new;
}

/* this function displays the probe window */
void popup_probe_window(GtkWidget * widget, gpointer data)
{
    probe_t *probe;
    int next;
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;
    gchar *name;

    /* get a pointer to the probe data structure */
    probe = (probe_t *) data;

    /* create window if needed */
    if (probe->window == NULL) {
	create_probe_window(probe);
    }
    gtk_clist_clear(GTK_CLIST(probe->lists[0]));
    gtk_clist_clear(GTK_CLIST(probe->lists[1]));
    gtk_clist_clear(GTK_CLIST(probe->lists[2]));
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	name = pin->name;
	gtk_clist_append(GTK_CLIST(probe->lists[0]), &name);
	next = pin->next_ptr;
    }
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	name = sig->name;
	gtk_clist_append(GTK_CLIST(probe->lists[1]), &name);
	next = sig->next_ptr;
    }
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	name = param->name;
	gtk_clist_append(GTK_CLIST(probe->lists[2]), &name);
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    gtk_widget_show_all(probe->window);
}

/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

static void create_probe_window(probe_t * probe)
{
    GtkWidget *vbox, *hbox, *notebk;
    GtkWidget *button_OK, *button_accept, *button_cancel;
    GtkWidget *tab_label, *scrolled_window;
    gchar *tab_label_text[3];
    gint n;

    /* create window, set it's size, and leave it re-sizeable */
    probe->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_usize(GTK_WIDGET(probe->window), 300, 400);
    gtk_window_set_policy(GTK_WINDOW(probe->window), TRUE, TRUE, FALSE);
    /* window should appear in center of screen */
    gtk_window_set_position(GTK_WINDOW(probe->window), GTK_WIN_POS_CENTER);
    /* set set_probe window title */
    gtk_window_set_title(GTK_WINDOW(probe->window), probe->probe_name);

    /* a vbox to hold everything */
    vbox = gtk_vbox_new(FALSE, 3);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    /* add the vbox to the window */
    gtk_container_add(GTK_CONTAINER(probe->window), vbox);
    gtk_widget_show(vbox);

    /* an hbox to hold the OK, accept, and cancel buttons */
    hbox = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
    /* add the hbox to the vbox */
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    /* create the buttons and add them to the hbox */
    button_OK = gtk_button_new_with_label("OK");
    button_accept = gtk_button_new_with_label("Accept");
    button_cancel = gtk_button_new_with_label("Cancel");

    gtk_box_pack_start(GTK_BOX(hbox), button_OK, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), button_accept, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), button_cancel, TRUE, TRUE, 4);

    /* activate the new selection if 'OK' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_OK), "clicked",
	GTK_SIGNAL_FUNC(accept_selection_and_close), probe);

    /* activate the new selection if 'accept' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_accept), "clicked",
	GTK_SIGNAL_FUNC(accept_selection), probe);

    /* make the window disappear if 'cancel' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_cancel), "clicked",
	GTK_SIGNAL_FUNC(close_selection), probe);

    gtk_widget_show(button_OK);
    gtk_widget_show(button_accept);
    gtk_widget_show(button_cancel);

    /* set probe->window to NULL if window is destroyed */
    gtk_signal_connect(GTK_OBJECT(probe->window), "destroy",
	GTK_SIGNAL_FUNC(gtk_widget_destroyed), &(probe->window));

    /* create a notebook to hold pin, signal, and parameter lists */
    notebk = gtk_notebook_new();
    /* add the notebook to the window */
    gtk_box_pack_start(GTK_BOX(vbox), notebk, TRUE, TRUE, 0);
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
	probe->lists[n] = gtk_clist_new(1);
	/* set up a callback for when the user selects a line */
	gtk_signal_connect(GTK_OBJECT(probe->lists[n]), "select_row",
	    GTK_SIGNAL_FUNC(selection_made), probe);
	/* It isn't necessary to shadow the border, but it looks nice :) */
	gtk_clist_set_shadow_type(GTK_CLIST(probe->lists[n]), GTK_SHADOW_OUT);
	/* set list for single selection only */
	gtk_clist_set_selection_mode(GTK_CLIST(probe->lists[n]),
	    GTK_SELECTION_BROWSE);
	/* put the list into the scrolled window */
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
	    (scrolled_window), probe->lists[n]);
	gtk_widget_show(probe->lists[n]);
	/* create a label for the page */
	tab_label = gtk_label_new(tab_label_text[n]);
	gtk_label_set_justify(GTK_LABEL(tab_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap(GTK_LABEL(tab_label), FALSE);
	gtk_widget_show(tab_label);
	/* create a box for the tab label */
	hbox = gtk_hbox_new(TRUE, 0);
	/* add the label to the box */
	gtk_box_pack_start(GTK_BOX(hbox), tab_label, TRUE, TRUE, 0);
	gtk_widget_show(hbox);
	/* add page to the notebook */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebk), scrolled_window, hbox);
	/* set tab attributes */
	gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK(notebk), hbox,
	    TRUE, TRUE, GTK_PACK_START);
    }
    /* done */
    gtk_widget_show(notebk);
}

static void accept_selection(GtkWidget * widget, gpointer data)
{
    probe_t *probe;
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;

    /* get a pointer to the probe data structure */
    probe = (probe_t *) data;

    if (probe->pickname == NULL) {
	/* not a valid selection */
	/* should pop up a message or something here, instead we ignore it */
	return;
    }
    if (probe->listnum == 0) {
	/* search the pin list */
	pin = halpr_find_pin_by_name(probe->pickname);
	if (pin == NULL) {
	    /* pin not found (can happen if pin is deleted after the list was
	       generated) */
	    /* again, error handling leaves a bit to be desired! */
	    return;
	}
	probe->type = pin->type;
	probe->name = pin->name;
	if (pin->signal == 0) {
	    /* pin is unlinked, get data from dummysig */
	    probe->data = &(pin->dummysig);
	} else {
	    /* pin is linked to a signal */
	    sig = SHMPTR(pin->signal);
	    probe->data = SHMPTR(sig->data_ptr);
	}
    } else if (probe->listnum == 1) {
	/* search the signal list */
	sig = halpr_find_sig_by_name(probe->pickname);
	if (sig == NULL) {
	    /* signal not found (can happen if signal is deleted after the
	       list was generated) */
	    return;
	}
	probe->type = sig->type;
	probe->name = sig->name;
	probe->data = SHMPTR(sig->data_ptr);
    } else if (probe->listnum == 2) {
	/* search the parameter list */
	param = halpr_find_param_by_name(probe->pickname);
	if (param == NULL) {
	    /* parameter not found (can happen if param is deleted after the
	       list was generated) */
	    return;
	}
	probe->type = param->type;
	probe->name = param->name;
	probe->data = SHMPTR(param->data_ptr);
    } else {
	/* not one of our three lists - bad */
	return;
    }
    /* at this point, the probe structure contains pointers to the item we
       wish to display */
}

static void accept_selection_and_close(GtkWidget * widget, gpointer data)
{
    accept_selection(widget, data);
    close_selection(widget, data);
}

static void close_selection(GtkWidget * widget, gpointer data)
{
    probe_t *probe;

    /* get a pointer to the probe data structure */
    probe = (probe_t *) data;
    /* hide the window */
    gtk_widget_hide_all(probe->window);
}

/* If we come here, then the user has selected a row in the list. */
static void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, gpointer data)
{
    probe_t *probe;
    GdkEventType type;
    gint n;

    /* get a pointer to the probe data structure */
    probe = (probe_t *) data;

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
    /* figure out which notebook tab it was */
    for (n = 0; n < 3; n++) {
	if (clist == probe->lists[n]) {
	    probe->listnum = n;
	}
    }
    /* Get the text from the list */
    gtk_clist_get_text(GTK_CLIST(clist), row, column, &(probe->pickname));
    return;
}
