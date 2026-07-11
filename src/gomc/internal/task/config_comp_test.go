// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"math"
	"os"
	"path/filepath"
	"testing"
)

type compTriplet struct{ nom, fwd, rev float64 }

func approxComp(got, want []compTriplet) bool {
	if len(got) != len(want) {
		return false
	}
	for i := range got {
		if math.Abs(got[i].nom-want[i].nom) > 1e-9 ||
			math.Abs(got[i].fwd-want[i].fwd) > 1e-9 ||
			math.Abs(got[i].rev-want[i].rev) > 1e-9 {
			return false
		}
	}
	return true
}

func TestLoadJointComp(t *testing.T) {
	dir := t.TempDir()
	file := filepath.Join(dir, "comp.txt")
	// Header comment + a blank line exercise the skip path; C++ would stop at the
	// comment, gomc tolerates it (strict superset for pure-triplet files).
	content := "# leadscrew comp for joint 2\n" +
		"0.0 0.0 -0.001\n" +
		"0.1 0.098 0.051\n" +
		"\n" +
		"0.2 0.171 0.194\n"
	if err := os.WriteFile(file, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}

	t.Run("type0_positions_to_diffs", func(t *testing.T) {
		var got []compTriplet
		err := loadJointComp(2, file, 0, func(joint int32, nom, fwd, rev float64) error {
			if joint != 2 {
				t.Errorf("joint = %d, want 2", joint)
			}
			got = append(got, compTriplet{nom, fwd, rev})
			return nil
		})
		if err != nil {
			t.Fatalf("loadJointComp: %v", err)
		}
		// type 0: fwd/rev trims = nominal - value.
		want := []compTriplet{
			{0.0, 0.0, 0.001},
			{0.1, 0.002, 0.049},
			{0.2, 0.029, 0.006},
		}
		if !approxComp(got, want) {
			t.Errorf("type0: got %v, want %v", got, want)
		}
	})

	t.Run("type1_passthrough", func(t *testing.T) {
		var got []compTriplet
		err := loadJointComp(0, file, 1, func(_ int32, nom, fwd, rev float64) error {
			got = append(got, compTriplet{nom, fwd, rev})
			return nil
		})
		if err != nil {
			t.Fatalf("loadJointComp: %v", err)
		}
		want := []compTriplet{
			{0.0, 0.0, -0.001},
			{0.1, 0.098, 0.051},
			{0.2, 0.171, 0.194},
		}
		if !approxComp(got, want) {
			t.Errorf("type1: got %v, want %v", got, want)
		}
	})

	t.Run("no_triplets_errors", func(t *testing.T) {
		empty := filepath.Join(dir, "empty.txt")
		if err := os.WriteFile(empty, []byte("# only a comment\n"), 0o644); err != nil {
			t.Fatal(err)
		}
		if err := loadJointComp(0, empty, 0, func(int32, float64, float64, float64) error { return nil }); err == nil {
			t.Error("expected an error for a file with no triplets")
		}
	})

	t.Run("missing_file_errors", func(t *testing.T) {
		if err := loadJointComp(0, filepath.Join(dir, "nope.txt"), 0, func(int32, float64, float64, float64) error { return nil }); err == nil {
			t.Error("expected an error for a missing file")
		}
	})
}
