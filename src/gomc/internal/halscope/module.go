// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package halscope implements the HAL oscilloscope as a gomod.
//
// The RT sample function runs in a HAL thread (C code via cgo).
// All non-RT logic — API handlers, watch loops, buffer read-out — lives
// in Go.  The halscope.gmi IDL generates only client code (TS/Python)
// and WS/REST routing types.
package halscope

/*
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include
#cgo LDFLAGS:

#include "halscope_rt.h"
#include "hal_priv.h"

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <fnmatch.h>
#include <errno.h>

// hal_export_funct wrapper — Go can't take address of C function directly.
static int go_hal_export_funct(const char *name, halscope_t *s,
                               int comp_id) {
    return hal_export_funct(name, halscope_sample, s, 1, 0, comp_id);
}

// Self dl_handle for RT component registration.
static void *self_dl_handle(void) {
    return dlopen(NULL, RTLD_NOW);
}

// --- SHMPTR wrappers (cgo cannot invoke C macros) ---

static hal_thread_t *shmptr_thread(rtapi_intptr_t off) {
    return (hal_thread_t *)SHMPTR(off);
}
static hal_pin_t *shmptr_pin(rtapi_intptr_t off) {
    return (hal_pin_t *)SHMPTR(off);
}
static hal_sig_t *shmptr_sig(rtapi_intptr_t off) {
    return (hal_sig_t *)SHMPTR(off);
}
static hal_param_t *shmptr_param(rtapi_intptr_t off) {
    return (hal_param_t *)SHMPTR(off);
}
static void *shmptr_void(rtapi_intptr_t off) {
    return SHMPTR(off);
}

// --- Accessor for hal_data (extern global, tricky from cgo) ---
static hal_data_t *get_hal_data(void) { return hal_data; }

// --- Union setter (cgo cannot access C union fields) ---
static void set_trigger_level(halscope_data_t *d, double v) {
    d->d_real = (real_t)v;
}

static void set_trigger_level_s32(halscope_data_t *d, int32_t v) {
    d->d_s32 = v;
}

static void set_trigger_level_u32(halscope_data_t *d, uint32_t v) {
    d->d_u32 = v;
}

static double get_trigger_level_real(halscope_data_t *d) {
    return (double)d->d_real;
}

*/
import "C"

import (
	"encoding/json"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"sync"
	"sync/atomic"
	"unsafe"

	halscopeapi "github.com/sittner/linuxcnc/src/gomc/generated/gmi/halscope"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("halscope", newHalscope)
	apiserver.RegisterMeta(halscopeapi.HalscopeMeta)
}

// halscope implements gomc.Module.
type halscope struct {
	logger    *slog.Logger
	s         *C.halscope_t // shared state — RT reads, Go writes
	compID    C.int
	mu        sync.Mutex // protects non-atomic config writes
	name      string     // HAL component name (from load command)
	functName string     // HAL function name: name + ".sample"
	statePath string     // path for persistent state file (empty = disabled)
}

func newHalscope(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	numSamples := C.int(C.HALSCOPE_DEFAULT_NUM_SAMPLES)
	// TODO: parse args for num_samples=N

	s := C.halscope_alloc(numSamples)
	if s == nil {
		return nil, fmt.Errorf("halscope: failed to allocate instance")
	}

	// Create HAL RT component.  We need the gomc-server binary's own
	// dl_handle so HAL can lock it during RT thread execution.
	dlHandle := C.self_dl_handle()
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	compID := C.hal_init_ex(cName, dlHandle, C.COMPONENT_TYPE_REALTIME)
	if compID < 0 {
		C.halscope_free(s)
		return nil, fmt.Errorf("halscope: hal_init_ex failed: %d", int(compID))
	}

	// Export the RT sample function to HAL.
	functName := name + ".sample"
	cFunctName := C.CString(functName)
	defer C.free(unsafe.Pointer(cFunctName))
	rv := C.go_hal_export_funct(cFunctName, s, compID)
	if rv != 0 {
		C.hal_exit(compID)
		C.halscope_free(s)
		return nil, fmt.Errorf("halscope: hal_export_funct failed: %d", int(rv))
	}

	C.hal_ready(compID)

	m := &halscope{
		logger:    logger,
		s:         s,
		compID:    compID,
		name:      name,
		functName: functName,
	}

	// Resolve state file path from INI: [HAL]SCOPE_STATE_STORAGE,
	// defaulting to <config_dir>/halscope_state.json.
	if ini != nil {
		sp := ini.Get("HAL", "SCOPE_STATE_STORAGE")
		if sp == "" {
			configDir := filepath.Dir(ini.SourceFile())
			sp = filepath.Join(configDir, "halscope_state.json")
		}
		m.statePath = sp
	}

	// Register REST API.
	reg := apiserver.DefaultRegistry()
	if reg != nil {
		m.registerREST(reg, name)
	}

	// Register WebSocket watch API.
	wreg := apiserver.DefaultWatchRegistry()
	if wreg == nil {
		apiserver.SetDefaultWatchRegistry(apiserver.NewWatchRegistry())
		wreg = apiserver.DefaultWatchRegistry()
	}
	if wreg != nil {
		m.registerWatch(wreg, name)
	}

	// Restore state from file if available.
	// NOTE: deferred to Start() — at module load time, not all HAL pins
	// may exist yet (e.g. motmod pins are loaded after plugin modules).

	logger.Info("halscope loaded", "name", name, "num_samples", int(numSamples), "comp_id", int(compID))
	return m, nil
}

func (m *halscope) Start() error {
	if m.statePath != "" {
		if err := m.loadState(); err != nil {
			m.logger.Warn("halscope: failed to load state", "path", m.statePath, "err", err)
		} else {
			m.logger.Info("halscope: restored state", "path", m.statePath)
		}
	}
	return nil
}

func (m *halscope) Stop() {
	if m.statePath != "" {
		if err := m.saveState(); err != nil {
			m.logger.Warn("halscope: failed to save state on stop", "path", m.statePath, "err", err)
		} else {
			m.logger.Info("halscope: saved state on stop", "path", m.statePath)
		}
	}
}

func (m *halscope) Destroy() {
	// hal_exit removes the component and all its functions/pins from HAL,
	// including any thread linkages.  No need to call hal_del_funct_from_thread
	// explicitly — it would access thread structures that may already be torn
	// down during the shutdown sequence.
	C.hal_exit(m.compID)
	C.halscope_free(m.s)
}

// ------------------------------------------------------------------ //
//                     REST API REGISTRATION                           //
// ------------------------------------------------------------------ //

func (m *halscope) registerREST(reg *apiserver.Registry, instance string) {
	if err := halscopeapi.RegisterHalscopeAPI(reg, instance, m); err != nil {
		m.logger.Error("halscope: register REST API failed", "err", err)
	}
}

func (m *halscope) registerWatch(wreg *apiserver.WatchRegistry, instance string) {
	halscopeapi.RegisterHalscopeWatch(wreg, instance, m, nil)
}

// ------------------------------------------------------------------ //
//                     HalscopeWatchCallbacks IMPLEMENTATION            //
// ------------------------------------------------------------------ //

// WatchState implements halscopeapi.HalscopeWatchCallbacks.
func (m *halscope) WatchState() (*halscopeapi.ScopeStatus, error) {
	st := m.getStatus()
	return &st, nil
}

// WatchSamples implements halscopeapi.HalscopeWatchCallbacks.
func (m *halscope) WatchSamples() ([]byte, uint64, error) {
	s := m.s

	db := int(C.halscope_atomic_load_int((*C.int)(unsafe.Pointer(&s.done_buf)), C.memory_order_acquire))
	if db < 0 || s.done_len == 0 {
		return nil, 0, nil
	}

	gen := uint64(C.halscope_atomic_load_uint((*C.uint)(unsafe.Pointer(&s.done_gen)), C.memory_order_acquire))

	// Borrow the done buffer — increment refcount so RT won't reuse it.
	C.halscope_atomic_fetch_add_int((*C.int)(unsafe.Pointer(&s.bufs[db].readers)), 1, C.memory_order_acquire)

	// Verify done_buf hasn't changed — guards against TOCTOU race where
	// RT completes a new capture between our load and refcount increment.
	db2 := int(C.halscope_atomic_load_int((*C.int)(unsafe.Pointer(&s.done_buf)), C.memory_order_acquire))
	if db2 != db {
		C.halscope_atomic_fetch_sub_int((*C.int)(unsafe.Pointer(&s.bufs[db].readers)), 1, C.memory_order_release)
		return nil, 0, nil
	}

	totalLen := int(s.done_len)
	hdrSize := int(C.halscope_get_header_size())
	dataBytes := totalLen - hdrSize
	ringStartBytes := int(s.done_ring_start) * 8 // sizeof(double)

	result := make([]byte, totalLen)
	src := s.bufs[db].data

	// Copy header.
	C.memcpy(unsafe.Pointer(&result[0]), unsafe.Pointer(src), C.size_t(hdrSize))

	// Copy + linearize data.
	srcData := unsafe.Add(unsafe.Pointer(src), hdrSize)
	dstData := unsafe.Add(unsafe.Pointer(&result[0]), hdrSize)

	if ringStartBytes == 0 || ringStartBytes >= dataBytes {
		C.memcpy(dstData, srcData, C.size_t(dataBytes))
	} else {
		part1 := dataBytes - ringStartBytes
		C.memcpy(dstData, unsafe.Add(srcData, ringStartBytes), C.size_t(part1))
		C.memcpy(unsafe.Add(dstData, part1), srcData, C.size_t(ringStartBytes))
	}

	// Release borrow.
	C.halscope_atomic_fetch_sub_int((*C.int)(unsafe.Pointer(&s.bufs[db].readers)), 1, C.memory_order_release)

	return result, gen, nil
}

// ------------------------------------------------------------------ //
//                     HalscopeCallbacks IMPLEMENTATION                 //
// ------------------------------------------------------------------ //

func (m *halscope) ListThreads() ([]halscopeapi.ThreadInfo, error) {
	var threads []halscopeapi.ThreadInfo
	next := C.get_hal_data().thread_list_ptr
	for next != nil {
		t := next
		threads = append(threads, halscopeapi.ThreadInfo{
			Name:     C.GoString(&t.name[0]),
			PeriodNs: int64(t.period),
		})
		next = t.next_ptr
	}
	return threads, nil
}

func (m *halscope) Configure(config halscopeapi.CaptureConfig) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	s := m.s
	state := C.halscope_atomic_load_state((*C.halscope_state_t)(unsafe.Pointer(&s.state)), C.memory_order_acquire)
	if state != C.HALSCOPE_ST_IDLE && state != C.HALSCOPE_ST_DONE {
		return -int32(C.EBUSY), nil
	}

	// Handle thread (re-)assignment.
	if config.ThreadName != "" {
		currentThread := C.GoString(&s.thread_name[0])
		if currentThread != "" && currentThread != config.ThreadName {
			ct := C.CString(currentThread)
			cf := C.CString(m.functName)
			C.hal_del_funct_from_thread(cf, ct)
			C.free(unsafe.Pointer(ct))
			C.free(unsafe.Pointer(cf))
			s.thread_name[0] = 0
		}
		if C.GoString(&s.thread_name[0]) != config.ThreadName {
			cf := C.CString(m.functName)
			ct := C.CString(config.ThreadName)
			rv := C.hal_add_funct_to_thread(cf, ct, -1)
			C.free(unsafe.Pointer(cf))
			C.free(unsafe.Pointer(ct))
			if rv != 0 {
				return int32(rv), nil
			}
			cName := C.CString(config.ThreadName)
			C.strncpy(&s.thread_name[0], cName, C.size_t(C.HAL_NAME_LEN))
			C.free(unsafe.Pointer(cName))
		}
	}

	// Set max_channels and derive rec_len from buffer size.
	if config.MaxChannels > 0 {
		mc := int(config.MaxChannels)
		// Snap to valid values: 1, 2, 4, 8, 16
		if mc > 16 {
			mc = 16
		} else if mc > 8 {
			mc = 16
		} else if mc > 4 {
			mc = 8
		} else if mc > 2 {
			mc = 4
		} else if mc > 1 {
			mc = 2
		}
		s.max_channels = C.int(mc)
		s.rec_len = s.num_samples / s.max_channels
	}

	if config.SamplePeriodMult > 0 {
		s.mult = C.int(config.SamplePeriodMult)
	}
	// Always center trigger at midpoint of buffer (matches original halscope)
	s.pre_trig = s.rec_len / 2

	go m.saveState()
	return 0, nil
}

func (m *halscope) SetChannel(ch halscopeapi.ChannelConfig) (int32, error) {
	if ch.Channel < 0 || ch.Channel >= C.HALSCOPE_MAX_CHANNELS {
		return -int32(C.EINVAL), nil
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	s := m.s

	// Enforce max_channels limit — channel index must be < max_channels.
	if ch.Channel >= int32(s.max_channels) {
		return -int32(C.EINVAL), nil
	}

	// Resolve HAL name.
	var halType C.hal_type_t
	var dataLen C.int
	var dataAddr unsafe.Pointer
	cName := C.CString(ch.PinName)
	defer C.free(unsafe.Pointer(cName))

	rv := m.resolveHALName(cName, &halType, &dataLen, &dataAddr)
	if rv != 0 {
		return int32(rv), nil
	}

	c := &s.channels[ch.Channel]
	c.enabled = 1
	cPN := C.CString(ch.PinName)
	C.strncpy(&c.pin_name[0], cPN, C.size_t(C.HAL_NAME_LEN))
	C.free(unsafe.Pointer(cPN))
	c.data_type = halType
	c.data_len = dataLen
	c.data_addr = dataAddr

	if s.trig.channel < 0 {
		s.trig.channel = C.int(ch.Channel)
	}

	go m.saveState()
	return 0, nil
}

func (m *halscope) ClearChannel(channel int32) (int32, error) {
	if channel < 0 || channel >= C.HALSCOPE_MAX_CHANNELS {
		return -int32(C.EINVAL), nil
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	C.memset(unsafe.Pointer(&m.s.channels[channel]), 0,
		C.size_t(unsafe.Sizeof(m.s.channels[0])))

	go m.saveState()
	return 0, nil
}

func (m *halscope) SetTrigger(trig halscopeapi.TriggerConfig) (int32, error) {
	if trig.Channel < -1 || trig.Channel >= C.HALSCOPE_MAX_CHANNELS {
		return -int32(C.EINVAL), nil
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	s := m.s
	s.trig.channel = C.int(trig.Channel)

	// Store level in the correct union member for the trigger channel's type.
	if trig.Channel >= 0 && trig.Channel < C.HALSCOPE_MAX_CHANNELS {
		switch s.channels[trig.Channel].data_type {
		case C.HAL_S32:
			C.set_trigger_level_s32(&s.trig.level, C.int32_t(trig.Level))
		case C.HAL_U32:
			C.set_trigger_level_u32(&s.trig.level, C.uint32_t(trig.Level))
		default:
			C.set_trigger_level(&s.trig.level, C.double(trig.Level))
		}
	} else {
		C.set_trigger_level(&s.trig.level, C.double(trig.Level))
	}

	if trig.Edge == halscopeapi.TrigEdge_RISING {
		s.trig.edge = 1
	} else {
		s.trig.edge = 0
	}
	if trig.AutoTrig {
		s.trig.auto_trig = 1
	} else {
		s.trig.auto_trig = 0
	}

	go m.saveState()
	return 0, nil
}

func (m *halscope) Arm() (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	s := m.s
	state := C.halscope_atomic_load_state((*C.halscope_state_t)(unsafe.Pointer(&s.state)), C.memory_order_acquire)
	if state != C.HALSCOPE_ST_IDLE && state != C.HALSCOPE_ST_DONE {
		return -int32(C.EBUSY), nil
	}
	if s.max_channels == 0 || s.rec_len == 0 {
		return -int32(C.EINVAL), nil
	}
	if s.thread_name[0] == 0 {
		return -int32(C.EINVAL), nil
	}

	C.halscope_atomic_store_state((*C.halscope_state_t)(unsafe.Pointer(&s.state)), C.HALSCOPE_ST_INIT, C.memory_order_release)
	return 0, nil
}

func (m *halscope) ForceTrigger() (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	state := C.halscope_atomic_load_state((*C.halscope_state_t)(unsafe.Pointer(&m.s.state)), C.memory_order_acquire)
	if state != C.HALSCOPE_ST_PRE_TRIG && state != C.HALSCOPE_ST_TRIG_WAIT {
		return -int32(C.EINVAL), nil
	}
	m.s.trig.force = 1
	return 0, nil
}

func (m *halscope) SetContinuous(enabled bool) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if enabled {
		C.halscope_atomic_store_int((*C.int)(unsafe.Pointer(&m.s.continuous)), 1, C.memory_order_release)
	} else {
		C.halscope_atomic_store_int((*C.int)(unsafe.Pointer(&m.s.continuous)), 0, C.memory_order_release)
	}

	go m.saveState()
	return 0, nil
}

func (m *halscope) Reset() (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	C.halscope_atomic_store_int((*C.int)(unsafe.Pointer(&m.s.continuous)), 0, C.memory_order_release)
	C.halscope_atomic_store_state((*C.halscope_state_t)(unsafe.Pointer(&m.s.state)), C.HALSCOPE_ST_RESET, C.memory_order_release)
	return 0, nil
}

func (m *halscope) GetStatus() (*halscopeapi.ScopeStatus, error) {
	st := m.getStatus()
	return &st, nil
}

func (m *halscope) ListPins(pattern string, kind string) ([]string, error) {
	match := pattern
	if match == "" {
		match = "*"
	}
	cMatch := C.CString(match)
	defer C.free(unsafe.Pointer(cMatch))

	wantPins := kind == "" || kind == "pin"
	wantSigs := kind == "" || kind == "sig"
	wantParams := kind == "" || kind == "param"

	names := make([]string, 0)

	C.rtapi_mutex_get(&C.get_hal_data().mutex)

	if wantPins {
		next := C.get_hal_data().pin_list_ptr
		for next != nil {
			pin := next
			if C.fnmatch(cMatch, &pin.name[0], 0) == 0 {
				names = append(names, C.GoString(&pin.name[0]))
			}
			next = pin.next_ptr
		}
	}
	if wantSigs {
		next := C.get_hal_data().sig_list_ptr
		for next != nil {
			sig := next
			if C.fnmatch(cMatch, &sig.name[0], 0) == 0 {
				names = append(names, C.GoString(&sig.name[0]))
			}
			next = sig.next_ptr
		}
	}
	if wantParams {
		next := C.get_hal_data().param_list_ptr
		for next != nil {
			param := next
			if C.fnmatch(cMatch, &param.name[0], 0) == 0 {
				names = append(names, C.GoString(&param.name[0]))
			}
			next = param.next_ptr
		}
	}

	C.rtapi_mutex_give(&C.get_hal_data().mutex)

	return names, nil
}

// ------------------------------------------------------------------ //
//                     HELPERS                                         //
// ------------------------------------------------------------------ //

// halscope_state_t alias for readability in Go.
type halscope_state_t = C.halscope_state_t

func (m *halscope) getStatus() halscopeapi.ScopeStatus {
	s := m.s
	numSamples := int32(s.num_samples)
	st := halscopeapi.ScopeStatus{
		State:            halscopeapi.ScopeState(C.halscope_atomic_load_state((*C.halscope_state_t)(unsafe.Pointer(&s.state)), C.memory_order_acquire)),
		Samples:          int32(s.samples),
		RecLen:           int32(s.rec_len),
		PreTrig:          int32(s.pre_trig),
		SampleLen:        int32(s.sample_len),
		MaxChannels:      int32(s.max_channels),
		SamplePeriodMult: int32(s.mult),
		TrigChannel:      int32(s.trig.channel),
		TrigLevel:        float64(C.get_trigger_level_real(&s.trig.level)),
		TrigEdge:         halscopeapi.TrigEdge(s.trig.edge),
		TrigAutoTrig:     s.trig.auto_trig != 0,
		Generation:       uint32(atomic.LoadUint32((*uint32)(unsafe.Pointer(&s.done_gen)))),
		Continuous:       C.halscope_atomic_load_int((*C.int)(unsafe.Pointer(&s.continuous)), C.memory_order_acquire) != 0,
		ChannelOptions: []halscopeapi.ChannelOption{
			{MaxChannels: 1, RecLen: numSamples / 1},
			{MaxChannels: 2, RecLen: numSamples / 2},
			{MaxChannels: 4, RecLen: numSamples / 4},
			{MaxChannels: 8, RecLen: numSamples / 8},
			{MaxChannels: 16, RecLen: numSamples / 16},
		},
	}

	threadName := C.GoString(&s.thread_name[0])
	st.ThreadName = threadName

	// Look up thread period.
	if threadName != "" {
		next := C.get_hal_data().thread_list_ptr
		for next != nil {
			t := next
			if C.GoString(&t.name[0]) == threadName {
				st.ThreadPeriodNs = int64(t.period)
				break
			}
			next = t.next_ptr
		}
	}

	// Build channel list.
	for n := 0; n < C.HALSCOPE_MAX_CHANNELS; n++ {
		if s.channels[n].enabled != 0 {
			st.Channels = append(st.Channels, halscopeapi.ChannelInfo{
				Channel:  int32(n),
				PinName:  C.GoString(&s.channels[n].pin_name[0]),
				DataType: halscopeapi.HalType(s.channels[n].data_type),
				Enabled:  true,
			})
		}
	}
	if st.Channels == nil {
		st.Channels = []halscopeapi.ChannelInfo{}
	}

	return st
}

func (m *halscope) resolveHALName(cName *C.char, halType *C.hal_type_t, dataLen *C.int, dataAddr *unsafe.Pointer) int {
	C.rtapi_mutex_get(&C.get_hal_data().mutex)
	defer C.rtapi_mutex_give(&C.get_hal_data().mutex)

	// Try pin.
	pin := C.halpr_find_pin_by_name(cName)
	if pin != nil {
		*halType = pin._type
		if pin.signal != nil {
			sig := pin.signal
			*dataAddr = unsafe.Pointer(sig.data_ptr)
		} else {
			*dataAddr = unsafe.Pointer(&pin.dummysig)
		}
		m.setDataLen(*halType, dataLen)
		return 0
	}

	// Try signal.
	sig := C.halpr_find_sig_by_name(cName)
	if sig != nil {
		*halType = sig._type
		*dataAddr = unsafe.Pointer(sig.data_ptr)
		m.setDataLen(*halType, dataLen)
		return 0
	}

	// Try parameter.
	param := C.halpr_find_param_by_name(cName)
	if param != nil {
		*halType = param._type
		*dataAddr = unsafe.Pointer(param.data_ptr)
		m.setDataLen(*halType, dataLen)
		return 0
	}

	return -int(C.ENOENT)
}

func (m *halscope) setDataLen(t C.hal_type_t, dataLen *C.int) {
	switch t {
	case C.HAL_BIT:
		*dataLen = 1
	case C.HAL_S32, C.HAL_U32:
		*dataLen = 4
	case C.HAL_FLOAT:
		*dataLen = 8
	default:
		*dataLen = 0
	}
}

// ------------------------------------------------------------------ //
//                   STATE PERSISTENCE                                 //
// ------------------------------------------------------------------ //

// scopeStateFile is the JSON format for the persistent state file.
type scopeStateFile struct {
	Version  int            `json:"version"`
	Config   stateConfig    `json:"config"`
	Channels []stateChannel `json:"channels"`
	Trigger  stateTrigger   `json:"trigger"`
}

type stateConfig struct {
	ThreadName       string `json:"threadName"`
	MaxChannels      int    `json:"maxChannels"`
	SamplePeriodMult int    `json:"samplePeriodMult"`
	Continuous       bool   `json:"continuous"`
}

type stateChannel struct {
	Channel  int    `json:"channel"`
	PinName  string `json:"pinName"`
	DataType int    `json:"dataType"`
}

type stateTrigger struct {
	Channel  int     `json:"channel"`
	Level    float64 `json:"level"`
	Edge     int     `json:"edge"`
	AutoTrig bool    `json:"autoTrig"`
}

// saveState writes the current scope configuration to the state file.
// Caller must NOT hold m.mu.
func (m *halscope) saveState() error {
	if m.statePath == "" {
		return nil
	}

	m.mu.Lock()
	s := m.s

	sf := scopeStateFile{
		Version: 1,
		Config: stateConfig{
			ThreadName:       C.GoString(&s.thread_name[0]),
			MaxChannels:      int(s.max_channels),
			SamplePeriodMult: int(s.mult),
			Continuous:       C.halscope_atomic_load_int((*C.int)(unsafe.Pointer(&s.continuous)), C.memory_order_acquire) != 0,
		},
		Trigger: stateTrigger{
			Channel:  int(s.trig.channel),
			Level:    float64(C.get_trigger_level_real(&s.trig.level)),
			Edge:     int(s.trig.edge),
			AutoTrig: s.trig.auto_trig != 0,
		},
	}

	for i := 0; i < C.HALSCOPE_MAX_CHANNELS; i++ {
		if s.channels[i].enabled != 0 {
			sf.Channels = append(sf.Channels, stateChannel{
				Channel:  i,
				PinName:  C.GoString(&s.channels[i].pin_name[0]),
				DataType: int(s.channels[i].data_type),
			})
		}
	}
	m.mu.Unlock()

	if sf.Channels == nil {
		sf.Channels = []stateChannel{}
	}

	data, err := json.MarshalIndent(sf, "", "  ")
	if err != nil {
		return fmt.Errorf("marshal: %w", err)
	}

	// Atomic write: tmp file + rename
	tmp := m.statePath + ".tmp"
	if err := os.WriteFile(tmp, data, 0644); err != nil {
		return fmt.Errorf("write tmp: %w", err)
	}
	if err := os.Rename(tmp, m.statePath); err != nil {
		os.Remove(tmp)
		return fmt.Errorf("rename: %w", err)
	}
	return nil
}

// loadState restores scope configuration from the state file.
// Channels whose HAL pins no longer exist or whose data type changed
// are silently skipped.
// Must be called before any captures start (during init).
func (m *halscope) loadState() error {
	if m.statePath == "" {
		return nil
	}

	data, err := os.ReadFile(m.statePath)
	if err != nil {
		if os.IsNotExist(err) {
			return nil // no state file yet — not an error
		}
		return fmt.Errorf("read: %w", err)
	}

	var sf scopeStateFile
	if err := json.Unmarshal(data, &sf); err != nil {
		return fmt.Errorf("unmarshal: %w", err)
	}
	if sf.Version != 1 {
		return fmt.Errorf("unsupported state file version %d", sf.Version)
	}

	s := m.s

	// Restore thread assignment.
	if sf.Config.ThreadName != "" {
		cf := C.CString(m.functName)
		ct := C.CString(sf.Config.ThreadName)
		rv := C.hal_add_funct_to_thread(cf, ct, -1)
		C.free(unsafe.Pointer(cf))
		C.free(unsafe.Pointer(ct))
		if rv != 0 {
			m.logger.Warn("halscope: state restore: thread not available",
				"thread", sf.Config.ThreadName, "rc", int(rv))
		} else {
			cName := C.CString(sf.Config.ThreadName)
			C.strncpy(&s.thread_name[0], cName, C.size_t(C.HAL_NAME_LEN))
			C.free(unsafe.Pointer(cName))
		}
	}

	// Restore max_channels and derive rec_len.
	if sf.Config.MaxChannels > 0 {
		mc := sf.Config.MaxChannels
		if mc > 16 {
			mc = 16
		}
		s.max_channels = C.int(mc)
		s.rec_len = s.num_samples / s.max_channels
		s.pre_trig = s.rec_len / 2
	}

	if sf.Config.SamplePeriodMult > 0 {
		s.mult = C.int(sf.Config.SamplePeriodMult)
	}

	if sf.Config.Continuous {
		C.halscope_atomic_store_int((*C.int)(unsafe.Pointer(&s.continuous)), 1, C.memory_order_release)
	}

	// Restore channels — validate each pin still exists with same type.
	restoredCount := 0
	for _, ch := range sf.Channels {
		if ch.Channel < 0 || ch.Channel >= C.HALSCOPE_MAX_CHANNELS {
			m.logger.Warn("halscope: state restore: channel index out of range",
				"channel", ch.Channel, "pin", ch.PinName)
			continue
		}
		if ch.Channel >= int(s.max_channels) {
			m.logger.Warn("halscope: state restore: channel >= max_channels",
				"channel", ch.Channel, "max_channels", int(s.max_channels), "pin", ch.PinName)
			continue
		}

		cName := C.CString(ch.PinName)
		var halType C.hal_type_t
		var dataLen C.int
		var dataAddr unsafe.Pointer

		rv := m.resolveHALName(cName, &halType, &dataLen, &dataAddr)
		C.free(unsafe.Pointer(cName))

		if rv != 0 {
			m.logger.Warn("halscope: state restore: pin not found, skipping",
				"channel", ch.Channel, "pin", ch.PinName)
			continue
		}
		if int(halType) != ch.DataType {
			m.logger.Warn("halscope: state restore: pin type changed, skipping",
				"channel", ch.Channel, "pin", ch.PinName,
				"expected", ch.DataType, "got", int(halType))
			continue
		}

		c := &s.channels[ch.Channel]
		c.enabled = 1
		cPN := C.CString(ch.PinName)
		C.strncpy(&c.pin_name[0], cPN, C.size_t(C.HAL_NAME_LEN))
		C.free(unsafe.Pointer(cPN))
		c.data_type = halType
		c.data_len = dataLen
		c.data_addr = dataAddr
		restoredCount++
	}

	// Restore trigger — only if the trigger channel was successfully restored.
	if sf.Trigger.Channel >= 0 && sf.Trigger.Channel < C.HALSCOPE_MAX_CHANNELS &&
		s.channels[sf.Trigger.Channel].enabled != 0 {
		s.trig.channel = C.int(sf.Trigger.Channel)
		// Use type-specific setter matching the trigger channel's data type.
		switch s.channels[sf.Trigger.Channel].data_type {
		case C.HAL_S32:
			C.set_trigger_level_s32(&s.trig.level, C.int32_t(sf.Trigger.Level))
		case C.HAL_U32:
			C.set_trigger_level_u32(&s.trig.level, C.uint32_t(sf.Trigger.Level))
		default:
			C.set_trigger_level(&s.trig.level, C.double(sf.Trigger.Level))
		}
		if sf.Trigger.Edge == 1 {
			s.trig.edge = 1
		} else {
			s.trig.edge = 0
		}
		if sf.Trigger.AutoTrig {
			s.trig.auto_trig = 1
		} else {
			s.trig.auto_trig = 0
		}
	} else {
		s.trig.channel = -1
	}

	m.logger.Info("halscope: state restored",
		"channels", restoredCount, "thread", sf.Config.ThreadName)
	return nil
}
