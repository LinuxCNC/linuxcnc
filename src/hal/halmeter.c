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
#include "halgtk.h"		/* HAL related GTK stuff */

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

int comp_id;			/* HAL component ID */

GtkWidget *main_window;

typedef struct {
    probe_t *probe;		/* probe that locates the data */
    GtkWidget *value_label;	/* label object to display value */
    GtkWidget *name_label;	/* label object to display name */
} meter_t;

meter_t *meter;

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

meter_t *meter_new(void);

static void exit_from_hal(void);
static int refresh_value(gpointer data);
static char *data_value(int type, void *valptr);

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

    /* create a meter object */
    meter = meter_new();
    if (meter == NULL) {
	exit(-1);
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
	GTK_SIGNAL_FUNC(popup_probe_window), meter->probe);

    gtk_widget_show(button_select);
    gtk_widget_show(button_exit);

    /* The interface is completely set up so we show the window and enter the
       gtk_main loop. */
    gtk_widget_show(main_window);
    gtk_main();

    return (0);
}

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
