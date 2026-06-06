// Package calibreg provides a global registry of INI→HAL pin mappings
// discovered during HAL file loading. When a setp command uses a [SECTION]KEY
// INI reference, the halparse package records the mapping here. The emccalib
// gomod reads this registry to know which pins are tunable.
package calibreg

import "sync"

// IniPinMapping records one discovered INI→pin relationship.
type IniPinMapping struct {
	Pin      string  // HAL pin/param name, e.g. "pid.0.Pgain"
	Section  string  // INI section, e.g. "JOINT_0"
	Key      string  // INI key, e.g. "P"
	IniValue float64 // resolved numeric value at load time
}

var (
	mu       sync.Mutex
	mappings []IniPinMapping
)

// Record adds a mapping to the registry. Called by halparse during setp
// processing when an INI reference is resolved.
func Record(m IniPinMapping) {
	mu.Lock()
	mappings = append(mappings, m)
	mu.Unlock()
}

// GetAll returns a copy of all recorded mappings.
func GetAll() []IniPinMapping {
	mu.Lock()
	defer mu.Unlock()
	result := make([]IniPinMapping, len(mappings))
	copy(result, mappings)
	return result
}

// Reset clears all recorded mappings. Used in tests.
func Reset() {
	mu.Lock()
	mappings = nil
	mu.Unlock()
}
