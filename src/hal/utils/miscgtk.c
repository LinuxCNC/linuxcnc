/** This file, 'miscgtk.c', contains code for some generic GTK
    functions as declared in 'miscgtk.h'.  This includes new
    widgets and other items of a general nature.

    It is also used for "compatibility code" needed to support
    different versions of GTK.  Currently, GTK-1.2 is the oldest
    version supported.  As GTK progresses and more version 1.2 APIs
    are deprecated, support for pre-2.0 versions may eventually be
    dropped.
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

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <gtk/gtk.h>
#include "miscgtk.h"		/* decls for this code */

/***********************************************************************
*                  GLOBAL VARIABLES DECLARATIONS                       *
************************************************************************/

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

/***********************************************************************
*                    PUBLIC FUNCTION DEFINITIONS                       *
************************************************************************/

#if !GTK_CHECK_VERSION(2,0,0)
void gtk_widget_set_double_buffered( GtkWidget *widget, gboolean double_buffered) {
    // does nothing
}

void gtk_widget_modify_fg( GtkWidget *widget, GtkStateType state, const GdkColor *color) {
    gtk_widget_ensure_style(widget);
    gtk_widget_set_style(widget, gtk_style_copy(widget->style));
    widget->style->fg[state] = *color;
}

void gtk_widget_modify_bg( GtkWidget *widget, GtkStateType state, const GdkColor *color) {
    gtk_widget_ensure_style(widget);
    gtk_widget_set_style(widget, gtk_style_copy(widget->style));
    widget->style->bg[state] = *color;
}

GdkFont* gtk_style_get_font(GtkStyle *style) {
    return (style)->font;
}
#endif

#if !GTK_CHECK_VERSION(2,8,0)
void gtk_window_set_urgency_hint( GtkWindow *window, gboolean state ) { }
gboolean gtk_window_is_active( GtkWindow *window ) { return FALSE; }
#endif


GtkWidget *gtk_label_new_in_box(const gchar * text, GtkWidget * box,
    gboolean expand, gboolean fill, guint padding)
{
    GtkWidget *label;

    label = gtk_label_new(text);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
    gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(box), label, expand, fill, padding);
    gtk_widget_show(label);
    return label;
}

void gtk_vseparator_new_in_box(GtkWidget * box, guint padding)
{
    GtkWidget *bar;

    bar = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box), bar, FALSE, FALSE, padding);
    gtk_widget_show(bar);
}

void gtk_hseparator_new_in_box(GtkWidget * box, guint padding)
{
    GtkWidget *bar;

    bar = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(box), bar, FALSE, FALSE, padding);
    gtk_widget_show(bar);
}

GtkWidget *gtk_vbox_new_in_box(gboolean homogeneous, guint spacing,
    guint border, GtkWidget * box, gboolean expand, gboolean fill,
    guint padding)
{
    GtkWidget *vbox;

    vbox = gtk_vbox_new(homogeneous, spacing);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), border);
    gtk_box_pack_start(GTK_BOX(box), vbox, expand, fill, padding);
    gtk_widget_show(vbox);
    return vbox;
}

GtkWidget *gtk_hbox_new_in_box(gboolean homogeneous, guint spacing,
    guint border, GtkWidget * box, gboolean expand, gboolean fill,
    guint padding)
{
    GtkWidget *hbox;

    hbox = gtk_hbox_new(homogeneous, spacing);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), border);
    gtk_box_pack_start(GTK_BOX(box), hbox, expand, fill, padding);
    gtk_widget_show(hbox);
    return hbox;
}

GtkWidget *gtk_vbox_framed_new_in_box(const gchar * name, gboolean homogeneous,
    guint spacing, guint border, GtkWidget * box, gboolean expand,
    gboolean fill, guint padding)
{
    GtkWidget *vbox, *frame;

    frame = gtk_frame_new(name);
    gtk_box_pack_start(GTK_BOX(box), frame, expand, fill, padding);
    gtk_widget_show(frame);
    vbox = gtk_vbox_new(homogeneous, spacing);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), border);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
    gtk_widget_show(vbox);
    return vbox;
}

GtkWidget *gtk_hbox_framed_new_in_box(const gchar * name, gboolean homogeneous,
    guint spacing, guint border, GtkWidget * box, gboolean expand,
    gboolean fill, guint padding)
{
    GtkWidget *hbox, *frame;

    frame = gtk_frame_new(name);
    gtk_box_pack_start(GTK_BOX(box), frame, expand, fill, padding);
    gtk_widget_show(frame);
    hbox = gtk_hbox_new(homogeneous, spacing);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), border);
    gtk_container_add(GTK_CONTAINER(frame), hbox);
    gtk_widget_show(hbox);
    return hbox;
}

void gtk_label_set_text_if(GtkWidget * label, const gchar * text)
{
    if (label != NULL) {
	gtk_label_set_text(GTK_LABEL(label), text);
    }
}

void gtk_label_size_to_fit(GtkLabel * label, const gchar * str)
{
    GtkRequisition req;
    gchar *current_text;
    gint text_len;
    gchar *text_buf;

    /* get a pointer to the current text */
    gtk_label_get(label, &current_text);
    /* how long is it */
    text_len = strlen(current_text);
    /* allocate memory to save it */
    text_buf = malloc(text_len + 2);
    if (text_buf == NULL) {
	printf("gtk_label_size_to_fit() - malloc failed\n");
	return;
    }
    /* save the text */
    strncpy(text_buf, current_text, text_len + 1);
    /* set the label to display the new text */
    gtk_label_set_text(label, str);
    /* how big is the label with the new text? */
    gtk_widget_size_request(GTK_WIDGET(label), &req);
    /* freeze it at this size */
    gtk_widget_set_usize(GTK_WIDGET(label), req.width, req.height);
    /* restore the old text */
    gtk_label_set_text(label, text_buf);
    /* free the buffer */
    free(text_buf);
    /* wow, all that just to size a box! what a pain! */
    return;
}

int dialog_generic_msg(GtkWidget * parent, const gchar * title, const gchar * msg,
    const gchar * button1, const gchar * button2, const gchar * button3, const gchar * button4)
{
    dialog_generic_t dialog;
    GtkWidget *button, *label;
    const gchar *button_name_array[4];
    void (*button_funct_array[4]) (GtkWidget *, dialog_generic_t *);
    gint n;

    dialog.retval = 0;
    /* create dialog window, disable resizing */
    dialog.window = gtk_dialog_new();
    gtk_window_set_policy(GTK_WINDOW(dialog.window), FALSE, FALSE, FALSE);
    /* set title */
    if (title != NULL) {
	gtk_window_set_title(GTK_WINDOW(dialog.window), title);
    } else {
	gtk_window_set_title(GTK_WINDOW(dialog.window), "Dialog");
    }
    if (msg != NULL) {
	label = gtk_label_new(msg);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->vbox), label,
	    TRUE, TRUE, 0);
	gtk_misc_set_padding(GTK_MISC(label), 15, 15);
    }
    /* set up a callback function when the window is destroyed */
    gtk_signal_connect(GTK_OBJECT(dialog.window), "destroy",
	GTK_SIGNAL_FUNC(dialog_generic_destroyed), &dialog);
    /* transfer button name pointers to an array for looping */
    button_name_array[0] = button1;
    button_name_array[1] = button2;
    button_name_array[2] = button3;
    button_name_array[3] = button4;
    /* make a matching array of pointers to functions */
    button_funct_array[0] = dialog_generic_button1;
    button_funct_array[1] = dialog_generic_button2;
    button_funct_array[2] = dialog_generic_button3;
    button_funct_array[3] = dialog_generic_button4;
    /* loop to make buttons */
    for (n = 0; n < 4; n++) {
	if (button_name_array[n] != NULL) {
	    /* make a button */
	    button = gtk_button_new_with_label(button_name_array[n]);
	    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.window)->
		    action_area), button, TRUE, TRUE, 4);
	    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(button_funct_array[n]), &dialog);
	}
    }
    if (parent != NULL) {
	gtk_window_set_transient_for(GTK_WINDOW(dialog.window),
	    GTK_WINDOW(parent));
    }
    gtk_window_set_modal(GTK_WINDOW(dialog.window), TRUE);
    gtk_widget_show_all(dialog.window);
    gtk_main();
    return dialog.retval;
}

void dialog_generic_button1(GtkWidget * widget, dialog_generic_t * dptr)
{
    /* set return value */
    dptr->retval = 1;
    /* destroy window to cause dialog_generic_destroyed() to be called */
    gtk_widget_destroy(dptr->window);
}

void dialog_generic_button2(GtkWidget * widget, dialog_generic_t * dptr)
{
    dptr->retval = 2;
    gtk_widget_destroy(dptr->window);
}

void dialog_generic_button3(GtkWidget * widget, dialog_generic_t * dptr)
{
    dptr->retval = 3;
    gtk_widget_destroy(dptr->window);
}

void dialog_generic_button4(GtkWidget * widget, dialog_generic_t * dptr)
{
    dptr->retval = 4;
    gtk_widget_destroy(dptr->window);
}

void dialog_generic_destroyed(GtkWidget * widget, dialog_generic_t * dptr)
{
    /* this will drop out of the gtk_main call and allow dialog_generic() to
       return */
    gtk_main_quit();
}

/***********************************************************************
*                        LOCAL FUNCTION CODE                           *
************************************************************************/
