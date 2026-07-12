// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package haljson registers the haljson HAL-to-JSON bridge module with the
// gomc module registry. It creates HAL pins from an XML configuration file and
// exposes them as structured JSON via REST (GET/POST) and WebSocket
// subscriptions.
//
// Usage in a HAL file:
//
//	load haljson config=my-panel.xml
//	load haljson [mypanel] config=my-panel.xml rate=50
//
// Parameters:
//   - config=<path> — path to the XML config file (required; resolved relative
//     to the INI file directory if not absolute)
//   - rate=<ms>     — default WS subscription rate in milliseconds (default: 50)
//
// The XML config file is preprocessed through Go's text/template engine before
// parsing, using the same context and functions as HAL files (.INI, .Axes,
// .Joints, .Env plus the ini/seq/range/hasJoint/hasAxis helpers). Files with no
// "{{" directives are passed through unchanged.
package haljson

import (
	"encoding/json"
	"fmt"
	"log/slog"
	"path/filepath"
	"strconv"
	"strings"
	"time"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	halparse "github.com/sittner/linuxcnc/src/gomc/internal/halparse"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("haljson", newHaljsonModule)
}

// haljsonModule implements gomc.Module.
type haljsonModule struct {
	logger *slog.Logger
	comp   *hal.Component
	roots  []*jsonRoot
}

func (m *haljsonModule) Start() error { return nil }
func (m *haljsonModule) Stop()        {}
func (m *haljsonModule) Destroy() {
	if m.comp != nil {
		if err := m.comp.Exit(); err != nil {
			m.logger.Debug("haljson HAL component exit error", "error", err)
		}
	}
}

// parseArgs extracts key=value parameters from the load command args.
func parseArgs(args []string) (configPath string, rateMS int) {
	rateMS = 50
	for _, arg := range args {
		k, v, ok := strings.Cut(arg, "=")
		if !ok {
			continue
		}
		switch k {
		case "config":
			configPath = v
		case "rate":
			if n, err := strconv.Atoi(v); err == nil && n > 0 {
				rateMS = n
			}
		}
	}
	return
}

func newHaljsonModule(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	configPath, rateMS := parseArgs(args)
	if configPath == "" {
		return nil, fmt.Errorf("haljson: missing required config= parameter")
	}

	// Resolve relative paths against the INI file directory, or the current
	// working directory when loaded without an INI (e.g. `gomc-server -f`).
	if !filepath.IsAbs(configPath) {
		iniDir := "."
		if ini != nil {
			iniDir = filepath.Dir(ini.SourceFile())
		}
		configPath = filepath.Join(iniDir, configPath)
	}

	logger = logger.With("module", "haljson", "instance", name)
	logger.Info("loading config", "path", configPath)

	// Parse the XML config file, rendering it through the Go text/template
	// engine with the same INI context that HAL files receive.
	var tmplData *halparse.HalTemplateData
	if ini != nil {
		tmplData = halparse.NewHalTemplateData(ini.AllSections())
	}
	roots, err := parseConfig(configPath, tmplData)
	if err != nil {
		return nil, fmt.Errorf("haljson %q: parsing config %q: %w", name, configPath, err)
	}

	// Create HAL component.
	comp, err := hal.NewComponent(name)
	if err != nil {
		return nil, fmt.Errorf("haljson %q: creating HAL component: %w", name, err)
	}

	// Export all HAL pins defined in the config.
	for _, root := range roots {
		if err := root.createPins(comp); err != nil {
			return nil, fmt.Errorf("haljson %q: creating pins: %w", name, err)
		}
	}

	if err := comp.Ready(); err != nil {
		return nil, fmt.Errorf("haljson %q: hal ready: %w", name, err)
	}

	// Register WebSocket watch API.
	if apiserver.DefaultWatchRegistry() == nil {
		apiserver.SetDefaultWatchRegistry(apiserver.NewWatchRegistry())
	}

	rate := time.Duration(rateMS) * time.Millisecond
	watches := make([]apiserver.WatchFuncMeta, 0, len(roots))
	for _, root := range roots {
		r := root // capture for closure
		watches = append(watches, apiserver.WatchFuncMeta{
			Name:        r.path,
			DefaultRate: rate,
			Factory:     newWatchFactory(r),
		})
	}

	// Register commands for writing to output pins.
	commands := make([]apiserver.CommandMeta, 0, len(roots))
	for _, root := range roots {
		r := root
		commands = append(commands, apiserver.CommandMeta{
			Name: r.path,
			Handler: func(req json.RawMessage) (json.RawMessage, error) {
				if err := r.applyJSON(req); err != nil {
					return nil, err
				}
				return []byte(`{"ok":true}`), nil
			},
		})
	}

	apiserver.DefaultWatchRegistry().Register(&apiserver.WatchAPI{
		APIName:  name,
		Instance: name,
		Watches:  watches,
		Commands: commands,
	})

	// Register REST API (GET to read pins, POST to write pins).
	meta := buildRESTMeta(name, roots)
	apiserver.RegisterMeta(meta)
	reg := apiserver.DefaultRegistry()
	if err := reg.Register(name, 1, name, unsafe.Pointer(&roots)); err != nil {
		return nil, fmt.Errorf("haljson %q: registering REST API: %w", name, err)
	}

	logger.Info("haljson initialized", "instance", name, "roots", len(roots), "rate_ms", rateMS)

	return &haljsonModule{
		logger: logger,
		comp:   comp,
		roots:  roots,
	}, nil
}
