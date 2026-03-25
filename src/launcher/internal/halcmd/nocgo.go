//go:build !cgo

package halcmd

import (
	"errors"

	hal "github.com/sittner/linuxcnc/src/launcher/pkg/hal"
)

// ErrNoCGO is returned by all stub functions when CGO is not available.
var ErrNoCGO = errors.New("halcmd: CGO is required but not available")

// --- CGO function stubs ---
// These provide stub implementations of all unexported halcmd functions defined
// in cgo.go (which is excluded from non-CGO builds). They allow the halcmd
// package to compile with CGO_ENABLED=0.

func halStartThreads() error               { return ErrNoCGO }
func halStopThreads() error                { return ErrNoCGO }
func halListComponents() ([]string, error) { return nil, ErrNoCGO }
func halUnloadAll(_ int) error             { return ErrNoCGO }
func halLockRTComponents() error           { return ErrNoCGO }

func halNewSig(_ string, _ hal.PinType) error { return ErrNoCGO }
func halDelSig(_ string) error                { return ErrNoCGO }
func halSetS(_, _ string) error               { return ErrNoCGO }
func halGetS(_ string) (string, error)        { return "", ErrNoCGO }
func halSType(_ string) (hal.PinType, error)  { return 0, ErrNoCGO }

func halSetP(_, _ string) error              { return ErrNoCGO }
func halGetP(_ string) (string, error)       { return "", ErrNoCGO }
func halPType(_ string) (hal.PinType, error) { return 0, ErrNoCGO }

func halNet(_ string, _ []string) error { return ErrNoCGO }
func halLinkPS(_, _ string) error       { return ErrNoCGO }
func halUnlinkP(_ string) error         { return ErrNoCGO }

func halAddF(_, _ string, _ int) error { return ErrNoCGO }
func halDelF(_, _ string) error        { return ErrNoCGO }

func halSetLock(_ int) error { return ErrNoCGO }
func halGetLock() int        { return 0 }

func halAlias(_, _, _ string) error { return ErrNoCGO }
func halUnAlias(_, _ string) error  { return ErrNoCGO }

func halLoadRT(_ string, _ []string) error { return ErrNoCGO }
func halUnloadRT(_ string) error           { return ErrNoCGO }
func halUnloadUSR(_ string) error          { return ErrNoCGO }
func halWaitUSR(_ string, _ int) error     { return ErrNoCGO }
func halNewInst(_, _, _ string) error      { return ErrNoCGO }
func halRtapiAppInit() error               { return ErrNoCGO }
func halRtapiAppCleanup()                  {}
func halRtapiInitializeApp()               {}

func halLoadUSR(_ int, _ string, _ int, _ string, _ []string) error {
	return ErrNoCGO
}

func halListPins(_ string) ([]string, error)       { return nil, ErrNoCGO }
func halListSigs(_ string) ([]string, error)       { return nil, ErrNoCGO }
func halListRetainSigs(_ string) ([]string, error) { return nil, ErrNoCGO }
func halListParams(_ string) ([]string, error)     { return nil, ErrNoCGO }
func halListFuncts(_ string) ([]string, error)     { return nil, ErrNoCGO }
func halListThreads(_ string) ([]string, error)    { return nil, ErrNoCGO }

func halShowComps(_ string) ([]CompInfo, error)     { return nil, ErrNoCGO }
func halShowPins(_ string) ([]PinInfo, error)       { return nil, ErrNoCGO }
func halShowParams(_ string) ([]ParamInfo, error)   { return nil, ErrNoCGO }
func halShowSigs(_ string) ([]SigInfo, error)       { return nil, ErrNoCGO }
func halShowFuncts(_ string) ([]FunctInfo, error)   { return nil, ErrNoCGO }
func halShowThreads(_ string) ([]ThreadInfo, error) { return nil, ErrNoCGO }

func halStatus() (*StatusInfo, error)    { return nil, ErrNoCGO }
func halSave(_ string) ([]string, error) { return nil, ErrNoCGO }
func halSetDebug(_ int) error            { return ErrNoCGO }
