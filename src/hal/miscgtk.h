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

/***********************************************************************
*                      FUNCTIONS PROVIDED BY MISCGTK.C                 *
************************************************************************/

/** gtk_label_new_in_box() is used to create a label and pack it into
    a box.  It simply calls other GTK functions that do the real work.
    Normally it would take 4-5 lines of code to do the same thing.
*/
GtkWidget *gtk_label_new_in_box(gchar * text, GtkJustification just,
    GtkWidget * box, gboolean expand, gboolean fill, guint padding);

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

/** yet another convenience function - this one works exactly like
    'gtk_label_set_text() except that if 'label' is null it returns
    without doing anything - handy for refreshing displays that may
    or may not be valid, for instance if they are part of a dialog
    that might not be active.
*/
void gtk_label_set_text_if(GtkWidget * label, gchar * text);

/** gtk_label_size_to_fit() sets the size of the label to fit the
    the string "str".  It is usefull when you want the size of a
    label to remain constant, even if it's contents change.
*/
void gtk_label_size_to_fit(GtkLabel * label, gchar * str);

/* generic dialog typedef */
typedef struct {
    GtkWidget *window;
    int retval;
} dialog_generic_t;

/** dialog_generic_msg() generates a modal dialog box with a message
    and up two four buttons.  It returns an integer indicating which
    button the user pressed, or zero if the dialog was closed.
    'parent' is the parent window - may be NULL if there is no parent.
    'title' is the title for the dialog - if NULL, "Dialog" will be
    used.
    'msg' is the message text, if NULL, no message will be displayed.
    'button1' thru 'button4' are the text for the buttons.  Only
    buttons that are not NULL will be displayed - for a two button
    dialog, supply two valid pointers and two NULLs.
*/
int dialog_generic_msg(GtkWidget * parent, gchar * title, gchar * msg,
    gchar * button1, gchar * button2, gchar * button3, gchar * button4);

/** the following functions are used by the generic dialog functions,
    but may also be usefull for custom dialogs, so they are made public
    here.
*/
void dialog_generic_button1(GtkWidget * widget, dialog_generic_t * dptr);
void dialog_generic_button2(GtkWidget * widget, dialog_generic_t * dptr);
void dialog_generic_button3(GtkWidget * widget, dialog_generic_t * dptr);
void dialog_generic_button4(GtkWidget * widget, dialog_generic_t * dptr);
void dialog_generic_destroyed(GtkWidget * widget, dialog_generic_t * dptr);

#endif /* MISCGTK_H */
