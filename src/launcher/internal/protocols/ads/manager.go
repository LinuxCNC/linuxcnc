// Package ads implements the ADS protocol manager for the launcher.
//
// It reads [PROTOCOLS]ADS entries from the INI file, creates HAL components
// and pins for each ADS config file, and manages the lifecycle of the ADS
// TCP servers.
package ads

import (
	"fmt"
	"log/slog"
	"path/filepath"

	"github.com/sittner/linuxcnc/src/launcher/pkg/hal"

	adsProto "github.com/sittner/linuxcnc/src/launcher/pkg/ads"
	"github.com/sittner/linuxcnc/src/launcher/pkg/adsbridge"
	"github.com/sittner/linuxcnc/src/launcher/pkg/adsconfig"
	"github.com/sittner/linuxcnc/src/launcher/pkg/inifile"
)

// DefaultAMSPort is the default AMS port used for the ADS server.
const DefaultAMSPort = 851

// instance holds the state for a single ADS config file.
type instance struct {
	conf    *adsconfig.ServerConf
	comp    *hal.Component
	server  *adsProto.Server
	bridge  *adsbridge.Bridge
	symbols *adsProto.SymbolTable
}

// Manager implements protocols.Protocol for ADS.
type Manager struct {
	ini       *inifile.IniFile
	logger    *slog.Logger
	verbose   bool
	instances []*instance
}

// NewManager creates a new ADS protocol manager.
//
// The verbose flag enables debug logging on the ADS TCP servers.
func NewManager(ini *inifile.IniFile, logger *slog.Logger, verbose bool) *Manager {
	return &Manager{ini: ini, logger: logger, verbose: verbose}
}

// Init reads [PROTOCOLS]ADS entries from the INI file, parses each config
// file, creates a HAL component with pins for each, and registers all symbols
// in the ADS symbol table.
//
// After Init returns, all HAL pins are created and can be wired by HAL file
// net/addf/setp commands (which run after this in the launcher sequence).
func (m *Manager) Init() error {
	entries := m.ini.GetAll("PROTOCOLS", "ADS")
	if len(entries) == 0 {
		return nil
	}

	iniDir := filepath.Dir(m.ini.SourceFile())

	for _, entry := range entries {
		path := entry
		if !filepath.IsAbs(path) {
			path = filepath.Join(iniDir, path)
		}

		m.logger.Info("loading ADS config", "path", path)

		conf, aliases, tree, err := adsconfig.ParseConfFile(path)
		if err != nil {
			return fmt.Errorf("ADS config %q: %w", path, err)
		}

		// Compute layout (byte offsets for all symbols).
		pins, err := adsconfig.ComputeLayout(tree, aliases)
		if err != nil {
			return fmt.Errorf("ADS config %q: computing layout: %w", path, err)
		}

		// Create HAL component.
		comp, err := hal.NewComponent(conf.Name)
		if err != nil {
			return fmt.Errorf("ADS %q: creating HAL component: %w", conf.Name, err)
		}

		// Create symbol table and bridge (HAL pins + ADS symbol registrations).
		st := adsProto.NewSymbolTable()
		bridge, err := adsbridge.NewBridge(comp, pins, st, aliases)
		if err != nil {
			return fmt.Errorf("ADS %q: creating bridge: %w", conf.Name, err)
		}

		// Apply struct/enum type info to container groups.
		adsbridge.ApplyContainerTypeInfo(tree, "", st, aliases)

		// Mark HAL component ready so that HAL files can wire its pins.
		if err := comp.Ready(); err != nil {
			return fmt.Errorf("ADS %q: hal ready: %w", conf.Name, err)
		}

		// Parse AMS Net ID.
		netID, err := adsProto.ParseAMSNetID(conf.AMSNetID)
		if err != nil {
			return fmt.Errorf("ADS %q: invalid AMS Net ID %q: %w", conf.Name, conf.AMSNetID, err)
		}

		// Create ADS TCP server (not started yet).
		addr := fmt.Sprintf("%s:%d", conf.Bind, conf.Port)
		server := adsProto.NewServer(addr, netID, DefaultAMSPort, st, m.verbose)

		m.instances = append(m.instances, &instance{
			conf:    conf,
			comp:    comp,
			server:  server,
			bridge:  bridge,
			symbols: st,
		})

		m.logger.Info("ADS instance initialized",
			"name", conf.Name,
			"addr", addr,
			"ams-net-id", conf.AMSNetID,
			"pins", len(pins),
		)
	}

	return nil
}

// Start opens TCP listeners for all ADS instances.
func (m *Manager) Start() error {
	for _, inst := range m.instances {
		if err := inst.server.Start(); err != nil {
			return fmt.Errorf("ADS %q: start server: %w", inst.conf.Name, err)
		}
		m.logger.Info("ADS server started", "name", inst.conf.Name)
	}
	return nil
}

// Stop shuts down all ADS TCP servers and exits HAL components.
func (m *Manager) Stop() {
	for _, inst := range m.instances {
		m.logger.Debug("stopping ADS server", "name", inst.conf.Name)
		inst.server.Stop()
		if err := inst.comp.Exit(); err != nil {
			m.logger.Debug("ADS HAL component exit error", "name", inst.conf.Name, "error", err)
		}
	}
}
