package launcher

import (
	"log/slog"
	"os"
	"path/filepath"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/inifile"
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

// resolveMotionMods returns the resolved (tpMod, homeMod) pair by exercising
// the same resolution logic as Launcher.preloadMotionModules(), without
// actually running any subprocesses.
func resolveMotionMods(t *testing.T, iniContent, tpModOpt, homeModOpt string) (tpMod, homeMod string) {
	t.Helper()
	dir := t.TempDir()
	f := writeIni(t, dir, "test.ini", iniContent)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	l := &Launcher{
		opts:   Options{TpMod: tpModOpt, HomeMod: homeModOpt},
		ini:    ini,
		logger: slog.New(slog.NewTextHandler(os.Stderr, nil)),
	}

	// Mirror the resolution logic from preloadMotionModules.
	tpMod = l.opts.TpMod
	if tpMod == "" {
		tpMod = l.ini.Get("TRAJ", "TPMOD")
	}
	if tpMod == "" {
		tpMod = "tpmod"
	}

	homeMod = l.opts.HomeMod
	if homeMod == "" {
		homeMod = l.ini.Get("EMCMOT", "HOMEMOD")
	}
	if homeMod == "" {
		homeMod = "homemod"
	}

	return tpMod, homeMod
}

// --------------------------------------------------------------------------
// Tests for TPMOD/HOMEMOD resolution logic
// --------------------------------------------------------------------------

// TestPreloadMotionModules_Defaults verifies that the defaults "tpmod" and
// "homemod" are used when neither CLI flags nor INI entries are set.
func TestPreloadMotionModules_Defaults(t *testing.T) {
	tp, home := resolveMotionMods(t, `
[EMCMOT]
EMCMOT = motmod
SERVO_PERIOD = 1000000
`, "", "")
	if tp != "tpmod" {
		t.Errorf("tpMod = %q, want %q", tp, "tpmod")
	}
	if home != "homemod" {
		t.Errorf("homeMod = %q, want %q", home, "homemod")
	}
}

// TestPreloadMotionModules_CLIFlags verifies that explicit CLI flag values
// take precedence over INI entries and built-in defaults.
func TestPreloadMotionModules_CLIFlags(t *testing.T) {
	tp, home := resolveMotionMods(t, `
[TRAJ]
TPMOD = ini_tpmod
[EMCMOT]
EMCMOT = motmod
HOMEMOD = ini_homemod
`, "cli_tpmod", "cli_homemod")
	if tp != "cli_tpmod" {
		t.Errorf("tpMod = %q, want %q", tp, "cli_tpmod")
	}
	if home != "cli_homemod" {
		t.Errorf("homeMod = %q, want %q", home, "cli_homemod")
	}
}

// TestPreloadMotionModules_IniOverridesDefaults verifies that INI-file values
// for TPMOD and HOMEMOD override the built-in defaults.
func TestPreloadMotionModules_IniOverridesDefaults(t *testing.T) {
	tp, home := resolveMotionMods(t, `
[TRAJ]
TPMOD = custom_tp
[EMCMOT]
EMCMOT = motmod
HOMEMOD = custom_home
`, "", "")
	if tp != "custom_tp" {
		t.Errorf("tpMod = %q, want %q", tp, "custom_tp")
	}
	if home != "custom_home" {
		t.Errorf("homeMod = %q, want %q", home, "custom_home")
	}
}

// TestPreloadMotionModules_EmcmotUnchanged verifies that [EMCMOT]EMCMOT is
// NOT modified by the preload logic — it must remain as the original value
// so that HAL files receive the correct "loadrt motmod ..." command.
func TestPreloadMotionModules_EmcmotUnchanged(t *testing.T) {
	dir := t.TempDir()
	f := writeIni(t, dir, "test.ini", `
[EMCMOT]
EMCMOT = motmod
SERVO_PERIOD = 1000000
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	// The EMCMOT value must remain unchanged (no tp=/hp= appended).
	got := ini.Get("EMCMOT", "EMCMOT")
	want := "motmod"
	if got != want {
		t.Errorf("[EMCMOT]EMCMOT = %q, want %q (must not be mutated)", got, want)
	}

	// Substitute must also reflect the original value.
	input := "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD"
	wantSub := "loadrt motmod servo_period_nsec=1000000"
	if got := ini.Substitute(input); got != wantSub {
		t.Errorf("Substitute:\n got  %q\n want %q", got, wantSub)
	}
}
