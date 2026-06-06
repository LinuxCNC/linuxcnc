// Package apiserver implements the dynamic API registry and HTTP server
// for LinuxCNC's inter-module communication system.
package apiserver

import "unsafe"

// DispatchFunc is the uniform signature for all generated dispatch wrappers.
// Both cmod and gomod generate functions with this signature.
// The HTTP server calls these — it never touches callbacks directly.
type DispatchFunc func(callbacks unsafe.Pointer, req []byte) ([]byte, error)

// FuncMeta holds static metadata + dispatch for one API function (generated).
// Routing info and dispatch wrapper live together — no parallel arrays.
type FuncMeta struct {
	Name     string // "pin_read"
	Method   string // "GET", "POST", etc. (empty if not REST-exported)
	Path     string // "/pin/{name}" (empty if not REST-exported)
	RTSafe   bool
	Dispatch DispatchFunc // generated wrapper (nil if not REST-exported)
}

// APIMeta holds static metadata for an entire API (generated, read-only).
type APIMeta struct {
	Name       string // "hal"
	Version    int
	RESTExport bool
	Prefix     string     // REST path prefix
	Funcs      []FuncMeta // routing + dispatch in one place
}

// RegisteredAPI is one registered API instance in the registry.
type RegisteredAPI struct {
	APIName   string         // "tp" — API name from registration
	Version   int            // API version from registration
	Meta      *APIMeta       // optional — REST routing/dispatch (nil for pure C-to-C)
	Instance  string         // "default" — unique instance name within an API
	Callbacks unsafe.Pointer // opaque — *tp_callbacks_t (cmod) or Go interface
}

// StreamConn represents a single WebSocket connection for stream_server.
// The generated bridge calls WriteBinary to send data frames to the client.
type StreamConn interface {
	// WriteBinary sends a binary WebSocket frame. Returns error on disconnect.
	WriteBinary(data []byte) error
	// ReadBinary blocks until a binary frame is received. Returns data or error on disconnect.
	ReadBinary() ([]byte, error)
	// Done returns a channel that is closed when the connection should stop.
	Done() <-chan struct{}
}

// StreamServer is the interface that generated stream_server types implement.
type StreamServer interface {
	// ServeConn handles one WebSocket connection. Blocks until done.
	ServeConn(conn StreamConn)
}
