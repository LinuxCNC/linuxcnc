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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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

    bar = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(box), bar, FALSE, FALSE, padding);
    gtk_widget_show(bar);
}

void gtk_hseparator_new_in_box(GtkWidget * box, guint padding)
{
    GtkWidget *bar;

    bar = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(box), bar, FALSE, FALSE, padding);
    gtk_widget_show(bar);
}

GtkWidget *gtk_vbox_new_in_box(gboolean homogeneous, guint spacing,
    guint border, GtkWidget * box, gboolean expand, gboolean fill,
    guint padding)
{
    GtkWidget *vbox;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
    gtk_box_set_homogeneous(GTK_BOX(vbox), homogeneous);
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

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
    gtk_box_set_homogeneous(GTK_BOX(hbox), homogeneous);
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
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
    gtk_box_set_homogeneous(GTK_BOX(vbox), homogeneous);
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
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
    gtk_box_set_homogeneous(GTK_BOX(hbox), homogeneous);
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
    const gchar *current_text;
    gint text_len;
    gchar *text_buf;

    /* get a pointer to the current text */
    current_text = gtk_label_get_text(label);
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
    gtk_widget_get_preferred_size(GTK_WIDGET(label), NULL, &req);
    /* freeze it at this size */
    gtk_widget_set_size_request(GTK_WIDGET(label), req.width, req.height);
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
    GtkWidget *button, *label, *content_area;
    const gchar *button_name_array[4];
    void (*button_funct_array[4]) (GtkWidget *, dialog_generic_t *);
    gint n;

    dialog.retval = 0;
    /* create dialog window, disable resizing */
    dialog.window = gtk_dialog_new();
    gtk_window_set_resizable(GTK_WINDOW(dialog.window), FALSE);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog.window));
    /* set title */
    if (title != NULL) {
	gtk_window_set_title(GTK_WINDOW(dialog.window), title);
    } else {
	gtk_window_set_title(GTK_WINDOW(dialog.window), "Dialog");
    }
    if (msg != NULL) {
	label = gtk_label_new(msg);
	gtk_widget_set_margin_top(label, 15);
	gtk_widget_set_margin_bottom(label, 15);
	gtk_widget_set_margin_start(label, 15);
	gtk_widget_set_margin_end(label, 15);
    gtk_box_pack_start(GTK_BOX(GTK_CONTAINER(content_area)),
            label, TRUE, TRUE, 0);
    }
    /* set up a callback function when the window is destroyed */
    g_signal_connect(dialog.window, "destroy",
	G_CALLBACK(dialog_generic_destroyed), &dialog);
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
            g_signal_connect(button, "clicked",
                    G_CALLBACK(button_funct_array[n]), &dialog);
            gtk_dialog_add_action_widget(GTK_DIALOG(dialog.window),
                    button, n + 1);
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

void add_to_list(GtkWidget *list, char *strs[], const int num_cols)
{
    GtkListStore *store;
    GtkTreeIter iter;

    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));

    /* Hardcoded to only support one and two columns. */
    gtk_list_store_append(store, &iter);
    if (num_cols == 1) {
        gtk_list_store_set(store, &iter, 0, strs[0], -1);
    } else if (num_cols == 2) {
        gtk_list_store_set(store, &iter, 0, strs[0], 1, strs[1], -1);
    } else {
        printf("Failed to add item, to TreeView list\n");
    }
}

void init_list(GtkWidget *list, char *titles[], const int len)
{
    GtkCellRenderer *renderer;
    GtkListStore *store;

    int i;

    for (i = 0; i < len; i++) {
        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(list),
                -1, titles[i], renderer, "text", i, NULL);
    }

    store = gtk_list_store_new(len, G_TYPE_STRING, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));

    g_object_unref(store);
}

void clear_list(GtkWidget *list)
{
    GtkListStore *store;
    GtkTreeModel *model;
    GtkTreeIter iter;

    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));

    if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) {
        return;
    }

    gtk_list_store_clear(store);
}

void mark_selected_row(GtkWidget *list, const int row)
{
    GtkTreePath *path;
    GtkTreeSelection *selection;

    path = gtk_tree_path_new_from_indices(row, -1);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    gtk_tree_selection_select_path(selection, path);

    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(list), path, NULL, TRUE, 0.5, 0.5);
}

void set_file_filter(GtkFileChooser *chooser, const char *str, const char *ext)
{
    GtkFileFilter *filter_all;
    GtkFileFilter *filter_spes;

    filter_all = gtk_file_filter_new();
    filter_spes = gtk_file_filter_new();

    gtk_file_filter_set_name(filter_all, "All files");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter_all);

    gtk_file_filter_set_name(filter_spes, str);
    gtk_file_filter_add_pattern(filter_spes, ext);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter_spes);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser), filter_spes);
}

/***********************************************************************
*                        LOCAL FUNCTION CODE                           *
************************************************************************/
