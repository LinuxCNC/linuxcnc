/********************************************************************
 * Description: interpl.hh
 *   Mechanism for building lists of arbitrary NML messages, used by
 *   the canonical interface and interpreter to pass planned sequences
 *   to the HME.
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
#ifndef INTERP_LIST_HH
#define INTERP_LIST_HH

#include <deque>
#include <memory>

class NMLmsg;

// these go on the interp list
struct NML_INTERP_LIST_NODE
{
    int line_number;  // line number it was on
    // std::vector<char> command;
    std::unique_ptr<NMLmsg> command;
};

// here's the interp list itself
class NML_INTERP_LIST
{
public:
    void set_line_number(int line);
    int get_line_number();
    int append(std::unique_ptr<NMLmsg>&& command);
    std::unique_ptr<NMLmsg> get();
    void clear();
    void print();
    int len();

private:
    std::deque<NML_INTERP_LIST_NODE> linked_list;
    int next_line_number = 0;  // line number used to fill temp_node
    int line_number = 0;       // line number of node from get()
                               // NML_INTERP_LIST_NODE node; // pointer returned by get
};

extern NML_INTERP_LIST interp_list; /* NML Union, for interpreter */

#endif
