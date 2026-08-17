#!/usr/bin/env python3
# Developer helper: validate the four MTConnect documents this agent produces
# against the official MTConnect 1.7 XSDs (plus the LinuxCNC extension schema for
# the streaming endpoints).
#
# Requires the `xmlschema` package (XSD 1.1; libxml2/xmllint cannot parse the
# 1.7 schema, which uses XSD-1.1 constructs).  The official schemas are fetched
# from schemas.mtconnect.org unless a local directory is given via $MTC_XSD_DIR.
#
#   pip install xmlschema
#   ./validate_schemas.py [INI]        # default: example.ini
#
# Exit status is non-zero if any document fails, so it can gate CI.

import io
import os
import sys

BASE = "http://schemas.mtconnect.org/schemas"


def _xsd(name):
    d = os.environ.get("MTC_XSD_DIR")
    return os.path.join(d, name) if d else "%s/%s" % (BASE, name)


def _bootstrap_mtc():
    # mtc installs to $EMC2_HOME/lib/python (on PYTHONPATH under rip-environment);
    # add it explicitly so this runs without the full environment.
    home = os.environ.get("EMC2_HOME")
    if home:
        sys.path.insert(0, os.path.join(home, "lib", "python"))


def main(argv):
    ini_path = argv[1] if len(argv) > 1 else "example.ini"
    try:
        import xmlschema
    except ImportError:
        print("error: needs the 'xmlschema' package (pip install xmlschema)")
        return 2

    _bootstrap_mtc()
    from mtc.agent import AgentState, asset_dir
    EXT_XSD = os.path.join(asset_dir(), "mtconnect-linuxcnc-1.xsd")
    state = AgentState(ini_path)
    state.poll_once()
    docs = {
        "MTConnectDevices_1.7.xsd": ("/probe", state.probe_document()),
        "MTConnectAssets_1.7.xsd": ("/assets", state.assets_document()),
        # Streams validate against the extension schema (which imports the base).
        EXT_XSD: ("/current", state.current_document()),
    }
    ok = True
    for xsd, (label, doc) in docs.items():
        loc = xsd if xsd == EXT_XSD else _xsd(xsd)
        schema = xmlschema.XMLSchema11(loc, validation="skip")
        errors = list(schema.iter_errors(io.StringIO(doc)))
        if errors:
            ok = False
            print("FAIL %s (%d errors)" % (label, len(errors)))
            for e in errors[:8]:
                print("   ", e.reason or e.message)
        else:
            base = "base" if xsd != EXT_XSD else "base+extension"
            print("ok   %s valid (%s)" % (label, base))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
