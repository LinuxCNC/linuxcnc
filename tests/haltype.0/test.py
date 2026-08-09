#!/usr/bin/env python3
# Sanity checks for the native IntEnum tagging classes (halenum.hh).
#
# The classes are built from the hal.h constants in _hal and shared by
# every consumer: hal.Type/hal.Dir, the _hal module itself, and the
# halpp bindings (covered in tests/halpp).
import pickle
import sys

import _hal
import hal

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print("ok -", msg)
    else:
        print("FAIL -", msg)
        failures += 1

# Canonical members: the preferred spelling of each value, in the order
# interactive tools prefer (first occurrence of a duplicate value wins).
check([m.name for m in hal.Type] ==
      ["BOOL", "REAL", "SINT", "UINT", "PORT", "S32", "U32"],
      "hal.Type canonical members")
check([m.name for m in hal.Dir] ==
      ["IN", "OUT", "IO", "RO", "WO", "RW"],
      "hal.Dir canonical members")

# Values come straight from the hal.h constants exported by _hal.
for name in ("BOOL", "REAL", "S32", "U32", "PORT", "S64", "U64",
             "SINT", "UINT"):
    member = hal.Type[name]
    const = getattr(_hal, "HAL_" + name)
    check(int(member) == const,
          "hal.Type.%s == _hal.HAL_%s (%d)" % (name, name, const))
for name in ("IN", "OUT", "IO", "RO", "WO", "RW"):
    member = hal.Dir[name]
    const = getattr(_hal, "HAL_" + name)
    check(int(member) == const,
          "hal.Dir.%s == _hal.HAL_%s (%d)" % (name, name, const))

# The alternative spellings are aliases of the canonical members, both
# the fixed-width, the enumerator and the HAL_* macro spellings.
check(hal.Type.HAL_REAL is hal.Type.REAL, "HAL_REAL aliases REAL")
check(hal.Type.HAL_FLOAT is hal.Type.REAL, "HAL_FLOAT aliases REAL")
check(hal.Type.HAL_BIT is hal.Type.BOOL, "HAL_BIT aliases BOOL")
check(hal.Type.HAL_S32 is hal.Type.S32, "HAL_S32 aliases S32")
check(hal.Type.S64 is hal.Type.SINT, "S64 aliases SINT")
check(hal.Type.U64 is hal.Type.UINT, "U64 aliases UINT")
check(hal.Type.HAL_SINT is hal.Type.SINT, "HAL_SINT aliases SINT")
check(hal.Type.HAL_S64 is hal.Type.SINT, "HAL_S64 aliases SINT")
check(hal.Type.HAL_U64 is hal.Type.UINT, "HAL_U64 aliases UINT")
check(hal.Dir.HAL_IN is hal.Dir.IN, "HAL_IN aliases IN")
check(hal.Dir.HAL_RW is hal.Dir.RW, "HAL_RW aliases RW")

# IntEnum members are ints and interoperate with the plain constants.
check(isinstance(hal.Type.REAL, int), "hal.Type.REAL is an int")
check(hal.Type.REAL == hal.HAL_FLOAT, "hal.Type.REAL == hal.HAL_FLOAT")
check(hal.Dir.IO == hal.Dir.IN | hal.Dir.OUT, "hal.Dir.IO is IN|OUT")
check(hal.Dir.RW == hal.Dir.RO | hal.Dir.WO, "hal.Dir.RW is RO|WO")

# One shared pair of classes, reachable from every spelling.
check(hal.Type is _hal.Type, "hal.Type is _hal.Type")
check(hal.Dir is _hal.Dir, "hal.Dir is _hal.Dir")

# Construction from a value, the way query results are tagged.
check(hal.Type(2) is hal.Type.REAL, "hal.Type(2) is REAL")
check(hal.Dir(16) is hal.Dir.IN, "hal.Dir(16) is IN")
try:
    hal.Type(99)
    check(False, "hal.Type(99) raises ValueError")
except ValueError:
    check(True, "hal.Type(99) raises ValueError")

# Tags print with their names, not bare numbers.
check(repr(hal.Type.REAL) == "<Type.REAL: 2>", "repr shows the member name")

# Pickle round-trips through hal.Type (the class __module__ is "hal").
check(pickle.loads(pickle.dumps(hal.Type.REAL)) is hal.Type.REAL,
      "hal.Type.REAL survives pickle")

if failures:
    print("%d FAILURES" % failures)
    sys.exit(1)
print("ALL TESTS PASSED")
