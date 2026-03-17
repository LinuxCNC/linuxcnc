//go:build !cgo

package hal

import (
	"errors"
	"strings"
	"testing"
)

// TestExecuteToken_AllTypes verifies that executeToken dispatches each concrete
// TokenData type to the correct command function.  For non-no-op tokens the
// returned error must be errNoCGO (proving dispatch reached the C shim).
// For no-op tokens (EchoToken, UnEchoToken, PrintToken) the return must be nil.
func TestExecuteToken_AllTypes(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	noCGO := func(t *testing.T, tok Token) {
		t.Helper()
		err := executeToken(tok)
		if err == nil {
			t.Fatal("expected error, got nil")
		}
		if !errors.Is(err, errNoCGO) {
			t.Errorf("expected errNoCGO, got %v", err)
		}
	}
	noOp := func(t *testing.T, tok Token) {
		t.Helper()
		if err := executeToken(tok); err != nil {
			t.Errorf("expected nil, got %v", err)
		}
	}

	tests := []struct {
		name  string
		tok   Token
		check func(*testing.T, Token)
	}{
		{"LoadRT", Token{loc, &LoadRTToken{Comp: "pid", Count: 1}}, noCGO},
		{"LoadUSR", Token{loc, &LoadUSRToken{Prog: "halui"}}, noCGO},
		{"Net", Token{loc, &NetToken{Signal: "sig", Pins: []string{"p.pin"}}}, noCGO},
		{"SetP", Token{loc, &SetPToken{Name: "x.y", Value: "1"}}, noCGO},
		{"SetS", Token{loc, &SetSToken{Name: "s", Value: "0"}}, noCGO},
		{"GetP", Token{loc, &GetPToken{Name: "x.y"}}, noCGO},
		{"GetS", Token{loc, &GetSToken{Name: "s"}}, noCGO},
		{"AddF", Token{loc, &AddFToken{Funct: "f", Thread: "t", Pos: -1}}, noCGO},
		{"DelF", Token{loc, &DelFToken{Funct: "f", Thread: "t"}}, noCGO},
		{"NewSig", Token{loc, &NewSigToken{Name: "s", SigType: TypeBit}}, noCGO},
		{"DelSig", Token{loc, &DelSigToken{Name: "s"}}, noCGO},
		{"LinkPS", Token{loc, &LinkPSToken{Pin: "p", Sig: "s"}}, noCGO},
		{"LinkSP", Token{loc, &LinkSPToken{Sig: "s", Pin: "p"}}, noCGO},
		{"LinkPP", Token{loc, &LinkPPToken{Pin1: "p1", Pin2: "p2"}}, noCGO},
		{"UnlinkP", Token{loc, &UnlinkPToken{Pin: "p"}}, noCGO},
		{"Alias", Token{loc, &AliasToken{Kind: AliasPin, Name: "p", Alias: "a"}}, noCGO},
		{"UnAlias", Token{loc, &UnAliasToken{Kind: AliasParam, Name: "p"}}, noCGO},
		{"Start", Token{loc, &StartToken{}}, noCGO},
		{"Stop", Token{loc, &StopToken{}}, noCGO},
		{"Lock", Token{loc, &LockToken{Level: LockAll}}, noCGO},
		{"Unlock", Token{loc, &UnlockToken{Level: LockNone}}, noCGO},
		{"UnloadRT", Token{loc, &UnloadRTToken{Comp: "c"}}, noCGO},
		{"UnloadUSR", Token{loc, &UnloadUSRToken{Comp: "c"}}, noCGO},
		{"Unload", Token{loc, &UnloadToken{Comp: "c"}}, noCGO},
		{"WaitUSR", Token{loc, &WaitUSRToken{Comp: "c"}}, noCGO},
		{"List", Token{loc, &ListToken{ObjType: ObjPin}}, noCGO},
		{"Show", Token{loc, &ShowToken{ObjType: ObjAll}}, noCGO},
		{"Save", Token{loc, &SaveToken{SaveType: SaveAll}}, noCGO},
		{"Status", Token{loc, &StatusToken{}}, noCGO},
		{"Debug", Token{loc, &DebugToken{Level: 3}}, noCGO},
		{"PType", Token{loc, &PTypeToken{Name: "x.y"}}, noCGO},
		{"SType", Token{loc, &STypeToken{Name: "s"}}, noCGO},
		{"Echo", Token{loc, &EchoToken{}}, noOp},
		{"UnEcho", Token{loc, &UnEchoToken{}}, noOp},
		{"Print", Token{loc, &PrintToken{Message: "hello"}}, noOp},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			tc.check(t, tc.tok)
		})
	}
}

// TestExecuteToken_UnknownType verifies that a Token with nil Data returns an
// error mentioning "unknown token type".
func TestExecuteToken_UnknownType(t *testing.T) {
	tok := Token{Location: SourceLoc{File: "f.hal", Line: 7}, Data: nil}
	err := executeToken(tok)
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !strings.Contains(err.Error(), "unknown token type") {
		t.Errorf("expected 'unknown token type' in error, got %q", err.Error())
	}
}

// TestExecutionError_Format verifies the "file:line: message" format.
func TestExecutionError_Format(t *testing.T) {
	e := &ExecutionError{
		Loc: SourceLoc{File: "file.hal", Line: 42},
		Err: errNoCGO,
	}
	want := "file.hal:42: " + errNoCGO.Error()
	if got := e.Error(); got != want {
		t.Errorf("ExecutionError.Error() = %q, want %q", got, want)
	}
}

// TestExecutionError_Unwrap verifies that errors.Is works through ExecutionError.
func TestExecutionError_Unwrap(t *testing.T) {
	e := &ExecutionError{
		Loc: SourceLoc{File: "file.hal", Line: 42},
		Err: errNoCGO,
	}
	if !errors.Is(e, errNoCGO) {
		t.Error("errors.Is(execErr, errNoCGO) returned false, want true")
	}
}

// TestBuildLoadRTArgs verifies the helper reconstructs args from a LoadRTToken.
func TestBuildLoadRTArgs(t *testing.T) {
	d := &LoadRTToken{
		Comp:   "pid",
		Count:  3,
		Names:  []string{"a", "b"},
		Params: map[string]string{"debug": "1", "cfg": "foo"},
	}
	args := buildLoadRTArgs(d)

	mustContain := func(s string) {
		t.Helper()
		for _, a := range args {
			if a == s {
				return
			}
		}
		t.Errorf("expected arg %q in %v", s, args)
	}

	mustContain("count=3")
	mustContain("names=a,b")
	mustContain("debug=1")
	mustContain("cfg=foo")
}

// TestBuildLoadRTArgs_Empty verifies empty token produces empty args.
func TestBuildLoadRTArgs_Empty(t *testing.T) {
	d := &LoadRTToken{Comp: "and2"}
	args := buildLoadRTArgs(d)
	if len(args) != 0 {
		t.Errorf("expected no args for empty token, got %v", args)
	}
}

// TestLoadUSROpts verifies the helper maps LoadUSRToken fields to LoadUSROptions.
func TestLoadUSROpts(t *testing.T) {
	d := &LoadUSRToken{
		WaitReady: true,
		WaitName:  "mycomp",
		WaitExit:  false,
		NoStdin:   true,
		Timeout:   15,
	}
	opts := loadUSROpts(d)
	if !opts.WaitReady {
		t.Error("WaitReady should be true")
	}
	if opts.WaitName != "mycomp" {
		t.Errorf("WaitName = %q, want %q", opts.WaitName, "mycomp")
	}
	if opts.WaitExit {
		t.Error("WaitExit should be false")
	}
	if !opts.NoStdin {
		t.Error("NoStdin should be true")
	}
	if opts.TimeoutSecs != 15 {
		t.Errorf("TimeoutSecs = %d, want 15", opts.TimeoutSecs)
	}
}

// TestHalObjTypeToString verifies all enum values map to the correct string.
func TestHalObjTypeToString(t *testing.T) {
	tests := []struct {
		in   HalObjType
		want string
	}{
		{ObjPin, "pin"},
		{ObjSig, "sig"},
		{ObjParam, "param"},
		{ObjFunct, "funct"},
		{ObjThread, "thread"},
		{ObjComp, "comp"},
		{ObjAll, "all"},
		{HalObjType(999), "all"}, // default
	}
	for _, tc := range tests {
		if got := halObjTypeToString(tc.in); got != tc.want {
			t.Errorf("halObjTypeToString(%d) = %q, want %q", tc.in, got, tc.want)
		}
	}
}

// TestSaveTypeToString verifies all enum values map to the correct string.
func TestSaveTypeToString(t *testing.T) {
	tests := []struct {
		in   SaveType
		want string
	}{
		{SaveComp, "comp"},
		{SaveSig, "sig"},
		{SaveLink, "link"},
		{SaveNet, "net"},
		{SaveParam, "param"},
		{SaveThread, "thread"},
		{SaveAll, "all"},
		{SaveType(999), "all"}, // default
	}
	for _, tc := range tests {
		if got := saveTypeToString(tc.in); got != tc.want {
			t.Errorf("saveTypeToString(%d) = %q, want %q", tc.in, got, tc.want)
		}
	}
}

// TestAliasKindStr verifies both enum values and the default.
func TestAliasKindStr(t *testing.T) {
	if got := aliasKindStr(AliasPin); got != "pin" {
		t.Errorf("aliasKindStr(AliasPin) = %q, want %q", got, "pin")
	}
	if got := aliasKindStr(AliasParam); got != "param" {
		t.Errorf("aliasKindStr(AliasParam) = %q, want %q", got, "param")
	}
	if got := aliasKindStr(AliasKind(99)); got != "pin" {
		t.Errorf("aliasKindStr(unknown) = %q, want %q", got, "pin")
	}
}

// TestParseResultExecute verifies that Execute returns errNoCGO from the first
// merged loadrt call when LoadRT tokens are present.
func TestParseResultExecute(t *testing.T) {
	r := &ParseResult{
		LoadRT: []Token{
			{
				Location: SourceLoc{File: "test.hal", Line: 1},
				Data:     &LoadRTToken{Comp: "pid", Count: 2},
			},
		},
		LoadUSR: []Token{
			{
				Location: SourceLoc{File: "test.hal", Line: 2},
				Data:     &LoadUSRToken{Prog: "halui"},
			},
		},
		HALCmd: []Token{
			{
				Location: SourceLoc{File: "test.hal", Line: 3},
				Data:     &SetPToken{Name: "x.y", Value: "1"},
			},
		},
	}
	err := r.Execute()
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !errors.Is(err, errNoCGO) {
		t.Errorf("expected errNoCGO, got %v", err)
	}
}

// TestParseResultExecute_Empty verifies that an empty ParseResult returns nil.
func TestParseResultExecute_Empty(t *testing.T) {
	r := &ParseResult{}
	if err := r.Execute(); err != nil {
		t.Errorf("expected nil for empty ParseResult, got %v", err)
	}
}

// TestParseResultExecute_LoadRTMerge verifies that two LoadRTTokens for the
// same module are merged into a single LoadRT call (one error, not two).
func TestParseResultExecute_LoadRTMerge(t *testing.T) {
	r := &ParseResult{
		LoadRT: []Token{
			{
				Location: SourceLoc{File: "test.hal", Line: 1},
				Data:     &LoadRTToken{Comp: "and2", Count: 2},
			},
			{
				Location: SourceLoc{File: "test.hal", Line: 5},
				Data:     &LoadRTToken{Comp: "and2", Count: 3},
			},
		},
	}
	err := r.Execute()
	// Both tokens are for the same module; TwopassCollector merges them into a
	// single LoadRT("and2", "count=3") call.  The call fails with errNoCGO.
	if err == nil {
		t.Fatal("expected error from merged loadrt, got nil")
	}
	if !errors.Is(err, errNoCGO) {
		t.Errorf("expected errNoCGO, got %v", err)
	}
}
