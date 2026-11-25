#!/usr/bin/env python3
# Offline tests for the MTConnect agent prototype.
#
# Exercises the probe generator, the probe<->registry consistency, the
# observation buffer + streams, and the asset builder without a running
# LinuxCNC.  Run:  python3 test_mtc.py
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
import xml.etree.ElementTree as ET

# The mtc package installs to $EMC2_HOME/lib/python; make it importable when this
# is run from the config directory (rip-environment already puts it on the path).
_EMC2_HOME = os.environ.get("EMC2_HOME")
if _EMC2_HOME:
    sys.path.insert(0, os.path.join(_EMC2_HOME, "lib", "python"))

from mtc.ini_reader import IniReader
from mtc import kinematics as kin
from mtc.device_model import DeviceConfig, probe_xml, MTC_NS, EXT_NS
from mtc.observations import build_dataitems
from mtc.streams import (ObservationBuffer, current_xml, sample_xml, assets_xml,
                         STREAMS_NS)
from mtc.lcnc_source import ToolAsset, LcncSource
from mtc.agent import AgentState
from mtc.http_agent import HttpAgent
from mtc.shdr_agent import ShdrAgent

# Resolve config paths relative to this file, not the current directory, so the
# suite runs from any CWD (e.g. when wired into tests/ and driven by runtests).
_HERE = os.path.dirname(os.path.abspath(__file__))


def _cfg(relpath):
    return os.path.normpath(os.path.join(_HERE, relpath))


CONFIGS = {
    "3axis": _cfg("../axis.ini"),
    "5axis": _cfg("../vismach/5axis/table-rotary-tilting/xyzac-trt.ini"),
}
TS = "2026-07-01T12:00:00.000Z"


def load(path):
    ini = IniReader(path)
    model = kin.build_model(ini)
    config = DeviceConfig.from_ini(ini)
    return ini, model, config


def test_kinematics():
    _, model, _ = load(CONFIGS["5axis"])
    assert model.kins_module == "xyzac-trt-kins", model.kins_module
    assert model.coordinates == "XYZAC", model.coordinates
    assert model.joints_count == 5
    jmap = model.joint_axis_map()
    assert jmap == {0: "X", 1: "Y", 2: "Z", 3: "A", 4: "C"}, jmap
    kinds = {a.letter: a.kind for a in model.axes}
    assert kinds["A"] == "ANGULAR" and kinds["X"] == "LINEAR", kinds
    # A rotates about X, C about Z
    vecs = {a.letter: a.vector for a in model.axes}
    assert vecs["A"] == (1.0, 0.0, 0.0) and vecs["C"] == (0.0, 0.0, 1.0), vecs
    print("ok  kinematics (5-axis XYZAC mapping + rotary vectors)")


def test_probe_registry_consistency():
    for label, path in CONFIGS.items():
        _, model, config = load(path)
        xml = probe_xml(model, config, creation_time=TS)
        root = ET.fromstring(xml)
        probe_ids = {}
        for di in root.iter("{%s}DataItem" % MTC_NS):
            probe_ids[di.get("id")] = (di.get("category"), di.get("type"),
                                       di.get("subType"))
        for d in build_dataitems(model, config):
            assert d.id in probe_ids, "%s: %s missing from probe" % (label, d.id)
            cat, typ, sub = probe_ids[d.id]
            expected_type = ("x:" + d.type) if d.ext else d.type
            assert (cat, typ, sub) == (d.category, expected_type, d.subType), \
                "%s: %s attrs %s != %s" % (label, d.id, (cat, typ, sub),
                                           (d.category, d.type, d.subType))
        # kinematics extension present
        kext = root.find(".//{%s}Kinematics" % EXT_NS)
        assert kext is not None, "%s: no kinematics extension" % label
        assert kext.get("module") == model.kins_module
        print("ok  probe/registry consistency (%s, %d DataItems)"
              % (label, len(probe_ids)))


def test_buffer_and_streams():
    _, model, config = load(CONFIGS["3axis"])
    dataitems = build_dataitems(model, config)
    buf = ObservationBuffer(dataitems)

    n = buf.ingest({"execution": "READY", "pos_x": 1.0, "pos_y": 2.0}, TS)
    assert n == 3, n
    assert buf.ingest({"pos_x": 1.0}, TS) == 0, "unchanged value must not record"
    assert buf.ingest({"pos_x": 5.0}, TS) == 1
    assert buf.last_sequence == 4, buf.last_sequence

    # /current: every DataItem present, unset ones UNAVAILABLE
    cur = ET.fromstring(current_xml(buf, config, TS))
    observed = {e.get("dataItemId"): e.text
                for e in cur.iter() if e.get("dataItemId")}
    assert observed["pos_x"] == "5.0", observed.get("pos_x")
    assert observed["pos_z"] == "UNAVAILABLE", observed.get("pos_z")
    assert observed["execution"] == "READY"

    # /sample: sequence range 1..2 returns exactly the first two observations
    smp = ET.fromstring(sample_xml(buf, config, TS, from_seq=1, count=2))
    seqs = sorted(int(e.get("sequence")) for e in smp.iter()
                  if e.get("sequence"))
    assert seqs == [1, 2], seqs
    # ComponentStream grouping + Samples/Events wrappers exist
    assert cur.find(".//{%s}ComponentStream" % STREAMS_NS) is not None
    print("ok  buffer + current/sample streams")


def test_work_offset_table():
    _, model, config = load(CONFIGS["3axis"])
    buf = ObservationBuffer(build_dataitems(model, config))
    buf.ingest({"workoffset": {"G55": {"X": 1.5, "Y": -2.0, "Z": 0.0}}}, TS)
    cur = ET.fromstring(current_xml(buf, config, TS))
    # WORK_OFFSET is a standard EVENT type; TABLE representation streams as
    # <WorkOffsetTable> (not <WorkOffset>).
    wo = cur.find(".//{%s}WorkOffsetTable" % STREAMS_NS)
    assert wo is not None and wo.get("count") == "1", wo
    entry = wo.find("{%s}Entry" % STREAMS_NS)
    assert entry.get("key") == "G55", entry.attrib
    cells = {c.get("key"): c.text for c in entry.findall("{%s}Cell" % STREAMS_NS)}
    assert cells == {"X": "1.5", "Y": "-2", "Z": "0"}, cells
    # probe advertises it as a standard WORK_OFFSET EVENT with TABLE representation
    probe = ET.fromstring(probe_xml(model, config))
    di = [d for d in probe.iter("{%s}DataItem" % MTC_NS)
          if d.get("id") == "workoffset"][0]
    assert di.get("representation") == "TABLE" and di.get("type") == "WORK_OFFSET"
    assert di.get("category") == "EVENT", di.get("category")
    print("ok  work offset TABLE (WorkOffsetTable Entry/Cell + probe representation)")


def test_tool_offset_and_rotation():
    _, model, config = load(CONFIGS["3axis"])
    buf = ObservationBuffer(build_dataitems(model, config))
    buf.ingest({"tooloffset": {"T3": {"X": 0.0, "Y": 0.0, "Z": 2.5}},
                "xyrotation": 30.0}, TS)
    cur = ET.fromstring(current_xml(buf, config, TS))

    # TOOL_OFFSET is a standard EVENT type -> <ToolOffsetTable> in STREAMS_NS.
    to = cur.find(".//{%s}ToolOffsetTable" % STREAMS_NS)
    assert to is not None, "no ToolOffsetTable in stream"
    entry = to.find("{%s}Entry" % STREAMS_NS)
    assert entry.get("key") == "T3"
    zc = [c for c in entry.findall("{%s}Cell" % STREAMS_NS) if c.get("key") == "Z"][0]
    assert zc.text == "2.5", zc.text
    # xyrotation has no standard MTConnect type, so it stays a LinuxCNC extension.
    rot = cur.find(".//{%s}CoordinateRotation" % EXT_NS)
    assert rot is not None and rot.text == "30.0", rot

    probe = ET.fromstring(probe_xml(model, config))
    types = {d.get("id"): d.get("type") for d in probe.iter("{%s}DataItem" % MTC_NS)}
    assert types["tooloffset"] == "TOOL_OFFSET", types.get("tooloffset")   # standard now
    assert types["xyrotation"] == "x:COORDINATE_ROTATION", types.get("xyrotation")
    print("ok  tool offset (standard ToolOffsetTable) + XY rotation (x: extension)")


def test_assets():
    _, _, config = load(CONFIGS["3axis"])
    tools = [
        ToolAsset(tool_no=1, pocket=1, in_spindle=True, diameter=6.35, length_z=50.0,
                  comment="1/4 flat endmill  "),  # trailing space should be trimmed
        ToolAsset(tool_no=2, pocket=5, in_spindle=False, diameter=3.0),
    ]
    root = ET.fromstring(assets_xml(tools, config, TS))
    ns = "{urn:mtconnect.org:MTConnectAssets:1.7}"
    cts = root.findall(".//%sCuttingTool" % ns)
    assert len(cts) == 2, len(cts)
    assert cts[0].get("assetId") == "tool-1"
    assert cts[0].get("serialNumber") == "tool-1", cts[0].attrib  # required attr
    loc = cts[0].find(".//%sLocation" % ns)
    assert loc.get("type") == "SPINDLE" and loc.text == "1", loc.attrib
    desc = cts[0].find("%sDescription" % ns)
    assert desc is not None and desc.text == "1/4 flat endmill", repr(desc.text)
    assert cts[1].find("%sDescription" % ns) is None  # empty comment -> no element
    # length offset is FunctionalLength (LF), not BodyLengthMax
    lf = cts[0].find(".//%sFunctionalLength" % ns)
    assert lf is not None and lf.get("code") == "LF" and lf.text == "50", ET.tostring(cts[0])
    assert cts[0].find(".//%sBodyLengthMax" % ns) is None
    # diameter is a CuttingItem measurement, not a tool-level one
    dia = cts[0].find(".//%sCuttingItems/%sCuttingItem/%sMeasurements/%sCuttingDiameter"
                      % (ns, ns, ns, ns))
    assert dia is not None and dia.text == "6.35" and dia.get("code") == "DC", ET.tostring(cts[0])
    print("ok  assets (serialNumber + FunctionalLength + CuttingItem diameter + comment)")


def test_probe_standard_blocks():
    # spindle Specifications, Coolant system, and the kinematics extension living
    # inside the (schema-valid) Description host.
    _, model, config = load(_cfg("example.ini"))
    root = ET.fromstring(probe_xml(model, config, creation_time=TS))
    ns = "{%s}" % MTC_NS
    spec = root.find(".//%sRotary[@id='spindle']//%sSpecification" % (ns, ns))
    assert spec is not None and spec.get("type") == "ROTARY_VELOCITY", "no spindle spec"
    assert spec.find("%sMaximum" % ns).text == "6000"
    cool = root.find(".//%sCoolant" % ns)
    assert cool is not None, "no Coolant component"
    assert cool.find(".//%sDataItem[@id='coolant_flood']" % ns).get("type") == "x:FLOOD"
    # x:Kinematics is nested in a Description (the lax xs:any extension point)
    kin_ext = root.find(".//%sDescription/{%s}Kinematics" % (ns, EXT_NS))
    assert kin_ext is not None, "x:Kinematics not under Description"
    assert kin_ext.get("nativeLinearUnits") == "INCH"
    # Agent device is present (schema-required alongside the machine Device)
    assert root.find(".//%sAgent" % ns) is not None, "no Agent device"
    print("ok  probe standard blocks (spindle Spec, Coolant, kinematics host, Agent)")


def test_spindle_constraints():
    ns = "{%s}" % MTC_NS

    def spdl_cmd(path):
        _, model, config = load(path)
        root = ET.fromstring(probe_xml(model, config, creation_time=TS))
        for di in root.iter("%sDataItem" % ns):
            if di.get("id") == "spdl_speed_cmd":
                return di
        raise AssertionError("spdl_speed_cmd missing")

    # example.ini declares [SPINDLE_0] MIN/MAX_FORWARD_VELOCITY -> Constraints
    di = spdl_cmd(_cfg("example.ini"))
    con = di.find("%sConstraints" % ns)
    assert con is not None, "no Constraints on spdl_speed_cmd"
    assert con.find("%sMinimum" % ns).text == "100", ET.tostring(di)
    assert con.find("%sMaximum" % ns).text == "6000", ET.tostring(di)

    # a config without spindle velocity limits emits no Constraints (no bogus
    # ~2.1e9 default range fabricated).
    assert spdl_cmd(CONFIGS["3axis"]).find("%sConstraints" % ns) is None
    print("ok  spindle speed range Constraints (from [SPINDLE_0], omitted when absent)")


def test_schema_validation():
    # Gated: needs the xmlschema package (XSD 1.1) and the official 1.7 XSDs
    # (local dir via $MTC_XSD_DIR, else fetched).  Skips cleanly otherwise so
    # the core suite stays dependency- and network-free.
    import os
    try:
        import io
        import xmlschema
    except ImportError:
        print("skip schema validation (xmlschema not installed)")
        return
    from mtc.agent import asset_dir
    xsd_dir = os.environ.get("MTC_XSD_DIR")
    base = (lambda n: os.path.join(xsd_dir, n)) if xsd_dir else \
        (lambda n: "http://schemas.mtconnect.org/schemas/" + n)
    ext = os.path.join(asset_dir(), "mtconnect-linuxcnc-1.xsd")
    state = AgentState(_cfg("example.ini"))
    state.poll_once()
    checks = [("MTConnectDevices_1.7.xsd", state.probe_document()),
              ("MTConnectAssets_1.7.xsd", state.assets_document()),
              (ext, state.current_document())]
    try:
        for xsd, doc in checks:
            loc = xsd if xsd == ext else base(xsd)
            schema = xmlschema.XMLSchema11(loc, validation="skip")
            errs = list(schema.iter_errors(io.StringIO(doc)))
            assert not errs, "%s: %s" % (xsd, (errs[0].reason or errs[0].message))
    except (OSError, xmlschema.XMLSchemaException) as exc:
        if xsd_dir:
            raise
        print("skip schema validation (schemas unreachable: %s)" % exc)
        return
    print("ok  schema validation (probe/assets base, current base+extension)")


def test_source_offline():
    _, model, config = load(CONFIGS["3axis"])
    src = LcncSource(model, config)
    # No linuxcnc extension / no running instance -> graceful empty results.
    if not src.available():
        assert src.sample_values() == {}
        assert src.tool_assets() == []
        print("ok  lcnc_source offline (graceful, no stat)")
    else:
        print("ok  lcnc_source live (stat available)")


def test_http_endpoints():
    state = AgentState(CONFIGS["3axis"])
    state.buffer.ingest({"execution": "ACTIVE", "pos_x": 3.5}, TS)
    state._assets = [ToolAsset(tool_no=7, pocket=7, in_spindle=True, diameter=10.0)]

    http = HttpAgent(state, host="127.0.0.1", port=0)
    http.start()
    try:
        base = "http://127.0.0.1:%d" % http.port

        def get(path):
            with urllib.request.urlopen(base + path, timeout=5) as r:
                return r.status, r.read().decode()

        st, body = get("/probe")
        assert st == 200 and "MTConnectDevices" in body, st
        st, body = get("/current")
        assert st == 200 and "ACTIVE" in body, body[:200]
        st, body = get("/sample?from=1&count=10")
        assert st == 200 and 'sequence="1"' in body, body[:200]
        st, body = get("/assets")
        assert st == 200 and "tool-7" in body, body[:200]

        # Bad route and out-of-range sequence return MTConnectError docs.
        try:
            get("/nope")
            assert False, "expected 404"
        except urllib.error.HTTPError as e:
            assert e.code == 404 and "MTConnectError" in e.read().decode()
        try:
            get("/sample?from=999999")
            assert False, "expected 406"
        except urllib.error.HTTPError as e:
            assert e.code == 406 and "OUT_OF_RANGE" in e.read().decode()
        print("ok  embedded HTTP agent (probe/current/sample/assets + errors)")
    finally:
        http.stop()


def test_user_pages():
    state = AgentState(CONFIGS["3axis"])
    root = tempfile.mkdtemp()
    pages = os.path.join(root, "pages")
    evil = os.path.join(root, "pages_evil")  # sibling sharing the name prefix
    os.mkdir(pages)
    os.mkdir(evil)
    with open(os.path.join(pages, "twin.html"), "w") as fh:
        fh.write("<html><body>twin</body></html>")
    with open(os.path.join(pages, "app.js"), "w") as fh:
        fh.write("console.log(1)")
    with open(os.path.join(evil, "e.html"), "w") as fh:
        fh.write("outside")
    state.user_pages = os.path.realpath(pages)

    http = HttpAgent(state, host="127.0.0.1", port=0)
    http.start()
    try:
        base = "http://127.0.0.1:%d" % http.port

        def get(path):
            with urllib.request.urlopen(base + path, timeout=5) as r:
                return r.status, r.read(), r.headers.get("Content-Type")

        st, body, ctype = get("/html/twin.html")
        assert st == 200 and b"twin" in body and "text/html" in ctype, (st, ctype)
        st, body, ctype = get("/html/app.js")
        assert st == 200 and "javascript" in ctype, ctype

        # Missing file, and escapes outside USER_PAGES: a ".." traversal and
        # the sibling-prefix bypass (pages vs pages_evil) must both 404.
        for path in ("/html/nope.html", "/html/../pages_evil/e.html"):
            try:
                get(path)
                assert False, "expected 404 for " + path
            except urllib.error.HTTPError as e:
                assert e.code == 404, (path, e.code)
        print("ok  user pages (serve + content type + traversal rejected)")
    finally:
        http.stop()


def _is_iso_ts(field):
    # e.g. 2026-07-28T12:00:00.000Z -- cheap structural check (no regex import).
    return (len(field) >= 20 and field[4] == "-" and field[7] == "-"
            and field[10] == "T" and field.endswith("Z"))


def test_shdr():
    # Offline SHDR adapter test: no LinuxCNC, values injected via _last_values
    # (what AgentState.latest_values() returns), which is the poll snapshot the
    # adapter diffs and streams.
    state = AgentState(CONFIGS["3axis"])

    shdr = ShdrAgent(state, port=0, host="127.0.0.1")
    shdr.start()
    client = None
    try:
        client = socket.create_connection(("127.0.0.1", shdr.port), timeout=2)
        client.settimeout(2)

        # Wait until the server has registered the client (its handler thread ran
        # _add_client) so the broadcast below is guaranteed to reach us.
        deadline = time.time() + 2
        while time.time() < deadline:
            with shdr._lock:
                registered = len(shdr._clients)
            if registered:
                break
            time.sleep(0.01)
        assert registered == 1, registered

        buf = [b""]

        def recv_line():
            while b"\n" not in buf[0]:
                chunk = client.recv(4096)
                if not chunk:
                    raise AssertionError("connection closed before a full line")
                buf[0] += chunk
            line, _, rest = buf[0].partition(b"\n")
            buf[0] = rest
            return line.decode()

        # (a) ingest/poll some values, then stream the changes.  A structured
        # (TABLE) value must be skipped, never emitted as a malformed line.
        state._last_values = {"execution": "ACTIVE", "pos_x": 3.5,
                              "workoffset": {"G54": {"X": 1.0}}}
        shdr.publish_changes()

        line = recv_line()
        fields = line.split("|")
        assert len(fields) >= 3, "not a well-formed SHDR line: %r" % line
        assert _is_iso_ts(fields[0]), "first field not ISO ts: %r" % fields[0]
        ids = fields[1::2]
        assert "execution" in ids and "pos_x" in ids, ids
        assert "workoffset" not in line, "TABLE value must be skipped: %r" % line

        # unchanged values produce nothing; a real change is streamed
        shdr.publish_changes()
        state._last_values = {"execution": "READY", "pos_x": 3.5,
                              "workoffset": {"G54": {"X": 1.0}}}
        shdr.publish_changes()
        line2 = recv_line()
        f2 = line2.split("|")
        assert _is_iso_ts(f2[0]) and "execution" in f2[1::2], line2
        assert "READY" in line2, line2

        # (b) heartbeat: '* PING' -> '* PONG 10000'
        client.sendall(b"* PING\n")
        pong = recv_line()
        assert pong == "* PONG 10000", repr(pong)

        print("ok  SHDR adapter (initial/changed ts|id|value lines, TABLE skipped, PING/PONG)")
    finally:
        if client is not None:
            client.close()
        shdr.stop()


def test_hal_items():
    import tempfile
    from mtc.hal_items import HalSource
    ini_text = (
        "[EMC]\nMACHINE = hal-item-demo\n"
        "[TRAJ]\nCOORDINATES = X Y Z\nLINEAR_UNITS = inch\n"
        "[KINS]\nKINEMATICS = trivkins\nJOINTS = 3\n"
        "[AXIS_X]\nMIN_LIMIT = -10\nMAX_LIMIT = 10\n"
        "[AXIS_Y]\nMIN_LIMIT = -6\nMAX_LIMIT = 6\n"
        "[AXIS_Z]\nMIN_LIMIT = -8\nMAX_LIMIT = 0\n"
        "[MTCONNECT]\n"
        "HAL_ITEM = pin=spindle.0.load, id=spindle_load, type=LOAD, units=PERCENT, component=spindle\n"
        "HAL_ITEM = pin=hm2.temp, id=board_temp, type=TEMPERATURE, units=CELSIUS\n"
        "HAL_ITEM = pin=bad.one, id=bad_custom, type=WIDGET_COUNT\n"   # rejected
        "HAL_ITEM = pin=x, id=1nope, type=LOAD\n"                     # bad id, rejected
    )
    d = tempfile.mkdtemp()
    p = os.path.join(d, "hal.ini")
    with open(p, "w") as fh:
        fh.write(ini_text)

    ini = IniReader(p)
    model = kin.build_model(ini)
    config = DeviceConfig.from_ini(ini)
    # only the two valid items survive parsing
    assert [i.id for i in config.hal_items] == ["spindle_load", "board_temp"], \
        [i.id for i in config.hal_items]

    # they appear in the registry as SAMPLE items of the right type
    reg = {d.id: d for d in build_dataitems(model, config)}
    assert reg["spindle_load"].type == "LOAD" and reg["spindle_load"].category == "SAMPLE"
    assert reg["spindle_load"].comp_id == "spindle"          # targeted the spindle
    assert reg["board_temp"].comp_id == "sensors"            # default Sensor host

    # probe: generic item under Auxiliaries/Sensor, targeted item under the spindle
    probe = ET.fromstring(probe_xml(model, config))
    ns = "{%s}" % MTC_NS
    sensor = probe.find(".//%sAuxiliaries/%sComponents/%sSensor" % (ns, ns, ns))
    assert sensor is not None, "no Auxiliaries/Sensor"
    sensor_ids = {di.get("id") for di in sensor.iter("%sDataItem" % ns)}
    assert "board_temp" in sensor_ids and "spindle_load" not in sensor_ids, sensor_ids
    sp = probe.find(".//%sRotary[@id='spindle']" % ns)
    assert "spindle_load" in {di.get("id") for di in sp.iter("%sDataItem" % ns)}

    # offline HalSource (no hal extension) yields nothing, gracefully
    assert HalSource(config.hal_items).sample_values() == {}
    print("ok  HAL items (parse/validate, placement, offline HalSource)")


def test_solid_models():
    import os
    import tempfile
    from mtc.models import MachineModels, MeshRef

    _, model, config = load(CONFIGS["3axis"])

    # No models configured -> no SolidModel / Configuration emitted.
    plain = ET.fromstring(probe_xml(model, config))
    assert plain.find(".//{%s}SolidModel" % MTC_NS) is None

    tmp = tempfile.mkdtemp()
    path = os.path.join(tmp, "z_head.stl")
    with open(path, "wb") as fh:
        fh.write(b"solid dummy\nendsolid dummy\n")
    ref = MeshRef("z_head.stl", path, "STL", "model/stl", True)
    mm = MachineModels(units="MILLIMETER", base=ref, axis={"Z": ref},
                       chain=["dev_base", "axis_x", "axis_y", "axis_z"],
                       parents={"axis_x": "dev_base", "axis_y": "axis_x",
                                "axis_z": "axis_y"},
                       served={"z_head.stl": ref})

    probe = ET.fromstring(probe_xml(model, config, models=mm))
    sms = probe.findall(".//{%s}SolidModel" % MTC_NS)
    hrefs = {sm.get("href") for sm in sms}
    assert "/models/z_head.stl" in hrefs, hrefs
    assert probe.find(".//{%s}CoordinateSystem[@type='MACHINE']" % MTC_NS) is not None
    # serial chain: Z link hangs off Y's motion
    mz = [m for m in probe.iter("{%s}Motion" % MTC_NS) if m.get("id") == "motion_z"][0]
    assert mz.get("parentIdRef") == "motion_y", mz.attrib

    # branched: re-root Z at the base (knee-mill quill) -> no parentIdRef
    mm.parents["axis_z"] = "dev_base"
    probe2 = ET.fromstring(probe_xml(model, config, models=mm))
    mz2 = [m for m in probe2.iter("{%s}Motion" % MTC_NS) if m.get("id") == "motion_z"][0]
    assert mz2.get("parentIdRef") is None, mz2.attrib

    # inverted axis: Motion <Axis> vector is negated (work-carrying link)
    mm.invert = {"Z"}
    probe3 = ET.fromstring(probe_xml(model, config, models=mm))
    mz3 = [m for m in probe3.iter("{%s}Motion" % MTC_NS) if m.get("id") == "motion_z"][0]
    assert mz3.find("{%s}Axis" % MTC_NS).text == "0 0 -1", mz3.find("{%s}Axis" % MTC_NS).text

    # /models route serves the file; unknown model -> 404
    state = AgentState(CONFIGS["3axis"])
    state.models = mm
    http = HttpAgent(state, host="127.0.0.1", port=0)
    http.start()
    try:
        with urllib.request.urlopen(
                "http://127.0.0.1:%d/models/z_head.stl" % http.port, timeout=5) as r:
            assert r.status == 200 and b"endsolid" in r.read()
        try:
            urllib.request.urlopen("http://127.0.0.1:%d/models/nope.stl" % http.port)
            assert False, "expected 404"
        except urllib.error.HTTPError as e:
            assert e.code == 404
        print("ok  solid models (SolidModel/CoordinateSystems/chain + /models route)")
    finally:
        http.stop()


def test_auto_geometry():
    import os
    import tempfile
    from mtc.models import build_models

    ini_text = (
        "[EMC]\nMACHINE = auto-demo\n"
        "[TRAJ]\nCOORDINATES = X Y Z\nLINEAR_UNITS = inch\n"
        "[KINS]\nKINEMATICS = trivkins\nJOINTS = 3\n"
        "[AXIS_X]\nMIN_LIMIT = -10\nMAX_LIMIT = 10\n"
        "[AXIS_Y]\nMIN_LIMIT = -6\nMAX_LIMIT = 6\n"
        "[AXIS_Z]\nMIN_LIMIT = -8\nMAX_LIMIT = 0\n"
        "[MTCONNECT]\nMODEL_AUTO = 1\nMODEL_PARENT_Z = BASE\nMODEL_INVERT = X Y\n"
    )
    d = tempfile.mkdtemp()
    p = os.path.join(d, "auto.ini")
    with open(p, "w") as fh:
        fh.write(ini_text)

    ini = IniReader(p)
    model = kin.build_model(ini)
    config = DeviceConfig.from_ini(ini)
    mm = build_models(ini, model, config)
    assert mm.enabled() and mm.base is not None
    assert set(mm.axis) == {"X", "Y", "Z"}, set(mm.axis)
    # Geometry is served in the MTConnect canonical unit (millimetre); the inch
    # travel limits are scaled up by 25.4 in the generated boxes.
    assert mm.units == "MILLIMETER", mm.units
    assert all(v.startswith("solid") for v in mm.generated.values())
    # X box half-width tracks the 20" span * 25.4 -> ~500 mm scale, not ~20.
    xs = [float(tok) for line in mm.generated["axis_x.stl"].splitlines()
          if line.strip().startswith("vertex") for tok in [line.split()[1]]]
    assert max(xs) > 100, max(xs)  # would be < 20 if still in inches

    probe = ET.fromstring(probe_xml(model, config, models=mm))
    hrefs = {sm.get("href") for sm in probe.findall(".//{%s}SolidModel" % MTC_NS)}
    assert {"/models/dev_base.stl", "/models/axis_x.stl", "/models/axis_z.stl"} <= hrefs, hrefs

    # the agent serves the generated STL bytes (no file on disk)
    state = AgentState(p)
    http = HttpAgent(state, host="127.0.0.1", port=0)
    http.start()
    try:
        with urllib.request.urlopen(
                "http://127.0.0.1:%d/models/axis_x.stl" % http.port, timeout=5) as r:
            assert r.status == 200 and b"endsolid" in r.read()
        print("ok  auto geometry (generated boxes served + probe SolidModels)")
    finally:
        http.stop()


def test_ha_discovery():
    # ha.py is an optional contrib (Home Assistant is not part of the core agent),
    # so skip cleanly if the contrib is not present.
    import json
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "contrib"))
    try:
        import ha
    except ImportError:
        print("skip HA discovery (contrib not installed)")
        return

    _, model, config = load(CONFIGS["3axis"])
    sensors = ha.build_sensors(model, config)
    keys = {s["key"] for s in sensors}
    assert {"execution", "spdl_speed", "pos_x", "pos_y", "pos_z"} <= keys, keys

    px = [s for s in sensors if s["key"] == "pos_x"][0]
    d = ha.discovery_payload(px, config, "st/topic", "av/topic")
    assert d["state_topic"] == "st/topic"
    assert d["value_template"] == "{{ value_json.pos_x }}"
    assert d["unit_of_measurement"] == "mm"          # canonical (values are mm)
    assert d["device"]["identifiers"] == [config.uuid]
    assert d["unique_id"] == "%s_pos_x" % config.uuid

    state = json.loads(ha.state_json(
        {"execution": "ACTIVE", "pos_x": 1.5, "pos_z": "UNAVAILABLE",
         "workoffset": {"G54": {}}}, sensors))
    assert state["execution"] == "ACTIVE" and state["pos_x"] == 1.5
    assert "pos_z" not in state          # UNAVAILABLE skipped
    assert "workoffset" not in state     # structured value skipped
    print("ok  HA discovery (sensors + discovery payload + state JSON)")


def test_entry_dump_probe():
    # The installed entry point (bin/mtconnect-agent, on PATH via rip-environment).
    exe = os.path.join(_EMC2_HOME, "bin", "mtconnect-agent") if _EMC2_HOME \
        else "mtconnect-agent"
    if not (os.path.isfile(exe) or _EMC2_HOME is None):
        print("skip entry --dump-probe (mtconnect-agent not built/installed)")
        return
    out = subprocess.check_output([exe, "--dump-probe", CONFIGS["5axis"]], text=True)
    assert "xyzac-trt-kins" in out and "MTConnectDevices" in out
    print("ok  entry --dump-probe")


def main():
    test_kinematics()
    test_probe_registry_consistency()
    test_buffer_and_streams()
    test_work_offset_table()
    test_tool_offset_and_rotation()
    test_assets()
    test_probe_standard_blocks()
    test_spindle_constraints()
    test_schema_validation()
    test_source_offline()
    test_http_endpoints()
    test_user_pages()
    test_shdr()
    test_hal_items()
    test_solid_models()
    test_auto_geometry()
    test_ha_discovery()
    test_entry_dump_probe()
    print("\nALL TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
