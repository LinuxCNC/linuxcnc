"""Maintenance counters, intervals and event history for the turret."""

import time


class Maintenance:
    """Track maintenance intervals against the turret counters.

    ``intervals`` are :class:`turret.config.Interval` objects.  Each
    interval keeps a baseline snapshot of the counters taken the last
    time it was serviced; an interval is *due* when the difference since
    the baseline reaches the configured amount.
    """

    def __init__(self, intervals=None):
        self.intervals = list(intervals or [])
        self.last_service = {}   # code -> {"changes": n, "hours": h, "time": t}

    # ------------------------------------------------------------------
    def _baseline(self, code):
        base = self.last_service.get(code)
        if not base:
            return 0, 0.0
        return int(base.get("changes", 0)), float(base.get("hours", 0.0))

    def due(self, changes, hours):
        """Return the list of due intervals (dicts with code/name/detail)."""
        result = []
        for item in self.intervals:
            base_c, base_h = self._baseline(item.code)
            if item.every_changes > 0:
                delta = int(changes) - base_c
                if delta >= item.every_changes:
                    result.append({
                        "code": item.code,
                        "name": item.name,
                        "detail": "%d/%d cambios" % (delta, item.every_changes),
                    })
                    continue
            if item.every_hours > 0:
                delta = float(hours) - base_h
                if delta >= item.every_hours:
                    result.append({
                        "code": item.code,
                        "name": item.name,
                        "detail": "%.1f/%.1f horas" % (delta, item.every_hours),
                    })
        return result

    def progress(self, changes, hours):
        """Return per-interval progress for the GUI (0..1)."""
        result = []
        for item in self.intervals:
            base_c, base_h = self._baseline(item.code)
            pct = 0.0
            if item.every_changes > 0:
                pct = max(pct, (int(changes) - base_c) / float(item.every_changes))
            if item.every_hours > 0:
                pct = max(pct, (float(hours) - base_h) / item.every_hours)
            result.append({
                "code": item.code,
                "name": item.name,
                "progress": min(1.0, pct),
                "due": pct >= 1.0,
            })
        return result

    def mark_service(self, code, changes, hours, now=None):
        self.last_service[int(code)] = {
            "changes": int(changes),
            "hours": float(hours),
            "time": float(now if now is not None else time.time()),
        }

    # ------------------------------------------------------------------
    def to_dict(self):
        return {"last_service": self.last_service}

    def load_dict(self, data):
        if isinstance(data, dict) and isinstance(data.get("last_service"), dict):
            self.last_service = {int(k): v for k, v in data["last_service"].items()}


class History:
    """Bounded list of alarm/maintenance events (JSON persistable)."""

    def __init__(self, cap=200):
        self.cap = int(cap)
        self.events = []

    def append(self, code, text, station=0, kind="alarm", now=None):
        self.events.append({
            "time": float(now if now is not None else time.time()),
            "kind": kind,
            "code": int(code),
            "station": int(station),
            "text": text,
        })
        if len(self.events) > self.cap:
            self.events = self.events[-self.cap:]

    def to_list(self):
        return list(self.events)

    def load_list(self, data):
        if isinstance(data, list):
            self.events = data[-self.cap:]
