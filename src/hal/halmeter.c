/** This file, 'halmeter.c', is a GUI program that serves as a simple
    meter to look at HAL signals.  It is a user space component and
    uses GTK 1.2 for the GUI code.  It allows you to view one pin,
    signal, or parameter, and updates its display about 10 times
    per second.  (It is not a realtime program, and heavy loading
    can temporarily slow or stop the update.)  Clicking on the 'Select'
    button pops up a dialog that allows you to select what pin/signal/
    parameter you want to monitor.  Multiple instances of the program
    can be started if you want to monitor more than one item.
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

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

int comp_id;			/* HAL component ID */

GtkWidget *main_window;
GtkWidget *select_window = NULL;
GtkWidget *value_label;
GtkWidget *name_label;
GtkWidget *lists[3];
char *selected_name = NULL;
int selected_list = -1;
hal_type_t selected_type = -1;
void *selected_data = NULL;

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

static void exit_from_hal(void);
static int refresh_value(gpointer data);
static char *data_value(int type, void *valptr);
static void define_select_window(void);
static void popup_select_window(GtkWidget * widget, gpointer data);
static void accept_selection(GtkWidget * widget, gpointer data);
static void accept_selection_and_close(GtkWidget * widget, gpointer data);
static void close_selection(GtkWidget * widget, gpointer data);
void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, gpointer data);

/***********************************************************************
*                        MAIN() FUNCTION                               *
************************************************************************/

int main(int argc, gchar * argv[])
{
    GtkWidget *vbox, *hbox;
    GtkWidget *button_select, *button_exit;
    char buf[30];

    /* process and remove any GTK specific command line args */
    gtk_init(&argc, &argv);

    /* process my own command line args (if any) here */
    /* create a unique module name */
    snprintf(buf, 29, "meter%d", getpid());
    /* connect to the HAL */
    comp_id = hal_init(buf);
    if (comp_id < 0) {
	return -1;
    }
    /* register an exit function to disconnect from the HAL */
    g_atexit(exit_from_hal);

    /* create main window, set it's size, and lock the size */
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_usize(GTK_WIDGET(main_window), 220, 80);
    gtk_window_set_policy(GTK_WINDOW(main_window), FALSE, FALSE, FALSE);
    /* set main window title */
    gtk_window_set_title(GTK_WINDOW(main_window), "HAL Meter");
    /* this makes the application exit when the window is closed */
    gtk_signal_connect(GTK_OBJECT(main_window), "destroy",
	GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

    /* a vbox to hold the displayed value and the pin/sig/param name */
    vbox = gtk_vbox_new(FALSE, 3);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    /* add the vbox to the main window */
    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    gtk_widget_show(vbox);

    /* create a label widget to hold the value */
    value_label = gtk_label_new("----");
    /* center justify text, no wordwrap */
    gtk_label_set_justify(GTK_LABEL(value_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(value_label), FALSE);
    /* add the label to the vbox */
    gtk_box_pack_start(GTK_BOX(vbox), value_label, TRUE, TRUE, 0);
    /* arrange for periodic refresh of the value */
    gtk_timeout_add(100, refresh_value, value_label);
    gtk_widget_show(value_label);

    /* create a label widget to hold the name */
    name_label = gtk_label_new("------");
    /* center justify text, no wordwrap */
    gtk_label_set_justify(GTK_LABEL(name_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(name_label), FALSE);

    gtk_box_pack_start(GTK_BOX(vbox), name_label, TRUE, TRUE, 0);
    gtk_widget_show(name_label);

    /* an hbox to hold the select and exit buttons */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
    /* add the hbox to the vbox */
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    /* create the buttons and add them to the hbox */
    button_select = gtk_button_new_with_label("Select");
    button_exit = gtk_button_new_with_label("Exit");

    gtk_box_pack_start(GTK_BOX(hbox), button_select, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), button_exit, TRUE, TRUE, 4);

    /* make the application exit when the 'exit' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_exit), "clicked",
	GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

    /* activate the selection window when the 'select' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_select), "clicked",
	GTK_SIGNAL_FUNC(popup_select_window), NULL);

    gtk_widget_show(button_select);
    gtk_widget_show(button_exit);

    /* set up (but do not display) the selection window */
    define_select_window();

    /* The interface is completely set up so we show the window and enter the 
       gtk_main loop. */
    gtk_widget_show(main_window);
    gtk_main();

    return (0);
}

/* this function implements the select window */
static void define_select_window(void)
{
    GtkWidget *vbox, *hbox, *notebk;
    GtkWidget *button_OK, *button_accept, *button_cancel;
    GtkWidget *tab_label, *scrolled_window;
    gchar *tab_label_text[3];
    gint n;

    if (select_window != NULL) {
	/* window already exists, don't create another one */
	return;
    }
    /* create select window, set it's size, and leave it re-sizeable */
    select_window = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_widget_set_usize(GTK_WIDGET(select_window), 300, 400);
    gtk_window_set_policy(GTK_WINDOW(select_window), TRUE, TRUE, FALSE);
    /* window should appear in center of screen */
    gtk_window_set_position(GTK_WINDOW(select_window), GTK_WIN_POS_CENTER);
    /* set select window title */
    gtk_window_set_title(GTK_WINDOW(select_window), "Select Item");

    /* a vbox to hold stuff */
    vbox = gtk_vbox_new(FALSE, 3);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    /* add the vbox to the window */
    gtk_container_add(GTK_CONTAINER(select_window), vbox);
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

    /* activate the new selection 'OK' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_OK), "clicked",
	GTK_SIGNAL_FUNC(accept_selection_and_close), NULL);

    /* activate the new selection 'accept' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_accept), "clicked",
	GTK_SIGNAL_FUNC(accept_selection), NULL);

    /* make the window disappear 'cancel' button is clicked */
    gtk_signal_connect(GTK_OBJECT(button_cancel), "clicked",
	GTK_SIGNAL_FUNC(close_selection), NULL);

    gtk_widget_show(button_OK);
    gtk_widget_show(button_accept);
    gtk_widget_show(button_cancel);

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
	lists[n] = gtk_clist_new(1);
	/* set up a callback for when the user selects a line */
	gtk_signal_connect(GTK_OBJECT(lists[n]), "select_row",
	    GTK_SIGNAL_FUNC(selection_made), (gpointer) n);
	/* It isn't necessary to shadow the border, but it looks nice :) */
	gtk_clist_set_shadow_type(GTK_CLIST(lists[n]), GTK_SHADOW_OUT);
	/* set list for single selection only */
	gtk_clist_set_selection_mode(GTK_CLIST(lists[n]),
	    GTK_SELECTION_BROWSE);
	/* put the list into the scrolled window */
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
	    (scrolled_window), lists[n]);
	gtk_widget_show(lists[n]);
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

/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

static void exit_from_hal(void)
{
    hal_exit(comp_id);
}

/* this function refreshes the value display */
static int refresh_value(gpointer data)
{
    char *value_str;

    if (selected_data == NULL) {
	return 1;
    }
    value_str = data_value(selected_type, selected_data);
    gtk_label_set_text(GTK_LABEL(data), value_str);
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

/* this function displays the select window */
static void popup_select_window(GtkWidget * widget, gpointer data)
{
    int next;
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;
    gchar *name;

    gtk_clist_clear(GTK_CLIST(lists[0]));
    gtk_clist_clear(GTK_CLIST(lists[1]));
    gtk_clist_clear(GTK_CLIST(lists[2]));
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	name = pin->name;
	gtk_clist_append(GTK_CLIST(lists[0]), &name);
	next = pin->next_ptr;
    }
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	name = sig->name;
	gtk_clist_append(GTK_CLIST(lists[1]), &name);
	next = sig->next_ptr;
    }
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	name = param->name;
	gtk_clist_append(GTK_CLIST(lists[2]), &name);
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    gtk_widget_show_all(select_window);
}

static void accept_selection(GtkWidget * widget, gpointer data)
{
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;

    if (selected_name == NULL) {
	/* not a valid selection */
	/* should pop up a message or something here, instead we ignore it */
	return;
    }
    if (selected_list == 0) {
	/* search the pin list */
	pin = halpr_find_pin_by_name(selected_name);
	if (pin == NULL) {
	    /* pin not found (can happen if pin is deleted after the list was 
	       generated) */
	    /* again, error handling leaves a bit to be desired! */
	    return;
	}
	selected_type = pin->type;
	if (pin->signal == 0) {
	    /* pin is unlinked, get data from dummysig */
	    selected_data = &(pin->dummysig);
	} else {
	    /* pin is linked to a signal */
	    sig = SHMPTR(pin->signal);
	    selected_data = SHMPTR(sig->data_ptr);
	}
    } else if (selected_list == 1) {
	/* search the signal list */
	sig = halpr_find_sig_by_name(selected_name);
	if (sig == NULL) {
	    /* signal not found (can happen if signal is deleted after the
	       list was generated) */
	    return;
	}
	selected_type = sig->type;
	selected_data = SHMPTR(sig->data_ptr);
    } else if (selected_list == 2) {
	/* search the parameter list */
	param = halpr_find_param_by_name(selected_name);
	if (param == NULL) {
	    /* parameter not found (can happen if param is deleted after the
	       list was generated) */
	    return;
	}
	selected_type = param->type;
	selected_data = SHMPTR(param->data_ptr);
    } else {
	/* not one of our three lists - bad */
	return;
    }
    /* at this point, the selected_xxx globals identify the item we wish to
       display */
    gtk_label_set_text(GTK_LABEL(name_label), selected_name);
    refresh_value(value_label);
}

static void accept_selection_and_close(GtkWidget * widget, gpointer data)
{
    accept_selection(widget, data);
    close_selection(widget, data);
}

static void close_selection(GtkWidget * widget, gpointer data)
{
    gtk_widget_hide_all(select_window);
}

/* If we come here, then the user has selected a row in the list. */
void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, gpointer data)
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
    /* Get the text from the list, to the global 'selected_name' */
    gtk_clist_get_text(GTK_CLIST(clist), row, column, &selected_name);
    /* also store the list number (passed in 'data') */
    selected_list = (int) data;
    return;
}
