package gopassthrough
// Package passthrough is a Go module for gomc-server that copies input pins
// to output pins. It demonstrates the basic gomod pattern with all pin types.
//
// Install with:
//
//	modcompile add-gomod .
//
// Then in your HAL file:
//
//	load passthrough
package passthrough

import (
	"fmt"
	"log/slog"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("passthrough", newPassthrough)
}

type passthrough struct {
	logger   *slog.Logger
	comp     *hal.Component
	inBit    *hal.Pin[bool]
	outBit   *hal.Pin[bool]
	inFloat  *hal.Pin[float64]
	outFloat *hal.Pin[float64]
	inS32    *hal.Pin[int32]
	outS32   *hal.Pin[int32]
	inU32    *hal.Pin[uint32]
	outU32   *hal.Pin[uint32]
	inStr    *hal.Pin[string]
	outStr   *hal.Pin[string]
	done     chan struct{}
}

func newPassthrough(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	m := &passthrough{
		logger: logger.With("module", name),
		done:   make(chan struct{}),
	}

	comp, err := hal.NewComponent(name)
	if err != nil {
		return nil, fmt.Errorf("hal.NewComponent: %w", err)
	}
	m.comp = comp

	m.inBit, err = hal.NewPin[bool](comp, "in-bit", hal.In)
	if err != nil {
		return nil, fmt.Errorf("NewPin in-bit: %w", err)
	}
	m.outBit, err = hal.NewPin[bool](comp, "out-bit", hal.Out)
	if err != nil {
		return nil, fmt.Errorf("NewPin out-bit: %w", err)
	}

	m.inFloat, err = hal.NewPin[float64](comp, "in-float", hal.In)
	if err != nil {
		return nil, fmt.Errorf("NewPin in-float: %w", err)
	}
	m.outFloat, err = hal.NewPin[float64](comp, "out-float", hal.Out)
	if err != nil {
		return nil, fmt.Errorf("NewPin out-float: %w", err)
	}

	m.inS32, err = hal.NewPin[int32](comp, "in-s32", hal.In)
	if err != nil {
		return nil, fmt.Errorf("NewPin in-s32: %w", err)
	}
	m.outS32, err = hal.NewPin[int32](comp, "out-s32", hal.Out)
	if err != nil {
		return nil, fmt.Errorf("NewPin out-s32: %w", err)
	}

	m.inU32, err = hal.NewPin[uint32](comp, "in-u32", hal.In)
	if err != nil {
		return nil, fmt.Errorf("NewPin in-u32: %w", err)
	}
	m.outU32, err = hal.NewPin[uint32](comp, "out-u32", hal.Out)
	if err != nil {
		return nil, fmt.Errorf("NewPin out-u32: %w", err)
	}

	m.inStr, err = hal.NewPin[string](comp, "in-str", hal.In)
	if err != nil {
		return nil, fmt.Errorf("NewPin in-str: %w", err)
	}
	m.outStr, err = hal.NewPin[string](comp, "out-str", hal.Out)
	if err != nil {
		return nil, fmt.Errorf("NewPin out-str: %w", err)
	}

	if err := comp.Ready(); err != nil {
		return nil, fmt.Errorf("comp.Ready: %w", err)
	}

	m.logger.Info("passthrough ready")
	return m, nil
}

func (m *passthrough) Start() error {
	go m.run()
	return nil
}

func (m *passthrough) run() {
	for {
		select {
		case <-m.done:
			return
		default:
			m.outBit.Set(m.inBit.Get())
			m.outFloat.Set(m.inFloat.Get())
			m.outS32.Set(m.inS32.Get())
			m.outU32.Set(m.inU32.Get())
			m.outStr.Set(m.inStr.Get())
			time.Sleep(10 * time.Millisecond)
		}
	}
}

func (m *passthrough) Stop() {
	close(m.done)
}

func (m *passthrough) Destroy() {
	if m.comp != nil {
		m.comp.Exit()
	}
}
