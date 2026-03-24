// Package gomodule defines the public interface that Go plugin .so files must
// satisfy to be loaded by the linuxcnc-launcher via the "load" HAL command.
//
// Both the launcher and external plugin packages import this package.
// External plugins built with "go build -buildmode=plugin" must export a
// symbol named "New" whose type matches Factory.
//
// # Plugin lifecycle
//
// The launcher calls these methods in order:
//  1. Factory (New symbol) — create and configure the module instance
//  2. Init — create HAL pins/components (before HAL file wiring)
//  3. Start — begin active operation (after HAL threads start)
//  4. Stop — shut down gracefully (during launcher cleanup)
//
// # Important constraints
//
//   - Go plugins can be loaded but never unloaded (plugin.Open has no Close).
//     Stop handles logical shutdown; the code stays in memory until process exit.
//   - The plugin must be built with the exact same Go version and dependency
//     versions as the launcher binary.
//   - Only supported on Linux (Go plugin limitation).
package gomodule

import (
	"log/slog"

	"github.com/sittner/linuxcnc/src/launcher/pkg/inifile"
)

// Module is the interface that Go plugin .so files must satisfy.
//
// The launcher calls Init, Start, and Stop in that order during the
// startup/shutdown lifecycle.
type Module interface {
	// Init creates HAL components and registers pins. Called after
	// ParseResult.Load() but before ParseResult.Execute(), so that
	// HAL files can wire the module's pins via net/addf/setp.
	Init() error

	// Start begins active operation (e.g. starts goroutines, opens
	// network connections). Called after HAL threads are started.
	Start() error

	// Stop shuts down the module gracefully. Called during launcher
	// cleanup before HAL components are unloaded.
	Stop()
}

// Factory is the function signature that plugins must export as the symbol
// "New". The launcher looks up this symbol and calls it to create the module.
//
// Parameters:
//   - ini: the parsed INI configuration file for the current machine config
//   - logger: a structured logger scoped to the launcher
//   - args: the arguments from the "load" command line (everything after the
//     module path), split by the parser into individual tokens
//
// Example HAL file usage:
//
//	load /usr/lib/linuxcnc/go-modules/mymodule.so config=/path/to/config.ini
//
// In this example, args would be []string{"config=/path/to/config.ini"}.
type Factory func(ini *inifile.IniFile, logger *slog.Logger, args []string) (Module, error)
