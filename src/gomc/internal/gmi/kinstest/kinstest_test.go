// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package kinstest

import (
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

func TestTrivkinsLoadAndRegister(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	mod, err := loadTrivkins()
	if err != nil {
		t.Fatalf("loadTrivkins: %v", err)
	}
	defer unloadTrivkins(mod)

	// The .so's New() should have called kins_api_register(name, ...) with name="trivkins".
	cbs := getKinsCallbacks()
	if cbs == nil {
		t.Fatal("kins_api_get returned nil — registration failed")
	}
}

func TestTrivkinsForward(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	mod, err := loadTrivkins()
	if err != nil {
		t.Fatalf("loadTrivkins: %v", err)
	}
	defer unloadTrivkins(mod)

	cbs := getKinsCallbacks()
	if cbs == nil {
		t.Fatal("kins_api_get returned nil")
	}

	// Forward: joints → world (identity: joint[i] = axis[i])
	var joints [16]float64
	joints[0] = 10.0 // X
	joints[1] = 20.0 // Y
	joints[2] = 30.0 // Z

	world, rc := callForward(cbs, joints)
	if rc != 0 {
		t.Fatalf("forward returned %d", rc)
	}
	if float64(world.x) != 10.0 || float64(world.y) != 20.0 || float64(world.z) != 30.0 {
		t.Errorf("forward: got (%.1f, %.1f, %.1f), want (10.0, 20.0, 30.0)",
			float64(world.x), float64(world.y), float64(world.z))
	}
}

func TestTrivkinsInverse(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	mod, err := loadTrivkins()
	if err != nil {
		t.Fatalf("loadTrivkins: %v", err)
	}
	defer unloadTrivkins(mod)

	cbs := getKinsCallbacks()
	if cbs == nil {
		t.Fatal("kins_api_get returned nil")
	}

	// Inverse: world → joints (identity)
	world := callForward // reuse forward to build a world pose
	_ = world

	var joints [16]float64
	joints[0] = 1.0
	joints[1] = 2.0
	joints[2] = 3.0
	pose, _ := callForward(cbs, joints)

	result, rc := callInverse(cbs, pose)
	if rc != 0 {
		t.Fatalf("inverse returned %d", rc)
	}
	if result[0] != 1.0 || result[1] != 2.0 || result[2] != 3.0 {
		t.Errorf("inverse: got (%.1f, %.1f, %.1f), want (1.0, 2.0, 3.0)",
			result[0], result[1], result[2])
	}
}

func TestTrivkinsType(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	mod, err := loadTrivkins()
	if err != nil {
		t.Fatalf("loadTrivkins: %v", err)
	}
	defer unloadTrivkins(mod)

	cbs := getKinsCallbacks()
	if cbs == nil {
		t.Fatal("kins_api_get returned nil")
	}

	ktype, rc := callType(cbs)
	if rc != 0 {
		t.Fatalf("type returned %d", rc)
	}
	// Default kinstype='1' → KINS_IDENTITY = 1
	if ktype != 1 {
		t.Errorf("type = %d, want 1 (IDENTITY)", ktype)
	}
}

func TestTrivkinsSwitchable(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	mod, err := loadTrivkins()
	if err != nil {
		t.Fatalf("loadTrivkins: %v", err)
	}
	defer unloadTrivkins(mod)

	cbs := getKinsCallbacks()
	if cbs == nil {
		t.Fatal("kins_api_get returned nil")
	}

	sw := callSwitchable(cbs)
	if sw != 0 {
		t.Errorf("switchable = %d, want 0 (not switchable)", sw)
	}
}
