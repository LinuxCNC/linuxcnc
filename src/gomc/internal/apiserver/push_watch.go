package apiserver

import (
	"encoding/json"
	"sync"
	"unsafe"
)

// PushConverter converts raw C struct data (via unsafe.Pointer) to JSON.
// The pointer is valid only for the duration of the call — the converter
// must not retain it. Registered by generated packages in init().
type PushConverter func(data unsafe.Pointer, size int) (json.RawMessage, error)

// PushWatch stores the latest JSON produced by a push from C code.
// It implements the pattern: C pushes raw struct → converter runs → JSON stored.
// A WatchFunc reads the stored JSON (no cgo call on read path).
type PushWatch struct {
	mu        sync.Mutex
	converter PushConverter
	data      json.RawMessage
}

// NewPushWatch creates a PushWatch backed by the given converter.
func NewPushWatch(converter PushConverter) *PushWatch {
	return &PushWatch{converter: converter}
}

// Push receives raw C struct data, converts it to JSON, and stores the result.
// Called from the C push_watch callback (synchronous — the C pointer is valid
// for the duration of this call).
func (pw *PushWatch) Push(data unsafe.Pointer, size int) error {
	j, err := pw.converter(data, size)
	if err != nil {
		return err
	}
	pw.mu.Lock()
	pw.data = j
	pw.mu.Unlock()
	return nil
}

// WatchFunc returns the latest stored JSON. Safe to call from pushLoop.
func (pw *PushWatch) WatchFunc() (json.RawMessage, error) {
	pw.mu.Lock()
	d := pw.data
	pw.mu.Unlock()
	return d, nil
}

// --- Push Converter Registry ---
//
// Generated packages register their converters in init().
// When a cmod calls push_watch(), the Go export looks up the converter
// and creates/retrieves the PushWatch for that API/instance/func triple.

type pushKey struct {
	api, instance, funcName string
}

var (
	pushConvertersMu sync.RWMutex
	pushConverters   = map[string]PushConverter{} // key: "apiName"

	pushWatchesMu sync.RWMutex
	pushWatches   = map[pushKey]*PushWatch{}
)

// RegisterPushConverter registers a converter for the given API name.
// Called from generated packages' init() functions.
func RegisterPushConverter(apiName string, converter PushConverter) {
	pushConvertersMu.Lock()
	pushConverters[apiName] = converter
	pushConvertersMu.Unlock()
}

// GetPushConverter looks up a registered converter.
func GetPushConverter(apiName string) PushConverter {
	pushConvertersMu.RLock()
	c := pushConverters[apiName]
	pushConvertersMu.RUnlock()
	return c
}

// GetOrCreatePushWatch returns the PushWatch for the given triple,
// creating it if necessary using the registered converter.
func GetOrCreatePushWatch(apiName, instance, funcName string) *PushWatch {
	key := pushKey{apiName, instance, funcName}

	pushWatchesMu.RLock()
	pw := pushWatches[key]
	pushWatchesMu.RUnlock()
	if pw != nil {
		return pw
	}

	// Slow path: create
	pushWatchesMu.Lock()
	defer pushWatchesMu.Unlock()

	// Double-check after acquiring write lock
	if pw = pushWatches[key]; pw != nil {
		return pw
	}

	conv := pushConverters[apiName]
	if conv == nil {
		return nil
	}

	pw = NewPushWatch(conv)
	pushWatches[key] = pw
	return pw
}

// GetPushWatch returns an existing PushWatch or nil.
func GetPushWatch(apiName, instance, funcName string) *PushWatch {
	key := pushKey{apiName, instance, funcName}
	pushWatchesMu.RLock()
	pw := pushWatches[key]
	pushWatchesMu.RUnlock()
	return pw
}
