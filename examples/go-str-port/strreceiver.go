package gostrport
// Package strreceiver is part of the go-str-port example.
// It reads strings from a HAL port pin and logs them.
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
	gomc.RegisterModule("strreceiver", newStrReceiver)
}

type strReceiver struct {
	logger *slog.Logger
	comp   *hal.Component
	inPin  *hal.Pin[string]
	done   chan struct{}
}

func newStrReceiver(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	m := &strReceiver{
		logger: logger.With("module", name),
		done:   make(chan struct{}),
	}

	comp, err := hal.NewComponent(name)
	if err != nil {
		return nil, fmt.Errorf("hal.NewComponent: %w", err)
	}
	m.comp = comp

	m.inPin, err = hal.NewPin[string](comp, "in", hal.In)
	if err != nil {
		return nil, fmt.Errorf("NewPin in: %w", err)
	}

	if err := comp.Ready(); err != nil {
		return nil, fmt.Errorf("comp.Ready: %w", err)
	}

	m.logger.Info("strreceiver ready")
	return m, nil
}

func (m *strReceiver) Start() error {
	go m.run()
	return nil
}

func (m *strReceiver) run() {
	for {
		select {
		case <-m.done:
			return
		default:
			msg := m.inPin.Get()
			if msg != "" {
				m.logger.Info("received", "msg", msg)
			}
			time.Sleep(10 * time.Millisecond)
		}
	}
}

func (m *strReceiver) Stop() {
	close(m.done)
}

func (m *strReceiver) Destroy() {
	if m.comp != nil {
		m.comp.Exit()
	}
}
