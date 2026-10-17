/********************************************************************
* Description: cmd_queue.cc
*   Command injection point for task-internal command producers.
*   See cmd_queue.hh for what it is for.
*
* License: GPL Version 2
********************************************************************/

#include "cmd_queue.hh"

#include "nml_intf/emc.hh"
#include "nml_intf/emc_nml.hh"

#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

namespace {

struct Entry {
    std::vector<char>  data;    /* the message, byte for byte */
    taskcmd::Ticket    ticket = 0;
    unsigned long      key = 0; /* 0: never superseded */
};

std::mutex         g_mu;        /* guards everything below */
std::deque<Entry>  g_queue;
Entry              g_current;   /* keeps the popped message alive */
taskcmd::Ticket    g_next_ticket = 1;

/* A backlog this long means the producer is far ahead of task and the
   commands are stale anyway; dropping is better than growing without
   bound. Jogs coalesce, so reaching this takes a genuine flood. */
const size_t MAX_QUEUED = 64;

} // namespace

taskcmd::Ticket taskcmd::pushRaw(const void *cmd, size_t size,
                                 unsigned long coalesce_key)
{
    if (cmd == NULL || size == 0 || size > MAX_CMD_SIZE) {
        return 0;
    }

    std::lock_guard<std::mutex> lk(g_mu);

    const Ticket ticket = g_next_ticket++;

    if (coalesce_key != 0) {
        for (Entry &e : g_queue) {
            if (e.key != coalesce_key) {
                continue;
            }
            /* Supersede in place: the newer command takes the older one's
               turn, so ordering against other commands is unchanged. */
            e.data.assign(static_cast<const char *>(cmd),
                          static_cast<const char *>(cmd) + size);
            e.ticket = ticket;
            return ticket;
        }
    }

    if (g_queue.size() >= MAX_QUEUED) {
        return 0;
    }

    Entry e;
    e.data.assign(static_cast<const char *>(cmd),
                  static_cast<const char *>(cmd) + size);
    e.ticket = ticket;
    e.key = coalesce_key;
    g_queue.push_back(std::move(e));

    return ticket;
}

bool taskcmd::pending(Ticket ticket)
{
    if (ticket == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lk(g_mu);
    for (const Entry &e : g_queue) {
        if (e.ticket == ticket) {
            return true;
        }
    }
    return false;
}

RCS_CMD_MSG *taskcmd::next(int echo_serial_number)
{
    std::lock_guard<std::mutex> lk(g_mu);

    if (g_queue.empty()) {
        return NULL;
    }

    g_current = std::move(g_queue.front());
    g_queue.pop_front();

    /* Task treats a command as new when its serial differs from the one it
       last echoed, so hand it the next serial along. */
    RCS_CMD_MSG *msg = reinterpret_cast<RCS_CMD_MSG *>(g_current.data.data());
    msg->serial_number = echo_serial_number + 1;
    return msg;
}
