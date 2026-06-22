// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package launcher — gomc_env.go provides the Go-side log ring drain loop
// and INI callback implementations for the gomc C plugin environment.
//
// All C helper functions (pass-through HAL/RTAPI callbacks, env allocation,
// ring buffer read) live in the cgo preamble of cmodules.go.  This file
// contains only pure Go code.
package launcher

/*
#include "../../pkg/cmodule/gomc_log.h"
#include <string.h>

// gomc_sub_ring_write writes one message to a subscriber's ring.
// Returns 0 on success, -1 if the ring is full (message dropped).
static int gomc_sub_ring_write(gomc_log_ring_t *ring,
                               uint32_t level, int64_t ts,
                               const char *component, const char *msg) {
    uint32_t pos = __atomic_fetch_add(&ring->write_pos, 1, __ATOMIC_RELAXED);
    uint32_t idx = pos & GOMC_LOG_RING_MASK;
    gomc_log_slot_t *slot = &ring->slots[idx];

    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&slot->seq, &expected, 1, 0,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return -1;  // ring full
    }

    slot->level = level;
    slot->timestamp_ns = ts;
    memcpy(slot->component, component, GOMC_LOG_COMPONENT_LEN);
    memcpy(slot->msg, msg, GOMC_LOG_MSG_LEN);

    __atomic_store_n(&slot->seq, pos + 1, __ATOMIC_RELEASE);
    return 0;
}
*/
import "C"

import (
	"context"
	"log/slog"
	"runtime/cgo"
	"sync"
	"syscall"
	"time"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
)

// gomcLogRing wraps a C-allocated gomc_log_ring_t and provides the Go-side
// drain loop that forwards log entries to the slog logger.
type gomcLogRing struct {
	ring    *C.gomc_log_ring_t
	readPos uint32
	cancel  context.CancelFunc
	wg      sync.WaitGroup

	// Subscriber fan-out: drain loop copies messages to per-subscriber rings.
	subsMu sync.Mutex
	subs   []*C.gomc_log_sub_t
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

		// Fan-out to subscribers whose level filter matches.
		r.fanOut(level, ts, compBuf, msgBuf)

		component := cStringFromBytes(compBuf)
		msg := cStringFromBytes(msgBuf)
		tsNano := int64(ts)

		// Notify Go-level error hooks (e.g. milltask operator messages).
		if int(level) >= 3 { // GOMC_LOG_ERROR
			gomc.NotifyLogError(component, msg)
		}

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

		// Convert C-side wall clock timestamp to Go time.
		logTime := time.Unix(0, tsNano)
		if !logger.Handler().Enabled(context.Background(), logLevel) {
			continue
		}
		record := slog.NewRecord(logTime, logLevel, msg, 0)
		record.AddAttrs(slog.String("component", component))
		_ = logger.Handler().Handle(context.Background(), record)
	}
	return count
}

// fanOut copies a log message to all subscriber rings whose level filter matches.
func (r *gomcLogRing) fanOut(level C.uint32_t, ts C.int64_t, comp, msg []byte) {
	r.subsMu.Lock()
	defer r.subsMu.Unlock()

	for _, sub := range r.subs {
		if level < sub.min_level {
			continue
		}
		C.gomc_sub_ring_write(sub.ring,
			level, ts,
			(*C.char)(unsafe.Pointer(&comp[0])),
			(*C.char)(unsafe.Pointer(&msg[0])))
	}
}

// subscribe creates a new subscription with a per-subscriber ring buffer.
func (r *gomcLogRing) subscribe(minLevel C.gomc_log_level_t) *C.gomc_log_sub_t {
	sub := (*C.gomc_log_sub_t)(C.calloc(1, C.size_t(unsafe.Sizeof(C.gomc_log_sub_t{}))))
	if sub == nil {
		return nil
	}
	sub.ring = C.gomc_ring_create()
	if sub.ring == nil {
		C.free(unsafe.Pointer(sub))
		return nil
	}
	sub.read_pos = 0
	sub.min_level = C.uint32_t(minLevel)

	r.subsMu.Lock()
	r.subs = append(r.subs, sub)
	r.subsMu.Unlock()
	return sub
}

// unsubscribe removes a subscription and frees its resources.
func (r *gomcLogRing) unsubscribe(sub *C.gomc_log_sub_t) {
	r.subsMu.Lock()
	for i, s := range r.subs {
		if s == sub {
			r.subs = append(r.subs[:i], r.subs[i+1:]...)
			break
		}
	}
	r.subsMu.Unlock()

	if sub.ring != nil {
		C.gomc_ring_destroy(sub.ring)
	}
	C.free(unsafe.Pointer(sub))
}

// --- Log subscribe/unsubscribe callbacks (exported to C) ---

//export gomc_log_subscribe_cb
func gomc_log_subscribe_cb(ctx unsafe.Pointer, minLevel C.gomc_log_level_t) *C.gomc_log_sub_t {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	return l.logRing.subscribe(minLevel)
}

//export gomc_log_unsubscribe_cb
func gomc_log_unsubscribe_cb(ctx unsafe.Pointer, sub *C.gomc_log_sub_t) {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	l.logRing.unsubscribe(sub)
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

//export gomc_ini_get_all
func gomc_ini_get_all(ctx unsafe.Pointer, section, key *C.char, outCount *C.int) **C.char {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	vals := l.ini.GetAll(C.GoString(section), C.GoString(key))
	n := len(vals)
	*outCount = C.int(n)
	if n == 0 {
		return nil
	}

	// Arena-allocate the pointer array (n+1 entries, NULL-terminated)
	// and each string.  All freed in destroyCModules via cModArena.
	ptrSize := unsafe.Sizeof((*C.char)(nil))
	arr := (**C.char)(C.malloc(C.size_t(uintptr(n+1) * ptrSize)))
	l.cModArena = append(l.cModArena, unsafe.Pointer(arr))

	for i, v := range vals {
		cs := C.CString(v)
		l.cModArena = append(l.cModArena, unsafe.Pointer(cs))
		*(**C.char)(unsafe.Add(unsafe.Pointer(arr), uintptr(i)*ptrSize)) = cs
	}
	// NULL terminator
	*(**C.char)(unsafe.Add(unsafe.Pointer(arr), uintptr(n)*ptrSize)) = nil

	return arr
}

// --- API registry callback implementations (exported to C) ---

//export gomc_api_register_cb
func gomc_api_register_cb(ctx unsafe.Pointer, apiName *C.char, version C.int,
	instanceName *C.char, callbacks unsafe.Pointer) C.int {

	reg := apiserver.DefaultRegistry()
	if reg == nil {
		slog.Error("register_api: no default registry")
		return -C.int(syscall.EINVAL)
	}

	name := C.GoString(apiName)
	ver := int(version)
	instance := C.GoString(instanceName)

	err := reg.Register(name, ver, instance, callbacks)
	if err != nil {
		slog.Error("register_api: registration failed",
			"api", name, "instance", instance, "error", err)
		switch err {
		case syscall.EEXIST:
			return -C.int(syscall.EEXIST)
		case syscall.EINVAL:
			return -C.int(syscall.EINVAL)
		default:
			return -1
		}
	}

	// If a watch factory exists for this API, create and register the WatchAPI.
	if factory := apiserver.GetWatchFactory(name); factory != nil {
		watchReg := apiserver.DefaultWatchRegistry()
		if watchReg == nil {
			watchReg = apiserver.NewWatchRegistry()
			apiserver.SetDefaultWatchRegistry(watchReg)
		}
		watchReg.Register(factory(instance, callbacks))
	}

	// If a stream server factory exists, create and register the stream endpoint.
	if factory := apiserver.GetStreamFactory(name); factory != nil {
		if srv := apiserver.DefaultServer(); srv != nil {
			srv.RegisterStream(name, instance, factory(instance, callbacks))
		}
	}

	return 0
}

//export gomc_api_get_cb
func gomc_api_get_cb(ctx unsafe.Pointer, apiName *C.char, version C.int,
	instanceName *C.char) unsafe.Pointer {

	reg := apiserver.DefaultRegistry()
	if reg == nil {
		slog.Error("get_api: no default registry")
		return nil
	}

	name := C.GoString(apiName)
	instance := C.GoString(instanceName)
	ver := int(version)

	cbs, err := reg.GetAPIUntracked(name, instance, ver)
	if err != nil {
		slog.Error("get_api: lookup failed",
			"api", name, "instance", instance, "version", ver, "error", err)
		return nil
	}
	return cbs
}

//export gomc_record_consumer_cb
func gomc_record_consumer_cb(ctx unsafe.Pointer, consumerInstance *C.char,
	apiName *C.char, providerInstance *C.char) {

	reg := apiserver.DefaultRegistry()
	if reg == nil {
		return
	}
	reg.RecordConsumer(C.GoString(consumerInstance), C.GoString(apiName), C.GoString(providerInstance))
}

//export gomc_watch_push_cb
func gomc_watch_push_cb(ctx unsafe.Pointer, apiName *C.char, instanceName *C.char,
	funcName *C.char, data unsafe.Pointer, dataLen C.int) C.int {

	name := C.GoString(apiName)
	instance := C.GoString(instanceName)
	fn := C.GoString(funcName)

	pw := apiserver.GetOrCreatePushWatch(name, instance, fn)
	if pw == nil {
		return -C.int(syscall.EINVAL)
	}

	if err := pw.Push(data, int(dataLen)); err != nil {
		slog.Error("push_watch: conversion failed",
			"api", name, "instance", instance, "func", fn, "error", err)
		return -1
	}
	return 0
}
