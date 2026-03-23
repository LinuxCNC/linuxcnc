//go:build !cgo

package hal

import "errors"

// errNoCGO is returned by all stub functions when CGO is not available.
var errNoCGO = errors.New("hal: CGO is required but not available")

// Pin stub for non-CGO builds. Provides an in-memory pin that satisfies
// the same interface as the CGO-backed Pin but does not interact with HAL.
type Pin[T PinValue] struct {
	value T
	name  string
	dir   Direction
	typ   PinType
}

// NewPin creates a stub pin for non-CGO builds.
func NewPin[T PinValue](c *Component, name string, dir Direction) (*Pin[T], error) {
	var zero T
	var pt PinType
	switch any(zero).(type) {
	case bool:
		pt = TypeBit
	case float64:
		pt = TypeFloat
	case int32:
		pt = TypeS32
	case uint32:
		pt = TypeU32
	case string:
		pt = TypePort
	}
	return &Pin[T]{value: zero, name: name, dir: dir, typ: pt}, nil
}

// Set sets the pin value.
func (p *Pin[T]) Set(v T) { p.value = v }

// Get returns the pin value.
func (p *Pin[T]) Get() T { return p.value }

// Type returns the pin type.
func (p *Pin[T]) Type() PinType { return p.typ }

// Name returns the pin name.
func (p *Pin[T]) Name() string { return p.name }

// Direction returns the pin direction.
func (p *Pin[T]) Direction() Direction { return p.dir }

// String returns a string representation of the pin.
func (p *Pin[T]) String() string { return p.name }

// --- CGO function stubs ---
// These provide stub implementations of all unexported hal* functions that are
// defined in cgo.go (which is excluded from non-CGO builds). They allow the
// package to compile with CGO_ENABLED=0 for pure-Go tests.

func halInit(_ string) (int, error)           { return 0, errNoCGO }
func halReady(_ int) error                    { return errNoCGO }
func halExit(_ int) error                     { return errNoCGO }
func halStartThreads() error                  { return errNoCGO }
func halStopThreads() error                   { return errNoCGO }
func halListComponents() ([]string, error)    { return nil, errNoCGO }
func halUnloadAll(_ int) error                { return errNoCGO }
func halNewSig(_ string, _ PinType) error     { return errNoCGO }
func halDelSig(_ string) error                { return errNoCGO }
func halSetS(_, _ string) error               { return errNoCGO }
func halGetS(_ string) (string, error)        { return "", errNoCGO }
func halSType(_ string) (PinType, error)      { return 0, errNoCGO }
func halSetP(_, _ string) error               { return errNoCGO }
func halGetP(_ string) (string, error)        { return "", errNoCGO }
func halPType(_ string) (PinType, error)      { return 0, errNoCGO }
func halNet(_ string, _ []string) error       { return errNoCGO }
func halLinkPS(_, _ string) error             { return errNoCGO }
func halUnlinkP(_ string) error               { return errNoCGO }
func halAddF(_, _ string, _ int) error        { return errNoCGO }
func halDelF(_, _ string) error               { return errNoCGO }
func halSetLock(_ int) error                  { return errNoCGO }
func halAlias(_, _, _ string) error            { return errNoCGO }
func halUnAlias(_, _ string) error             { return errNoCGO }
func halLoadRT(_ string, _ []string) error    { return errNoCGO }
func halUnloadRT(_ string) error              { return errNoCGO }
func halUnloadUSR(_ string) error             { return errNoCGO }
func halWaitUSR(_ string, _ int) error        { return errNoCGO }

func halLoadUSR(_ int, _ string, _ int, _ string, _ []string) error {
	return errNoCGO
}

func halListPins(_ string) ([]string, error)         { return nil, errNoCGO }
func halListSigs(_ string) ([]string, error)         { return nil, errNoCGO }
func halListRetainSigs(_ string) ([]string, error)   { return nil, errNoCGO }
func halListParams(_ string) ([]string, error)       { return nil, errNoCGO }
func halListFuncts(_ string) ([]string, error)       { return nil, errNoCGO }
func halListThreads(_ string) ([]string, error)      { return nil, errNoCGO }

func halShowComps(_ string) ([]CompInfo, error)    { return nil, errNoCGO }
func halShowPins(_ string) ([]PinInfo, error)      { return nil, errNoCGO }
func halShowParams(_ string) ([]ParamInfo, error)  { return nil, errNoCGO }
func halShowSigs(_ string) ([]SigInfo, error)      { return nil, errNoCGO }
func halShowFuncts(_ string) ([]FunctInfo, error)  { return nil, errNoCGO }
func halShowThreads(_ string) ([]ThreadInfo, error) { return nil, errNoCGO }

func halStatus() (*StatusInfo, error)    { return nil, errNoCGO }
func halSave(_ string) ([]string, error) { return nil, errNoCGO }
func halSetDebug(_ int) error            { return errNoCGO }
