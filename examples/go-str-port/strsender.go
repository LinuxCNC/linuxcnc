package gostrport
// Package strsender is a Go module for gomc-server that writes a test string
// to a HAL port pin continuously. Used with str-receiver to test port pins.
//
// Install with:
//
//	modcompile add-gomod .
//
// Then in your HAL file:
//
//	load strsender
//	load strreceiver
//	net test-str strsender.out strreceiver.in
package strport

import (
	"fmt"
	"log/slog"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("strsender", newStrSender)
}

type strSender struct {
	logger *slog.Logger
	comp   *hal.Component
	outPin *hal.Pin[string]
	done   chan struct{}
}

func newStrSender(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	m := &strSender{
		logger: logger.With("module", name),
		done:   make(chan struct{}),
	}

	comp, err := hal.NewComponent(name)
	if err != nil {
		return nil, fmt.Errorf("hal.NewComponent: %w", err)
	}
	m.comp = comp

	m.outPin, err = hal.NewPin[string](comp, "out", hal.Out)
	if err != nil {
		return nil, fmt.Errorf("NewPin out: %w", err)
	}

	if err := comp.Ready(); err != nil {
		return nil, fmt.Errorf("comp.Ready: %w", err)
	}

	m.logger.Info("strsender ready")
	return m, nil
}

func (m *strSender) Start() error {
	go m.run()
	return nil
}

func (m *strSender) run() {
	for {
		select {
		case <-m.done:
			return
		default:
			m.outPin.Set("hello from go")
			time.Sleep(10 * time.Millisecond)
		}
	}
}

func (m *strSender) Stop() {
	close(m.done)
}

func (m *strSender) Destroy() {
	if m.comp != nil {
		m.comp.Exit()
	}
}
