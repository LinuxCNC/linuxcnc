package halparse

import (
	"fmt"

	hal "github.com/sittner/linuxcnc/src/launcher/pkg/hal"
)

// SourceLoc records the file and line number where a token originated.
// Used for error messages that refer back to the original HAL file.
type SourceLoc struct {
	File string
	Line int
}

// ParseError is returned by LineParser when a line cannot be parsed.
// It always carries the SourceLoc of the offending line.
type ParseError struct {
	Loc SourceLoc
	Msg string
}

func (e *ParseError) Error() string {
	return fmt.Sprintf("%s:%d: %s", e.Loc.File, e.Loc.Line, e.Msg)
}

// TokenData is implemented by all per-command token structs.
// The unexported marker method seals the interface to this package —
// only types defined here can implement it.
type TokenData interface {
	tokenData() // unexported marker method seals the interface
}

// Token is the parsed representation of one HAL command line.
// It pairs a SourceLoc (for error reporting even after list reordering)
// with a concrete TokenData value whose type is the command discriminator.
type Token struct {
	Location SourceLoc
	Data     TokenData // exactly one concrete type
}

// AliasKind distinguishes pin aliases from param aliases.
type AliasKind int

const (
	AliasPin AliasKind = iota
	AliasParam
)

// LockLevel maps to HAL's HAL_LOCK_* constants.
type LockLevel int

const (
	LockNone   LockLevel = 0
	LockLoad   LockLevel = 1
	LockConfig LockLevel = 2
	LockTune   LockLevel = 3
	LockParams LockLevel = 4
	LockRun    LockLevel = 8
	LockAll    LockLevel = 255
)

// HalObjType enumerates the object classes accepted by list/show/save commands.
type HalObjType int

const (
	ObjPin HalObjType = iota
	ObjSig
	ObjParam
	ObjFunct
	ObjThread
	ObjComp
	ObjAll
)

// SaveType enumerates the save sub-commands.
type SaveType int

const (
	SaveComp SaveType = iota
	SaveSig
	SaveLink
	SaveNet
	SaveParam
	SaveThread
	SaveAll
)

// LoadRTToken represents a "loadrt" command.
type LoadRTToken struct {
	Comp   string            // module name
	Count  int               // parsed from count=N; 0 if absent
	Names  []string          // parsed from names=a,b,c; nil if absent
	Params map[string]string // remaining key=value args (module-specific)
}

func (*LoadRTToken) tokenData() {}

// LoadUSRToken represents a "loadusr" command.
type LoadUSRToken struct {
	WaitReady bool   // -W flag
	WaitName  string // -Wn <name> flag
	WaitExit  bool   // -w flag
	NoStdin   bool   // -i flag
	Timeout   int    // -T <secs> flag; 0 = default
	Prog      string
	Args      []string
}

func (*LoadUSRToken) tokenData() {}

// NetToken represents a "net" command.
type NetToken struct {
	Signal string   // signal name (first argument)
	Pins   []string // pin names with arrow tokens already stripped
}

func (*NetToken) tokenData() {}

// SetPToken represents a "setp" command.
type SetPToken struct {
	Name  string
	Value string // stays string — C shim interprets based on runtime pin type
}

func (*SetPToken) tokenData() {}

// SetSToken represents a "sets" command.
type SetSToken struct {
	Name  string
	Value string
}

func (*SetSToken) tokenData() {}

// GetPToken represents a "getp" command.
type GetPToken struct{ Name string }

func (*GetPToken) tokenData() {}

// GetSToken represents a "gets" command.
type GetSToken struct{ Name string }

func (*GetSToken) tokenData() {}

// AddFToken represents an "addf" command.
type AddFToken struct {
	Funct  string
	Thread string
	Pos    int // parsed from optional 3rd arg; default -1 (append)
}

func (*AddFToken) tokenData() {}

// DelFToken represents a "delf" command.
type DelFToken struct {
	Funct  string
	Thread string
}

func (*DelFToken) tokenData() {}

// NewSigToken represents a "newsig" command.
type NewSigToken struct {
	Name    string
	SigType hal.PinType
}

func (*NewSigToken) tokenData() {}

// DelSigToken represents a "delsig" command.
type DelSigToken struct{ Name string }

func (*DelSigToken) tokenData() {}

// LinkPSToken represents a "linkps" command (pin-first argument order).
type LinkPSToken struct{ Pin, Sig string }

func (*LinkPSToken) tokenData() {}

// LinkSPToken represents a "linksp" command (signal-first argument order).
type LinkSPToken struct{ Sig, Pin string }

func (*LinkSPToken) tokenData() {}

// LinkPPToken represents a "linkpp" command (deprecated; retained for compatibility).
type LinkPPToken struct {
	Pin1 string
	Pin2 string
}

func (*LinkPPToken) tokenData() {}

// UnlinkPToken represents an "unlinkp" command.
type UnlinkPToken struct{ Pin string }

func (*UnlinkPToken) tokenData() {}

// AliasToken represents an "alias" command.
type AliasToken struct {
	Kind  AliasKind // AliasPin or AliasParam
	Name  string
	Alias string
}

func (*AliasToken) tokenData() {}

// UnAliasToken represents an "unalias" command.
type UnAliasToken struct {
	Kind AliasKind
	Name string
}

func (*UnAliasToken) tokenData() {}

// StartToken represents a "start" command.
type StartToken struct{}

func (*StartToken) tokenData() {}

// StopToken represents a "stop" command.
type StopToken struct{}

func (*StopToken) tokenData() {}

// LockToken represents a "lock" command.
type LockToken struct {
	Level LockLevel
}

func (*LockToken) tokenData() {}

// UnlockToken represents an "unlock" command.
type UnlockToken struct {
	Level LockLevel
}

func (*UnlockToken) tokenData() {}

// UnloadRTToken represents an "unloadrt" command.
type UnloadRTToken struct{ Comp string }

func (*UnloadRTToken) tokenData() {}

// UnloadUSRToken represents an "unloadusr" command.
type UnloadUSRToken struct{ Comp string }

func (*UnloadUSRToken) tokenData() {}

// UnloadToken represents an "unload" command (dispatches to RT or USR).
type UnloadToken struct{ Comp string }

func (*UnloadToken) tokenData() {}

// WaitUSRToken represents a "waitusr" command.
type WaitUSRToken struct{ Comp string }

func (*WaitUSRToken) tokenData() {}

// ListToken represents a "list" command.
type ListToken struct {
	ObjType  HalObjType
	Patterns []string
}

func (*ListToken) tokenData() {}

// ShowToken represents a "show" command.
type ShowToken struct {
	ObjType  HalObjType
	Patterns []string
}

func (*ShowToken) tokenData() {}

// SaveToken represents a "save" command.
type SaveToken struct {
	SaveType SaveType
	File     string // empty string means stdout
}

func (*SaveToken) tokenData() {}

// StatusToken represents a "status" command.
type StatusToken struct{}

func (*StatusToken) tokenData() {}

// DebugToken represents a "debug" command.
type DebugToken struct {
	Level int // already parsed integer
}

func (*DebugToken) tokenData() {}

// PTypeToken represents a "ptype" command.
type PTypeToken struct{ Name string }

func (*PTypeToken) tokenData() {}

// STypeToken represents an "stype" command.
type STypeToken struct{ Name string }

func (*STypeToken) tokenData() {}

// EchoToken represents an "echo" command (no-op for backward compatibility).
type EchoToken struct{}

func (*EchoToken) tokenData() {}

// UnEchoToken represents an "unecho" command (no-op for backward compatibility).
type UnEchoToken struct{}

func (*UnEchoToken) tokenData() {}

// PrintToken represents a "print" command.
type PrintToken struct {
	Message string // free-form text, already joined at parse time
}

func (*PrintToken) tokenData() {}

// LoadToken represents the "load" command for Go plugin modules.
// The launcher resolves bare module names against EMC2_GOMOD_DIR and
// loads them via plugin.Open.  C RT modules use "loadrt" instead.
type LoadToken struct {
	Path  string   // module name or absolute path to .so
	Names []string // explicit instance names from [name1,name2,...]; nil for default
	Args  []string // remaining arguments after the path (and optional name list)
}

func (*LoadToken) tokenData() {}

// PathResolver resolves source file paths. The caller provides an implementation
// backed by whatever path resolution logic they have (e.g. the launcher's
// resolve.go with LIB: prefix and HALLIB_PATH support).
type PathResolver interface {
	Resolve(path string) (string, error)
}

// INILookup provides access to INI file values. The caller provides an
// implementation backed by whatever INI parser they already have.
type INILookup interface {
	Get(section, key string) (string, error)
	GetAll() map[string]map[string]string // needed for HalTemplateData
}

// ParseResult holds the execution buckets produced by MultiFileParser.
// LoadRT tokens are merged via TwopassCollector before execution.
// LoadUSR tokens with -W or -Wn flags are executed after all RT components load.
// Loads tokens are from the "load" command; they are exclusively for Go
// plugins and resolved against EMC2_GOMOD_DIR by the launcher.
// HALCmd tokens are everything else, executed in order after components start.
type ParseResult struct {
	LoadRT  []Token
	LoadUSR []Token
	Loads   []Token // "load" command tokens (*LoadToken) — Go plugins only
	HALCmd  []Token
}
