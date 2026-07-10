// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package apiserver

import (
	"encoding/json"
	"net/http/httptest"
	"syscall"
	"testing"
)

func TestRuneLen(t *testing.T) {
	// Multi-byte characters count as one each (chars, not bytes).
	if got := RuneLen("héllo"); got != 5 {
		t.Errorf("RuneLen(héllo) = %d, want 5", got)
	}
	if got := RuneLen(""); got != 0 {
		t.Errorf("RuneLen(empty) = %d, want 0", got)
	}
}

func TestValidateRegex(t *testing.T) {
	re := MustRegex("^[a-z]+$")
	if err := ValidateRegex("name", re, "abc"); err != nil {
		t.Errorf("ValidateRegex(abc) = %v, want nil", err)
	}
	err := ValidateRegex("name", re, "Ab3")
	if err == nil {
		t.Fatal("ValidateRegex(Ab3) = nil, want error")
	}
	if err.Field != "name" || err.Constraint != "regex" {
		t.Errorf("err = %+v, want field=name constraint=regex", err)
	}
}

func TestWriteDispatchErrorValidation(t *testing.T) {
	rec := httptest.NewRecorder()
	writeDispatchError(rec, NewValidationError("entry.diameter", "min", "entry.diameter must be >= 0"))

	if rec.Code != 400 {
		t.Fatalf("status = %d, want 400", rec.Code)
	}
	if ct := rec.Header().Get("Content-Type"); ct != "application/json" {
		t.Errorf("Content-Type = %q, want application/json", ct)
	}

	var body struct {
		Error      string `json:"error"`
		Code       int    `json:"code"`
		Field      string `json:"field"`
		Constraint string `json:"constraint"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &body); err != nil {
		t.Fatalf("body not JSON: %v", err)
	}
	if body.Code != 400 || body.Field != "entry.diameter" ||
		body.Constraint != "min" || body.Error != "entry.diameter must be >= 0" {
		t.Errorf("body = %+v, want the validation fields", body)
	}
}

func TestWriteDispatchErrorErrnoStillWorks(t *testing.T) {
	// A non-validation error must still follow the generic errno mapping.
	rec := httptest.NewRecorder()
	writeDispatchError(rec, syscall.EINVAL)
	if rec.Code != 400 {
		t.Errorf("EINVAL status = %d, want 400", rec.Code)
	}
}
