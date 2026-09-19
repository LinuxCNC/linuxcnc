"""HAL manager for the lathe turret (F1, userspace Python component).

Design notes
------------
* The component owns a small set of status pins (``turret.*``) that the
  QtPyVCP panel watches and that will be identical in the F2 ``.comp``.
* Machine I/O is read/written *directly* through ``hal.get_value`` /
  ``hal.set_p``, so changing the pin mapping is a configuration change
  (the manager is reloaded) instead of HAL surgery: no nets required and
  no LinuxCNC restart.
* The iocontrol handshake pins are driven the same way, so the stock
  tool change protocol (and the Fanuc ``LATHE_TXXXX`` T word) works
  without a HAL glue file.

Usage (from a HAL file)::

    loadusr -W python3 -m turret.manager --config /path/turret.json
"""

import argparse
import json
import os
import sys
import time

from .codes import Alarm, alarm_text
from .config import MAX_MAINT_CODES, MAX_STATIONS, TurretConfig
from .fsm import Inputs, State, TurretFSM, STATE_TEXT
from .maintenance import History, Maintenance

LOOP_PERIOD = 0.01
SAVE_PERIOD = 5.0


class HalIO:
    """Small wrapper around the Python HAL module (imported lazily)."""

    def __init__(self):
        import hal
        self.hal = hal
        self.comp = hal.component("turret")
        self._own = {}

    def newpin(self, name, kind, direction):
        pin = self.comp.newpin(name, kind, direction)
        self._own[name] = pin
        return pin

    def newparam(self, name, kind, direction):
        return self.comp.newparam(name, kind, direction)

    def param(self, name):
        return self.comp.getparam(name).value

    def param_set(self, name, value):
        self.comp.getparam(name).value = value

    def ready(self):
        self.comp.ready()

    # -- own pins ------------------------------------------------------
    def own(self, name):
        return self._own[name].value

    def own_set(self, name, value):
        self._own[name].value = value

    # -- other components' pins ---------------------------------------
    def get(self, name):
        return self.hal.get_value(name)

    def set(self, name, value):
        self.hal.set_p(name, value)


class TurretManager:
    def __init__(self, config_path, io=None):
        self.config_path = os.path.abspath(config_path)
        self.cfg = TurretConfig.load(self.config_path)
        errors = self.cfg.validate()
        if errors:
            raise SystemExit("turret: configuracion invalida:\n  " + "\n  ".join(errors))
        self.io = io if io is not None else HalIO()
        self.fsm = TurretFSM(self.cfg.stations, self.cfg.timing, self.cfg.logic)
        self.maintenance = Maintenance(self.cfg.maintenance)
        self.history = History()
        self._state_path = os.path.splitext(self.config_path)[0] + "_state.json"
        self._load_state()
        self._make_pins()
        self._prev = {}
        self._last_save = 0.0
        self._last_maint_check = 0.0
        self._last_hours = time.monotonic()
        self._hours = 0.0
        self._maint_notified = set()
        self._config_error = False
        self._watchdog_state = False
        self._lead_param = None
        self._prepared_latch = False
        self._changed_latch = False
        self._latched_pocket = None
        self._writer_warned = set()
        self._target = None
        self._mtime = self._file_mtime()
        self._stop = False

    # ------------------------------------------------------------------
    def _file_mtime(self):
        try:
            return os.path.getmtime(self.config_path)
        except OSError:
            return None

    def _load_state(self):
        try:
            with open(self._state_path, "r", encoding="utf-8") as fp:
                data = json.load(fp)
        except (OSError, ValueError):
            return
        self.fsm.load_state(data.get("fsm", {}))
        # the station is kept for display, but the reference is lost when
        # the machine is powered off: force a new reference search
        self.fsm.homed = False
        self.fsm.located = False
        self.maintenance.load_dict(data.get("maintenance", {}))
        self.history.load_list(data.get("history", []))

    def _save_state(self):
        data = {
            "fsm": self.fsm.state_dict(),
            "maintenance": self.maintenance.to_dict(),
            "history": self.history.to_list(),
            "saved": time.time(),
        }
        tmp = self._state_path + ".tmp"
        try:
            with open(tmp, "w", encoding="utf-8") as fp:
                json.dump(data, fp, indent=2)
                fp.flush()
                os.fsync(fp.fileno())
            os.replace(tmp, self._state_path)
        except OSError as exc:
            print("turret: no se pudo guardar %s: %s" % (self._state_path, exc),
                  file=sys.stderr)

    # ------------------------------------------------------------------
    def _make_pins(self):
        hal = self.io.hal
        io = self.io
        # status / commands owned by the component (panel + F2 parity)
        for name in ("busy", "homed", "located", "fault", "maint-due"):
            io.newpin(name, hal.Type.BIT, hal.Dir.OUT)
        for name in ("motor", "unclamp", "clamp", "prepared", "changed",
                     "watchdog-pet"):
            io.newpin(name, hal.Type.BIT, hal.Dir.OUT)
        for name in ("error", "state", "station", "maint-code",
                     "count-changes", "count-clamp"):
            io.newpin(name, hal.Type.S32, hal.Dir.OUT)
        io.newpin("hours", hal.Type.FLOAT, hal.Dir.OUT)
        try:
            io.newpin("fault-text", hal.Type.S32, hal.Dir.OUT)
        except Exception:
            pass
        for n in range(1, MAX_STATIONS + 1):
            io.newpin("station-%d-changes" % n, hal.Type.S32, hal.Dir.OUT)
        # command inputs from the panel (momentary buttons)
        for name in ("home", "reset", "cancel", "release", "lock", "step",
                     "jog", "unhome", "reload", "test-enable", "prepare",
                     "change", "clear-history"):
            io.newpin(name, hal.Type.BIT, hal.Dir.IN)
        for n in range(1, MAX_MAINT_CODES + 1):
            io.newpin("mark-service-%d" % n, hal.Type.BIT, hal.Dir.IN)
        io.newpin("pocket-request", hal.Type.S32, hal.Dir.IN)
        # live tunables (panel applies them without restarting)
        timing = self.cfg.timing
        for name in ("unclamp-timeout", "home-timeout", "rotate-timeout",
                     "settle-time", "locate-timeout", "locate-stable-ms",
                     "clamp-timeout", "strobe-filter-ms"):
            param = io.newparam(name, hal.Type.FLOAT, hal.Dir.IO)
            param.value = getattr(timing, name.replace("-", "_"))
        for name in ("max-retries", "lead-pulses"):
            param = io.newparam(name, hal.Type.S32, hal.Dir.IO)
            param.value = getattr(timing, name.replace("-", "_"))
        io.newparam("pockets", hal.Type.S32, hal.Dir.IO).value = self.cfg.stations

    def _apply_live_params(self):
        """Copy HAL params into the FSM (panel writes them live)."""
        t = self.cfg.timing
        for name in ("unclamp_timeout", "home_timeout", "rotate_timeout",
                     "settle_time", "locate_timeout", "locate_stable_ms",
                     "clamp_timeout", "strobe_filter_ms"):
            try:
                val = self.io.param(name.replace("_", "-"))
                if val and val > 0:
                    setattr(t, name, float(val))
            except Exception:
                pass
        try:
            t.max_retries = int(self.io.param("max-retries"))
        except Exception:
            pass
        try:
            val = int(self.io.param("lead-pulses"))
            if val != self._lead_param:
                self._lead_param = val
                if val >= 0:
                    self.fsm.lead = max(0, min(2, val))
        except Exception:
            pass

    # ------------------------------------------------------------------
    def _machine_pin(self, key):
        return self.cfg.pins.get(key) or None

    def _read_machine(self):
        """Read the configured machine inputs (missing pin -> E10)."""
        read = {}
        for key in ("pos1", "strobe", "changepos", "clamped"):
            pin = self._machine_pin(key)
            value = False
            if pin:
                try:
                    value = bool(self.io.get(pin))
                except Exception:
                    if not self._config_error:
                        self._config_error = True
                        self.fsm._set_fault(Alarm.E_CONFIG, time.monotonic())
                        self._log_alarm(Alarm.E_CONFIG, "pin inexistente: %s" % pin)
            read[key] = value
        return read

    def _write_machine(self, out):
        motor_pin = self._machine_pin("motor")
        unclamp_pin = self._machine_pin("unclamp")
        try:
            if motor_pin:
                self.io.set(motor_pin, 1 if out.motor else 0)
            if unclamp_pin:
                self.io.set(unclamp_pin, 1 if out.unclamp else 0)
        except Exception as exc:
            if not self._config_error:
                self._config_error = True
                self._log_alarm(Alarm.E_CONFIG, "no se puede escribir %s: %s"
                                % (motor_pin or unclamp_pin, exc))
                self.fsm._set_fault(Alarm.E_CONFIG, time.monotonic())

    # ------------------------------------------------------------------
    def _edge(self, name, value):
        prev = self._prev.get(name, False)
        self._prev[name] = bool(value)
        return bool(value) and not prev

    def _handshake(self, now):
        """Fanuc/lathe tool change handshake with iocontrol.

        The component's own prepare/change/pocket-request pins are ORed
        with the configured iocontrol pins, so the panel ("Ir a
        estacion") also works on a real machine.  prepared/changed are
        latched until the corresponding input goes low: the task lowers
        tool-prepare as soon as it reads tool-prepared, so a level-derived
        changed could be dropped before the task reads tool-changed.
        """
        cfg = self.cfg.pins
        manual = bool(self.io.own("prepare"))
        prepare = manual
        change = bool(self.io.own("change"))
        try:
            own_pocket = int(self.io.own("pocket-request"))
        except Exception:
            own_pocket = 0
        number = 0

        if cfg.get("tool_prepare"):
            try:
                prepare = prepare or bool(self.io.get(cfg["tool_prepare"]))
            except Exception:
                pass
        if cfg.get("tool_change"):
            try:
                change = change or bool(self.io.get(cfg["tool_change"]))
            except Exception:
                pass
        if cfg.get("tool_prep_number"):
            try:
                number = int(self.io.get(cfg["tool_prep_number"]))
            except Exception:
                number = 0

        pocket = 0
        if manual:
            # manual selection from the panel: use its own request
            if 1 <= own_pocket <= self.cfg.stations:
                pocket = own_pocket
        elif cfg.get("tool_prep_pocket"):
            # iocontrol (T/M6): the station comes from the tool table P
            try:
                value = int(self.io.get(cfg["tool_prep_pocket"]))
                if 1 <= value <= self.cfg.stations:
                    pocket = value
            except Exception:
                pocket = 0

        if not prepare:
            self._prepared_latch = False
            self._latched_pocket = None
            if self.fsm.state == State.IDLE:
                self._target = None
        elif pocket == 0:
            # T0 / unload: there is nothing to prepare
            if number != 0:
                self._warn_once(
                    "tool-prep-pocket",
                    "la herramienta %d no tiene P (pocket) en la tabla; "
                    "la torreta no puede ubicarla" % number)
            else:
                self._prepared_latch = True
                self._latched_pocket = 0
        else:
            if (self.fsm.state == State.IDLE and not self.fsm.busy
                    and (not self._prepared_latch
                         or self._latched_pocket != pocket)
                    and not (self.fsm.at_target
                             and self.fsm.station == pocket)):
                self._target = pocket
                self._latched_pocket = pocket
                self.fsm.request_select(pocket)
            if self.fsm.at_target and self.fsm.station == pocket:
                self._prepared_latch = True

        if not change:
            self._changed_latch = False
        elif self._prepared_latch:
            self._changed_latch = True

        prepared_v = 1 if self._prepared_latch else 0
        changed_v = 1 if self._changed_latch else 0
        self.io.own_set("prepared", prepared_v)
        self.io.own_set("changed", changed_v)
        for pin, value in ((cfg.get("tool_prepared"), prepared_v),
                           (cfg.get("tool_changed"), changed_v)):
            if not pin:
                continue
            try:
                self.io.set(pin, value)
            except Exception:
                self._warn_writer_once(pin)

    def _warn_once(self, key, text):
        if key in self._writer_warned:
            return
        self._writer_warned.add(key)
        self._log_alarm(Alarm.E_CONFIG, text)

    def _warn_writer_once(self, pin):
        self._warn_once(
            pin,
            "no se puede escribir %s: quitalo de cualquier net "
            "(por ejemplo tool-prep-loop/tool-change-loop)" % pin)

    # ------------------------------------------------------------------
    def _fault_pins(self):
        cfg = self.cfg.pins
        fault_pin = cfg.get("toolchanger_fault")
        reason_pin = cfg.get("toolchanger_reason")
        for pin in (fault_pin, reason_pin):
            if not pin:
                continue
            try:
                self.io.set(pin, (1 if self.fsm.fault else 0)
                            if pin == fault_pin else int(self.fsm.error))
            except Exception:
                self._warn_writer_once(pin)

    def _abort_program(self):
        try:
            import linuxcnc
            linuxcnc.command().abort()
        except Exception as exc:
            print("turret: no se pudo abortar el programa: %s" % exc,
                  file=sys.stderr)

    def _log_alarm(self, code, text, kind="alarm"):
        self.history.append(code=code, text=text, station=self.fsm.station,
                            kind=kind, now=time.time())
        print("turret: %s [%s] %s" % (kind, code, text), file=sys.stderr)

    # ------------------------------------------------------------------
    def _tick(self, now):
        io = self.io
        self._apply_live_params()
        machine = self._read_machine()
        inp = Inputs(
            pos1=machine["pos1"],
            strobe=machine["strobe"],
            changepos=machine["changepos"],
            clamped=machine["clamped"],
            interlock_ok=self._interlock_ok(),
            test_enable=bool(io.own("test-enable")),
            jog=bool(io.own("jog")),
        )
        # panel commands
        if self._edge("home", io.own("home")):
            self.fsm.request_home()
        if self._edge("reset", io.own("reset")):
            self.fsm.request_reset()
        if self._edge("cancel", io.own("cancel")):
            self.fsm.request_abort()
        if self._edge("release", io.own("release")):
            self.fsm.request_release()
        if self._edge("lock", io.own("lock")):
            self.fsm.request_lock()
        if self._edge("step", io.own("step")):
            self.fsm.request_step()
        if self._edge("jog", io.own("jog")):
            self.fsm.request_jog()
        if self._edge("unhome", io.own("unhome")):
            self.fsm.homed = False
        if self._edge("reload", io.own("reload")):
            self._reload_config()
        if self._edge("clear-history", io.own("clear-history")):
            self.history.events = []
        for item in self.maintenance.intervals:
            if item.code > MAX_MAINT_CODES:
                continue
            name = "mark-service-%d" % item.code
            if self._edge(name, io.own(name)):
                self.maintenance.mark_service(item.code, self.fsm.changes,
                                              self._hours)
                self._maint_notified.discard(item.code)
                self._log_alarm(item.code,
                                "Servicio realizado: %s" % item.name,
                                kind="maintenance")

        out = self.fsm.update(now, inp)
        self._handshake(now)

        io.own_set("motor", out.motor)
        io.own_set("unclamp", out.unclamp)
        io.own_set("clamp", out.clamp)
        io.own_set("busy", out.busy)
        io.own_set("homed", out.homed)
        io.own_set("located", out.located)
        io.own_set("fault", out.fault)
        io.own_set("error", out.error)
        io.own_set("state", out.state)
        io.own_set("station", out.station)
        io.own_set("count-changes", self.fsm.changes)
        io.own_set("count-clamp", self.fsm.clamp_cycles)
        io.own_set("hours", self._hours)
        for n in range(1, self.cfg.stations + 1):
            io.own_set("station-%d-changes" % n, self.fsm.station_counts[n - 1])
        self._write_machine(out)
        self._fault_pins()

        # watchdog heartbeat (toggles while alive; a dead process stops
        # toggling and the hardware watchdog drops the valve relay)
        self._watchdog_state = not self._watchdog_state
        io.own_set("watchdog-pet", self._watchdog_state)
        pet_pin = self._machine_pin("watchdog_pet")
        if pet_pin:
            try:
                io.set(pet_pin, 1 if self._watchdog_state else 0)
            except Exception:
                pass

        # fault bookkeeping / maintenance / persistence
        if self.fsm.fault and self.fsm.error:
            if self._prev.get("_faulted") != self.fsm.error:
                self._log_alarm(self.fsm.error, alarm_text(self.fsm.error))
                if self.cfg.logic.abort_on_fault:
                    self._abort_program()
            self._prev["_faulted"] = self.fsm.error
        else:
            self._prev["_faulted"] = 0
        if abs(now - self._last_maint_check) >= 1.0:
            self._last_maint_check = now
            self._maint_check()
        if abs(now - self._last_save) >= SAVE_PERIOD:
            self._last_save = now
            self._save_state()
        # config hot reload by file mtime
        mtime = self._file_mtime()
        if mtime is not None and mtime != self._mtime:
            self._reload_config()

    def _interlock_ok(self):
        pin = self._machine_pin("interlock")
        if not pin:
            return True
        try:
            return bool(self.io.get(pin))
        except Exception:
            return True

    def _maint_check(self):
        due = self.maintenance.due(self.fsm.changes, self._hours)
        self.io.own_set("maint-due", 1 if due else 0)
        if due:
            self.io.own_set("maint-code", due[0]["code"])
            for item in due:
                if item["code"] not in self._maint_notified:
                    self._maint_notified.add(item["code"])
                    self._log_alarm(item["code"],
                                    "Mantenimiento: %s (%s)" % (item["name"], item["detail"]),
                                    kind="maintenance")
        else:
            self.io.own_set("maint-code", 0)

    def _reload_config(self):
        self._mtime = self._file_mtime()
        try:
            cfg = TurretConfig.load(self.config_path)
        except (OSError, ValueError) as exc:
            self._log_alarm(Alarm.E_CONFIG, "no se puede releer la configuracion: %s" % exc)
            return
        if cfg.validate():
            self._log_alarm(Alarm.E_CONFIG, "configuracion invalida al recargar")
            return
        stations_changed = cfg.stations != self.cfg.stations
        self.cfg = cfg
        self.fsm.timing = cfg.timing
        self.fsm.logic = cfg.logic
        self.maintenance.intervals = cfg.maintenance
        self.io.param_set("pockets", cfg.stations)
        if stations_changed:
            old = self.fsm
            new = TurretFSM(cfg.stations, cfg.timing, cfg.logic)
            new.load_state(old.state_dict())
            new.homed = False    # station count changed: reference again
            self.fsm = new
            for n in range(1, min(old.stations, cfg.stations) + 1):
                self.fsm.station_counts[n - 1] = old.station_counts[n - 1]
        print("turret: configuracion recargada", file=sys.stderr)

    # ------------------------------------------------------------------
    def run(self):
        self.io.ready()
        self._last_hours = time.monotonic()
        try:
            while not self._stop:
                now = time.monotonic()
                self._hours += max(0.0, now - self._last_hours)
                self._last_hours = now
                self._tick(now)
                time.sleep(LOOP_PERIOD)
        except KeyboardInterrupt:
            pass
        finally:
            self.shutdown()

    def shutdown(self):
        try:
            self.io.own_set("motor", 0)
            self.io.own_set("unclamp", 0)
            self.io.own_set("busy", 0)
        except Exception:
            pass
        self._write_machine_safe()
        self._save_state()

    def _write_machine_safe(self):
        for key, value in (("motor", 0), ("unclamp", 0), ("clamp", 0)):
            pin = self._machine_pin(key)
            if not pin:
                continue
            try:
                self.io.set(pin, value)
            except Exception:
                pass


def main(argv=None):
    parser = argparse.ArgumentParser(description="LinuxCNC lathe turret manager")
    parser.add_argument("--config", required=True, help="ruta de turret.json")
    args = parser.parse_args(argv)
    manager = TurretManager(args.config)
    manager.run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
