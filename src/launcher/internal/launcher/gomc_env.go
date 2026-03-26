// Package launcher — gomc_env.go provides the Go-side log ring drain loop
// and INI callback implementations for the gomc C plugin environment.
//
// All C helper functions (pass-through HAL/RTAPI callbacks, env allocation,
// ring buffer read) live in the cgo preamble of cmodules.go.  This file
// contains only pure Go code.
package launcher

/*
#include "../../pkg/cmodule/gomc_log.h"
*/
import "C"

import (
	"context"
	"log/slog"
	"runtime/cgo"
	"sync"
	"time"
	"unsafe"
)

// gomcLogRing wraps a C-allocated gomc_log_ring_t and provides the Go-side
// drain loop that forwards log entries to the slog logger.
type gomcLogRing struct {
	ring    *C.gomc_log_ring_t
	readPos uint32
	cancel  context.CancelFunc
	wg      sync.WaitGroup
}

// newGomcLogRing allocates and returns a new log ring.
func newGomcLogRing() *gomcLogRing {
	return &gomcLogRing{
		ring: C.gomc_ring_create(),
	}
}

// startDrain starts a goroutine that continuously drains log entries from the
// ring buffer and forwards them to the given slog.Logger.
func (r *gomcLogRing) startDrain(logger *slog.Logger) {
	ctx, cancel := context.WithCancel(context.Background())
	r.cancel = cancel
	r.wg.Add(1)
	go r.drainLoop(ctx, logger)
}

// stopDrain signals the drain goroutine to stop and waits for it to finish.
// Performs a final drain to flush any remaining messages.
func (r *gomcLogRing) stopDrain(logger *slog.Logger) {
	if r.cancel != nil {
		r.cancel()
		r.wg.Wait()
	}
	// Final flush — drain any messages that arrived after cancel.
	r.drainAll(logger)
}

// destroy frees the C-allocated ring buffer.
func (r *gomcLogRing) destroy() {
	if r.ring != nil {
		C.gomc_ring_destroy(r.ring)
		r.ring = nil
	}
}

func (r *gomcLogRing) drainLoop(ctx context.Context, logger *slog.Logger) {
	defer r.wg.Done()
	for {
		n := r.drainAll(logger)
		if n == 0 {
			// No messages — check if we should exit.
			select {
			case <-ctx.Done():
				return
			default:
				// Brief sleep to avoid busy-spinning.  1ms is fast enough
				// for log display but gentle on CPU.
				time.Sleep(1 * time.Millisecond)
			}
		}
	}
}

func (r *gomcLogRing) drainAll(logger *slog.Logger) int {
	var (
		level C.uint32_t
		ts    C.int64_t
	)
	compBuf := make([]byte, C.GOMC_LOG_COMPONENT_LEN)
	msgBuf := make([]byte, C.GOMC_LOG_MSG_LEN)

	count := 0
	for {
		ok := C.gomc_ring_try_read(
			r.ring, C.uint32_t(r.readPos),
			&level, &ts,
			(*C.char)(unsafe.Pointer(&compBuf[0])),
			(*C.char)(unsafe.Pointer(&msgBuf[0])),
		)
		if ok == 0 {
			break
		}
		r.readPos++
		count++

		component := cStringFromBytes(compBuf)
		msg := cStringFromBytes(msgBuf)
		tsNano := int64(ts)

		logLevel := slog.LevelInfo
		switch int(level) {
		case 0: // GOMC_LOG_DEBUG
			logLevel = slog.LevelDebug
		case 1: // GOMC_LOG_INFO
			logLevel = slog.LevelInfo
		case 2: // GOMC_LOG_WARN
			logLevel = slog.LevelWarn
		case 3: // GOMC_LOG_ERROR
			logLevel = slog.LevelError
		}

		// Use the C-side monotonic timestamp for the log record.
		logTime := time.Unix(0, tsNano)
		record := slog.NewRecord(logTime, logLevel, msg, 0)
		record.AddAttrs(slog.String("component", component))
		_ = logger.Handler().Handle(context.Background(), record)
	}
	return count
}

// cStringFromBytes extracts a C string from a byte slice (up to first NUL).
func cStringFromBytes(b []byte) string {
	for i, c := range b {
		if c == 0 {
			return string(b[:i])
		}
	}
	return string(b)
}

// --- INI callback implementations (exported to C) ---

//export gomc_ini_get
func gomc_ini_get(ctx unsafe.Pointer, section, key *C.char) *C.char {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	val := l.ini.Get(C.GoString(section), C.GoString(key))
	if val == "" {
		return nil
	}
	cs := C.CString(val)
	l.cModArena = append(l.cModArena, unsafe.Pointer(cs))
	return cs
}

//export gomc_ini_source_file
func gomc_ini_source_file(ctx unsafe.Pointer) *C.char {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	cs := C.CString(l.ini.SourceFile())
	l.cModArena = append(l.cModArena, unsafe.Pointer(cs))
	return cs
}
