// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package gomc provides the registration interface for Go modules compiled
// into the gomc-server binary. External Go packages and in-tree modules use
// this package to register themselves at init() time so the launcher can
// instantiate them when a HAL "load" command references their name.
//
// This replaces the old plugin-based gomodule.Factory mechanism — modules are
// now compiled directly into the server binary instead of loaded as .so plugins.
package gomc

import (
	"log/slog"
	"sync"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// Module is the lifecycle interface for Go modules compiled into the server.
// Mirrors the cmod lifecycle: factory → Start → Stop → Destroy.
type Module interface {
	Start() error
	Stop()
	Destroy()
}

// Factory creates a new Module instance. Called by the launcher when a HAL
// "load" command references the registered module name.
type Factory func(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (Module, error)

var (
	mu        sync.RWMutex
	factories = make(map[string]Factory)
)

// RegisterModule registers a Go module factory by name. Called from init() of
// compiled-in packages. Panics on duplicate registration.
func RegisterModule(name string, factory Factory) {
	mu.Lock()
	defer mu.Unlock()
	if _, exists := factories[name]; exists {
		panic("gomc: duplicate module registration: " + name)
	}
	factories[name] = factory
}

// GetFactory returns the factory for the named module, or nil if not registered.
func GetFactory(name string) Factory {
	mu.RLock()
	defer mu.RUnlock()
	return factories[name]
}

// HasModule returns true if a module with the given name is registered.
func HasModule(name string) bool {
	mu.RLock()
	defer mu.RUnlock()
	_, ok := factories[name]
	return ok
}
