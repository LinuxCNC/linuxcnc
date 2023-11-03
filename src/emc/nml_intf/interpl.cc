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

#include "interpl.hh"   // these decls
#include "emc.hh"
#include "emcglb.h"
#include "nmlmsg.hh"    // class NMLmsg
#include "rcs_print.hh"

NML_INTERP_LIST interp_list; /* NML Union, for interpreter */


// sets the line number used for subsequent appends
void NML_INTERP_LIST::set_line_number(int line)
{
    next_line_number = line;
}

int NML_INTERP_LIST::append(std::unique_ptr<NMLmsg>&& nml_msg_ptr)
{
    /* check for invalid data */
    if (NULL == nml_msg_ptr) {
        rcs_print_error("NML_INTERP_LIST::append : attempt to append NULL msg\n");
        return -1;
    }

    if (0 == nml_msg_ptr->type) {
        rcs_print_error("NML_INTERP_LIST::append : attempt to append 0 type\n");
        return -1;
    }

    if (nml_msg_ptr->size < 4) {
        rcs_print_error("NML_INTERP_LIST::append : command size is invalid.");
        return -1;
    }

    NML_INTERP_LIST_NODE node {next_line_number, std::move(nml_msg_ptr)};

    // stick it on the list
    linked_list.emplace_back(std::move(node));

    if (emc_debug & EMC_DEBUG_INTERP_LIST) {
        rcs_print("NML_INTERP_LIST(%p)::append(nml_msg_ptr{size=%ld,type=%s}) : list_size=%lu, "
                  "line_number=%d\n",
                  this,
                  linked_list.front().command->size,
                  emc_symbol_lookup(linked_list.front().command->type),
                  linked_list.size(),
                  linked_list.front().line_number);
    }

    return 0;
}

std::unique_ptr<NMLmsg> NML_INTERP_LIST::get()
{
    if (linked_list.empty()) {
        line_number = 0;
        return NULL;
    }

    // get it off the front
    auto node = std::move(linked_list.front());
    linked_list.pop_front();

    // save line number of this one, for use by get_line_number
    line_number = node.line_number;

    if (emc_debug & EMC_DEBUG_INTERP_LIST) {
        rcs_print("NML_INTERP_LIST(%p)::get(): {size=%ld, type=%s}, list_size=%lu\n",
                  this,
                  node.command->size,
                  emc_symbol_lookup(node.command->type),
                  linked_list.size());
    }

    return std::move(node.command);
}

void NML_INTERP_LIST::clear()
{
    if (emc_debug & EMC_DEBUG_INTERP_LIST) {
        rcs_print("NML_INTERP_LIST(%p)::clear(): discarding %lu items\n", this, linked_list.size());
    }
    linked_list.clear();
}

void NML_INTERP_LIST::print()
{
    rcs_print("NML_INTERP_LIST::print(): list size=%lu\n", linked_list.size());
    for (auto& i : linked_list) {
        auto& msg = *(i.command);
        rcs_print("--> type=%s,  line_number=%d\n", emc_symbol_lookup(msg.type), i.line_number);
    }
    rcs_print("\n");
}

int NML_INTERP_LIST::len()
{
    return ((int)linked_list.size());
}

int NML_INTERP_LIST::get_line_number()
{
    return line_number;
}
