// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import "testing"

// AUTO_REVERSE / AUTO_FORWARD drive run-in-reverse and resume-forward.
// AUTO_FORWARD (=5) previously had no constant and no switch case, so a client
// sending it got a silent nil-success that never reached motion.Forward().
func TestAutoCommand_ReverseForwardCallMotion(t *testing.T) {
	cases := []struct {
		name string
		cmd  int32
		want string
	}{
		{"reverse", AutoReverse, "Reverse"},
		{"forward", AutoForward, "Forward"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			task, mot, _ := newTestTask()
			bringUp(task)
			task.SetMode(int32(ModeAuto))

			if err := task.AutoCommand(tc.cmd, 0); err != nil {
				t.Fatalf("AutoCommand(%s): %v", tc.name, err)
			}
			if got := mot.last(); got != tc.want {
				t.Fatalf("AutoCommand(%s): motion call = %q, want %q", tc.name, got, tc.want)
			}
		})
	}
}
