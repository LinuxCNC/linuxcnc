// Package mqttbridge registers the MQTT bridge module with the gomc module
// registry. It creates HAL pins from an XML config and bridges them
// bidirectionally to an MQTT broker.
//
// Usage in a HAL file:
//
//	load mqtt-bridge config=mqtt.xml
//	load mqtt-bridge [mqtt] config=mqtt.xml
//
// Parameters:
//   - config=<path> — path to the XML config file (required; resolved relative
//     to the INI file directory if not absolute)
package mqttbridge

import (
	"fmt"
	"log/slog"
	"path/filepath"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("mqtt-bridge", newMQTTBridge)
}

// mqttModule implements gomc.Module.
type mqttModule struct {
	logger *slog.Logger
	comp   *hal.Component
	bridge *bridge
}

func (m *mqttModule) Start() error {
	return m.bridge.start()
}

func (m *mqttModule) Stop() {
	m.bridge.stop()
}

func (m *mqttModule) Destroy() {
	if m.comp != nil {
		if err := m.comp.Exit(); err != nil {
			m.logger.Debug("mqtt-bridge HAL component exit error", "error", err)
		}
	}
}

func newMQTTBridge(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	configPath := parseConfigArg(args)
	if configPath == "" {
		return nil, fmt.Errorf("mqtt-bridge: missing required config= parameter")
	}

	// Resolve relative paths against the INI file directory.
	if !filepath.IsAbs(configPath) {
		iniDir := filepath.Dir(ini.SourceFile())
		configPath = filepath.Join(iniDir, configPath)
	}

	logger = logger.With("module", name)
	logger.Info("loading MQTT bridge config", "path", configPath)

	cfg, err := parseConfig(configPath)
	if err != nil {
		return nil, fmt.Errorf("mqtt-bridge config %q: %w", configPath, err)
	}

	// Create HAL component.
	comp, err := hal.NewComponent(name)
	if err != nil {
		return nil, fmt.Errorf("mqtt-bridge %q: creating HAL component: %w", name, err)
	}

	// Create bridge (pins + MQTT wiring).
	b, err := newBridge(comp, name, cfg, logger)
	if err != nil {
		comp.Exit()
		return nil, fmt.Errorf("mqtt-bridge %q: %w", name, err)
	}

	// Mark HAL component ready.
	if err := comp.Ready(); err != nil {
		comp.Exit()
		return nil, fmt.Errorf("mqtt-bridge %q: hal ready: %w", name, err)
	}

	logger.Info("mqtt-bridge initialized",
		"broker", cfg.Broker,
		"topics", len(cfg.Topics),
	)

	return &mqttModule{
		logger: logger,
		comp:   comp,
		bridge: b,
	}, nil
}

func parseConfigArg(args []string) string {
	for _, arg := range args {
		k, v, ok := strings.Cut(arg, "=")
		if ok && k == "config" {
			return v
		}
	}
	return ""
}
