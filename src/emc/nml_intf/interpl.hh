/********************************************************************
 * Description: interpl.hh
 *   A queue of planned messages, filled by the canonical interface as
 *   the interpreter reads ahead and drained by task, one message per
 *   cycle.
 *
 *   The queue itself is deliberately ignorant of what a message is: it
 *   stores them, remembers which source line each came from, and hands
 *   them back in order. Bind it to a concrete message type by
 *   specialising InterpListTraits -- see interpl_nml.hh, which is what
 *   ties it to the NML message classes today.
 *
 *   Derived from a work by Fred Proctor & Will Shackleford
 *
 * Author:
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2004 All rights reserved.
 ********************************************************************/
#ifndef INTERP_LIST_HH
#define INTERP_LIST_HH

#include <cstdio>
#include <deque>
#include <memory>

#include "debugflags.h"         // EMC_DEBUG_INTERP_LIST
#include "emcglb.h"             // emc_debug

/**
 * Per-message-type hooks for InterpList.
 *
 * All the list needs to know about a message is whether it is fit to be
 * queued and what to call it in a debug trace. The defaults suit a type
 * that carries no such notion; specialise this for one that does.
 */
template <class Msg>
struct InterpListTraits {
    /// True if the message is structurally fit to be queued.
    static bool valid(const Msg &) { return true; }
    /// Short name for the message, used only in EMC_DEBUG_INTERP_LIST traces.
    static const char *name(const Msg &) { return "message"; }
};

template <class Msg>
class InterpList
{
  public:
    /// Line number stamped onto every message appended from here on.
    void set_line_number(int line) { next_line_number = line; }

    /// Line number of the message most recently handed out by get().
    int get_line_number() const { return line_number; }

    int len() const { return static_cast<int>(linked_list.size()); }

    /// Queue a message. Returns 0, or -1 if the message was rejected.
    int append(std::unique_ptr<Msg> &&command);

    /// Pop the oldest message, or nullptr when the queue is empty.
    std::unique_ptr<Msg> get();

    void clear();
    void print() const;

  private:
    struct Node {
        int line_number;
        std::unique_ptr<Msg> command;
    };

    static bool tracing() { return (emc_debug & EMC_DEBUG_INTERP_LIST) != 0; }

    std::deque<Node> linked_list;
    int next_line_number = 0;  // stamped onto the next append()
    int line_number = 0;       // of the node last returned by get()
};

template <class Msg>
int InterpList<Msg>::append(std::unique_ptr<Msg> &&command)
{
    if (command == nullptr) {
        fprintf(stderr, "InterpList::append: attempt to append NULL msg\n");
        return -1;
    }
    if (!InterpListTraits<Msg>::valid(*command)) {
        fprintf(stderr, "InterpList::append: attempt to append a malformed msg\n");
        return -1;
    }

    linked_list.push_back(Node{next_line_number, std::move(command)});

    if (tracing()) {
        // Note: this reports the message just appended. The NML version
        // this replaces printed linked_list.front() here, i.e. the oldest
        // queued message rather than the new one.
        const Node &node = linked_list.back();
        printf("InterpList(%p)::append(%s): list_size=%zu, line_number=%d\n",
               static_cast<const void *>(this),
               InterpListTraits<Msg>::name(*node.command),
               linked_list.size(), node.line_number);
    }

    return 0;
}

template <class Msg>
std::unique_ptr<Msg> InterpList<Msg>::get()
{
    if (linked_list.empty()) {
        line_number = 0;
        return nullptr;
    }

    Node node = std::move(linked_list.front());
    linked_list.pop_front();

    // saved for get_line_number(), which task reads straight after get()
    line_number = node.line_number;

    if (tracing()) {
        printf("InterpList(%p)::get(): %s, list_size=%zu\n",
               static_cast<const void *>(this),
               InterpListTraits<Msg>::name(*node.command),
               linked_list.size());
    }

    return std::move(node.command);
}

template <class Msg>
void InterpList<Msg>::clear()
{
    if (tracing()) {
        printf("InterpList(%p)::clear(): discarding %zu items\n",
               static_cast<const void *>(this), linked_list.size());
    }
    linked_list.clear();
}

template <class Msg>
void InterpList<Msg>::print() const
{
    printf("InterpList::print(): list size=%zu\n", linked_list.size());
    for (const auto &node : linked_list) {
        printf("--> type=%s,  line_number=%d\n",
               InterpListTraits<Msg>::name(*node.command), node.line_number);
    }
    printf("\n");
}

#endif
