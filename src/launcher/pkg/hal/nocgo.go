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
// These provide stub implementations of the core unexported hal* functions
// defined in cgo.go (which is excluded from non-CGO builds). They allow the
// package to compile with CGO_ENABLED=0 for pure-Go tests.

func halInit(_ string) (int, error) { return 0, errNoCGO }
func halReady(_ int) error          { return errNoCGO }
func halExit(_ int) error           { return errNoCGO }
