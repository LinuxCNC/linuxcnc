// Package launcher — unload.go implements per-module unload (halcmd unload).
package launcher

import (
	"fmt"
	"syscall"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	halcmd "github.com/sittner/linuxcnc/src/gomc/internal/halcmd"
)

// UnloadModule unloads a single module by instance name.  The module is
// removed from the function lists, stopped, its APIs unregistered, destroyed,
// and (for cmods) dlclosed if no other instances share the same .so.
//
// Returns EBUSY if another loaded module depends on this module's APIs.
// Returns ENOENT if no module with the given name is found.
func (l *Launcher) UnloadModule(name string) error {
	// Check API dependency guard.
	reg := apiserver.DefaultRegistry()
	if reg != nil {
		consumers := reg.ConsumersOfProvider(name)
		// Filter out the module itself and modules that are no longer loaded.
		var active []string
		for _, c := range consumers {
			if c == name {
				continue
			}
			if l.isModuleLoaded(c) {
				active = append(active, c)
			}
		}
		if len(active) > 0 {
			return fmt.Errorf("cannot unload %q: APIs still consumed by %v: %w",
				name, active, syscall.EBUSY)
		}
	}

	// Try cmod first, then gomod.
	if err := l.unloadCModule(name); err == nil {
		return nil
	}
	if err := l.unloadGoModule(name); err == nil {
		return nil
	}

	return fmt.Errorf("module %q not found: %w", name, syscall.ENOENT)
}

// isModuleLoaded returns true if a module with the given instance name is
// currently loaded (either as cmod or gomod).
func (l *Launcher) isModuleLoaded(name string) bool {
	for _, cm := range l.cModules {
		if cm.name == name {
			return true
		}
	}
	for _, gm := range l.goModules {
		if gm.name == name {
			return true
		}
	}
	return false
}

// unloadCModule unloads a single C plugin module.
func (l *Launcher) unloadCModule(name string) error {
	idx := -1
	for i, cm := range l.cModules {
		if cm.name == name {
			idx = i
			break
		}
	}
	if idx < 0 {
		return syscall.ENOENT
	}

	cm := l.cModules[idx]

	// Step 1: Remove RT functions from threads.
	compID := halcmd.FindCompID(name)
	if compID > 0 {
		removed, _ := halcmd.DelFunctsByComp(compID)
		if removed > 0 {
			// Step 2: Wait for cycle barrier.
			baseline := halcmd.GetMaxCycleCount()
			if err := halcmd.WaitCycleAdvance(baseline); err != nil {
				l.logger.Warn("unload: cycle advance timeout", "module", name, "error", err)
			}
		}
	}

	// Step 3: Stop the module.
	if cm.started {
		cmodStop(cm)
	}

	// Step 4: Remove consumer records (this module as consumer).
	// Step 5: Unregister APIs (this module as provider).
	if reg := apiserver.DefaultRegistry(); reg != nil {
		reg.UnregisterByInstance(name)
	}

	// Step 6: Destroy the module.
	cmodDestroy(cm)

	// Step 7: Clean up env.
	cmodDestroyEnv(cm)
	cm.hCtx.Delete()

	// Step 8: dlclose only if no other instance shares this handle.
	if cm.handle != nil {
		shared := false
		for i, other := range l.cModules {
			if i != idx && other.handle == cm.handle {
				shared = true
				break
			}
		}
		if !shared {
			cmodDlclose(cm)
		}
	}

	// Step 9: Remove from slice.
	l.cModules = append(l.cModules[:idx], l.cModules[idx+1:]...)

	l.logger.Info("unloaded C module", "name", name)
	return nil
}

// unloadGoModule unloads a single Go module.
func (l *Launcher) unloadGoModule(name string) error {
	idx := -1
	for i, gm := range l.goModules {
		if gm.name == name {
			idx = i
			break
		}
	}
	if idx < 0 {
		return syscall.ENOENT
	}

	gm := l.goModules[idx]

	// Step 1: Remove RT functions from threads.
	compID := halcmd.FindCompID(name)
	if compID > 0 {
		removed, _ := halcmd.DelFunctsByComp(compID)
		if removed > 0 {
			// Step 2: Wait for cycle barrier.
			baseline := halcmd.GetMaxCycleCount()
			if err := halcmd.WaitCycleAdvance(baseline); err != nil {
				l.logger.Warn("unload: cycle advance timeout", "module", name, "error", err)
			}
		}
	}

	// Step 3: Stop the module.
	gm.mod.Stop()

	// Step 4+5: Remove consumer records and unregister APIs.
	if reg := apiserver.DefaultRegistry(); reg != nil {
		reg.UnregisterByInstance(name)
	}

	// Step 6: Destroy the module.
	gm.mod.Destroy()

	// Step 7: Remove from slice.
	l.goModules = append(l.goModules[:idx], l.goModules[idx+1:]...)

	l.logger.Info("unloaded Go module", "name", name)
	return nil
}
