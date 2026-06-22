// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package haljson

import (
	"encoding/json"
	"fmt"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

// buildRESTMeta creates an APIMeta for REST access to haljson roots.
// Each root gets a GET endpoint (read all pins) and a POST endpoint (write pins).
// URL pattern: GET /api/v1/<instance>/<rootPath>
//
//	POST /api/v1/<instance>/<rootPath>
func buildRESTMeta(name string, roots []*jsonRoot) *apiserver.APIMeta {
	funcs := make([]apiserver.FuncMeta, 0, len(roots)*2)
	for _, root := range roots {
		r := root
		funcs = append(funcs, apiserver.FuncMeta{
			Name:   "get_" + r.path,
			Method: "GET",
			Path:   "/" + r.path,
			Dispatch: func(_ unsafe.Pointer, _ []byte) ([]byte, error) {
				return r.buildJSON(), nil
			},
		})
		funcs = append(funcs, apiserver.FuncMeta{
			Name:   "set_" + r.path,
			Method: "POST",
			Path:   "/" + r.path,
			Dispatch: func(_ unsafe.Pointer, req []byte) ([]byte, error) {
				if err := r.applyJSON(json.RawMessage(req)); err != nil {
					return nil, fmt.Errorf("haljson: %w", err)
				}
				return []byte(`{"ok":true}`), nil
			},
		})
	}
	return &apiserver.APIMeta{
		Name:       name,
		Version:    1,
		RESTExport: true,
		Prefix:     "",
		Funcs:      funcs,
	}
}
