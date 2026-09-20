/********************************************************************
* Description: cmd_queue.hh
*   Command injection point for everything inside task that issues NML
*   commands without going through the NML command channel.
*
*   Task executes one command per cycle, taken either from the NML command
*   buffer or from this queue. A command placed here is therefore judged by
*   emcTaskPlan() exactly like one that arrived over NML: same mode and
*   state gating, same serial-number echo, same error reporting.
*
*   Two producers use it:
*     - ws_server.cc, from the websocket server thread
*     - halui.cc, from the task thread
*
* License: GPL Version 2
********************************************************************/
#ifndef TASK_CMD_QUEUE_HH
#define TASK_CMD_QUEUE_HH

#include <cstddef>

class RCS_CMD_MSG;

namespace taskcmd {

/* Biggest NML command that can be queued. EMC_TASK_PLAN_OPEN is the large
   one (a LINELEN path plus a 4k remote transfer buffer). */
const size_t MAX_CMD_SIZE = 8192;

/* Identifies one queued command, so a producer can tell whether task has
   taken it yet. Zero is never a valid ticket: push() returns 0 when the
   queue is full, and pending(0) is false. */
typedef unsigned long Ticket;

/* Queue a copy of a command message. Thread safe.

   coalesce_key, when non-zero, means "this command supersedes any command
   still queued under the same key": the pending one is overwritten in
   place, keeping its position in the queue. It is for commands that carry
   a state rather than an action -- a continuous jog is the case that
   matters, where a producer running faster than task drains the queue
   would otherwise build a backlog of stale velocities. Never use it for a
   command that must happen exactly once, such as an incremental jog.

   The superseded command's ticket stops being pending without ever having
   been executed, so a producer that watches a ticket must pass key 0. */
Ticket pushRaw(const void *cmd, size_t size, unsigned long coalesce_key);

template <class T>
Ticket push(const T &cmd, unsigned long coalesce_key = 0)
{
    static_assert(sizeof(T) <= MAX_CMD_SIZE, "NML command too large to queue");
    return pushRaw(&cmd, sizeof(T), coalesce_key);
}

/* True while the command is still waiting. False once task has taken it --
   which, for a caller running in the task loop after the status update,
   means the command has been issued and the status reflects it. */
bool pending(Ticket ticket);

/* Pop the next command, or NULL if none is waiting. Task loop only.

   The returned message stays valid until the next call and is meant to be
   consumed exactly like one freshly read from NML. echo_serial_number is
   the serial task last echoed; the message gets echo_serial_number + 1 so
   that task's "is this a new command?" test fires. Task puts the serial
   back afterwards without echoing it: an NML serial is the command
   buffer's write id, and echoing one of those here would make task drop
   the next command an NML client writes. */
RCS_CMD_MSG *next(int echo_serial_number);

} // namespace taskcmd

#endif /* TASK_CMD_QUEUE_HH */
