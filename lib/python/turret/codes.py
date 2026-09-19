"""Alarm, event and maintenance codes for the turret component.

All user visible text is Spanish (the panel is embedded in a Spanish
UI); codes are stable numbers so they can be logged and translated
independently.
"""

from enum import IntEnum


class Severity(IntEnum):
    INFO = 0
    WARNING = 1
    FAULT = 2


class Alarm(IntEnum):
    NONE = 0
    E_UNCLAMP_TIMEOUT = 1
    E_HOME_TIMEOUT = 2
    E_ROTATE_TIMEOUT = 3
    E_OVERSHOOT = 4
    E_NOT_LOCATED = 5
    E_CLAMP_TIMEOUT = 6
    E_STROBE_FAULT = 7
    E_INTERLOCK = 8
    E_OPERATOR_ABORT = 9
    E_CONFIG = 10
    E_WATCHDOG = 11


ALARM_TEXT = {
    Alarm.NONE: "Sin alarma",
    Alarm.E_UNCLAMP_TIMEOUT: "E1: la pinza no se libero a tiempo",
    Alarm.E_HOME_TIMEOUT: "E2: no se encontro la referencia (posicion 1 + strobe)",
    Alarm.E_ROTATE_TIMEOUT: "E3: la torreta no alcanzo la estacion pedida",
    Alarm.E_OVERSHOOT: "E4: la torreta se paso de la estacion (coast excesivo)",
    Alarm.E_NOT_LOCATED: "E5: la torreta no quedo asentada (sensor de posicion de cambio)",
    Alarm.E_CLAMP_TIMEOUT: "E6: la pinza no se cerro a tiempo",
    Alarm.E_STROBE_FAULT: "E7: pulsos de strobe con la torreta detenida (sensor o fuga)",
    Alarm.E_INTERLOCK: "E8: interlock perdido durante el movimiento",
    Alarm.E_OPERATOR_ABORT: "E9: abortado por el operador",
    Alarm.E_CONFIG: "E10: configuracion invalida o pin inexistente",
    Alarm.E_WATCHDOG: "E11: watchdog de la torreta vencido",
}


def alarm_text(code):
    """Return the Spanish text for an alarm code (tolerant to ints)."""
    try:
        return ALARM_TEXT[Alarm(code)]
    except (ValueError, KeyError):
        return "E?: alarma desconocida (%s)" % code


def is_fault(code):
    return int(code) > 0
