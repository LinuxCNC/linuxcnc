// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package classicladder

// API handler implementations — implements classicladderapi.ClassicladderCallbacks
// and ClassicladderWatchCallbacks.

/*
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include
#include "classicladder_rt.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"unsafe"

	api "github.com/sittner/linuxcnc/src/gomc/generated/gmi/classicladder"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

// registerAPI registers REST + WebSocket watch endpoints using generated dispatch code.
func (m *classicladder) registerAPI(reg *apiserver.Registry, instance string) {
	apiserver.RegisterMeta(api.ClassicladderMeta)
	if err := api.RegisterClassicladderAPI(reg, instance, m); err != nil {
		m.logger.Error("register REST API failed", "err", err)
	}
}

func (m *classicladder) registerWatch(wreg *apiserver.WatchRegistry, instance string) {
	api.RegisterClassicladderWatch(wreg, instance, m, api.ClassicladderCommands(m))
}

// --- ClassicladderCallbacks implementation ---

func (m *classicladder) GetStatus() (*api.Status, error) {
	return m.buildStatus(), nil
}

func (m *classicladder) SetState(state api.LadderState) (int32, error) {
	m.setState(int(state))
	if state == api.LadderState_RUN {
		m.modbus.start()
	} else {
		m.modbus.stop()
	}
	return 0, nil
}

func (m *classicladder) GetProgram() (*api.Program, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	prog := m.buildProgram()
	return &prog, nil
}

func (m *classicladder) SetProgram(program api.Program) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.applyProgram(&program)
	m.bumpGeneration()
	return 0, nil
}

func (m *classicladder) GetRung(index int32) (*api.Rung, error) {
	if index < 0 || index >= int32(m.rt.sizes.nbr_rungs) {
		return nil, errInvalidIndex
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	r := m.rungToAPI(int(index))
	return &r, nil
}

func (m *classicladder) SetRung(index int32, rung api.Rung) (int32, error) {
	if index < 0 || index >= int32(m.rt.sizes.nbr_rungs) {
		return -1, errInvalidIndex
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	m.applyRung(int(index), &rung)
	m.bumpGeneration()
	return 0, nil
}

func (m *classicladder) GetSection(index int32) (*api.Section, error) {
	if index < 0 || index >= int32(m.rt.sizes.nbr_sections) {
		return nil, errInvalidIndex
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	s := m.sectionToAPI(int(index))
	return &s, nil
}

func (m *classicladder) SetSection(index int32, section api.Section) (int32, error) {
	if index < 0 || index >= int32(m.rt.sizes.nbr_sections) {
		return -1, errInvalidIndex
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	m.applySection(int(index), &section)
	m.bumpGeneration()
	return 0, nil
}

func (m *classicladder) GetVariables() (*api.Variables, error) {
	vars := m.buildVariables()
	return &vars, nil
}

func (m *classicladder) SetVariable(varType int32, offset int32, value int32) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	C.write_var_ext(m.rt, C.int(varType), C.int(offset), C.int(value))
	return 0, nil
}

func (m *classicladder) LoadProject(path string) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.modbus.stop()
	if err := m.loadCLPFile(path); err != nil {
		return -1, err
	}
	m.bumpGeneration()
	m.modbus.start()
	return 0, nil
}

func (m *classicladder) SaveProject(path string) (int32, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	if err := m.saveCLPFile(path); err != nil {
		return -1, err
	}
	return 0, nil
}

func (m *classicladder) GetSymbols() ([]api.Symbol, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	symbols := make([]api.Symbol, 0)
	for i := 0; i < int(m.rt.sizes.nbr_symbols); i++ {
		s := &m.rt.symbols[i]
		vn := C.GoString(&s.var_name[0])
		if vn == "" {
			continue
		}
		symbols = append(symbols, api.Symbol{
			VarName: vn,
			Symbol:  C.GoString(&s.symbol[0]),
			Comment: C.GoString(&s.comment[0]),
		})
	}
	return symbols, nil
}

func (m *classicladder) SetSymbols(symbols []api.Symbol) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	// Clear existing
	for i := 0; i < int(m.rt.sizes.nbr_symbols); i++ {
		m.rt.symbols[i].var_name[0] = 0
	}
	for i, sym := range symbols {
		if i >= int(m.rt.sizes.nbr_symbols) {
			break
		}
		s := &m.rt.symbols[i]
		copyStringToC(&s.var_name[0], sym.VarName, C.CL_LGT_VAR_NAME)
		copyStringToC(&s.symbol[0], sym.Symbol, C.CL_LGT_SYMBOL_STRING)
		copyStringToC(&s.comment[0], sym.Comment, C.CL_LGT_SYMBOL_COMMENT)
	}
	m.bumpGeneration()
	return 0, nil
}

func (m *classicladder) GetExpressions() ([]api.ArithmExpr, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	exprs := make([]api.ArithmExpr, int(m.rt.sizes.nbr_arithm_expr))
	for i := 0; i < int(m.rt.sizes.nbr_arithm_expr); i++ {
		exprs[i] = api.ArithmExpr{
			Expr: C.GoString(&m.rt.arithm_exprs[i].expr[0]),
		}
	}
	return exprs, nil
}

func (m *classicladder) SetExpressions(exprs []api.ArithmExpr) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	for i, expr := range exprs {
		if i >= int(m.rt.sizes.nbr_arithm_expr) {
			break
		}
		copyStringToC(&m.rt.arithm_exprs[i].expr[0], expr.Expr, C.CL_ARITHM_EXPR_SIZE)
	}
	m.bumpGeneration()
	return 0, nil
}

// --- ClassicladderWatchCallbacks implementation ---

func (m *classicladder) WatchStatus() (*api.Status, error) {
	return m.buildStatus(), nil
}

func (m *classicladder) WatchVariables() (*api.Variables, error) {
	vars := m.buildVariables()
	return &vars, nil
}

// --- Data conversion helpers ---

func (m *classicladder) buildStatus() *api.Status {
	rt := m.rt
	return &api.Status{
		State:             api.LadderState(m.getState()),
		ScanTimeUs:        int32(m.getScanTimeNs() / 1000),
		PeriodicRefreshMs: int32(rt.periodic_refresh_ms),
		Sizes:             m.buildSizes(),
		ProjectFile:       m.projectFile,
	}
}

func (m *classicladder) buildSizes() api.SizeInfo {
	s := m.rt.sizes
	return api.SizeInfo{
		NbrRungs:       int32(s.nbr_rungs),
		NbrBits:        int32(s.nbr_bits),
		NbrWords:       int32(s.nbr_words),
		NbrTimers:      int32(s.nbr_timers),
		NbrMonostables: int32(s.nbr_monostables),
		NbrCounters:    int32(s.nbr_counters),
		NbrTimersIec:   int32(s.nbr_timers_iec),
		NbrPhysInputs:  int32(s.nbr_phys_inputs),
		NbrPhysOutputs: int32(s.nbr_phys_outputs),
		NbrArithmExpr:  int32(s.nbr_arithm_expr),
		NbrSections:    int32(s.nbr_sections),
		NbrSymbols:     int32(s.nbr_symbols),
		NbrS32in:       int32(s.nbr_s32_in),
		NbrS32out:      int32(s.nbr_s32_out),
		NbrFloatIn:     int32(s.nbr_float_in),
		NbrFloatOut:    int32(s.nbr_float_out),
		NbrErrorBits:   int32(s.nbr_error_bits),
	}
}

func (m *classicladder) rungToAPI(idx int) api.Rung {
	cr := &m.rt.rungs[idx]
	r := api.Rung{
		Used:     cr.used != 0,
		PrevRung: int32(cr.prev_rung),
		NextRung: int32(cr.next_rung),
		Label:    C.GoString(&cr.label[0]),
		Comment:  C.GoString(&cr.comment[0]),
	}
	for x := 0; x < C.CL_RUNG_WIDTH; x++ {
		for y := 0; y < C.CL_RUNG_HEIGHT; y++ {
			e := &cr.elements[x][y]
			r.Elements[y*C.CL_RUNG_WIDTH+x] = api.Element{
				Type:             int32(e._type),
				ConnectedWithTop: int32(e.connected_with_top),
				VarType:          int32(e.var_type),
				VarNum:           int32(e.var_num),
			}
		}
	}
	return r
}

func (m *classicladder) sectionToAPI(idx int) api.Section {
	s := &m.rt.sections[idx]
	return api.Section{
		Used:             s.used != 0,
		Name:             C.GoString(&s.name[0]),
		Language:         api.SectionLanguage(s.language),
		SubRoutineNumber: int32(s.sub_routine_number),
		FirstRung:        int32(s.first_rung),
		LastRung:         int32(s.last_rung),
		SequentialPage:   int32(s.sequential_page),
	}
}

func (m *classicladder) applyRung(idx int, r *api.Rung) {
	cr := &m.rt.rungs[idx]
	if r.Used {
		cr.used = 1
	} else {
		cr.used = 0
	}
	cr.prev_rung = C.int(r.PrevRung)
	cr.next_rung = C.int(r.NextRung)
	copyStringToC(&cr.label[0], r.Label, C.CL_LGT_LABEL)
	copyStringToC(&cr.comment[0], r.Comment, C.CL_LGT_COMMENT)
	for x := 0; x < C.CL_RUNG_WIDTH; x++ {
		for y := 0; y < C.CL_RUNG_HEIGHT; y++ {
			i := y*C.CL_RUNG_WIDTH + x
			e := r.Elements[i]
			cr.elements[x][y]._type = C.int16_t(e.Type)
			cr.elements[x][y].connected_with_top = C.int8_t(e.ConnectedWithTop)
			cr.elements[x][y].var_type = C.int32_t(e.VarType)
			cr.elements[x][y].var_num = C.int32_t(e.VarNum)
		}
	}
}

func (m *classicladder) applySection(idx int, s *api.Section) {
	sec := &m.rt.sections[idx]
	if s.Used {
		sec.used = 1
	} else {
		sec.used = 0
	}
	copyStringToC(&sec.name[0], s.Name, C.CL_LGT_SECTION_NAME)
	sec.language = C.int(s.Language)
	sec.sub_routine_number = C.int(s.SubRoutineNumber)
	sec.first_rung = C.int(s.FirstRung)
	sec.last_rung = C.int(s.LastRung)
	sec.sequential_page = C.int(s.SequentialPage)
}

func (m *classicladder) applyProgram(prog *api.Program) {
	rt := m.rt

	for i, r := range prog.Rungs {
		if i >= int(rt.sizes.nbr_rungs) {
			break
		}
		m.applyRung(i, &r)
	}

	for i, s := range prog.Sections {
		if i >= int(rt.sizes.nbr_sections) {
			break
		}
		m.applySection(i, &s)
	}

	for i, sym := range prog.Symbols {
		if i >= int(rt.sizes.nbr_symbols) {
			break
		}
		s := &rt.symbols[i]
		copyStringToC(&s.var_name[0], sym.VarName, C.CL_LGT_VAR_NAME)
		copyStringToC(&s.symbol[0], sym.Symbol, C.CL_LGT_SYMBOL_STRING)
		copyStringToC(&s.comment[0], sym.Comment, C.CL_LGT_SYMBOL_COMMENT)
	}

	for i, expr := range prog.ArithmExprs {
		if i >= int(rt.sizes.nbr_arithm_expr) {
			break
		}
		copyStringToC(&rt.arithm_exprs[i].expr[0], expr.Expr, C.CL_ARITHM_EXPR_SIZE)
	}

	for i, t := range prog.TimersIec {
		if i >= int(rt.sizes.nbr_timers_iec) {
			break
		}
		rt.timers_iec[i].preset = C.int(t.Preset)
		rt.timers_iec[i].base = C.int(t.Base)
		rt.timers_iec[i].timer_mode = C.char(t.Mode)
	}

	for i, t := range prog.Timers {
		if i >= int(rt.sizes.nbr_timers) {
			break
		}
		rt.timers[i].preset = C.int(t.Preset)
		rt.timers[i].base = C.int(t.Base)
	}

	for i, mo := range prog.Monostables {
		if i >= int(rt.sizes.nbr_monostables) {
			break
		}
		rt.monostables[i].preset = C.int(mo.Preset)
		rt.monostables[i].base = C.int(mo.Base)
	}

	for i, c := range prog.Counters {
		if i >= int(rt.sizes.nbr_counters) {
			break
		}
		rt.counters[i].preset = C.int(c.Preset)
	}
}

func (m *classicladder) buildProgram() api.Program {
	rt := m.rt
	prog := api.Program{
		Sizes: m.buildSizes(),
	}

	prog.Rungs = make([]api.Rung, int(rt.sizes.nbr_rungs))
	for i := 0; i < int(rt.sizes.nbr_rungs); i++ {
		prog.Rungs[i] = m.rungToAPI(i)
	}

	prog.Sections = make([]api.Section, int(rt.sizes.nbr_sections))
	for i := 0; i < int(rt.sizes.nbr_sections); i++ {
		prog.Sections[i] = m.sectionToAPI(i)
	}

	prog.Symbols = make([]api.Symbol, int(rt.sizes.nbr_symbols))
	for i := 0; i < int(rt.sizes.nbr_symbols); i++ {
		s := &rt.symbols[i]
		prog.Symbols[i] = api.Symbol{
			VarName: C.GoString(&s.var_name[0]),
			Symbol:  C.GoString(&s.symbol[0]),
			Comment: C.GoString(&s.comment[0]),
		}
	}

	prog.ArithmExprs = make([]api.ArithmExpr, int(rt.sizes.nbr_arithm_expr))
	for i := 0; i < int(rt.sizes.nbr_arithm_expr); i++ {
		prog.ArithmExprs[i] = api.ArithmExpr{
			Expr: C.GoString(&rt.arithm_exprs[i].expr[0]),
		}
	}

	prog.TimersIec = make([]api.TimerIEC, int(rt.sizes.nbr_timers_iec))
	for i := 0; i < int(rt.sizes.nbr_timers_iec); i++ {
		t := &rt.timers_iec[i]
		prog.TimersIec[i] = api.TimerIEC{
			Preset: int32(t.preset),
			Value:  int32(t.value),
			Base:   int32(t.base),
			Mode:   api.TimerIECMode(t.timer_mode),
			Input:  t.input != 0,
			Output: t.output != 0,
		}
	}

	prog.Timers = make([]api.Timer, int(rt.sizes.nbr_timers))
	for i := 0; i < int(rt.sizes.nbr_timers); i++ {
		t := &rt.timers[i]
		prog.Timers[i] = api.Timer{
			Preset:        int32(t.preset),
			Value:         int32(t.value),
			Base:          int32(t.base),
			InputEnable:   t.input_enable != 0,
			InputControl:  t.input_control != 0,
			OutputDone:    t.output_done != 0,
			OutputRunning: t.output_running != 0,
		}
	}

	prog.Monostables = make([]api.Monostable, int(rt.sizes.nbr_monostables))
	for i := 0; i < int(rt.sizes.nbr_monostables); i++ {
		mo := &rt.monostables[i]
		prog.Monostables[i] = api.Monostable{
			Preset:        int32(mo.preset),
			Value:         int32(mo.value),
			Base:          int32(mo.base),
			Input:         mo.input != 0,
			OutputRunning: mo.output_running != 0,
		}
	}

	prog.Counters = make([]api.Counter, int(rt.sizes.nbr_counters))
	for i := 0; i < int(rt.sizes.nbr_counters); i++ {
		c := &rt.counters[i]
		prog.Counters[i] = api.Counter{
			Preset:         int32(c.preset),
			Value:          int32(c.value),
			InputReset:     c.input_reset != 0,
			InputPreset:    c.input_preset != 0,
			InputCountUp:   c.input_count_up != 0,
			InputCountDown: c.input_count_down != 0,
			OutputDone:     c.output_done != 0,
			OutputEmpty:    c.output_empty != 0,
			OutputFull:     c.output_full != 0,
		}
	}

	return prog
}

func (m *classicladder) buildVariables() api.Variables {
	rt := m.rt
	nbits := int(rt.sizes.nbr_bits)
	nin := int(rt.sizes.nbr_phys_inputs)
	nout := int(rt.sizes.nbr_phys_outputs)
	nerr := int(rt.sizes.nbr_error_bits)

	var vars api.Variables
	vars.Bools.Bits = make([]bool, nbits)
	for i := 0; i < nbits; i++ {
		vars.Bools.Bits[i] = rt.var_bits[i] != 0
	}
	vars.Bools.PhysInputs = make([]bool, nin)
	for i := 0; i < nin; i++ {
		vars.Bools.PhysInputs[i] = rt.var_bits[nbits+i] != 0
	}
	vars.Bools.PhysOutputs = make([]bool, nout)
	for i := 0; i < nout; i++ {
		vars.Bools.PhysOutputs[i] = rt.var_bits[nbits+nin+i] != 0
	}
	vars.Bools.ErrorBits = make([]bool, nerr)
	for i := 0; i < nerr; i++ {
		vars.Bools.ErrorBits[i] = rt.var_bits[nbits+nin+nout+C.CL_MAX_STEPS+i] != 0
	}

	nwords := int(rt.sizes.nbr_words)
	ns32in := int(rt.sizes.nbr_s32_in)
	ns32out := int(rt.sizes.nbr_s32_out)
	vars.Words.Words = make([]int32, nwords)
	for i := 0; i < nwords; i++ {
		vars.Words.Words[i] = int32(rt.var_words[i])
	}
	vars.Words.PhysWordInputs = make([]int32, ns32in)
	for i := 0; i < ns32in; i++ {
		vars.Words.PhysWordInputs[i] = int32(rt.var_words[nwords+i])
	}
	vars.Words.PhysWordOutputs = make([]int32, ns32out)
	for i := 0; i < ns32out; i++ {
		vars.Words.PhysWordOutputs[i] = int32(rt.var_words[nwords+ns32in+i])
	}

	nfin := int(rt.sizes.nbr_float_in)
	nfout := int(rt.sizes.nbr_float_out)
	vars.Floats.PhysFloatInputs = make([]float64, nfin)
	for i := 0; i < nfin; i++ {
		vars.Floats.PhysFloatInputs[i] = float64(rt.var_floats[i])
	}
	vars.Floats.PhysFloatOutputs = make([]float64, nfout)
	for i := 0; i < nfout; i++ {
		vars.Floats.PhysFloatOutputs[i] = float64(rt.var_floats[nfin+i])
	}

	return vars
}

// --- Utility functions ---

func copyStringToC(dst *C.char, src string, maxLen C.int) {
	cstr := C.CString(src)
	C.strncpy(dst, cstr, C.size_t(maxLen-1))
	p := (*C.char)(unsafe.Pointer(uintptr(unsafe.Pointer(dst)) + uintptr(maxLen-1)))
	*p = 0
	C.free(unsafe.Pointer(cstr))
}

var errInvalidIndex = fmt.Errorf("invalid index")

// --- Modbus API handlers ---

func (m *classicladder) GetModbusComParams() (*api.ModbusComParams, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	cfg := m.modbus.cfg
	return &api.ModbusComParams{
		SerialPort:        cfg.SerialPort,
		SerialSpeed:       int32(cfg.SerialSpeed),
		SerialDataBits:    int32(cfg.SerialDataBits),
		SerialStopBits:    int32(cfg.SerialStopBits),
		SerialParity:      int32(cfg.SerialParity),
		SerialUseRts:      cfg.SerialUseRTS,
		ElementOffset:     int32(cfg.ElementOffset),
		TimeInterFrame:    int32(cfg.TimeInterFrame),
		TimeOutReceipt:    int32(cfg.TimeOutReceipt),
		TimeAfterTransmit: int32(cfg.TimeAfterTransmit),
		DebugLevel:        int32(cfg.DebugLevel),
		MapCoilRead:       int32(cfg.MapCoilRead),
		MapCoilWrite:      int32(cfg.MapCoilWrite),
		MapInputs:         int32(cfg.MapInputs),
		MapHolding:        int32(cfg.MapHolding),
		MapRegisterRead:   int32(cfg.MapRegisterRead),
		MapRegisterWrite:  int32(cfg.MapRegisterWrite),
	}, nil
}

func (m *classicladder) SetModbusComParams(params api.ModbusComParams) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	wasRunning := m.modbus.running
	if wasRunning {
		m.modbus.stop()
	}
	m.modbus.cfg.SerialPort = params.SerialPort
	m.modbus.cfg.SerialSpeed = int(params.SerialSpeed)
	m.modbus.cfg.SerialDataBits = int(params.SerialDataBits)
	m.modbus.cfg.SerialStopBits = int(params.SerialStopBits)
	m.modbus.cfg.SerialParity = int(params.SerialParity)
	m.modbus.cfg.SerialUseRTS = params.SerialUseRts
	m.modbus.cfg.ElementOffset = int(params.ElementOffset)
	m.modbus.cfg.TimeInterFrame = int(params.TimeInterFrame)
	m.modbus.cfg.TimeOutReceipt = int(params.TimeOutReceipt)
	m.modbus.cfg.TimeAfterTransmit = int(params.TimeAfterTransmit)
	m.modbus.cfg.DebugLevel = int(params.DebugLevel)
	m.modbus.cfg.MapCoilRead = int(params.MapCoilRead)
	m.modbus.cfg.MapCoilWrite = int(params.MapCoilWrite)
	m.modbus.cfg.MapInputs = int(params.MapInputs)
	m.modbus.cfg.MapHolding = int(params.MapHolding)
	m.modbus.cfg.MapRegisterRead = int(params.MapRegisterRead)
	m.modbus.cfg.MapRegisterWrite = int(params.MapRegisterWrite)
	if wasRunning {
		m.modbus.start()
	}
	return 0, nil
}

func (m *classicladder) GetModbusRequests() ([]api.ModbusRequest, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	reqs := make([]api.ModbusRequest, len(m.modbus.cfg.Requests))
	for i, r := range m.modbus.cfg.Requests {
		reqs[i] = api.ModbusRequest{
			SlaveAddr:          r.SlaveAddr,
			TypeReq:            api.ModbusReqType(r.TypeReq),
			FirstModbusElement: int32(r.FirstModbusElement),
			NbrModbusElements:  int32(r.NbrModbusElements),
			LogicInverted:      r.LogicInverted,
			OffsetVarMapped:    int32(r.OffsetVarMapped),
		}
	}
	return reqs, nil
}

func (m *classicladder) SetModbusRequests(requests []api.ModbusRequest) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	wasRunning := m.modbus.running
	if wasRunning {
		m.modbus.stop()
	}
	m.modbus.cfg.Requests = make([]modbusRequest, len(requests))
	for i, r := range requests {
		m.modbus.cfg.Requests[i] = modbusRequest{
			SlaveAddr:          r.SlaveAddr,
			TypeReq:            int(r.TypeReq),
			FirstModbusElement: int(r.FirstModbusElement),
			NbrModbusElements:  int(r.NbrModbusElements),
			LogicInverted:      r.LogicInverted,
			OffsetVarMapped:    int(r.OffsetVarMapped),
		}
	}
	if wasRunning {
		m.modbus.start()
	}
	return 0, nil
}

func (m *classicladder) GetModbusStatus() (*api.ModbusStatus, error) {
	m.modbus.mu.Lock()
	defer m.modbus.mu.Unlock()
	return &api.ModbusStatus{
		Running:    m.modbus.running,
		CurrentReq: int32(m.modbus.currentReq),
		FrameCount: int32(m.modbus.frameCount),
		ErrorCount: int32(m.modbus.errorCount),
		SlavePort:  int32(m.slavePort),
	}, nil
}
