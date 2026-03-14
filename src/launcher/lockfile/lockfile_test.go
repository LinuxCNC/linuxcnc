package lockfile_test

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/lockfile"
)

// setTempLockPath overrides LockFilePath for the duration of a test and
// restores it automatically via t.Cleanup.
func setTempLockPath(t *testing.T) {
	t.Helper()
	old := lockfile.LockFilePath
	lockfile.LockFilePath = filepath.Join(t.TempDir(), "linuxcnc.lock")
	t.Cleanup(func() { lockfile.LockFilePath = old })
}

func TestAcquireRelease(t *testing.T) {
	setTempLockPath(t)

	if err := lockfile.Acquire(); err != nil {
		t.Fatalf("Acquire: %v", err)
	}

	// Lock file should now exist.
	if _, err := os.Stat(lockfile.LockFilePath); err != nil {
		t.Errorf("lock file not found after Acquire: %v", err)
	}

	if err := lockfile.Release(); err != nil {
		t.Fatalf("Release: %v", err)
	}

	// Lock file should be gone after Release.
	if _, err := os.Stat(lockfile.LockFilePath); !os.IsNotExist(err) {
		t.Errorf("lock file still exists after Release")
	}
}

func TestReleaseIdempotent(t *testing.T) {
	setTempLockPath(t)
	// Release on a non-existent lock file should not error.
	if err := lockfile.Release(); err != nil {
		t.Fatalf("Release (no file): %v", err)
	}
}

func TestAcquireTwice(t *testing.T) {
	setTempLockPath(t)

	if err := lockfile.Acquire(); err != nil {
		t.Fatalf("first Acquire: %v", err)
	}
	t.Cleanup(func() { _ = lockfile.Release() })

	// Second Acquire when stdin is not a TTY (CI environment) should
	// auto-cleanup and succeed.
	if err := lockfile.Acquire(); err != nil {
		t.Fatalf("second Acquire (auto-cleanup): %v", err)
	}
}
