package hal

import (
	"fmt"
	"sort"
	"strings"
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

// executeToken dispatches a single Token to the corresponding Layer 2
// command function in command.go.
func executeToken(tok Token) error {
	var err error
	switch d := tok.Data.(type) {
	case *LoadRTToken:
		err = LoadRT(d.Comp, buildLoadRTArgs(d)...)
	case *LoadUSRToken:
		err = LoadUSR(loadUSROpts(d), d.Prog, d.Args...)
	case *NetToken:
		err = Net(d.Signal, d.Pins...)
	case *SetPToken:
		err = SetP(d.Name, d.Value)
	case *SetSToken:
		err = SetS(d.Name, d.Value)
	case *GetPToken:
		_, err = GetP(d.Name)
	case *GetSToken:
		_, err = GetS(d.Name)
	case *AddFToken:
		err = AddF(d.Funct, d.Thread, d.Pos)
	case *DelFToken:
		err = DelF(d.Funct, d.Thread)
	case *NewSigToken:
		err = NewSig(d.Name, d.SigType)
	case *DelSigToken:
		err = DelSig(d.Name)
	case *LinkPSToken:
		err = LinkPS(d.Pin, d.Sig)
	case *LinkSPToken:
		err = LinkSP(d.Sig, d.Pin)
	case *LinkPPToken:
		// linkpp is deprecated; treat Pin2 as signal name (same as linkps)
		err = LinkPS(d.Pin1, d.Pin2)
	case *UnlinkPToken:
		err = UnlinkP(d.Pin)
	case *AliasToken:
		err = Alias(aliasKindStr(d.Kind), d.Name, d.Alias)
	case *UnAliasToken:
		err = UnAlias(aliasKindStr(d.Kind), d.Name)
	case *StartToken:
		err = StartThreads()
	case *StopToken:
		err = StopThreads()
	case *LockToken:
		err = halSetLock(int(d.Level))
	case *UnlockToken:
		err = halSetLock(int(d.Level))
	case *UnloadRTToken:
		err = UnloadRT(d.Comp)
	case *UnloadUSRToken:
		err = UnloadUSR(d.Comp)
	case *UnloadToken:
		err = Unload(d.Comp)
	case *WaitUSRToken:
		err = WaitUSR(d.Comp)
	case *ListToken:
		_, err = List(halObjTypeToString(d.ObjType), d.Patterns...)
	case *ShowToken:
		_, err = Show(halObjTypeToString(d.ObjType), d.Patterns...)
	case *SaveToken:
		_, err = Save(saveTypeToString(d.SaveType), d.File)
	case *StatusToken:
		_, err = Status()
	case *DebugToken:
		err = SetDebug(d.Level)
	case *PTypeToken:
		_, err = PType(d.Name)
	case *STypeToken:
		_, err = SType(d.Name)
	case *EchoToken:
		return nil
	case *UnEchoToken:
		return nil
	case *PrintToken:
		return nil
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

// Execute runs all commands in a ParseResult in the correct order:
// 1. Merged loadrt commands (via TwopassCollector)
// 2. loadusr commands (in order)
// 3. All other HAL commands (in order)
// Returns the first error encountered.
func (r *ParseResult) Execute() error {
	// Phase 1: merge and execute loadrt tokens
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
		if err := LoadRT(cmd[0], cmd[1:]...); err != nil {
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

	// Phase 2: execute loadusr tokens
	for _, tok := range r.LoadUSR {
		if err := executeToken(tok); err != nil {
			return err
		}
	}

	// Phase 3: execute remaining HAL commands
	for _, tok := range r.HALCmd {
		if err := executeToken(tok); err != nil {
			return err
		}
	}

	return nil
}

// buildLoadRTArgs reconstructs the string args from a LoadRTToken for LoadRT().
// This is used only by executeToken() for direct single-token dispatch (e.g.
// interactive halcmd calls). ParseResult.Execute() does NOT use this function;
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

// loadUSROpts converts a LoadUSRToken to *LoadUSROptions for LoadUSR().
func loadUSROpts(d *LoadUSRToken) *LoadUSROptions {
	return &LoadUSROptions{
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
