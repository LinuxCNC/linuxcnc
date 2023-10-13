/********************************************************************
* Description: rcs_exit.cc
*   This module provides a portable way to make sure multiple
*   functions are called before exiting.
*   These functions should be written to take an int  and return void.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

/* Forward Function Prototypes */
#include "rcs_exit.hh"

#include "linklist.hh"		// LinkedList
#include "rcs_print.hh"		// rcs_print_error()
#include "timer.hh"		// esleep()

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>		// exit()
#include <signal.h>		// signal() , SIGINT

#ifdef __cplusplus
}
#endif

static LinkedList *exit_list = (LinkedList *) NULL;

struct RCS_EXIT_LIST_ENTRY {
    long process_id;
    void (*fptr) (int);
};

// NOTE --
// the GNU VxWorks C++ cross-compiler (g++68k) has a bug, that
// prevents me from passing a pointer to a function as the first
// argument of a function.

int attach_rcs_exit_list(void (*fptr) (int))
{
    RCS_EXIT_LIST_ENTRY entry;
    if (NULL == exit_list) {
	exit_list = new LinkedList;
    }
    if (NULL == exit_list) {
	rcs_print_error("attach_rcs_exit_list:: Out of Memory.\n");
	return -1;
    }
    entry.process_id = 0;
    entry.fptr = fptr;
    return exit_list->store_at_tail(&entry, sizeof(entry), 1);
}

void rcs_cleanup(int code)
{
    RCS_EXIT_LIST_ENTRY *entry;
    long process_id = 0;

    if (NULL == exit_list) {
	return;
    }
    entry = (RCS_EXIT_LIST_ENTRY *) exit_list->get_head();
    while (NULL != entry) {
	if (entry->process_id == process_id && entry->fptr != NULL) {
	    entry->fptr(code);
	}
	entry = (RCS_EXIT_LIST_ENTRY *) exit_list->get_next();
    }
    if (exit_list->list_size == 0) {
	delete exit_list;
	exit_list = (LinkedList *) NULL;
    }
}

static int rcs_ready_for_exit = 0;
static int rcs_exit_sig = 0;
static void rcs_exit_signal_handler(int sig)
{
    rcs_ready_for_exit = 1;
    rcs_exit_sig = sig;
}

void rcs_exit(int code)
{
    rcs_cleanup(code);
    if (code == -1) {
	rcs_print_error("\n Errors Reported!!!\n Press ^C to exit.\n");
	signal(SIGINT, rcs_exit_signal_handler);
	int secs = 0;
	while (!rcs_ready_for_exit && secs < 600) {
	    esleep(1.0);
	    secs++;
	}
    }
    exit(code);
}
