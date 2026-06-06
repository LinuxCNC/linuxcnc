// Package classicladder implements the Classic Ladder PLC as a gomod.
//
// The RT refresh function runs in a HAL thread (C code via cgo).
// All non-RT logic — file I/O, API handlers, watch loops — lives in Go.
package classicladder

/*
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include
#cgo LDFLAGS:

#include "classicladder_rt.h"

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

// hal_export_funct wrapper — Go can't take address of C function directly.
static int go_hal_export_funct(const char *name, classicladder_rt_t *rt,
                               int comp_id) {
    return hal_export_funct(name, classicladder_refresh, rt, 1, 0, comp_id);
}

// Self dl_handle for RT component registration.
static void *self_dl_handle(void) {
    return dlopen(NULL, RTLD_NOW);
}
*/
import "C"

import (
	"fmt"
	"log/slog"
	"strings"
	"sync"
	"sync/atomic"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("classicladder", newClassicLadder)
}

// classicladder implements gomc.Module.
type classicladder struct {
	logger      *slog.Logger
	rt          *C.classicladder_rt_t
	compID      C.int
	mu          sync.RWMutex // protects program data modifications
	name        string
	functName   string
	projectFile string
	modbus      *modbusMaster
	modbusSlave *modbusSlave
	slavePort   int
}

func newClassicLadder(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	// Default sizes
	sizes := C.cl_sizes_t{
		nbr_rungs:        C.int(C.CL_MAX_RUNGS),
		nbr_bits:         100,
		nbr_words:        100,
		nbr_timers:       C.int(C.CL_MAX_TIMERS),
		nbr_monostables:  C.int(C.CL_MAX_MONOSTABLES),
		nbr_counters:     C.int(C.CL_MAX_COUNTERS),
		nbr_timers_iec:   C.int(C.CL_MAX_TIMERS_IEC),
		nbr_phys_inputs:  15,
		nbr_phys_outputs: 15,
		nbr_arithm_expr:  C.int(C.CL_MAX_ARITHM_EXPR),
		nbr_sections:     C.int(C.CL_MAX_SECTIONS),
		nbr_symbols:      C.int(C.CL_MAX_SYMBOLS),
		nbr_s32_in:       10,
		nbr_s32_out:      10,
		nbr_float_in:     10,
		nbr_float_out:    10,
		nbr_error_bits:   10,
	}

	// Parse args for size overrides (e.g. numRungs=200 numBits=500)
	// and project file path (last positional arg or modbus_port=N)
	var projectFile string
	var slavePort int
	for _, arg := range args {
		if !strings.Contains(arg, "=") {
			// Positional arg = project file path
			projectFile = arg
			continue
		}
		parts := strings.SplitN(arg, "=", 2)
		key, val := parts[0], parts[1]
		v := C.int(atoi(val))
		switch key {
		case "numRungs":
			sizes.nbr_rungs = v
		case "numBits":
			sizes.nbr_bits = v
		case "numWords":
			sizes.nbr_words = v
		case "numTimers":
			sizes.nbr_timers = v
		case "numMonostables":
			sizes.nbr_monostables = v
		case "numCounters":
			sizes.nbr_counters = v
		case "numTimersIec":
			sizes.nbr_timers_iec = v
		case "numPhysInputs":
			sizes.nbr_phys_inputs = v
		case "numPhysOutputs":
			sizes.nbr_phys_outputs = v
		case "numArithmExpr":
			sizes.nbr_arithm_expr = v
		case "numSections":
			sizes.nbr_sections = v
		case "numSymbols":
			sizes.nbr_symbols = v
		case "numS32in":
			sizes.nbr_s32_in = v
		case "numS32out":
			sizes.nbr_s32_out = v
		case "numFloatIn":
			sizes.nbr_float_in = v
		case "numFloatOut":
			sizes.nbr_float_out = v
		case "modbus_port":
			slavePort = atoi(val)
		}
	}

	rt := C.classicladder_rt_alloc(&sizes)
	if rt == nil {
		return nil, fmt.Errorf("classicladder: failed to allocate RT instance")
	}
	C.classicladder_rt_init_data(rt)

	// Create HAL RT component
	dlHandle := C.self_dl_handle()
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	compID := C.hal_init_ex(cName, dlHandle, C.COMPONENT_TYPE_REALTIME)
	if compID < 0 {
		C.classicladder_rt_free(rt)
		return nil, fmt.Errorf("classicladder: hal_init_ex failed: %d", int(compID))
	}

	// Export the RT refresh function to HAL
	functName := name + ".refresh"
	cFunctName := C.CString(functName)
	defer C.free(unsafe.Pointer(cFunctName))
	rv := C.go_hal_export_funct(cFunctName, rt, compID)
	if rv != 0 {
		C.hal_exit(compID)
		C.classicladder_rt_free(rt)
		return nil, fmt.Errorf("classicladder: hal_export_funct failed: %d", int(rv))
	}

	// Create HAL pins
	if err := createHALPins(rt, compID, name); err != nil {
		C.hal_exit(compID)
		C.classicladder_rt_free(rt)
		return nil, err
	}

	C.hal_ready(compID)

	m := &classicladder{
		logger:      logger,
		rt:          rt,
		compID:      compID,
		name:        name,
		functName:   functName,
		projectFile: projectFile,
		modbus:      newModbusMaster(rt, logger),
		modbusSlave: newModbusSlave(rt, logger),
		slavePort:   slavePort,
	}

	// Register REST API
	reg := apiserver.DefaultRegistry()
	if reg != nil {
		m.registerAPI(reg, name)
	}

	// Register WebSocket watch API
	wreg := apiserver.DefaultWatchRegistry()
	if wreg == nil {
		apiserver.SetDefaultWatchRegistry(apiserver.NewWatchRegistry())
		wreg = apiserver.DefaultWatchRegistry()
	}
	if wreg != nil {
		m.registerWatch(wreg, name)
	}

	logger.Info("classicladder loaded", "name", name, "comp_id", int(compID))
	return m, nil
}

func (m *classicladder) Start() error {
	// Load project file if specified as module argument
	if m.projectFile != "" {
		if err := m.loadCLPFile(m.projectFile); err != nil {
			m.logger.Error("failed to load project", "path", m.projectFile, "err", err)
			return err
		}
		m.setState(C.CL_STATE_RUN)
	}
	// Start Modbus master if configured
	m.modbus.start()
	// Start Modbus slave if configured
	if m.slavePort > 0 {
		m.modbusSlave.start(m.slavePort)
	}
	return nil
}

func (m *classicladder) Stop() {
	m.modbus.stop()
	m.modbusSlave.stop()
	C.hal_exit(m.compID)
	C.classicladder_rt_free(m.rt)
}

func (m *classicladder) Destroy() {}

// --- HAL pin creation ---

func createHALPins(rt *C.classicladder_rt_t, compID C.int, name string) error {
	// Bit inputs
	for i := C.int(0); i < rt.sizes.nbr_phys_inputs; i++ {
		pinName := C.CString(fmt.Sprintf("%s.0.in-%02d", name, int(i)))
		rv := C.hal_pin_bit_new(pinName, C.HAL_IN, &rt.hal_inputs[i], compID)
		C.free(unsafe.Pointer(pinName))
		if rv != 0 {
			return fmt.Errorf("failed to create pin in-%02d: %d", int(i), int(rv))
		}
	}

	// Bit outputs
	for i := C.int(0); i < rt.sizes.nbr_phys_outputs; i++ {
		pinName := C.CString(fmt.Sprintf("%s.0.out-%02d", name, int(i)))
		rv := C.hal_pin_bit_new(pinName, C.HAL_OUT, &rt.hal_outputs[i], compID)
		C.free(unsafe.Pointer(pinName))
		if rv != 0 {
			return fmt.Errorf("failed to create pin out-%02d: %d", int(i), int(rv))
		}
	}

	// S32 inputs
	for i := C.int(0); i < rt.sizes.nbr_s32_in; i++ {
		pinName := C.CString(fmt.Sprintf("%s.0.s32in-%02d", name, int(i)))
		rv := C.hal_pin_s32_new(pinName, C.HAL_IN, &rt.hal_s32_inputs[i], compID)
		C.free(unsafe.Pointer(pinName))
		if rv != 0 {
			return fmt.Errorf("failed to create pin s32in-%02d: %d", int(i), int(rv))
		}
	}

	// S32 outputs
	for i := C.int(0); i < rt.sizes.nbr_s32_out; i++ {
		pinName := C.CString(fmt.Sprintf("%s.0.s32out-%02d", name, int(i)))
		rv := C.hal_pin_s32_new(pinName, C.HAL_OUT, &rt.hal_s32_outputs[i], compID)
		C.free(unsafe.Pointer(pinName))
		if rv != 0 {
			return fmt.Errorf("failed to create pin s32out-%02d: %d", int(i), int(rv))
		}
	}

	// Float inputs
	for i := C.int(0); i < rt.sizes.nbr_float_in; i++ {
		pinName := C.CString(fmt.Sprintf("%s.0.floatin-%02d", name, int(i)))
		rv := C.hal_pin_float_new(pinName, C.HAL_IN, &rt.hal_float_inputs[i], compID)
		C.free(unsafe.Pointer(pinName))
		if rv != 0 {
			return fmt.Errorf("failed to create pin floatin-%02d: %d", int(i), int(rv))
		}
	}

	// Float outputs
	for i := C.int(0); i < rt.sizes.nbr_float_out; i++ {
		pinName := C.CString(fmt.Sprintf("%s.0.floatout-%02d", name, int(i)))
		rv := C.hal_pin_float_new(pinName, C.HAL_OUT, &rt.hal_float_outputs[i], compID)
		C.free(unsafe.Pointer(pinName))
		if rv != 0 {
			return fmt.Errorf("failed to create pin floatout-%02d: %d", int(i), int(rv))
		}
	}

	// hide_gui pin
	pinName := C.CString(fmt.Sprintf("%s.0.hide_gui", name))
	rv := C.hal_pin_bit_new(pinName, C.HAL_IN, &rt.hal_hide_gui, compID)
	C.free(unsafe.Pointer(pinName))
	if rv != 0 {
		return fmt.Errorf("failed to create hide_gui pin: %d", int(rv))
	}

	return nil
}

// --- State accessors (atomic, safe from any goroutine) ---

func (m *classicladder) getState() int {
	return int(atomic.LoadInt32((*int32)(unsafe.Pointer(&m.rt.state))))
}

func (m *classicladder) setState(state int) {
	atomic.StoreInt32((*int32)(unsafe.Pointer(&m.rt.state)), int32(state))
}

func (m *classicladder) getScanTimeNs() int32 {
	return atomic.LoadInt32((*int32)(unsafe.Pointer(&m.rt.duration_of_last_scan_ns)))
}

func (m *classicladder) getGeneration() uint32 {
	return atomic.LoadUint32((*uint32)(unsafe.Pointer(&m.rt.generation)))
}

func (m *classicladder) bumpGeneration() {
	atomic.AddUint32((*uint32)(unsafe.Pointer(&m.rt.generation)), 1)
}
