// Package realtime manages the LinuxCNC realtime environment (uspace only).
//
// This package corresponds to the uspace path of the legacy scripts/realtime.in
// bash script.  Kernel-module paths (RTAI, Xenomai) are intentionally not
// implemented here.
package realtime

import (
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"path/filepath"
	"time"

	"github.com/sittner/linuxcnc/src/launcher/config"
)

const (
	// defaultStopTimeout is how long Stop() waits for rtapi_app to exit before
	// sending SIGKILL.
	defaultStopTimeout = 10 * time.Second

	// rtapiAppName is the name of the realtime API application binary.
	rtapiAppName = "rtapi_app"

	// shmDev is the shared-memory device used by uspace rtapi.
	shmDev = "/dev/zero"
)

// Manager manages the LinuxCNC uspace realtime environment.
type Manager struct {
	logger      *slog.Logger
	rtapiAppPath string
	stopTimeout  time.Duration
}

// New returns a new Manager.  The rtapi_app binary is located in
// config.EMC2BinDir.  If logger is nil a default structured logger writing to
// stderr is used.
func New(logger *slog.Logger) *Manager {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(os.Stderr, nil))
	}
	return &Manager{
		logger:      logger,
		rtapiAppPath: filepath.Join(config.EMC2BinDir, rtapiAppName),
		stopTimeout:  defaultStopTimeout,
	}
}

// Start performs the realtime startup validation sequence (uspace).
//
// For uspace the actual module loading happens inside rtapi_app when halcmd
// executes loadrt.  Start() therefore only validates that the environment is
// sane and that rtapi_app is not already running.
func (m *Manager) Start() error {
	// 1. Sanity-check: /dev/zero must be accessible (mirrors CheckLoaded in
	//    realtime.in which waits for $SHM_DEV to become writable).
	if err := checkDevZero(); err != nil {
		return fmt.Errorf("realtime: %w", err)
	}

	// 2. Ensure rtapi_app is not already running.
	if m.IsRunning() {
		return fmt.Errorf("realtime: %s is already running", rtapiAppName)
	}

	// 3. Propagate debug level to rtapi if requested.
	if debug := os.Getenv("RTAPI_DEBUG"); debug != "" {
		m.logger.Debug("RTAPI_DEBUG set", "value", debug)
	}

	m.logger.Info("realtime environment ready (uspace)")
	return nil
}

// Stop performs the realtime shutdown sequence (uspace).
//
// It sends the 'exit' command to rtapi_app, waits up to the configured timeout
// for the process to exit, cleans up IPC resources, and force-kills the process
// if it has not exited within the timeout.
func (m *Manager) Stop() error {
	if !m.IsRunning() {
		m.logger.Debug("realtime: rtapi_app is not running, nothing to stop")
		return nil
	}

	// 1. Ask rtapi_app to exit cleanly.
	m.logger.Info("stopping realtime environment")
	exitCmd := exec.Command(m.rtapiAppPath, "exit")
	if out, err := exitCmd.CombinedOutput(); err != nil {
		// Non-fatal: rtapi_app exit may return non-zero even on success.
		m.logger.Debug("rtapi_app exit returned error", "error", err, "output", string(out))
	}

	// 2. Wait for rtapi_app to terminate.
	deadline := time.Now().Add(m.stopTimeout)
	for time.Now().Before(deadline) {
		if !m.IsRunning() {
			break
		}
		time.Sleep(100 * time.Millisecond)
	}

	// 3. Clean up IPC resources regardless of whether rtapi_app exited.
	if err := m.cleanupIPC(); err != nil {
		m.logger.Warn("IPC cleanup encountered errors", "error", err)
	}

	// 4. Force-kill if still running after timeout.
	if m.IsRunning() {
		m.logger.Error("rtapi_app did not exit within timeout, sending SIGKILL")
		if out, err := exec.Command("pkill", "-KILL", rtapiAppName).CombinedOutput(); err != nil {
			m.logger.Error("failed to SIGKILL rtapi_app", "error", err, "output", string(out))
			return fmt.Errorf("realtime: rtapi_app failed to exit and could not be killed: %w", err)
		}
	}

	m.logger.Info("realtime environment stopped")
	return nil
}

// IsRunning reports whether at least one non-zombie rtapi_app process is
// currently running.  It mirrors the CheckStatus function in realtime.in.
func (m *Manager) IsRunning() bool {
	// ps -C <name> exits 0 if the process exists.
	out, err := exec.Command("ps", "-C", rtapiAppName, "-o", "stat=", "-o", "comm=").Output()
	if err != nil || len(out) == 0 {
		return false
	}
	// Filter out zombie (Z) processes — same as `grep -v '^Z'` in realtime.in.
	for _, line := range splitLines(string(out)) {
		if len(line) > 0 && line[0] != 'Z' {
			return true
		}
	}
	return false
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
	// Closing /dev/zero (a character device) never returns a meaningful error;
	// we still check and discard it to satisfy the linter.
	_ = f.Close()
	return nil
}

// splitLines splits s on newlines and returns non-empty lines.
func splitLines(s string) []string {
	var lines []string
	start := 0
	for i := 0; i < len(s); i++ {
		if s[i] == '\n' {
			if line := s[start:i]; len(line) > 0 {
				lines = append(lines, line)
			}
			start = i + 1
		}
	}
	if tail := s[start:]; len(tail) > 0 {
		lines = append(lines, tail)
	}
	return lines
}
