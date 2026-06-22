// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package inirest

import (
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/ini"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func boolPtr(v bool) *bool { return &v }

func setupTestINI(t *testing.T) *iniImpl {
	t.Helper()
	parsed, err := inifile.ParseString(`
[DISPLAY]
GEOMETRY = XYZABCUVW
MAX_FEED_OVERRIDE = 1.5
LATHE =

[FILTER]
PROGRAM_EXTENSION = .nc
PROGRAM_EXTENSION = .ngc
PROGRAM_EXTENSION = .py

[KINS]
JOINTS = 3

[EMC]
MACHINE = Test Machine
`)
	if err != nil {
		t.Fatal(err)
	}
	// Register with a fresh registry.
	reg := apiserver.NewRegistry()
	if err := Register(reg, parsed); err != nil {
		t.Fatal(err)
	}
	return &iniImpl{ini: parsed}
}

func TestQuerySingleValue(t *testing.T) {
	impl := setupTestINI(t)

	results, err := impl.Query([]ini.IniQueryItem{
		{Section: "DISPLAY", Key: "GEOMETRY"},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(results) != 1 {
		t.Fatalf("expected 1 result, got %d", len(results))
	}
	if results[0].Value != "XYZABCUVW" {
		t.Errorf("expected XYZABCUVW, got %v", results[0].Value)
	}
}

func TestQueryMissingKey(t *testing.T) {
	impl := setupTestINI(t)

	results, err := impl.Query([]ini.IniQueryItem{
		{Section: "DISPLAY", Key: "NONEXISTENT"},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(results) != 1 {
		t.Fatalf("expected 1 result, got %d", len(results))
	}
	if results[0].Value != "" {
		t.Errorf("expected empty value for missing key, got %v", results[0].Value)
	}
}

func TestQueryEmptyValue(t *testing.T) {
	impl := setupTestINI(t)

	results, err := impl.Query([]ini.IniQueryItem{
		{Section: "DISPLAY", Key: "LATHE"},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(results) != 1 {
		t.Fatalf("expected 1 result, got %d", len(results))
	}
	// Empty value — key exists but value is "".
	// With generated types we can't distinguish via pointer,
	// but the value should be returned as empty string.
	if results[0].Value != "" {
		t.Errorf("expected empty string, got %q", results[0].Value)
	}
}

func TestQueryFindAll(t *testing.T) {
	impl := setupTestINI(t)

	results, err := impl.Query([]ini.IniQueryItem{
		{Section: "FILTER", Key: "PROGRAM_EXTENSION", All: boolPtr(true)},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(results) != 1 {
		t.Fatalf("expected 1 result, got %d", len(results))
	}
	if len(results[0].Values) != 3 {
		t.Fatalf("expected 3 values, got %d: %v", len(results[0].Values), results[0].Values)
	}
	want := []string{".nc", ".ngc", ".py"}
	for i, w := range want {
		if results[0].Values[i] != w {
			t.Errorf("values[%d] = %q, want %q", i, results[0].Values[i], w)
		}
	}
}

func TestQueryFindAllMissing(t *testing.T) {
	impl := setupTestINI(t)

	results, err := impl.Query([]ini.IniQueryItem{
		{Section: "FILTER", Key: "NONEXISTENT", All: boolPtr(true)},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(results) != 1 {
		t.Fatalf("expected 1 result, got %d", len(results))
	}
	if results[0].Values == nil {
		t.Error("expected empty slice, got nil")
	} else if len(results[0].Values) != 0 {
		t.Errorf("expected 0 values, got %d", len(results[0].Values))
	}
}

func TestQueryBulk(t *testing.T) {
	impl := setupTestINI(t)

	results, err := impl.Query([]ini.IniQueryItem{
		{Section: "DISPLAY", Key: "GEOMETRY"},
		{Section: "DISPLAY", Key: "MAX_FEED_OVERRIDE"},
		{Section: "EMC", Key: "MACHINE"},
		{Section: "KINS", Key: "JOINTS"},
		{Section: "DISPLAY", Key: "NONEXISTENT"},
		{Section: "FILTER", Key: "PROGRAM_EXTENSION", All: boolPtr(true)},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(results) != 6 {
		t.Fatalf("expected 6 results, got %d", len(results))
	}

	if results[0].Value != "XYZABCUVW" {
		t.Errorf("result[0]: want XYZABCUVW, got %v", results[0].Value)
	}
	if results[1].Value != "1.5" {
		t.Errorf("result[1]: want 1.5, got %v", results[1].Value)
	}
	if results[4].Value != "" {
		t.Errorf("result[4]: want empty for missing key, got %v", results[4].Value)
	}
	if len(results[5].Values) != 3 {
		t.Errorf("result[5]: want 3 values, got %d", len(results[5].Values))
	}
}
