/** This file, 'vcp_main.c', is a GUI program allows a user to
    generate a "Virtual Control Panel" with on-screen GUI widgets
    that can control or display the state of HAL pins. It is a
    user space component and uses GTK 1.2 or 2.0 for the GUI code.
*/

/** Copyright (C) 2005 John Kasunich
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
    License along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of 
    harming persons must have provisions for completely removing 
    power from all motors, etc, before persons enter any danger area.
    All machinery must be designed to comply with local and national 
    safety codes, and the authors of this software can not, and do 
    not, take any responsibility for such compliance.
    
    In particular, controls on a Virtual Control Panel MUST NOT be 
    relied on for safety.  Real controls that physically interrupt 
    the flow of power to the machine must be used to render the 
    machine safe.

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
#include "tokenizer.h"
#include "vcp.h"


/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

char errbuf[ERRBUFLEN];
int comp_id;		/* HAL component ID */

/***********************************************************************
*                         LOCAL VARIABLES                              *
************************************************************************/

static vcp_widget_t *root;	/* root of widget tree */

/***********************************************************************
*                  LOCAL FUNCTION PROTOTYPES                           *
************************************************************************/

/* init functions */
static int init_widget ( vcp_widget_t *wp );

/* callback functions */
static void exit_from_hal(void);
static void free_tree(void);
static void quit(int sig);
static int heartbeat(gpointer data);
static void run_poll_functs(vcp_widget_t *wp);

/* parser functions */
static vcp_widget_t *get_vcp_tree(char *filename);
static vcp_widget_t *get_child_widget(vcp_widget_t *parent, char *name, token_file_t *tf);
static void free_widget(vcp_widget_t *widget);
static int get_attrib(vcp_widget_t *widget, char *name, token_file_t *tf);
static int convert_attrib(vcp_attrib_def_t *adef, char *value, void *base_addr);

/***********************************************************************
*                        MAIN() FUNCTION                               *
************************************************************************/

int main(int argc, gchar * argv[])
{
    vcp_widget_t *wp, *main_win;

    /* process and remove any GTK specific command line args */
    gtk_init(&argc, &argv);
    
    printf ( "NOTICE:  VCP is still under construction\n" );
    printf ( "It may be broken at any time, and should not be used yet!\n" );
    sleep(2);

    /* process halvcp command line args (if any) here */
    if ( argc != 2 ) {
	printf ( "usage: halvcp <file>\n" );
	return -1;
    }
    /* open and parse the .vcp file */
    root = get_vcp_tree(argv[1]);
    if ( root == NULL ) {
	rtapi_print_msg(RTAPI_MSG_ERR, "VCP: ERROR: error in input file\n");
	return -1;
    }
    /* register an exit function to free the tree */
    g_atexit(free_tree);
    printf ( "Got a good tree\n" );
    /* connect to the HAL */
    comp_id = hal_init("halvcp");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "VCP: ERROR: hal_init() failed\n");
	return -1;
    }
    /* register an exit function to cleanup and disconnect from the HAL */
    g_atexit(exit_from_hal);
    printf ( "connected to HAL\n" );

    /* look for main window in widget tree */
    if ( root->child == NULL ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VCP: ERROR: nothing inside vcp{} block\n");
	return -1;
    }
    wp = root->child;
    main_win = NULL;
    while ( wp != NULL ) {
	if ( strcmp(wp->type->name, "main-window") == 0 ) {
	    main_win = wp;
	} else {
	    printf ( "unexpected top level widget '%s'\n", wp->type->name );
	}
	wp = wp->sibling;
    }
    if ( main_win == NULL ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "VCP: ERROR: 'main-window' missing\n");
	return -1;
    }
    /* init all widgets inside (and including) main window */
    if ( init_widget(main_win) != 0 ) {
	return -1;
    }    
    /* register signal handlers for ctrl-C and SIGTERM */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    /* The interface is now completely set up */
    /* show the window */
    gtk_widget_show(main_win->gtk_widget);
    /* arrange for periodic call of heartbeat() (every 100mS) */
    gtk_timeout_add(100, heartbeat, NULL);
    /* enter the main loop */
    gtk_main();
    return (0);
}

/***********************************************************************
*                      LOCAL INIT FUNCTION CODE                        *
************************************************************************/

static int init_widget ( vcp_widget_t *wp )
{
    if ( wp->type->init_funct != NULL ) {
	/* call the init function */
	if ( wp->type->init_funct(wp) != 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"VCP: ERROR: error initing widget '%s'\n",
		wp->type->name);
	    return -1;
	}
    }
    /* init children and siblings */
    if ( wp->child != NULL ) {
	if ( init_widget(wp->child) != 0 ) {
	    return -1;
	}
    }
    if ( wp->sibling != NULL ) {
	if ( init_widget(wp->sibling) != 0 ) {
	    return -1;
	}
    }
    return 0;
}

/***********************************************************************
*                    LOCAL CALLBACK FUNCTION CODE                      *
************************************************************************/

static void exit_from_hal(void)
{
    hal_exit(comp_id);
    printf ( "exited from HAL\n" );
}

static void free_tree(void)
{
    free_widget(root);
    printf ( "freed widget tree\n" );
}

static void quit(int sig)
{
    gtk_main_quit();
}

/* This function is called approximately 10 times per second.
   It is responsible for updating anything that is supposed to be
   "live", such as the display widgets.
*/

static int heartbeat(gpointer data)
{
    run_poll_functs(root);
    return 0;
}

static void run_poll_functs(vcp_widget_t *wp)
{
    /* run poll function of this widget (if any) */
    if ( wp->poll_funct != NULL ) {
	/* call the poll function */
	wp->poll_funct(wp);
    }
    /* run poll functions of children and siblings */
    if ( wp->child != NULL ) {
	run_poll_functs(wp->child);
    }
    if ( wp->sibling != NULL ) {
	run_poll_functs(wp->sibling);
    }
}

/***********************************************************************
*                     LOCAL PARSER FUNCTION CODE                       *
************************************************************************/


static vcp_widget_t *get_vcp_tree(char *filename)
{
    token_file_t *tf;
    vcp_widget_t *wp;
    char *name, *tmp;

    tf = tf_open(filename, 100, "{}=", '\\', '#', '"' );
    if ( tf == NULL ) {
	return NULL;
    }
    name = tf_get_token(tf);
    tmp = tf_get_token(tf);
    if ( name == NULL ) {
	printf ( "nothing in file '%s'\n", filename );
	tf_close(tf);
	return NULL;
    }
    if (( strcmp(name, "vcp") != 0 ) || (tmp == NULL ) || ( tmp[0] != '{' )) {
	printf ( "VCP file '%s' must start with 'vcp {'\n", filename );
	tf_close(tf);
	return NULL;
    }	
    wp = get_child_widget(NULL, name, tf );
    if ( wp == NULL ) {
        printf ( "error in VCP file '%s'\n", filename );
	tf_close(tf);
	return NULL;
    }
    tf_close(tf);
    return wp;
}


static vcp_widget_t *get_child_widget(vcp_widget_t *parent, char *name, token_file_t *tf)
{
    vcp_widget_def_t *wdef;
    vcp_attrib_def_t *adef;
    vcp_widget_t *widget, *wp;
    char *item_name, *delim, *cp;
    int n;

printf ( "get_child('%s')\n", name );
    n = 0;
    wdef = widget_defs[n];
    while ( wdef != NULL ) {
	if ( strcmp(wdef->name, name) == 0 ) {
	    break;
	}
	wdef = widget_defs[n++];
    }
    if ( wdef == NULL ) {
	printf ( "line %d: invalid widget type '%s'\n",
	    tf->tokenline, name );
	return NULL;
    }
printf ( "  matched\n" );
    /* found a match for name, check if it is a legal child of parent */
    if ( parent != NULL ) {
	if ( ( wdef->w_class & parent->type->w_class_mask ) == 0 ) {
	    printf ( "line %d: '%s' not permitted as child of '%s'\n",
		tf->tokenline, wdef->name, parent->type->name );
	    return NULL;
	}
    }
printf ( "  legal\n" );
    /* allocate the widget structure */
    widget = malloc(sizeof(vcp_widget_t));
    if ( widget == NULL ) {
	printf( "get_child_widget(): unable to allocate memory\n" );
	return NULL;
    }
printf ( "  allocated\n" );
    /* zero the entire widget structure */
    cp = (char *)widget;
    for ( n = 0 ; n < sizeof(vcp_widget_t) ; n++ ) {
	*cp++ = 0;
    }
    /* initialize non-zero widget structures fields */
    widget->type = wdef;
    widget->parent = parent;
    widget->linenum = tf->tokenline;
printf ( "  inited\n" );
    /* link the widget to its parent */
    if ( parent != NULL ) {
	/* check if parent allows children */
	if ( ( parent->type->w_class_mask & CH_ONE ) == 0 ) {
	    printf ( "line %d: '%s' may not have child widgets\n",
		tf->tokenline, parent->type->name );
	    free_widget(widget);
	    return NULL;
	}
	/* check if it is a legal child of parent */
	if ( ( wdef->w_class & parent->type->w_class_mask ) == 0 ) {
	    printf ( "line %d: '%s' not permitted as child of '%s'\n",
		tf->tokenline, wdef->name, parent->type->name );
	    free_widget(widget);
	    return NULL;
	}
	if ( parent->child == NULL ) {
	    /* no existing child, link it */
	    parent->child = widget;
	} else {
	    /* parent has one child, does it support more than one? */
	    if ( ( parent->type->w_class_mask & CH_MANY ) == 0 ) {
		printf ( "line %d: '%s' can only have one child\n",
		    tf->tokenline, parent->type->name );
		free_widget(widget);
		return NULL;
	    }
	    /* link it as sibling of existing child(ren) */
	    wp = parent->child;
	    while ( wp->sibling != NULL ) {
		wp = wp->sibling;
	    }
	    wp->sibling = widget;
	}
    }
printf ( "  linked\n" );
    /* allocate the widget's private data */
    widget->priv_data = malloc(wdef->priv_data_size);
    if ( widget->priv_data == NULL ) {
	printf( "get_child_widget(): unable to allocate memory\n" );
	free_widget(widget);
	return NULL;
    }
printf ( "  pd allocated\n" );
    /* clear private data */
    cp = widget->priv_data;
    for ( n = 0 ; n < wdef->priv_data_size ; n++ ) {
	*cp++ = 0;
    }
printf ( "  pd cleared\n" );
    /* set default values for all attributes */
    adef = wdef->attribs;
    if ( adef != NULL ) {
	while ( adef->name != NULL ) {
	    if ( convert_attrib(adef, adef->deflt, widget->priv_data) != 0 ) {
		printf ( "(internal error) widget %s: %s\n", wdef->name, errbuf );
		free_widget(widget);
		return NULL;
	    }
	    adef++;
	}
    }
printf ( "  defaulted\n" );
    /* parse remainder of widget */
    while (1) {
	item_name = tf_get_token(tf);
	if ( item_name == NULL ) {
	    printf ( "line %d: unexpected EOF in widget '%s'\n",
		tf->tokenline, wdef->name );
	    free_widget(widget);
	    return NULL;
	} else if ( item_name[0] == '}' ) {
	    /* end of widget, check for any string attribs without values */
	    adef = widget->type->attribs;
	    if ( adef != NULL ) {
		while ( adef->name != NULL ) {
		    if ( adef->datatype == ATTRIB_STRING ) {
			/* point to attribute value */
			cp = *((char **)(widget->priv_data + adef->offset));
			if ( cp == NULL ) {
			    printf ( "line %d: attribute '%s' must be supplied for widget '%s'\n",
				tf->tokenline, adef->name, wdef->name );
			    free_widget(widget);
			    return NULL;
			}
		    }
		    adef++;
		}
	    }
	    return widget;
	} else if ( !isalnum(item_name[0]) ) {
	    printf ( "line %d: bad widget or attribute name: '%s'\n",
		tf->tokenline, item_name );
	    free_widget(widget);
	    return NULL;
	}
	delim = tf_get_token(tf);
	if ( delim == NULL ) {
	    printf ( "line %d: unexpected EOF in widget '%s'\n",
		tf->tokenline, wdef->name );
	    free_widget(widget);
	    return NULL;
	} else if ( delim[0] == '=' ) {
	    /* name is a attribute */
	    if ( get_attrib(widget, item_name, tf) != 0 ) {
		/* message already printed inside get_attrib() */
		free_widget(widget);
		return NULL;
	    }
	} else if ( delim[0] == '{' ) {
	    /* recurse deeper */
	    if ( get_child_widget(widget, item_name, tf) == NULL ) {
		/* message already printed inside get_child_widget() */
		free_widget(widget);
		return NULL;
	    }
	} else {
	    printf ( "line %d: expected '=' or '{' after '%s'\n",
			tf->tokenline, item_name );
	    free_widget(widget);
	    return NULL;
	}
    }
}


static int get_attrib(vcp_widget_t *widget, char *name, token_file_t *tf)
{
    vcp_widget_def_t *wdef;
    vcp_attrib_def_t *adef;
    char *value;

    wdef = widget->type;
    adef = wdef->attribs;
    if ( adef == NULL ) {
	printf ( "line %d: widget '%s' accepts no attributes\n",
	    tf->tokenline, wdef->name );
	return -1;
    }
    while (( adef->name != NULL ) && ( strcmp(adef->name, name) ) != 0 ) {
	adef++;
    }
    if ( adef->name == NULL ) {
	printf ( "line %d: attribute '%s' is invalid for widget '%s'\n",
	    tf->tokenline, name, wdef->name );
	return -1;
    }
    /* found a match for name */
    value = tf_get_token(tf);
    if ( value == NULL ) {
	printf ( "line %d: missing value for attribute '%s'\n",
	    tf->tokenline, name );
	return -1;
    }
    if ( convert_attrib(adef, value, widget->priv_data) != 0 ) {
	printf ( "line %d: %s\n", tf->tokenline, errbuf );
	return -1;
    }
    return 0;
}

static int convert_attrib(vcp_attrib_def_t *adef, char *value, void *base_addr)
{
    char *cp;
    double float_value;
    int int_value, bool_value;

    switch ( adef->datatype ) {
    case ATTRIB_FLOAT:
	float_value = strtod(value, &cp);
	if ( *cp != '\0' ) {
	    snprintf ( errbuf, ERRBUFLEN,
		"bad value for float attribute '%s': '%s'\n", adef->name, value );
	    return -1;
	}
	/* is this the default value */
	if ( value != adef->deflt ) {
	    /* no, token from an input file, free it */
	    free(value);
	}
	/* store value */
	*((double *)(base_addr + adef->offset)) = float_value;
	break;
    case ATTRIB_INT:
	int_value = strtol(value, &cp, 0);
	if ( *cp != '\0' ) {
	    snprintf ( errbuf, ERRBUFLEN,
		"bad value for integer attribute '%s': '%s'\n", adef->name, value );
	    return -1;
	}
	/* is this the default value */
	if ( value != adef->deflt ) {
	    /* no, token from an input file, free it */
	    free(value);
	}
	/* store value */
	*((int *)(base_addr + adef->offset)) = int_value;
	break;
    case ATTRIB_BOOL:
	if ( strcasecmp(value, "true" ) == 0 ) {
	    bool_value = 1;
	} else if ( strcasecmp(value, "t" ) == 0 ) {
	    bool_value = 1;
	} else if ( strcasecmp(value, "1" ) == 0 ) {
	    bool_value = 1;
	} else if ( strcasecmp(value, "false" ) == 0 ) {
	    bool_value = 0;
	} else if ( strcasecmp(value, "f" ) == 0 ) {
	    bool_value = 0;
	} else if ( strcasecmp(value, "0" ) == 0 ) {
	    bool_value = 0;
	} else {
	    snprintf ( errbuf, ERRBUFLEN,
		"bad value for boolean attribute '%s': '%s'\n", adef->name, value );
	    return -1;
	}
	/* is this the default value */
	if ( value != adef->deflt ) {
	    /* no, token from an input file, free it */
	    free(value);
	}
	/* store value */
	*((int *)(base_addr + adef->offset)) = bool_value;
	break;
    case ATTRIB_STRING:
	/* get previous value, if any */
	cp = *((char **)(base_addr + adef->offset));
	/* was there a non-default previous value? */
	if (( cp != NULL ) && ( cp != adef->deflt )) {
	    /* yes, it was a token from an input file, free it */
	    free(cp);
	}
	/* store value */
	*((char **)(base_addr + adef->offset)) = value;
	break;
    default:
	snprintf ( errbuf, ERRBUFLEN,
	    "(internal error) bad type for attribute '%s'\n", adef->name );
	return -1;
	break;
    }
    return 0;
}

static void free_widget(vcp_widget_t *widget)
{
    vcp_attrib_def_t *adef;
    char *cp;
    vcp_widget_t *wp;
    
    if ( widget == NULL ) {
	return;
    }
    /* first free any children */
    if ( widget->child != NULL ) {
	free_widget(widget->child);
    }
    /* then any younger siblings */
    if ( widget->sibling != NULL ) {
	free_widget(widget->sibling);
    }
    /* free any string attribute values */
    adef = widget->type->attribs;
    if (( adef != NULL ) && ( widget->priv_data != NULL )) {
	while ( adef->name != NULL ) {
	    if ( adef->datatype == ATTRIB_STRING ) {
		/* point to attribute value */
		cp = *((char **)(widget->priv_data + adef->offset));
		/* was there a non-default previous value? */
		if (( cp != NULL ) && ( cp != adef->deflt )) {
		    /* yes, it was a token from an input file, free it */
		    free(cp);
		}
	    }
	    adef++;
	}
    }
    /* free widget's private data */
    if ( widget->priv_data != NULL ) {
	free(widget->priv_data);
    }
    /* unlink widget from parent or older sibling */
    if (( widget->parent != NULL ) && ( widget->parent->child != NULL )) {
	if ( widget->parent->child == widget ) {
	    widget->parent->child = NULL;
	} else {
	    wp = widget->parent->child;
	    while ( wp->sibling != NULL ) {
		if ( wp->sibling == widget ) {
		    wp->sibling = NULL;
		} else {
		    wp = wp->sibling;
		}
	    }
	}
    }
    /* and finally free the widget structure itself */
    free(widget);
}
