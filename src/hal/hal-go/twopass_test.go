package hal

import "testing"

func TestTwopassCollector_MergeNames(t *testing.T) {
	c := NewTwopassCollector()
	c.CollectLoadRT("pid", []string{"names=pid.x,pid.y"})
	c.CollectLoadRT("pid", []string{"names=pid.y,pid.z"})

	cmds := c.MergedLoadRTCommands()
	if len(cmds) != 1 {
		t.Fatalf("expected 1 merged command, got %d", len(cmds))
	}
	// Should have pid.x, pid.y, pid.z (pid.y not duplicated)
	found := false
	for _, arg := range cmds[0] {
		if arg == "names=pid.x,pid.y,pid.z" {
			found = true
		}
	}
	if !found {
		t.Errorf("expected merged names=pid.x,pid.y,pid.z, got %v", cmds[0])
	}
}

func TestTwopassCollector_MergeCount(t *testing.T) {
	c := NewTwopassCollector()
	c.CollectLoadRT("and2", []string{"count=3"})
	c.CollectLoadRT("and2", []string{"count=5"})

	cmds := c.MergedLoadRTCommands()
	if len(cmds) != 1 {
		t.Fatalf("expected 1 merged command, got %d", len(cmds))
	}
	found := false
	for _, arg := range cmds[0] {
		if arg == "count=5" {
			found = true
		}
	}
	if !found {
		t.Errorf("expected count=5, got %v", cmds[0])
	}
}

func TestTwopassCollector_MultipleModules(t *testing.T) {
	c := NewTwopassCollector()
	c.CollectLoadRT("pid", []string{"names=pid.x"})
	c.CollectLoadRT("and2", []string{"count=2"})
	c.CollectLoadRT("pid", []string{"names=pid.y"})

	cmds := c.MergedLoadRTCommands()
	if len(cmds) != 2 {
		t.Fatalf("expected 2 modules, got %d", len(cmds))
	}
	// Order is preserved by first-seen insertion: pid was first, and2 was second.
	if cmds[0][0] != "pid" {
		t.Errorf("expected first module pid, got %s", cmds[0][0])
	}
	if cmds[1][0] != "and2" {
		t.Errorf("expected second module and2, got %s", cmds[1][0])
	}
}

func TestIsLoadRT(t *testing.T) {
	if !IsLoadRT([]string{"loadrt", "pid", "names=pid.x"}) {
		t.Error("expected true for loadrt command")
	}
	if IsLoadRT([]string{"addf", "pid.x", "servo-thread"}) {
		t.Error("expected false for non-loadrt command")
	}
	if IsLoadRT([]string{}) {
		t.Error("expected false for empty tokens")
	}
}
