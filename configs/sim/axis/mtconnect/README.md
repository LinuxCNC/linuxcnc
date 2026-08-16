# MTConnect agent for LinuxCNC — demo config

First-class MTConnect support for LinuxCNC: a userspace, non-realtime agent that
exposes machine **status**, a rich **kinematic description**, and **tool data**
over MTConnect. It ships an embedded HTTP agent (no external dependency) and can
optionally publish over the standard MTConnect **MQTT** binding. The `/probe`
response carries enough kinematic detail for an external tool (e.g. a FreeCAD
Path plugin) to auto-configure a machine.

This directory is a demo sim config. The agent itself installs with LinuxCNC
(`bin/mtconnect-agent`, package `lib/python/mtc`, assets under
`share/linuxcnc/mtconnect`). For the full reference see the `mtconnect-agent(1)`
man page and the `[MTCONNECT]` section of the INI configuration chapter.

## Quick start

```sh
# From this directory:
linuxcnc example.ini            # launches the sim + the MTConnect agent

# In another terminal:
curl http://localhost:5000/probe     # MTConnectDevices (structure + kinematics)
curl http://localhost:5000/current   # latest value of every DataItem
curl http://localhost:5000/sample?from=1&count=100
curl http://localhost:5000/assets    # CuttingTool assets (tool table)
```

You can inspect the generated device model without launching LinuxCNC (after
sourcing `scripts/rip-environment` in a RIP build, or with LinuxCNC installed):

```sh
mtconnect-agent --dump-probe example.ini
mtconnect-agent --dump-probe ../vismach/5axis/table-rotary-tilting/xyzac-trt.ini
```

## Enabling it in your own config

Add a few lines to your INI. The device model is generated automatically from
`[TRAJ]`, `[KINS]`, `[AXIS_*]` and `[JOINT_*]`.

```ini
[MTCONNECT]
ENABLE      = 1
DEVICE_NAME = my_mill
UUID        = linuxcnc-my-mill-0001
HTTP_PORT   = 5000
# HTTP_BIND defaults to 127.0.0.1 (loopback); set 0.0.0.0 to expose on the LAN.
# HTTP_BIND = 127.0.0.1
# TRANSPORT: comma list of http, mqtt, shdr (no inline comments in INI values)
TRANSPORT   = http
SAMPLE_HZ   = 10
# MQTT_BROKER = localhost
# MQTT_PORT   = 1883
# MQTT_PREFIX = MTConnect
```

Load the agent from a HAL file (see `mtconnect.hal`):

```
loadusr -W mtconnect-agent
```

`-W` waits until the component is ready; the INI comes from `$INI_FILE_NAME`.
Loading it in the HAL file (rather than `[APPLICATIONS]`) makes the status pins
(`active`, `connected`) available for linking.

## What is exposed

| Endpoint   | Content |
|------------|---------|
| `/probe`   | Device structure: Controller/Path, Axes (Linear/Rotary + Motion), spindle, and the `x:Kinematics` extension |
| `/current` | Latest value of every DataItem (execution, mode, positions, spindle, feed, tool) |
| `/sample`  | Sequence-numbered observation history (`?from=&count=`) |
| `/assets`  | Tool table as `CuttingTool` assets (location, diameter, length offsets) |

### LinuxCNC → MTConnect mapping (`linuxcnc.stat()`)

| MTConnect DataItem | Source |
|---|---|
| `EMERGENCY_STOP` | `task_state` |
| `CONTROLLER_MODE` | `task_mode` |
| `EXECUTION` | `interp_state`, `task_paused` |
| `PROGRAM`, `LINE_NUMBER` | `file`, `motion_line` (executing line, not interp read-ahead) |
| `PATH_FEEDRATE` (+ OVERRIDE) | `current_vel`, `feedrate` |
| `POSITION` / `ANGLE` (ACTUAL/COMMANDED) | `actual_position`, `position` |
| `ROTARY_VELOCITY`, `ROTARY_MODE`, `DIRECTION` | `spindle[0]` |
| `TOOL_NUMBER`, `TOOL_ASSET_ID` | `tool_in_spindle` |
| `ASSET_CHANGED` | tool change detection |
| `CuttingTool` assets | `tool_table` |

## The kinematic description (`x:Kinematics`)

Standard MTConnect models axes as `Linear`/`Rotary` components with `Motion`
elements (`PRISMATIC`/`REVOLUTE`, direction vector). LinuxCNC specifics that do
not map cleanly are carried in a versioned extension namespace
`urn:linuxcnc:mtconnect:1` — the primary contract for auto-configuration:

```xml
<x:Kinematics module="xyzac-trt-kins" coordinates="XYZAC" joints="5"
              params="sparm=identityfirst">
  <x:JointMap>
    <x:Joint number="0" kind="LINEAR"  axis="X" min="-200" max="200" home="0"/>
    <x:Joint number="3" kind="ANGULAR" axis="A" min="-100" max="50"  home="0"/>
    <x:Joint number="4" kind="ANGULAR" axis="C" min="-36000" max="36000" home="0"/>
  </x:JointMap>
  <x:Axis name="X" kind="LINEAR"  vector="1 0 0" min="-200" max="200"/>
  <x:Axis name="A" kind="ANGULAR" vector="1 0 0" min="-100" max="50"/>
  <x:Axis name="C" kind="ANGULAR" vector="0 0 1" min="-36000" max="36000"/>
</x:Kinematics>
```

A FreeCAD plugin reads the standard `Axes`/`Motion` tree, or the compact
`x:Kinematics` block, to create/configure a machine: axis list and type,
travel limits, home positions, kinematics module/type, and the joint↔axis map.

## Solid models (geometry)

A standard MTConnect twin viewer (e.g. the one at demo.mtconnect.org, or
TrakHound) renders a machine from `/probe` alone: geometry is **referenced, never
streamed**. The device model carries `<SolidModel href="...">` elements pointing
to external mesh files, plus `<CoordinateSystems>` and a `<Motion>` chain; the
viewer fetches each mesh once (from the agent's `/models/<name>`) and animates it
using the streamed positions. LinuxCNC bundles no viewer and no JavaScript — this
is plain, standards-compliant MTConnect.

### Zero-config geometry (`MODEL_AUTO`)

```ini
[MTCONNECT]
MODEL_AUTO = 1
```

The agent generates simple placeholder **box** meshes for the base and each axis
straight from the travel limits and serves them from memory — any machine gets a
functional twin with **no mesh files**. This is what the example config uses.

### Real geometry (per-link meshes)

Supply your own meshes (STL / OBJ / glTF) for fidelity:

```ini
[MTCONNECT]
MODEL_DIR     = models        ; base dir for relative paths (default: INI dir)
MODEL_BASE    = frame.stl     ; static frame / column (device-level SolidModel)
MODEL_X       = x_table.stl   ; link that moves with X
MODEL_Y       = y_saddle.stl
MODEL_Z       = spindle.stl
MODEL_SPINDLE = spindle.stl
```

Author each mesh in **millimetres** (MTConnect's canonical unit — the SolidModel
element carries no per-mesh unit) in the machine frame at the all-axes-zero pose.
An explicit
`MODEL_<axis>` overrides the generated box for that link, so files and
`MODEL_AUTO` can be mixed.

### Topology and direction (both modes)

These describe the mechanics and are **not** derivable from a trivkins INI:

```ini
MODEL_CHAIN    = Y X Z    ; nesting order of the moving links (base -> tip)
MODEL_PARENT_Z = BASE     ; branch: Z (quill/tool) hangs off the base, not X/Y
MODEL_INVERT   = X Y      ; work-carrying links move opposite the reported coord
```

The agent then serves each mesh at `GET /models/<name>`, emits a device-level
`<SolidModel>` for the base and a per-axis `<SolidModel>` in each axis's
`<Configuration>`, and emits `<CoordinateSystems>` (WORLD → MACHINE) plus a
`<Motion>` chain (`parentIdRef`) so the twin nests transforms correctly.

### Viewing the twin

The agent serves the geometry but **bundles no viewer** — rendering is left to a
standard MTConnect client, so no third-party JavaScript lives in LinuxCNC. Two
paths:

- **A standard MTConnect twin viewer (recommended).** Point one at the agent and
  it renders the machine from `/probe` + `/models/` with zero extra setup: the
  demo viewer at <https://demo.mtconnect.org>, TrakHound, or cppagent's demo
  twin. Expose the agent first with `[MTCONNECT]HTTP_BIND = 0.0.0.0` (it defaults
  to loopback only) and give the viewer `http://<machine>:5000`.

- **Self-host a small viewer.** An earlier revision of this config shipped a
  ~12 KB three.js viewer (`twin.html`) that was removed from the tree to keep
  third-party JS out of core. It still works against this agent — it fetches
  `/probe` and `/current` and loads the `/models/` meshes. To run it yourself,
  recover it from git history
  (`git show <commit>:share/linuxcnc/mtconnect/twin.html`), repoint its three.js
  import map at a CDN or a local `libjs-three`, and serve it same-origin as the
  agent (e.g. behind a small reverse proxy, since the relative `fetch` calls
  assume one origin).

## Using the official cppagent instead of the embedded agent

Put `shdr` in `TRANSPORT` and the agent acts as an SHDR adapter (default port
7878), streaming `<ts>|id|value` lines to an external cppagent. Configure that
cppagent with a `Devices.xml`, which the generated `/probe` document doubles as:

```sh
mtconnect-agent --dump-probe example.ini > Devices.xml
```

The `dataItemId`s in the SHDR stream match the ids in that `Devices.xml`.

## MQTT

With `mqtt` in `TRANSPORT` and `python3-paho-mqtt` installed, the agent publishes
the standard (vendor-neutral) MTConnect MQTT topics:
`<prefix>/Probe/<uuid>` (retained), `<prefix>/Current/<uuid>`,
`<prefix>/Sample/<uuid>`, `<prefix>/Asset/<uuid>/<assetId>`. A consumer discovers
the whole device from the retained Probe topic.

**Home Assistant** is not part of the core agent (its MQTT Discovery format is
HA-specific). An optional bridge, `contrib/mtconnect-ha-bridge`, reuses the
device model to publish HA discovery; configure it with arguments so no HA key
touches the machine INI:

```
loadusr -W mtconnect-ha-bridge --broker=[HA]BROKER --username=[HA]USER --password=[HA]PASSWORD
```

## Layout (installed)

```
src/hal/user_comps/mtconnect-agent.py   entry point -> bin/mtconnect-agent
lib/python/mtc/                          the agent package:
  ini_reader.py    INI access (linuxcnc.ini, with offline fallback)
  kinematics.py    build the kinematic model from the INI
  observations.py  shared DataItem registry (keeps probe and streams in sync)
  device_model.py  /probe MTConnectDevices document (+ CLI --dump-probe)
  lcnc_source.py   live status + tool table from linuxcnc.stat()
  models.py        solid-model config + served mesh registry (SolidModel geometry)
  streams.py       /current, /sample buffer + XML; /assets XML
  agent.py         transport-agnostic agent core (document builders)
  http_agent.py    embedded HTTP server
  mqtt_agent.py    optional MQTT publisher (vendor-neutral MTConnect binding)
share/linuxcnc/mtconnect/mtconnect-linuxcnc-1.xsd  extension schema
configs/sim/axis/mtconnect/                         this demo config + tests
  contrib/mtconnect-ha-bridge   opt-in Home Assistant MQTT-discovery bridge
  contrib/ha.py                 HA discovery helper (used by the bridge)
```

## Testing

```sh
. scripts/rip-environment      # so mtc is importable and bin/ is on PATH
python3 test_mtc.py            # offline suite (no running LinuxCNC required)
python3 validate_schemas.py    # validate the documents against the MTConnect XSDs
                               #   (needs the 'xmlschema' package)
```

`test_mtc.py` covers kinematics mapping, probe↔registry consistency (3- and
5-axis), the observation buffer + streams, assets, the live HTTP endpoints, and
(when `xmlschema` is available) schema validation of all four documents.

## Custom HAL pins

Expose any HAL pin/signal as a data item with `HAL_ITEM` lines (read via
`hal.get_value`, no HAL wiring):

```ini
[MTCONNECT]
HAL_ITEM = pin=spindle.0.load, id=spindle_load, type=LOAD, units=PERCENT, component=spindle
HAL_ITEM = pin=hm2_5i25.temp,  id=board_temp,   type=TEMPERATURE, units=CELSIUS
```

Standard MTConnect SAMPLE types only (LOAD, TEMPERATURE, PRESSURE, VOLTAGE,
AMPERAGE, FREQUENCY, ANGLE, VELOCITY, TORQUE, …). `component=` hosts it under a
generic `Sensor` (default) or an existing component (spindle/controller/path/an
axis). Unsupported types are skipped with a warning. See `mtconnect-agent(1)`.

## Status

Complete and packaged for the build: the HTTP transport, HAL component and
schema-valid documents; the vendor-neutral MQTT binding plus the optional
Home Assistant bridge contrib; `SolidModel` geometry under `/models/` for any
standard MTConnect twin viewer; and the SHDR adapter for an external cppagent.
Realtime servo-rate data is explicitly out of scope.
