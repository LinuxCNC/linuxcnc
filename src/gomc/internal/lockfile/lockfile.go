// Package lockfile manages the LinuxCNC lock file (/tmp/linuxcnc.lock).
//
// The lock file prevents multiple concurrent LinuxCNC instances from running.
// flock(LOCK_EX|LOCK_NB) is used so that the kernel automatically releases
// the lock when the holding process exits — even on kill -9.
package lockfile

import (
	"fmt"
	"os"
	"syscall"
)

// LockFilePath is the canonical path of the LinuxCNC lock file.
// It is a var (not const) so tests can override it with a temp path.
var LockFilePath = "/tmp/linuxcnc.lock"

// LockFile represents an exclusive lock backed by an OS file.
type LockFile struct {
	path string
	file *os.File
}

// New returns a new LockFile for the given path.
func New(path string) *LockFile {
	return &LockFile{path: path}
}

// Acquire opens/creates the lock file, applies flock(LOCK_EX|LOCK_NB),
// and writes the current PID. If another instance holds the lock,
// an error is returned immediately — no stale-lock handling needed because
// flock is auto-released by the kernel when the process exits.
func (l *LockFile) Acquire() error {
	f, err := os.OpenFile(l.path, os.O_CREATE|os.O_RDWR, 0666)
	if err != nil {
		return fmt.Errorf("open lock file: %w", err)
	}

	if err := syscall.Flock(int(f.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
		f.Close()
		return fmt.Errorf("another LinuxCNC instance is running (lock held on %s)", l.path)
	}

	// Write our PID (truncate first in case the file had old content).
	// Errors here are intentionally ignored: the PID content is informational
	// only and does not affect locking correctness.
	_ = f.Truncate(0)
	_, _ = f.Seek(0, 0)
	fmt.Fprintf(f, "%d\n", os.Getpid())
	f.Sync()

	l.file = f
	return nil
}

// Release releases the flock and removes the PID file.
func (l *LockFile) Release() error {
	if l.file == nil {
		return nil
	}
	// flock is released automatically when the file is closed, but be explicit.
	syscall.Flock(int(l.file.Fd()), syscall.LOCK_UN)
	l.file.Close()
	os.Remove(l.path)
	l.file = nil
	return nil
}
