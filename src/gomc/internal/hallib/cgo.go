// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package hallib compiles the HAL infrastructure (hal_lib, uspace_rtapi_lib,
// uspace_rtapi_string) directly into the gomc-server binary.
// Import this package with a blank identifier to link the HAL runtime.
package hallib

/*
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include
#cgo LDFLAGS: -pthread -lrt -ldl
*/
import "C"
