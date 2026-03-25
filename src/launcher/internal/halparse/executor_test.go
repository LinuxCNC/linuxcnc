//go:build !cgo

package halparse

import (
	"errors"
	"strings"
	"testing"

	halcmd "github.com/sittner/linuxcnc/src/launcher/internal/halcmd"
	hal "github.com/sittner/linuxcnc/src/launcher/pkg/hal"
)

// TestExecuteToken_AllTypes verifies that executeToken dispatches each concrete
// TokenData type to the correct command function.  For non-no-op tokens the
// returned error must be halcmd.ErrNoCGO (proving dispatch reached the C shim).
// For no-op tokens (EchoToken, UnEchoToken, PrintToken) the return must be nil.
func TestExecuteToken_AllTypes(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 1}

	noCGO := func(t *testing.T, tok Token) {
		t.Helper()
		err := executeToken(tok)
		if err == nil {
			t.Fatal("expected error, got nil")
		}
		if !errors.Is(err, halcmd.ErrNoCGO) {
			t.Errorf("expected ErrNoCGO, got %v", err)
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
		{"NewSig", Token{loc, &NewSigToken{Name: "s", SigType: hal.TypeBit}}, noCGO},
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
		{"Load", Token{loc, &LoadToken{Path: "/tmp/foo.so", Args: []string{"a=1"}, Params: "a=1"}}, noCGO},
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
		Err: halcmd.ErrNoCGO,
	}
	want := "file.hal:42: " + halcmd.ErrNoCGO.Error()
	if got := e.Error(); got != want {
		t.Errorf("ExecutionError.Error() = %q, want %q", got, want)
	}
}

// TestExecutionError_Unwrap verifies that errors.Is works through ExecutionError.
func TestExecutionError_Unwrap(t *testing.T) {
	e := &ExecutionError{
		Loc: SourceLoc{File: "file.hal", Line: 42},
		Err: halcmd.ErrNoCGO,
	}
	if !errors.Is(e, halcmd.ErrNoCGO) {
		t.Error("errors.Is(execErr, ErrNoCGO) returned false, want true")
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

// TestParseResultLoad verifies that Load returns ErrNoCGO from the first
// loadusr call when LoadUSR tokens are present.
func TestParseResultLoad(t *testing.T) {
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
	err := r.Load(nil)
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !errors.Is(err, halcmd.ErrNoCGO) {
		t.Errorf("expected ErrNoCGO, got %v", err)
	}
}

// TestParseResultExecute verifies that Execute returns ErrNoCGO from the first
// HALCmd token.
func TestParseResultExecute(t *testing.T) {
	r := &ParseResult{
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
	if !errors.Is(err, halcmd.ErrNoCGO) {
		t.Errorf("expected ErrNoCGO, got %v", err)
	}
}

// TestParseResultLoad_Empty verifies that an empty ParseResult's Load returns nil.
func TestParseResultLoad_Empty(t *testing.T) {
	r := &ParseResult{}
	if err := r.Load(nil); err != nil {
		t.Errorf("expected nil for empty ParseResult Load, got %v", err)
	}
}

// TestParseResultExecute_Empty verifies that an empty ParseResult's Execute returns nil.
func TestParseResultExecute_Empty(t *testing.T) {
	r := &ParseResult{}
	if err := r.Execute(); err != nil {
		t.Errorf("expected nil for empty ParseResult Execute, got %v", err)
	}
}

// TestParseResultLoad_LoadRTMerge verifies that two LoadRTTokens for the
// same module are merged into a single LoadRT call (one error, not two).
func TestParseResultLoad_LoadRTMerge(t *testing.T) {
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
	err := r.Load(nil)
	// Both tokens are for the same module; TwopassCollector merges them into a
	// single LoadRT("and2", "count=3") call.  The call fails with ErrNoCGO.
	if err == nil {
		t.Fatal("expected error from merged loadrt, got nil")
	}
	if !errors.Is(err, halcmd.ErrNoCGO) {
		t.Errorf("expected ErrNoCGO, got %v", err)
	}
}

// TestParseResultIterLoads verifies that IterLoads calls the callback for each
// LoadToken in Loads, providing the correct path, name, and args.
func TestParseResultIterLoads(t *testing.T) {
	r := &ParseResult{
		Loads: []Token{
			{
				Location: SourceLoc{File: "test.hal", Line: 1},
				Data:     &LoadToken{Path: "/tmp/foo.so", Args: []string{"a=1", "b=2"}},
			},
			{
				Location: SourceLoc{File: "test.hal", Line: 2},
				Data:     &LoadToken{Path: "/tmp/bar.so", Args: nil},
			},
		},
	}

	type call struct {
		path string
		name string
		args []string
	}
	var calls []call

	err := r.IterLoads(func(path string, name string, args []string) error {
		calls = append(calls, call{path, name, args})
		return nil
	})
	if err != nil {
		t.Fatalf("IterLoads returned unexpected error: %v", err)
	}
	if len(calls) != 2 {
		t.Fatalf("expected 2 calls, got %d", len(calls))
	}
	if calls[0].path != "/tmp/foo.so" {
		t.Errorf("calls[0].path = %q, want %q", calls[0].path, "/tmp/foo.so")
	}
	if calls[0].name != "foo" {
		t.Errorf("calls[0].name = %q, want %q", calls[0].name, "foo")
	}
	if len(calls[0].args) != 2 || calls[0].args[0] != "a=1" || calls[0].args[1] != "b=2" {
		t.Errorf("calls[0].args = %v, want [a=1 b=2]", calls[0].args)
	}
	if calls[1].path != "/tmp/bar.so" {
		t.Errorf("calls[1].path = %q, want %q", calls[1].path, "/tmp/bar.so")
	}
	if calls[1].name != "bar" {
		t.Errorf("calls[1].name = %q, want %q", calls[1].name, "bar")
	}
}

// TestParseResultIterLoads_MultiInstance verifies that IterLoads expands
// the Names list into one callback per instance name.
func TestParseResultIterLoads_MultiInstance(t *testing.T) {
	r := &ParseResult{
		Loads: []Token{
			{
				Location: SourceLoc{File: "test.hal", Line: 1},
				Data:     &LoadToken{Path: "mymod", Names: []string{"inst1", "inst2"}, Args: []string{"x=1"}},
			},
		},
	}

	type call struct {
		path string
		name string
		args []string
	}
	var calls []call

	err := r.IterLoads(func(path string, name string, args []string) error {
		calls = append(calls, call{path, name, args})
		return nil
	})
	if err != nil {
		t.Fatalf("IterLoads returned unexpected error: %v", err)
	}
	if len(calls) != 2 {
		t.Fatalf("expected 2 calls, got %d", len(calls))
	}
	if calls[0].name != "inst1" {
		t.Errorf("calls[0].name = %q, want %q", calls[0].name, "inst1")
	}
	if calls[1].name != "inst2" {
		t.Errorf("calls[1].name = %q, want %q", calls[1].name, "inst2")
	}
	// Both calls should receive the same path and args
	for i, c := range calls {
		if c.path != "mymod" {
			t.Errorf("calls[%d].path = %q, want %q", i, c.path, "mymod")
		}
		if len(c.args) != 1 || c.args[0] != "x=1" {
			t.Errorf("calls[%d].args = %v, want [x=1]", i, c.args)
		}
	}
}

// TestParseResultIterLoads_ErrorPropagation verifies that IterLoads stops on
// the first error and wraps it with the source location.
func TestParseResultIterLoads_ErrorPropagation(t *testing.T) {
	loc := SourceLoc{File: "test.hal", Line: 7}
	r := &ParseResult{
		Loads: []Token{
			{Location: loc, Data: &LoadToken{Path: "/tmp/fail.so"}},
		},
	}

	sentinelErr := errors.New("intentional failure")
	err := r.IterLoads(func(_ string, _ string, _ []string) error {
		return sentinelErr
	})
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	if !errors.Is(err, sentinelErr) {
		t.Errorf("expected sentinel error via errors.Is, got %v", err)
	}
	// ExecutionError should include the source location
	if !strings.Contains(err.Error(), "test.hal:7") {
		t.Errorf("error %q should contain source location 'test.hal:7'", err.Error())
	}
}
