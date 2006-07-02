/** This file, 'vcp_widgets.c', implements most of the GUI widgets that
    halvcp uses to build a Virtual Control Panel.
*/

/** Copyright (C) 2005 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
    Public License as published by the Free Software Foundation.
    This program is distributed in the hope that it will be useful,
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

#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <hal.h>
#include "vcp.h"

/*************** Local Function Prototypes *********************/

static gboolean alloc_color_rgb(GdkColor * color, GdkColormap * map,
    unsigned char red, unsigned char green, unsigned char blue);
static gboolean alloc_color(GdkColor * color, GdkColormap * map);
static void add_to_parent ( vcp_widget_t *parent, GtkWidget *child,
			    int expand, int padding );

/*********** Widget Definitions (structs and code) *************/

/** General notes:

    Every widget must declare a vcp_widget_def_t structure, and
    initialize it with the appropriate values.  Fields are as
    follows:

    'name'             The name of the widget, as a string.
    'w_class'          The class of the widget.
    'w_class_mask'     A bitmask identifying legal child widgets.
    'attribs'          A pointer to an array of vcp_attrib_def_t structs
                         that identify the widget's attributes.  Can
                         be NULL if there are no attributes.
    'priv_data_size'   The number of bytes of RAM needed for the widget's
                         private data, including attribute values (but 
                         not HAL data).  Can be zero if there are no
                         no attributes and no other internal data.
    'init_funct'       Pointer to a function that initializes the 
                         widget.  Can be NULL, but just about any
                         usefull widget will require some init code.

    When the init_funct is called, it is passed a pointer to a new 
    vcp_widget structure.  The 'type' pointer in the structure points
    to the definition above, so the init function can get any data it
    needs from the definition, or indirectly from the attribute 
    definitions.  The 'parent', 'child', and 'sibling' pointers in 
    the widget structure will already be set, as will the linenum
    field.  The 'priv_data' pointer will point to a block of the 
    desired size.  The 'hal_data' field will be NULL, if the widget
    needs HAL data, the init function must allocate it.  The 
    'poll_funct' field will also be NULL, the init function should
    set it if needed.

    If specified, the 'poll_funct' will be called about ten times
    per second (this may wind up configurable).  It is passed a
    pointer to the widget structure.
*/

/* Widgets start here */

/** VCP WIDGET: This is the top level widget in the file.  It
    isn't a real widget, it simply provides a wrapper for
    everything else.  In theory the "main-window" widget
    could serve this role, but someday I might want to allow
    for pop-ups or other widgets that are siblings of the
    main window rather than its children.
*/

vcp_widget_def_t vcp_def = {
    "vcp",
    CL_TOP_LEVEL,
    CH_ONE | CL_MAIN_WINDOW,
    NULL,
    0,
    NULL
};


/** MAIN WINDOW WIDGET: The application must have one and only one
    of these widgets.  It is the main GUI window of the program.
*/

static int init_main_window(vcp_widget_t *widget);

typedef struct {
    char *title;
    int height;
    int width;
} main_win_data_t;

vcp_attrib_def_t main_win_attribs[] = {
    { "title", "VCP", ATTRIB_STRING, offsetof(main_win_data_t, title) },
    { "height", "-2", ATTRIB_INT, offsetof(main_win_data_t, height) },
    { "width", "-2", ATTRIB_INT, offsetof(main_win_data_t, width) },
    { NULL, NULL, 0, 0 }
};

vcp_widget_def_t main_window_def = {
    "main-window",
    CL_MAIN_WINDOW, 
    CH_ONE | CL_CONTROL | CL_LAYOUT | CL_DISPLAY,
    main_win_attribs, 
    sizeof(main_win_data_t),
    init_main_window
};


static void main_window_closed(GtkWidget * widget, gpointer * gdata)
{
    gtk_main_quit();
}

static int init_main_window ( vcp_widget_t *wp )
{
    static int have_one = 0;
    main_win_data_t *pd;
    GtkWidget *gwp;

    if ( have_one != 0 ) {
	printf ( "line %d: can't have two main windows\n", wp->linenum );
	return -1;
    }
    /* get pointer to private data */
    pd = (main_win_data_t *)(wp->priv_data);
    /* create main window, set it's size */
    gwp = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    wp->gtk_widget = gwp;
    wp->gtk_type = BIN;
    /* set the minimum size */
    gtk_widget_set_usize(GTK_WIDGET(gwp), pd->width, pd->height);
    /* allow the user to expand it */
    gtk_window_set_policy(GTK_WINDOW(gwp), FALSE, TRUE, FALSE);
    /* set main window title */
    gtk_window_set_title(GTK_WINDOW(gwp), pd->title);
    /* this makes the application exit when the window is closed */
    gtk_signal_connect(GTK_OBJECT(gwp), "destroy",
	GTK_SIGNAL_FUNC(main_window_closed), NULL);
    have_one = 1;
    return 0;
}


/** BOX: The box widget is used to place more than one child widget
    inside a parent widget that normally holds only one child.
    It can also draw an optional frame and title around the group
    of widgets it contains.
*/

static int init_box(vcp_widget_t *widget);

typedef struct {
    char *layout;
    int expand;
    int padding;
    int uniform;
    int space;
    int frame;
    char *title;
    int border;
} box_data_t;

vcp_attrib_def_t box_attribs[] = {
    { "layout", "V", ATTRIB_STRING, offsetof(box_data_t, layout) },
    { "expand", "0", ATTRIB_BOOL, offsetof(box_data_t, expand) },
    { "padding", "0", ATTRIB_INT, offsetof(box_data_t, padding) },
    { "uniform", "0", ATTRIB_BOOL, offsetof(box_data_t, uniform) },
    { "space", "0", ATTRIB_INT, offsetof(box_data_t, space) },
    { "frame", "0", ATTRIB_BOOL, offsetof(box_data_t, frame) },
    { "title", "", ATTRIB_STRING, offsetof(box_data_t, title) },
    { "border", "0", ATTRIB_INT, offsetof(box_data_t, border) },
    { NULL, NULL, 0, 0 }
};

vcp_widget_def_t box_def = {
    "box",
    CL_LAYOUT,
    CH_MANY | CL_CONTROL | CL_LAYOUT | CL_LABEL | CL_DISPLAY,
    box_attribs,
    sizeof(box_data_t),
    init_box
};

static int init_box ( vcp_widget_t *wp )
{
    box_data_t *pd;
    GtkWidget *gwp, *gwp2;
 
    /* get pointer to private data */
    pd = (box_data_t *)(wp->priv_data);
    /* we only look a the first character of the layout param */
    pd->layout[0] = toupper(pd->layout[0]);
    if (( pd->layout[0] != 'V' ) && ( pd->layout[0] != 'H' )) {
	printf ( "line %d:box layout must be either horizontal or vertical\n",
	    wp->linenum );
	return -1;
    }
    /* create the box */
    if ( pd->layout[0] == 'H' ) {
	gwp = gtk_hbox_new(pd->uniform, pd->space);
    } else {
	gwp = gtk_vbox_new(pd->uniform, pd->space);
    }
    wp->gtk_widget = gwp;
    wp->gtk_type = BOX;
    gtk_container_set_border_width(GTK_CONTAINER(gwp), pd->border);
    /* see if we need a frame */
    if (( pd->frame != 0 ) || ( pd->title[0] != '\0' )) {
	/* create a frame */
	if ( pd->title[0] == '\0' ) {
	    /* no title */
	    gwp2 = gtk_frame_new(NULL);
	} else {
	    gwp2 = gtk_frame_new(pd->title);
	}
	gtk_container_add(GTK_CONTAINER(gwp2), gwp);
	gtk_widget_show(gwp);
        gwp = gwp2;
    }
    /* add to the parent widget */
    add_to_parent(wp->parent, gwp, pd->expand, pd->padding);
    gtk_widget_show(gwp);
    return 0;
}


/** LABEL: The label widget provides a simple, static label.
*/

static int init_label(vcp_widget_t *widget);

typedef struct {
    int expand;
    int padding;
    char *text;
} label_data_t;

vcp_attrib_def_t label_attribs[] = {
    { "expand", "0", ATTRIB_BOOL, offsetof(label_data_t, expand) },
    { "padding", "0", ATTRIB_INT, offsetof(label_data_t, padding) },
    { "text", NULL, ATTRIB_STRING, offsetof(label_data_t, text) },
    { NULL, NULL, 0, 0 }
};

vcp_widget_def_t label_def = {
    "label",
    CL_LABEL,
    CH_NONE,
    label_attribs,
    sizeof(label_data_t),
    init_label
};

static int init_label ( vcp_widget_t *wp )
{
    label_data_t *pd;
    GtkWidget *gwp;
 
    pd = (label_data_t *)(wp->priv_data);
    /* create a label */
    gwp = gtk_label_new(pd->text);
    wp->gtk_widget = gwp;
    wp->gtk_type = NONE;
    /* add the label to the parent widget */
    add_to_parent(wp->parent, gwp, pd->expand, pd->padding);
    gtk_widget_show(gwp);
    return 0;
}


/** BUTTON: The button widget sets a bit HAL pin TRUE while it is 
    pressed, and FALSE when it is released.  The button is not 
    labeled, but it can accept any child widget (or widgets, if
    they are in a box).  Normally, a label widget would be
    specified as a child.
*/

static int init_button(vcp_widget_t *widget);

typedef struct {
    int expand;
    int padding;
    char *halpin;
    hal_bit_t pin_state;
} button_data_t;

typedef struct {
    hal_bit_t *pin;
} button_hal_t;

vcp_attrib_def_t button_attribs[] = {
    { "expand", "0", ATTRIB_BOOL, offsetof(button_data_t, expand) },
    { "padding", "0", ATTRIB_INT, offsetof(button_data_t, padding) },
    { "halpin", NULL, ATTRIB_STRING, offsetof(button_data_t, halpin) },
    { NULL, NULL, 0, 0 }
};

vcp_widget_def_t button_def = {
    "button",
    CL_CONTROL,
    CH_ONE | CL_LAYOUT | CL_LABEL,
    button_attribs,
    sizeof(button_data_t),
    init_button
};

static void button_pressed(GtkWidget * widget, gpointer gdata)
{
    vcp_widget_t *wp;
    button_data_t *dp;
    button_hal_t *hp;

    wp = (vcp_widget_t *)gdata;
    dp = (button_data_t *)wp->priv_data;
    hp = (button_hal_t *)wp->hal_data;
    dp->pin_state = 1;
    *(hp->pin) = dp->pin_state;
}

static void button_released(GtkWidget * widget, gpointer gdata)
{
    vcp_widget_t *wp;
    button_data_t *dp;
    button_hal_t *hp;

    wp = (vcp_widget_t *)gdata;
    dp = (button_data_t *)wp->priv_data;
    hp = (button_hal_t *)wp->hal_data;
    dp->pin_state = 0;
    *(hp->pin) = dp->pin_state;
}

static void button_refresh (vcp_widget_t *wp)
{
    button_data_t *dp;
    button_hal_t *hp;

    dp = (button_data_t *)wp->priv_data;
    hp = (button_hal_t *)wp->hal_data;
    *(hp->pin) = dp->pin_state;
}

static int init_button ( vcp_widget_t *wp )
{
    button_data_t *pd;
    button_hal_t *hd;
    int retval;
    GtkWidget *gwp;

    pd = (button_data_t *)(wp->priv_data);

    /* allocate HAL memory for pin */
    hd = hal_malloc(sizeof(button_hal_t));
    if (hd == NULL) {
	printf( "init_button(): unable to allocate HAL memory\n" );
	return -1;
    }
    wp->hal_data = hd;
    /* export pin */
    retval = hal_pin_bit_new(pd->halpin, HAL_WR, &(hd->pin), comp_id);
    if (retval != 0) {
	printf( "init_button(): unable to export HAL pin '%s'\n", pd->halpin );
	return retval;
    }
    /* does the button have a child? */
    if ( wp->child != NULL ) {
	/* create a plain button so the child can go inside */
	gwp = gtk_button_new();
    } else {
	/* create a button with a blank label inside */
	gwp = gtk_button_new_with_label(NULL);
    }
    wp->gtk_widget = gwp;
    wp->gtk_type = BIN;
    /* put it in its parent */
    add_to_parent(wp->parent, gwp, pd->expand, pd->padding);
    /* connect handler functions */
    gtk_signal_connect(GTK_OBJECT(gwp), "pressed",
	GTK_SIGNAL_FUNC(button_pressed), wp);
    gtk_signal_connect(GTK_OBJECT(gwp), "released",
	GTK_SIGNAL_FUNC(button_released), wp);
    /* use a poll function for periodic refresh, even if the user 
       doesn't press the button */
    wp->poll_funct = button_refresh;
    gtk_widget_show(gwp);
    return 0;
}

/** LED: The LED widget monitors a bit HAL pin and changes color
    to indicate the state of the pin.
*/

static int init_led(vcp_widget_t *widget);

typedef struct {
    int expand;
    int padding;
    int diameter;
    char *halpin;
    int last;
    GtkWidget *drawing;		/* drawing area for LED image */
    GdkColormap *map;		/* colormap for LED image */
    GdkColor color_border;	/* the circle around the LED */
    GdkColor color_off;		/* LED off color */
    GdkColor color_on;		/* LED on color */
    GdkDrawable *win;		/* the window */
    GdkGC *context;		/* graphics context for drawing LED */
} led_data_t;

typedef struct {
    hal_bit_t *pin;
} led_hal_t;

vcp_attrib_def_t led_attribs[] = {
    { "expand", "0", ATTRIB_BOOL, offsetof(led_data_t, expand) },
    { "padding", "0", ATTRIB_INT, offsetof(led_data_t, padding) },
    { "size", "20", ATTRIB_INT, offsetof(led_data_t, diameter) },
    { "on-color", "#FF0000", ATTRIB_COLOR, offsetof(led_data_t, color_on) },
    { "off-color", "#600000", ATTRIB_COLOR, offsetof(led_data_t, color_off) },
    { "halpin", NULL, ATTRIB_STRING, offsetof(led_data_t, halpin) },
    { NULL, NULL, 0, 0 }
};

vcp_widget_def_t led_def = {
    "LED",
    CL_DISPLAY,
    CH_NONE,
    led_attribs,
    sizeof(led_data_t),
    init_led
};

static int led_refresh (GtkWidget *widget, GdkEventExpose *event, vcp_widget_t *wp)
{
    led_data_t *dp;
    led_hal_t *hp;
    int width, height;
    int border_dia, led_dia;
    int xstart, ystart;

    dp = (led_data_t *)wp->priv_data;
    hp = (led_hal_t *)wp->hal_data;
    /* get window pointer */
    dp->win = dp->drawing->window;
    if (dp->win == NULL) {
	/* window isn't visible yet, do nothing */
	printf("led_refresh(): win = NULL, bailing!\n");
	return FALSE;
    }
    /* create drawing context if needed */
    if (dp->context == NULL) {
	dp->context = gdk_gc_new(dp->win);
    }
    border_dia = dp->diameter;
    /* get window dimensions */
    gdk_window_get_geometry(dp->win, NULL, NULL, &width, &height, NULL);
    if ( border_dia > (height-2) ) {
	border_dia = height-2;
    }
    if ( border_dia > (width-2) ) {
	border_dia = width-2;
    }
    led_dia = border_dia-2;
    if ( led_dia < 0 ) {
	led_dia = 0;
    }
    xstart = (width-border_dia)/2;
    ystart = (height-border_dia)/2;
    /* clear the display */
    gdk_window_clear(dp->win);
    gdk_gc_set_foreground(dp->context, &(dp->color_border));
    gdk_draw_arc ( dp->win, dp->context, 0, xstart, ystart, border_dia, border_dia, 0, 64*360 );
    if ( *(hp->pin) ) {
        gdk_gc_set_foreground(dp->context, &(dp->color_on));
    } else {
        gdk_gc_set_foreground(dp->context, &(dp->color_off));
    }
    gdk_draw_arc ( dp->win, dp->context, 1, xstart+1, ystart+1, led_dia, led_dia, 0, 64*360 );

    return FALSE;
}
  
static void led_refresh_check(vcp_widget_t *wp) {
    led_data_t *dp;
    led_hal_t *hp;

    dp = (led_data_t *)wp->priv_data;
    hp = (led_hal_t *)wp->hal_data;

    if( *(hp->pin) != dp->last ) {
        dp->last = *(hp->pin);
        gtk_widget_queue_draw(dp->drawing);
    }
}

static int init_led ( vcp_widget_t *wp )
{
    led_data_t *pd;
    led_hal_t *hd;
    int retval;
    GtkWidget *gwp;

    pd = (led_data_t *)(wp->priv_data);

    /* allocate HAL memory for pin */
    hd = hal_malloc(sizeof(led_hal_t));
    if (hd == NULL) {
	printf( "init_led(): unable to allocate HAL memory\n" );
	return -1;
    }
    wp->hal_data = hd;
    /* export pin */
    retval = hal_pin_bit_new(pd->halpin, HAL_RD, &(hd->pin), comp_id);
    if (retval != 0) {
	printf( "init_button(): unable to export HAL pin '%s'\n", pd->halpin );
	return retval;
    }
    pd = wp->priv_data;
    /* allocate a drawing area */
    gwp = gtk_drawing_area_new();
    /* set its size to fit the LED */
    gtk_drawing_area_size(GTK_DRAWING_AREA(gwp), pd->diameter+2, pd->diameter+2);
    pd->drawing = gwp;
    pd->last = -1;
    wp->gtk_widget = gwp;
    wp->gtk_type = NONE;
    /* hook up a function to handle expose events */
    gtk_signal_connect(GTK_OBJECT(gwp), "expose_event",
	GTK_SIGNAL_FUNC(led_refresh), wp);
    gtk_widget_set_double_buffered(GTK_WIDGET(gwp), TRUE);
    /* get color map */
    pd->map = gtk_widget_get_colormap(gwp);
    /* allocate colors */
    alloc_color_rgb(&(pd->color_border), pd->map, 0, 0, 0);
    alloc_color(&(pd->color_off), pd->map);
    alloc_color(&(pd->color_on), pd->map);
    /* put it in its parent */
    add_to_parent(wp->parent, gwp, pd->expand, pd->padding);
    /* use a poll function for periodic refresh */
    wp->poll_funct = led_refresh_check;
    gtk_widget_show(gwp);
    return 0;
}

/******************* list of widget definitions ****************/

vcp_widget_def_t *widget_defs[] = {
    &vcp_def,
    &main_window_def,
    &box_def,
    &label_def,
    &button_def,
    &led_def,
    NULL
};
    
/*********************** Local Function Code ********************/

static gboolean alloc_color_rgb(GdkColor * color, GdkColormap * map,
    unsigned char red, unsigned char green, unsigned char blue)
{
    int retval;

    color->red = ((unsigned long) red) << 8;
    color->green = ((unsigned long) green) << 8;
    color->blue = ((unsigned long) blue) << 8;
    color->pixel =
	((unsigned long) red) << 16 | ((unsigned long) green) << 8 |
	((unsigned long) blue);
    retval = gdk_colormap_alloc_color(map, color, FALSE, TRUE);
    if (retval == 0) {
	printf("alloc_color_rgb( %d, %d, %d ) failed\n", red, green, blue);
    }
    return retval;
}

static gboolean alloc_color(GdkColor * color, GdkColormap * map)
{
    int retval;

    retval = gdk_colormap_alloc_color(map, color, FALSE, TRUE);
    if (retval == 0) {
	printf("alloc_color( %d, %d, %d ) failed\n", color->red, color->green, color->blue);
    }
    return retval;
}

static void add_to_parent ( vcp_widget_t *parent, GtkWidget *child,
			    int expand, int padding )
{
    if ( parent->gtk_type == BOX ) {
	gtk_box_pack_start(GTK_BOX(parent->gtk_widget),
	    child, expand, TRUE, padding);
    } else if ( parent->gtk_type == BIN ) {
	gtk_container_add(GTK_CONTAINER(parent->gtk_widget), child);
    } else {
	printf ( "can't pack things into '%s'\n", parent->type->name );
    }
}
