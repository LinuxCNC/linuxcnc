package hal

/*
#include "hal.h"
*/
import "C"
import (
	"fmt"
	"sync"
	"unsafe"
)

// Pin represents a HAL pin with type-safe access.
//
// Pins are the connection points between HAL components. They can be
// linked to signals, which allow components to exchange data. The generic
// type parameter T ensures type safety at compile time.
//
// Type T must be one of: bool, float64, int32, uint32 (matching HAL types
// HAL_BIT, HAL_FLOAT, HAL_S32, HAL_U32).
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
//
// Type inference example:
//   pin, err := NewPin[float64](comp, "speed", hal.In)
//   pin, err := NewPin[bool](comp, "enable", hal.In)
//   pin, err := NewPin[int32](comp, "count", hal.Out)
func NewPin[T PinValue](c *Component, name string, dir Direction) (*Pin[T], error) {
	if c == nil {
		return nil, newError("NewPin", "component is nil", -22)
	}

	if name == "" || len(name) > 47 {
		return nil, newError("NewPin", ErrInvalidName.Message, ErrInvalidName.Code)
	}

	if dir != In && dir != Out && dir != IO {
		return nil, newError("NewPin", "invalid direction", -22)
	}

	// Build fully-qualified pin name
	fullName := fmt.Sprintf("%s.%s", c.Name(), name)

	// Create the pin by calling the appropriate hal_pin_*_new() function
	// based on the type parameter T
	var ptr unsafe.Pointer
	var err error
	var zeroValue T
	switch any(zeroValue).(type) {
	case bool:
		cPtr, e := halPinBitNew(fullName, dir, c.id)
		ptr = unsafe.Pointer(cPtr)
		err = e
	case float64:
		cPtr, e := halPinFloatNew(fullName, dir, c.id)
		ptr = unsafe.Pointer(cPtr)
		err = e
	case int32:
		cPtr, e := halPinS32New(fullName, dir, c.id)
		ptr = unsafe.Pointer(cPtr)
		err = e
	case uint32:
		cPtr, e := halPinU32New(fullName, dir, c.id)
		ptr = unsafe.Pointer(cPtr)
		err = e
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
		// HAL bool is stored as hal_bit_t (C bool)
		cPtr := (*C.hal_bit_t)(p.ptr)
		val := bool(*cPtr)
		return any(val).(T)
	case float64:
		// HAL float is stored as hal_float_t (C double)
		cPtr := (*C.hal_float_t)(p.ptr)
		val := float64(*cPtr)
		return any(val).(T)
	case int32:
		// HAL S32 is stored as hal_s32_t (C int32_t)
		cPtr := (*C.hal_s32_t)(p.ptr)
		val := int32(*cPtr)
		return any(val).(T)
	case uint32:
		// HAL U32 is stored as hal_u32_t (C uint32_t)
		cPtr := (*C.hal_u32_t)(p.ptr)
		val := uint32(*cPtr)
		return any(val).(T)
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
		// HAL bool is stored as hal_bit_t (C bool)
		cPtr := (*C.hal_bit_t)(p.ptr)
		*cPtr = C.hal_bit_t(any(value).(bool))
	case float64:
		// HAL float is stored as hal_float_t (C double)
		cPtr := (*C.hal_float_t)(p.ptr)
		*cPtr = C.hal_float_t(any(value).(float64))
	case int32:
		// HAL S32 is stored as hal_s32_t (C int32_t)
		cPtr := (*C.hal_s32_t)(p.ptr)
		*cPtr = C.hal_s32_t(any(value).(int32))
	case uint32:
		// HAL U32 is stored as hal_u32_t (C uint32_t)
		cPtr := (*C.hal_u32_t)(p.ptr)
		*cPtr = C.hal_u32_t(any(value).(uint32))
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
