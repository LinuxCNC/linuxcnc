// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2

// Package hallibtest lets a test binary link the HAL implementation with a
// single blank import.
//
// It blank-imports internal/hallib, which provides the hal_lib / uspace_rtapi C
// symbols that pkg/hal only declares. The production binary gets these by
// cmd/gomc-server importing internal/hallib directly; a test binary built from a
// package that uses HAL but does not otherwise import hallib fails to link
// (undefined reference to hal_ready, hal_pin_float_new, ...).
//
// Each package that needs it has a link_test.go containing a single blank import
// of this package (Go requires one such file per test binary), instead of
// repeating this explanation nine times.
package hallibtest

import _ "github.com/sittner/linuxcnc/src/gomc/internal/hallib"
