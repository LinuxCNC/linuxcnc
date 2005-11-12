/** This file, 'halmeter.c', is a GUI program that serves as a simple
    meter to look at HAL signals.  It is a user space component and
    uses GTK 1.2 for the GUI code.  It allows you to view one pin,
    signal, or parameter, and updates its display about 10 times
    per second.  (It is not a realtime program, and heavy loading
    can temporarily slow or stop the update.)  Clicking on the 'Select'
    button pops up a dialog that allows you to select what pin/signal/
    parameter you want to monitor.  Multiple instances of the program
    can be started if you want to monitor more than one item.  If you
    add "pin|sig|par[am] <name>" to the command line, the meter will
    initially display the pin/signal/parameter <name>, otherwise it
    will initially display nothing.
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
#include <signal.h>
#include <ctype.h>
#include <string.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "../hal_priv.h"	/* private HAL decls */

#include <gtk/gtk.h>
#include "miscgtk.h"		/* generic GTK stuff */

/***********************************************************************
*                            TYPEDEFS                                  *
************************************************************************/

/** a 'probe' is an object that references a HAL pin, signal, or
    parameter.  The user may select the item that is to be probed.
*/

#define PROBE_NAME_LEN 63

typedef struct {
    int listnum;		/* 0 = pin, 1 = signal, 2 = parameter */
    char *pickname;		/* name from list, not validated */
    hal_pin_t *pin;		/* metadata (if it's a pin) */
    hal_sig_t *sig;		/* metadata (if it's a signal) */
    hal_param_t *param;		/* metadata (if it's a parameter) */
    GtkWidget *window;		/* selection dialog window */
    GtkWidget *notebook;	/* pointer to the notebook */
    GtkWidget *lists[3];	/* lists for pins, sigs, and params */
    char probe_name[PROBE_NAME_LEN + 1];	/* name of this probe */
} probe_t;

typedef struct {
    probe_t *probe;		/* probe that locates the data */
    GtkWidget *value_label;	/* label object to display value */
    GtkWidget *name_label;	/* label object to display name */
} meter_t;

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

int comp_id;			/* HAL component ID */

GtkWidget *main_window;

meter_t *meter;

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

static meter_t *meter_new(void);

/** 'probe_new()' creates a new probe structure.  It also creates
    a dialog window for the probe that allows the user to pick the
    pin, signal, or parameter that the probe will attach to.  It
    should be called during the init phase of the program, before
    the main event loop is started.
*/
static probe_t *probe_new(char *probe_name);

/** 'popup_probe_window()' is an event handler function that opens
    the selection dialog for a probe.  'data' must be a pointer to
    a probe_t structure that was allocated by 'probe_new'.
*/
static void popup_probe_window(GtkWidget * widget, gpointer data);

static void quit(int sig);
static void exit_from_hal(void);
static int refresh_value(gpointer data);
static char *data_value(int type, void *valptr);

static void create_probe_window(probe_t * probe);
static void accept_selection(GtkWidget * widget, gpointer data);
static void accept_selection_and_close(GtkWidget * widget, gpointer data);
static void close_selection(GtkWidget * widget, gpointer data);
static void selection_made(GtkWidget * clist, gint row, gint column,
    GdkEventButton * event, gpointer data);

/***********************************************************************
*                        MAIN() FUNCTION                               *
************************************************************************/

int main(int argc, gchar * argv[])
{
    GtkWidget *vbox, *hbox;
    GtkWidget *button_select, *button_exit;
    char buf[30];
    int initial_type;
    char *initial_name;

    /* process and remove any GTK specific command line args */
    gtk_init(&argc, &argv);

    /* process my own command line args (if any) here */
    if (argc == 3) {
	/* check for user specified initial probe point */
	if (strncmp(argv[1], "pin", 3) == 0) {
	    /* initial probe is a pin */
	    initial_type = 0;
	} else if (strncmp(argv[1], "sig", 3) == 0) {
	    /* initial probe is a signal */
	    initial_type = 1;
	} else if (strncmp(argv[1], "par", 3) == 0) {
	    /* initial probe is a parameter */
	    initial_type = 2;
	} else {
	    printf("ERROR: '%s' is not a valid probe type\n", argv[1]);
	    return -1;
	}
	initial_name = argv[2];
    } else {
	initial_type = 0;
	initial_name = NULL;
    }

    /* create a unique module name */
    snprintf(buf, 29, "meter%d", getpid());
    /* connect to the HAL */
    comp_id = hal_init(buf);
    if (comp_id < 0) {
	return -1;
    }
    /* register an exit function to disconnect from the HAL */
    g_atexit(exit_from_hal);
    /* capture INT (ctrl-C) and TERM signals */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

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

    /* create a meter object */
    meter = meter_new();
    if (meter == NULL) {
	exit(-1);
    }
    
    /* set up for initial probe, if any */
    if (initial_name != NULL) {
	meter->probe->pickname = initial_name;
	meter->probe->listnum = initial_type;
	accept_selection(NULL, meter->probe);
    }

    /* add the meter's value label to the vbox */
    gtk_box_pack_start(GTK_BOX(vbox), meter->value_label, TRUE, TRUE, 0);
    gtk_widget_show(meter->value_label);

    /* add the meter's name label to the vbox */
    gtk_box_pack_start(GTK_BOX(vbox), meter->name_label, TRUE, TRUE, 0);
    gtk_widget_show(meter->name_label);

    /* arrange for periodic refresh of the value */
    gtk_timeout_add(100, refresh_value, meter);

    /* an hbox to hold the select and exit buttons */
    hbox = gtk_hbox_new_in_box(FALSE, 0, 0, vbox, FALSE, TRUE, 0);

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
	GTK_SIGNAL_FUNC(popup_probe_window), meter->probe);

    gtk_widget_show(button_select);
    gtk_widget_show(button_exit);

    /* The interface is completely set up so we show the window and enter the
       gtk_main loop. */
    gtk_widget_show(main_window);
    gtk_main();

    return (0);
}

/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

static meter_t *meter_new(void)
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
    new->listnum = -1;
    new->pin = NULL;
    new->sig = NULL;
    new->param = NULL;
    strncpy(new->probe_name, probe_name, HAL_NAME_LEN);
    /* window will be created just before it is displayed */
    new->window = NULL;
    /* done */
    return new;
}

void popup_probe_window(GtkWidget * widget, gpointer data)
{
    probe_t *probe;
    int next, row;
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
    
    /* this selects the page holding the current selected probe */
    if (probe->listnum >= 0)
	gtk_notebook_set_page((GtkNotebook *)probe->notebook, probe->listnum);
    
    gtk_clist_clear(GTK_CLIST(probe->lists[0]));
    gtk_clist_clear(GTK_CLIST(probe->lists[1]));
    gtk_clist_clear(GTK_CLIST(probe->lists[2]));
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    row = 0;
    while (next != 0) {
	pin = SHMPTR(next);
	name = pin->name;
	gtk_clist_append(GTK_CLIST(probe->lists[0]), &name);

	/* if we have a pin selected, and it matches the current one, mark this row) */
	if ((probe->listnum == 0) && (probe->pin == pin)) {
	    gtk_clist_select_row(GTK_CLIST(probe->lists[0]), row, 0);
	    /* Get the text from the list */
	    gtk_clist_get_text(GTK_CLIST(probe->lists[0]), row, 0, &(probe->pickname));
	}
	
	next = pin->next_ptr;
	row++;
    }
    next = hal_data->sig_list_ptr;
    row = 0;
    while (next != 0) {
	sig = SHMPTR(next);
	name = sig->name;
	gtk_clist_append(GTK_CLIST(probe->lists[1]), &name);

	/* if we have a signal selected, and it matches the current one, mark this row) */
	if ((probe->listnum == 1) && (probe->sig == sig)) {
	    gtk_clist_select_row(GTK_CLIST(probe->lists[1]), row, 0);
	    /* Get the text from the list */
	    gtk_clist_get_text(GTK_CLIST(probe->lists[1]), row, 0, &(probe->pickname));
	}

	next = sig->next_ptr;
	row++;
    }
    next = hal_data->param_list_ptr;
    row = 0;
    while (next != 0) {
	param = SHMPTR(next);
	name = param->name;
	gtk_clist_append(GTK_CLIST(probe->lists[2]), &name);

	/* if we have a param selected, and it matches the current one, mark this row) */
	if ((probe->listnum == 2) && (probe->param == param)) {
	    gtk_clist_select_row(GTK_CLIST(probe->lists[2]), row, 0);
	    /* Get the text from the list */
	    gtk_clist_get_text(GTK_CLIST(probe->lists[2]), row, 0, &(probe->pickname));
	}
	
	next = param->next_ptr;
	row++;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    gtk_widget_show_all(probe->window);
}

static void quit(int sig)
{
    gtk_main_quit();
}

static void exit_from_hal(void)
{
    hal_exit(comp_id);
}

/* this function refreshes the value display */
static int refresh_value(gpointer data)
{
    meter_t *meter;
    probe_t *probe;
    char *value_str, *name_str;
    hal_sig_t *sig;

    meter = (meter_t *) data;
    probe = meter->probe;

    if (probe->pin != NULL) {
	if (probe->pin->name[0] == '\0') {
	    /* pin has been deleted, can't display it any more */
	    probe->pin = NULL;
	    return 1;
	}
	name_str = probe->pin->name;
	if (probe->pin->signal == 0) {
	    /* pin is unlinked, get data from dummysig */
	    value_str = data_value(probe->pin->type, &(probe->pin->dummysig));
	} else {
	    /* pin is linked to a signal */
	    sig = SHMPTR(probe->pin->signal);
	    value_str = data_value(probe->pin->type, SHMPTR(sig->data_ptr));
	}
    } else if (probe->sig != NULL) {
	if (probe->sig->name[0] == '\0') {
	    /* signal has been deleted, can't display it any more */
	    probe->sig = NULL;
	    return 1;
	}
	name_str = probe->sig->name;
	value_str =
	    data_value(probe->sig->type, SHMPTR(probe->sig->data_ptr));
    } else if (probe->param != NULL) {
	if (probe->param->name[0] == '\0') {
	    /* parameter has been deleted, can't display it any more */
	    probe->param = NULL;
	    return 1;
	}
	name_str = probe->param->name;
	value_str =
	    data_value(probe->param->type, SHMPTR(probe->param->data_ptr));
    } else {
	name_str = "-----";
	value_str = "---";
    }
    gtk_label_set_text(GTK_LABEL(meter->value_label), value_str);
    gtk_label_set_text(GTK_LABEL(meter->name_label), name_str);
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

static void create_probe_window(probe_t * probe)
{
    GtkWidget *vbox, *hbox, *notebk;
    GtkWidget *button_OK, *button_accept, *button_cancel;
    GtkWidget *scrolled_window;
    gchar *tab_label_text[3];
    gint n;

    /* create window, set it's size */
    probe->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_usize(GTK_WIDGET(probe->window), -2, 300);
    /* allow user to grow but not shrink the window */
    gtk_window_set_policy(GTK_WINDOW(probe->window), FALSE, TRUE, FALSE);
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

    /* create a notebook to hold pin, signal, and parameter lists */
    notebk = gtk_notebook_new();
    /* remember the notebook so we can change the pages later */
    probe->notebook = notebk;
    /* add the notebook to the window */
    gtk_box_pack_start(GTK_BOX(vbox), notebk, TRUE, TRUE, 0);
    /* set overall notebook parameters */
    gtk_notebook_set_homogeneous_tabs(GTK_NOTEBOOK(notebk), TRUE);
    /* text for tab labels */
    tab_label_text[0] = " Pins ";
    tab_label_text[1] = " Signals ";
    tab_label_text[2] = " Parameters ";
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

    /* an hbox to hold the OK, accept, and cancel buttons */
    hbox = gtk_hbox_new_in_box(TRUE, 0, 0, vbox, FALSE, TRUE, 0);

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

    /* done */
}

static void accept_selection(GtkWidget * widget, gpointer data)
{
    probe_t *probe;

    /* get a pointer to the probe data structure */
    probe = (probe_t *) data;
    /* discard info about previous item */
    probe->pin = NULL;
    probe->sig = NULL;
    probe->param = NULL;
    if (probe->pickname == NULL) {
	/* not a valid selection */
	/* should pop up a message or something here, instead we ignore it */
	return;
    }
    if (probe->listnum == 0) {
	/* search the pin list */
	probe->pin = halpr_find_pin_by_name(probe->pickname);
    } else if (probe->listnum == 1) {
	/* search the signal list */
	probe->sig = halpr_find_sig_by_name(probe->pickname);
    } else if (probe->listnum == 2) {
	/* search the parameter list */
	probe->param = halpr_find_param_by_name(probe->pickname);
    }
    /* at this point, the probe structure contain a pointer to the item we
       wish to display, or all three are NULL if the item doesn't exist */
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
