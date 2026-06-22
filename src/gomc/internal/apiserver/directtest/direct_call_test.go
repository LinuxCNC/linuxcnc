// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package directtest

import (
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

func TestDirectKinsRegisterAndCall(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	// Build C callbacks (simulating what a cmod would do).
	cbs := MakeCallbacks()

	err := reg.Register("kins", 1, "kinematics", CallbacksPtr(&cbs))
	if err != nil {
		t.Fatalf("Register: %v", err)
	}

	// Lookup (simulating what the motion controller would do).
	ptr, err := reg.GetAPIUntracked("kins", "kinematics", 1)
	if err != nil {
		t.Fatalf("GetAPI: %v", err)
	}
	kins := (*Callbacks)(ptr)

	// Forward: joints → world
	world, rc := Forward(kins, [3]float64{10.0, 20.0, 30.0})
	if rc != 0 {
		t.Fatalf("forward returned %d", rc)
	}
	x, y, z := PoseXYZ(world)
	if x != 10.0 || y != 20.0 || z != 30.0 {
		t.Errorf("forward: got (%.1f, %.1f, %.1f), want (10.0, 20.0, 30.0)", x, y, z)
	}

	// Inverse: world → joints
	world2 := Pose{}
	world2.x = 1.5
	world2.y = 2.5
	world2.z = 3.5
	joints, rc := Inverse(kins, world2)
	if rc != 0 {
		t.Fatalf("inverse returned %d", rc)
	}
	if joints != [3]float64{1.5, 2.5, 3.5} {
		t.Errorf("inverse: got %v, want [1.5, 2.5, 3.5]", joints)
	}

	// Type query
	kinsType, rc := GetType(kins)
	if rc != 0 {
		t.Fatalf("type returned %d", rc)
	}
	if kinsType != 1 {
		t.Errorf("type = %d, want 1 (IDENTITY)", kinsType)
	}
}

func TestDirectKinsLookupNotFound(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	_, err := reg.GetAPIUntracked("kins", "kinematics", 1)
	if err == nil {
		t.Fatal("expected error for unregistered lookup")
	}
}

func TestDirectKinsVersionMismatch(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	cbs := MakeCallbacks()
	reg.Register("kins", 1, "kinematics", CallbacksPtr(&cbs))

	_, err := reg.GetAPIUntracked("kins", "kinematics", 2)
	if err == nil {
		t.Fatal("expected error for version mismatch")
	}
}

func TestDirectKinsDuplicateRegister(t *testing.T) {
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	defer apiserver.SetDefaultRegistry(nil)

	cbs := MakeCallbacks()

	err := reg.Register("kins", 1, "kinematics", CallbacksPtr(&cbs))
	if err != nil {
		t.Fatalf("first Register: %v", err)
	}

	err = reg.Register("kins", 1, "kinematics", CallbacksPtr(&cbs))
	if err == nil {
		t.Fatal("expected EEXIST for duplicate registration")
	}
}
