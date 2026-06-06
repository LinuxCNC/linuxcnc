package kinstest

import (
	"math"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

// setupPuma sets up registry, loads pumakins, returns cleanup func.
func setupPuma(t *testing.T) {
	t.Helper()
	reg := apiserver.NewRegistry()
	apiserver.SetDefaultRegistry(reg)
	t.Cleanup(func() { apiserver.SetDefaultRegistry(nil) })

	mod, err := loadKinsModule("pumakins", true)
	if err != nil {
		t.Fatalf("loadKinsModule(pumakins): %v", err)
	}
	t.Cleanup(func() { unloadKinsModule(mod) })
}

func TestPumakinsLoadAndRegister(t *testing.T) {
	setupPuma(t)
	cbs := getKinsCallbacksFor("pumakins")
	if cbs == nil {
		t.Fatal("kins_api_get returned nil — pumakins registration failed")
	}
}

func TestPumakinsRoundTrip(t *testing.T) {
	setupPuma(t)
	cbs := getKinsCallbacksFor("pumakins")
	if cbs == nil {
		t.Fatal("kins_api_get returned nil")
	}

	// Test several joint configurations for forward→inverse round-trip.
	cases := []struct {
		name   string
		joints [16]float64
	}{
		{"zero", [16]float64{0, 0, 0, 0, 0, 0}},
		{"j1=45", [16]float64{45, 0, 0, 0, 0, 0}},
		{"j5=45", [16]float64{0, 0, 0, 0, 45, 0}},
		{"j1=30,j2=20", [16]float64{30, 20, 0, 0, 0, 0}},
		{"mixed", [16]float64{10, 20, -30, 40, 50, 60}},
		{"wrist", [16]float64{0, 0, 0, 30, 45, 60}},
		{"wide", [16]float64{45, 30, -20, 10, 60, -15}},
	}

	const tol = 1e-6

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			world, rc := callForward(cbs, tc.joints)
			if rc != 0 {
				t.Fatalf("forward returned %d", rc)
			}

			result, rc := callInverse(cbs, world)
			if rc != 0 {
				t.Fatalf("inverse returned %d", rc)
			}

			for i := 0; i < 6; i++ {
				diff := math.Abs(result[i] - tc.joints[i])
				if diff > tol {
					t.Errorf("joint[%d]: got %.9f, want %.9f (diff=%.2e)",
						i, result[i], tc.joints[i], diff)
				}
			}
		})
	}
}

// TestPumakinsRPYConvention verifies the A/B/C output matches the
// posemath ZYX convention: A=roll(about X), B=pitch(about Y), C=yaw(about Z).
// When j1 (base Z rotation) changes, only C (yaw) should change significantly.
func TestPumakinsRPYConvention(t *testing.T) {
	setupPuma(t)
	cbs := getKinsCallbacksFor("pumakins")
	if cbs == nil {
		t.Fatal("kins_api_get returned nil")
	}

	// Zero position.
	world0, rc := callForward(cbs, [16]float64{})
	if rc != 0 {
		t.Fatalf("forward(zero) returned %d", rc)
	}

	// At zero, pitch (B) should be near 0.
	if math.Abs(float64(world0.b)) > 1.0 {
		t.Errorf("zero position: B(pitch) = %.3f, expected near 0", float64(world0.b))
	}

	// j1=90 should primarily affect yaw (C) — base rotates about Z.
	world90, rc := callForward(cbs, [16]float64{90, 0, 0, 0, 0, 0})
	if rc != 0 {
		t.Fatalf("forward(j1=90) returned %d", rc)
	}

	// Yaw should differ by ~90° from zero position.
	yawDiff := math.Abs(float64(world90.c) - float64(world0.c))
	if math.Abs(yawDiff-90.0) > 1.0 {
		t.Errorf("j1=90 yaw change = %.3f, expected ~90° (C0=%.3f, C90=%.3f)",
			yawDiff, float64(world0.c), float64(world90.c))
	}

	// Pitch (B) should remain approximately unchanged.
	if math.Abs(float64(world90.b)-float64(world0.b)) > 1.0 {
		t.Errorf("j1=90 pitch change = %.3f, expected ~0",
			math.Abs(float64(world90.b)-float64(world0.b)))
	}
}
