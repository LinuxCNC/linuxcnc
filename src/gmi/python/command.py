"""gmi.command — Drop-in replacement for linuxcnc.command().

Sends commands to the emccmd REST endpoint. Method names match
linuxcnc.command(): cmd.state(), cmd.auto(), cmd.jog(), etc.

Usage:
    c = gmi.Command()
    c.mode(gmi.MODE_MDI)
    c.mdi("G0 X10")
    c.wait_complete()
"""

from __future__ import annotations

import json
import queue
import threading
import urllib.request
from typing import Optional

from gmi import rest_url


class Command:
    """Drop-in replacement for linuxcnc.command().

    All methods correspond to REST calls to the emccmd API.
    """

    def __init__(self, instance: str = "milltask"):
        self._base = rest_url() + "/api/v1/" + instance
        self._async_queue = queue.Queue()
        self._async_worker = threading.Thread(
            target=self._async_loop, daemon=True)
        self._async_worker.start()

    def _async_loop(self):
        """Worker thread that sends queued requests in order."""
        while True:
            req = self._async_queue.get()
            try:
                with urllib.request.urlopen(req, timeout=10) as resp:
                    resp.read()
            except Exception:
                pass

    def _post(self, path: str, data: dict = None) -> dict:
        """Send POST request to the emccmd REST endpoint."""
        url = self._base + path
        body = json.dumps(data or {}).encode("utf-8")
        req = urllib.request.Request(
            url, data=body,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                return json.loads(resp.read())
        except urllib.error.HTTPError as e:
            import sys
            err_body = e.read().decode("utf-8", errors="replace")
            print(f"gmi.Command: {e.code} {url}: {err_body}", file=sys.stderr)
            raise

    def _post_async(self, path: str, data: dict = None):
        """Queue a POST request for ordered async delivery (non-blocking)."""
        url = self._base + path
        body = json.dumps(data or {}).encode("utf-8")
        req = urllib.request.Request(
            url, data=body,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        self._async_queue.put(req)

    def state(self, state: int):
        """Set task state (STATE_ESTOP, STATE_ON, etc.)."""
        self._post("/state", {"state": state})

    def mode(self, mode: int):
        """Set task mode (MODE_MANUAL, MODE_MDI, MODE_AUTO)."""
        self._post("/mode", {"mode": mode})

    def auto(self, cmd: int, line: int = 0):
        """Auto program control (AUTO_RUN, AUTO_STEP, AUTO_PAUSE, etc.)."""
        self._post("/auto", {"cmd": cmd, "line": line})

    def mdi(self, command: str):
        """Execute MDI command string."""
        self._post("/mdi", {"command": command})

    def jog(self, jog_type: int, jjogmode: bool, axis_or_joint: int,
            velocity: float = 0.0, distance: float = 0.0):
        """Jog an axis or joint (non-blocking, fire-and-forget)."""
        self._post_async("/jog", {
            "jog_type": jog_type,
            "jjogmode": bool(jjogmode),
            "axis_or_joint": axis_or_joint,
            "velocity": velocity,
            "distance": distance,
        })

    def jog_stop(self, jjogmode: bool, axis_or_joint: int):
        """Stop a jog (non-blocking, fire-and-forget)."""
        self._post_async("/jog-stop", {
            "jjogmode": bool(jjogmode),
            "axis_or_joint": axis_or_joint,
        })

    def spindle(self, direction: int, speed: float = 0.0,
                spindle: int = 0, wait: int = 0):
        """Control spindle (SPINDLE_FORWARD, SPINDLE_OFF, etc.)."""
        self._post("/spindle", {
            "cmd": direction,
            "speed": speed,
            "spindle_num": spindle,
            "wait": wait,
        })

    def home(self, joint: int):
        """Home a joint (-1 = all)."""
        self._post("/home", {"joint": joint})

    def unhome(self, joint: int):
        """Unhome a joint (-1 = all)."""
        self._post("/unhome", {"joint": joint})

    def override_limits(self, joint: int = 0):
        """Override soft limits (joint < 0 resumes normal limit checking)."""
        self._post("/override-limits", {"joint": joint})

    def teleop_enable(self, enable: bool):
        """Enable/disable teleop mode."""
        self._post("/teleop", {"enable": bool(enable)})

    def feedrate(self, rate: float):
        """Set feed override (0.0 - 1.0+)."""
        self._post("/feed-override", {"rate": rate})

    def spindleoverride(self, rate: float, spindle: int = 0):
        """Set spindle speed override."""
        self._post("/spindle-override", {"rate": rate, "spindle_num": spindle})

    def rapidrate(self, rate: float):
        """Set rapid override (0.0 - 1.0)."""
        self._post("/rapid-override", {"rate": rate})

    def maxvel(self, velocity: float):
        """Set maximum velocity."""
        self._post("/max-velocity", {"velocity": velocity})

    def flood(self, on):
        """Flood coolant on/off."""
        self._post("/flood", {"on": bool(on)})

    def mist(self, on):
        """Mist coolant on/off."""
        self._post("/mist", {"on": bool(on)})

    def brake(self, on, spindle: int = 0):
        """Spindle brake engage/release."""
        self._post("/brake", {"on": bool(on), "spindle_num": spindle})

    def abort(self):
        """Abort current operation."""
        self._post("/abort")

    def task_plan_synch(self):
        """Synchronize task planner."""
        self._post("/task-plan-synch")

    def set_optional_stop(self, on: bool):
        """Set optional stop."""
        self._post("/optional-stop", {"on": bool(on)})

    def set_block_delete(self, on: bool):
        """Set block delete."""
        self._post("/block-delete", {"on": bool(on)})

    def load_tool_table(self, file: str = ""):
        """Reload tool table from file (empty string = default table)."""
        self._post("/load-tool-table", {"file": file})

    def program_open(self, filename: str):
        """Open a program file."""
        self._post("/program-open", {"file": filename})

    def wait_complete(self, timeout: float = 5.0) -> int:
        """Wait for command completion.

        Returns:
            1 (RCS_DONE), 3 (RCS_ERROR), or -1 (timeout)
        """
        return self._post("/wait-complete", {"timeout": timeout})

    def debug(self, level: int):
        """Set debug level (bitmask of DEBUG_* flags)."""
        self._post("/debug", {"debug": level})

    def set_jog_axis(self, axis: int):
        """Set the active jog axis (0=X, 1=Y, ...)."""
        self._post("/jog-axis", {"axis": axis})

    def set_jog_increment(self, increment: float):
        """Set jog increment (0 = continuous)."""
        self._post("/jog-increment", {"increment": increment})

    def set_jog_speed(self, speed: float):
        """Set linear jog speed (units/sec)."""
        self._post("/jog-speed", {"speed": speed})

    def set_ajog_speed(self, speed: float):
        """Set angular jog speed (deg/sec)."""
        self._post("/ajog-speed", {"speed": speed})
