package halparse

import (
	"fmt"
	"sort"
	"strings"

	halcmd "github.com/sittner/linuxcnc/src/launcher/internal/halcmd"
)

// ExecutionError wraps a SourceLoc and an underlying error for reporting
// execution failures with file:line context.
type ExecutionError struct {
	Loc SourceLoc
	Err error
}

func (e *ExecutionError) Error() string {
	return fmt.Sprintf("%s:%d: %s", e.Loc.File, e.Loc.Line, e.Err)
}

func (e *ExecutionError) Unwrap() error { return e.Err }

// executeToken dispatches a single Token to the corresponding halcmd function.
func executeToken(tok Token) error {
	var err error
	switch d := tok.Data.(type) {
	case *LoadRTToken:
		err = halcmd.LoadRT(d.Comp, buildLoadRTArgs(d)...)
	case *LoadUSRToken:
		err = halcmd.LoadUSR(loadUSROpts(d), d.Prog, d.Args...)
	case *NetToken:
		err = halcmd.Net(d.Signal, d.Pins...)
	case *SetPToken:
		err = halcmd.SetP(d.Name, d.Value)
	case *SetSToken:
		err = halcmd.SetS(d.Name, d.Value)
	case *GetPToken:
		_, err = halcmd.GetP(d.Name)
	case *GetSToken:
		_, err = halcmd.GetS(d.Name)
	case *AddFToken:
		err = halcmd.AddF(d.Funct, d.Thread, d.Pos)
	case *DelFToken:
		err = halcmd.DelF(d.Funct, d.Thread)
	case *NewSigToken:
		err = halcmd.NewSig(d.Name, d.SigType)
	case *DelSigToken:
		err = halcmd.DelSig(d.Name)
	case *LinkPSToken:
		err = halcmd.LinkPS(d.Pin, d.Sig)
	case *LinkSPToken:
		err = halcmd.LinkSP(d.Sig, d.Pin)
	case *LinkPPToken:
		// linkpp is deprecated; treat Pin2 as signal name (same as linkps)
		err = halcmd.LinkPS(d.Pin1, d.Pin2)
	case *UnlinkPToken:
		err = halcmd.UnlinkP(d.Pin)
	case *AliasToken:
		err = halcmd.Alias(aliasKindStr(d.Kind), d.Name, d.Alias)
	case *UnAliasToken:
		err = halcmd.UnAlias(aliasKindStr(d.Kind), d.Name)
	case *StartToken:
		err = halcmd.StartThreads()
	case *StopToken:
		err = halcmd.StopThreads()
	case *LockToken:
		err = halcmd.SetLock(int(d.Level))
	case *UnlockToken:
		// unlock semantics: clear the specified lock bits.
		// e.g. "unlock all" (level=255) → set lock to 0 (LockNone)
		// e.g. "unlock tune" (level=3)  → set lock to LockAll &^ 3
		err = halcmd.SetLock(int(LockAll) &^ int(d.Level))
	case *UnloadRTToken:
		err = halcmd.UnloadRT(d.Comp)
	case *UnloadUSRToken:
		err = halcmd.UnloadUSR(d.Comp)
	case *UnloadToken:
		err = halcmd.Unload(d.Comp)
	case *WaitUSRToken:
		err = halcmd.WaitUSR(d.Comp)
	case *ListToken:
		_, err = halcmd.List(halObjTypeToString(d.ObjType), d.Patterns...)
	case *ShowToken:
		_, err = halcmd.Show(halObjTypeToString(d.ObjType), d.Patterns...)
	case *SaveToken:
		_, err = halcmd.Save(saveTypeToString(d.SaveType), d.File)
	case *StatusToken:
		_, err = halcmd.Status()
	case *DebugToken:
		err = halcmd.SetDebug(d.Level)
	case *PTypeToken:
		_, err = halcmd.PType(d.Name)
	case *STypeToken:
		_, err = halcmd.SType(d.Name)
	case *EchoToken:
		return nil
	case *UnEchoToken:
		return nil
	case *PrintToken:
		return nil
	case *LoadToken:
		// The "load" command is for C and Go plugin modules.  It is handled
		// by the launcher via IterLoads, never by executeToken directly.
		return &ExecutionError{
			Loc: tok.Location,
			Err: fmt.Errorf("load command cannot be executed directly; use the launcher"),
		}
	default:
		return &ExecutionError{
			Loc: tok.Location,
			Err: fmt.Errorf("unknown token type %T", tok.Data),
		}
	}
	if err != nil {
		return &ExecutionError{Loc: tok.Location, Err: err}
	}
	return nil
}

// ExecLoadUSR executes the loadusr phase: all "loadusr" commands with their
// -W/-Wn wait flags.  Call this before loading plugin modules and before
// ExecLoadRT.
func (r *ParseResult) ExecLoadUSR() error {
	for _, tok := range r.LoadUSR {
		if err := executeToken(tok); err != nil {
			return err
		}
	}
	return nil
}

// ExecLoadRT executes the loadrt phase: all "loadrt" commands are merged via
// TwopassCollector and then loaded.  Call this after ExecLoadUSR and after any
// plugin modules have been loaded.
func (r *ParseResult) ExecLoadRT() error {
	collector := NewTwopassCollector()
	for _, tok := range r.LoadRT {
		if d, ok := tok.Data.(*LoadRTToken); ok {
			collector.CollectLoadRTToken(d)
		}
	}
	mergedCmds := collector.MergedLoadRTCommands()
	for _, cmd := range mergedCmds {
		if len(cmd) == 0 {
			continue
		}
		if err := halcmd.LoadRT(cmd[0], cmd[1:]...); err != nil {
			loc := SourceLoc{}
			// Find the first LoadRT token for this module to get a source loc
			for _, tok := range r.LoadRT {
				if d, ok := tok.Data.(*LoadRTToken); ok && d.Comp == cmd[0] {
					loc = tok.Location
					break
				}
			}
			return &ExecutionError{Loc: loc, Err: err}
		}
	}
	return nil
}

// Execute runs the remaining HAL commands (net, addf, setp, etc.).
// Must be called after Load().
func (r *ParseResult) Execute() error {
	for _, tok := range r.HALCmd {
		if err := executeToken(tok); err != nil {
			return err
		}
	}

	return nil
}

// IterLoads calls fn once for each instance described by "load" command tokens
// in ParseResult.Loads, passing the module path, instance name, and argument
// slice.
//
// When a LoadToken has no explicit Names, a single call is made with the default
// name derived from the module path (basename without .so).  When Names are set
// (via the [name1,name2,...] syntax), fn is called once per name.
func (r *ParseResult) IterLoads(fn func(path string, name string, args []string) error) error {
	for _, tok := range r.Loads {
		d, ok := tok.Data.(*LoadToken)
		if !ok {
			continue
		}
		names := d.Names
		if len(names) == 0 {
			// Default: basename without .so extension.
			base := d.Path
			if i := strings.LastIndex(base, "/"); i >= 0 {
				base = base[i+1:]
			}
			base = strings.TrimSuffix(base, ".so")
			names = []string{base}
		}
		for _, name := range names {
			if err := fn(d.Path, name, d.Args); err != nil {
				return &ExecutionError{Loc: tok.Location, Err: err}
			}
		}
	}
	return nil
}

// buildLoadRTArgs reconstructs the string args from a LoadRTToken for LoadRT().
// This is used only by executeToken() for direct single-token dispatch (e.g.
// interactive halcmd calls). ParseResult.LoadRT() does NOT use this function;
// it feeds LoadRTTokens through TwopassCollector.CollectLoadRTToken() →
// MergedLoadRTCommands() instead.
func buildLoadRTArgs(d *LoadRTToken) []string {
	var args []string
	if d.Count > 0 {
		args = append(args, fmt.Sprintf("count=%d", d.Count))
	}
	if len(d.Names) > 0 {
		args = append(args, fmt.Sprintf("names=%s", strings.Join(d.Names, ",")))
	}
	// Sort Params keys for deterministic ordering
	keys := make([]string, 0, len(d.Params))
	for k := range d.Params {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		args = append(args, fmt.Sprintf("%s=%s", k, d.Params[k]))
	}
	return args
}

// loadUSROpts converts a LoadUSRToken to *halcmd.LoadUSROptions for halcmd.LoadUSR().
func loadUSROpts(d *LoadUSRToken) *halcmd.LoadUSROptions {
	return &halcmd.LoadUSROptions{
		WaitReady:   d.WaitReady,
		WaitName:    d.WaitName,
		WaitExit:    d.WaitExit,
		NoStdin:     d.NoStdin,
		TimeoutSecs: d.Timeout,
	}
}

// aliasKindStr converts an AliasKind enum to the string accepted by Alias()/UnAlias().
func aliasKindStr(k AliasKind) string {
	switch k {
	case AliasPin:
		return "pin"
	case AliasParam:
		return "param"
	default:
		return "pin"
	}
}

// halObjTypeToString converts a HalObjType enum to the string accepted by List()/Show().
func halObjTypeToString(t HalObjType) string {
	switch t {
	case ObjPin:
		return "pin"
	case ObjSig:
		return "sig"
	case ObjParam:
		return "param"
	case ObjFunct:
		return "funct"
	case ObjThread:
		return "thread"
	case ObjComp:
		return "comp"
	case ObjAll:
		return "all"
	default:
		return "all"
	}
}

// saveTypeToString converts a SaveType enum to the string accepted by Save().
func saveTypeToString(t SaveType) string {
	switch t {
	case SaveComp:
		return "comp"
	case SaveSig:
		return "sig"
	case SaveLink:
		return "link"
	case SaveNet:
		return "net"
	case SaveParam:
		return "param"
	case SaveThread:
		return "thread"
	case SaveAll:
		return "all"
	default:
		return "all"
	}
}
