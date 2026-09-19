"""Turret control package for LinuxCNC (Fanuc-style lathe turrets).

F1 implementation: a userspace Python HAL component (`turret.manager`)
with a robust state machine, alarms, maintenance counters and a
QtPyVCP configuration/monitor panel.

The FSM (`turret.fsm`) and the configuration model (`turret.config`)
are pure Python so they can be unit tested without HAL.
"""

__version__ = "0.1.0"
