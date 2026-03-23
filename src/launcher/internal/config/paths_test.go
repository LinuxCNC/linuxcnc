package config_test

import (
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/internal/config"
)

// TestPathsDefaultValues verifies that compile-time path variables default to
// empty strings when not set via -ldflags.
func TestPathsDefaultValues(t *testing.T) {
	// Without -ldflags, all variables should be empty strings.
	vars := map[string]string{
		"EMC2Home":       config.EMC2Home,
		"EMC2BinDir":     config.EMC2BinDir,
		"EMC2TclDir":     config.EMC2TclDir,
		"EMC2HelpDir":    config.EMC2HelpDir,
		"EMC2RtlibDir":   config.EMC2RtlibDir,
		"EMC2ConfigPath": config.EMC2ConfigPath,
		"EMC2NCFilesDir": config.EMC2NCFilesDir,
		"EMC2LangDir":    config.EMC2LangDir,
		"EMC2ImageDir":   config.EMC2ImageDir,
		"EMC2TclLibDir":  config.EMC2TclLibDir,
		"HalibDir":       config.HalibDir,
		"EMC2Version":    config.EMC2Version,
		"RunInPlace":     config.RunInPlace,
		"DefaultNmlFile": config.DefaultNmlFile,
		"ModExt":         config.ModExt,
		"KernelVers":     config.KernelVers,
	}
	for name, val := range vars {
		if val != "" {
			t.Errorf("config.%s = %q, want empty string (not set via -ldflags)", name, val)
		}
	}
}
