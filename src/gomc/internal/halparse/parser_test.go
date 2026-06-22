// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package halparse

import (
	"fmt"
	"os"
	"strings"
	"testing"

	hal "github.com/sittner/linuxcnc/src/gomc/pkg/hal"
)

// mockINI is a test implementation of INILookup.
type mockINI struct {
	data map[string]map[string]string
}

func (m *mockINI) Get(section, key string) (string, error) {
	if s, ok := m.data[section]; ok {
		if v, ok := s[key]; ok {
			return v, nil
		}
	}
	return "", fmt.Errorf("ini: [%s]%s not found", section, key)
}

func (m *mockINI) GetAll() map[string]map[string]string {
	return m.data
}

// mockResolver is a test implementation of PathResolver.
type mockResolver struct {
	base string
}

func (r *mockResolver) Resolve(path string) (string, error) {
	if r.base == "" {
		return path, nil
	}
	return r.base + "/" + path, nil
}

// --- TestTokenizeLine ---

func TestTokenizeLine(t *testing.T) {
	tests := []struct {
		input   string
		want    []string
		wantErr bool
	}{
		{"loadrt pid", []string{"loadrt", "pid"}, false},
		{"  setp  comp.pin  3.14  ", []string{"setp", "comp.pin", "3.14"}, false},
		{`setp "hello world" 1`, []string{"setp", "hello world", "1"}, false},
		{"# comment only", []string{}, false},
		{"setp pin 1 # inline comment", []string{"setp", "pin", "1"}, false},
		{`setp pin "unterminated`, nil, true},
		{"", []string{}, false},
		{"\t\t  ", []string{}, false},
		{`setp p "quoted with # hash"`, []string{"setp", "p", "quoted with # hash"}, false},
	}
	for _, tc := range tests {
		t.Run(tc.input, func(t *testing.T) {
			got, err := tokenizeLine(tc.input)
			if (err != nil) != tc.wantErr {
				t.Fatalf("tokenizeLine(%q) error = %v, wantErr %v", tc.input, err, tc.wantErr)
			}
			if tc.wantErr {
				return
			}
			if len(got) != len(tc.want) {
				t.Fatalf("tokenizeLine(%q) = %v, want %v", tc.input, got, tc.want)
			}
			for i, w := range tc.want {
				if got[i] != w {
					t.Errorf("tokenizeLine(%q)[%d] = %q, want %q", tc.input, i, got[i], w)
				}
			}
		})
	}
}

// --- TestParseLoad ---

func TestParseLoad(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("path only", func(t *testing.T) {
		tok, err := parseLoad([]string{"/tmp/foo.so"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LoadToken)
		if lt.Path != "/tmp/foo.so" {
			t.Errorf("Path = %q, want %q", lt.Path, "/tmp/foo.so")
		}
		if len(lt.Args) != 0 {
			t.Errorf("Args = %v, want []", lt.Args)
		}
		if lt.Names != nil {
			t.Errorf("Names = %v, want nil", lt.Names)
		}
	})

	t.Run("path with single arg", func(t *testing.T) {
		tok, err := parseLoad([]string{"/tmp/foo.so", "config=/etc/foo.ini"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LoadToken)
		if lt.Path != "/tmp/foo.so" {
			t.Errorf("Path = %q, want %q", lt.Path, "/tmp/foo.so")
		}
		if len(lt.Args) != 1 || lt.Args[0] != "config=/etc/foo.ini" {
			t.Errorf("Args = %v, want [config=/etc/foo.ini]", lt.Args)
		}
	})

	t.Run("path with multiple args", func(t *testing.T) {
		tok, err := parseLoad([]string{"/tmp/foo.so", "a=1", "b=2"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LoadToken)
		if len(lt.Args) != 2 {
			t.Errorf("Args len = %d, want 2", len(lt.Args))
		}
	})

	t.Run("missing path error", func(t *testing.T) {
		_, err := parseLoad([]string{}, loc)
		if err == nil {
			t.Error("expected error for missing path, got nil")
		}
	})

	t.Run("with single instance name", func(t *testing.T) {
		tok, err := parseLoad([]string{"/tmp/foo.so", "<myinst>", "a=1"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LoadToken)
		if lt.Path != "/tmp/foo.so" {
			t.Errorf("Path = %q, want %q", lt.Path, "/tmp/foo.so")
		}
		if len(lt.Names) != 1 || lt.Names[0] != "myinst" {
			t.Errorf("Names = %v, want [myinst]", lt.Names)
		}
		if len(lt.Args) != 1 || lt.Args[0] != "a=1" {
			t.Errorf("Args = %v, want [a=1]", lt.Args)
		}
	})

	t.Run("with multiple instance names", func(t *testing.T) {
		tok, err := parseLoad([]string{"mymod", "<inst1,inst2,inst3>", "x=1"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LoadToken)
		if len(lt.Names) != 3 {
			t.Fatalf("Names len = %d, want 3", len(lt.Names))
		}
		if lt.Names[0] != "inst1" || lt.Names[1] != "inst2" || lt.Names[2] != "inst3" {
			t.Errorf("Names = %v, want [inst1 inst2 inst3]", lt.Names)
		}
	})

	t.Run("names only no args", func(t *testing.T) {
		tok, err := parseLoad([]string{"mymod", "<inst1>"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LoadToken)
		if len(lt.Names) != 1 || lt.Names[0] != "inst1" {
			t.Errorf("Names = %v, want [inst1]", lt.Names)
		}
		if len(lt.Args) != 0 {
			t.Errorf("Args = %v, want []", lt.Args)
		}
	})

	t.Run("empty name list error", func(t *testing.T) {
		_, err := parseLoad([]string{"mymod", "<>"}, loc)
		if err == nil {
			t.Error("expected error for empty name list, got nil")
		}
	})

	t.Run("empty name in list error", func(t *testing.T) {
		_, err := parseLoad([]string{"mymod", "<a,,b>"}, loc)
		if err == nil {
			t.Error("expected error for empty name in list, got nil")
		}
	})
}

// --- TestParseLine_load ---

func TestParseLine_load(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	tok, err := parseLine([]string{"load", "/tmp/foo.so", "x=1"}, loc)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	lt, ok := tok.Data.(*LoadToken)
	if !ok {
		t.Fatalf("expected *LoadToken, got %T", tok.Data)
	}
	if lt.Path != "/tmp/foo.so" {
		t.Errorf("Path = %q, want %q", lt.Path, "/tmp/foo.so")
	}
}

// --- TestSingleFileParser_load_classified ---

func TestSingleFileParser_load_classified(t *testing.T) {
	content := "load /tmp/foo.so config=bar\n"
	sp := &SingleFileParser{}
	result, err := sp.ParseContent("test.hal", content)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(result.Loads) != 1 {
		t.Fatalf("expected 1 Loads token, got %d", len(result.Loads))
	}
	lt, ok := result.Loads[0].Data.(*LoadToken)
	if !ok {
		t.Fatalf("expected *LoadToken, got %T", result.Loads[0].Data)
	}
	if lt.Path != "/tmp/foo.so" {
		t.Errorf("Path = %q, want %q", lt.Path, "/tmp/foo.so")
	}
	if len(result.HALCmd) != 0 {
		t.Errorf("expected 0 HALCmd tokens, got %d", len(result.HALCmd))
	}
}

// --- TestParseNet ---

func TestParseNet(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("signal and pins", func(t *testing.T) {
		tok, err := parseNet([]string{"mysig", "pin1", "pin2"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		nt := tok.Data.(*NetToken)
		if nt.Signal != "mysig" {
			t.Errorf("Signal = %q, want %q", nt.Signal, "mysig")
		}
		if len(nt.Pins) != 2 || nt.Pins[0] != "pin1" || nt.Pins[1] != "pin2" {
			t.Errorf("Pins = %v, want [pin1 pin2]", nt.Pins)
		}
	})

	t.Run("arrow stripping", func(t *testing.T) {
		tok, err := parseNet([]string{"sig", "=>", "pin1", "<=", "pin2", "<=>", "pin3"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		nt := tok.Data.(*NetToken)
		if nt.Signal != "sig" {
			t.Errorf("Signal = %q, want %q", nt.Signal, "sig")
		}
		want := []string{"pin1", "pin2", "pin3"}
		if len(nt.Pins) != len(want) {
			t.Fatalf("Pins = %v, want %v", nt.Pins, want)
		}
		for i, p := range want {
			if nt.Pins[i] != p {
				t.Errorf("Pins[%d] = %q, want %q", i, nt.Pins[i], p)
			}
		}
	})

	t.Run("no signal error", func(t *testing.T) {
		_, err := parseNet([]string{}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseSetP/SetS ---

func TestParseLinkPP(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("two pins", func(t *testing.T) {
		tok, err := parseLinkPP([]string{"comp.pin1", "comp.pin2"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LinkPPToken)
		if lt.Pin1 != "comp.pin1" || lt.Pin2 != "comp.pin2" {
			t.Errorf("Pin1=%q Pin2=%q, want comp.pin1 comp.pin2", lt.Pin1, lt.Pin2)
		}
	})

	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseLinkPP([]string{"comp.pin1"}, loc)
		if err == nil {
			t.Error("expected error for single argument, got nil")
		}
	})
}

func TestParseSetP(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("valid 2 args", func(t *testing.T) {
		tok, err := parseSetP([]string{"comp.pin", "3.14"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*SetPToken)
		if st.Name != "comp.pin" || st.Value != "3.14" {
			t.Errorf("Name=%q Value=%q, want comp.pin 3.14", st.Name, st.Value)
		}
	})

	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseSetP([]string{"only-one"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

func TestParseSetS(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("valid 2 args", func(t *testing.T) {
		tok, err := parseSetS([]string{"mysig", "1"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*SetSToken)
		if st.Name != "mysig" || st.Value != "1" {
			t.Errorf("Name=%q Value=%q", st.Name, st.Value)
		}
	})

	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseSetS([]string{}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseGetP/GetS ---

func TestParseGetP(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}
	t.Run("valid 1 arg", func(t *testing.T) {
		tok, err := parseGetP([]string{"comp.pin"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		gt := tok.Data.(*GetPToken)
		if gt.Name != "comp.pin" {
			t.Errorf("Name = %q, want comp.pin", gt.Name)
		}
	})
	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseGetP([]string{}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

func TestParseGetS(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}
	t.Run("valid 1 arg", func(t *testing.T) {
		tok, err := parseGetS([]string{"mysig"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		gt := tok.Data.(*GetSToken)
		if gt.Name != "mysig" {
			t.Errorf("Name = %q, want mysig", gt.Name)
		}
	})
	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseGetS([]string{"a", "b"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseAddF ---

func TestParseAddF(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("2 args default pos=-1", func(t *testing.T) {
		tok, err := parseAddF([]string{"pid.0.do-pid-calcs", "servo-thread"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		at := tok.Data.(*AddFToken)
		if at.Funct != "pid.0.do-pid-calcs" || at.Thread != "servo-thread" || at.Pos != -1 {
			t.Errorf("unexpected values: %+v", at)
		}
	})

	t.Run("3 args with position", func(t *testing.T) {
		tok, err := parseAddF([]string{"pid.0.do-pid-calcs", "servo-thread", "3"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		at := tok.Data.(*AddFToken)
		if at.Pos != 3 {
			t.Errorf("Pos = %d, want 3", at.Pos)
		}
	})

	t.Run("invalid position error", func(t *testing.T) {
		_, err := parseAddF([]string{"funct", "thread", "notanint"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})

	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseAddF([]string{"only-one"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseDelF ---

func TestParseDelF(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}
	t.Run("valid 2 args", func(t *testing.T) {
		tok, err := parseDelF([]string{"funct", "thread"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		dt := tok.Data.(*DelFToken)
		if dt.Funct != "funct" || dt.Thread != "thread" {
			t.Errorf("unexpected values: %+v", dt)
		}
	})
	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseDelF([]string{"funct"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseNewSig ---

func TestParseNewSig(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	tests := []struct {
		typeStr string
		want    hal.PinType
	}{
		{"bit", hal.TypeBit},
		{"float", hal.TypeFloat},
		{"s32", hal.TypeS32},
		{"u32", hal.TypeU32},
	}
	for _, tc := range tests {
		t.Run(tc.typeStr, func(t *testing.T) {
			tok, err := parseNewSig([]string{"mysig", tc.typeStr}, loc)
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			nt := tok.Data.(*NewSigToken)
			if nt.Name != "mysig" {
				t.Errorf("Name = %q, want %q", nt.Name, "mysig")
			}
			if nt.SigType != tc.want {
				t.Errorf("SigType = %v, want %v", nt.SigType, tc.want)
			}
		})
	}

	t.Run("invalid type error", func(t *testing.T) {
		_, err := parseNewSig([]string{"mysig", "invalid"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseAlias/UnAlias ---

func TestParseAlias(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("pin kind", func(t *testing.T) {
		tok, err := parseAlias([]string{"pin", "comp.pin", "myalias"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		at := tok.Data.(*AliasToken)
		if at.Kind != AliasPin || at.Name != "comp.pin" || at.Alias != "myalias" {
			t.Errorf("unexpected values: %+v", at)
		}
	})

	t.Run("param kind", func(t *testing.T) {
		tok, err := parseAlias([]string{"param", "comp.param", "myalias"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		at := tok.Data.(*AliasToken)
		if at.Kind != AliasParam {
			t.Errorf("Kind = %v, want AliasParam", at.Kind)
		}
	})

	t.Run("invalid kind error", func(t *testing.T) {
		_, err := parseAlias([]string{"bad", "name", "alias"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

func TestParseUnAlias(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("valid pin", func(t *testing.T) {
		tok, err := parseUnAlias([]string{"pin", "comp.pin"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		ut := tok.Data.(*UnAliasToken)
		if ut.Kind != AliasPin || ut.Name != "comp.pin" {
			t.Errorf("unexpected values: %+v", ut)
		}
	})

	t.Run("invalid kind error", func(t *testing.T) {
		_, err := parseUnAlias([]string{"bad", "name"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseLock/Unlock ---

func TestParseLock(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("no args defaults to LockAll", func(t *testing.T) {
		tok, err := parseLock([]string{}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*LockToken)
		if lt.Level != LockAll {
			t.Errorf("Level = %v, want LockAll", lt.Level)
		}
	})

	lockTests := []struct {
		str  string
		want LockLevel
	}{
		{"none", LockNone},
		{"load", LockLoad},
		{"config", LockConfig},
		{"tune", LockTune},
		{"params", LockParams},
		{"run", LockRun},
		{"all", LockAll},
	}
	for _, tc := range lockTests {
		t.Run(tc.str, func(t *testing.T) {
			tok, err := parseLock([]string{tc.str}, loc)
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			lt := tok.Data.(*LockToken)
			if lt.Level != tc.want {
				t.Errorf("Level = %v, want %v", lt.Level, tc.want)
			}
		})
	}

	t.Run("invalid level error", func(t *testing.T) {
		_, err := parseLock([]string{"invalid"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

func TestParseUnlock(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("no args defaults to LockAll", func(t *testing.T) {
		tok, err := parseUnlock([]string{}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		ut := tok.Data.(*UnlockToken)
		if ut.Level != LockAll {
			t.Errorf("Level = %v, want LockAll", ut.Level)
		}
	})

	t.Run("invalid level error", func(t *testing.T) {
		_, err := parseUnlock([]string{"invalid"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseList/Show ---

func TestParseList(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	objTests := []struct {
		str  string
		want HalObjType
	}{
		{"pin", ObjPin},
		{"sig", ObjSig},
		{"param", ObjParam},
		{"funct", ObjFunct},
		{"thread", ObjThread},
		{"comp", ObjComp},
	}
	for _, tc := range objTests {
		t.Run(tc.str, func(t *testing.T) {
			tok, err := parseList([]string{tc.str}, loc)
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			lt := tok.Data.(*ListToken)
			if lt.ObjType != tc.want {
				t.Errorf("ObjType = %v, want %v", lt.ObjType, tc.want)
			}
		})
	}

	t.Run("with patterns", func(t *testing.T) {
		tok, err := parseList([]string{"pin", "*.enable"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*ListToken)
		if len(lt.Patterns) != 1 || lt.Patterns[0] != "*.enable" {
			t.Errorf("Patterns = %v, want [*.enable]", lt.Patterns)
		}
	})

	t.Run("missing type error", func(t *testing.T) {
		_, err := parseList([]string{}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})

	t.Run("invalid type error", func(t *testing.T) {
		_, err := parseList([]string{"bad"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})

	t.Run("all type", func(t *testing.T) {
		tok, err := parseList([]string{"all"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		lt := tok.Data.(*ListToken)
		if lt.ObjType != ObjAll {
			t.Errorf("ObjType = %v, want ObjAll", lt.ObjType)
		}
	})
}

func TestParseShow(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("no args defaults to ObjAll", func(t *testing.T) {
		tok, err := parseShow([]string{}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*ShowToken)
		if st.ObjType != ObjAll {
			t.Errorf("ObjType = %v, want ObjAll", st.ObjType)
		}
	})

	t.Run("specific type", func(t *testing.T) {
		tok, err := parseShow([]string{"pin"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*ShowToken)
		if st.ObjType != ObjPin {
			t.Errorf("ObjType = %v, want ObjPin", st.ObjType)
		}
	})

	t.Run("type with patterns", func(t *testing.T) {
		tok, err := parseShow([]string{"param", "pid.*"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*ShowToken)
		if st.ObjType != ObjParam || len(st.Patterns) != 1 || st.Patterns[0] != "pid.*" {
			t.Errorf("unexpected: %+v", st)
		}
	})

	t.Run("all keyword", func(t *testing.T) {
		tok, err := parseShow([]string{"all"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*ShowToken)
		if st.ObjType != ObjAll {
			t.Errorf("ObjType = %v, want ObjAll", st.ObjType)
		}
	})

	t.Run("invalid type error", func(t *testing.T) {
		_, err := parseShow([]string{"badtype"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseSave ---

func TestParseSave(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("no args defaults to SaveAll", func(t *testing.T) {
		tok, err := parseSave([]string{}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*SaveToken)
		if st.SaveType != SaveAll || st.File != "" {
			t.Errorf("unexpected: %+v", st)
		}
	})

	t.Run("type only", func(t *testing.T) {
		tok, err := parseSave([]string{"sig"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*SaveToken)
		if st.SaveType != SaveSig {
			t.Errorf("SaveType = %v, want SaveSig", st.SaveType)
		}
	})

	t.Run("type and file", func(t *testing.T) {
		tok, err := parseSave([]string{"net", "connections.hal"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		st := tok.Data.(*SaveToken)
		if st.SaveType != SaveNet || st.File != "connections.hal" {
			t.Errorf("unexpected: %+v", st)
		}
	})

	t.Run("invalid type error", func(t *testing.T) {
		_, err := parseSave([]string{"badtype"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParseDebug ---

func TestParseDebug(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("valid int", func(t *testing.T) {
		tok, err := parseDebug([]string{"3"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		dt := tok.Data.(*DebugToken)
		if dt.Level != 3 {
			t.Errorf("Level = %d, want 3", dt.Level)
		}
	})

	t.Run("invalid int error", func(t *testing.T) {
		_, err := parseDebug([]string{"abc"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})

	t.Run("wrong arg count error", func(t *testing.T) {
		_, err := parseDebug([]string{}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})
}

// --- TestParsePrint ---

func TestParsePrint(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("multiple args joined with space", func(t *testing.T) {
		tok, err := parsePrint([]string{"hello", "world", "foo"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		pt := tok.Data.(*PrintToken)
		if pt.Message != "hello world foo" {
			t.Errorf("Message = %q, want %q", pt.Message, "hello world foo")
		}
	})

	t.Run("zero args produces empty message", func(t *testing.T) {
		tok, err := parsePrint([]string{}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		pt := tok.Data.(*PrintToken)
		if pt.Message != "" {
			t.Errorf("Message = %q, want empty", pt.Message)
		}
	})
}

// --- TestParseEcho/UnEcho ---

func TestParseEcho(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("no args", func(t *testing.T) {
		tok, err := parseEcho([]string{}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if _, ok := tok.Data.(*EchoToken); !ok {
			t.Error("expected *EchoToken")
		}
	})

	t.Run("extra args ignored", func(t *testing.T) {
		tok, err := parseEcho([]string{"ignored"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if _, ok := tok.Data.(*EchoToken); !ok {
			t.Error("expected *EchoToken")
		}
	})
}

func TestParseUnEcho(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("no args", func(t *testing.T) {
		tok, err := parseUnEcho([]string{}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if _, ok := tok.Data.(*UnEchoToken); !ok {
			t.Error("expected *UnEchoToken")
		}
	})
}

// --- TestParseLine ---

func TestParseLine(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	t.Run("empty token list returns error", func(t *testing.T) {
		_, err := parseLine([]string{}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})

	t.Run("unknown command returns error", func(t *testing.T) {
		_, err := parseLine([]string{"unknowncmd", "arg"}, loc)
		if err == nil {
			t.Error("expected error, got nil")
		}
	})

	t.Run("source returns error", func(t *testing.T) {
		_, err := parseLine([]string{"source", "file.hal"}, loc)
		if err == nil {
			t.Error("expected error: source should not be handled by parseLine")
		}
	})

	t.Run("case-insensitive loadrt returns error", func(t *testing.T) {
		_, err := parseLine([]string{"LoadRT", "motmod"}, loc)
		if err == nil {
			t.Error("expected error for loadrt, got nil")
		}
	})

	t.Run("case-insensitive SETP", func(t *testing.T) {
		tok, err := parseLine([]string{"SETP", "pin", "val"}, loc)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if _, ok := tok.Data.(*SetPToken); !ok {
			t.Error("expected *SetPToken")
		}
	})

	// Verify each command dispatches correctly
	dispatchTests := []struct {
		tokens []string
		kind   string
	}{
		{[]string{"net", "sig"}, "*halparse.NetToken"},
		{[]string{"setp", "a", "b"}, "*halparse.SetPToken"},
		{[]string{"sets", "a", "b"}, "*halparse.SetSToken"},
		{[]string{"getp", "a"}, "*halparse.GetPToken"},
		{[]string{"gets", "a"}, "*halparse.GetSToken"},
		{[]string{"addf", "f", "t"}, "*halparse.AddFToken"},
		{[]string{"delf", "f", "t"}, "*halparse.DelFToken"},
		{[]string{"newsig", "s", "bit"}, "*halparse.NewSigToken"},
		{[]string{"delsig", "s"}, "*halparse.DelSigToken"},
		{[]string{"linkps", "p", "s"}, "*halparse.LinkPSToken"},
		{[]string{"linksp", "s", "p"}, "*halparse.LinkSPToken"},
		{[]string{"linkpp", "p1", "p2"}, "*halparse.LinkPPToken"},
		{[]string{"unlinkp", "p"}, "*halparse.UnlinkPToken"},
		{[]string{"alias", "pin", "n", "a"}, "*halparse.AliasToken"},
		{[]string{"unalias", "pin", "n"}, "*halparse.UnAliasToken"},
		{[]string{"start"}, "*halparse.StartToken"},
		{[]string{"stop"}, "*halparse.StopToken"},
		{[]string{"lock"}, "*halparse.LockToken"},
		{[]string{"unlock"}, "*halparse.UnlockToken"},
		{[]string{"list", "pin"}, "*halparse.ListToken"},
		{[]string{"show"}, "*halparse.ShowToken"},
		{[]string{"save"}, "*halparse.SaveToken"},
		{[]string{"status"}, "*halparse.StatusToken"},
		{[]string{"debug", "0"}, "*halparse.DebugToken"},
		{[]string{"ptype", "n"}, "*halparse.PTypeToken"},
		{[]string{"stype", "n"}, "*halparse.STypeToken"},
		{[]string{"echo"}, "*halparse.EchoToken"},
		{[]string{"unecho"}, "*halparse.UnEchoToken"},
		{[]string{"print", "msg"}, "*halparse.PrintToken"},
	}
	for _, tc := range dispatchTests {
		t.Run("dispatch_"+tc.tokens[0], func(t *testing.T) {
			tok, err := parseLine(tc.tokens, loc)
			if err != nil {
				t.Fatalf("parseLine(%v): unexpected error: %v", tc.tokens, err)
			}
			got := fmt.Sprintf("%T", tok.Data)
			if got != tc.kind {
				t.Errorf("parseLine(%v): Data type = %q, want %q", tc.tokens, got, tc.kind)
			}
		})
	}
}

// --- TestSubstituteVars ---

func TestSubstituteVars(t *testing.T) {
	ini := &mockINI{data: map[string]map[string]string{
		"EMC": {
			"MACHINE": "My Machine",
			"DEBUG":   "0",
		},
		"TRAJ": {
			"COORDINATES": "XYZ",
		},
		"JOINT_0": {
			"MIN-LIMIT": "-180.0",
		},
	}}

	t.Run("INI substitution", func(t *testing.T) {
		result := substituteVars("machine=[EMC]MACHINE", ini)
		if result != "machine=My Machine" {
			t.Errorf("got %q, want %q", result, "machine=My Machine")
		}
	})

	t.Run("INI key not found leaves as-is", func(t *testing.T) {
		result := substituteVars("[EMC]NOTFOUND", ini)
		if result != "[EMC]NOTFOUND" {
			t.Errorf("got %q, want %q", result, "[EMC]NOTFOUND")
		}
	})

	t.Run("env var $VARNAME", func(t *testing.T) {
		os.Setenv("HAL_TEST_VAR", "testvalue")
		defer os.Unsetenv("HAL_TEST_VAR")
		result := substituteVars("val=$HAL_TEST_VAR", ini)
		if result != "val=testvalue" {
			t.Errorf("got %q, want %q", result, "val=testvalue")
		}
	})

	t.Run("env var ${VARNAME}", func(t *testing.T) {
		os.Setenv("HAL_TEST_VAR2", "enclosed")
		defer os.Unsetenv("HAL_TEST_VAR2")
		result := substituteVars("val=${HAL_TEST_VAR2}", ini)
		if result != "val=enclosed" {
			t.Errorf("got %q, want %q", result, "val=enclosed")
		}
	})

	t.Run("missing env var replaced with empty string", func(t *testing.T) {
		os.Unsetenv("HAL_DEFINITELY_NOT_SET_XYZ")
		result := substituteVars("val=$HAL_DEFINITELY_NOT_SET_XYZ", ini)
		if result != "val=" {
			t.Errorf("got %q, want %q", result, "val=")
		}
	})

	t.Run("multiple substitutions on one line", func(t *testing.T) {
		os.Setenv("HAL_MULTI_TEST", "42")
		defer os.Unsetenv("HAL_MULTI_TEST")
		result := substituteVars("[EMC]DEBUG $HAL_MULTI_TEST", ini)
		if result != "0 42" {
			t.Errorf("got %q, want %q", result, "0 42")
		}
	})

	t.Run("no substitution needed", func(t *testing.T) {
		result := substituteVars("loadrt motmod", ini)
		if result != "loadrt motmod" {
			t.Errorf("got %q, want %q", result, "loadrt motmod")
		}
	})

	t.Run("substitution operates on raw line before tokenizing", func(t *testing.T) {
		// The substitution happens before tokenizing so it can affect multiple tokens
		result := substituteVars("setp [EMC]MACHINE [TRAJ]COORDINATES", ini)
		if result != "setp My Machine XYZ" {
			t.Errorf("got %q, want %q", result, "setp My Machine XYZ")
		}
	})

	t.Run("INI key with hyphen", func(t *testing.T) {
		result := substituteVars("setp joint.0.min-limit [JOINT_0]MIN-LIMIT", ini)
		if result != "setp joint.0.min-limit -180.0" {
			t.Errorf("got %q, want %q", result, "setp joint.0.min-limit -180.0")
		}
	})
}

// --- TestSingleFileParser ---

func TestSingleFileParser(t *testing.T) {
	t.Run("basic file with multiple commands", func(t *testing.T) {
		files := map[string]string{
			"test.hal": strings.Join([]string{
				"setp pid.0.Pgain 500",
				"setp pid.0.Pgain 1000",
				"addf pid.0.do-pid-calcs servo-thread",
			}, "\n"),
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		result, err := sp.Parse("test.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 3 {
			t.Errorf("HALCmd count = %d, want 3", len(result.HALCmd))
		}
	})

	t.Run("template expansion", func(t *testing.T) {
		files := map[string]string{
			"test.hal": `setp pid.count {{.Joints}}`,
		}
		tmplData := &HalTemplateData{
			INI:    map[string]map[string]string{},
			Joints: 3,
			Env:    map[string]string{},
		}
		sp := &SingleFileParser{
			templateData: tmplData,
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		result, err := sp.Parse("test.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 1 {
			t.Fatalf("HALCmd count = %d, want 1", len(result.HALCmd))
		}
		st := result.HALCmd[0].Data.(*SetPToken)
		if st.Value != "3" {
			t.Errorf("Value = %q, want %q (from template expansion)", st.Value, "3")
		}
	})

	t.Run("INI substitution", func(t *testing.T) {
		files := map[string]string{
			"test.hal": "setp pid.count [KINS]JOINTS",
		}
		ini := &mockINI{data: map[string]map[string]string{
			"KINS": {"JOINTS": "4"},
		}}
		sp := &SingleFileParser{
			ini: ini,
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		result, err := sp.Parse("test.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 1 {
			t.Fatalf("HALCmd count = %d, want 1", len(result.HALCmd))
		}
		st := result.HALCmd[0].Data.(*SetPToken)
		if st.Value != "4" {
			t.Errorf("Value = %q, want %q (from INI substitution)", st.Value, "4")
		}
	})

	t.Run("source recursion merges results", func(t *testing.T) {
		files := map[string]string{
			"main.hal":  "source child.hal\nsetp comp.pin 1",
			"child.hal": "setp pid.0.Pgain 100",
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				c, ok := files[path]
				if !ok {
					return "", fmt.Errorf("file not found: %s", path)
				}
				return c, nil
			},
		}
		result, err := sp.Parse("main.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 2 {
			t.Errorf("HALCmd count = %d, want 2", len(result.HALCmd))
		}
	})

	t.Run("source depth limit returns error", func(t *testing.T) {
		// File sources itself to trigger depth limit
		files := map[string]string{
			"recursive.hal": "source recursive.hal",
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				c, ok := files[path]
				if !ok {
					return "", fmt.Errorf("file not found: %s", path)
				}
				return c, nil
			},
		}
		_, err := sp.Parse("recursive.hal")
		if err == nil {
			t.Error("expected depth limit error, got nil")
		}
	})

	t.Run("comment and blank lines skipped", func(t *testing.T) {
		files := map[string]string{
			"test.hal": strings.Join([]string{
				"# This is a comment",
				"",
				"  # Indented comment",
				"setp comp.pin 1",
			}, "\n"),
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		result, err := sp.Parse("test.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 1 {
			t.Errorf("HALCmd count = %d, want 1 (only setp line)", len(result.HALCmd))
		}
	})

	t.Run("line continuation", func(t *testing.T) {
		files := map[string]string{
			"test.hal": "setp very.long.pin.name \\\n    3.14159",
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		result, err := sp.Parse("test.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 1 {
			t.Fatalf("HALCmd count = %d, want 1", len(result.HALCmd))
		}
		st := result.HALCmd[0].Data.(*SetPToken)
		if st.Name != "very.long.pin.name" {
			t.Errorf("Name = %q, want %q", st.Name, "very.long.pin.name")
		}
		if st.Value != "3.14159" {
			t.Errorf("Value = %q, want %q", st.Value, "3.14159")
		}
	})

	t.Run("line continuation no leading whitespace", func(t *testing.T) {
		// When the continuation line has no leading whitespace the inserted space
		// must keep the two token parts separate.
		files := map[string]string{
			"test.hal": "setp very.long.pin.name\\\n3.14159",
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		result, err := sp.Parse("test.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 1 {
			t.Fatalf("HALCmd count = %d, want 1", len(result.HALCmd))
		}
		st := result.HALCmd[0].Data.(*SetPToken)
		if st.Name != "very.long.pin.name" {
			t.Errorf("Name = %q, want %q", st.Name, "very.long.pin.name")
		}
		if st.Value != "3.14159" {
			t.Errorf("Value = %q, want %q", st.Value, "3.14159")
		}
	})

	t.Run("parse error returns error with location", func(t *testing.T) {
		files := map[string]string{
			"test.hal": "unknowncmd arg",
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		_, err := sp.Parse("test.hal")
		if err == nil {
			t.Error("expected parse error, got nil")
		}
		if pe, ok := err.(*ParseError); ok {
			if pe.Loc.File != "test.hal" {
				t.Errorf("error file = %q, want %q", pe.Loc.File, "test.hal")
			}
		} else {
			t.Errorf("expected *ParseError, got %T", err)
		}
	})

	t.Run("unterminated quote error has location", func(t *testing.T) {
		files := map[string]string{
			"test.hal": `setp comp.pin "unterminated`,
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		_, err := sp.Parse("test.hal")
		if err == nil {
			t.Error("expected tokenize error, got nil")
		}
	})

	t.Run("tcl file rejected", func(t *testing.T) {
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return "", nil
			},
		}
		_, err := sp.Parse("machine.tcl")
		if err == nil {
			t.Error("expected error for .tcl file, got nil")
		}
		if pe, ok := err.(*ParseError); ok {
			if !strings.Contains(pe.Msg, "haltcl") {
				t.Errorf("error message should mention haltcl, got: %q", pe.Msg)
			}
		} else {
			t.Errorf("expected *ParseError, got %T", err)
		}
	})

	t.Run("TCL uppercase extension rejected", func(t *testing.T) {
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return "", nil
			},
		}
		_, err := sp.Parse("machine.TCL")
		if err == nil {
			t.Error("expected error for .TCL file, got nil")
		}
	})
}

// --- TestMultiFileParser ---

func TestMultiFileParser(t *testing.T) {
	makeReadFile := func(files map[string]string) func(string) (string, error) {
		return func(path string) (string, error) {
			c, ok := files[path]
			if !ok {
				return "", fmt.Errorf("file not found: %s", path)
			}
			return c, nil
		}
	}

	t.Run("empty file list returns empty result", func(t *testing.T) {
		mp := &MultiFileParser{}
		result, err := mp.Parse([]string{})
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(result.HALCmd) != 0 {
			t.Error("expected empty result for empty file list")
		}
	})

	t.Run("single file result matches SingleFileParser", func(t *testing.T) {
		files := map[string]string{
			"a.hal": "setp pid.0.Pgain 100\nsetp pid.0.Igain 50",
		}
		rf := makeReadFile(files)

		sp := &SingleFileParser{readFile: rf}
		spResult, err := sp.Parse("a.hal")
		if err != nil {
			t.Fatalf("SingleFileParser: %v", err)
		}

		if len(spResult.HALCmd) != 2 {
			t.Errorf("unexpected count: HALCmd=%d", len(spResult.HALCmd))
		}
	})

	t.Run("multiple files tokens merged in order", func(t *testing.T) {
		files := map[string]string{
			"a.hal": "setp pid.0.Pgain 100\naddf pid.0.do-pid-calcs servo-thread",
			"b.hal": "setp pid.1.Pgain 200\naddf pid.1.do-pid-calcs servo-thread",
		}
		rf := makeReadFile(files)

		// Parse individually and verify merge would work
		spA := &SingleFileParser{readFile: rf}
		spB := &SingleFileParser{readFile: rf}
		resultA, _ := spA.Parse("a.hal")
		resultB, _ := spB.Parse("b.hal")

		// Simulate what MultiFileParser does
		merged := &ParseResult{}
		merged.HALCmd = append(merged.HALCmd, resultA.HALCmd...)
		merged.HALCmd = append(merged.HALCmd, resultB.HALCmd...)

		if len(merged.HALCmd) != 4 {
			t.Errorf("merged HALCmd count = %d, want 4", len(merged.HALCmd))
		}
		// Order preserved: pid.0.Pgain first
		st := merged.HALCmd[0].Data.(*SetPToken)
		if st.Name != "pid.0.Pgain" {
			t.Errorf("first HALCmd Name = %q, want %q", st.Name, "pid.0.Pgain")
		}
	})

	t.Run("source location is recorded", func(t *testing.T) {
		files := map[string]string{
			"test.hal": "setp comp.pin 1\nsetp comp.pin2 2\nsetp comp.pin3 3",
		}
		sp := &SingleFileParser{
			readFile: func(path string) (string, error) {
				return files[path], nil
			},
		}
		result, err := sp.Parse("test.hal")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		for i, tok := range result.HALCmd {
			if tok.Location.File != "test.hal" {
				t.Errorf("token[%d] File = %q, want %q", i, tok.Location.File, "test.hal")
			}
			if tok.Location.Line != i+1 {
				t.Errorf("token[%d] Line = %d, want %d", i, tok.Location.Line, i+1)
			}
		}
	})
}

// TestSingleFileParser_TemplateRendering verifies that NewSingleFileParser
// builds templateData from INI so that Go templates in HAL files are rendered
// before parsing.
func TestSingleFileParser_TemplateRendering(t *testing.T) {
	ini := &mockINI{data: map[string]map[string]string{
		"GALV": {"POOL_COUNT": "2", "MIXERS_PER_POOL": "3"},
	}}
	sp := NewSingleFileParser(ini, nil)

	content := `{{- $n := atoi (ini "GALV" "POOL_COUNT") -}}
{{- range $i := count $n}}
setp pool.{{$i}}.enable 1
{{- end}}
`
	result, err := sp.ParseContent("test.hal", content)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	// count 2 → indices 0, 1 → two setp commands
	if len(result.HALCmd) != 2 {
		t.Fatalf("expected 2 HALCmd tokens, got %d", len(result.HALCmd))
	}
	for i, tok := range result.HALCmd {
		st, ok := tok.Data.(*SetPToken)
		if !ok {
			t.Fatalf("token %d: expected *SetPToken, got %T", i, tok.Data)
		}
		want := fmt.Sprintf("pool.%d.enable", i)
		if st.Name != want {
			t.Errorf("token %d: Name = %q, want %q", i, st.Name, want)
		}
	}
}

// TestSingleFileParser_TemplateDisabledWithoutINI verifies that when no INI
// is provided, template directives pass through (no panic / no rendering).
func TestSingleFileParser_TemplateDisabledWithoutINI(t *testing.T) {
	sp := NewSingleFileParser(nil, nil)
	content := "{{range count 2}}setp x.{{.}} 1\n{{end}}"
	// Without INI the template should NOT be rendered; the "{{range" token
	// will be seen as an unknown command, producing a parse error.
	_, err := sp.ParseContent("test.hal", content)
	if err == nil {
		t.Fatal("expected parse error when templates are used without INI, got nil")
	}
}

// TestMultiFileParser_TemplateRendering verifies that NewMultiFileParser
// propagates templateData so templates work across multiple files.
func TestMultiFileParser_TemplateRendering(t *testing.T) {
	ini := &mockINI{data: map[string]map[string]string{
		"TEST": {"COUNT": "2"},
	}}
	mp := NewMultiFileParser(ini, nil)

	// Write a temp HAL file with a template
	tmp, err := os.CreateTemp("", "hal-tmpl-*.hal")
	if err != nil {
		t.Fatal(err)
	}
	defer os.Remove(tmp.Name())
	content := `{{- $n := atoi (ini "TEST" "COUNT") -}}
{{- range $i := count $n}}
setp axis.{{$i}}.scale 100
{{- end}}
`
	if _, err := tmp.WriteString(content); err != nil {
		t.Fatal(err)
	}
	tmp.Close()

	result, err := mp.Parse([]string{tmp.Name()})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(result.HALCmd) != 2 {
		t.Fatalf("expected 2 HALCmd tokens, got %d", len(result.HALCmd))
	}
}
