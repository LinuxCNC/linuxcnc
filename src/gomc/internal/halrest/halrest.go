// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package halrest implements the server-side halcmd REST API handler.
// It registers with the apiserver and dispatches REST calls to the
// launcher's internal halcmd package (no liblinuxcnchal.so dependency).
package halrest

import (
	"encoding/json"
	"fmt"
	"time"

	halcmdapi "github.com/sittner/linuxcnc/src/gomc/generated/gmi/halcmd"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/internal/halcmd"
)

// LoadModuleFunc is the callback signature for dynamically loading a
// cmod plugin at runtime.  The launcher sets this via SetLoadModuleFunc.
type LoadModuleFunc func(module string, args []string) error

// UnloadModuleFunc is the callback signature for unloading a module
// by instance name.  The launcher sets this via SetUnloadModuleFunc.
type UnloadModuleFunc func(name string) error

var loadModuleHook LoadModuleFunc
var unloadModuleHook UnloadModuleFunc

// SetLoadModuleFunc sets the callback used by the "load" command to
// dynamically load a cmod .so into gomc-server.
func SetLoadModuleFunc(fn LoadModuleFunc) {
	loadModuleHook = fn
}

// SetUnloadModuleFunc sets the callback used by the "unload" command.
func SetUnloadModuleFunc(fn UnloadModuleFunc) {
	unloadModuleHook = fn
}

// Register registers the halcmd REST API with the given registry.
// This makes the /api/v1/halcmd/* endpoints available.
func Register(reg *apiserver.Registry) error {
	apiserver.RegisterMeta(halcmdapi.HalcmdMeta)
	return halcmdapi.RegisterHalcmdAPI(reg, "halcmd", &halcmdImpl{})
}

// ─── Watch support ───

// RegisterWatch registers the halcmd watch API with the given watch registry.
// This enables WebSocket clients to subscribe to live HAL pin/signal values.
// The interval parameter sets the default push rate; 0 uses the default (100ms).
// Configurable via [HAL]WATCH_INTERVAL in the INI file.
func RegisterWatch(wreg *apiserver.WatchRegistry, interval time.Duration) {
	if interval <= 0 {
		interval = 100 * time.Millisecond
	}
	wreg.Register(&apiserver.WatchAPI{
		APIName:  "halcmd",
		Instance: "halcmd",
		Watches: []apiserver.WatchFuncMeta{
			{
				Name:        "watch_items",
				DefaultRate: interval,
				Factory:     watchItemsFactory,
			},
		},
	})
}

// watchItemsArgs holds the subscription arguments sent by the client.
type watchItemsArgs struct {
	Names []string `json:"names"`
}

// watchItemsMeta is sent once on the first poll to provide item metadata.
type watchItemsMeta struct {
	Meta   []watchItemMetaEntry `json:"meta"`
	Values map[string]string    `json:"values"`
}

type watchItemMetaEntry struct {
	Name   string `json:"name"`
	Type   string `json:"type"`
	Dir    string `json:"dir,omitempty"`
	Kind   string `json:"kind"`
	Owner  string `json:"owner,omitempty"`
	Linked bool   `json:"linked"`
	Signal string `json:"signal,omitempty"`
}

// watchItemsFactory creates a per-connection stateful watch function.
// It resolves HAL item names to direct shmem pointers and polls only those,
// doing raw memory comparison to avoid serialization of unchanged values.
func watchItemsFactory(args json.RawMessage) (apiserver.WatchFunc, error) {
	var filter watchItemsArgs
	if len(args) > 0 {
		_ = json.Unmarshal(args, &filter)
	}
	if len(filter.Names) == 0 {
		return nil, fmt.Errorf("watch_items requires 'names' argument")
	}

	ws, err := halcmd.NewWatchSet(filter.Names)
	if err != nil {
		return nil, err
	}

	// First call flag — send metadata + initial values
	first := true

	return func() (json.RawMessage, error) {
		changed := ws.Poll()

		if first || ws.MetaChanged() {
			first = false
			// Build metadata response with initial values
			metas := ws.Meta()
			resp := watchItemsMeta{
				Meta:   make([]watchItemMetaEntry, len(metas)),
				Values: make(map[string]string, len(changed)),
			}
			for i, m := range metas {
				resp.Meta[i] = watchItemMetaEntry{
					Name:   m.Name,
					Type:   m.Type,
					Dir:    m.Dir,
					Kind:   m.Kind,
					Owner:  m.Owner,
					Linked: m.Linked,
					Signal: m.Signal,
				}
			}
			for _, v := range changed {
				resp.Values[v.Name] = v.Value
			}
			return json.Marshal(resp)
		}

		// Subsequent calls: only send changed name→value pairs
		if changed == nil {
			return nil, nil // nothing changed — pushLoop skips nil
		}
		out := make(map[string]string, len(changed))
		for _, v := range changed {
			out[v.Name] = v.Value
		}
		return json.Marshal(out)
	}, nil
}
