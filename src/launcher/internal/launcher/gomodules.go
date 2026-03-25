// Package launcher — gomodules.go handles loading, lifecycle management of
// Go plugin .so files loaded via the "load" HAL command.
package launcher

import (
	"fmt"
	"path/filepath"
	"plugin"
	"strings"

	"github.com/sittner/linuxcnc/src/launcher/internal/config"
	"github.com/sittner/linuxcnc/src/launcher/pkg/gomodule"
)

// resolveGoModulePath resolves a Go module name or path to an absolute .so path.
// If the name contains a '/' it is treated as an absolute or relative path and
// used as-is.  Otherwise, the bare module name is resolved to
// $EMC2_GOMOD_DIR/<name>.so — the same pattern loadrt uses with EMC2_RTLIB_DIR.
func resolveGoModulePath(name string) string {
	if strings.Contains(name, "/") {
		return name
	}
	// Strip .so suffix if the user specified it explicitly.
	name = strings.TrimSuffix(name, ".so")
	return filepath.Join(config.EMC2GomodDir, name+".so")
}

// loadGoPlugin loads a Go plugin .so, looks up the "New" symbol, validates
// its signature against gomodule.Factory, calls the factory with the launcher's
// INI file and logger, and appends the module to l.goModules.
//
// The factory is expected to create and fully initialize the module (including
// HAL component/pin creation) before returning.
//
// Note: Go plugins can be loaded but never unloaded (plugin.Open has no Close).
// Stop() handles logical shutdown; the plugin code stays resident until the
// process exits.
func (l *Launcher) loadGoPlugin(path string, name string, args []string) error {
	l.logger.Info("loading Go plugin", "path", path, "name", name)

	p, err := plugin.Open(path)
	if err != nil {
		return fmt.Errorf("load Go plugin %q: %w", path, err)
	}

	sym, err := p.Lookup("New")
	if err != nil {
		return fmt.Errorf("load Go plugin %q: missing \"New\" symbol: %w", path, err)
	}

	factoryPtr, ok := sym.(*gomodule.Factory)
	if !ok {
		return fmt.Errorf("load Go plugin %q: \"New\" symbol has wrong type %T (expected *gomodule.Factory)", path, sym)
	}

	mod, err := (*factoryPtr)(l.ini, l.logger, name, args)
	if err != nil {
		return fmt.Errorf("load Go plugin %q: factory error: %w", path, err)
	}

	l.goModules = append(l.goModules, mod)
	l.logger.Info("Go plugin loaded and initialized", "path", path)

	return nil
}

// startGoModules calls Start() on all loaded Go plugin modules.
// Called after HAL threads are started, matching the protocol start sequence.
func (l *Launcher) startGoModules() error {
	for _, m := range l.goModules {
		if err := m.Start(); err != nil {
			return fmt.Errorf("Go module Start() failed: %w", err)
		}
	}
	return nil
}

// stopGoModules calls Stop() on all loaded Go plugin modules in reverse order.
// Called during cleanup before Destroy.
func (l *Launcher) stopGoModules() {
	for i := len(l.goModules) - 1; i >= 0; i-- {
		l.goModules[i].Stop()
	}
}

// destroyGoModules calls Destroy() on all loaded Go plugin modules in reverse order.
// Called after all modules have been stopped to release resources.
func (l *Launcher) destroyGoModules() {
	for i := len(l.goModules) - 1; i >= 0; i-- {
		l.goModules[i].Destroy()
	}
}
