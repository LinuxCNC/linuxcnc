# Checklist: instalar la torreta en una máquina nueva

Imprimí esto y tildá cada punto. El detalle completo está en
`docs/src/gui/turret.adoc`.

## 1. Hardware

- [ ] Sensores PNP 24 V a las entradas de la Mesa: `pos1` (posición 1),
      `strobe` (pulso por estación), `changepos` (asentada, habilita
      pinza), `clamped` (pinza cerrada).
- [ ] Solenoide de giro por relé/SSR desde una salida de la Mesa
      (nunca directo), con diodo de flyback.
- [ ] Válvula de pinza por relé/SSR (ON = desbloquear).
- [ ] (Recomendado) Relé de watchdog en serie con la alimentación de las
      válvulas.
- [ ] Cadena de E-stop intacta: el software no reemplaza el hardware.

## 2. Archivos de la máquina

- [ ] Copiar `turret.json`, `turret.hal` y `tool.tbl` de esta carpeta al
      directorio de configuración de la máquina.
- [ ] Ajustar los pines en `turret.json` (o generarlo con el asistente
      del panel, pestaña *Configuracion*).
- [ ] Incluir `turret.hal` como `HALFILE` (o pegar sus líneas).
- [ ] Agregar la pestaña del panel en `[DISPLAY]`.

## 3. Nets que NO deben existir

- [ ] Sin `net` sobre `iocontrol.0.tool-prepared` ni `tool-changed`.
- [ ] Sin `net` sobre `iocontrol.0.toolchanger-fault` ni
      `toolchanger-reason`.
- [ ] Borrar `tool-prep-loop` / `tool-change-loop` si venían de un
      ejemplo viejo. La torreta maneja esos pines por nombre.
- Si quedan conectados, el panel muestra **E10** indicando el pin.

## 4. INI

- [ ] `[RS274NGC] LATHE_TXXXX = 1` (T0101 cambia sin M6).
- [ ] **Sin** `REMAP=T` (el remap gana y desactiva el T nativo; se avisa
      al arrancar).
- [ ] `[EMCIO] TOOL_TABLE = tool.tbl` y `RANDOM_TOOLCHANGER = 0`.
- [ ] Panel:
      `EMBED_TAB_NAME = Torreta`
      `EMBED_TAB_COMMAND = qtvcp turret`
      `EMBED_TAB_LOCATION = tabWidget_utilities`

## 5. Tabla de herramientas

- [ ] Cada herramienta con `P` = estación física: `T1 P1 ...`, `T2 P2 ...`
- [ ] Sin `P` la torreta no puede ubicarla: da **E10** y no prepara.
- [ ] Usar `T0101` (no `T10001`); las filas viejas `T10001..` se migran
      una vez a `WX/WZ` y quedan inertes.
- [ ] Offsets y desgaste por herramienta (`X Z WX WZ ...`).

## 6. Puesta en marcha

- [ ] Encender: el controlador arranca **sin referencia** (no sabe dónde
      está; es normal).
- [ ] Referenciar: botón **Referenciar** (no necesita modo servicio) o
      mantener **Avanzar** con modo servicio hasta que se active la
      posición 1 (queda referenciada al pasar).
- [ ] Con modo servicio, probar **Ir a N** en todas las estaciones y
      verificar `changepos` + `clamped`.
- [ ] Probar pinza: **Liberar** / **Meter**.
- [ ] MDI `T0101`: verificar que gira, prepara y cambia (pines
      `turret.prepared`/`changed`) y que aplica el offset (`#5081`,
      `#5083`).
- [ ] Verificar el watchdog: si se mata el manager, el relé de válvulas
      debe caer (motor y pinza sin presión).
- [ ] Probar una falla a propósito (desconectar un sensor) y ver el
      código E en el panel + historial, y que el reset exige
      re-referenciar.

## 7. Ajuste fino (en caliente desde el panel o `hal.set_p`)

- [ ] `strobe_filter_ms`: subir si el ruido hidráulico cuenta pulsos
      falsos.
- [ ] `settle_time` y `lead_pulses`: si la torreta se pasa o queda
      corta (el lead se adapta solo; el valor inicial es el que
      configurás).
- [ ] Timeouts: `unclamp/rotate/locate/clamp/home`.
- [ ] `jog_timeout` (10 s por defecto).
- [ ] Intervalos de mantenimiento (cambios/horas) y códigos.

## 8. Si algo no anda

| Síntoma | Causa probable |
|---|---|
| `T` no mueve | Torreta ya referenciada y herramienta cargada (no re-cambia), `P` faltante en la tabla, o nets de loopback (E10) |
| El cambio queda colgado | `tool-prepared/changed` con net (E10), o pin de iocontrol inexistente |
| Gira y se pasa | `lead_pulses` mal; bajar `decel` de la válvula; calibrar `settle_time` |
| Cuenta estaciones de más | Ruido en `strobe`: subir `strobe_filter_ms` |
| E5 (no asentada) | `changepos` desconectado o `locate_timeout` corto |
| Referencia no encontrada | `pos1`/`strobe` invertidos o cableado; ver LEDs de sensores en el panel |
| No gira (motor) | Salida por relé/SSR, fusible, o `interlock` (E8) |
| La máquina queda en error tras una falla | Reset en el panel; el fault es latch |

## 9. Datos a guardar de la máquina

- [ ] Copia de `turret.json`, `turret_state.json` (contadores) y
      `tool.tbl` en el backup de la máquina.
- [ ] Anotar estación → herramienta real y sentido de giro.
- [ ] Anotar el tipo de válvula/solenoide y el cableado usado.
