package classicladder

import (
	"testing"
)

func TestCompileCompare_Constants(t *testing.T) {
	tests := []struct {
		expr string
		want bool
	}{
		{"5>3", true},
		{"3>5", false},
		{"5=5", true},
		{"5=3", false},
		{"3<5", true},
		{"5<3", false},
		{"5>=5", true},
		{"5>=6", false},
		{"5<=5", true},
		{"6<=5", false},
		{"5<>3", true},
		{"5<>5", false},
	}

	for _, tt := range tests {
		ce, err := compileExpression(tt.expr, 0)
		if err != nil {
			t.Errorf("compileExpression(%q): %v", tt.expr, err)
			continue
		}
		if ce.valid != 1 {
			t.Errorf("compileExpression(%q): not valid", tt.expr)
			continue
		}
		// Evaluate using C evaluator - need an RT instance
		got := evalCompiledForTest(ce)
		if got != tt.want {
			t.Errorf("eval(%q) = %v, want %v", tt.expr, got, tt.want)
		}
	}
}

func TestCompileCompare_Arithmetic(t *testing.T) {
	tests := []struct {
		expr string
		want bool
	}{
		{"2+3>4", true},
		{"2*3=6", true},
		{"10/2=5", true},
		{"10%3=1", true},
		{"2**3=8", true},
		{"(2+3)*2=10", true},
		{"2+3*2=8", true}, // precedence: 2+(3*2)=8
		{"!0>0", true},    // !0 = 1, 1>0
		{"ABS(-5)=5", true},
		{"MINI(3,7)=3", true},
		{"MAXI(3,7)=7", true},
		{"MOY(4,6)=5", true},
		{"$FF=255", true},
		{"$10=16", true},
	}

	for _, tt := range tests {
		ce, err := compileExpression(tt.expr, 0)
		if err != nil {
			t.Errorf("compileExpression(%q): %v", tt.expr, err)
			continue
		}
		got := evalCompiledForTest(ce)
		if got != tt.want {
			t.Errorf("eval(%q) = %v, want %v", tt.expr, got, tt.want)
		}
	}
}

func TestCompileCompare_BitwiseOps(t *testing.T) {
	tests := []struct {
		expr string
		want bool
	}{
		{"3&1=1", true}, // 0b11 & 0b01 = 0b01
		{"3|4=7", true}, // 0b011 | 0b100 = 0b111
		{"5^3=6", true}, // 0b101 ^ 0b011 = 0b110
	}

	for _, tt := range tests {
		ce, err := compileExpression(tt.expr, 0)
		if err != nil {
			t.Errorf("compileExpression(%q): %v", tt.expr, err)
			continue
		}
		got := evalCompiledForTest(ce)
		if got != tt.want {
			t.Errorf("eval(%q) = %v, want %v", tt.expr, got, tt.want)
		}
	}
}

func TestCompileOperate_Simple(t *testing.T) {
	// @200/0@ := 42  — store 42 into MEM_WORD[0]
	ce, err := compileExpression("@200/0@ := 42", 1)
	if err != nil {
		t.Fatalf("compileExpression: %v", err)
	}
	if ce.valid != 1 {
		t.Fatal("not valid")
	}
	if ce.kind != 1 {
		t.Errorf("kind = %d, want 1", ce.kind)
	}
	// Verify it has a STORE_VAR at the end
	lastIdx := int(ce.len) - 1
	if ce.code[lastIdx].opcode != opStoreVar {
		t.Errorf("last opcode = %d, want STORE_VAR (%d)", ce.code[lastIdx].opcode, opStoreVar)
	}
}

func TestCompileOperate_WithArith(t *testing.T) {
	// @200/0@ := @200/1@ + @200/2@ * 3
	ce, err := compileExpression("@200/0@ := @200/1@ + @200/2@ * 3", 1)
	if err != nil {
		t.Fatalf("compileExpression: %v", err)
	}
	if ce.valid != 1 {
		t.Fatal("not valid")
	}
}

func TestCompileExpression_Variable(t *testing.T) {
	// @200/5@ > 10 — MEM_WORD[5] > 10
	ce, err := compileExpression("@200/5@ > 10", 0)
	if err != nil {
		t.Fatalf("compileExpression: %v", err)
	}
	if ce.valid != 1 {
		t.Fatal("not valid")
	}
	// First instruction should be LOAD_VAR
	if ce.code[0].opcode != opLoadVar {
		t.Errorf("first opcode = %d, want LOAD_VAR (%d)", ce.code[0].opcode, opLoadVar)
	}
	// Operand should be (200<<16 | 5)
	expected := int32((200 << 16) | 5)
	if int32(ce.code[0].operand) != expected {
		t.Errorf("operand = %d, want %d", ce.code[0].operand, expected)
	}
}

func TestCompileExpression_IndexedVar(t *testing.T) {
	// @200/0[200/5]@ > 0 — MEM_WORD[0 + MEM_WORD[5]] > 0
	ce, err := compileExpression("@200/0[200/5]@ > 0", 0)
	if err != nil {
		t.Fatalf("compileExpression: %v", err)
	}
	if ce.valid != 1 {
		t.Fatal("not valid")
	}
	// Should have LOAD_VAR (for index), LOAD_VAR_IDX, PUSH_CONST, CMP_GT
	if ce.code[0].opcode != opLoadVar {
		t.Errorf("code[0] opcode = %d, want LOAD_VAR", ce.code[0].opcode)
	}
	if ce.code[1].opcode != opLoadVarIdx {
		t.Errorf("code[1] opcode = %d, want LOAD_VAR_IDX", ce.code[1].opcode)
	}
}

func TestCompileExpression_Errors(t *testing.T) {
	tests := []string{
		"",       // empty (valid but not compiled)
		"5 > ",   // incomplete
		"5 + )",  // unmatched paren
		"FOO(1)", // unknown function
	}

	for _, expr := range tests {
		_, err := compileExpression(expr, 0)
		if expr == "" {
			// Empty is not an error, just empty
			continue
		}
		if err == nil {
			t.Errorf("compileExpression(%q): expected error, got nil", expr)
		}
	}
}

func TestCompileOperate_Evaluate(t *testing.T) {
	tests := []struct {
		expr   string
		offset int
		want   int32
	}{
		{"@200/0@ := 42", 0, 42},
		{"@200/1@ := 3 + 4 * 2", 1, 11},
		{"@200/2@ := (3 + 4) * 2", 2, 14},
		{"@200/3@ := ABS(-10)", 3, 10},
		{"@200/4@ := MINI(5, 3)", 4, 3},
		{"@200/5@ := MAXI(5, 3)", 5, 5},
		{"@200/6@ := 2 ** 8", 6, 256},
		{"@200/7@ := $1F", 7, 31},
	}

	for _, tt := range tests {
		ce, err := compileExpression(tt.expr, 1)
		if err != nil {
			t.Errorf("compileExpression(%q): %v", tt.expr, err)
			continue
		}
		got := evalOperateForTest(ce, tt.offset)
		if got != tt.want {
			t.Errorf("eval(%q) → word[%d] = %d, want %d", tt.expr, tt.offset, got, tt.want)
		}
	}
}
