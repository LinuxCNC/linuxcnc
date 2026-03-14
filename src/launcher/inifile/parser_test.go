package inifile_test

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/inifile"
)

// writeFile is a helper that writes content to a file in dir and returns the path.
func writeFile(t *testing.T, dir, name, content string) string {
	t.Helper()
	path := filepath.Join(dir, name)
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatalf("writeFile %s: %v", path, err)
	}
	return path
}

// --------------------------------------------------------------------------
// 1. Basic section and key-value parsing
// --------------------------------------------------------------------------

func TestBasicSectionAndKeyValue(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "basic.ini", `
[SECTION1]
KEY1 = value1
KEY2 = value2

[SECTION2]
ANOTHER = hello
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	if got := ini.Get("SECTION1", "KEY1"); got != "value1" {
		t.Errorf("KEY1 = %q, want %q", got, "value1")
	}
	if got := ini.Get("SECTION1", "KEY2"); got != "value2" {
		t.Errorf("KEY2 = %q, want %q", got, "value2")
	}
	if got := ini.Get("SECTION2", "ANOTHER"); got != "hello" {
		t.Errorf("ANOTHER = %q, want %q", got, "hello")
	}
}

// --------------------------------------------------------------------------
// 2. Comments (# and ;)
// --------------------------------------------------------------------------

func TestComments(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "comments.ini", `
# This is a comment
[SECTION]
; also a comment
KEY = value ; inline semicolon comment
KEY2 = value2 # inline hash comment
KEY3 = #notacomment
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	if got := ini.Get("SECTION", "KEY"); got != "value" {
		t.Errorf("KEY = %q, want %q", got, "value")
	}
	if got := ini.Get("SECTION", "KEY2"); got != "value2" {
		t.Errorf("KEY2 = %q, want %q", got, "value2")
	}
	// '#' not preceded by whitespace is NOT a comment.
	if got := ini.Get("SECTION", "KEY3"); got != "#notacomment" {
		t.Errorf("KEY3 = %q, want %q", got, "#notacomment")
	}
}

// --------------------------------------------------------------------------
// 3. #INCLUDE with relative paths
// --------------------------------------------------------------------------

func TestIncludeRelative(t *testing.T) {
	dir := t.TempDir()
	writeFile(t, dir, "extra.ini", `
[INCLUDED]
INC_KEY = incval
`)
	f := writeFile(t, dir, "main.ini", `
[MAIN]
MAIN_KEY = mainval
#INCLUDE extra.ini
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	if got := ini.Get("MAIN", "MAIN_KEY"); got != "mainval" {
		t.Errorf("MAIN_KEY = %q, want %q", got, "mainval")
	}
	if got := ini.Get("INCLUDED", "INC_KEY"); got != "incval" {
		t.Errorf("INC_KEY = %q, want %q", got, "incval")
	}
}

// --------------------------------------------------------------------------
// 4. #INCLUDE with absolute paths
// --------------------------------------------------------------------------

func TestIncludeAbsolute(t *testing.T) {
	dir := t.TempDir()
	incPath := writeFile(t, dir, "abs.ini", `
[ABSEC]
ABS = yes
`)
	// Write main file that uses an absolute path.
	f := writeFile(t, dir, "main.ini", "[MAIN]\nFOO = bar\n#INCLUDE "+incPath+"\n")

	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if got := ini.Get("ABSEC", "ABS"); got != "yes" {
		t.Errorf("ABS = %q, want %q", got, "yes")
	}
}

// --------------------------------------------------------------------------
// 5. Nested #INCLUDE
// --------------------------------------------------------------------------

func TestIncludeNested(t *testing.T) {
	dir := t.TempDir()
	writeFile(t, dir, "deep.ini", `
[DEEP]
D = deepval
`)
	writeFile(t, dir, "middle.ini", `
[MIDDLE]
M = middleval
#INCLUDE deep.ini
`)
	f := writeFile(t, dir, "root.ini", `
[ROOT]
R = rootval
#INCLUDE middle.ini
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if got := ini.Get("ROOT", "R"); got != "rootval" {
		t.Errorf("R = %q, want %q", got, "rootval")
	}
	if got := ini.Get("MIDDLE", "M"); got != "middleval" {
		t.Errorf("M = %q, want %q", got, "middleval")
	}
	if got := ini.Get("DEEP", "D"); got != "deepval" {
		t.Errorf("D = %q, want %q", got, "deepval")
	}
}

// --------------------------------------------------------------------------
// 6. #INCLUDE with missing file (should error)
// --------------------------------------------------------------------------

func TestIncludeMissingFile(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "main.ini", `
[X]
K = v
#INCLUDE nonexistent.ini
`)
	_, err := inifile.Parse(f)
	if err == nil {
		t.Fatal("expected error for missing #INCLUDE file, got nil")
	}
}

// --------------------------------------------------------------------------
// 7. Repeated keys
// --------------------------------------------------------------------------

func TestRepeatedKeys(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "rep.ini", `
[HAL]
HALFILE = first.hal
HALFILE = second.hal
HALFILE = third.hal
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	// Get() returns first.
	if got := ini.Get("HAL", "HALFILE"); got != "first.hal" {
		t.Errorf("Get HALFILE = %q, want %q", got, "first.hal")
	}

	// GetAll() returns all.
	all := ini.GetAll("HAL", "HALFILE")
	want := []string{"first.hal", "second.hal", "third.hal"}
	if len(all) != len(want) {
		t.Fatalf("GetAll len = %d, want %d", len(all), len(want))
	}
	for i, v := range all {
		if v != want[i] {
			t.Errorf("GetAll[%d] = %q, want %q", i, v, want[i])
		}
	}
}

// --------------------------------------------------------------------------
// 8. GetN for numbered access
// --------------------------------------------------------------------------

func TestGetN(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "getn.ini", `
[HAL]
HALFILE = a.hal
HALFILE = b.hal
HALFILE = c.hal
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if got := ini.GetN("HAL", "HALFILE", 1); got != "a.hal" {
		t.Errorf("GetN 1 = %q, want %q", got, "a.hal")
	}
	if got := ini.GetN("HAL", "HALFILE", 2); got != "b.hal" {
		t.Errorf("GetN 2 = %q, want %q", got, "b.hal")
	}
	if got := ini.GetN("HAL", "HALFILE", 3); got != "c.hal" {
		t.Errorf("GetN 3 = %q, want %q", got, "c.hal")
	}
	// Out of range returns empty string.
	if got := ini.GetN("HAL", "HALFILE", 4); got != "" {
		t.Errorf("GetN 4 = %q, want %q", got, "")
	}
}

// --------------------------------------------------------------------------
// 9. GetWithFallback
// --------------------------------------------------------------------------

func TestGetWithFallback(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "fallback.ini", `
[DISPLAY]
DISPLAY = axis
[EMC]
MACHINE = My Machine
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	// First match wins.
	val, ok := ini.GetWithFallback([][2]string{
		{"DISPLAY", "DISPLAY"},
		{"EMC", "DISPLAY"},
	})
	if !ok || val != "axis" {
		t.Errorf("GetWithFallback = %q, %v; want %q, true", val, ok, "axis")
	}

	// Fallback to second pair when first is absent.
	val, ok = ini.GetWithFallback([][2]string{
		{"DISPLAY", "MACHINE"},
		{"EMC", "MACHINE"},
	})
	if !ok || val != "My Machine" {
		t.Errorf("GetWithFallback fallback = %q, %v; want %q, true", val, ok, "My Machine")
	}

	// Not found at all.
	_, ok = ini.GetWithFallback([][2]string{
		{"MISSING", "KEY"},
	})
	if ok {
		t.Error("GetWithFallback missing: expected ok=false")
	}
}

// --------------------------------------------------------------------------
// 10. Empty values
// --------------------------------------------------------------------------

func TestEmptyValues(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "empty.ini", `
[S]
EMPTY =
ALSO_EMPTY =    
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if got := ini.Get("S", "EMPTY"); got != "" {
		t.Errorf("EMPTY = %q, want empty string", got)
	}
	if got := ini.Get("S", "ALSO_EMPTY"); got != "" {
		t.Errorf("ALSO_EMPTY = %q, want empty string", got)
	}
}

// --------------------------------------------------------------------------
// 11. Whitespace handling around =
// --------------------------------------------------------------------------

func TestWhitespaceAroundEquals(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "ws.ini", `
[S]
A=1
B = 2
C  =  3
D	=	4
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	cases := [][2]string{{"A", "1"}, {"B", "2"}, {"C", "3"}, {"D", "4"}}
	for _, c := range cases {
		if got := ini.Get("S", c[0]); got != c[1] {
			t.Errorf("Get S/%s = %q, want %q", c[0], got, c[1])
		}
	}
}

// --------------------------------------------------------------------------
// 12. Variable substitution ([SECTION]KEY patterns)
// --------------------------------------------------------------------------

func TestSubstitute(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "sub.ini", `
[EMCMOT]
SERVO_PERIOD = 1000000
[TASK]
CYCLE_TIME = 0.010
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	input := "loadrt motmod servo_period_nsec=[EMCMOT]SERVO_PERIOD task_period=[TASK]CYCLE_TIME"
	want := "loadrt motmod servo_period_nsec=1000000 task_period=0.010"
	if got := ini.Substitute(input); got != want {
		t.Errorf("Substitute:\n got  %q\n want %q", got, want)
	}

	// Unknown reference is left unchanged.
	input2 := "something=[MISSING]KEY"
	if got := ini.Substitute(input2); got != input2 {
		t.Errorf("Substitute unknown: got %q, want %q", got, input2)
	}
}

// --------------------------------------------------------------------------
// 13. Environment variable expansion in #INCLUDE paths
// --------------------------------------------------------------------------

func TestIncludeEnvExpansion(t *testing.T) {
	dir := t.TempDir()
	writeFile(t, dir, "env.ini", `
[ENV]
ENVKEY = envval
`)
	t.Setenv("TEST_INC_DIR", dir)
	f := writeFile(t, dir, "main.ini", "#INCLUDE $TEST_INC_DIR/env.ini\n")

	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if got := ini.Get("ENV", "ENVKEY"); got != "envval" {
		t.Errorf("ENVKEY = %q, want %q", got, "envval")
	}
}

// --------------------------------------------------------------------------
// Extra: missing section/key returns empty string
// --------------------------------------------------------------------------

func TestMissing(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "x.ini", "[S]\nK = v\n")
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if got := ini.Get("NOSECTION", "K"); got != "" {
		t.Errorf("missing section = %q, want empty", got)
	}
	if got := ini.Get("S", "NOKEY"); got != "" {
		t.Errorf("missing key = %q, want empty", got)
	}
}

// --------------------------------------------------------------------------
// Extra: parse error for circular #INCLUDE
// --------------------------------------------------------------------------

func TestCircularInclude(t *testing.T) {
	dir := t.TempDir()
	// a.ini includes b.ini, b.ini includes a.ini
	writeFile(t, dir, "b.ini", "#INCLUDE a.ini\n")
	f := writeFile(t, dir, "a.ini", "#INCLUDE b.ini\n")

	_, err := inifile.Parse(f)
	if err == nil {
		t.Fatal("expected error for circular #INCLUDE, got nil")
	}
	if !strings.Contains(err.Error(), "circular") {
		t.Errorf("expected 'circular' in error, got: %v", err)
	}
}

// --------------------------------------------------------------------------
// Extra: inline comment marker ordering
// --------------------------------------------------------------------------

func TestInlineCommentOrdering(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "order.ini", `
[S]
KEY1 = value #comment ; more
KEY2 = value with;semicolon
KEY3 = no comment here
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	// '#' preceded by space comes before ';', so truncate at '#'.
	if got := ini.Get("S", "KEY1"); got != "value" {
		t.Errorf("KEY1 = %q, want %q", got, "value")
	}
	// ';' with no whitespace-preceded '#' — truncate at ';'.
	if got := ini.Get("S", "KEY2"); got != "value with" {
		t.Errorf("KEY2 = %q, want %q", got, "value with")
	}
	// No comment markers — value preserved as-is.
	if got := ini.Get("S", "KEY3"); got != "no comment here" {
		t.Errorf("KEY3 = %q, want %q", got, "no comment here")
	}
}

// --------------------------------------------------------------------------
// Set — update and append
// --------------------------------------------------------------------------

func TestSet_UpdatesExistingKey(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "set.ini", `
[EMCMOT]
EMCMOT = motmod
SERVO_PERIOD = 1000000
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	updated := ini.Set("EMCMOT", "EMCMOT", "motmod tp=tpmod hp=homemod")
	if !updated {
		t.Error("Set: expected true (existing entry updated), got false")
	}
	if got := ini.Get("EMCMOT", "EMCMOT"); got != "motmod tp=tpmod hp=homemod" {
		t.Errorf("Get after Set = %q, want %q", got, "motmod tp=tpmod hp=homemod")
	}
	// Other entries in the section must be unaffected.
	if got := ini.Get("EMCMOT", "SERVO_PERIOD"); got != "1000000" {
		t.Errorf("SERVO_PERIOD after Set = %q, want %q", got, "1000000")
	}
	// Substitute must reflect the updated value.
	input := "loadrt [EMCMOT]EMCMOT servo_period_nsec=[EMCMOT]SERVO_PERIOD"
	want := "loadrt motmod tp=tpmod hp=homemod servo_period_nsec=1000000"
	if got := ini.Substitute(input); got != want {
		t.Errorf("Substitute after Set:\n got  %q\n want %q", got, want)
	}
}

func TestSet_AppendsNewKey(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "setnew.ini", `
[EMCMOT]
SERVO_PERIOD = 1000000
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	updated := ini.Set("EMCMOT", "EMCMOT", "motmod tp=tpmod hp=homemod")
	if updated {
		t.Error("Set: expected false (new entry added), got true")
	}
	if got := ini.Get("EMCMOT", "EMCMOT"); got != "motmod tp=tpmod hp=homemod" {
		t.Errorf("Get new key = %q, want %q", got, "motmod tp=tpmod hp=homemod")
	}
}

func TestSet_CreatesNewSection(t *testing.T) {
	dir := t.TempDir()
	f := writeFile(t, dir, "setsec.ini", `
[OTHER]
FOO = bar
`)
	ini, err := inifile.Parse(f)
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}

	updated := ini.Set("NEWSEC", "NEWKEY", "newval")
	if updated {
		t.Error("Set: expected false (new section created), got true")
	}
	if got := ini.Get("NEWSEC", "NEWKEY"); got != "newval" {
		t.Errorf("Get new section/key = %q, want %q", got, "newval")
	}
	// Existing section must be unaffected.
	if got := ini.Get("OTHER", "FOO"); got != "bar" {
		t.Errorf("OTHER/FOO after Set = %q, want %q", got, "bar")
	}
}
