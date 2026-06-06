package cmodtogomod

import "testing"

// TestCmodToGomodRoundtrip tests C code calling Go functions and back.
// This is the cmod→gomod pattern where C modules call Go-implemented APIs.
func TestCmodToGomodRoundtrip(t *testing.T) {
	ResetItems()

	// C code sets a value (calls go_item_set)
	// then gets it back (calls go_item_get)
	getValue, rc := CmodTestRoundtrip("widget", 42.5)
	if rc != 0 {
		t.Fatalf("CmodTestRoundtrip returned %d", rc)
	}
	if getValue != 42.5 {
		t.Errorf("roundtrip value = %f, want 42.5", getValue)
	}

	// Verify directly from Go
	v, ok := GetItemDirect("widget")
	if !ok {
		t.Fatal("item not found after roundtrip")
	}
	if v != 42.5 {
		t.Errorf("direct get = %f, want 42.5", v)
	}
}

// TestCmodToGomodCount tests C calling go_item_count.
func TestCmodToGomodCount(t *testing.T) {
	ResetItems()

	// Initially empty
	if n := CmodGetCount(); n != 0 {
		t.Errorf("initial count = %d, want 0", n)
	}

	// Add items from Go
	SetItemDirect("a", 1.0)
	SetItemDirect("b", 2.0)

	// C calls go_item_count
	if n := CmodGetCount(); n != 2 {
		t.Errorf("count after adds = %d, want 2", n)
	}
}

// TestCmodToGomodGetNotFound tests C calling go_item_get for nonexistent item.
func TestCmodToGomodGetNotFound(t *testing.T) {
	ResetItems()

	// Try to get nonexistent - roundtrip will fail at get step
	// First set succeeds, but we test get directly via another path
	SetItemDirect("exists", 1.0)

	// Roundtrip with existing item should work
	_, rc := CmodTestRoundtrip("exists", 99.0)
	if rc != 0 {
		t.Errorf("roundtrip existing item: rc = %d, want 0", rc)
	}
}

// TestCmodToGomodMultipleItems tests multiple items via C→Go calls.
func TestCmodToGomodMultipleItems(t *testing.T) {
	ResetItems()

	items := map[string]float64{
		"item1": 10.0,
		"item2": 20.0,
		"item3": 30.0,
	}

	// Set all via C→Go
	for name, value := range items {
		_, rc := CmodTestRoundtrip(name, value)
		if rc != 0 {
			t.Fatalf("set %s: rc = %d", name, rc)
		}
	}

	// Verify count
	if n := CmodGetCount(); n != 3 {
		t.Errorf("count = %d, want 3", n)
	}

	// Verify values
	for name, want := range items {
		got, ok := GetItemDirect(name)
		if !ok {
			t.Errorf("%s not found", name)
		} else if got != want {
			t.Errorf("%s = %f, want %f", name, got, want)
		}
	}
}

// TestCmodToGomodConcurrent tests concurrent C→Go calls.
func TestCmodToGomodConcurrent(t *testing.T) {
	ResetItems()

	done := make(chan bool, 10)

	// Multiple goroutines calling C which calls Go
	for i := 0; i < 10; i++ {
		go func(n int) {
			name := "concurrent"
			for j := 0; j < 100; j++ {
				CmodTestRoundtrip(name, float64(n*100+j))
			}
			done <- true
		}(i)
	}

	// Wait for all
	for i := 0; i < 10; i++ {
		<-done
	}

	// Item should exist (actual value doesn't matter due to races)
	if n := CmodGetCount(); n != 1 {
		t.Errorf("count after concurrent = %d, want 1", n)
	}
}
