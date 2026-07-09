// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package adsbridge

// Blank-import the HAL implementation so the test binary links the hal_lib /
// uspace_rtapi symbols that pkg/hal only declares. The production binary gets
// these by cmd/gomc-server importing internal/hallib; test binaries built from
// this package alone otherwise fail to link (undefined reference to hal_ready,
// hal_pin_float_new, ...). Same pattern as internal/task/hallink_test.go.
import _ "github.com/sittner/linuxcnc/src/gomc/internal/hallib"
