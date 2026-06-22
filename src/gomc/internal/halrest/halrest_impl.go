// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package halrest

import (
	"fmt"

	halcmdapi "github.com/sittner/linuxcnc/src/gomc/generated/gmi/halcmd"
	"github.com/sittner/linuxcnc/src/gomc/internal/halcmd"
	hal "github.com/sittner/linuxcnc/src/gomc/pkg/hal"
)

// halcmdImpl implements halcmdapi.HalcmdCallbacks by delegating to the
// internal halcmd package (direct HAL shared-memory access via CGO).
type halcmdImpl struct{}

func showPatterns(pattern string) []string {
	if pattern != "" {
		return []string{pattern}
	}
	return nil
}

func (h *halcmdImpl) ListPins(pattern string) ([]halcmdapi.PinInfo, error) {
	result, err := halcmd.Show("pin", showPatterns(pattern)...)
	if err != nil {
		return nil, err
	}
	out := make([]halcmdapi.PinInfo, 0, len(result.Pins))
	for _, p := range result.Pins {
		pi := halcmdapi.PinInfo{
			Name:      p.Name,
			Type:      p.Type,
			Dir:       p.Direction,
			Value:     p.Value,
			Owner:     p.Owner,
			Linked:    p.Signal != "",
			HasWriter: p.HasWriter,
		}
		if p.Signal != "" {
			pi.Signal = p.Signal
		}
		out = append(out, pi)
	}
	return out, nil
}

func (h *halcmdImpl) ListParams(pattern string) ([]halcmdapi.ParamInfo, error) {
	result, err := halcmd.Show("param", showPatterns(pattern)...)
	if err != nil {
		return nil, err
	}
	out := make([]halcmdapi.ParamInfo, 0, len(result.Params))
	for _, p := range result.Params {
		out = append(out, halcmdapi.ParamInfo{
			Name:  p.Name,
			Type:  p.Type,
			Dir:   p.Direction,
			Value: p.Value,
			Owner: p.Owner,
		})
	}
	return out, nil
}

func (h *halcmdImpl) ListSignals(pattern string) ([]halcmdapi.SignalInfo, error) {
	result, err := halcmd.Show("sig", showPatterns(pattern)...)
	if err != nil {
		return nil, err
	}
	out := make([]halcmdapi.SignalInfo, 0, len(result.Signals))
	for _, s := range result.Signals {
		out = append(out, halcmdapi.SignalInfo{
			Name:    s.Name,
			Type:    s.Type,
			Value:   s.Value,
			Writers: []string{},
			Readers: []string{},
			Bidirs:  []string{},
		})
	}
	return out, nil
}

func (h *halcmdImpl) ListComponents(pattern string) ([]halcmdapi.ComponentInfo, error) {
	result, err := halcmd.Show("comp", showPatterns(pattern)...)
	if err != nil {
		return nil, err
	}
	out := make([]halcmdapi.ComponentInfo, 0, len(result.Comps))
	for _, c := range result.Comps {
		out = append(out, halcmdapi.ComponentInfo{
			Name:  c.Name,
			Id:    int32(c.ID),
			Type:  c.Type,
			State: "Ready",
		})
	}
	return out, nil
}

func (h *halcmdImpl) ListFunctions(pattern string) ([]halcmdapi.FunctionInfo, error) {
	result, err := halcmd.Show("funct", showPatterns(pattern)...)
	if err != nil {
		return nil, err
	}
	out := make([]halcmdapi.FunctionInfo, 0, len(result.Functs))
	for _, f := range result.Functs {
		out = append(out, halcmdapi.FunctionInfo{
			Name:    f.Name,
			Owner:   f.Owner,
			Users:   f.Users,
			Runtime: f.MaxTime,
			Fp:      f.FP,
		})
	}
	return out, nil
}

func (h *halcmdImpl) ListThreads(pattern string) ([]halcmdapi.ThreadInfo, error) {
	result, err := halcmd.Show("thread", showPatterns(pattern)...)
	if err != nil {
		return nil, err
	}
	out := make([]halcmdapi.ThreadInfo, 0, len(result.Threads))
	for _, t := range result.Threads {
		out = append(out, halcmdapi.ThreadInfo{
			Name:      t.Name,
			Period:    t.Period,
			Fp:        t.FP,
			Functions: t.Functs,
		})
	}
	return out, nil
}

func (h *halcmdImpl) GetPin(name string) (*halcmdapi.PinInfo, error) {
	result, err := halcmd.Show("pin", name)
	if err != nil {
		return nil, err
	}
	if len(result.Pins) == 0 {
		return nil, fmt.Errorf("pin %q not found", name)
	}
	p := result.Pins[0]
	pi := &halcmdapi.PinInfo{
		Name:      p.Name,
		Type:      p.Type,
		Dir:       p.Direction,
		Value:     p.Value,
		Owner:     p.Owner,
		Linked:    p.Signal != "",
		HasWriter: p.HasWriter,
	}
	if p.Signal != "" {
		pi.Signal = p.Signal
	}
	return pi, nil
}

func (h *halcmdImpl) GetParam(name string) (*halcmdapi.ParamInfo, error) {
	result, err := halcmd.Show("param", name)
	if err != nil {
		return nil, err
	}
	if len(result.Params) == 0 {
		return nil, fmt.Errorf("param %q not found", name)
	}
	p := result.Params[0]
	return &halcmdapi.ParamInfo{
		Name:  p.Name,
		Type:  p.Type,
		Dir:   p.Direction,
		Value: p.Value,
		Owner: p.Owner,
	}, nil
}

func (h *halcmdImpl) GetSignal(name string) (*halcmdapi.SignalInfo, error) {
	result, err := halcmd.Show("sig", name)
	if err != nil {
		return nil, err
	}
	if len(result.Signals) == 0 {
		return nil, fmt.Errorf("signal %q not found", name)
	}
	s := result.Signals[0]

	// Find connected pins by listing all pins and filtering by signal name.
	pinResult, err := halcmd.Show("pin")
	writers := []string{}
	readers := []string{}
	bidirs := []string{}
	if err == nil {
		for _, p := range pinResult.Pins {
			if p.Signal == name {
				switch p.Direction {
				case "OUT":
					writers = append(writers, p.Name)
				case "IN":
					readers = append(readers, p.Name)
				case "IO":
					bidirs = append(bidirs, p.Name)
				}
			}
		}
	}

	return &halcmdapi.SignalInfo{
		Name:    s.Name,
		Type:    s.Type,
		Value:   s.Value,
		Writers: writers,
		Readers: readers,
		Bidirs:  bidirs,
	}, nil
}

func (h *halcmdImpl) GetStatus() (*halcmdapi.HalStatus, error) {
	st, err := halcmd.Status()
	if err != nil {
		return nil, err
	}
	result, err := halcmd.Show("all")
	if err != nil {
		return nil, err
	}
	return &halcmdapi.HalStatus{
		RtLock:     st.LockLevel != "NONE",
		Components: int32(len(result.Comps)),
		Pins:       int32(len(result.Pins)),
		Signals:    int32(len(result.Signals)),
		Params:     int32(len(result.Params)),
		Threads:    int32(len(result.Threads)),
		Functions:  int32(len(result.Functs)),
	}, nil
}

func okCmd() *halcmdapi.CmdResult {
	return &halcmdapi.CmdResult{Success: true}
}

func errCmd(err error) (*halcmdapi.CmdResult, error) {
	return &halcmdapi.CmdResult{Success: false, Error: err.Error()}, nil
}

func (h *halcmdImpl) SetPin(name string, value string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.SetP(name, value); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) SetParam(name string, value string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.SetP(name, value); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) AliasPin(name string, alias string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.Alias("pin", name, alias); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) AliasParam(name string, alias string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.Alias("param", name, alias); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) UnaliasPin(name string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.UnAlias("pin", name); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) UnaliasParam(name string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.UnAlias("param", name); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) SetSignal(name string, value string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.SetS(name, value); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) NewSignal(name string, type_ string) (*halcmdapi.CmdResult, error) {
	halType, err := parseHalType(type_)
	if err != nil {
		return errCmd(err)
	}
	if err := halcmd.NewSig(name, halType); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) DeleteSignal(name string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.DelSig(name); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Link(pin string, signal string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.LinkPS(pin, signal); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) LinkPp(pin1 string, pin2 string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.Net(pin1, pin1, pin2); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Unlink(pin string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.UnlinkP(pin); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Net(signal string, pins []string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.Net(signal, pins...); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Load(module string, args []string) (*halcmdapi.CmdResult, error) {
	if loadModuleHook == nil {
		return errCmd(fmt.Errorf("load: not supported (gomc-server launcher not initialized)"))
	}
	if err := loadModuleHook(module, args); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Unload(name string) (*halcmdapi.CmdResult, error) {
	if unloadModuleHook == nil {
		return errCmd(fmt.Errorf("unload: not supported (gomc-server launcher not initialized)"))
	}
	if err := unloadModuleHook(name); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Newthread(name string, periodNs int64, fp *bool, cpuId *int32) (*halcmdapi.CmdResult, error) {
	usesFP := 0
	if fp != nil && *fp {
		usesFP = 1
	}
	cpuID := -1
	if cpuId != nil {
		cpuID = int(*cpuId)
	}
	if err := halcmd.CreateThreadCPU(name, periodNs, usesFP, cpuID); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Delthread(name string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.ThreadDelete(name); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Addf(thread string, function string, position *int32) (*halcmdapi.CmdResult, error) {
	pos := -1
	if position != nil {
		pos = int(*position)
	}
	if err := halcmd.AddF(function, thread, pos); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Delf(thread string, function string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.DelF(function, thread); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Start() (*halcmdapi.CmdResult, error) {
	if err := halcmd.StartThreads(); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Stop() (*halcmdapi.CmdResult, error) {
	if err := halcmd.StopThreads(); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Lock(level string) (*halcmdapi.CmdResult, error) {
	if level == "" {
		level = "all"
	}
	if err := halcmd.Lock(level); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Unlock(level string) (*halcmdapi.CmdResult, error) {
	if level == "" {
		level = "all"
	}
	if err := halcmd.Unlock(level); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) SetDebug(level int32) (*halcmdapi.CmdResult, error) {
	if err := halcmd.SetDebug(int(level)); err != nil {
		return errCmd(err)
	}
	return okCmd(), nil
}

func (h *halcmdImpl) Save(type_ string) (*halcmdapi.CmdResult, error) {
	if type_ == "" {
		type_ = "all"
	}
	lines, err := halcmd.Save(type_, "")
	if err != nil {
		return errCmd(err)
	}
	output := ""
	for _, line := range lines {
		output += line + "\n"
	}
	return &halcmdapi.CmdResult{Success: true, Output: output}, nil
}

func (h *halcmdImpl) Retain(name string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.Retain(name); err != nil {
		return errCmd(err)
	}
	return &halcmdapi.CmdResult{Success: true}, nil
}

func (h *halcmdImpl) Unretain(name string) (*halcmdapi.CmdResult, error) {
	if err := halcmd.Unretain(name); err != nil {
		return errCmd(err)
	}
	return &halcmdapi.CmdResult{Success: true}, nil
}

func (h *halcmdImpl) WatchItems(names []string) ([]halcmdapi.PinInfo, error) {
	result, err := halcmd.Show("pin")
	if err != nil {
		return nil, err
	}
	out := make([]halcmdapi.PinInfo, 0, len(result.Pins))
	for _, p := range result.Pins {
		pi := halcmdapi.PinInfo{
			Name:      p.Name,
			Type:      p.Type,
			Dir:       p.Direction,
			Value:     p.Value,
			Owner:     p.Owner,
			Linked:    p.Signal != "",
			HasWriter: p.HasWriter,
		}
		if p.Signal != "" {
			pi.Signal = p.Signal
		}
		out = append(out, pi)
	}
	return out, nil
}

// parseHalType converts a HAL type string to a hal.PinType constant.
func parseHalType(s string) (hal.PinType, error) {
	switch s {
	case "bit":
		return hal.TypeBit, nil
	case "float":
		return hal.TypeFloat, nil
	case "s32":
		return hal.TypeS32, nil
	case "u32":
		return hal.TypeU32, nil
	default:
		return 0, fmt.Errorf("unknown HAL type: %s", s)
	}
}
