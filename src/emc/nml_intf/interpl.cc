/********************************************************************
* Description: interpl.cc
*   Mechanism for queueing NML messages, used by the interpreter and
*   canonical interface to report list of NML statements from program
*   files to HME.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/


#include <string.h>		/* memcpy() */

#include "rcs.hh"		// LinkedList
#include "interpl.hh"		// these decls
#include "emc.hh"
#include "emcglb.h"
#include "linklist.hh"
#include "nmlmsg.hh"            /* class NMLmsg */
#include "rcs_print.hh"

NML_INTERP_LIST interp_list;	/* NML Union, for interpreter */

NML_INTERP_LIST::NML_INTERP_LIST()
{
    linked_list_ptr = new LinkedList;

    next_line_number = 0;
    line_number = 0;
}

NML_INTERP_LIST::~NML_INTERP_LIST()
{
    if (NULL != linked_list_ptr) {
	delete linked_list_ptr;
	linked_list_ptr = NULL;
    }
}

int NML_INTERP_LIST::append(NMLmsg & nml_msg)
{
    return append(&nml_msg);
}

// sets the line number used for subsequent appends
int NML_INTERP_LIST::set_line_number(int line)
{
    next_line_number = line;

    return 0;
}

int NML_INTERP_LIST::append(NMLmsg * nml_msg_ptr)
{
    /* check for invalid data */
    if (NULL == nml_msg_ptr) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : attempt to append NULL msg\n");
	return -1;
    }

    if (0 == nml_msg_ptr->type) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : attempt to append 0 type\n");
	return -1;
    }

    if (nml_msg_ptr->size > MAX_NML_COMMAND_SIZE - 64) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : command size is too large.");
	return -1;
    }
    if (nml_msg_ptr->size < 4) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : command size is invalid.");
	return -1;
    }
#ifdef DEBUG_INTERPL
    if (sizeof(temp_node) < MAX_NML_COMMAND_SIZE + 4 ||
	sizeof(temp_node) > MAX_NML_COMMAND_SIZE + 16 ||
	((void *) &temp_node.line_number) >
	((void *) &temp_node.command.commandbuf)) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : assumptions about NML_INTERP_LIST_NODE have been violated.");
	return -1;
    }
#endif

    if (NULL == linked_list_ptr) {
	return -1;
    }
    // fill in the NML_INTERP_LIST_NODE
    temp_node.line_number = next_line_number;
    memcpy(temp_node.command.commandbuf, nml_msg_ptr, nml_msg_ptr->size);

    // stick it on the list
    linked_list_ptr->store_at_tail(&temp_node,
				   nml_msg_ptr->size +
				   sizeof(temp_node.line_number) +
				   sizeof(temp_node.dummy) + 32 + (32 -
								   nml_msg_ptr->
								   size %
								   32), 1);

    if (emc_debug & EMC_DEBUG_INTERP_LIST) {
	rcs_print
	    ("NML_INTERP_LIST::append(nml_msg_ptr{size=%ld,type=%s}) : list_size=%d, line_number=%d\n",
	     nml_msg_ptr->size, emc_symbol_lookup(nml_msg_ptr->type),
	     linked_list_ptr->list_size, temp_node.line_number);
    }

    return 0;
}

NMLmsg *NML_INTERP_LIST::get()
{
    NMLmsg *ret;
    NML_INTERP_LIST_NODE *node_ptr;

    if (NULL == linked_list_ptr) {
	line_number = 0;
	return NULL;
    }

    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->retrieve_head();

    if (NULL == node_ptr) {
	line_number = 0;
	return NULL;
    }
    // save line number of this one, for use by get_line_number
    line_number = node_ptr->line_number;

    // get it off the front
    ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);

    return ret;
}

void NML_INTERP_LIST::clear()
{
    if (NULL != linked_list_ptr) {
	linked_list_ptr->delete_members();
    }
}

void NML_INTERP_LIST::print()
{
    NMLmsg *ret;
    NML_INTERP_LIST_NODE *node_ptr;
    int line_number;

    if (NULL == linked_list_ptr) {
	return;
    }
    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_head();

    rcs_print("NML_INTERP_LIST::print(): list size=%d\n",linked_list_ptr->list_size);
    while (NULL != node_ptr) {
	line_number = node_ptr->line_number;
	ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);
	rcs_print("--> type=%s,  line_number=%d\n",
		  emc_symbol_lookup((int)ret->type),
		  line_number);
	node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_next();
    }
    rcs_print("\n");
}

int NML_INTERP_LIST::len()
{
    if (NULL == linked_list_ptr) {
	return 0;
    }

    return ((int) linked_list_ptr->list_size);
}

int NML_INTERP_LIST::get_line_number()
{
    return line_number;
}
