// Package adsmodule registers the ADS/AMS protocol server module with the gomc
// module registry. When compiled into the gomc-server binary, this package's
// init() function registers a factory that creates ADS server instances in
// response to HAL "load ads-server" commands.
//
// Usage in a HAL file:
//
//	load ads-server [ads] config=galv-hmi.conf
//	load ads-server [ads] config=galv-hmi.conf debug=1
//
// Parameters:
//   - config=<path>  — path to the ADS .conf file (required; resolved relative
//     to the INI file directory if not absolute)
//   - debug=1        — enable verbose ADS protocol logging
package adsmodule

import (
	"fmt"
	"log/slog"
	"path/filepath"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/internal/ads"
	"github.com/sittner/linuxcnc/src/gomc/internal/adsbridge"
	"github.com/sittner/linuxcnc/src/gomc/internal/adsconfig"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// DefaultAMSPort is the default AMS port used for the ADS server.
const DefaultAMSPort = 851

func init() {
	gomc.RegisterModule("ads-server", newADSModule)
}

// adsModule implements gomc.Module for the ADS server.
type adsModule struct {
	logger  *slog.Logger
	comp    *hal.Component
	server  *ads.Server
	bridge  *adsbridge.Bridge
	symbols *ads.SymbolTable
	conf    *adsconfig.ServerConf
}

func (m *adsModule) Start() error {
	if err := m.server.Start(); err != nil {
		return fmt.Errorf("ADS %q: start server: %w", m.conf.Name, err)
	}
	m.logger.Info("ADS server started", "name", m.conf.Name)
	return nil
}

func (m *adsModule) Stop() {
	m.logger.Debug("stopping ADS server", "name", m.conf.Name)
	m.server.Stop()
}

func (m *adsModule) Destroy() {
	if m.comp != nil {
		if err := m.comp.Exit(); err != nil {
			m.logger.Debug("ADS HAL component exit error", "name", m.conf.Name, "error", err)
		}
	}
}

// parseArgs extracts key=value parameters from the load command args.
func parseArgs(args []string) (configPath string, debug bool) {
	for _, arg := range args {
		k, v, ok := strings.Cut(arg, "=")
		if !ok {
			continue
		}
		switch k {
		case "config":
			configPath = v
		case "debug":
			debug = v == "1"
		}
	}
	return
}

func newADSModule(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	configPath, debug := parseArgs(args)
	if configPath == "" {
		return nil, fmt.Errorf("ads-server: missing required config= parameter")
	}

	// Resolve relative paths against the INI file directory.
	if !filepath.IsAbs(configPath) {
		iniDir := filepath.Dir(ini.SourceFile())
		configPath = filepath.Join(iniDir, configPath)
	}

	logger = logger.With("plugin", name)
	logger.Info("loading ADS config", "path", configPath)

	conf, aliases, tree, err := adsconfig.ParseConfFile(configPath)
	if err != nil {
		return nil, fmt.Errorf("ADS config %q: %w", configPath, err)
	}

	// Override the conf name with the instance name from the load command.
	conf.Name = name

	// Compute layout (byte offsets for all symbols).
	pins, err := adsconfig.ComputeLayout(tree, aliases)
	if err != nil {
		return nil, fmt.Errorf("ADS config %q: computing layout: %w", configPath, err)
	}

	// Create HAL component.
	comp, err := hal.NewComponent(name)
	if err != nil {
		return nil, fmt.Errorf("ADS %q: creating HAL component: %w", name, err)
	}

	// Create symbol table and bridge (HAL pins + ADS symbol registrations).
	st := ads.NewSymbolTable()
	bridge, err := adsbridge.NewBridge(comp, pins, st, aliases)
	if err != nil {
		return nil, fmt.Errorf("ADS %q: creating bridge: %w", name, err)
	}

	// Apply struct/enum type info to container groups.
	adsbridge.ApplyContainerTypeInfo(tree, "", st, aliases)

	// Mark HAL component ready so that HAL files can wire its pins.
	if err := comp.Ready(); err != nil {
		return nil, fmt.Errorf("ADS %q: hal ready: %w", name, err)
	}

	// Parse AMS Net ID.
	netID, err := ads.ParseAMSNetID(conf.AMSNetID)
	if err != nil {
		return nil, fmt.Errorf("ADS %q: invalid AMS Net ID %q: %w", name, conf.AMSNetID, err)
	}

	// Create ADS TCP server (not started yet).
	addr := fmt.Sprintf("%s:%d", conf.Bind, conf.Port)
	server := ads.NewServer(addr, netID, DefaultAMSPort, st, debug, logger)

	logger.Info("ADS instance initialized",
		"name", name,
		"addr", addr,
		"ams-net-id", conf.AMSNetID,
		"pins", len(pins),
	)

	return &adsModule{
		logger:  logger,
		comp:    comp,
		server:  server,
		bridge:  bridge,
		symbols: st,
		conf:    conf,
	}, nil
}
