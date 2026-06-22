// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package pyvcpmodule registers the PyVCP panel module with the gomc module
// registry. When compiled into the gomc-server binary, this package's init()
// function registers a factory that creates PyVCP panel instances in response
// to HAL "load pyvcp" commands.
//
// Each instance parses a PyVCP XML file, extracts widget pin definitions,
// creates a HAL component with the required pins, and provides REST + WebSocket
// endpoints for the Python frontend to display/control the panel.
//
// Usage in a HAL file:
//
//	load pyvcp [mypanel] xml=panel.xml
//
// Parameters:
//   - xml=<path>  — path to the PyVCP XML file (required; resolved relative
//     to the INI file directory if not absolute)
package pyvcpmodule

import (
	"fmt"
	"log/slog"
	"path/filepath"
	"strings"
	"sync"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/pyvcp"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("pyvcp", newPyVCPModule)

	// Register REST meta so the HTTP server knows about pyvcp routes.
	apiserver.RegisterMeta(pyvcp.PyvcpMeta)
}

// pyvcpModule implements gomc.Module for a PyVCP panel.
type pyvcpModule struct {
	logger *slog.Logger
	comp   *hal.Component
	panel  *panel
}

func (m *pyvcpModule) Start() error { return nil }
func (m *pyvcpModule) Stop()        {}
func (m *pyvcpModule) Destroy() {
	if m.comp != nil {
		if err := m.comp.Exit(); err != nil {
			m.logger.Debug("pyvcp HAL component exit error", "name", m.panel.name, "error", err)
		}
	}
}

func newPyVCPModule(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	xmlPath := ""
	for _, arg := range args {
		k, v, ok := strings.Cut(arg, "=")
		if !ok {
			continue
		}
		if k == "xml" {
			xmlPath = v
		}
	}
	if xmlPath == "" {
		return nil, fmt.Errorf("pyvcp: missing required xml= parameter")
	}

	// Resolve relative paths against the INI file directory.
	if !filepath.IsAbs(xmlPath) {
		iniDir := filepath.Dir(ini.SourceFile())
		xmlPath = filepath.Join(iniDir, xmlPath)
	}

	logger = logger.With("module", "pyvcp", "name", name)
	logger.Info("loading PyVCP panel", "xml", xmlPath)

	// Parse XML and extract pin definitions.
	p, err := parsePanel(name, xmlPath)
	if err != nil {
		return nil, fmt.Errorf("pyvcp %q: %w", name, err)
	}

	// Create HAL component.
	comp, err := hal.NewComponent(name)
	if err != nil {
		return nil, fmt.Errorf("pyvcp %q: creating HAL component: %w", name, err)
	}

	// Create all pins.
	if err := p.createPins(comp); err != nil {
		return nil, fmt.Errorf("pyvcp %q: creating pins: %w", name, err)
	}

	if err := comp.Ready(); err != nil {
		return nil, fmt.Errorf("pyvcp %q: hal ready: %w", name, err)
	}

	// Register with the panel registry for REST/WS access.
	panelRegistry.register(p)

	// Register the API instance with the apiserver registry.
	cb := &pyvcpCallbacks{panel: p, comp: comp}
	if err := pyvcp.RegisterPyvcpAPI(apiserver.DefaultRegistry(), name, cb); err != nil {
		return nil, fmt.Errorf("pyvcp %q: api register: %w", name, err)
	}

	// Register WebSocket watch API.
	if apiserver.DefaultWatchRegistry() == nil {
		apiserver.SetDefaultWatchRegistry(apiserver.NewWatchRegistry())
	}
	pyvcp.RegisterPyvcpWatch(
		apiserver.DefaultWatchRegistry(), name, cb,
		pyvcp.PyvcpCommands(cb),
	)

	logger.Info("PyVCP panel initialized", "name", name, "pins", len(p.pins))

	return &pyvcpModule{
		logger: logger,
		comp:   comp,
		panel:  p,
	}, nil
}

// --- Panel registry (shared across all pyvcp instances) ---

var panelRegistry = newPanelRegistry()

type panelRegistry_ struct {
	mu     sync.RWMutex
	panels map[string]*panel
}

func newPanelRegistry() *panelRegistry_ {
	return &panelRegistry_{panels: make(map[string]*panel)}
}

func (r *panelRegistry_) register(p *panel) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.panels[p.name] = p
}

func (r *panelRegistry_) get(name string) *panel {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.panels[name]
}

func (r *panelRegistry_) list() []string {
	r.mu.RLock()
	defer r.mu.RUnlock()
	names := make([]string, 0, len(r.panels))
	for name := range r.panels {
		names = append(names, name)
	}
	return names
}

// pyvcpCallbacks holds the state for one panel's API callbacks.
// Implements pyvcp.PyvcpCallbacks and pyvcp.PyvcpWatchCallbacks.
type pyvcpCallbacks struct {
	panel *panel
	comp  *hal.Component
}

// --- PyvcpCallbacks implementation (REST + WS commands) ---

func (cb *pyvcpCallbacks) ListPanels() ([]string, error) {
	return panelRegistry.list(), nil
}

func (cb *pyvcpCallbacks) GetPanel(name string) (*pyvcp.PanelInfo, error) {
	p := panelRegistry.get(name)
	if p == nil {
		return nil, fmt.Errorf("panel %q not found", name)
	}
	defs := make([]pyvcp.PinDef, len(p.pins))
	for i, pin := range p.pins {
		defs[i] = pyvcp.PinDef{
			Name:    pin.name,
			HalType: pyvcp.HalType(pin.halType),
			Dir:     pyvcp.PinDir(pin.dir),
		}
	}
	return &pyvcp.PanelInfo{
		Name: p.name,
		Xml:  p.xml,
		Pins: defs,
	}, nil
}

func (cb *pyvcpCallbacks) SetPin(panel string, name string, value string) (bool, error) {
	for _, pin := range cb.panel.pins {
		if pin.name == name {
			return pin.writeValue(value)
		}
	}
	return false, fmt.Errorf("pin %q not found", name)
}

// --- PyvcpWatchCallbacks implementation (WS watch) ---

func (cb *pyvcpCallbacks) WatchPins() ([]pyvcp.PinValue, error) {
	values := make([]pyvcp.PinValue, len(cb.panel.pins))
	for i, pin := range cb.panel.pins {
		values[i] = pyvcp.PinValue{
			Name:  pin.name,
			Value: pin.readValue(),
		}
	}
	return values, nil
}
