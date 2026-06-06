// Package realtime manages the LinuxCNC realtime environment (uspace only).
//
// This package corresponds to the uspace path of the legacy scripts/realtime.in
// bash script.  Kernel-module paths (RTAI, Xenomai) are intentionally not
// implemented here.
//
// RT module loading happens in-process via dlopen in the halcmd CGo shims.
// This package only handles environment validation on startup.
package realtime

import (
	"fmt"
	"log/slog"
	"os"
)

const (
	// shmDev is the shared-memory device used by uspace rtapi.
	shmDev = "/dev/zero"
)

// Manager manages the LinuxCNC uspace realtime environment.
type Manager struct {
	logger *slog.Logger
}

// New returns a new Manager.  If logger is nil a default structured logger
// writing to stderr is used.
func New(logger *slog.Logger) *Manager {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(os.Stderr, nil))
	}
	return &Manager{
		logger: logger,
	}
}

// Start performs the realtime startup validation sequence (uspace).
//
// RT module loading now happens in-process via dlopen when halcmd.LoadRT()
// is called.  Start() validates that the environment is sane.
func (m *Manager) Start() error {
	// Sanity-check: /dev/zero must be accessible (mirrors CheckLoaded in
	// realtime.in which waits for $SHM_DEV to become writable).
	if err := checkDevZero(); err != nil {
		return fmt.Errorf("realtime: %w", err)
	}

	// Propagate debug level to rtapi if requested.
	if debug := os.Getenv("RTAPI_DEBUG"); debug != "" {
		m.logger.Debug("RTAPI_DEBUG set", "value", debug)
	}

	m.logger.Info("realtime environment ready (uspace)")
	return nil
}

// checkDevZero verifies that /dev/zero is accessible.
func checkDevZero() error {
	return checkDevZeroAt(shmDev)
}

// checkDevZeroAt verifies that the given path is accessible.
// It is a separate function to allow testing with arbitrary paths.
func checkDevZeroAt(path string) error {
	f, err := os.Open(path)
	if err != nil {
		return fmt.Errorf("cannot open %s: %w", path, err)
	}
	_ = f.Close()
	return nil
}
