#!/usr/bin/env python3

import hal

# These should all give a FutureWarning
a = hal.HAL_BIT
assert a == hal.Type.BOOL, "Bad BIT/BOOL"
a = hal.HAL_FLOAT
assert a == hal.Type.REAL, "Bad FLOAT/REAL"
a = hal.HAL_S32
assert a == hal.Type.SINT, "Bad S32/SINT"
a = hal.HAL_U32
assert a == hal.Type.UINT, "Bad U32/UINT"
a = hal.HAL_S64
assert a == hal.Type.SINT, "Bad S64/SINT"
a = hal.HAL_U64
assert a == hal.Type.UINT, "Bad U64/UINT"

