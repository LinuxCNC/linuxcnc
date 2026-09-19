#!/usr/bin/env python3
"""HAL simulator for a hydraulic single-solenoid lathe turret.

It behaves like the real mechanism so the ``turret.manager`` can be
tested without a machine:

  * ``motor``   (in)  : energized = rotate one direction
  * ``unclamp`` (in)  : ON = clamp released (spring closes)
  * ``pos1``    (out) : station 1 index sensor
  * ``strobe``  (out) : pulse per station while rotating
  * ``changepos`` (out): turret seated, clamp allowed
  * ``clamped`` (out) : clamp closed
  * ``station`` (out) : current station (simulation only)

Everything runs in a ``hal.component``; the manager drives/reads it with
``hal.set_p`` / ``hal.get_value`` so no HAL nets are needed.
"""

import argparse
import sys
import time

import hal

LOOP_PERIOD = 0.01


class TurretSim:
    def __init__(self, stations=8, pulse_period=0.08, coast_pulses=1,
                 start=2, unclamp_delay=0.05, clamp_delay=0.10,
                 changepos_delay=0.10):
        self.stations = stations
        self.pulse_period = pulse_period
        self.coast_pulses = coast_pulses
        self.station = start
        self.unclamp_delay = unclamp_delay
        self.clamp_delay = clamp_delay
        self.changepos_delay = changepos_delay

        self.comp = hal.component("sim_turret")
        self.motor = self.comp.newpin("motor", hal.Type.BIT, hal.Dir.IN)
        self.unclamp = self.comp.newpin("unclamp", hal.Type.BIT, hal.Dir.IN)
        self.pos1 = self.comp.newpin("pos1", hal.Type.BIT, hal.Dir.OUT)
        self.strobe = self.comp.newpin("strobe", hal.Type.BIT, hal.Dir.OUT)
        self.changepos = self.comp.newpin("changepos", hal.Type.BIT, hal.Dir.OUT)
        self.clamped = self.comp.newpin("clamped", hal.Type.BIT, hal.Dir.OUT)
        self.station_pin = self.comp.newpin("station", hal.Type.S32, hal.Dir.OUT)

        self._pulse_next = None
        self._coast_left = 0
        self._motor_prev = False
        self._clamped = True
        self._unclamp_since = None
        self._clamp_since = None
        self._stop_since = None

        # initial output values (powered up, clamped, seated)
        self.pos1.value = 1 if self.station == 1 else 0
        self.strobe.value = 0
        self.changepos.value = 1
        self.clamped.value = 1
        self.station_pin.value = self.station

    # ------------------------------------------------------------------
    def tick(self, now, dt):
        motor = bool(self.motor.value)
        unclamp = bool(self.unclamp.value)

        # clamp / unclamp dynamics
        if unclamp:
            if self._unclamp_since is None:
                self._unclamp_since = now
            if (now - self._unclamp_since) >= self.unclamp_delay:
                self._clamped = False
            self._clamp_since = None
        else:
            self._unclamp_since = None
            if self._clamped:
                self._clamp_since = None
            elif self._clamp_since is None:
                self._clamp_since = now
            if self._clamp_since is not None and \
                    (now - self._clamp_since) >= self.clamp_delay:
                self._clamped = True

        # rotation and coast
        if motor and not self._motor_prev:
            self._pulse_next = now + self.pulse_period
            self._stop_since = None
        if not motor and self._motor_prev:
            self._coast_left = self.coast_pulses
            self._pulse_next = (now + self.pulse_period
                                if self._coast_left else None)
            self._stop_since = None
        self._motor_prev = motor

        strobe = False
        if self._pulse_next is not None and now >= self._pulse_next:
            strobe = True
            self.station = (self.station % self.stations) + 1
            self._stop_since = None
            if motor:
                self._pulse_next = now + self.pulse_period
            elif self._coast_left > 0:
                self._coast_left -= 1
                self._pulse_next = (now + self.pulse_period
                                    if self._coast_left else None)
            else:
                self._pulse_next = None
        elif self._pulse_next is None and not motor:
            if self._stop_since is None:
                self._stop_since = now

        moving = motor or self._coast_left > 0 or self._pulse_next is not None
        if not moving and self._stop_since is None:
            self._stop_since = now
        located = (not moving and self._stop_since is not None and
                   (now - self._stop_since) >= self.changepos_delay)

        self.pos1.value = 1 if self.station == 1 else 0
        self.strobe.value = 1 if strobe else 0
        self.changepos.value = 1 if located else 0
        self.clamped.value = 1 if self._clamped else 0
        self.station_pin.value = self.station

    def run(self):
        self.comp.ready()
        last = time.monotonic()
        try:
            while True:
                now = time.monotonic()
                dt = now - last
                last = now
                self.tick(now, min(dt, 0.1))
                time.sleep(LOOP_PERIOD)
        except KeyboardInterrupt:
            pass


def main(argv=None):
    parser = argparse.ArgumentParser(description="turret simulator")
    parser.add_argument("--stations", type=int, default=8)
    parser.add_argument("--pulse-period", type=float, default=0.08)
    parser.add_argument("--coast-pulses", type=int, default=1)
    parser.add_argument("--start", type=int, default=2)
    args = parser.parse_args(argv)
    sim = TurretSim(stations=args.stations, pulse_period=args.pulse_period,
                    coast_pulses=args.coast_pulses, start=args.start)
    sim.run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
