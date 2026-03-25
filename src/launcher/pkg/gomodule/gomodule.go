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
//  1. Factory (New symbol) — create, configure, and initialize the module
//     (including HAL component/pin creation, before HAL file wiring)
//  2. Start — begin active operation (after HAL threads start)
//  3. Stop — shut down gracefully (during launcher cleanup)
//  4. Destroy — release all resources (after all modules have stopped)
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
// The launcher calls Start, Stop, and Destroy in that order during
// the startup/shutdown lifecycle. HAL component and pin creation
// happens in the Factory (New) before the module is returned.
type Module interface {
	// Start begins active operation (e.g. starts goroutines, opens
	// network connections). Called after HAL threads are started.
	Start() error

	// Stop shuts down the module gracefully. Signals shutdown, joins
	// goroutines, and closes connections. Called during launcher
	// cleanup before Destroy. All modules are stopped before any
	// module's Destroy is called.
	Stop()

	// Destroy releases all resources (HAL components, allocated memory).
	// Called after all modules have been stopped. The module must not
	// use any shared resources after Destroy returns.
	Destroy()
}

// Factory is the function signature that plugins must export as the symbol
// "New". The launcher looks up this symbol and calls it to create the module.
//
// Parameters:
//   - ini: the parsed INI configuration file for the current machine config
//   - logger: a structured logger scoped to the launcher
//   - name: the instance name for this module instance, to be used as the
//     HAL component name. Defaults to the module's base filename (without .so)
//     when no explicit names are given. For multi-instance loading, each
//     instance receives a distinct name from the [name1,name2,...] syntax.
//   - args: the arguments from the "load" command line (everything after the
//     module path and optional name list), split by the parser into individual
//     tokens
//
// Example HAL file usage:
//
//	load mymodule config=/path/to/config.ini
//
// creates one instance named "mymodule" with args ["config=/path/to/config.ini"].
//
//	load mymodule [inst1,inst2] config=/path/to/config.ini
//
// creates two instances named "inst1" and "inst2", each receiving the same args.
type Factory func(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (Module, error)
