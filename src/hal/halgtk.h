#ifndef HALGTK_H
#define HALGTK_H

/** This file, 'halgtk.h', contains declarations for various code
    used by GTK based hal programs such as 'halmeter' and 'halscope'.
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
*                            TYPEDEFS                                  *
************************************************************************/

/** a 'probe' is an object that references a HAL pin, signal, or
    parameter.  The user may select the item that is to be probed.
*/

#define PROBE_NAME_LEN 63

typedef struct {
    int listnum;		/* 0 = pin, 1 = signal, 2 = parameter */
    char *pickname;		/* name from list, not validated */
    char *name;			/* name of pin/sig/param */
    hal_type_t type;		/* type of pin/sig/param */
    void *data;			/* address of data */
    GtkWidget *window;		/* selection dialog window */
    GtkWidget *lists[3];	/* lists for pins, sigs, and params */
    char probe_name[PROBE_NAME_LEN + 1];	/* name of this probe */
} probe_t;

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

/** 'probe_new()' creates a new probe structure.  It also creates
    a dialog window for the probe that allows the user to pick the
    pin, signal, or parameter that the probe will attach to.  It
    should be called during the init phase of the program, before
    the main event loop is started.
*/

probe_t *probe_new(char *probe_name);

/** 'popup_probe_window()' is an event handler function that opens
    the selection dialog for a probe.  'data' must be a pointer to
    a probe_t structure that was allocated by 'probe_new'.
*/

void popup_probe_window(GtkWidget * widget, gpointer data);

#endif /* HALGTK_H */
