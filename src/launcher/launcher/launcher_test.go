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

// resolveEmcmot returns the [EMCMOT]EMCMOT value after TPMOD/HOMEMOD
// injection, by exercising the same logic as Launcher.Run().
func resolveEmcmot(t *testing.T, iniContent, tpModOpt, homeModOpt string) string {
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
	l.injectEmcmotModules()
	return ini.Get("EMCMOT", "EMCMOT")
}

// --------------------------------------------------------------------------
// Tests for TPMOD/HOMEMOD injection
// --------------------------------------------------------------------------

// TestInjectEmcmotModules_Defaults verifies that the defaults "tpmod" and
// "homemod" are appended when neither CLI flags nor INI entries are set.
func TestInjectEmcmotModules_Defaults(t *testing.T) {
	got := resolveEmcmot(t, `
[EMCMOT]
EMCMOT = motmod
SERVO_PERIOD = 1000000
`, "", "")
	want := "motmod tp=tpmod hp=homemod"
	if got != want {
		t.Errorf("EMCMOT = %q, want %q", got, want)
	}
}

// TestInjectEmcmotModules_CLIFlags verifies that explicit CLI flag values
// take precedence over INI entries and built-in defaults.
func TestInjectEmcmotModules_CLIFlags(t *testing.T) {
	got := resolveEmcmot(t, `
[TRAJ]
TPMOD = ini_tpmod
[EMCMOT]
EMCMOT = motmod
HOMEMOD = ini_homemod
`, "cli_tpmod", "cli_homemod")
	want := "motmod tp=cli_tpmod hp=cli_homemod"
	if got != want {
		t.Errorf("EMCMOT = %q, want %q", got, want)
	}
}

// TestInjectEmcmotModules_IniOverridesDefaults verifies that INI-file values
// for TPMOD and HOMEMOD override the built-in defaults.
func TestInjectEmcmotModules_IniOverridesDefaults(t *testing.T) {
	got := resolveEmcmot(t, `
[TRAJ]
TPMOD = custom_tp
[EMCMOT]
EMCMOT = motmod
HOMEMOD = custom_home
`, "", "")
	want := "motmod tp=custom_tp hp=custom_home"
	if got != want {
		t.Errorf("EMCMOT = %q, want %q", got, want)
	}
}

// TestInjectEmcmotModules_SubstituteReflectsUpdate verifies that
// ini.Substitute() returns the updated EMCMOT value after injection.
func TestInjectEmcmotModules_SubstituteReflectsUpdate(t *testing.T) {
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
	l := &Launcher{
		opts:   Options{},
		ini:    ini,
		logger: slog.New(slog.NewTextHandler(os.Stderr, nil)),
	}
	l.injectEmcmotModules()

	input := "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD"
	want := "loadrt motmod tp=tpmod hp=homemod servo_period_nsec=1000000"
	if got := ini.Substitute(input); got != want {
		t.Errorf("Substitute after inject:\n got  %q\n want %q", got, want)
	}
}
