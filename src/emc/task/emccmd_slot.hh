#ifndef EMCCMD_SLOT_HH
#define EMCCMD_SLOT_HH

// Synchronous command slot for GMI → milltask command path.
//
// GMI callbacks (called from Go goroutines) submit commands via
// emccmd_submit(), which blocks until milltask processes the command
// and calls emccmd_slot_done().  This gives callers synchronous
// semantics — the HTTP response means "command processed."

#include <cstddef>

class RCS_CMD_MSG;

// Initialize the command slot.  Called once from milltask New().
void emccmd_slot_init();

// Submit a command and block until milltask processes it.
// Thread-safe.  Only one caller can submit at a time (serialized by mutex).
// Returns the RCS_STATUS result set by milltask via emccmd_slot_done().
int emccmd_submit(RCS_CMD_MSG *msg, size_t msg_size);

// Called by milltask main loop: take the pending command, if any.
// Copies the command into `buf` (which must be at least `buf_size` bytes).
// Returns the message size on success, 0 if no command is pending.
size_t emccmd_slot_take(void *buf, size_t buf_size);

// Called by milltask main loop after processing the command.
// Stores `status` (RCS_DONE, RCS_ERROR, etc.) and unblocks the caller.
void emccmd_slot_done(int status);

#endif // EMCCMD_SLOT_HH
