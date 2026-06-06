// Package mygomodule is a skeleton Go module for LinuxCNC's gomc-server.
//
// Install with:
//
//	modcompile add-gomod .
//
// Then in your HAL file:
//
//	load mygomodule [optional-arguments]
package mygomodule

import (
	"fmt"
	"log/slog"

	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("mygomodule", newMyGoModule)
}

// myGoModule is a minimal HAL component that copies an input float pin
// to an output float pin. Replace this with your own logic.
type myGoModule struct {
	logger  *slog.Logger
	name    string
	args    []string
	comp    *hal.Component
	inputF  *hal.Pin[float64] // in  float64
	outputF *hal.Pin[float64] // out float64
}

func newMyGoModule(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	m := &myGoModule{
		logger: logger.With("module", name),
		name:   name,
		args:   args,
	}

	m.logger.Info("myGoModule created", "name", m.name, "args", m.args)

	comp, err := hal.NewComponent(m.name)
	if err != nil {
		return nil, fmt.Errorf("hal.NewComponent: %w", err)
	}
	m.comp = comp

	m.inputF, err = hal.NewPin[float64](comp, "in-f", hal.In)
	if err != nil {
		return nil, fmt.Errorf("NewPin[float64] in-f: %w", err)
	}

	m.outputF, err = hal.NewPin[float64](comp, "out-f", hal.Out)
	if err != nil {
		return nil, fmt.Errorf("NewPin[float64] out-f: %w", err)
	}

	if err := comp.Ready(); err != nil {
		return nil, fmt.Errorf("comp.Ready: %w", err)
	}

	m.logger.Info("HAL component ready", "name", m.name)
	return m, nil
}

// Start begins operation. Called after HAL threads are started.
// Launch any background goroutines here.
func (m *myGoModule) Start() error {
	m.logger.Info("myGoModule Start()")
	return nil
}

// Stop shuts down the module gracefully. Called during gomc-server cleanup.
// Stop background goroutines and close connections here.
func (m *myGoModule) Stop() {
	m.logger.Info("myGoModule Stop()")
}

// Destroy releases all resources. Called after all modules have been stopped.
func (m *myGoModule) Destroy() {
	m.logger.Info("myGoModule Destroy()")
	if m.comp != nil {
		if err := m.comp.Exit(); err != nil {
			m.logger.Warn("HAL component exit error", "error", err)
		}
	}
}
