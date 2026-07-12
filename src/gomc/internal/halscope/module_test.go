package halscope

import "testing"

func TestParseHalscopeArgs(t *testing.T) {
	const (
		defNum     = 16000
		minNum     = 16 // HALSCOPE_MAX_CHANNELS
		defPersist = "persistence"
	)

	tests := []struct {
		name        string
		args        []string
		wantNum     int
		wantPersist string
		wantErr     bool
	}{
		{
			name:        "no args uses defaults",
			args:        nil,
			wantNum:     defNum,
			wantPersist: defPersist,
		},
		{
			name:        "num_samples override",
			args:        []string{"num_samples=8000"},
			wantNum:     8000,
			wantPersist: defPersist,
		},
		{
			name:        "persist_instance override",
			args:        []string{"persist_instance=scope_state"},
			wantNum:     defNum,
			wantPersist: "scope_state",
		},
		{
			name:        "both keys, order independent",
			args:        []string{"persist_instance=foo", "num_samples=32"},
			wantNum:     32,
			wantPersist: "foo",
		},
		{
			name:        "unknown keys ignored",
			args:        []string{"bogus=1", "num_samples=64", "flag"},
			wantNum:     64,
			wantPersist: defPersist,
		},
		{
			name:        "num_samples at minimum is accepted",
			args:        []string{"num_samples=16"},
			wantNum:     minNum,
			wantPersist: defPersist,
		},
		{
			name:    "num_samples below minimum rejected",
			args:    []string{"num_samples=15"},
			wantErr: true,
		},
		{
			name:    "num_samples zero rejected",
			args:    []string{"num_samples=0"},
			wantErr: true,
		},
		{
			name:    "num_samples negative rejected",
			args:    []string{"num_samples=-100"},
			wantErr: true,
		},
		{
			name:    "num_samples non-integer rejected",
			args:    []string{"num_samples=lots"},
			wantErr: true,
		},
		{
			name:    "num_samples empty value rejected",
			args:    []string{"num_samples="},
			wantErr: true,
		},
		{
			name:        "persist_instance empty value uses default",
			args:        []string{"persist_instance="},
			wantNum:     defNum,
			wantPersist: defPersist,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			gotNum, gotPersist, err := parseHalscopeArgs(tt.args, defNum, minNum, defPersist)
			if tt.wantErr {
				if err == nil {
					t.Fatalf("expected error, got num=%d persist=%q", gotNum, gotPersist)
				}
				return
			}
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			if gotNum != tt.wantNum {
				t.Errorf("numSamples = %d, want %d", gotNum, tt.wantNum)
			}
			if gotPersist != tt.wantPersist {
				t.Errorf("persistInstance = %q, want %q", gotPersist, tt.wantPersist)
			}
		})
	}
}
