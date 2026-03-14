package lockfile_test

import (
	"path/filepath"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/lockfile"
)

// newTestLock creates a LockFile pointing at a temp path and registers cleanup.
func newTestLock(t *testing.T) *lockfile.LockFile {
	t.Helper()
	path := filepath.Join(t.TempDir(), "linuxcnc.lock")
	lf := lockfile.New(path)
	t.Cleanup(func() { _ = lf.Release() })
	return lf
}

func TestAcquireRelease(t *testing.T) {
	lf := lockfile.New(filepath.Join(t.TempDir(), "linuxcnc.lock"))

	if err := lf.Acquire(); err != nil {
		t.Fatalf("Acquire: %v", err)
	}

	if err := lf.Release(); err != nil {
		t.Fatalf("Release: %v", err)
	}
}

func TestReleaseIdempotent(t *testing.T) {
	lf := lockfile.New(filepath.Join(t.TempDir(), "linuxcnc.lock"))
	// Release on a never-acquired lock should not error.
	if err := lf.Release(); err != nil {
		t.Fatalf("Release (no file): %v", err)
	}
	// Second Release is also safe.
	if err := lf.Release(); err != nil {
		t.Fatalf("Release (second): %v", err)
	}
}

func TestAcquireTwice_SameProcess(t *testing.T) {
	path := filepath.Join(t.TempDir(), "linuxcnc.lock")

	lf1 := lockfile.New(path)
	if err := lf1.Acquire(); err != nil {
		t.Fatalf("first Acquire: %v", err)
	}
	defer lf1.Release()

	// Second Acquire on the same path from the same process must fail
	// because flock(LOCK_EX|LOCK_NB) rejects concurrent open-file-descriptions
	// holding the lock.
	lf2 := lockfile.New(path)
	if err := lf2.Acquire(); err == nil {
		defer lf2.Release()
		t.Fatal("second Acquire succeeded — expected error (lock already held)")
	}
}

func TestAcquireAfterRelease(t *testing.T) {
	path := filepath.Join(t.TempDir(), "linuxcnc.lock")

	lf := lockfile.New(path)
	if err := lf.Acquire(); err != nil {
		t.Fatalf("first Acquire: %v", err)
	}
	if err := lf.Release(); err != nil {
		t.Fatalf("Release: %v", err)
	}

	// After Release the lock is gone; a fresh Acquire must succeed.
	lf2 := lockfile.New(path)
	if err := lf2.Acquire(); err != nil {
		t.Fatalf("re-Acquire after Release: %v", err)
	}
	defer lf2.Release()
}
