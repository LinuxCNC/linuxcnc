# EtherCAT Distributed Clock Synchronisation

## Table of Contents

1. [Introduction](#1-introduction)
2. [Architecture Overview](#2-architecture-overview)
3. [Servo-Cycle Call Sequence](#3-servo-cycle-call-sequence)
4. [M2R Mode — PI Controller Details](#4-m2r-mode--pi-controller-details)
5. [R2M Mode Details](#5-r2m-mode-details)
6. [XML Configuration](#6-xml-configuration)
7. [HAL Pins](#7-hal-pins)
8. [Key Data Structures](#8-key-data-structures)
9. [Troubleshooting](#9-troubleshooting)
10. [Source File Reference](#10-source-file-reference)

---

## 1. Introduction

EtherCAT **Distributed Clocks (DC)** provide a hardware-level mechanism for synchronising the internal clocks of all EtherCAT slaves on a bus to a common time base. Each DC-capable slave contains a 64-bit clock register that is continuously adjusted by the EtherCAT master to stay aligned with a designated *reference clock slave* — typically the first DC-capable device on the bus. The correction propagates through the ring in a single frame so that all slaves share a time base accurate to within tens of nanoseconds of each other.

For motion-control applications this matters critically:

- **Deterministic output timing**: Drives, I/O and encoder devices all latch or update their process data at the same instant in time. Without DC sync, jitter in each slave's local clock accumulates independently, causing positional errors and resonance in coordinated multi-axis moves.
- **Alignment with the servo thread**: LinuxCNC's servo thread fires at a fixed interval controlled by the RTAPI real-time layer. For optimal performance the EtherCAT application time — the timestamp stamped on every outgoing frame — must stay tightly aligned with that interval.

The DC sync subsystem in `linuxcnc-ethercat` solves both problems: it keeps the EtherCAT network's shared time base synchronised with the LinuxCNC servo thread, and it distributes that common reference to all participating slaves every cycle.

---

## 2. Architecture Overview

Two synchronisation modes are available. Both set the EtherCAT *application time* each servo cycle and trigger `ecrt_master_sync_slave_clocks()` to distribute corrections to every DC slave; they differ in *which clock follows which*.

### 2.1 M2R — Master-to-Reference Mode

> **Requires** LinuxCNC `RTAPI_TASK_PLL_SUPPORT`.

In M2R mode the **LinuxCNC RTAPI task** is the authoritative time source. A discrete-time PI controller continuously measures the phase difference between the LinuxCNC application time and the EtherCAT reference clock, then drives `rtapi_task_pll_set_correction()` to nudge the RTAPI task's own wakeup time so that the two clocks converge. Because the RTAPI task fires at exactly the corrected interval, the EtherCAT reference clock and the servo thread are locked together in a closed-loop phase-locked loop (PLL).

```
  LinuxCNC RTAPI task
        │
        ▼
  cycle_start()  ─── sets app_time_ns (predicted from RTAPI reference)
        │
  pre_send()     ─── reads reference clock, computes phase error, triggers slave sync
        │
  ecrt_master_send()
        │
  post_send()    ─── runs PI controller ──► rtapi_task_pll_set_correction()
                                                    │
                                            adjusts RTAPI task wakeup
                                                    │
                                        (feeds back into next cycle_start)
```

M2R is the recommended mode when PLL support is available because it achieves sub-microsecond phase alignment.

### 2.2 R2M — Reference-to-Master Mode

In R2M mode the direction is reversed: the LinuxCNC master *tells* the EtherCAT reference clock what time it is every `ref_clock_sync_cycles` cycles via `ecrt_master_sync_reference_clock_to()`. There is no closed-loop correction of the RTAPI task itself; the RTAPI timer fires on its own schedule and the EtherCAT reference clock is periodically snapped to it. This is a simpler, open-loop scheme that works without PLL support.

```
  LinuxCNC RTAPI task
        │
        ▼
  cycle_start()  ─── sets app_time_ns (reads fresh from RTAPI reference)
        │
  pre_send()     ─── every N cycles: ecrt_master_sync_reference_clock_to()
        │                            always: ecrt_master_sync_slave_clocks()
  ecrt_master_send()
        │
  post_send()    ─── (no-op)
```

### 2.3 Mode Selection

The mode is selected by the **sign** of the `refClockSyncCycles` configuration attribute on the `<master>` element:

| `refClockSyncCycles` value | Mode selected |
|---|---|
| positive integer (e.g. `10`) | R2M — sync reference clock every N cycles |
| negative integer (e.g. `-1`) | M2R — PLL-based, requires `RTAPI_TASK_PLL_SUPPORT` |
| `0` | DC sync disabled |

When LinuxCNC does **not** provide `RTAPI_TASK_PLL_SUPPORT`, M2R is silently downgraded to R2M regardless of the configured value.

---

## 3. Servo-Cycle Call Sequence

Both modes register three callbacks that are invoked from the LinuxCNC real-time thread each servo cycle. The sequence is:

```
lcec_read_master()
  └─ dcsync_callbacks.cycle_start(master)
  └─ ecrt_master_receive()
  └─ ecrt_domain_process()
  └─ [slave read callbacks]

lcec_write_master()
  └─ [slave write callbacks]
  └─ ecrt_domain_queue()
  └─ dcsync_callbacks.pre_send(master)
  └─ ecrt_master_send()
  └─ dcsync_callbacks.post_send(master)
```

### 3.1 `cycle_start()`

Called at the very start of `lcec_read_master()`, *before* receiving process data, so that the application time is set before any EtherCAT interaction.

- **M2R**: Reads `rtapi_task_pll_get_reference()` once to seed the time base on the first valid cycle (when `dc_started` is zero), then in all subsequent cycles increments the stored `app_time_ns` by exactly one servo period. This produces a smooth, jitter-free application time even if the RTAPI timer fires slightly late.
- **R2M**: Reads `rtapi_task_pll_get_reference()` (or `rtapi_get_time()` when unavailable) fresh every cycle. Because there is no prediction, any jitter in the RTAPI timer directly appears in the application time.

Both modes write the resulting timestamp to the EtherCAT master via `ecrt_master_application_time()`.

### 3.2 `pre_send()`

Called immediately before `ecrt_master_send()`, at the point of maximum relevance for the outgoing frame timestamp.

- **M2R**: Reads the 32-bit hardware counter from the reference clock slave and unwraps it to a 64-bit nanosecond value. Computes the phase error `diff = app_time_ns − ref_time_ns`. If `|diff|` exceeds 1.5 servo periods, the time base is re-snapped to the nearest whole-period boundary ("re-snap" logic). Always calls `ecrt_master_sync_slave_clocks()` to distribute corrections to all DC slaves.
- **R2M**: Decrements an internal counter; when it reaches zero, calls `ecrt_master_sync_reference_clock_to()` with the *current* `rtapi_get_time()` value (not the application time) so that any processing delay accumulated since `cycle_start()` is compensated. Always calls `ecrt_master_sync_slave_clocks()`.

### 3.3 `post_send()`

Called after `ecrt_master_send()` returns, using the latency of the frame round-trip as quiet time for computation.

- **M2R**: Runs the PI controller (see [Section 4](#4-m2r-mode--pi-controller-details)) using the phase error computed in `pre_send()`, then applies the resulting correction via `rtapi_task_pll_set_correction()`. Updates the HAL pins `pll_err`, `pll_out`, and `pll_reset_cnt`.
- **R2M**: No-op.

---

## 4. M2R Mode — PI Controller Details

The M2R controller is a standard discrete-time PI designed for the phase-locked loop. Its gains are computed analytically from two physical parameters defined as constants in `src/dcsync_m2r.c`:

| Constant | Value | Meaning |
|---|---|---|
| `DC_SETTLE_TIME` | 1.5 s | Target settling time (to within 2 %) |
| `DC_DAMPING` | 0.707 | Damping ratio (critically damped / Butterworth) |
| `DC_INTEGRATOR_MAX` | 1000 ns | Anti-windup integrator clamp |
| `DC_CORRECTION_MAX_NS` | 5000 ns | Safety clamp on the output correction |

### 4.1 Natural Frequency

The closed-loop bandwidth is derived from the settling-time specification:

```
wn = 4 / (DC_DAMPING × DC_SETTLE_TIME)
   = 4 / (0.707 × 1.5)
   ≈ 3.77 rad/s
```

### 4.2 Discrete-Time Gains

Given the servo period `T` in seconds (`app_time_period` converted to seconds):

```
wd = wn × T           (normalised frequency)
dc_kp = 2 × DC_DAMPING × wd
dc_ki = wd²
```

These gains are computed once at initialisation and stored in `lcec_master_t.dc_kp` and `lcec_master_t.dc_ki`.

### 4.3 Controller Update (post_send)

Each cycle `post_send()` performs:

```c
// Integrator update with anti-windup
integrator += dc_ki * error_ns;
if (integrator >  DC_INTEGRATOR_MAX) integrator =  DC_INTEGRATOR_MAX;
if (integrator < -DC_INTEGRATOR_MAX) integrator = -DC_INTEGRATOR_MAX;

// PI output
correction_ns = dc_kp * error_ns + integrator;

// Output safety clamp
if (correction_ns >  DC_CORRECTION_MAX_NS) correction_ns =  DC_CORRECTION_MAX_NS;
if (correction_ns < -DC_CORRECTION_MAX_NS) correction_ns = -DC_CORRECTION_MAX_NS;

rtapi_task_pll_set_correction((long)correction_ns);
```

### 4.4 32-bit Counter Unwrapping

The EtherCAT hardware exposes the reference clock as a 32-bit nanosecond counter that wraps every ~4.3 seconds. `pre_send()` unwraps this to a monotonic 64-bit value by detecting rollover events:

```c
uint32_t hw32 = (uint32_t)(master->dc_time_ns & 0xFFFFFFFF);
uint64_t upper = master->dc_time_ns & ~(uint64_t)0xFFFFFFFF;
if (new32 < hw32) upper += (uint64_t)1 << 32;   // rollover
master->dc_time_ns = upper | new32;
```

### 4.5 Re-snap on Large Phase Error

If the phase error exceeds 1.5 servo periods (the re-snap threshold), `app_time_ns` is corrected by the nearest whole number of periods so that the residual sub-period phase error is preserved. `pll_reset_cnt` is then incremented. Because the correction is a whole-period shift, the sub-period phase error remains continuous and the PI integrator state is deliberately kept intact — no integrator reset occurs.

---

## 5. R2M Mode Details

### 5.1 `cycle_start()`

Reads the current RTAPI reference time each cycle and converts it to EtherCAT application time using the globally computed `dc_time_offset`:

```c
app_time_ns = rtapi_task_pll_get_reference() + dc_time_offset;
ecrt_master_application_time(master->master, app_time_ns);
```

`dc_time_offset` is computed once at module load time as the difference between the wall-clock `gettimeofday()` and the monotonic RTAPI time, anchoring EtherCAT time to the same epoch as the system clock.

### 5.2 Periodic Reference Clock Sync

`pre_send()` maintains a countdown counter initialised to `ref_clock_sync_cycles`. Each call decrements it; when it reaches zero the counter is reset and the reference clock is synced:

```c
if (--master->ref_clock_sync_counter <= 0) {
    master->ref_clock_sync_counter = master->ref_clock_sync_cycles;
    uint64_t now = rtapi_get_time() + dc_time_offset;
    ecrt_master_sync_reference_clock_to(master->master, now);
}
ecrt_master_sync_slave_clocks(master->master);
```

Note that `rtapi_get_time()` is used here (rather than the stored `app_time_ns`) to account for any processing time elapsed since `cycle_start()`.

### 5.3 `post_send()`

R2M mode registers a no-op function for `post_send()`, so no computation is performed after frame transmission.

---

## 6. XML Configuration

### 6.1 Master-Level Attributes

The `<master>` element accepts the following DC-related attributes:

| Attribute | Type | Description |
|---|---|---|
| `appTimePeriod` | integer (ns) | **Required for DC.** The servo period that is advertised to the EtherCAT stack. Should match the LinuxCNC servo thread period. |
| `refClockSyncCycles` | integer | Mode selector and R2M sync divider (see [Section 2.3](#23-mode-selection)). |
| `refClockSlaveIdx` | integer | Bus position (0-based) of the slave to use as the DC reference clock. Default (`-1`) uses the first DC-capable slave. |

### 6.2 Slave-Level `<dcConf>` Element

Each slave that supports Distributed Clocks can have a `<dcConf>` child element:

| Attribute | Type | Description |
|---|---|---|
| `assignActivate` | hex | DC activation word from the slave's ESI file (e.g. `0x0300`). |
| `sync0Cycle` | integer (ns) or `*N` | SYNC0 period. Prefix with `*` followed by a positive integer to specify a multiple of `appTimePeriod` (e.g. `*1` = one master period, `*2` = double period). Fractional multiples are not supported. |
| `sync0Shift` | integer (ns) | SYNC0 phase shift relative to the reference clock. |
| `sync1Cycle` | integer (ns) or `*N` | SYNC1 period (same integer-only `*N` syntax supported). |
| `sync1Shift` | integer (ns) | SYNC1 phase shift relative to SYNC0. |

### 6.3 Example Configuration

```xml
<masters>
  <master idx="0"
          appTimePeriod="1000000"
          refClockSyncCycles="10">

    <slaves>

      <!-- EL7201 servo drive — DC enabled, one master period -->
      <slave idx="0" type="EL7201">
        <dcConf assignActivate="0x0700"
                sync0Cycle="*1"
                sync0Shift="0"
                sync1Cycle="0"
                sync1Shift="0"/>
      </slave>

      <!-- EL7211 servo drive — DC enabled, SYNC0 shifted by 200 µs -->
      <slave idx="1" type="EL7211">
        <dcConf assignActivate="0x0700"
                sync0Cycle="*1"
                sync0Shift="200000"
                sync1Cycle="0"
                sync1Shift="0"/>
      </slave>

    </slaves>
  </master>
</masters>
```

To use M2R mode (requires `RTAPI_TASK_PLL_SUPPORT`), set `refClockSyncCycles` to a negative value:

```xml
<master idx="0"
        appTimePeriod="1000000"
        refClockSyncCycles="-1">
```

---

## 7. HAL Pins

DC-sync-related HAL pins are created on each master instance. They are only present when LinuxCNC provides `RTAPI_TASK_PLL_SUPPORT`.

All pin names are prefixed with the component name and master index, e.g. `lcec.0.` for master 0.

| Pin name | Type | Direction | Description |
|---|---|---|---|
| `pll-err` | `s32` | OUT | Current phase error between application time and EtherCAT reference clock, in nanoseconds. Positive values mean the reference clock is behind. |
| `pll-out` | `s32` | OUT | Current PLL correction value being applied, in nanoseconds. |
| `pll-reset-cnt` | `u32` | OUT | Cumulative number of re-snap corrections applied when the phase error exceeded 1.5 servo periods. The PI integrator is not reset during a re-snap. |

These pins are useful for monitoring synchronisation quality. In a well-tuned system `pll-err` should settle to within a few hundred nanoseconds after startup.

---

## 8. Key Data Structures

### 8.1 `lcec_master_t` — DC-relevant fields (`src/lcec.h`)

| Field | Type | Description |
|---|---|---|
| `app_time_period` | `uint32_t` | Servo period advertised to the EtherCAT stack (nanoseconds). Set from `appTimePeriod` in the XML config. |
| `ref_clock_sync_cycles` | `int` | DC mode selector / R2M sync divider. Negative = M2R, positive = R2M with this period, zero = disabled. |
| `ref_clock_slave_idx` | `int` | Bus index of the DC reference slave, or `-1` for the default (first DC-capable slave). |
| `app_time_ns` | `uint64_t` | Application time written to the EtherCAT master this cycle (nanoseconds). |
| `ref_time_ns` | `uint64_t` | Reference time snapshot used for DC phase calculation. |
| `dc_time_ns` | `uint64_t` | Most recently read DC system time from the reference slave (nanoseconds, unwrapped 64-bit). |
| `dc_started` | `int` | Non-zero once the PI controller has been seeded from the first valid reference clock reading. |
| `dc_diff_ns` | `int64_t` | Current DC phase error (application time minus reference clock time), nanoseconds. |
| `dc_kp` | `double` | PI proportional gain (computed at init from period and settling-time parameters). |
| `dc_ki` | `double` | PI integral gain. |
| `dc_integrator` | `double` | PI integrator state (nanoseconds). |
| `ref_clock_sync_counter` | `int` | Countdown counter for R2M periodic reference sync. |
| `dcsync_callbacks` | `lcec_dcsync_callbacks_t` | Function pointers for `cycle_start`, `pre_send`, `post_send`. |
| `period_last` | `long` | Servo period measured in the previous cycle (nanoseconds). |

### 8.2 `lcec_dcsync_callbacks_t` (`src/lcec.h`)

```c
typedef struct {
    lcec_dcsync_callback_t cycle_start;  // Called at start of lcec_read_master()
    lcec_dcsync_callback_t pre_send;     // Called before ecrt_master_send()
    lcec_dcsync_callback_t post_send;    // Called after ecrt_master_send()
} lcec_dcsync_callbacks_t;
```

All three are function pointers with signature `void (*)(struct lcec_master *)`. The correct set of callbacks is installed by `lcec_dc_init_m2r()` or `lcec_dc_init_r2m()` during master initialisation.

### 8.3 `lcec_slave_dc_t` (`src/lcec.h`)

```c
typedef struct {
    uint16_t assignActivate; // DC activation word (ESI Table 60)
    uint32_t sync0Cycle;     // SYNC0 period in nanoseconds (0 = disabled)
    int32_t  sync0Shift;     // SYNC0 shift relative to reference (ns)
    uint32_t sync1Cycle;     // SYNC1 period in nanoseconds (0 = disabled)
    int32_t  sync1Shift;     // SYNC1 shift relative to SYNC0 (ns)
} lcec_slave_dc_t;
```

Populated from the `<dcConf>` XML element by the configuration parser and passed to `ecrt_slave_config_dc()` at master startup.

### 8.4 Global `dc_time_offset` (`src/priv.h`, `src/lcec_main.c`)

```c
extern int64_t dc_time_offset;
```

Computed once at module load as `EC_TIMEVAL2NANO(gettimeofday()) − rtapi_get_time()`. It anchors the EtherCAT application time to the same wall-clock epoch as the system clock, allowing EtherCAT timestamps and RTAPI monotonic time to be combined arithmetically.

---

## 9. Troubleshooting

### 9.1 `pll-err` stuck at a large constant value

The phase error is not converging. Likely causes:

- `appTimePeriod` does not match the actual LinuxCNC servo thread period. Verify both values in your HAL file (`loadrt [EMCMOT]EMCMOT servo_period_nsec=...`) and in the EtherCAT XML config.
- The DC reference slave is not being selected correctly. If multiple DC-capable slaves are present, try setting `refClockSlaveIdx` explicitly.
- The slave's `assignActivate` word is wrong. Consult the slave's ESI (XML device description) file; typical values are `0x0300` or `0x0700`.

### 9.2 `pll-err` oscillating at large amplitude

The PLL is not stable. In M2R mode this usually indicates:

- `DC_SETTLE_TIME` and `DC_DAMPING` were compiled for a different servo period than the one in use. Changing `appTimePeriod` requires recompilation.
- Kernel jitter is exceeding the PI controller's ability to compensate. Check for interrupt latency issues with `latency-test`.

In R2M mode, large oscillation suggests `refClockSyncCycles` is set too high (infrequent syncs allow clock drift to accumulate). Try reducing the value towards `1`.

### 9.3 `pll-reset-cnt` incrementing continuously

The phase error is repeatedly exceeding the re-snap threshold (1.5 × servo period). This typically indicates:

- Severe network latency or packet loss on the EtherCAT bus.
- A misconfigured `sync0Cycle` that is not a valid multiple of the master period.
- Application-level overruns causing the servo thread to miss cycles.

### 9.4 Tuning `refClockSyncCycles` (R2M mode)

Lower values mean more frequent reference clock updates, which reduces maximum clock drift but increases CPU overhead. Values between `1` (every cycle) and `10` (every ten cycles) are typical for a 1 ms servo period. Start at `1` and increase until `pll-err` becomes unacceptably large.

### 9.5 Enabling M2R mode

M2R requires a real-time kernel and LinuxCNC with `RTAPI_TASK_PLL_SUPPORT`. Verify this is available:

```bash
grep RTAPI_TASK_PLL_SUPPORT /usr/include/rtapi.h
```

If the symbol is absent, the build will silently fall back to R2M mode.

---

## 10. Source File Reference

| File | Purpose |
|---|---|
| `src/dcsync_m2r.c` | M2R mode implementation: PI controller, 32-bit counter unwrapping, PLL correction via `rtapi_task_pll_set_correction()` |
| `src/dcsync_r2m.c` | R2M mode implementation: periodic reference clock sync via `ecrt_master_sync_reference_clock_to()` |
| `src/priv.h` | Internal types, `dc_time_offset` global, `lcec_dc_init_r2m()` / `lcec_dc_init_m2r()` declarations |
| `src/lcec.h` | `lcec_master_t` struct with all DC fields; `lcec_slave_dc_t`; `lcec_dcsync_callbacks_t` |
| `src/lcec_main.c` | Module init (computes `dc_time_offset`), master startup (`ecrt_slave_config_dc()`, mode selection), real-time read/write dispatch |
| `src/lcec_conf.c` | XML parser: `<master appTimePeriod refClockSyncCycles>`, `<dcConf>` element, `parseSyncCycle()` helper |
