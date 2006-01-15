#ifndef VCP_H
#define VCP_H

/** This file, 'vcp.h', contains declarations and prototypes 
    use by halvcp, the HAL Virtual Control Panel builder.
*/

/** Copyright (C) 2005 John Kasunich
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
    License along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS PROGRAM ACCEPT ABSOLUTELY NO LIABILITY FOR
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

#include <gtk/gtk.h>

typedef enum {
    ATTRIB_STRING = 0,
    ATTRIB_BOOL,
    ATTRIB_INT,
    ATTRIB_FLOAT,
} attrib_type_t;

/* This struct defines a widget attribute.  An array of these
   is created and initialized in the code to tell the program
   what attributes a widget has.
*/

typedef struct {
    char *name;
    char *deflt;
    attrib_type_t datatype;
    int offset;
} vcp_attrib_def_t;

/* widget classes - these are used to indicate which widgets
   can and cannot be nested inside one another */

#define CL_TOP_LEVEL    0x0001    /* the entire file defines one of these */
#define CL_MAIN_WINDOW  0x0002    /* main window of the app, only one */
#define CL_CONTROL      0x0004    /* buttons and sliders, etc */
#define CL_DISPLAY      0x0008    /* LEDs and meters etc */
#define CL_LAYOUT       0x0010    /* layout items, invisible */
#define CL_LABEL        0x0020    /* static text and graphics */

#define CH_NONE         0x0000    /* no children allowed */
#define CH_ONE          0x8000    /* one child allowed */
#define CH_MANY         0xC000    /* multiple children allowed */

/* this struct describes a specific instance of a widget.
   A tree structure of these is constructed as the .vcp
   file is read on startup.
*/

typedef struct vcp_widget_s {
    struct vcp_widget_def_s *type;	/* ptr to generic info about widget */
    struct vcp_widget_s *parent;	/* null for mainwindow */
    struct vcp_widget_s *sibling;	/* other widgets inside parent */
    struct vcp_widget_s *child;		/* list of child widget(s) if any */
    int linenum;			/* line number in vcp file */
    GtkWidget *gtk_widget;		/* child gtk's nest into this */
    enum { NONE, BOX, BIN } gtk_type;	/* establishes how things next */
    void *priv_data;			/* attribs and widget specific data */
    void *hal_data;			/* HAL pins and/or parameters */
    void (*poll_funct)(struct vcp_widget_s *wp);  /* polling function */
} vcp_widget_t;

/* This struct defines a widget.  An array of these is 
   created and initialized in the code to tell the program
   what widgets it supports.
*/

typedef struct vcp_widget_def_s {
    char *name;
    long w_class;
    long w_class_mask;
    vcp_attrib_def_t *attribs;
    int priv_data_size;
    int (*init_funct)(vcp_widget_t *widget);
} vcp_widget_def_t;

/* GLOBALS */

#define ERRBUFLEN 200

extern char errbuf[];
extern int comp_id;
extern vcp_widget_def_t *widget_defs[];

#endif /* VCP_H */
