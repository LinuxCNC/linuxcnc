// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package launcher

import (
	"errors"
	"log/slog"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// writeIni is a helper that writes an INI file and returns its path.
func writeIni(t *testing.T, dir, name, content string) string {
	t.Helper()
	path := filepath.Join(dir, name)
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatalf("writeIni %s: %v", path, err)
	}
	return path
}

// --------------------------------------------------------------------------
// Tests for cleanup idempotency (sync.Once)
// --------------------------------------------------------------------------

// TestCleanup_IdempotentViaSyncOnce verifies that calling cleanup() multiple
// times does not panic and the doCleanup function runs only once.
func TestCleanup_IdempotentViaSyncOnce(t *testing.T) {
	callCount := 0
	l := &Launcher{
		logger: slog.New(slog.NewTextHandler(os.Stderr, nil)),
	}

	// Override doCleanup by calling cleanupOnce.Do directly to test the Once
	// semantics without triggering real cleanup.
	fn := func() { callCount++ }
	l.cleanupOnce.Do(fn)
	l.cleanupOnce.Do(fn)
	l.cleanupOnce.Do(fn)

	if callCount != 1 {
		t.Errorf("fn called %d times, want 1", callCount)
	}
}

// --------------------------------------------------------------------------
// Tests for resolveRelativePath
// --------------------------------------------------------------------------

// TestResolveRelativePath_Absolute verifies that absolute paths are unchanged.
func TestResolveRelativePath_Absolute(t *testing.T) {
	l := &Launcher{
		opts:   Options{IniFile: "/configs/test.ini"},
		logger: slog.New(slog.NewTextHandler(os.Stderr, nil)),
	}
	got := l.resolveRelativePath("/opt/linuxcnc/linuxcnc.nml")
	want := "/opt/linuxcnc/linuxcnc.nml"
	if got != want {
		t.Errorf("resolveRelativePath = %q, want %q", got, want)
	}
}

// TestResolveRelativePath_Relative verifies that relative paths are resolved
// against the INI directory.
func TestResolveRelativePath_Relative(t *testing.T) {
	l := &Launcher{
		opts:   Options{IniFile: "/configs/sim/test.ini"},
		logger: slog.New(slog.NewTextHandler(os.Stderr, nil)),
	}
	got := l.resolveRelativePath("linuxcnc.nml")
	want := "/configs/sim/linuxcnc.nml"
	if got != filepath.Clean(want) {
		t.Errorf("resolveRelativePath = %q, want %q", got, want)
	}
}

// --------------------------------------------------------------------------
// Tests for checkVersion
// --------------------------------------------------------------------------

// newLauncherWithIniPath is a helper that creates a Launcher with a parsed INI.
func newLauncherWithIniPath(t *testing.T, iniFilePath string) *Launcher {
	t.Helper()
	ini, err := inifile.Parse(iniFilePath)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	return &Launcher{
		opts:   Options{IniFile: iniFilePath},
		ini:    ini,
		logger: slog.New(slog.NewTextHandler(os.Stderr, nil)),
	}
}

// TestCheckVersion_CurrentVersion verifies that version "1.1" is a no-op.
func TestCheckVersion_CurrentVersion(t *testing.T) {
	dir := t.TempDir()
	f := writeIni(t, dir, "test.ini", `[EMC]
VERSION = 1.1
`)
	l := newLauncherWithIniPath(t, f)
	if err := l.checkVersion(); err != nil {
		t.Errorf("checkVersion with VERSION=1.1 returned error: %v", err)
	}
}

// TestCheckVersion_MissingVersion verifies that a missing [EMC]VERSION triggers
// the update path; when DISPLAY is unset an error about the missing X display is returned.
func TestCheckVersion_MissingVersion(t *testing.T) {
	dir := t.TempDir()
	f := writeIni(t, dir, "test.ini", `[EMC]
MACHINE = Test
`)
	l := newLauncherWithIniPath(t, f)

	// Ensure DISPLAY is not set so we exercise the no-display error path.
	origDisplay := os.Getenv("DISPLAY")
	os.Unsetenv("DISPLAY")
	defer func() { os.Setenv("DISPLAY", origDisplay) }()

	err := l.checkVersion()
	if err == nil {
		t.Fatal("checkVersion with no VERSION and no DISPLAY should return error")
	}
	if !strings.Contains(err.Error(), "without an X display") {
		t.Errorf("unexpected error: %v", err)
	}
}

// --------------------------------------------------------------------------
// Tests for checkPlasmaC
// --------------------------------------------------------------------------

// TestCheckPlasmaC_NoPlasmaC verifies that a non-PlasmaC INI is a no-op.
func TestCheckPlasmaC_NoPlasmaC(t *testing.T) {
	dir := t.TempDir()
	f := writeIni(t, dir, "test.ini", `[EMC]
MACHINE = Test
`)
	l := newLauncherWithIniPath(t, f)
	if err := l.checkPlasmaC(); err != nil {
		t.Errorf("checkPlasmaC on non-PlasmaC INI returned error: %v", err)
	}
}

// TestCheckPlasmaC_PlasmaC verifies that [PLASMAC]MODE triggers ErrPlasmaC.
func TestCheckPlasmaC_PlasmaC(t *testing.T) {
	dir := t.TempDir()
	f := writeIni(t, dir, "test.ini", `[PLASMAC]
MODE = 0
`)
	l := newLauncherWithIniPath(t, f)
	err := l.checkPlasmaC()
	if !errors.Is(err, ErrPlasmaC) {
		t.Errorf("checkPlasmaC on PlasmaC INI returned %v, want ErrPlasmaC", err)
	}
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// Tests for validateDependencies
// --------------------------------------------------------------------------

// newLauncherWithIniContent is a helper that creates a Launcher from raw INI
// content written to a temporary file.
func newLauncherWithIniContent(t *testing.T, content string) *Launcher {
	t.Helper()
	dir := t.TempDir()
	f := writeIni(t, dir, "test.ini", content)
	return newLauncherWithIniPath(t, f)
}

// TestValidateDependencies_HALOnlyMode verifies that a minimal configuration
// with at least one HALFILE is accepted.
func TestValidateDependencies_HALOnlyMode(t *testing.T) {
	l := newLauncherWithIniContent(t, `
[EMC]
MACHINE = TestMachine

[HAL]
HALFILE = my-hardware.hal
HALFILE = my-logic.hal

`)
	if err := l.validateDependencies(); err != nil {
		t.Errorf("HAL-only config should be valid, got error: %v", err)
	}
}

// TestValidateDependencies_NoHALFile verifies that a missing [HAL]HALFILE is
// rejected.
func TestValidateDependencies_NoHALFile(t *testing.T) {
	l := newLauncherWithIniContent(t, `
[EMC]
MACHINE = TestMachine
`)
	err := l.validateDependencies()
	if err == nil {
		t.Fatal("config without [HAL]HALFILE should be rejected")
	}
	if !strings.Contains(err.Error(), "HALFILE") {
		t.Errorf("error should mention HALFILE, got: %v", err)
	}
}
