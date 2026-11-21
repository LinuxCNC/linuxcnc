#!/usr/bin/env python3
#
# Copyright © 2026 Petter Reinholdtsen.
# Copyright © 2026 LinuxCNC developers.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.

"""Test halshow.py functionality without GUI. Requires QT_QPA_PLATFORM=offscreen."""
import sys, os, tempfile

repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(repo_root, "lib", "python"))
sys.path.insert(0, os.path.join(repo_root, "scripts"))

from halshow import Preferences, format_value, HalApi

def test_preferences_defaults():
    p = Preferences("/dev/null")
    assert p.ratio == 0.3
    assert p.old_w_leftf == 160
    assert isinstance(p.watchlist, list) and len(p.watchlist) == 0
    assert p.workmode == "showhal"
    assert p.watchInterval == 200
    assert p.col1_width == 100
    assert p.ffmts == ""
    assert p.ifmts == ""
    assert p.alwaysOnTop is False
    assert p.autoSaveWatchlist is True
    assert p.separateParams is True
    print("OK: Preferences defaults")

def test_preferences_load_tcl_format():
    """Verify the Python halshow can read a preference file written by Tcl halshow."""
    fd, path = tempfile.mkstemp(suffix=".txt")
    os.close(fd)
    try:
        # Exact format produced by tcl/bin/halshow.tcl saveIni()
        tcl_prefs = """# Halshow settings
# This file is generated automatically.
wm geometry . 906x558+123+456
placeFrames 0.35
set ::ratio 0.35
set ::old_w_leftf 160
set ::watchlist {
    pin.one
    sig.two
}
set ::workmode showhal
set ::watchInterval 300
set ::col1_width 120
set ::ffmts %.4f
set ::ifmts %03d
set ::alwaysOnTop 0
set ::autoSaveWatchlist 1
set ::separateParams 0
"""
        with open(path, "w") as f:
            f.write(tcl_prefs)

        p = Preferences(path)
        assert p.load(), "load returned False"
        assert p.ratio == 0.35, f"ratio={p.ratio}"
        assert p.old_w_leftf == 160
        assert p.watchlist == ["pin.one", "sig.two"], f"watchlist={p.watchlist}"
        assert p.workmode == "showhal", f"workmode={p.workmode}"
        assert p.watchInterval == 300, f"watchInterval={p.watchInterval}"
        assert p.col1_width == 120, f"col1_width={p.col1_width}"
        assert p.ffmts == "%.4f", f"ffmts={p.ffmts!r}"
        assert p.ifmts == "%03d", f"ifmts={p.ifmts!r}"
        assert p.alwaysOnTop is False, f"alwaysOnTop={p.alwaysOnTop}"
        assert p.autoSaveWatchlist is True, f"autoSaveWatchlist={p.autoSaveWatchlist}"
        assert p.separateParams is False, f"separateParams={p.separateParams}"
    finally:
        os.unlink(path)
    print("OK: Preferences load Tcl format")

def test_preferences_roundtrip():
    fd, path = tempfile.mkstemp(suffix=".txt")
    os.close(fd)
    try:
        p1 = Preferences(path)
        p1.ratio = 0.5
        p1.watchInterval = 200
        p1.col1_width = 150
        p1.workmode = "watchhal"
        p1.alwaysOnTop = True
        p1.ffmts = '"%.4f"'
        p1.ifmts = "'03d'"
        p1.watchlist = ["pin.one", "sig.two"]
        p1.save()

        p2 = Preferences(path)
        assert p2.load(), "load returned False"
        assert p2.ratio == 0.5, f"ratio={p2.ratio}"
        assert p2.watchInterval == 200, f"watchInterval={p2.watchInterval}"
        assert p2.col1_width == 150, f"col1_width={p2.col1_width}"
        assert p2.workmode == "watchhal", f"workmode={p2.workmode}"
        assert p2.alwaysOnTop is True
    finally:
        os.unlink(path)
    print("OK: Preferences save/load round-trip")

def test_format_value():
    assert format_value("42", "s32") == "42"
    assert format_value("-10", "u32") == "-10"
    assert format_value("99", "s32", ifmt="04d") == "0099"
    result = format_value("3.14159", "float", ffmt="%.2f")
    assert result == "3.14", f"Expected '3.14', got {result!r}"
    result = format_value("notanumber", "unknown")
    assert result == "notanumber"
    print("OK: format_value")

def test_type_mappings():
    assert HalApi.TYPE_NAME[0] == "bit"
    assert HalApi.TYPE_NAME[1] == "float"
    assert HalApi.TYPE_NAME[6] == "port"
    assert HalApi.PIN_DIR[16] == "IN"
    assert HalApi.PIN_DIR[32] == "OUT"
    assert HalApi.PIN_DIR[48] == "I/O"
    assert HalApi.PARAM_DIR[64] == "RO"
    assert HalApi.PARAM_DIR[192] == "RW"
    print("OK: Type/direction mappings")

def test_list_unknown_type():
    result = HalApi.list("bogus_type")
    assert result == "", f"Expected '', got {result!r}"
    print("OK: list() unknown type returns empty string")

def test_halapi_with_shm():
    """Test HalApi operations with actual SHM component (requires running HAL)."""
    try:
        HalApi.init()
    except RuntimeError as e:
        print(f"SKIP: HalApi SHM tests ({e})")
        return

    # list() should return strings for all types
    for typ in ("pin", "param", "sig", "comp", "funct", "thread"):
        result = HalApi.list(typ)
        assert isinstance(result, str), f"list('{typ}') returned {type(result)}"

    # Components should include our own
    comps = HalApi.list("comp")
    assert "_halshow_" in comps, "Our component not found in list('comp')"

    # show() on a known component should work (use SHM-based pin/param instead of halcmd fallback)
    pins = HalApi.list("pin")
    if pins.strip():
        pin_name = [l for l in pins.split("\n") if l.strip()][0]
        shown = HalApi.show("pin", pin_name)
        assert isinstance(shown, str), f"show returned {type(shown)}"
    print("OK: HalApi SHM operations (list, show)")

def test_halapi_cleanup():
    """Verify HalApi.cleanup() removes the component from SHM."""
    import _hal

    # Ensure clean state - previous tests may have left _comp in a bad state
    if not HalApi._initialized:
        HalApi.init()
    comp_name = f"_halshow_{os.getpid()}"

    # Mark ready so it becomes visible to get_info_components
    HalApi._comp.ready()
    components_before = _hal.get_info_components()
    our_comp = [c for c in components_before if c["NAME"] == comp_name]
    assert len(our_comp) > 0, f"Component {comp_name} not visible after ready()"

    # Cleanup should remove it from SHM. After exit(), _comp becomes invalid,
    # so we need a fresh component to query SHM with afterward.
    HalApi.cleanup()

    # Create a temporary component to query SHM (our original _comp is dead)
    tmp = _hal.component(f"_testz_{os.getpid()}")
    try:
        components_after = _hal.get_info_components()
        remaining = [c for c in components_after if c["NAME"] == comp_name]
        assert len(remaining) == 0, f"Component {comp_name} still in SHM after cleanup: {remaining}"
    finally:
        tmp.exit()

    print("OK: HalApi.cleanup removes component from SHM")


def main():
    errors = []
    tests = [
        test_preferences_defaults,
        test_preferences_roundtrip,
        test_preferences_load_tcl_format,
        test_format_value,
        test_type_mappings,
        test_list_unknown_type,
        test_halapi_with_shm,
        test_halapi_cleanup,
    ]
    for fn in tests:
        try:
            fn()
        except Exception as e:
            errors.append(f"{fn.__name__}: {type(e).__name__}: {e}")

    if errors:
        print(f"\nFAILURES ({len(errors)}):")
        for e in errors:
            print(f"  - {e}")
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
