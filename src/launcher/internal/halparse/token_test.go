package halparse

import (
	"testing"

	hal "linuxcnc.org/hal"
)

// Compile-time interface compliance checks for all 35 token structs.
var (
	_ TokenData = (*LoadRTToken)(nil)
	_ TokenData = (*LoadUSRToken)(nil)
	_ TokenData = (*NetToken)(nil)
	_ TokenData = (*SetPToken)(nil)
	_ TokenData = (*SetSToken)(nil)
	_ TokenData = (*GetPToken)(nil)
	_ TokenData = (*GetSToken)(nil)
	_ TokenData = (*AddFToken)(nil)
	_ TokenData = (*DelFToken)(nil)
	_ TokenData = (*NewSigToken)(nil)
	_ TokenData = (*DelSigToken)(nil)
	_ TokenData = (*LinkPSToken)(nil)
	_ TokenData = (*LinkSPToken)(nil)
	_ TokenData = (*LinkPPToken)(nil)
	_ TokenData = (*UnlinkPToken)(nil)
	_ TokenData = (*AliasToken)(nil)
	_ TokenData = (*UnAliasToken)(nil)
	_ TokenData = (*StartToken)(nil)
	_ TokenData = (*StopToken)(nil)
	_ TokenData = (*LockToken)(nil)
	_ TokenData = (*UnlockToken)(nil)
	_ TokenData = (*UnloadRTToken)(nil)
	_ TokenData = (*UnloadUSRToken)(nil)
	_ TokenData = (*UnloadToken)(nil)
	_ TokenData = (*WaitUSRToken)(nil)
	_ TokenData = (*ListToken)(nil)
	_ TokenData = (*ShowToken)(nil)
	_ TokenData = (*SaveToken)(nil)
	_ TokenData = (*StatusToken)(nil)
	_ TokenData = (*DebugToken)(nil)
	_ TokenData = (*PTypeToken)(nil)
	_ TokenData = (*STypeToken)(nil)
	_ TokenData = (*EchoToken)(nil)
	_ TokenData = (*UnEchoToken)(nil)
	_ TokenData = (*PrintToken)(nil)
)

func TestParseErrorFormat(t *testing.T) {
	err := &ParseError{
		Loc: SourceLoc{File: "file.hal", Line: 10},
		Msg: "some error message",
	}
	want := "file.hal:10: some error message"
	if got := err.Error(); got != want {
		t.Errorf("ParseError.Error() = %q, want %q", got, want)
	}
}

func TestTokenHoldsData(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 5}

	t.Run("NetToken", func(t *testing.T) {
		tok := Token{
			Location: loc,
			Data:     &NetToken{Signal: "mysig", Pins: []string{"comp.pin0", "comp.pin1"}},
		}
		nt, ok := tok.Data.(*NetToken)
		if !ok {
			t.Fatal("type assertion to *NetToken failed")
		}
		if nt.Signal != "mysig" {
			t.Errorf("Signal = %q, want %q", nt.Signal, "mysig")
		}
	})

	t.Run("LoadRTToken", func(t *testing.T) {
		tok := Token{
			Location: loc,
			Data: &LoadRTToken{
				Comp:  "hal_lib",
				Count: 2,
				Names: []string{"a", "b"},
			},
		}
		lt, ok := tok.Data.(*LoadRTToken)
		if !ok {
			t.Fatal("type assertion to *LoadRTToken failed")
		}
		if lt.Comp != "hal_lib" {
			t.Errorf("Comp = %q, want %q", lt.Comp, "hal_lib")
		}
	})

	t.Run("SetPToken", func(t *testing.T) {
		tok := Token{
			Location: loc,
			Data:     &SetPToken{Name: "comp.param", Value: "3.14"},
		}
		st, ok := tok.Data.(*SetPToken)
		if !ok {
			t.Fatal("type assertion to *SetPToken failed")
		}
		if st.Value != "3.14" {
			t.Errorf("Value = %q, want %q", st.Value, "3.14")
		}
	})
}

func TestLockLevelValues(t *testing.T) {
	tests := []struct {
		name string
		got  LockLevel
		want LockLevel
	}{
		{"LockNone", LockNone, 0},
		{"LockLoad", LockLoad, 1},
		{"LockConfig", LockConfig, 2},
		{"LockTune", LockTune, 3},
		{"LockParams", LockParams, 4},
		{"LockRun", LockRun, 8},
		{"LockAll", LockAll, 255},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.want {
				t.Errorf("%s = %d, want %d", tc.name, tc.got, tc.want)
			}
		})
	}
}

func TestAliasKindValues(t *testing.T) {
	if AliasPin != 0 {
		t.Errorf("AliasPin = %d, want 0", AliasPin)
	}
	if AliasParam != 1 {
		t.Errorf("AliasParam = %d, want 1", AliasParam)
	}
}

func TestHalObjTypeIota(t *testing.T) {
	tests := []struct {
		name string
		got  HalObjType
		want HalObjType
	}{
		{"ObjPin", ObjPin, 0},
		{"ObjSig", ObjSig, 1},
		{"ObjParam", ObjParam, 2},
		{"ObjFunct", ObjFunct, 3},
		{"ObjThread", ObjThread, 4},
		{"ObjComp", ObjComp, 5},
		{"ObjAll", ObjAll, 6},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.want {
				t.Errorf("%s = %d, want %d", tc.name, tc.got, tc.want)
			}
		})
	}
}

func TestSaveTypeIota(t *testing.T) {
	tests := []struct {
		name string
		got  SaveType
		want SaveType
	}{
		{"SaveComp", SaveComp, 0},
		{"SaveSig", SaveSig, 1},
		{"SaveLink", SaveLink, 2},
		{"SaveNet", SaveNet, 3},
		{"SaveParam", SaveParam, 4},
		{"SaveThread", SaveThread, 5},
		{"SaveAll", SaveAll, 6},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.got != tc.want {
				t.Errorf("%s = %d, want %d", tc.name, tc.got, tc.want)
			}
		})
	}
}

func TestNewSigTokenUsesPinType(t *testing.T) {
	tok := NewSigToken{Name: "mysig", SigType: hal.TypeBit}
	if tok.SigType != hal.TypeBit {
		t.Errorf("SigType = %d, want TypeBit (%d)", tok.SigType, hal.TypeBit)
	}
	if int(tok.SigType) != 1 {
		t.Errorf("TypeBit value = %d, want 1", int(tok.SigType))
	}
}
