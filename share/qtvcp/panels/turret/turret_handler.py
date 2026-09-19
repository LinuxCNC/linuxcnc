#!/usr/bin/env python3
"""QtPyVCP panel for the lathe turret: estado, alarmas, mantenimiento y
configuracion en tiempo real (los cambios de parametros se aplican en
caliente; el manager recarga el JSON por fecha de modificacion).
"""

import json
import os
import time

import hal
from qtpy.QtCore import Qt, QTimer
from qtpy.QtGui import QColor
from qtpy.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox,
                            QDoubleSpinBox, QFormLayout, QGridLayout,
                            QGroupBox, QHBoxLayout, QLabel, QPlainTextEdit,
                            QPushButton, QSpinBox, QTableWidget,
                            QTableWidgetItem, QVBoxLayout, QWidget)

from qtvcp.core import Info
from qtvcp import logger

from turret.codes import alarm_text
from turret.config import TurretConfig
from turret.fsm import STATE_TEXT

LOG = logger.getLogger(__name__)
INFO = Info()

COMP = "turret"

HAL_IN = 1 << 4
HAL_OUT = 1 << 5

# key, etiqueta y rol para el asistente de mapeo de pines
PIN_ROLES = (
    ("pos1", "Sensor posicion 1 (index)", "read"),
    ("strobe", "Sensor strobe (pulso por estacion)", "read"),
    ("changepos", "Sensor posicion de cambio", "read"),
    ("clamped", "Sensor pinza cerrada", "read"),
    ("motor", "Salida solenoide de giro", "write"),
    ("unclamp", "Salida pinza (ON = liberar)", "write"),
    ("clamp", "Salida pinza (opcional, 2 solenoides)", "write"),
    ("interlock", "Interlock (maquina habilitada)", "any"),
    ("watchdog_pet", "Watchdog (pet)", "any"),
    ("tool_prepare", "iocontrol: tool-prepare (leer)", "io_out"),
    ("tool_change", "iocontrol: tool-change (leer)", "io_out"),
    ("tool_prep_pocket", "iocontrol: tool-prep-pocket (leer)", "io_out"),
    ("tool_prepared", "iocontrol: tool-prepared (escribir)", "io_in"),
    ("tool_changed", "iocontrol: tool-changed (escribir)", "io_in"),
    ("toolchanger_fault", "iocontrol: toolchanger-fault (escribir)", "io_in"),
    ("toolchanger_reason", "iocontrol: toolchanger-reason (escribir)", "io_in"),
)


class HandlerClass:

    # ------------------------------------------------------------------
    def __init__(self, halcomp, widgets, paths):
        self.hal = halcomp
        self.w = widgets
        self.PATHS = paths
        self.config_path = None
        self.config = None
        self._state_path = None
        self._leds = {}
        self._labels = {}
        self._maint_rows = []
        self._ticks = 0

    # ------------------------------------------------------------------
    def initialized__(self):
        self._resolve_config()
        self._build_estado(self.w.tab_estado)
        self._build_estaciones(self.w.tab_estaciones)
        self._build_alarmas(self.w.tab_alarmas)
        self._build_mantenimiento(self.w.tab_mantenimiento)
        self._build_config(self.w.tab_config)
        self._timer = QTimer(self.w)
        self._timer.timeout.connect(self._update)
        self._timer.start(200)
        self._update()

    # ------------------------------------------------------------------
    # config helpers
    # ------------------------------------------------------------------
    def _resolve_config(self):
        candidates = []
        try:
            from_ini = INFO.INI.getstring("TURRET", "CONFIG", fallback=None)
            if from_ini:
                candidates.append(from_ini)
            base = os.path.dirname(INFO.INIPATH)
            candidates.append(os.path.join(base, "turret.json"))
            candidates.append(os.path.join(base, "turret", "turret.json"))
        except Exception:
            pass
        for path in candidates:
            if path and os.path.exists(path):
                self.config_path = os.path.abspath(path)
                break
        if self.config_path:
            self._state_path = os.path.splitext(self.config_path)[0] + "_state.json"
            try:
                self.config = TurretConfig.load(self.config_path)
            except Exception as exc:
                LOG.error("turret panel: no se puede leer %s: %s"
                          % (self.config_path, exc))

    # ------------------------------------------------------------------
    # HAL helpers
    # ------------------------------------------------------------------
    def _pin(self, name, default=0):
        try:
            return hal.get_value("%s.%s" % (COMP, name))
        except Exception:
            return default

    def _set(self, name, value):
        try:
            hal.set_p("%s.%s" % (COMP, name), value)
        except Exception as exc:
            LOG.error("turret panel: no se puede escribir %s.%s: %s"
                      % (COMP, name, exc))

    def _pulse(self, name):
        self._set(name, 1)
        QTimer.singleShot(150, lambda: self._set(name, 0))

    def _machine_pin(self, key):
        if not self.config:
            return None
        return self.config.pins.get(key)

    def _machine(self, key, default=0):
        pin = self._machine_pin(key)
        if not pin:
            return default
        try:
            return hal.get_value(pin)
        except Exception:
            return default

    # ------------------------------------------------------------------
    # UI helpers
    # ------------------------------------------------------------------
    def _make_led(self):
        led = QLabel()
        led.setFixedSize(16, 16)
        led.setStyleSheet("background-color: #444; border-radius: 8px;")
        return led

    def _set_led(self, led, on, color="#28c76f"):
        led.setStyleSheet("background-color: %s; border-radius: 8px;"
                          % (color if on else "#444"))

    def _button(self, text, callback, parent=None, checkable=False):
        btn = QPushButton(text)
        btn.setMinimumHeight(32)
        if checkable:
            btn.setCheckable(True)
            btn.toggled.connect(callback)
        else:
            btn.clicked.connect(callback)
        return btn

    # ------------------------------------------------------------------
    # estado
    # ------------------------------------------------------------------
    def _build_estado(self, page):
        layout = QVBoxLayout(page)

        sensors = QGroupBox("Sensores")
        grid = QGridLayout(sensors)
        for row, (key, text) in enumerate((
                ("pos1", "Posicion 1 (index)"),
                ("strobe", "Strobe (pulso por estacion)"),
                ("changepos", "Posicion de cambio"),
                ("clamped", "Pinza cerrada"))):
            led = self._make_led()
            grid.addWidget(led, row, 0)
            grid.addWidget(QLabel(text), row, 1)
            self._leds[key] = led
        layout.addWidget(sensors)

        status = QGroupBox("Estado")
        form = QFormLayout(status)
        self._labels["state"] = QLabel("-")
        self._labels["station"] = QLabel("-")
        self._labels["homed"] = QLabel("-")
        self._labels["located"] = QLabel("-")
        self._labels["counts"] = QLabel("-")
        form.addRow("Fase:", self._labels["state"])
        form.addRow("Estacion:", self._labels["station"])
        form.addRow("Referenciada:", self._labels["homed"])
        form.addRow("Asentada:", self._labels["located"])
        form.addRow("Cambios / pinza:", self._labels["counts"])
        layout.addWidget(status)

        cmd = QGroupBox("Comandos")
        cgrid = QGridLayout(cmd)
        cgrid.addWidget(self._button("Referenciar (ir a posicion 1)",
                                      lambda: self._pulse("home")), 0, 0, 1, 2)
        cgrid.addWidget(self._button("Paso (+1)", lambda: self._pulse("step")), 1, 0)
        jog = QPushButton("Avanzar (mantener)")
        jog.setMinimumHeight(32)
        jog.pressed.connect(lambda: self._set("jog", 1))
        jog.released.connect(lambda: self._set("jog", 0))
        cgrid.addWidget(jog, 1, 1)
        self.target = QSpinBox()
        self.target.setRange(1, 24)
        self.target.setValue(1)
        cgrid.addWidget(QLabel("Estacion:"), 2, 0)
        cgrid.addWidget(self.target, 2, 1)
        cgrid.addWidget(self._button("Ir", self._goto), 3, 0, 1, 2)
        cgrid.addWidget(self._button("Liberar pinza (sacar)", lambda: self._pulse("release")), 4, 0)
        cgrid.addWidget(self._button("Meter pinza", lambda: self._pulse("lock")), 4, 1)
        cgrid.addWidget(self._button("Reset fallas", lambda: self._pulse("reset")), 5, 0)
        cgrid.addWidget(self._button("Cancelar", lambda: self._pulse("cancel")), 5, 1)
        self.test_enable = QCheckBox("Modo servicio (test-enable)")
        self.test_enable.toggled.connect(lambda v: self._set("test-enable", 1 if v else 0))
        cgrid.addWidget(self.test_enable, 6, 0, 1, 2)
        hint = QLabel("Referenciar no necesita modo servicio; el avance, "
                      "el paso y la pinza si.")
        hint.setWordWrap(True)
        cgrid.addWidget(hint, 7, 0, 1, 2)
        layout.addWidget(cmd)
        layout.addStretch(1)

    def _goto(self):
        station = self.target.value()
        self._set("pocket-request", station)
        self._pulse("prepare")

    # ------------------------------------------------------------------
    # alarmas
    # ------------------------------------------------------------------
    def _build_alarmas(self, page):
        layout = QVBoxLayout(page)
        box = QGroupBox("Alarma activa")
        vbox = QVBoxLayout(box)
        self._labels["alarm"] = QLabel("Sin alarma")
        self._labels["alarm"].setWordWrap(True)
        vbox.addWidget(self._labels["alarm"])
        layout.addWidget(box)
        history = QGroupBox("Historial")
        hbox = QVBoxLayout(history)
        self.history_view = QPlainTextEdit()
        self.history_view.setReadOnly(True)
        hbox.addWidget(self.history_view)
        row = QHBoxLayout()
        row.addWidget(self._button("Limpiar historial", self._clear_history))
        row.addWidget(self._button("Actualizar", self._refresh_history))
        hbox.addLayout(row)
        layout.addWidget(history, 1)

    def _clear_history(self):
        self._pulse("clear-history")
        QTimer.singleShot(300, self._refresh_history)

    def _refresh_history(self):
        events = []
        if self._state_path and os.path.exists(self._state_path):
            try:
                with open(self._state_path, "r", encoding="utf-8") as fp:
                    events = json.load(fp).get("history", [])
            except Exception:
                events = []
        lines = []
        for ev in reversed(events[-200:]):
            stamp = time.strftime("%d/%m %H:%M:%S", time.localtime(ev.get("time", 0)))
            lines.append("[%s] %s E%s est.%s %s"
                         % (stamp, ev.get("kind", "?"), ev.get("code", ""),
                            ev.get("station", ""), ev.get("text", "")))
        self.history_view.setPlainText("\n".join(lines))

    # ------------------------------------------------------------------
    # mantenimiento
    # ------------------------------------------------------------------
    def _build_mantenimiento(self, page):
        layout = QVBoxLayout(page)
        counters = QGroupBox("Contadores")
        cgrid = QGridLayout(counters)
        self._labels["maint_counts"] = QLabel("-")
        self._labels["maint_due"] = QLabel("-")
        cgrid.addWidget(QLabel("Cambios / pinza / horas:"), 0, 0)
        cgrid.addWidget(self._labels["maint_counts"], 0, 1)
        cgrid.addWidget(QLabel("Estado:"), 1, 0)
        cgrid.addWidget(self._labels["maint_due"], 1, 1)
        layout.addWidget(counters)

        service = QGroupBox("Servicio")
        srow = QHBoxLayout(service)
        srow.addWidget(self._button("Liberar (sacar torreta)", lambda: self._pulse("release")))
        srow.addWidget(self._button("Meter (bloquear)", lambda: self._pulse("lock")))
        srow.addWidget(self._button("Volver a referenciar", lambda: self._pulse("home")))
        layout.addWidget(service)

        tablebox = QGroupBox("Rutinas programadas")
        tbox = QVBoxLayout(tablebox)
        self.maint_table = QTableWidget(0, 5)
        self.maint_table.setHorizontalHeaderLabels(
            ["Codigo", "Rutina", "Intervalo", "Estado", ""])
        self.maint_table.verticalHeader().setVisible(False)
        self.maint_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        tbox.addWidget(self.maint_table)
        layout.addWidget(tablebox, 1)
        self._refresh_maintenance()

    def _refresh_maintenance(self):
        if not self.config:
            return
        self.maint_table.setRowCount(0)
        baselines = {}
        if self._state_path and os.path.exists(self._state_path):
            try:
                with open(self._state_path, "r", encoding="utf-8") as fp:
                    baselines = json.load(fp).get("maintenance", {}).get("last_service", {})
            except Exception:
                baselines = {}
        changes = self._pin("count-changes", 0)
        hours = self._pin("hours", 0.0)
        for item in self.config.maintenance:
            base = baselines.get(str(item.code)) or baselines.get(item.code) or {}
            base_c = int(base.get("changes", 0))
            base_h = float(base.get("hours", 0.0))
            due = False
            detail = []
            if item.every_changes > 0:
                delta = changes - base_c
                detail.append("%d/%d cambios" % (delta, item.every_changes))
                due = due or delta >= item.every_changes
            if item.every_hours > 0:
                delta = hours - base_h
                detail.append("%.0f/%.0f h" % (delta, item.every_hours))
                due = due or delta >= item.every_hours
            row = self.maint_table.rowCount()
            self.maint_table.insertRow(row)
            self.maint_table.setItem(row, 0, QTableWidgetItem(str(item.code)))
            self.maint_table.setItem(row, 1, QTableWidgetItem(item.name))
            self.maint_table.setItem(row, 2, QTableWidgetItem(" / ".join(detail)))
            estado = QTableWidgetItem("VENCIDO" if due else "OK")
            if due:
                estado.setForeground(QColor('red'))
            self.maint_table.setItem(row, 3, estado)
            btn = QPushButton("Marcar")
            if item.code <= 32:
                btn.clicked.connect(
                    lambda _=False, code=item.code: self._mark_service(code))
            else:
                btn.setEnabled(False)
            self.maint_table.setCellWidget(row, 4, btn)

    def _mark_service(self, code):
        self._pulse("mark-service-%d" % code)
        QTimer.singleShot(400, self._refresh_maintenance)

    # ------------------------------------------------------------------
    # configuracion
    # ------------------------------------------------------------------
    # ------------------------------------------------------------------
    # estaciones (tabla de herramientas estandar)
    # ------------------------------------------------------------------
    def _build_estaciones(self, page):
        layout = QVBoxLayout(page)
        try:
            from qtvcp.widgets.tool_offsetview import ToolOffsetView
            view = ToolOffsetView()
            try:
                view._hal_init()
            except Exception:
                pass
            layout.addWidget(view)
        except Exception as exc:
            layout.addWidget(QLabel(
                "Tabla de herramientas no disponible:\n%s\n\n"
                "Usa el editor de tool.tbl de tu pantalla." % exc))

    # ------------------------------------------------------------------
    # configuracion (asistente de pines + JSON)
    # ------------------------------------------------------------------
    def _build_config(self, page):
        layout = QVBoxLayout(page)

        mapbox = QGroupBox("Mapeo de pines")
        grid = QGridLayout(mapbox)
        self.pin_combos = {}
        self.pin_roles = {}
        for row, (key, label, role) in enumerate(PIN_ROLES):
            grid.addWidget(QLabel(label), row, 0)
            combo = QComboBox()
            combo.setEditable(True)
            combo.setMinimumWidth(260)
            grid.addWidget(combo, row, 1)
            self.pin_combos[key] = combo
            self.pin_roles[key] = role
        layout.addWidget(mapbox)

        maprow = QHBoxLayout()
        maprow.addWidget(self._button("Refrescar pines", self._refresh_pin_list))
        maprow.addWidget(self._button("Guardar mapeo", self._save_mapping))
        layout.addLayout(maprow)

        jsonbox = QGroupBox("Configuracion JSON (avanzada)")
        jbox = QVBoxLayout(jsonbox)
        self.config_view = QPlainTextEdit()
        jbox.addWidget(self.config_view, 1)
        row = QHBoxLayout()
        row.addWidget(self._button("Validar", self._validate_config))
        row.addWidget(self._button("Guardar", self._save_config))
        row.addWidget(self._button("Recargar", self._load_config_text))
        jbox.addLayout(row)
        layout.addWidget(jsonbox, 1)

        self._labels["config_status"] = QLabel("")
        layout.addWidget(self._labels["config_status"])
        self._load_config_text()
        self._sync_mapping_widgets()
        self._refresh_pin_list()

    def _all_pins(self):
        try:
            return hal.get_info_pins()
        except Exception as exc:
            LOG.error("turret panel: no se pueden listar los pines: %s" % exc)
            return []

    def _candidates(self, role, pins):
        result = []
        for info in pins:
            name = info.get("NAME", "")
            if not name or name.startswith(COMP + "."):
                continue
            direction = int(info.get("DIRECTION", 0))
            if role == "read" or role == "any":
                result.append(name)
            elif role == "write":
                if direction & HAL_IN:
                    result.append(name)
            elif role == "io_out":
                if name.startswith("iocontrol.0.") and (direction & HAL_OUT):
                    result.append(name)
            elif role == "io_in":
                if name.startswith("iocontrol.0.") and (direction & HAL_IN):
                    result.append(name)
        return sorted(set(result))

    def _sync_mapping_widgets(self):
        pins = (self.config.pins if self.config else {})
        for key, combo in getattr(self, "pin_combos", {}).items():
            combo.blockSignals(True)
            combo.setEditText(pins.get(key) or "")
            combo.blockSignals(False)

    def _refresh_pin_list(self):
        pins = self._all_pins()
        for key, combo in getattr(self, "pin_combos", {}).items():
            role = self.pin_roles[key]
            current = combo.currentText().strip()
            combo.blockSignals(True)
            combo.clear()
            combo.addItem("")
            for name in self._candidates(role, pins):
                combo.addItem(name)
            combo.setEditText(current)
            combo.blockSignals(False)
        self._labels["config_status"].setText(
            "Pines detectados: %d. Elegi y guarda el mapeo." % len(pins))

    def _save_mapping(self):
        if not self.config:
            self._labels["config_status"].setText("Sin configuracion cargada")
            return
        for key, combo in self.pin_combos.items():
            value = combo.currentText().strip()
            self.config.pins[key] = value or None
        errors = self.config.validate()
        if errors:
            self._labels["config_status"].setText(
                "Errores: " + "; ".join(errors))
            return
        self.config.save(self.config_path)
        self._write_f2_conf()
        self._labels["config_status"].setText(
            "Mapeo guardado: el manager recarga en caliente "
            "(los pines nuevos se aplican sin reiniciar)")
        self._load_config_text()

    def _load_config_text(self):
        if not self.config_path or not os.path.exists(self.config_path):
            self._labels["config_status"].setText("No se encontro turret.json")
            return
        with open(self.config_path, "r", encoding="utf-8") as fp:
            self.config_view.setPlainText(fp.read())
        self._labels["config_status"].setText("Configuracion: %s" % self.config_path)

    def _parse_config_text(self):
        try:
            data = json.loads(self.config_view.toPlainText())
        except ValueError as exc:
            self._labels["config_status"].setText("JSON invalido: %s" % exc)
            return None
        cfg = TurretConfig.from_dict(data)
        errors = cfg.validate()
        if errors:
            self._labels["config_status"].setText(
                "Errores: " + "; ".join(errors))
            return None
        return cfg

    def _write_f2_conf(self):
        """Export turret.conf for the compiled F2 component."""
        if not self.config_path:
            return None
        try:
            from turret import genconf
            return genconf.write(self.config_path)
        except Exception as exc:
            LOG.error("turret panel: no se puede generar el .conf F2: %s" % exc)
            return None

    def _validate_config(self):
        cfg = self._parse_config_text()
        if cfg is not None:
            self._labels["config_status"].setText("Configuracion valida")

    def _save_config(self):
        cfg = self._parse_config_text()
        if cfg is None:
            return
        cfg.save(self.config_path)
        self.config = cfg
        self._sync_mapping_widgets()
        self._write_f2_conf()
        self._labels["config_status"].setText(
            "Guardado: el manager recarga en caliente (los cambios de pines "
            "o estaciones requieren re-referenciar)")
        self._refresh_maintenance()

    # ------------------------------------------------------------------
    # live update
    # ------------------------------------------------------------------
    def _update(self):
        for key, led in self._leds.items():
            self._set_led(led, bool(self._machine(key, 0)))
        state = int(self._pin("state", 0))
        self._labels["state"].setText(STATE_TEXT.get(state, str(state)))
        self._labels["station"].setText(str(self._pin("station", 0)))
        self._labels["homed"].setText("si" if self._pin("homed", 0) else "no")
        self._labels["located"].setText("si" if self._pin("located", 0) else "no")
        self._labels["counts"].setText("%d / %d" % (
            self._pin("count-changes", 0), self._pin("count-clamp", 0)))
        fault = bool(self._pin("fault", 0))
        error = int(self._pin("error", 0))
        if fault:
            self._labels["alarm"].setText(alarm_text(error))
            self._labels["alarm"].setStyleSheet("color: red; font-weight: bold;")
        else:
            self._labels["alarm"].setText("Sin alarma")
            self._labels["alarm"].setStyleSheet("")
        self._labels["maint_counts"].setText("%d / %d / %.1f h" % (
            self._pin("count-changes", 0), self._pin("count-clamp", 0),
            self._pin("hours", 0.0)))
        if self._pin("maint-due", 0):
            code = int(self._pin("maint-code", 0))
            self._labels["maint_due"].setText("MANTENIMIENTO VENCIDO (M%d)" % code)
            self._labels["maint_due"].setStyleSheet("color: orange; font-weight: bold;")
        else:
            self._labels["maint_due"].setText("OK")
            self._labels["maint_due"].setStyleSheet("")
        self._ticks += 1
        if self._ticks % 25 == 0:
            self._refresh_maintenance()

    # ------------------------------------------------------------------
    def before_loop(self):
        pass

    def after_loop(self):
        pass

    def process_key(self, event):
        return False

    def closing_cleanup__(self):
        if hasattr(self, "_timer"):
            self._timer.stop()
