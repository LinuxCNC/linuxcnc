"""Generate the F2 component configuration (key = value) from turret.json.

The compiled userspace component (`src/hal/user_comps/turret.comp`) cannot
parse JSON, so the configuration is exported to a simple `key = value`
file that it can read (and re-read on `turret.reload`).
"""

import argparse
import os
import sys

from .config import MAX_MAINT_CODES, TurretConfig

PIN_KEYS = (
    "pos1", "strobe", "changepos", "clamped", "motor", "unclamp", "clamp",
    "interlock", "watchdog_pet",
    "tool_prepare", "tool_prepared", "tool_change", "tool_changed",
    "tool_prep_pocket", "tool_prep_number",
    "toolchanger_fault", "toolchanger_reason",
)


def generate(cfg):
    """Return the key = value configuration text for a TurretConfig."""
    lines = ["# turret configuration (generated from turret.json)"]
    lines.append("stations = %d" % cfg.stations)
    for key in PIN_KEYS:
        value = cfg.pins.get(key)
        lines.append("pins.%s = %s" % (key, value if value else "none"))
    t = cfg.timing
    lines.append("timing.strobe_filter_ms = %g" % t.strobe_filter_ms)
    lines.append("timing.unclamp_timeout = %g" % t.unclamp_timeout)
    lines.append("timing.home_timeout = %g" % t.home_timeout)
    lines.append("timing.rotate_timeout = %g" % t.rotate_timeout)
    lines.append("timing.settle_time = %g" % t.settle_time)
    lines.append("timing.locate_timeout = %g" % t.locate_timeout)
    lines.append("timing.locate_stable_ms = %g" % t.locate_stable_ms)
    lines.append("timing.clamp_timeout = %g" % t.clamp_timeout)
    lines.append("timing.max_retries = %d" % t.max_retries)
    lines.append("timing.lead_pulses = %d" % t.lead_pulses)
    lines.append("timing.lead_auto = %d" % (1 if t.lead_auto else 0))
    lines.append("timing.jog_timeout = %g" % t.jog_timeout)
    logic = cfg.logic
    lines.append("logic.clamped_inverted = %d" % (1 if logic.clamped_inverted else 0))
    lines.append("logic.require_changepos = %d" % (1 if logic.require_changepos else 0))
    lines.append("logic.unclamp_before_rotate = %d" % (1 if logic.unclamp_before_rotate else 0))
    lines.append("logic.interlock_required = %d" % (1 if logic.interlock_required else 0))
    lines.append("logic.abort_on_fault = %d" % (1 if logic.abort_on_fault else 0))
    lines.append("logic.test_enable_required = %d" % (1 if logic.test_enable_required else 0))
    for item in cfg.maintenance:
        if item.code < 1 or item.code > MAX_MAINT_CODES:
            continue
        name = item.name.replace("\n", " ").strip()
        lines.append("maint.%d.name = %s" % (item.code, name))
        lines.append("maint.%d.every_changes = %d" % (item.code, item.every_changes))
        lines.append("maint.%d.every_hours = %g" % (item.code, item.every_hours))
    return "\n".join(lines) + "\n"


def conf_path_for(json_path):
    return os.path.splitext(os.path.abspath(json_path))[0] + ".conf"


def write(json_path, conf_path=None):
    """Validate turret.json and write the F2 configuration file."""
    cfg = TurretConfig.load(json_path)
    errors = cfg.validate()
    if errors:
        raise ValueError("configuracion invalida: " + "; ".join(errors))
    path = conf_path or conf_path_for(json_path)
    text = generate(cfg)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fp:
        fp.write(text)
        fp.flush()
        os.fsync(fp.fileno())
    os.replace(tmp, path)
    return path


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Genera la configuracion del componente F2 (turret.conf)")
    parser.add_argument("config", help="ruta de turret.json")
    parser.add_argument("-o", "--output", help="ruta del .conf de salida")
    args = parser.parse_args(argv)
    try:
        path = write(args.config, args.output)
    except (OSError, ValueError) as exc:
        print("genconf: %s" % exc, file=sys.stderr)
        return 1
    print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
