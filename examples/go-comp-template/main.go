// Package main is a skeleton Go plugin for LinuxCNC's hal-go runtime.
//
// Build with:
//
//	go build -buildmode=plugin -o mygomodule.so .
//
// Then in your HAL file:
//
//	load /path/to/mygomodule.so [optional-arguments]
package main

import (
	"fmt"
	"log/slog"

	hal "github.com/sittner/linuxcnc/src/launcher/pkg/hal"

	"github.com/sittner/linuxcnc/src/launcher/pkg/gomodule"
	"github.com/sittner/linuxcnc/src/launcher/pkg/inifile"
)

// passthroughModule is a minimal HAL component that copies an input float pin
// to an output float pin.  Replace this with your own logic.
type passthroughModule struct {
	logger  *slog.Logger
	name    string
	args    []string
	comp    *hal.Component
	inputF  *hal.Pin[float64] // in  float64
	outputF *hal.Pin[float64] // out float64
}

// Init creates the HAL component and its pins.
// This is called after the plugin is loaded and before HAL file wiring,
// so the pins created here can be connected via net/setp/addf in the HAL file.
func (m *passthroughModule) Init() error {
	m.logger.Info("passthroughModule Init()", "name", m.name, "args", m.args)

	comp, err := hal.NewComponent(m.name)
	if err != nil {
		return fmt.Errorf("hal.NewComponent: %w", err)
	}
	m.comp = comp

	m.inputF, err = hal.NewPin[float64](comp, "in-f", hal.In)
	if err != nil {
		return fmt.Errorf("NewPin[float64] in-f: %w", err)
	}

	m.outputF, err = hal.NewPin[float64](comp, "out-f", hal.Out)
	if err != nil {
		return fmt.Errorf("NewPin[float64] out-f: %w", err)
	}

	if err := comp.Ready(); err != nil {
		return fmt.Errorf("hal.Ready: %w", err)
	}

	return nil
}

// Start begins operation.  Called after HAL threads are started.
// Launch any background goroutines here.
func (m *passthroughModule) Start() error {
	m.logger.Info("passthroughModule Start()")
	// TODO: start background goroutine, open connections, etc.
	return nil
}

// Stop shuts down the module gracefully.  Called during launcher cleanup.
// Note: the plugin code is never actually unloaded from memory (plugin.Open
// has no Close), but all goroutines should be stopped here.
func (m *passthroughModule) Stop() {
	m.logger.Info("passthroughModule Stop()")
	// TODO: stop background goroutines, close connections, etc.
	if m.comp != nil {
		if err := m.comp.Exit(); err != nil {
			m.logger.Warn("HAL component exit error", "error", err)
		}
	}
}

// New is the plugin entry point.  The launcher looks up this symbol by name
// and calls it to create the module instance.
//
// The signature must match gomodule.Factory exactly:
//
//	func(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomodule.Module, error)
//
// name is the instance name — use it as the HAL component name.
// For single-instance loading it defaults to the module's base filename.
// For multi-instance loading via [name1,name2,...] each instance gets a distinct name.
//
// args contains the individual arguments after the module path (and optional name list)
// on the "load" line, e.g.:
//
//	load /path/to/mygomodule.so config=/path/to/config.ini key=value
//
// would give args = []string{"config=/path/to/config.ini", "key=value"}.
//
// The variable is exported by name: the launcher calls plugin.Lookup("New")
// at runtime to find it.  It must be declared as a package-level var (not a
// function) so that its address is stable and the linker exports it correctly.
var New gomodule.Factory = func(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomodule.Module, error) {
	return &passthroughModule{
		logger: logger.With("plugin", name),
		name:   name,
		args:   args,
	}, nil
}
