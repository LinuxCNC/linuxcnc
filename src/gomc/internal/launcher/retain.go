// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package launcher — retain.go integrates retain signal persistence directly
// into the launcher process.
//
// The retain system saves and restores HAL signal values marked with the
// HAL_SIGFLAG_RETAIN flag across sessions.  It consists of two parts:
//
//  1. RT function (C, runs in servo-thread): snapshots all retain-flagged
//     signal values atomically within a single servo cycle, ensuring
//     process image consistency.
//
//  2. Userspace goroutine (Go): periodically triggers the RT snapshot,
//     detects changes, and persists values to a var file on disk.
//
// Communication between the RT function and the Go goroutine uses a single
// atomic uint32 action field in process-local memory (not a HAL param),
// following the same request/response protocol as the original retain
// components but without inter-process IPC.
package launcher

/*
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include -I${SRCDIR}/../../generated/gmi/persist
#cgo LDFLAGS:

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdatomic.h>
#include "hal.h"
#include "hal_priv.h"

#define PERSIST_API_CGO
#include "persist_api.h"

// retain action values — same protocol as retain.h.
#define RETAIN_ACTION_NOOP  0
#define RETAIN_ACTION_READ  1
#define RETAIN_ACTION_STORE 2

// retain_state_t holds the shared state between the RT function and the
// userspace goroutine.  Allocated via calloc in retain_state_create().
typedef struct {
    _Atomic uint32_t action;
    const persist_callbacks_t *persist;
    int32_t handle;
} retain_state_t;

static retain_state_t *retain_state_create(void) {
    return (retain_state_t *)calloc(1, sizeof(retain_state_t));
}

static void retain_state_destroy(retain_state_t *st) {
    free(st);
}

static int32_t retain_open_namespace(retain_state_t *st, const char *ns) {
    persist_open_result_t res = st->persist->open(st->persist->ctx, ns);
    return res.handle;
}

// retain_sync is the RT function exported as "retain.sync".
// It runs in the servo thread and snapshots retain-flagged signal values
// when triggered by the userspace goroutine.
static void retain_sync(void *arg, long period) {
    retain_state_t *st = (retain_state_t *)arg;
    int changed = 0;
    void *next;
    hal_sig_t *sig;
    void *data_addr;
    hal_data_t *hd = hal_data;

    // Only act when userspace requests a read.
    if (atomic_load_explicit(&st->action, memory_order_acquire) != RETAIN_ACTION_READ) {
        return;
    }

    // Try to get the mutex — skip this cycle if contended.
    if (rtapi_mutex_try(&(hd->mutex))) {
        return;
    }

    next = hd->sig_list_ptr;
    while (next != 0) {
        sig = SHMPTR(next);
        next = sig->next_ptr;
        data_addr = SHMPTR(sig->data_ptr);

        if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) {
            continue;
        }

        switch (sig->type) {
        case HAL_BIT:
            if (sig->retain_val.bit != *((hal_bit_t *)data_addr)) {
                changed = 1;
                sig->retain_val.bit = *((hal_bit_t *)data_addr);
            }
            break;
        case HAL_U32:
            if (sig->retain_val.u32 != *((hal_u32_t *)data_addr)) {
                changed = 1;
                sig->retain_val.u32 = *((hal_u32_t *)data_addr);
            }
            break;
        case HAL_S32:
            if (sig->retain_val.s32 != *((hal_s32_t *)data_addr)) {
                changed = 1;
                sig->retain_val.s32 = *((hal_s32_t *)data_addr);
            }
            break;
        case HAL_FLOAT:
            if (sig->retain_val.flt != *((hal_float_t *)data_addr)) {
                changed = 1;
                sig->retain_val.flt = *((hal_float_t *)data_addr);
            }
            break;
        default:
            break;
        }
    }

    rtapi_mutex_give(&(hd->mutex));

    // Signal result to userspace.
    atomic_store_explicit(&st->action,
        changed ? RETAIN_ACTION_STORE : RETAIN_ACTION_NOOP,
        memory_order_release);
}

// retain_init creates the "retain" pseudo-component, exports the RT function,
// and returns the component ID.  The caller must call hal_ready() after addf.
static int retain_init(retain_state_t *st) {
    int comp_id = hal_init_ex("retain", NULL, COMPONENT_TYPE_REALTIME);
    if (comp_id < 0) {
        return comp_id;
    }

    if (hal_export_funct("retain.sync", retain_sync, (void *)st, 1, 0, comp_id) != 0) {
        hal_exit(comp_id);
        return -1;
    }

    return comp_id;
}

// retain_load_vars restores signal values from the persist backend.
// Returns 0 on success, -1 on error.
static int retain_load_vars(retain_state_t *st) {
    hal_data_t *hd = hal_data;
    hal_sig_t *sig;
    void *data_addr;

    if (st->persist == NULL) return -1;

    persist_get_entries_result_t res = st->persist->get_entries(
        st->persist->ctx, st->handle);
    if (res.len == 0) {
        if (res.data) free(res.data);
        return 0;
    }

    rtapi_mutex_get(&(hd->mutex));
    for (size_t i = 0; i < res.len; i++) {
        const char *name = res.data[i].key;
        const char *value = res.data[i].value;
        if (name == NULL || value == NULL) continue;

        sig = halpr_find_sig_by_name(name);
        if (sig == NULL) continue;
        if (sig->writers > 0) continue;
        if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) continue;

        data_addr = SHMPTR(sig->data_ptr);
        switch (sig->type) {
        case HAL_BIT:
            if (strcmp("1", value) == 0 || strcasecmp("TRUE", value) == 0) {
                *((hal_bit_t *)data_addr) = 1;
            } else if (strcmp("0", value) == 0 || strcasecmp("FALSE", value) == 0) {
                *((hal_bit_t *)data_addr) = 0;
            }
            break;
        case HAL_U32: {
            char *endp;
            uint32_t v = strtoul(value, &endp, 0);
            if (*endp == '\0') *((hal_u32_t *)data_addr) = v;
            break;
        }
        case HAL_S32: {
            char *endp;
            int32_t v = strtol(value, &endp, 0);
            if (*endp == '\0') *((hal_s32_t *)data_addr) = v;
            break;
        }
        case HAL_FLOAT: {
            char *endp;
            double v = strtod(value, &endp);
            if (*endp == '\0') *((hal_float_t *)data_addr) = v;
            break;
        }
        default:
            break;
        }
    }
    rtapi_mutex_give(&(hd->mutex));

    free(res.data);
    return 0;
}

// retain_save_vars saves retain-flagged signal values via persist backend.
// Returns 0 on success, -1 on error.
static int retain_save_vars(retain_state_t *st) {
    hal_data_t *hd = hal_data;
    void *next;
    hal_sig_t *sig;
    size_t count = 0;
    size_t cap = 64;

    if (st->persist == NULL) return -1;

    persist_entry_t *entries = (persist_entry_t *)malloc(cap * sizeof(persist_entry_t));
    if (entries == NULL) return -1;
    char **value_bufs = (char **)malloc(cap * sizeof(char *));
    if (value_bufs == NULL) { free(entries); return -1; }

    rtapi_mutex_get(&(hd->mutex));
    next = hd->sig_list_ptr;
    while (next != 0) {
        sig = SHMPTR(next);
        next = sig->next_ptr;
        if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) continue;

        if (count >= cap) {
            cap *= 2;
            entries = (persist_entry_t *)realloc(entries, cap * sizeof(persist_entry_t));
            value_bufs = (char **)realloc(value_bufs, cap * sizeof(char *));
            if (entries == NULL || value_bufs == NULL) {
                rtapi_mutex_give(&(hd->mutex));
                goto cleanup;
            }
        }

        char vbuf[64];
        switch (sig->type) {
        case HAL_BIT:
            snprintf(vbuf, sizeof(vbuf), "%s", sig->retain_val.bit ? "TRUE" : "FALSE");
            break;
        case HAL_U32:
            snprintf(vbuf, sizeof(vbuf), "%u", sig->retain_val.u32);
            break;
        case HAL_S32:
            snprintf(vbuf, sizeof(vbuf), "%d", sig->retain_val.s32);
            break;
        case HAL_FLOAT:
            snprintf(vbuf, sizeof(vbuf), "%.6f", sig->retain_val.flt);
            break;
        default:
            continue;
        }

        value_bufs[count] = strdup(vbuf);
        entries[count].key = sig->name;
        entries[count].value = value_bufs[count];
        entries[count].updated = 0;
        count++;
    }
    rtapi_mutex_give(&(hd->mutex));

    if (count > 0) {
        st->persist->set_entries(st->persist->ctx, st->handle, entries, count);
    }

    for (size_t i = 0; i < count; i++) {
        free(value_bufs[i]);
    }
    free(value_bufs);
    free(entries);
    return 0;

cleanup:
    for (size_t i = 0; i < count; i++) {
        free(value_bufs[i]);
    }
    free(value_bufs);
    free(entries);
    return -1;
}
*/
import "C"
import (
	"context"
	"fmt"
	"runtime"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	halcmd "github.com/sittner/linuxcnc/src/gomc/internal/halcmd"
)

const (
	retainActionNoop  = 0
	retainActionRead  = 1
	retainActionStore = 2

	retainDefaultPollPeriod = 1000 // ms
	retainSyncTimeout       = time.Second
)

// retainInstance holds all state for the integrated retain subsystem.
type retainInstance struct {
	compID int
	state  *C.retain_state_t
	poll   time.Duration
	cancel context.CancelFunc
	done   chan struct{} // closed when goroutine exits
}

// loadRetain checks for retained HAL signals and, if any are found, sets up
// the integrated retain subsystem: creates a "retain" pseudo-component with
// an exported RT function, restores saved values, and starts a background
// goroutine for periodic persistence.
func (l *Launcher) loadRetain() error {
	// Check whether any retained signals exist.
	signals, err := halcmd.List("retain")
	if err != nil {
		l.logger.Debug("hal list retain returned error (no retain signals)", "error", err)
		return nil
	}
	if len(signals) == 0 {
		l.logger.Debug("no retained signals found, skipping retain load")
		return nil
	}

	l.logger.Info("loading retain", "signals", len(signals))

	// Allocate the shared state for RT↔userspace communication.
	state := C.retain_state_create()
	if state == nil {
		return fmt.Errorf("retain: failed to allocate state")
	}

	// Create the "retain" pseudo-component and export retain.sync.
	compID := int(C.retain_init(state))
	if compID < 0 {
		C.retain_state_destroy(state)
		return fmt.Errorf("retain: hal_init/export_funct failed")
	}

	// Determine the sync thread.
	syncThread := l.ini.Get("RETAIN", "SYNC_THREAD")
	if syncThread == "" {
		syncThread = "servo-thread"
	}
	if err := halcmd.AddF("retain.sync", syncThread, -1); err != nil {
		C.hal_exit(C.int(compID))
		C.retain_state_destroy(state)
		return fmt.Errorf("retain: addf retain.sync %s: %w", syncThread, err)
	}

	// Mark the component ready now that the function is added.
	if ret := C.hal_ready(C.int(compID)); ret != 0 {
		C.hal_exit(C.int(compID))
		C.retain_state_destroy(state)
		return fmt.Errorf("retain: hal_ready failed: %d", int(ret))
	}

	// Look up persist API.
	reg := apiserver.DefaultRegistry()
	persistInstance := "persistence"
	if ps := l.ini.Get("RETAIN", "PERSIST_INSTANCE"); ps != "" {
		persistInstance = ps
	}
	persistCbs, err := reg.GetAPIFor("retain", "persist", persistInstance, 2)
	if err != nil {
		C.hal_exit(C.int(compID))
		C.retain_state_destroy(state)
		return fmt.Errorf("retain: persist API lookup (%s): %w", persistInstance, err)
	}
	state.persist = (*C.persist_callbacks_t)(persistCbs)

	// Open the hal_retain namespace.
	cNs := C.CString("hal_retain")
	state.handle = C.retain_open_namespace(state, cNs)
	C.free(unsafe.Pointer(cNs))

	// Determine the poll period.
	pollPeriod := retainDefaultPollPeriod
	if ps := l.ini.Get("RETAIN", "POLL_PERIOD"); ps != "" {
		if v := parseInt(ps); v > 0 {
			pollPeriod = v
		}
	}

	// Restore values from persist.
	if C.retain_load_vars(state) != 0 {
		l.logger.Warn("retain: failed to load from persist")
	} else {
		l.logger.Info("retain: restored values from persist")
	}

	// Start the background goroutine.
	ctx, cancel := context.WithCancel(context.Background())
	ri := &retainInstance{
		compID: compID,
		state:  state,
		poll:   time.Duration(pollPeriod) * time.Millisecond,
		cancel: cancel,
		done:   make(chan struct{}),
	}
	l.retain = ri

	go l.retainLoop(ctx, ri)

	return nil
}

// retainLoop is the background goroutine that periodically triggers the RT
// snapshot and saves changed values to disk.
func (l *Launcher) retainLoop(ctx context.Context, ri *retainInstance) {
	defer close(ri.done)

	// Do an initial sync to populate retain_val fields.
	if l.retainSync(ri) {
		l.retainSave(ri)
	}

	ticker := time.NewTicker(ri.poll)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			// Final sync before exit.
			if l.retainSync(ri) {
				l.retainSave(ri)
			}
			return
		case <-ticker.C:
			if l.retainSync(ri) {
				l.retainSave(ri)
			}
		}
	}
}

// retainSync triggers the RT snapshot and waits for completion.
// Returns true if data has changed and needs saving.
func (l *Launcher) retainSync(ri *retainInstance) bool {
	// Request the RT function to snapshot.
	atomic.StoreUint32((*uint32)(unsafe.Pointer(&ri.state.action)), retainActionRead)

	// Wait for the RT function to complete the snapshot.
	deadline := time.Now().Add(retainSyncTimeout)
	for atomic.LoadUint32((*uint32)(unsafe.Pointer(&ri.state.action))) == retainActionRead {
		if time.Now().After(deadline) {
			l.logger.Warn("retain: sync timeout")
			return false
		}
		runtime.Gosched()
	}

	return atomic.LoadUint32((*uint32)(unsafe.Pointer(&ri.state.action))) == retainActionStore
}

// retainSave persists retain signal values via the persist backend.
func (l *Launcher) retainSave(ri *retainInstance) {
	if C.retain_save_vars(ri.state) != 0 {
		l.logger.Warn("retain: failed to save to persist")
	}
}

// stopRetain signals the retain goroutine to stop and waits for it to finish.
// Must be called before StopThreads so the final sync can still execute.
func (l *Launcher) stopRetain() {
	if l.retain == nil {
		return
	}
	l.retain.cancel()
	<-l.retain.done
}

// destroyRetain releases the retain HAL component and frees the shared state.
// Must be called after StopThreads.
func (l *Launcher) destroyRetain() {
	if l.retain == nil {
		return
	}
	C.hal_exit(C.int(l.retain.compID))
	C.retain_state_destroy(l.retain.state)
	l.retain = nil
}

// parseInt parses a decimal integer string, returning 0 on error.
func parseInt(s string) int {
	var v int
	_, err := fmt.Sscanf(s, "%d", &v)
	if err != nil {
		return 0
	}
	return v
}
