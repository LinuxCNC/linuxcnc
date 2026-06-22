// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package apiserver

import (
	"syscall"
	"testing"
	"unsafe"
)

// fakeCallbacks is a dummy value for tests — we just need a non-nil pointer.
var fakeCallbacks = unsafe.Pointer(&struct{}{})

func testMeta(name string, version int, rest bool) *APIMeta {
	return &APIMeta{
		Name:       name,
		Version:    version,
		RESTExport: rest,
		Prefix:     name,
	}
}

func TestRegisterAndGet(t *testing.T) {
	r := NewRegistry()

	err := r.Register("hal", 1, "hal", fakeCallbacks)
	if err != nil {
		t.Fatalf("Register: unexpected error: %v", err)
	}

	api := r.Get("hal")
	if api == nil {
		t.Fatal("Get: returned nil for registered instance")
	}
	if api.Instance != "hal" {
		t.Errorf("Instance = %q, want %q", api.Instance, "hal")
	}
	if api.APIName != "hal" {
		t.Errorf("APIName = %q, want %q", api.APIName, "hal")
	}
	if api.Callbacks != fakeCallbacks {
		t.Error("Callbacks pointer mismatch")
	}
}

func TestRegisterDuplicate(t *testing.T) {
	r := NewRegistry()

	if err := r.Register("hal", 1, "hal", fakeCallbacks); err != nil {
		t.Fatalf("first Register: %v", err)
	}

	err := r.Register("hal", 1, "hal", fakeCallbacks)
	if err != syscall.EEXIST {
		t.Errorf("duplicate Register: got %v, want EEXIST", err)
	}
}

func TestRegisterInvalidArgs(t *testing.T) {
	r := NewRegistry()

	if err := r.Register("", 1, "x", fakeCallbacks); err != syscall.EINVAL {
		t.Errorf("empty apiName: got %v, want EINVAL", err)
	}
	if err := r.Register("x", 1, "", fakeCallbacks); err != syscall.EINVAL {
		t.Errorf("empty instance: got %v, want EINVAL", err)
	}
}

func TestGetAPIVersionMatch(t *testing.T) {
	r := NewRegistry()
	r.Register("hal", 2, "hal", fakeCallbacks)

	cb, err := r.GetAPIUntracked("hal", "hal", 2)
	if err != nil {
		t.Fatalf("GetAPI: %v", err)
	}
	if cb != fakeCallbacks {
		t.Error("callbacks pointer mismatch")
	}
}

func TestGetAPIVersionMismatch(t *testing.T) {
	r := NewRegistry()
	r.Register("hal", 2, "hal", fakeCallbacks)

	_, err := r.GetAPIUntracked("hal", "hal", 1)
	if err != syscall.EINVAL {
		t.Errorf("version mismatch: got %v, want EINVAL", err)
	}
}

func TestGetAPINotFound(t *testing.T) {
	r := NewRegistry()

	_, err := r.GetAPIUntracked("hal", "nonexistent", 1)
	if err != syscall.ENOENT {
		t.Errorf("not found: got %v, want ENOENT", err)
	}
}

func TestGetNotFound(t *testing.T) {
	r := NewRegistry()

	if api := r.Get("nonexistent"); api != nil {
		t.Errorf("Get: got %v, want nil", api)
	}
}

func TestInstances(t *testing.T) {
	r := NewRegistry()
	r.Register("hal", 1, "hal", fakeCallbacks)
	r.Register("halcmd", 1, "halcmd", fakeCallbacks)

	names := r.Instances()
	if len(names) != 2 {
		t.Fatalf("Instances: got %d, want 2", len(names))
	}

	found := map[string]bool{}
	for _, n := range names {
		found[n] = true
	}
	if !found["hal"] || !found["halcmd"] {
		t.Errorf("Instances = %v, want hal + halcmd", names)
	}
}

func TestMultipleAPIs(t *testing.T) {
	r := NewRegistry()
	r.Register("hal", 1, "hal", fakeCallbacks)
	r.Register("halcmd", 1, "halcmd", fakeCallbacks)

	if r.Get("hal") == nil {
		t.Error("hal not found")
	}
	if r.Get("halcmd") == nil {
		t.Error("halcmd not found")
	}
}
