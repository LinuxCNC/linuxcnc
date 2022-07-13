#ifndef MISCGTK_H
#define MISCGTK_H

/** This file, 'miscgtk.h', contains declarations for generic
    code used by GTK based programs.  This includes new widgets
    and other items of a general nature.
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
    information, go to https://linuxcnc.org.
*/

/***********************************************************************
*                      FUNCTIONS PROVIDED BY MISCGTK.C                 *
************************************************************************/

/** gtk_label_new_in_box() is used to create a label and pack it into
    a box.  It simply calls other GTK functions that do the real work.
    Normally it would take 4-5 lines of code to do the same thing.
*/
GtkWidget *gtk_label_new_in_box(const gchar * text, GtkWidget * box,
    gboolean expand, gboolean fill, guint padding);

/** more convenience functions - vertical and horizontal separators
    These functions set expand and fill to FALSE - if you don't like
    that, do it yourself.
*/
void gtk_vseparator_new_in_box(GtkWidget * box, guint padding);
void gtk_hseparator_new_in_box(GtkWidget * box, guint padding);

/** convenience functions for nesting boxes.  homogeneous and spacing
    apply to the new box, expand, fill, and padding apply to the box
    it is going into
*/
GtkWidget *gtk_vbox_new_in_box(gboolean homogeneous, guint spacing,
    guint border, GtkWidget * box, gboolean expand, gboolean fill,
    guint padding);
GtkWidget *gtk_hbox_new_in_box(gboolean homogeneous, guint spacing,
    guint border, GtkWidget * box, gboolean expand, gboolean fill,
    guint padding);

/** convenience functions for nesting boxes.  the new box is placed
    in a frame, which is in turn placed in the parent box.  'name'
    is the name displayed by the frame
*/
GtkWidget *gtk_vbox_framed_new_in_box(const gchar * name, gboolean homogeneous,
    guint spacing, guint border, GtkWidget * box, gboolean expand,
    gboolean fill, guint padding);
GtkWidget *gtk_hbox_framed_new_in_box(const gchar * name, gboolean homogeneous,
    guint spacing, guint border, GtkWidget * box, gboolean expand,
    gboolean fill, guint padding);

/** yet another convenience function - this one works exactly like
    'gtk_label_set_text() except that if 'label' is null it returns
    without doing anything - handy for refreshing displays that may
    or may not be valid, for instance if they are part of a dialog
    that might not be active.
*/
void gtk_label_set_text_if(GtkWidget * label, const gchar * text);

/** gtk_label_size_to_fit() sets the size of the label to fit the
    the string "str".  It is useful when you want the size of a
    label to remain constant, even if it's contents change.
*/
void gtk_label_size_to_fit(GtkLabel * label, const gchar * str);

/* Initialize a TreeView. */
void init_list(GtkWidget *list, char *titles[], const int num_cols);

/* Add new row to a TreeView. */
void add_to_list(GtkWidget *list, char *strs[], const int num);

/* Clears all elements from a TreeView list. */
void clear_list(GtkWidget *list);

/** If a row has  previously been selected, this function ensures that it
    will be selected again.
*/
void mark_selected_row(GtkWidget *list, const int row);

/* Set a file filter for the open and save dialog boxes. */
void set_file_filter(GtkFileChooser *chooser, const char *str, const char *ext);

#endif /* MISCGTK_H */
