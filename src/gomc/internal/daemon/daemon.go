// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package daemon provides daemonization, PID file management, and syslog logging.
package daemon

import (
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"syscall"
)

// DefaultPidFile is the default path for the PID file when -daemon is used
// without specifying a path.
const DefaultPidFile = "/var/run/gomc-server.pid"

// envSentinel is the environment variable used to detect the re-execed child.
const envSentinel = "_GOMC_DAEMONIZED"

// IsChild returns true if this process is the daemonized child (re-execed).
func IsChild() bool {
	return os.Getenv(envSentinel) == "1"
}

// Daemonize re-execs the current process in the background and writes the
// child PID to the given pidFile. The parent process exits after the child
// has started. This function only returns in the child process.
func Daemonize(pidFile string) error {
	if IsChild() {
		// We are the child — remove the sentinel and continue.
		os.Unsetenv(envSentinel)
		return writePidFile(pidFile)
	}

	// Parent: re-exec ourselves with the sentinel set.
	exe, err := os.Executable()
	if err != nil {
		return fmt.Errorf("daemon: resolving executable: %w", err)
	}

	cmd := exec.Command(exe, os.Args[1:]...)
	cmd.Env = append(os.Environ(), envSentinel+"=1")
	cmd.Stdin = nil
	cmd.Stdout = nil
	cmd.Stderr = nil
	cmd.SysProcAttr = &syscall.SysProcAttr{Setsid: true}

	if err := cmd.Start(); err != nil {
		return fmt.Errorf("daemon: starting child: %w", err)
	}

	// Write the child's PID to the pidfile from the parent, then exit.
	pid := cmd.Process.Pid
	if err := os.WriteFile(pidFile, []byte(strconv.Itoa(pid)+"\n"), 0644); err != nil {
		// Best effort — child is already running.
		fmt.Fprintf(os.Stderr, "daemon: writing pid file: %v\n", err)
	}

	os.Exit(0)
	return nil // unreachable
}

// writePidFile writes the current process PID to the file (called in the child).
func writePidFile(path string) error {
	return os.WriteFile(path, []byte(strconv.Itoa(os.Getpid())+"\n"), 0644)
}

// RemovePidFile removes the PID file on clean shutdown.
func RemovePidFile(path string) {
	os.Remove(path)
}

// RedirectStdio redirects stdin, stdout, stderr to /dev/null (for daemon mode).
func RedirectStdio() error {
	devNull, err := os.OpenFile(os.DevNull, os.O_RDWR, 0)
	if err != nil {
		return err
	}
	syscall.Dup2(int(devNull.Fd()), int(os.Stdin.Fd()))
	syscall.Dup2(int(devNull.Fd()), int(os.Stdout.Fd()))
	syscall.Dup2(int(devNull.Fd()), int(os.Stderr.Fd()))
	devNull.Close()
	return nil
}
