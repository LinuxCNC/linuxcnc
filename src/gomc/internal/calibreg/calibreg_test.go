package calibreg_test

import (
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/calibreg"
)

func TestRecordAndGetAll(t *testing.T) {
	calibreg.Reset()

	calibreg.Record(calibreg.IniPinMapping{
		Pin:      "pid.0.Pgain",
		Section:  "JOINT_0",
		Key:      "P",
		IniValue: 150.0,
	})
	calibreg.Record(calibreg.IniPinMapping{
		Pin:      "pid.0.Igain",
		Section:  "JOINT_0",
		Key:      "I",
		IniValue: 0.5,
	})

	all := calibreg.GetAll()
	if len(all) != 2 {
		t.Fatalf("GetAll: got %d mappings, want 2", len(all))
	}
	if all[0].Pin != "pid.0.Pgain" || all[0].Section != "JOINT_0" || all[0].Key != "P" {
		t.Errorf("mapping[0] = %+v, unexpected", all[0])
	}
	if all[1].IniValue != 0.5 {
		t.Errorf("mapping[1].IniValue = %f, want 0.5", all[1].IniValue)
	}

	// Verify GetAll returns a copy.
	all[0].Pin = "modified"
	fresh := calibreg.GetAll()
	if fresh[0].Pin != "pid.0.Pgain" {
		t.Error("GetAll did not return a copy")
	}
}

func TestReset(t *testing.T) {
	calibreg.Reset()
	calibreg.Record(calibreg.IniPinMapping{Pin: "x", Section: "S", Key: "K", IniValue: 1.0})
	calibreg.Reset()
	if len(calibreg.GetAll()) != 0 {
		t.Error("Reset did not clear mappings")
	}
}
