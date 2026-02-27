# EtherCAT Userspace Master Support for linuxcnc-ethercat

## Overview

This document describes the implementation plan for adding EtherCAT userspace
master support to `linuxcnc-ethercat` (branch `ec-master-uspace`), using the
userspace master API from `sittner/ethercat` (branch `uspace`).

## Background

### Kernel Master (current)

The existing code uses `ecrt_request_master(master->index)` to obtain a master
handle, passing only a numeric index. The kernel module manages the EtherCAT
hardware directly.

### Userspace Master (new, under `#ifdef EC_USPACE_MASTER`)

The `sittner/ethercat` `uspace` branch introduces a completely different
initialization path. When `EC_USPACE_MASTER` is defined in `ecrt.h`, the
old `ecrt_request_master()` / `ecrt_open_master()` APIs are **not available**.
Instead:

- `ecrt_lib_init(log_cb, socket_path)` — one-time library init
- `ecrt_startup_master(index, transport_type, interface, backup_interface, debug_level, run_on_cpu)` — replaces `ecrt_request_master(index)`
- `ecrt_release_master(master)` — unchanged name, different internal cleanup
- `ecrt_lib_cleanup()` — one-time library cleanup

## Design Principles

- **`EC_USPACE_MASTER` is defined by `ecrt.h`** from the uspace ethercat
  build — no manual define or build flag needed.
- All new XML attributes except `interface` are **optional with sensible
  defaults**.
- The compile-time guard `#ifdef EC_USPACE_MASTER` selects the code path;
  no runtime detection.
- IPC socket path is a **module parameter** on the `lcec` realtime component.

## XML Configuration

### Master Element

```xml
<!-- Full example with all attributes -->
<master idx="0" name="master0" appTimePeriod="1000000"
        interface="eth0" backupInterface="eth1"
        transportType="raw" debugLevel="1" runOnCpu="-1" />

<!-- Example using XDP native transport (requires HAVE_XDP) -->
<master idx="0" name="master0" appTimePeriod="1000000"
        interface="eth0" transportType="xdp-native" />

<!-- Minimal example (using defaults for optional attributes) -->
<master idx="0" name="master0" appTimePeriod="1000000" interface="eth0" />
```

### Attribute Reference

| Attribute         | Required | Default | Description |
|-------------------|----------|---------|-------------|
| `interface`       | **YES**  | —       | Primary network interface name (e.g. `"eth0"`) |
| `backupInterface` | no       | (none)  | Backup network interface name, or omit for none |
| `transportType`   | no       | `"raw"` | Transport type: `"raw"` (AF_PACKET raw socket), `"xdp-skb"` (AF_XDP generic/SKB mode), `"xdp-native"` (AF_XDP native driver mode). XDP types require the ethercat library built with `HAVE_XDP`. |
| `debugLevel`      | no       | `0`     | Debug verbosity level |
| `runOnCpu`        | no       | `-1`    | CPU affinity for master threads (`-1` = no binding) |

These attributes are only parsed when built against the uspace ethercat
library (i.e. `EC_USPACE_MASTER` is defined in `ecrt.h`). On kernel builds
they are silently ignored (unknown attribute error).

## Module Parameter

The `lcec` realtime module gains one new parameter on uspace builds:

```
loadrt lcec ipc_socket="/tmp/ethercat.sock"
```

| Parameter    | Default | Description |
|--------------|---------|-------------|
| `ipc_socket` | `NULL`  | Unix socket path for the EtherCAT IPC server. `NULL` = no tool access (the `ethercat` CLI tool cannot connect). |

## File Changes

### 1. `src/lcec_conf.h` — Configuration Data Structure

Add uspace-specific fields to `LCEC_CONF_MASTER_T`:

```c
typedef struct {
  LCEC_CONF_TYPE_T confType;
  int index;
  uint32_t appTimePeriod;
  int refClockSyncCycles;
  char name[LCEC_CONF_STR_MAXLEN];
#ifdef EC_USPACE_MASTER
  int transportType;                          // ec_transport_type_t, default: 0 (EC_TRANSPORT_RAW)
  char interface[LCEC_CONF_STR_MAXLEN];       // primary NIC — REQUIRED
  char backupInterface[LCEC_CONF_STR_MAXLEN]; // backup NIC or empty string
  unsigned int debugLevel;                    // default: 0
  int runOnCpu;                               // default: -1 (no binding)
#endif
} LCEC_CONF_MASTER_T;
```

### 2. `src/lcec.h` — Runtime Master Structure

Add matching runtime fields to `lcec_master_t`:

```c
typedef struct lcec_master {
  struct lcec_master *prev;
  struct lcec_master *next;
  int index;
#ifdef EC_USPACE_MASTER
  int transport_type;
  char interface[LCEC_CONF_STR_MAXLEN];
  char backup_interface[LCEC_CONF_STR_MAXLEN];
  unsigned int debug_level;
  int run_on_cpu;
#endif
  char name[LCEC_CONF_STR_MAXLEN];
  ec_master_t *master;
  unsigned long mutex;
  int pdo_entry_count;
  ec_pdo_entry_reg_t *pdo_entry_regs;
  ec_domain_t *domain;
  uint8_t *process_data;
  int process_data_len;
  struct lcec_slave *first_slave;
  struct lcec_slave *last_slave;
  lcec_master_data_t *hal_data;
  uint64_t app_time_base;
  uint32_t app_time_period;
  long period_last;
  int sync_ref_cnt;
  int sync_ref_cycles;
  long long state_update_timer;
  ec_master_state_t ms;
#ifdef RTAPI_TASK_PLL_SUPPORT
  uint64_t dc_ref;
  uint32_t app_time_last;
  int dc_time_valid_last;
#endif
} lcec_master_t;
```

### 3. `src/lcec_conf.c` — XML Configuration Parser

#### 3a. Set defaults and parse new attributes in `parseMasterAttrs()`

Before the attribute parsing loop, set defaults:

```c
  p->confType = lcecConfTypeMaster;
#ifdef EC_USPACE_MASTER
  p->transportType = 0;         // EC_TRANSPORT_RAW
  p->interface[0] = 0;
  p->backupInterface[0] = 0;
  p->debugLevel = 0;
  p->runOnCpu = -1;             // no CPU binding
#endif
```

Inside the `while (*attr)` loop, after existing attributes and before the
error handler:

```c
#ifdef EC_USPACE_MASTER
    // parse interface (required)
    if (strcmp(name, "interface") == 0) {
      strncpy(p->interface, val, LCEC_CONF_STR_MAXLEN);
      p->interface[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // parse backupInterface (optional)
    if (strcmp(name, "backupInterface") == 0) {
      strncpy(p->backupInterface, val, LCEC_CONF_STR_MAXLEN);
      p->backupInterface[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // parse transportType (optional, default: "raw")
    if (strcmp(name, "transportType") == 0) {
      if (strcmp(val, "raw") == 0) {
        p->transportType = EC_TRANSPORT_RAW;
#ifdef HAVE_XDP
      } else if (strcmp(val, "xdp-skb") == 0) {
        p->transportType = EC_TRANSPORT_XDP_SKB;
      } else if (strcmp(val, "xdp-native") == 0) {
        p->transportType = EC_TRANSPORT_XDP_NATIVE;
#endif
      } else {
        fprintf(stderr, "%s: ERROR: Unknown transportType '%s'"
            " (valid values: raw"
#ifdef HAVE_XDP
            ", xdp-skb, xdp-native"
#endif
            ")\n", modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }
      continue;
    }

    // parse debugLevel (optional, default: 0)
    if (strcmp(name, "debugLevel") == 0) {
      p->debugLevel = atoi(val);
      continue;
    }

    // parse runOnCpu (optional, default: -1)
    if (strcmp(name, "runOnCpu") == 0) {
      p->runOnCpu = atoi(val);
      continue;
    }
#endif
```

#### 3b. Validate after parsing

After the attribute loop completes, before the default name logic:

```c
#ifdef EC_USPACE_MASTER
  if (p->interface[0] == 0) {
    fprintf(stderr, "%s: ERROR: Master %d requires 'interface' attribute\n",
        modname, p->index);
    XML_StopParser(inst->parser, 0);
    return;
  }
#endif
```

### 4. `src/lcec_main.c` — Realtime Module (Core Changes)

#### 4a. Module parameter for IPC socket path

Near the existing static variables (around line 321):

```c
#ifdef EC_USPACE_MASTER
static char *ipc_socket = NULL;
RTAPI_MP_STRING(ipc_socket, "EtherCAT userspace master IPC socket path (NULL = no tool access)");
#endif
```

#### 4b. `rtapi_app_main()` — Library init and master startup

After `lcec_init_master_hal()` succeeds, before the master loop:

```c
#ifdef EC_USPACE_MASTER
  // initialize userspace ethercat master library
  if (ecrt_lib_init(NULL, ipc_socket) < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "ecrt_lib_init() failed\n");
    goto fail2;
  }
#endif
```

Replace the master initialization section:

```c
  // initialize masters
  for (master = first_master; master != NULL; master = master->next) {
#ifdef EC_USPACE_MASTER
    // startup userspace master
    master->master = ecrt_startup_master(
        master->index,
        (ec_transport_type_t) master->transport_type,
        master->interface,
        master->backup_interface[0] ? master->backup_interface : NULL,
        master->debug_level,
        (unsigned int) master->run_on_cpu);
    if (!master->master) {
      rtapi_print_msg(RTAPI_MSG_ERR,
          LCEC_MSG_PFX "startup of master %s (index %d, iface %s) failed\n",
          master->name, master->index, master->interface);
      goto fail2;
    }
#else
    // request kernel ethercat master
    if (!(master->master = ecrt_request_master(master->index))) {
      rtapi_print_msg(RTAPI_MSG_ERR,
          LCEC_MSG_PFX "requesting master %s (index %d) failed\n",
          master->name, master->index);
      goto fail2;
    }
#ifdef __KERNEL__
    // register callbacks
    ecrt_master_callbacks(master->master, lcec_request_lock, lcec_release_lock, master);
#endif
#endif /* EC_USPACE_MASTER */

    // create domain (unchanged — works in both modes)
    if (!(master->domain = ecrt_master_create_domain(master->master))) {
```

#### 4c. `lcec_parse_config()` — Copy uspace fields to runtime struct

In the `lcecConfTypeMaster` case, after copying `sync_ref_cycles`:

```c
        master->index = master_conf->index;
        strncpy(master->name, master_conf->name, LCEC_CONF_STR_MAXLEN);
        master->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
        master->app_time_period = master_conf->appTimePeriod;
        master->sync_ref_cycles = master_conf->refClockSyncCycles;
#ifdef EC_USPACE_MASTER
        master->transport_type = master_conf->transportType;
        strncpy(master->interface, master_conf->interface, LCEC_CONF_STR_MAXLEN);
        master->interface[LCEC_CONF_STR_MAXLEN - 1] = 0;
        strncpy(master->backup_interface, master_conf->backupInterface, LCEC_CONF_STR_MAXLEN);
        master->backup_interface[LCEC_CONF_STR_MAXLEN - 1] = 0;
        master->debug_level = master_conf->debugLevel;
        master->run_on_cpu = master_conf->runOnCpu;
#endif
```

#### 4d. `rtapi_app_exit()` — Add library cleanup

```c
void rtapi_app_exit(void) {
  lcec_master_t *master;

  // deactivate all masters
  for (master = first_master; master != NULL; master = master->next) {
    ecrt_master_deactivate(master->master);
  }

  lcec_clear_config();

#ifdef EC_USPACE_MASTER
  ecrt_lib_cleanup();
#endif

  hal_exit(comp_id);
}
```

#### 4e. `lcec_clear_config()` — No change needed

The existing code already calls `ecrt_release_master(master->master)` which
works correctly in both kernel and uspace modes.

## Files NOT Modified

- **`src/realtime.mk`** — `EC_USPACE_MASTER` propagates from `ecrt.h`;
  `-lethercat` link flag already present.
- **`src/user.mk`** — Same; `lcec_conf.c` includes `lcec_conf.h` which
  includes `ecrt.h`.
- **`configure.mk`** — No detection needed; the define comes from `ecrt.h`.
- **`src/Kbuild`** — Uspace builds use the `else` (non-kbuild) branch in
  `realtime.mk`.
- **All slave driver files** (`lcec_el*.c`, `lcec_ax*.c`, etc.) — They do
  not touch master initialization.
- **`src/lcec_rtapi.h`**, **`src/lcec_rtapi_user.h`**,
  **`src/lcec_rtapi_kmod.h`** — No changes required.

## Summary of Changes

| File | Change | Scope |
|------|--------|-------|
| `src/lcec_conf.h` | Add 5 fields to `LCEC_CONF_MASTER_T` | `#ifdef EC_USPACE_MASTER` |
| `src/lcec.h` | Add 5 fields to `lcec_master_t` | `#ifdef EC_USPACE_MASTER` |
| `src/lcec_conf.c` | Parse new XML attrs, set defaults, validate `interface` | `#ifdef EC_USPACE_MASTER` |
| `src/lcec_main.c` | Module param `ipc_socket`; `ecrt_lib_init()` + `ecrt_startup_master()` in init; copy uspace fields in `lcec_parse_config()`; `ecrt_lib_cleanup()` in exit | `#ifdef EC_USPACE_MASTER` |

## API Mapping

| Kernel Master API | Userspace Master API | Where Called |
|-------------------|----------------------|--------------|
| — | `ecrt_lib_init(log_cb, socket_path)` | `rtapi_app_main()`, once before master loop |
| `ecrt_request_master(index)` | `ecrt_startup_master(index, transport_type, interface, backup_interface, debug_level, run_on_cpu)` | `rtapi_app_main()`, per master |
| `ecrt_master_create_domain()` | `ecrt_master_create_domain()` | unchanged |
| `ecrt_master_activate()` | `ecrt_master_activate()` | unchanged |
| `ecrt_master_deactivate()` | `ecrt_master_deactivate()` | unchanged |
| `ecrt_release_master()` | `ecrt_release_master()` | `lcec_clear_config()`, unchanged |
| — | `ecrt_lib_cleanup()` | `rtapi_app_exit()`, once after `lcec_clear_config()` |
