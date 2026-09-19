"""Configuration model for the turret component.

The configuration lives in a JSON file (for example
``<config>/turret/turret.json``) and is the single source of truth used
by the manager, the HAL generator and the QtPyVCP panel.
"""

import json
import os
import tempfile
from dataclasses import dataclass, field, asdict

SUPPORTED_MODES = ("hydraulic-single-solenoid",)
MAX_STATIONS = 24
MAX_MAINT_CODES = 32
REQUIRED_PINS = ("pos1", "strobe", "changepos", "clamped", "motor", "unclamp")


@dataclass
class Timing:
    strobe_filter_ms: float = 3.0
    unclamp_timeout: float = 2.0
    home_timeout: float = 20.0
    rotate_timeout: float = 20.0
    settle_time: float = 0.4
    locate_timeout: float = 1.5
    locate_stable_ms: float = 150.0
    clamp_timeout: float = 2.0
    max_retries: int = 2
    lead_pulses: int = 0
    lead_auto: bool = True


@dataclass
class Logic:
    clamped_inverted: bool = False
    require_changepos: bool = True
    unclamp_before_rotate: bool = True
    interlock_required: bool = False
    abort_on_fault: bool = False
    test_enable_required: bool = True


@dataclass
class Interval:
    code: int
    name: str
    every_changes: int = 0
    every_hours: float = 0.0


@dataclass
class TurretConfig:
    version: int = 1
    stations: int = 8
    mode: str = "hydraulic-single-solenoid"
    pins: dict = field(default_factory=dict)
    timing: Timing = field(default_factory=Timing)
    logic: Logic = field(default_factory=Logic)
    maintenance: list = field(default_factory=list)

    # ------------------------------------------------------------------
    def to_dict(self):
        return {
            "version": self.version,
            "stations": self.stations,
            "mode": self.mode,
            "pins": dict(self.pins),
            "timing": asdict(self.timing),
            "logic": asdict(self.logic),
            "maintenance": [asdict(i) for i in self.maintenance],
        }

    @classmethod
    def from_dict(cls, data):
        cfg = cls()
        cfg.version = int(data.get("version", 1))
        cfg.stations = int(data.get("stations", 8))
        cfg.mode = str(data.get("mode", "hydraulic-single-solenoid"))
        cfg.pins = dict(data.get("pins", {}))
        timing = data.get("timing", {})
        for f in Timing.__dataclass_fields__:
            if f in timing:
                setattr(cfg.timing, f, timing[f])
        logic = data.get("logic", {})
        for f in Logic.__dataclass_fields__:
            if f in logic:
                setattr(cfg.logic, f, logic[f])
        cfg.maintenance = []
        for item in data.get("maintenance", []):
            cfg.maintenance.append(Interval(
                code=int(item.get("code", 0)),
                name=str(item.get("name", "")),
                every_changes=int(item.get("every_changes", 0)),
                every_hours=float(item.get("every_hours", 0.0)),
            ))
        return cfg

    # ------------------------------------------------------------------
    @classmethod
    def load(cls, path):
        with open(path, "r", encoding="utf-8") as fp:
            return cls.from_dict(json.load(fp))

    def save(self, path):
        """Atomically write the configuration (tmp file + rename)."""
        directory = os.path.dirname(os.path.abspath(path)) or "."
        fd, tmp = tempfile.mkstemp(prefix=".turret.", suffix=".json", dir=directory)
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as fp:
                json.dump(self.to_dict(), fp, indent=2, sort_keys=False)
                fp.write("\n")
                fp.flush()
                os.fsync(fp.fileno())
            os.replace(tmp, path)
        except BaseException:
            try:
                os.unlink(tmp)
            except OSError:
                pass
            raise

    # ------------------------------------------------------------------
    def validate(self):
        """Return a list of human readable problems (empty when valid)."""
        errors = []
        if self.stations < 1 or self.stations > MAX_STATIONS:
            errors.append("stations debe estar entre 1 y %d" % MAX_STATIONS)
        if self.mode not in SUPPORTED_MODES:
            errors.append("modo no soportado: %s" % self.mode)
        for name in REQUIRED_PINS:
            value = self.pins.get(name)
            if not value:
                if name == "clamp" or name == "changepos":
                    continue
                errors.append("falta el pin obligatorio '%s'" % name)
        t = self.timing
        for f in ("unclamp_timeout", "home_timeout", "rotate_timeout",
                  "settle_time", "locate_timeout", "clamp_timeout"):
            if getattr(t, f) <= 0:
                errors.append("timing.%s debe ser > 0" % f)
        if t.max_retries < 0:
            errors.append("timing.max_retries debe ser >= 0")
        if t.lead_pulses < 0:
            errors.append("timing.lead_pulses debe ser >= 0")
        seen = set()
        for item in self.maintenance:
            if item.code <= 0 or item.code > MAX_MAINT_CODES:
                errors.append("codigo de mantenimiento fuera de 1..%d: %s"
                              % (MAX_MAINT_CODES, item.code))
            if item.code in seen:
                errors.append("codigo de mantenimiento duplicado: %s" % item.code)
            seen.add(item.code)
            if item.every_changes <= 0 and item.every_hours <= 0:
                errors.append("mantenimiento %d sin intervalo" % item.code)
        return errors


DEFAULT_CONFIG = TurretConfig(
    stations=8,
    mode="hydraulic-single-solenoid",
    pins={},
    maintenance=[
        Interval(code=1, name="Pinza: revisar sellos y presion", every_changes=50000),
        Interval(code=2, name="Engrase de la corona", every_changes=500000),
        Interval(code=3, name="Filtro hidraulico", every_hours=2000.0),
    ],
)
