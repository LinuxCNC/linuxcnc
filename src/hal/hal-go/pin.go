package hal

/*
#include "hal.h"
*/
import "C"
import (
	"encoding/binary"
	"fmt"
	"sync"
	"unsafe"
)

// portFrameHeaderSize is the number of bytes used for the length prefix in the
// HAL_PORT framing protocol for string pins (4-byte big-endian uint32).
const portFrameHeaderSize = 4

// Pin represents a HAL pin with type-safe access.
//
// Pins are the connection points between HAL components. They can be
// linked to signals, which allow components to exchange data. The generic
// type parameter T ensures type safety at compile time.
//
// Type T must be one of: bool, float64, int32, uint32, string (matching HAL types
// HAL_BIT, HAL_FLOAT, HAL_S32, HAL_U32, HAL_PORT).
type Pin[T PinValue] struct {
	// name is the fully-qualified pin name (e.g., "component.pinname").
	name string

	// direction is the pin direction (In, Out, or IO).
	direction Direction

	// ptr is a pointer to the HAL shared memory for this pin.
	// This is set by hal_pin_*_new() and used for Get/Set operations.
	ptr unsafe.Pointer

	// comp is the component that owns this pin.
	comp *Component

	// mu protects the pin value.
	mu sync.RWMutex
}

// NewPin creates a new pin with the specified type and direction.
//
// The name should be just the pin name (e.g., "input"), not the full name.
// The component name will be prepended automatically (e.g., "mycomp.input").
//
// Valid directions are In (component reads), Out (component writes), or
// IO (bidirectional).
//
// This calls the appropriate hal_pin_*_new() function via CGO based on the
// type parameter T:
//   - bool -> hal_pin_bit_new()
//   - float64 -> hal_pin_float_new()
//   - int32 -> hal_pin_s32_new()
//   - uint32 -> hal_pin_u32_new()
//   - string -> hal_pin_port_new()
//
// Type inference example:
//   pin, err := NewPin[float64](comp, "speed", hal.In)
//   pin, err := NewPin[bool](comp, "enable", hal.In)
//   pin, err := NewPin[int32](comp, "count", hal.Out)
func NewPin[T PinValue](c *Component, name string, dir Direction) (*Pin[T], error) {
	if c == nil {
		return nil, newError("NewPin", "component is nil", -22)
	}

	if name == "" {
		return nil, newError("NewPin", ErrInvalidName.Message, ErrInvalidName.Code)
	}

	if dir != In && dir != Out && dir != IO {
		return nil, newError("NewPin", "invalid direction", -22)
	}

	// Build fully-qualified pin name
	fullName := fmt.Sprintf("%s.%s", c.Name(), name)
	if len(fullName) > NameLen {
		return nil, newError("NewPin", ErrInvalidName.Message, ErrInvalidName.Code)
	}

	// Create the pin by calling the appropriate hal_pin_*_new() function
	// based on the type parameter T
	var ptr unsafe.Pointer
	var err error
	var zeroValue T
	switch any(zeroValue).(type) {
	case bool:
		ptr, err = halPinBitNew(fullName, dir, c.id)
	case float64:
		ptr, err = halPinFloatNew(fullName, dir, c.id)
	case int32:
		ptr, err = halPinS32New(fullName, dir, c.id)
	case uint32:
		ptr, err = halPinU32New(fullName, dir, c.id)
	case string:
		ptr, err = halPinPortNew(fullName, dir, c.id)
	default:
		return nil, newError("NewPin", "unsupported pin type", -22)
	}

	if err != nil {
		return nil, err
	}

	pin := &Pin[T]{
		name:      fullName,
		direction: dir,
		ptr:       ptr,
		comp:      c,
	}

	return pin, nil
}

// Get reads the current pin value.
//
// For input pins, this reads the value written by the connected signal.
// For output pins, this reads the value last written by Set().
//
// This dereferences the pointer to HAL shared memory.
func (p *Pin[T]) Get() T {
	p.mu.RLock()
	defer p.mu.RUnlock()

	// Read from HAL shared memory based on the type
	var zeroValue T
	switch any(zeroValue).(type) {
	case bool:
		// p.ptr is **hal_bit_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_bit_t)(p.ptr)
		val := bool(**ptrPtr)
		return any(val).(T)
	case float64:
		// p.ptr is **hal_float_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_float_t)(p.ptr)
		val := float64(**ptrPtr)
		return any(val).(T)
	case int32:
		// p.ptr is **hal_s32_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_s32_t)(p.ptr)
		val := int32(**ptrPtr)
		return any(val).(T)
	case uint32:
		// p.ptr is **hal_u32_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_u32_t)(p.ptr)
		val := uint32(**ptrPtr)
		return any(val).(T)
	case string:
		// String pins use HAL_PORT with 4-byte big-endian length-prefix framing.
		// Use peek (non-consuming) so repeated Get() calls return the same value.
		// p.ptr is **hal_port_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_port_t)(p.ptr)
		portPtr := *ptrPtr
		readable := halPortReadable(portPtr)
		if readable < portFrameHeaderSize {
			return any("").(T)
		}
		header := halPortPeek(portPtr, portFrameHeaderSize)
		if header == nil {
			return any("").(T)
		}
		length := binary.BigEndian.Uint32(header)
		// Guard against overflow: length near MaxUint32 would wrap when adding the header size.
		if length > ^uint32(0)-portFrameHeaderSize {
			return any("").(T)
		}
		total := uint(portFrameHeaderSize) + uint(length)
		// Bounds check: total must not exceed available data to prevent excessive allocation.
		if readable < total {
			return any("").(T)
		}
		frame := halPortPeek(portPtr, total)
		if frame == nil {
			return any("").(T)
		}
		return any(string(frame[portFrameHeaderSize:])).(T)
	default:
		// Should never happen due to PinValue constraint
		return *new(T)
	}
}

// Set writes a value to the pin.
//
// For output pins, this writes the value that will be read by connected
// components. For input pins, calling Set() has no effect (the value is
// overwritten by the connected signal).
//
// This writes to HAL shared memory.
func (p *Pin[T]) Set(value T) {
	p.mu.Lock()
	defer p.mu.Unlock()

	// Write to HAL shared memory based on the type
	var zeroValue T
	switch any(zeroValue).(type) {
	case bool:
		// p.ptr is **hal_bit_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_bit_t)(p.ptr)
		**ptrPtr = C.hal_bit_t(any(value).(bool))
	case float64:
		// p.ptr is **hal_float_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_float_t)(p.ptr)
		**ptrPtr = C.hal_float_t(any(value).(float64))
	case int32:
		// p.ptr is **hal_s32_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_s32_t)(p.ptr)
		**ptrPtr = C.hal_s32_t(any(value).(int32))
	case uint32:
		// p.ptr is **hal_u32_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_u32_t)(p.ptr)
		**ptrPtr = C.hal_u32_t(any(value).(uint32))
	case string:
		// String pins use HAL_PORT with 4-byte big-endian length-prefix framing.
		// Clear first for "latest value" semantics, then write the framed message.
		// p.ptr is **hal_port_t; dereference at access time so HAL's updated
		// pointer (set when pin is linked via net) is always followed.
		ptrPtr := (**C.hal_port_t)(p.ptr)
		portPtr := *ptrPtr
		halPortClear(portPtr)
		str := any(value).(string)
		strBytes := []byte(str)
		frame := make([]byte, portFrameHeaderSize+len(strBytes))
		binary.BigEndian.PutUint32(frame[:portFrameHeaderSize], uint32(len(strBytes)))
		copy(frame[portFrameHeaderSize:], strBytes)
		halPortWrite(portPtr, frame)
	}
}

// Name returns the fully-qualified pin name.
func (p *Pin[T]) Name() string {
	return p.name
}

// Direction returns the pin direction.
func (p *Pin[T]) Direction() Direction {
	return p.direction
}

// Type returns the HAL type of the pin.
func (p *Pin[T]) Type() PinType {
	// Use type assertion to determine the HAL type
	var t T
	switch any(t).(type) {
	case bool:
		return TypeBit
	case float64:
		return TypeFloat
	case int32:
		return TypeS32
	case uint32:
		return TypeU32
	case string:
		return TypePort
	default:
		return -1 // Should never happen due to PinValue constraint
	}
}

// String returns a string representation of the pin.
func (p *Pin[T]) String() string {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return fmt.Sprintf("Pin{name=%s, type=%s, dir=%s, value=%v}",
		p.name, p.Type(), p.direction, p.Get())
}
