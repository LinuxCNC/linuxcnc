// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package haljson

import (
	"os"
	"path/filepath"
	"testing"

	halparse "github.com/sittner/linuxcnc/src/gomc/internal/halparse"
)

// writeTempConfig writes content to a temp file and returns its path.
func writeTempConfig(t *testing.T, content string) string {
	t.Helper()
	dir := t.TempDir()
	path := filepath.Join(dir, "config.xml")
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatalf("writing temp config: %v", err)
	}
	return path
}

// templateData builds a HalTemplateData from a simple INI map for tests.
func templateData() *halparse.HalTemplateData {
	return halparse.NewHalTemplateData(map[string]map[string]string{
		"TRAJ": {"COORDINATES": "X Y Z"},
		"KINS": {"JOINTS": "3"},
	})
}

// pinNames flattens all pin names (leaf items) under a root, in order.
func pinNames(items []*jsonItem) []string {
	var names []string
	for _, item := range items {
		switch item.kind {
		case itemPin:
			names = append(names, item.name)
		case itemObject, itemArray:
			names = append(names, pinNames(item.children)...)
		}
	}
	return names
}

func TestParseConfigPassthrough(t *testing.T) {
	// A config with no template directives must parse unchanged, whether or not
	// a template context is supplied.
	const cfg = `<halJson>
  <halJsonRoot path="pins">
    <halJsonPin name="jx" type="float" dir="in"/>
    <halJsonPin name="jy" type="float" dir="in"/>
  </halJsonRoot>
</halJson>`
	path := writeTempConfig(t, cfg)

	for _, td := range []*halparse.HalTemplateData{nil, templateData()} {
		roots, err := parseConfig(path, td)
		if err != nil {
			t.Fatalf("parseConfig: %v", err)
		}
		if len(roots) != 1 || roots[0].path != "pins" {
			t.Fatalf("unexpected roots: %+v", roots)
		}
		if got := pinNames(roots[0].items); len(got) != 2 || got[0] != "jx" || got[1] != "jy" {
			t.Fatalf("unexpected pins: %v", got)
		}
	}
}

func TestParseConfigTemplateRangeJoints(t *testing.T) {
	// range/seq over .Joints must expand into one pin per joint.
	const cfg = `<halJson>
  <halJsonRoot path="joints">
    {{- range seq 0 .Joints}}
    <halJsonPin name="{{printf "j%d" .}}" type="float" dir="in"/>
    {{- end}}
  </halJsonRoot>
</halJson>`
	path := writeTempConfig(t, cfg)

	roots, err := parseConfig(path, templateData())
	if err != nil {
		t.Fatalf("parseConfig: %v", err)
	}
	got := pinNames(roots[0].items)
	want := []string{"j0", "j1", "j2"}
	if len(got) != len(want) {
		t.Fatalf("got %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("pin %d: got %q, want %q", i, got[i], want[i])
		}
	}
}

func TestParseConfigTemplateIniAndAxis(t *testing.T) {
	// The ini function and hasAxis helper must be available and use the
	// supplied INI context.
	const cfg = `<halJson>
  <halJsonRoot path="{{replace (ini "TRAJ" "COORDINATES" | lower) " " "_"}}">
    {{- range .Axes}}
    {{- if hasAxis .}}
    <halJsonPin name="{{lower .}}" type="float" dir="out"/>
    {{- end}}
    {{- end}}
  </halJsonRoot>
</halJson>`
	path := writeTempConfig(t, cfg)

	roots, err := parseConfig(path, templateData())
	if err != nil {
		t.Fatalf("parseConfig: %v", err)
	}
	if roots[0].path != "x_y_z" {
		t.Fatalf("unexpected path: %q", roots[0].path)
	}
	got := pinNames(roots[0].items)
	want := []string{"x", "y", "z"}
	if len(got) != len(want) {
		t.Fatalf("got %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("pin %d: got %q, want %q", i, got[i], want[i])
		}
	}
}

func TestParseConfigTemplateError(t *testing.T) {
	// A malformed template directive must surface as an error, not silently
	// parse as XML.
	const cfg = `<halJson>
  <halJsonRoot path="pins">
    {{ this is not valid }}
  </halJsonRoot>
</halJson>`
	path := writeTempConfig(t, cfg)

	if _, err := parseConfig(path, templateData()); err == nil {
		t.Fatal("expected template error, got nil")
	}
}
