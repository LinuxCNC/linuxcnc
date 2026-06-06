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
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include
#cgo LDFLAGS:

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdatomic.h>
#include "hal.h"
#include "hal_priv.h"

// retain action values — same protocol as retain.h.
#define RETAIN_ACTION_NOOP  0
#define RETAIN_ACTION_READ  1
#define RETAIN_ACTION_STORE 2

// retain_state_t holds the shared state between the RT function and the
// userspace goroutine.  Allocated via calloc in retain_state_create().
typedef struct {
    _Atomic uint32_t action;
} retain_state_t;

static retain_state_t *retain_state_create(void) {
    return (retain_state_t *)calloc(1, sizeof(retain_state_t));
}

static void retain_state_destroy(retain_state_t *st) {
    free(st);
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

// retain_load_vars restores signal values from a var file.
// Returns 0 on success, -1 on error.
static int retain_load_vars(const char *file_name) {
    hal_data_t *hd = hal_data;
    FILE *f;
    char line[1024];
    char *name, *value, *s;
    hal_sig_t *sig;
    void *data_addr;

    f = fopen(file_name, "r");
    if (f == NULL) {
        return -1;
    }

    rtapi_mutex_get(&(hd->mutex));
    while (fgets(line, sizeof(line), f)) {
        // Skip leading whitespace.
        for (name = line; *name && (*name == '\t' || *name == ' '); name++)
            ;

        // Skip comment lines.
        if (*name == '#') {
            continue;
        }

        // Split name and value on first space.
        value = strchr(name, ' ');
        if (value == NULL) {
            continue;
        }
        *(value++) = '\0';

        if (*name == '\0') {
            continue;
        }

        // Terminate value at first whitespace.
        for (s = value; *s && *s != '\t' && *s != ' ' && *s != '\r' && *s != '\n'; s++)
            ;
        *s = '\0';

        if (*value == '\0') {
            continue;
        }

        sig = halpr_find_sig_by_name(name);
        if (sig == NULL) {
            continue;
        }

        // Only restore retain-flagged signals without writers.
        if (sig->writers > 0) {
            continue;
        }
        if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) {
            continue;
        }

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
            if (*endp == '\0') {
                *((hal_u32_t *)data_addr) = v;
            }
            break;
        }
        case HAL_S32: {
            char *endp;
            int32_t v = strtol(value, &endp, 0);
            if (*endp == '\0') {
                *((hal_s32_t *)data_addr) = v;
            }
            break;
        }
        case HAL_FLOAT: {
            char *endp;
            double v = strtod(value, &endp);
            if (*endp == '\0') {
                *((hal_float_t *)data_addr) = v;
            }
            break;
        }
        default:
            break;
        }
    }
    rtapi_mutex_give(&(hd->mutex));

    fclose(f);
    return 0;
}

// retain_save_vars saves retain-flagged signal values to a var file.
// Uses atomic write (tmp + rename).  Returns 0 on success, -1 on error.
static int retain_save_vars(const char *file_name) {
    hal_data_t *hd = hal_data;
    char tmp_name[256];
    FILE *f = NULL;
    void *next;
    hal_sig_t *sig;
    int ret;

    if (snprintf(tmp_name, sizeof(tmp_name), "%s.tmp", file_name) >= (int)sizeof(tmp_name)) {
        return -1;
    }

    f = fopen(tmp_name, "w");
    if (f == NULL) {
        return -1;
    }

    rtapi_mutex_get(&(hd->mutex));
    next = hd->sig_list_ptr;
    while (next != 0) {
        sig = SHMPTR(next);
        next = sig->next_ptr;

        if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) {
            continue;
        }

        switch (sig->type) {
        case HAL_BIT:
            ret = fprintf(f, "%s %s\n", sig->name, sig->retain_val.bit ? "TRUE" : "FALSE");
            break;
        case HAL_U32:
            ret = fprintf(f, "%s %u\n", sig->name, sig->retain_val.u32);
            break;
        case HAL_S32:
            ret = fprintf(f, "%s %d\n", sig->name, sig->retain_val.s32);
            break;
        case HAL_FLOAT:
            ret = fprintf(f, "%s %.6f\n", sig->name, sig->retain_val.flt);
            break;
        default:
            ret = 0;
            break;
        }

        if (ret < 0) {
            rtapi_mutex_give(&(hd->mutex));
            fclose(f);
            return -1;
        }
    }
    rtapi_mutex_give(&(hd->mutex));

    if (fflush(f) || fdatasync(fileno(f)) || fclose(f)) {
        return -1;
    }

    if (rename(tmp_name, file_name) < 0) {
        return -1;
    }

    return 0;
}
*/
import "C"
import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sync/atomic"
	"time"
	"unsafe"

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
	compID  int
	state   *C.retain_state_t
	varFile string
	poll    time.Duration
	cancel  context.CancelFunc
	done    chan struct{} // closed when goroutine exits
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

	// Determine the variable file path.
	varFile := l.ini.Get("RETAIN", "VAR_FILE")
	if varFile == "" {
		varFile = "retain.var"
	}
	if !filepath.IsAbs(varFile) {
		varFile = filepath.Join(filepath.Dir(l.opts.IniFile), varFile)
	}

	// Determine the poll period.
	pollPeriod := retainDefaultPollPeriod
	if ps := l.ini.Get("RETAIN", "POLL_PERIOD"); ps != "" {
		if v := parseInt(ps); v > 0 {
			pollPeriod = v
		}
	}

	// Restore values if var file exists.
	if _, err := os.Stat(varFile); err == nil {
		cPath := C.CString(varFile)
		defer C.free(unsafe.Pointer(cPath))
		if C.retain_load_vars(cPath) != 0 {
			l.logger.Warn("retain: failed to load var file", "path", varFile)
		} else {
			l.logger.Info("retain: restored values from var file", "path", varFile)
		}
	}

	// Start the background goroutine.
	ctx, cancel := context.WithCancel(context.Background())
	ri := &retainInstance{
		compID:  compID,
		state:   state,
		varFile: varFile,
		poll:    time.Duration(pollPeriod) * time.Millisecond,
		cancel:  cancel,
		done:    make(chan struct{}),
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

// retainSave persists retain signal values to the var file.
func (l *Launcher) retainSave(ri *retainInstance) {
	cPath := C.CString(ri.varFile)
	defer C.free(unsafe.Pointer(cPath))
	if C.retain_save_vars(cPath) != 0 {
		l.logger.Warn("retain: failed to save var file", "path", ri.varFile)
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
