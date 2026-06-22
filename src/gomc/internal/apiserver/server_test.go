// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package apiserver

import (
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"syscall"
	"testing"
	"unsafe"
)

// --- Path matching unit tests ---

func TestMatchPathLiteral(t *testing.T) {
	tests := []struct {
		pattern, path string
		want          bool
	}{
		{"/pins", "/pins", true},
		{"/pins", "/pin", false},
		{"/pins", "/pins/extra", false},
		{"/a/b/c", "/a/b/c", true},
		{"/a/b/c", "/a/b", false},
	}
	for _, tt := range tests {
		if got := matchPath(tt.pattern, tt.path); got != tt.want {
			t.Errorf("matchPath(%q, %q) = %v, want %v", tt.pattern, tt.path, got, tt.want)
		}
	}
}

func TestMatchPathParam(t *testing.T) {
	tests := []struct {
		pattern, path string
		want          bool
	}{
		{"/pin/{name}", "/pin/axis.0.pos-cmd", true},
		{"/pin/{name}", "/pin/", false}, // empty segment does not match {name}
		{"/pin/{name}", "/pin", false},
		{"/pin/{name}", "/pin/a/b", false},
		{"/signal/{name}/value", "/signal/foo/value", true},
		{"/signal/{name}/value", "/signal/foo/other", false},
	}
	for _, tt := range tests {
		if got := matchPath(tt.pattern, tt.path); got != tt.want {
			t.Errorf("matchPath(%q, %q) = %v, want %v", tt.pattern, tt.path, got, tt.want)
		}
	}
}

func TestMatchPathWildcardRest(t *testing.T) {
	tests := []struct {
		pattern, path string
		want          bool
	}{
		{"/exec/{cmd...}", "/exec/show/pin", true},
		{"/exec/{cmd...}", "/exec/x", true},
		{"/exec/{cmd...}", "/exec/", true},
	}
	for _, tt := range tests {
		if got := matchPath(tt.pattern, tt.path); got != tt.want {
			t.Errorf("matchPath(%q, %q) = %v, want %v", tt.pattern, tt.path, got, tt.want)
		}
	}
}

func TestExtractPathParams(t *testing.T) {
	params := extractPathParams("/pin/{name}", "/pin/axis.0.pos-cmd")
	if params["name"] != "axis.0.pos-cmd" {
		t.Errorf("name = %q, want %q", params["name"], "axis.0.pos-cmd")
	}

	params = extractPathParams("/comp/{comp}/pin/{pin}", "/comp/axis/pin/pos-cmd")
	if params["comp"] != "axis" {
		t.Errorf("comp = %q, want %q", params["comp"], "axis")
	}
	if params["pin"] != "pos-cmd" {
		t.Errorf("pin = %q, want %q", params["pin"], "pos-cmd")
	}

	params = extractPathParams("/exec/{cmd...}", "/exec/show/pin")
	if params["cmd"] != "show/pin" {
		t.Errorf("cmd = %q, want %q", params["cmd"], "show/pin")
	}
}

func TestMatchFunc(t *testing.T) {
	meta := &APIMeta{
		Funcs: []FuncMeta{
			{Name: "list_pins", Method: "GET", Path: "/pins"},
			{Name: "pin_read", Method: "GET", Path: "/pin/{name}"},
			{Name: "signal_create", Method: "POST", Path: "/signal"},
			{Name: "internal_only", Method: "", Path: ""},
		},
	}

	tests := []struct {
		method, path string
		want         int
	}{
		{"GET", "/pins", 0},
		{"GET", "/pin/axis.0", 1},
		{"POST", "/signal", 2},
		{"GET", "/signal", -1},  // wrong method
		{"PUT", "/pins", -1},    // wrong method
		{"GET", "/unknown", -1}, // no match
	}
	for _, tt := range tests {
		got := matchFunc(meta, tt.method, tt.path)
		if got != tt.want {
			t.Errorf("matchFunc(%q, %q) = %d, want %d", tt.method, tt.path, got, tt.want)
		}
	}
}

// --- HTTP integration tests ---

// mockDispatch creates a DispatchFunc that echoes back a JSON response
// containing the function name and the received request body.
func mockDispatch(name string) DispatchFunc {
	return func(callbacks unsafe.Pointer, req []byte) ([]byte, error) {
		resp := map[string]interface{}{
			"func": name,
		}
		if len(req) > 0 {
			var params map[string]interface{}
			if err := json.Unmarshal(req, &params); err == nil {
				resp["params"] = params
			}
		}
		return json.Marshal(resp)
	}
}

func errorDispatch(err error) DispatchFunc {
	return func(callbacks unsafe.Pointer, req []byte) ([]byte, error) {
		return nil, err
	}
}

func setupTestServer(t *testing.T) (*httptest.Server, *Registry) {
	t.Helper()

	reg := NewRegistry()
	meta := &APIMeta{
		Name:       "hal",
		Version:    1,
		RESTExport: true,
		Prefix:     "hal",
		Funcs: []FuncMeta{
			{Name: "list_pins", Method: "GET", Path: "/pins", Dispatch: mockDispatch("list_pins")},
			{Name: "pin_read", Method: "GET", Path: "/pin/{name}", Dispatch: mockDispatch("pin_read")},
			{Name: "signal_create", Method: "POST", Path: "/signal", Dispatch: mockDispatch("signal_create")},
			{Name: "fail_func", Method: "GET", Path: "/fail", Dispatch: errorDispatch(syscall.ENOENT)},
		},
	}
	RegisterMeta(meta)
	reg.Register("hal", 1, "hal0", fakeCallbacks)

	srv := NewServer(reg, "localhost:0")
	ts := httptest.NewServer(srv.Handler())
	t.Cleanup(ts.Close)
	return ts, reg
}

func TestHTTPGetSimple(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/hal0/pins")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		t.Errorf("status = %d, want 200", resp.StatusCode)
	}

	var result map[string]interface{}
	json.NewDecoder(resp.Body).Decode(&result)
	if result["func"] != "list_pins" {
		t.Errorf("func = %v, want list_pins", result["func"])
	}
}

func TestHTTPGetWithPathParam(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/hal0/pin/axis.0.pos-cmd")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		t.Errorf("status = %d, want 200", resp.StatusCode)
	}

	var result map[string]interface{}
	json.NewDecoder(resp.Body).Decode(&result)
	if result["func"] != "pin_read" {
		t.Errorf("func = %v, want pin_read", result["func"])
	}
	params, _ := result["params"].(map[string]interface{})
	if params["name"] != "axis.0.pos-cmd" {
		t.Errorf("params.name = %v, want axis.0.pos-cmd", params["name"])
	}
}

func TestHTTPGetWithQueryParam(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/hal0/pins?pattern=axis.*")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		t.Errorf("status = %d, want 200", resp.StatusCode)
	}

	var result map[string]interface{}
	json.NewDecoder(resp.Body).Decode(&result)
	params, _ := result["params"].(map[string]interface{})
	if params["pattern"] != "axis.*" {
		t.Errorf("params.pattern = %v, want axis.*", params["pattern"])
	}
}

func TestHTTPPost(t *testing.T) {
	ts, _ := setupTestServer(t)

	body := `{"name":"test-signal","type":"float"}`
	resp, err := http.Post(ts.URL+"/api/v1/hal0/signal", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		t.Errorf("status = %d, want 200", resp.StatusCode)
	}

	var result map[string]interface{}
	json.NewDecoder(resp.Body).Decode(&result)
	if result["func"] != "signal_create" {
		t.Errorf("func = %v, want signal_create", result["func"])
	}
	params, _ := result["params"].(map[string]interface{})
	if params["name"] != "test-signal" {
		t.Errorf("params.name = %v, want test-signal", params["name"])
	}
}

func TestHTTPNotFoundInstance(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/unknown/pins")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 404 {
		t.Errorf("status = %d, want 404", resp.StatusCode)
	}
}

func TestHTTPNotFoundPath(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/hal0/unknown/path")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 404 {
		t.Errorf("status = %d, want 404", resp.StatusCode)
	}
}

func TestHTTPMethodNotAllowed(t *testing.T) {
	ts, _ := setupTestServer(t)

	req, _ := http.NewRequest("DELETE", ts.URL+"/api/v1/hal0/pins", nil)
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 404 {
		t.Errorf("status = %d, want 404", resp.StatusCode)
	}
}

func TestHTTPDispatchError(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/hal0/fail")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 404 {
		t.Errorf("status = %d, want 404 (ENOENT)", resp.StatusCode)
	}

	var result apiError
	json.NewDecoder(resp.Body).Decode(&result)
	if result.Code != 404 {
		t.Errorf("error code = %d, want 404", result.Code)
	}
}

func TestHTTPNonRESTExported(t *testing.T) {
	reg := NewRegistry()
	meta := &APIMeta{
		Name:       "internal",
		Version:    1,
		RESTExport: false,
		Funcs: []FuncMeta{
			{Name: "do_thing", Method: "GET", Path: "/thing", Dispatch: mockDispatch("do_thing")},
		},
	}
	RegisterMeta(meta)
	reg.Register("internal", 1, "internal0", fakeCallbacks)

	srv := NewServer(reg, "localhost:0")
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()

	resp, err := http.Get(ts.URL + "/api/v1/internal0/thing")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 404 {
		t.Errorf("status = %d, want 404 (not REST-exported)", resp.StatusCode)
	}
}

func TestHTTPMissingInstance(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 404 {
		body, _ := io.ReadAll(resp.Body)
		t.Errorf("status = %d, want 404; body: %s", resp.StatusCode, body)
	}
}

func TestHTTPContentType(t *testing.T) {
	ts, _ := setupTestServer(t)

	resp, err := http.Get(ts.URL + "/api/v1/hal0/pins")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()

	ct := resp.Header.Get("Content-Type")
	if !strings.HasPrefix(ct, "application/json") {
		t.Errorf("Content-Type = %q, want application/json", ct)
	}
}
