# LinuxCNC Server Migration - Phase 1 Implementation Document

## Executive Summary

This document describes the implementation of a Go-based LinuxCNC server that consolidates the current multi-process architecture into a single server process while maintaining full backward compatibility through preserved NML communication.

**Goal:** Replace the `linuxcnc` startup script and multiple processes with a single `linuxcnc-server` binary that orchestrates all non-UI components.

**Compatibility:** All existing UIs (AXIS, gmoccapy, QtVCP, etc.) continue to work unchanged via NML.

**HAL-only mode:** When `[TASK]TASK` is not set in the INI file, the launcher starts only the
realtime environment and HAL components — no NML server, no iocontrol, no task controller.
This enables machines that only need HAL-based automation without the full CNC stack.

---

## 1. Architecture Overview

### 1.1 Current Architecture (Before)

```
linuxcnc (bash script)
    │
    ├── rtapi_app (process)     ← RT thread management
    │       │
    │       └── loads: motmod.so, tpmod.so, [kins].so, [hal comps]
    │
    ├── halcmd (process)        ← HAL configuration
    │
    ├── milltask (process)      ← Task controller
    │       │
    │       └── NML channels ←──────────────┐
    │                                        │
    ├── iocontrol (process)     ← IO controller
    │       │                                │
    │       └── NML channels ←──────────────┤
    │                                        │
    └── [UI process]            ← User interface
            │                                │
            └── NML channels ←──────────────┘
```

### 1.2 Target Architecture (After Phase 1)

```
linuxcnc-server (single Go process)
    │
    ├── [embedded] RTAPI initialization
    │       │
    │       └── loads: motmod.so, tpmod.so, [kins].so, [hal comps]
    │
    ├── [embedded] HAL configuration loader
    │
    ├── [goroutine] Task controller thread
    │       │
    │       └── NML channels ←──────────────┐  (preserved!)
    │                                        │
    ├── [goroutine] IO controller thread     │
    │       │                                │
    │       └── NML channels ←──────────────┤
    │                                        │
    └── [external] UI process    (unchanged) │
            │                                │
            └── NML channels ←──────────────┘
```

### 1.3 Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Server language | Go | Modern, excellent concurrency, easy networking for future API |
| C/C++ integration | cgo with C shims | Clean boundary, handles C++ name mangling |
| NML | Preserved | Backward compatibility with all existing UIs |
| Task/IOControl | Linked as libraries | Minimal changes to existing code |
| Configuration | Go parses INI, passes to components | Centralized config handling |
| HAL loading | Execute existing HAL file commands | Reuse proven mechanism |
| Conditional startup | Components started only when configured | Enables HAL-only mode |

---

## 1.4 Conditional Startup and HAL-only Mode

The launcher supports two operational modes determined entirely by the INI file:

### Full CNC Mode

When `[TASK]TASK` is set in the INI file, all CNC components are started:

```
INI file has [TASK]TASK
    │
    ├── linuxcncsvr (NML server) — started before realtime
    ├── Realtime environment (rtapi_app)
    ├── iocontrol (only if [TASK]TASK is set)
    ├── halui (only if [HAL]HALUI is set)
    ├── tpmod / homemod (trajectory + homing modules)
    ├── [HAL]HALFILE entries
    ├── Task controller ([TASK]TASK)
    ├── [HAL]HALCMD entries
    ├── retain (if retained signals exist)
    ├── HAL threads (halcmd start)
    ├── [APPLICATIONS]APP entries
    └── Display ([DISPLAY]DISPLAY) — if set, runs in foreground; else HAL-only wait
```

### HAL-only Mode

When `[TASK]TASK` is **not** set, only the realtime/HAL stack is started:

```
INI file has NO [TASK]TASK
    │
    ├── Realtime environment (rtapi_app)
    ├── (no iocontrol, no tpmod/homemod, no linuxcncsvr, no task controller)
    ├── [HAL]HALFILE entries
    ├── [HAL]HALCMD entries
    ├── retain (if retained signals exist)
    ├── HAL threads (halcmd start)
    ├── [APPLICATIONS]APP entries
    └── Wait for SIGINT/SIGTERM (Ctrl+C) — no display launched
```

A custom UI that communicates via HAL pins directly (e.g., a Python script using `hal` module,
or a Go program using `hal-go`) can run alongside the launcher in HAL-only mode.

### Component Startup Rules

| Component | Condition | Notes |
|-----------|-----------|-------|
| `linuxcncsvr` | `[TASK]TASK` set | NML server only needed with task controller |
| `iocontrol` | `[TASK]TASK` set | IO communicates with task via NML |
| `halui` | `[HAL]HALUI` set | Already conditional; `[TASK]TASK` required if set |
| `tpmod` / `homemod` | `[TASK]TASK` set | Motion modules only needed with task |
| Task controller | `[TASK]TASK` set | |
| Display | `[DISPLAY]DISPLAY` set | If not set → HAL-only wait loop |

### Dependency Validation

The launcher validates cross-section INI dependencies at startup and rejects contradictory
configurations with clear error messages before starting any processes:

| Configuration | Result |
|---------------|--------|
| No `[HAL]HALFILE` | ❌ Error: at least one `[HAL]HALFILE` required |
| `[HAL]HALUI` without `[TASK]TASK` | ❌ Error: halui requires task controller |
| `[EMCIO]EMCIO` or `[IO]IO` without `[TASK]TASK` | ❌ Error: iocontrol requires task controller |
| `[TASK]TASK` without `[KINS]KINEMATICS` | ❌ Error: kinematics required |
| `[TASK]TASK` without `[TRAJ]COORDINATES` | ❌ Error: trajectory config required |
| `[TASK]TASK` without `[EMCMOT]SERVO_PERIOD` | ❌ Error: motion config required |
| `[TASK]TASK` without `[RS274NGC]PARAMETER_FILE` | ❌ Error: G-code interpreter config required |

### Example: Minimal HAL-only INI File

```ini
[EMC]
MACHINE = MyHALMachine

[HAL]
HALFILE = my-hardware.hal
HALFILE = my-logic.hal

# No [TASK], no [DISPLAY], no [EMCIO]
# → linuxcnc runs in HAL-only mode
# → custom UI connects via HAL pins or the hal-go/Python hal API
```

---

## 2. Component Specifications

### 2.1 Directory Structure

```
linuxcnc/
├── src/
│   ├── server/                      # NEW: Go server
│   │   ├── main.go                  # Entry point
│   │   ├── go.mod                   # Go module definition
│   │   ├── go.sum                   # Dependencies
│   │   │
│   │   ├── config/                  # Configuration handling
│   │   │   ├── config.go            # Main config structures
│   │   │   ├── ini.go               # INI file parsing
│   │   │   └── validate.go          # Config validation
│   │   │
│   │   ├── rtapi/                   # RTAPI integration
│   │   │   ├── rtapi.go             # Go wrapper
│   │   │   └── rtapi_cgo.go         # cgo bindings
│   │   │
│   │   ├── hal/                     # HAL integration
│   │   │   ├── hal.go               # Go wrapper
│   │   │   ├── hal_cgo.go           # cgo bindings
│   │   │   └── loader.go            # HAL file loader
│   │   │
│   │   ├── task/                    # Task controller integration
│   │   │   ├── task.go              # Go wrapper
│   │   │   └── task_cgo.go          # cgo bindings
│   │   │
│   │   ├── iocontrol/               # IO controller integration
│   │   │   ├── iocontrol.go         # Go wrapper
│   │   │   └── iocontrol_cgo.go     # cgo bindings
│   │   │
│   │   └── shim/                    # C shim layer (compiled with cgo)
│   │       ├── shim.h               # Common definitions
│   │       ├── rtapi_shim.c         # RTAPI C shim
│   │       ├── rtapi_shim.h
│   │       ├── hal_shim.c           # HAL C shim
│   │       ├── hal_shim.h
│   │       ├── task_shim.c          # Task C shim
│   │       ├── task_shim.h
│   │       ├── iocontrol_shim.c     # IOControl C shim
│   │       └── iocontrol_shim.h
│   │
│   ├── emc/                         # EXISTING: Minimal modifications
│   │   ├── task/
│   │   │   ├── emctaskmain.cc       # MODIFIED: Remove main(), add init/cycle/shutdown
│   │   │   ├── emccanon.cc          # UNCHANGED
│   │   │   ├── taskintf.cc          # UNCHANGED
│   │   │   ├── taskclass.cc         # UNCHANGED
│   │   │   └── Submakefile          # MODIFIED: Build as library
│   │   │
│   │   ├── iotask/
│   │   │   ├── ioControl.cc         # MODIFIED: Remove main(), add init/cycle/shutdown
│   │   │   └── Submakefile          # MODIFIED: Build as library
│   │   │
│   │   ├── motion/                  # UNCHANGED
│   │   ├── tp/                      # UNCHANGED
│   │   ├── kinematics/              # UNCHANGED
│   │   └── rs274ngc/                # UNCHANGED
│   │
│   ├── hal/                         # UNCHANGED
│   ├── rtapi/                       # UNCHANGED
│   └── libnml/                      # UNCHANGED
│
├── lib/                             # Built libraries
│   ├── libtask.so
│   ├── libiocontrol.so
│   └── ... (existing libs)
│
├── bin/
│   ├── linuxcnc-server              # NEW: Go server binary
│   └── ... (existing binaries)
│
└── Makefile                         # MODIFIED: Add server build target
```

### 2.2 Build Outputs

| Output | Type | Contents |
|--------|------|----------|
| `lib/libtask.so` | Shared library | Task controller (modified emctaskmain.cc) |
| `lib/libiocontrol.so` | Shared library | IO controller (modified ioControl.cc) |
| `bin/linuxcnc-server` | Executable | Go server with embedded shims |

---

## 3. Implementation Details

### 3.1 Go Server Main Entry Point

```go
// src/server/main.go
package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"linuxcnc/server/config"
	"linuxcnc/server/hal"
	"linuxcnc/server/iocontrol"
	"linuxcnc/server/rtapi"
	"linuxcnc/server/task"

	"golang.org/x/sync/errgroup"
)

var (
	Version   = "dev"
	BuildTime = "unknown"
)

func main() {
	// Command line flags
	iniFile := flag.String("ini", "", "Path to INI configuration file")
	version := flag.Bool("version", false, "Print version and exit")
	debug := flag.Bool("debug", false, "Enable debug output")
	flag.Parse()

	if *version {
		fmt.Printf("linuxcnc-server %s (built %s)\n", Version, BuildTime)
		os.Exit(0)
	}

	if *iniFile == "" {
		fmt.Fprintln(os.Stderr, "Error: -ini flag is required")
		fmt.Fprintln(os.Stderr, "Usage: linuxcnc-server -ini <config.ini>")
		os.Exit(1)
	}

	// Run server
	if err := run(*iniFile, *debug); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}

func run(iniFile string, debug bool) error {
	// ===== Step 1: Load and validate configuration =====
	cfg, err := config.Load(iniFile)
	if err != nil {
		return fmt.Errorf("failed to load config: %w", err)
	}

	if err := cfg.Validate(); err != nil {
		return fmt.Errorf("invalid config: %w", err)
	}

	if debug {
		cfg.Dump(os.Stdout)
	}

	// ===== Step 2: Setup signal handling =====
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		sig := <-sigCh
		fmt.Printf("\nReceived signal %v, shutting down...\n", sig)
		cancel()
	}()

	// ===== Step 3: Initialize RTAPI =====
	rt, err := rtapi.Init(rtapi.Config{
		InstanceName: cfg.EMC.MachineName,
		Debug:        debug,
	})
	if err != nil {
		return fmt.Errorf("rtapi init failed: %w", err)
	}
	defer rt.Shutdown()

	fmt.Println("RTAPI initialized")

	// ===== Step 4: Initialize HAL =====
	h, err := hal.Init(hal.Config{
		ComponentName: "linuxcnc",
	})
	if err != nil {
		return fmt.Errorf("hal init failed: %w", err)
	}
	defer h.Shutdown()

	fmt.Println("HAL initialized")

	// ===== Step 5: Load HAL configuration =====
	halLoader := hal.NewLoader(h, cfg)
	if err := halLoader.LoadFiles(cfg.HAL.Files); err != nil {
		return fmt.Errorf("hal config failed: %w", err)
	}

	fmt.Printf("Loaded %d HAL files\n", len(cfg.HAL.Files))

	// ===== Step 6: Initialize IO Controller =====
	ioc, err := iocontrol.Init(iocontrol.Config{
		IniFile:   iniFile,
		CycleTime: cfg.EMCIO.CycleTime,
	})
	if err != nil {
		return fmt.Errorf("iocontrol init failed: %w", err)
	}
	defer ioc.Shutdown()

	fmt.Println("IO Controller initialized")

	// ===== Step 7: Initialize Task Controller =====
	tsk, err := task.Init(task.Config{
		IniFile:   iniFile,
		CycleTime: cfg.Task.CycleTime,
	})
	if err != nil {
		return fmt.Errorf("task init failed: %w", err)
	}
	defer tsk.Shutdown()

	fmt.Println("Task Controller initialized")

	// ===== Step 8: Signal HAL ready =====
	if err := h.Ready(); err != nil {
		return fmt.Errorf("hal ready failed: %w", err)
	}

	fmt.Println("HAL ready")

	// ===== Step 9: Run main loops =====
	fmt.Println("LinuxCNC server running. Press Ctrl+C to stop.")

	g, gctx := errgroup.WithContext(ctx)

	// Task controller loop
	g.Go(func() error {
		return tsk.Run(gctx)
	})

	// IO controller loop
	g.Go(func() error {
		return ioc.Run(gctx)
	})

	// Wait for shutdown
	if err := g.Wait(); err != nil && err != context.Canceled {
		return fmt.Errorf("runtime error: %w", err)
	}

	fmt.Println("Shutdown complete")
	return nil
}
```

### 3.2 Go Module Definition

```go
// src/server/go.mod
module linuxcnc/server

go 1.21

require (
	golang.org/x/sync v0.6.0
	gopkg.in/ini.v1 v1.67.0
)
```

### 3.3 Configuration Package

```go
// src/server/config/config.go
package config

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
)

// Config holds the complete server configuration parsed from INI file
type Config struct {
	// Path to the INI file (for passing to legacy components)
	IniPath string

	// [EMC] section
	EMC struct {
		MachineName string  `ini:"MACHINE"`
		Debug       int     `ini:"DEBUG"`
		Version     string  `ini:"VERSION"`
	}

	// [DISPLAY] section - for reference, UI handles this
	Display struct {
		Display         string  `ini:"DISPLAY"`
		CycleTime       float64 `ini:"CYCLE_TIME"`
		MaxFeedOverride float64 `ini:"MAX_FEED_OVERRIDE"`
	}

	// [TASK] section
	Task struct {
		CycleTime float64 `ini:"CYCLE_TIME"`
	}

	// [RS274NGC] section
	RS274NGC struct {
		ParameterFile  string `ini:"PARAMETER_FILE"`
		SubroutinePath string `ini:"SUBROUTINE_PATH"`
	}

	// [EMCMOT] section
	EMCMOT struct {
		ServoPeriod float64 `ini:"SERVO_PERIOD"`
		BasePeriod  float64 `ini:"BASE_PERIOD"`
		CommTimeout float64 `ini:"COMM_TIMEOUT"`
	}

	// [EMCIO] section
	EMCIO struct {
		CycleTime float64 `ini:"CYCLE_TIME"`
		ToolTable string  `ini:"TOOL_TABLE"`
	}

	// [HAL] section
	HAL struct {
		Files        []string `ini:"HALFILE,omitempty,allowshadow"`
		PostGUIFile  []string `ini:"POSTGUI_HALFILE,omitempty,allowshadow"`
		ShutdownFile string   `ini:"SHUTDOWN"`
	}

	// [TRAJ] section
	Traj struct {
		Coordinates     string  `ini:"COORDINATES"`
		LinearUnits     string  `ini:"LINEAR_UNITS"`
		AngularUnits    string  `ini:"ANGULAR_UNITS"`
		MaxVelocity     float64 `ini:"MAX_VELOCITY"`
		MaxAcceleration float64 `ini:"MAX_ACCELERATION"`
	}

	// [KINS] section
	Kins struct {
		Kinematics string `ini:"KINEMATICS"`
		Joints     int    `ini:"JOINTS"`
	}

	// Raw INI data for sections we pass through unchanged
	raw *iniFile
}

// Validate checks the configuration for required fields and valid values
func (c *Config) Validate() error {
	// Check required fields
	if c.EMC.MachineName == "" {
		c.EMC.MachineName = "LinuxCNC"
	}

	// Validate cycle times
	if c.Task.CycleTime <= 0 {
		c.Task.CycleTime = 0.010 // 10ms default
	}

	if c.EMCIO.CycleTime <= 0 {
		c.EMCIO.CycleTime = 0.100 // 100ms default
	}

	// Validate HAL files exist
	iniDir := filepath.Dir(c.IniPath)
	for _, f := range c.HAL.Files {
		path := f
		if !filepath.IsAbs(path) {
			path = filepath.Join(iniDir, f)
		}
		if _, err := os.Stat(path); err != nil {
			return fmt.Errorf("HAL file not found: %s", f)
		}
	}

	// Validate kinematics specified
	if c.Kins.Kinematics == "" {
		return fmt.Errorf("[KINS]KINEMATICS is required")
	}

	if c.Kins.Joints <= 0 {
		return fmt.Errorf("[KINS]JOINTS must be > 0")
	}

	return nil
}

// Dump writes the configuration to the given writer for debugging
func (c *Config) Dump(w io.Writer) {
	fmt.Fprintf(w, "=== Configuration ===\n")
	fmt.Fprintf(w, "INI File: %s\n", c.IniPath)
	fmt.Fprintf(w, "Machine: %s\n", c.EMC.MachineName)
	fmt.Fprintf(w, "Task Cycle: %.3fs\n", c.Task.CycleTime)
	fmt.Fprintf(w, "IO Cycle: %.3fs\n", c.EMCIO.CycleTime)
	fmt.Fprintf(w, "Kinematics: %s\n", c.Kins.Kinematics)
	fmt.Fprintf(w, "Joints: %d\n", c.Kins.Joints)
	fmt.Fprintf(w, "HAL Files: %v\n", c.HAL.Files)
	fmt.Fprintf(w, "=====================\n")
}

// GetSection returns raw INI section data for legacy component compatibility
func (c *Config) GetSection(name string) map[string]string {
	if c.raw == nil {
		return nil
	}
	return c.raw.GetSection(name)
}
```

```go
// src/server/config/ini.go
package config

import (
	"fmt"
	"os"
	"path/filepath"

	"gopkg.in/ini.v1"
)

// iniFile wraps the raw INI data
type iniFile struct {
	*ini.File
}

// Load parses an INI file and returns a Config structure
func Load(path string) (*Config, error) {
	// Resolve absolute path
	absPath, err := filepath.Abs(path)
	if err != nil {
		return nil, fmt.Errorf("invalid path: %w", err)
	}

	// Check file exists
	if _, err := os.Stat(absPath); err != nil {
		return nil, fmt.Errorf("file not found: %s", absPath)
	}

	// Parse INI file
	iniOpts := ini.LoadOptions{
		AllowBooleanKeys:          true,
		AllowShadows:              true,
		IgnoreInlineComment:       false,
		UnescapeValueDoubleQuotes: true,
	}

	f, err := ini.LoadSources(iniOpts, absPath)
	if err != nil {
		return nil, fmt.Errorf("parse error: %w", err)
	}

	cfg := &Config{
		IniPath: absPath,
		raw:     &iniFile{f},
	}

	// Map sections to struct
	if err := f.MapTo(cfg); err != nil {
		return nil, fmt.Errorf("mapping error: %w", err)
	}

	// Handle HALFILE specially (can have multiple entries)
	halSection := f.Section("HAL")
	if halSection != nil {
		cfg.HAL.Files = halSection.Key("HALFILE").ValueWithShadows()
		cfg.HAL.PostGUIFile = halSection.Key("POSTGUI_HALFILE").ValueWithShadows()
	}

	return cfg, nil
}

// GetSection returns all key-value pairs from a section
func (f *iniFile) GetSection(name string) map[string]string {
	section := f.Section(name)
	if section == nil {
		return nil
	}

	result := make(map[string]string)
	for _, key := range section.Keys() {
		result[key.Name()] = key.String()
	}
	return result
}

// ExpandPath expands a path relative to the INI file directory
func (c *Config) ExpandPath(path string) string {
	if filepath.IsAbs(path) {
		return path
	}
	return filepath.Join(filepath.Dir(c.IniPath), path)
}
```

### 3.4 RTAPI Integration

```go
// src/server/rtapi/rtapi.go
package rtapi

/*
#cgo CFLAGS: -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../hal
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -llinuxcnchal -lrtapi_app

#include "rtapi_shim.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// Config holds RTAPI initialization parameters
type Config struct {
	InstanceName string
	Debug        bool
}

// RTAPI represents an initialized RTAPI instance
type RTAPI struct {
	config Config
	id     C.int
}

// Init initializes the RTAPI subsystem
func Init(cfg Config) (*RTAPI, error) {
	name := C.CString(cfg.InstanceName)
	defer C.free(unsafe.Pointer(name))

	ret := C.rtapi_shim_init(name)
	if ret < 0 {
		return nil, fmt.Errorf("rtapi_init failed with code %d", ret)
	}

	return &RTAPI{
		config: cfg,
		id:     ret,
	}, nil
}

// Shutdown cleanly shuts down RTAPI
func (r *RTAPI) Shutdown() error {
	ret := C.rtapi_shim_exit()
	if ret < 0 {
		return fmt.Errorf("rtapi_exit failed with code %d", ret)
	}
	return nil
}

// LoadModule loads a realtime module
func (r *RTAPI) LoadModule(name string, args string) error {
	cname := C.CString(name)
	cargs := C.CString(args)
	defer C.free(unsafe.Pointer(cname))
	defer C.free(unsafe.Pointer(cargs))

	ret := C.rtapi_shim_loadrt(cname, cargs)
	if ret < 0 {
		return fmt.Errorf("loadrt %s failed with code %d", name, ret)
	}
	return nil
}

// UnloadModule unloads a realtime module
func (r *RTAPI) UnloadModule(name string) error {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	ret := C.rtapi_shim_unloadrt(cname)
	if ret < 0 {
		return fmt.Errorf("unloadrt %s failed with code %d", name, ret)
	}
	return nil
}
```

```c
// src/server/shim/rtapi_shim.h
#ifndef RTAPI_SHIM_H
#define RTAPI_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RTAPI Shim Layer
 *
 * Provides a clean C interface for Go's cgo to call RTAPI functions.
 * Handles initialization, module loading, and shutdown.
 */

/* Initialize RTAPI subsystem
 * Returns: component ID on success, negative error code on failure
 */
int rtapi_shim_init(const char *instance_name);

/* Shutdown RTAPI subsystem
 * Returns: 0 on success, negative error code on failure
 */
int rtapi_shim_exit(void);

/* Load a realtime module
 * name: module name (e.g., "motmod")
 * args: module arguments (e.g., "servo_period_nsec=1000000")
 * Returns: 0 on success, negative error code on failure
 */
int rtapi_shim_loadrt(const char *name, const char *args);

/* Unload a realtime module
 * Returns: 0 on success, negative error code on failure
 */
int rtapi_shim_unloadrt(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* RTAPI_SHIM_H */
```

```c
// src/server/shim/rtapi_shim.c
#include "rtapi_shim.h"
#include "rtapi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int rtapi_id = -1;

int rtapi_shim_init(const char *instance_name)
{
    // Set instance name environment variable if needed
    if (instance_name && *instance_name) {
        setenv("INSTANCE", instance_name, 1);
    }

    // Initialize RTAPI
    rtapi_id = rtapi_init("linuxcnc-server");
    if (rtapi_id < 0) {
        fprintf(stderr, "rtapi_shim: rtapi_init failed: %d\n", rtapi_id);
        return rtapi_id;
    }

    return rtapi_id;
}

int rtapi_shim_exit(void)
{
    if (rtapi_id < 0) {
        return 0; // Not initialized
    }

    int ret = rtapi_exit(rtapi_id);
    rtapi_id = -1;
    return ret;
}

int rtapi_shim_loadrt(const char *name, const char *args)
{
    // This mimics what halcmd loadrt does
    // In practice, we may need to call into rtapi_app or use dlopen

    char cmd[1024];
    if (args && *args) {
        snprintf(cmd, sizeof(cmd), "loadrt %s %s", name, args);
    } else {
        snprintf(cmd, sizeof(cmd), "loadrt %s", name);
    }

    // For now, delegate to halcmd
    // TODO: Implement direct loading via rtapi_app interface
    char halcmd[1100];
    snprintf(halcmd, sizeof(halcmd), "halcmd %s", cmd);

    int ret = system(halcmd);
    return (ret == 0) ? 0 : -1;
}

int rtapi_shim_unloadrt(const char *name)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "halcmd unloadrt %s", name);

    int ret = system(cmd);
    return (ret == 0) ? 0 : -1;
}
```

### 3.5 HAL Integration

```go
// src/server/hal/hal.go
package hal

/*
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../../rtapi
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -llinuxcnchal

#include "hal_shim.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// Config holds HAL initialization parameters
type Config struct {
	ComponentName string
}

// HAL represents an initialized HAL instance
type HAL struct {
	config Config
	id     C.int
}

// Init initializes the HAL subsystem
func Init(cfg Config) (*HAL, error) {
	name := C.CString(cfg.ComponentName)
	defer C.free(unsafe.Pointer(name))

	ret := C.hal_shim_init(name)
	if ret < 0 {
		return nil, fmt.Errorf("hal_init failed with code %d", ret)
	}

	return &HAL{
		config: cfg,
		id:     ret,
	}, nil
}

// Shutdown cleanly shuts down HAL
func (h *HAL) Shutdown() error {
	ret := C.hal_shim_exit()
	if ret < 0 {
		return fmt.Errorf("hal_exit failed with code %d", ret)
	}
	return nil
}

// Ready signals that HAL setup is complete
func (h *HAL) Ready() error {
	ret := C.hal_shim_ready()
	if ret < 0 {
		return fmt.Errorf("hal_ready failed with code %d", ret)
	}
	return nil
}

// ExecuteCommand executes a HAL command string
func (h *HAL) ExecuteCommand(cmd string) error {
	ccmd := C.CString(cmd)
	defer C.free(unsafe.Pointer(ccmd))

	ret := C.hal_shim_execute_cmd(ccmd)
	if ret < 0 {
		return fmt.Errorf("hal command failed: %s", cmd)
	}
	return nil
}
```

```go
// src/server/hal/loader.go
package hal

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"linuxcnc/server/config"
)

// Loader handles loading HAL configuration files
type Loader struct {
	hal    *HAL
	config *config.Config
}

// NewLoader creates a new HAL configuration loader
func NewLoader(h *HAL, cfg *config.Config) *Loader {
	return &Loader{
		hal:    h,
		config: cfg,
	}
}

// LoadFiles loads multiple HAL configuration files in order
func (l *Loader) LoadFiles(files []string) error {
	iniDir := filepath.Dir(l.config.IniPath)

	for _, f := range files {
		path := f
		if !filepath.IsAbs(path) {
			path = filepath.Join(iniDir, f)
		}

		if err := l.loadFile(path); err != nil {
			return fmt.Errorf("loading %s: %w", f, err)
		}
	}
	return nil
}

// loadFile loads a single HAL file
func (l *Loader) loadFile(path string) error {
	file, err := os.Open(path)
	if err != nil {
		return err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	lineNo := 0

	for scanner.Scan() {
		lineNo++
		line := strings.TrimSpace(scanner.Text())

		// Skip empty lines and comments
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		// Substitute INI variables [SECTION]KEY
		line = l.substituteIniVars(line)

		// Execute the HAL command
		if err := l.hal.ExecuteCommand(line); err != nil {
			return fmt.Errorf("line %d: %w", lineNo, err)
		}
	}

	return scanner.Err()
}

// substituteIniVars replaces [SECTION]KEY patterns with INI values
func (l *Loader) substituteIniVars(line string) string {
	// Pattern: [SECTION]KEY or [SECTION](KEY)
	// This is a simplified implementation
	// Full implementation should handle all HAL substitution patterns

	result := line

	// Simple regex-free approach for common patterns
	for {
		start := strings.Index(result, "[")
		if start < 0 {
			break
		}

		end := strings.Index(result[start:], "]")
		if end < 0 {
			break
		}
		end += start

		section := result[start+1 : end]

		// Check for KEY after ]
		rest := result[end+1:]
		keyEnd := strings.IndexAny(rest, " \t,;")
		if keyEnd < 0 {
			keyEnd = len(rest)
		}
		key := rest[:keyEnd]

		// Look up in config
		sectionData := l.config.GetSection(section)
		if sectionData != nil {
			if val, ok := sectionData[key]; ok {
				pattern := fmt.Sprintf("[%s]%s", section, key)
				result = strings.Replace(result, pattern, val, 1)
				continue
			}
		}

		// No substitution found, move past this bracket
		break
	}

	return result
}
```

```c
// src/server/shim/hal_shim.h
#ifndef HAL_SHIM_H
#define HAL_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HAL Shim Layer
 *
 * Provides a clean C interface for Go's cgo to call HAL functions.
 */

/* Initialize HAL component
 * Returns: component ID on success, negative error code on failure
 */
int hal_shim_init(const char *component_name);

/* Exit HAL component
 * Returns: 0 on success, negative error code on failure
 */
int hal_shim_exit(void);

/* Signal HAL component is ready
 * Returns: 0 on success, negative error code on failure
 */
int hal_shim_ready(void);

/* Execute a HAL command (like halcmd)
 * Returns: 0 on success, negative error code on failure
 */
int hal_shim_execute_cmd(const char *cmd);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SHIM_H */
```

```c
// src/server/shim/hal_shim.c
#include "hal_shim.h"
#include "hal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int hal_comp_id = -1;

int hal_shim_init(const char *component_name)
{
    hal_comp_id = hal_init(component_name);
    if (hal_comp_id < 0) {
        fprintf(stderr, "hal_shim: hal_init(%s) failed: %d\n",
                component_name, hal_comp_id);
        return hal_comp_id;
    }
    return hal_comp_id;
}

int hal_shim_exit(void)
{
    if (hal_comp_id < 0) {
        return 0;
    }

    int ret = hal_exit(hal_comp_id);
    hal_comp_id = -1;
    return ret;
}

int hal_shim_ready(void)
{
    if (hal_comp_id < 0) {
        return -1;
    }
    return hal_ready(hal_comp_id);
}

int hal_shim_execute_cmd(const char *cmd)
{
    // Delegate to halcmd for now
    // TODO: Implement direct HAL command parsing/execution
    char halcmd[2048];
    snprintf(halcmd, sizeof(halcmd), "halcmd %s", cmd);

    int ret = system(halcmd);
    if (ret != 0) {
        fprintf(stderr, "hal_shim: command failed: %s\n", cmd);
        return -1;
    }
    return 0;
}
```

### 3.6 Task Controller Integration

```go
// src/server/task/task.go
package task

/*
#cgo CFLAGS: -I${SRCDIR}/../../../emc/task -I${SRCDIR}/../../../emc/nml_intf
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -ltask -lnml -lstdc++ -lm

#include "task_shim.h"
#include <stdlib.h>
*/
import "C"

import (
	"context"
	"fmt"
	"time"
	"unsafe"
)

// Config holds Task controller initialization parameters
type Config struct {
	IniFile   string
	CycleTime float64 // seconds
}

// Task represents an initialized Task controller
type Task struct {
	config Config
}

// Init initializes the Task controller
func Init(cfg Config) (*Task, error) {
	iniFile := C.CString(cfg.IniFile)
	defer C.free(unsafe.Pointer(iniFile))

	ret := C.task_shim_init(iniFile)
	if ret != 0 {
		return nil, fmt.Errorf("task_init failed with code %d", ret)
	}

	return &Task{
		config: cfg,
	}, nil
}

// Shutdown cleanly shuts down the Task controller
func (t *Task) Shutdown() error {
	C.task_shim_shutdown()
	return nil
}

// Run executes the Task controller main loop
func (t *Task) Run(ctx context.Context) error {
	cycleTime := t.config.CycleTime
	if cycleTime <= 0 {
		cycleTime = 0.010 // 10ms default
	}

	ticker := time.NewTicker(time.Duration(cycleTime * float64(time.Second)))
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			ret := C.task_shim_cycle()
			if ret != 0 {
				// Non-fatal error, log and continue
				// Fatal errors should be handled differently
			}
		}
	}
}

// GetState returns the current task state
func (t *Task) GetState() int {
	return int(C.task_shim_get_state())
}
```

```c
// src/server/shim/task_shim.h
#ifndef TASK_SHIM_H
#define TASK_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Task Controller Shim Layer
 *
 * Provides a clean C interface for Go's cgo to control the task module.
 * The task module maintains NML communication for UI compatibility.
 */

/* Initialize task controller
 * ini_file: path to INI configuration file
 * Returns: 0 on success, negative error code on failure
 */
int task_shim_init(const char *ini_file);

/* Execute one task cycle (plan + execute)
 * Returns: 0 on success, negative on error
 */
int task_shim_cycle(void);

/* Shutdown task controller
 */
void task_shim_shutdown(void);

/* Get current task state
 * Returns: EMC_TASK_STATE enum value
 */
int task_shim_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_SHIM_H */
```

```c
// src/server/shim/task_shim.c
#include "task_shim.h"
#include <stdio.h>
#include <string.h>

/*
 * These extern declarations reference functions in emctaskmain.cc
 * They need to be added/exposed in the modified emctaskmain.cc
 */
extern int emcTaskInit(void);
extern int emcTaskPlan(void);
extern int emcTaskExecute(void);
extern void emcTaskShutdown(void);
extern int emcTaskGetState(void);

/* Global INI file path - emctaskmain.cc reads this */
extern char *emc_inifile;

static char ini_file_buffer[1024];

int task_shim_init(const char *ini_file)
{
    if (!ini_file) {
        fprintf(stderr, "task_shim: ini_file is NULL\n");
        return -1;
    }

    /* Store INI file path for emctaskmain to use */
    strncpy(ini_file_buffer, ini_file, sizeof(ini_file_buffer) - 1);
    ini_file_buffer[sizeof(ini_file_buffer) - 1] = '\0';
    emc_inifile = ini_file_buffer;

    /* Initialize task controller
     * This sets up NML channels, interpreter, etc.
     */
    int ret = emcTaskInit();
    if (ret != 0) {
        fprintf(stderr, "task_shim: emcTaskInit failed: %d\n", ret);
        return ret;
    }

    return 0;
}

int task_shim_cycle(void)
{
    int ret;

    /* Plan phase - read commands, run interpreter */
    ret = emcTaskPlan();
    if (ret != 0) {
        /* Plan can return non-zero for non-fatal conditions */
    }

    /* Execute phase - send commands to motion, handle state */
    ret = emcTaskExecute();
    if (ret != 0) {
        /* Execute can return non-zero for non-fatal conditions */
    }

    return 0;
}

void task_shim_shutdown(void)
{
    emcTaskShutdown();
}

int task_shim_get_state(void)
{
    return emcTaskGetState();
}
```

### 3.7 IO Controller Integration

```go
// src/server/iocontrol/iocontrol.go
package iocontrol

/*
#cgo CFLAGS: -I${SRCDIR}/../../../emc/iotask -I${SRCDIR}/../../../emc/nml_intf
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -liocontrol -lnml -lstdc++ -lm

#include "iocontrol_shim.h"
#include <stdlib.h>
*/
import "C"

import (
	"context"
	"fmt"
	"time"
	"unsafe"
)

// Config holds IO controller initialization parameters
type Config struct {
	IniFile   string
	CycleTime float64 // seconds
}

// IOControl represents an initialized IO controller
type IOControl struct {
	config Config
}

// Init initializes the IO controller
func Init(cfg Config) (*IOControl, error) {
	iniFile := C.CString(cfg.IniFile)
	defer C.free(unsafe.Pointer(iniFile))

	ret := C.iocontrol_shim_init(iniFile)
	if ret != 0 {
		return nil, fmt.Errorf("iocontrol_init failed with code %d", ret)
	}

	return &IOControl{
		config: cfg,
	}, nil
}

// Shutdown cleanly shuts down the IO controller
func (io *IOControl) Shutdown() error {
	C.iocontrol_shim_shutdown()
	return nil
}

// Run executes the IO controller main loop
func (io *IOControl) Run(ctx context.Context) error {
	cycleTime := io.config.CycleTime
	if cycleTime <= 0 {
		cycleTime = 0.100 // 100ms default
	}

	ticker := time.NewTicker(time.Duration(cycleTime * float64(time.Second)))
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			ret := C.iocontrol_shim_cycle()
			if ret != 0 {
				// Non-fatal error, continue
			}
		}
	}
}
```

```c
// src/server/shim/iocontrol_shim.h
#ifndef IOCONTROL_SHIM_H
#define IOCONTROL_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IO Controller Shim Layer
 *
 * Provides a clean C interface for Go's cgo to control iocontrol.
 * The IO controller maintains NML communication for UI compatibility.
 */

/* Initialize IO controller
 * ini_file: path to INI configuration file
 * Returns: 0 on success, negative error code on failure
 */
int iocontrol_shim_init(const char *ini_file);

/* Execute one IO controller cycle
 * Returns: 0 on success, negative on error
 */
int iocontrol_shim_cycle(void);

/* Shutdown IO controller
 */
void iocontrol_shim_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* IOCONTROL_SHIM_H */
```

```c
// src/server/shim/iocontrol_shim.c
#include "iocontrol_shim.h"
#include <stdio.h>
#include <string.h>

/*
 * These extern declarations reference functions that need to be
 * added/exposed in the modified ioControl.cc
 */
extern int iocontrol_init(const char *ini_file);
extern int iocontrol_cycle(void);
extern void iocontrol_shutdown(void);

int iocontrol_shim_init(const char *ini_file)
{
    if (!ini_file) {
        fprintf(stderr, "iocontrol_shim: ini_file is NULL\n");
        return -1;
    }

    int ret = iocontrol_init(ini_file);
    if (ret != 0) {
        fprintf(stderr, "iocontrol_shim: iocontrol_init failed: %d\n", ret);
        return ret;
    }

    return 0;
}

int iocontrol_shim_cycle(void)
{
    return iocontrol_cycle();
}

void iocontrol_shim_shutdown(void)
{
    iocontrol_shutdown();
}
```

---

## 4. Required Modifications to Existing Code

### 4.1 Modifications to emctaskmain.cc

The following changes are needed to convert milltask from a standalone executable to a library:

```cpp
// src/emc/task/emctaskmain.cc (modifications)

// ============================================================
// REMOVE: main() function and command-line argument parsing
// ============================================================

// DELETE this entire block (approximately lines 2800-3000):
/*
int main(int argc, char *argv[])
{
    // ... all the argument parsing ...
    // ... main loop ...
}
*/

// ============================================================
// ADD: Library initialization interface
// ============================================================

// Global INI file path (set by shim before calling init)
char *emc_inifile = NULL;

// Add these exported functions at the end of the file:

extern "C" {

/*
 * Initialize the task controller
 * Called once at startup
 * Returns: 0 on success, non-zero on error
 */
int emcTaskInit(void)
{
    int ret;

    // Check INI file is set
    if (emc_inifile == NULL || emc_inifile[0] == '\0') {
        rcs_print_error("emcTaskInit: emc_inifile not set\n");
        return -1;
    }

    // Read EMC_DEBUG from INI (existing code, move from main)
    IniFile inifile;
    if (inifile.Open(emc_inifile) == false) {
        rcs_print_error("emcTaskInit: can't open %s\n", emc_inifile);
        return -1;
    }

    // ... (move initialization code from main() here)
    // - NML channel creation
    // - Interpreter initialization
    // - Task state initialization
    // - etc.

    if ((ret = emcTaskNmlGet()) != 0) {
        rcs_print_error("emcTaskInit: emcTaskNmlGet failed\n");
        return ret;
    }

    if ((ret = emcTaskOnce(emc_inifile)) != 0) {
        rcs_print_error("emcTaskInit: emcTaskOnce failed\n");
        return ret;
    }

    return 0;
}

/*
 * Execute the planning phase
 * Called periodically from the main loop
 * Returns: 0 on success
 */
int emcTaskPlan(void)
{
    // Existing emcTaskPlan() logic
    // (this function likely already exists, just ensure it's exported)
    return emcTaskPlanExecute();
}

/*
 * Execute the execution phase
 * Called periodically from the main loop
 * Returns: 0 on success
 */
int emcTaskExecute(void)
{
    // Existing execution logic
    return emcTaskExecuteExecute();
}

/*
 * Shutdown the task controller
 * Called once at shutdown
 */
void emcTaskShutdown(void)
{
    // Move cleanup code from main() here
    // - NML channel cleanup
    // - Interpreter cleanup
    // - etc.

    emcTaskNmlDelete();
}

/*
 * Get current task state
 * Returns: EMC_TASK_STATE enum value
 */
int emcTaskGetState(void)
{
    return emcStatus->task.state;
}

} // extern "C"
```

### 4.2 Modifications to ioControl.cc

Similar modifications are needed for the IO controller:

```cpp
// src/emc/iotask/ioControl.cc (modifications)

// ============================================================
// REMOVE: main() function
// ============================================================

// DELETE the main() function

// ============================================================
// ADD: Library initialization interface
// ============================================================

static const char *io_inifile = NULL;
static bool io_initialized = false;

extern "C" {

/*
 * Initialize the IO controller
 * ini_file: path to INI configuration file
 * Returns: 0 on success, non-zero on error
 */
int iocontrol_init(const char *ini_file)
{
    if (io_initialized) {
        return 0; // Already initialized
    }

    io_inifile = ini_file;

    // Move initialization code from main() here:
    // - Parse INI file for [EMCIO] section
    // - Create NML channels
    // - Create HAL pins
    // - Initialize tool table
    // - etc.

    IniFile inifile;
    if (inifile.Open(io_inifile) == false) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "iocontrol: can't open INI file %s\n", io_inifile);
        return -1;
    }

    // ... rest of initialization ...

    io_initialized = true;
    return 0;
}

/*
 * Execute one IO controller cycle
 * Returns: 0 on success
 */
int iocontrol_cycle(void)
{
    if (!io_initialized) {
        return -1;
    }

    // This is essentially the body of the main loop:
    // - Read NML commands
    // - Process tool changes
    // - Handle coolant, lube, estop
    // - Update HAL pins
    // - Write NML status

    // ... existing loop body code ...

    return 0;
}

/*
 * Shutdown the IO controller
 */
void iocontrol_shutdown(void)
{
    if (!io_initialized) {
        return;
    }

    // Move cleanup code from main() here:
    // - Delete NML channels
    // - Remove HAL pins
    // - etc.

    io_initialized = false;
}

} // extern "C"
```

### 4.3 Modifications to Submakefiles

```makefile
# src/emc/task/Submakefile (modifications)

# ADD: Build task as shared library

# Existing object file list (keep as-is)
TASKSRCS := emc/task/emctaskmain.cc \
            emc/task/emccanon.cc \
            emc/task/taskintf.cc \
            emc/task/taskclass.cc \
            emc/task/taskmodule.cc \
            emc/task/backtrace.cc

TASKOBJS := $(patsubst %.cc,objects/%.o,$(TASKSRCS))

# ADD: Shared library target
../lib/libtask.so: $(TASKOBJS)
	$(ECHO) Linking libtask.so
	$(Q)$(CXX) -shared -o $@ $^ $(TASKLIBS) -Wl,-soname,libtask.so

# Keep existing milltask target for backward compatibility (optional)
../bin/milltask: $(TASKOBJS)
	$(ECHO) Linking milltask
	$(Q)$(CXX) -o $@ $^ $(TASKLIBS)

# ADD to TARGETS
TARGETS += ../lib/libtask.so
```

```makefile
# src/emc/iotask/Submakefile (modifications)

# ADD: Build iocontrol as shared library

IOCTLSRCS := emc/iotask/ioControl.cc

IOCTLOBJS := $(patsubst %.cc,objects/%.o,$(IOCTLSRCS))

# ADD: Shared library target
../lib/libiocontrol.so: $(IOCTLOBJS)
	$(ECHO) Linking libiocontrol.so
	$(Q)$(CXX) -shared -o $@ $^ $(IOCTLLIBS) -Wl,-soname,libiocontrol.so

# Keep existing iocontrol target for backward compatibility (optional)
../bin/iocontrol: $(IOCTLOBJS)
	$(ECHO) Linking iocontrol
	$(Q)$(CXX) -o $@ $^ $(IOCTLLIBS)

# ADD to TARGETS
TARGETS += ../lib/libiocontrol.so
```

---

## 5. Build System Integration

### 5.1 Top-Level Makefile Additions

```makefile
# Makefile (additions)

# ============================================================
# Go Server Build Targets
# ============================================================

GO ?= go
GOFLAGS ?=

SERVER_DIR := src/server
SERVER_BIN := bin/linuxcnc-server

# Build the Go server
.PHONY: server
server: libs
	@echo "Building linuxcnc-server..."
	cd $(SERVER_DIR) && \
	CGO_CFLAGS="-I$(abspath src/rtapi) -I$(abspath src/hal) -I$(abspath src/emc/nml_intf)" \
	CGO_LDFLAGS="-L$(abspath lib) -Wl,-rpath,$(abspath lib)" \
	$(GO) build $(GOFLAGS) -o $(abspath $(SERVER_BIN)) .

# Build required libraries first
.PHONY: libs
libs: ../lib/libtask.so ../lib/libiocontrol.so

# Development mode with race detector
.PHONY: server-dev
server-dev: libs
	cd $(SERVER_DIR) && \
	CGO_CFLAGS="-I$(abspath src/rtapi) -I$(abspath src/hal)" \
	CGO_LDFLAGS="-L$(abspath lib)" \
	$(GO) build -race $(GOFLAGS) -o $(abspath $(SERVER_BIN)) .

# Run tests
.PHONY: server-test
server-test:
	cd $(SERVER_DIR) && $(GO) test -v ./...

# Clean server artifacts
.PHONY: server-clean
server-clean:
	rm -f $(SERVER_BIN)
	cd $(SERVER_DIR) && $(GO) clean

# Add to main targets
TARGETS += server
```

### 5.2 Go Build Script (Alternative)

```bash
#!/bin/bash
# scripts/build-server.sh
#
# Build script for linuxcnc-server
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
SERVER_DIR="$ROOT_DIR/src/server"
LIB_DIR="$ROOT_DIR/lib"
BIN_DIR="$ROOT_DIR/bin"

# Ensure output directories exist
mkdir -p "$LIB_DIR" "$BIN_DIR"

# Build C/C++ libraries if needed
echo "=== Building C/C++ libraries ==="
make -C "$ROOT_DIR" ../lib/libtask.so ../lib/libiocontrol.so

# Set up Go environment
export CGO_ENABLED=1
export CGO_CFLAGS="-I$ROOT_DIR/src/rtapi -I$ROOT_DIR/src/hal -I$ROOT_DIR/src/emc/nml_intf"
export CGO_LDFLAGS="-L$LIB_DIR -Wl,-rpath,$LIB_DIR"

# Build Go server
echo "=== Building Go server ==="
cd "$SERVER_DIR"

# Get dependencies
go mod download

# Build
VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_TIME=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

go build \
    -ldflags "-X main.Version=$VERSION -X main.BuildTime=$BUILD_TIME" \
    -o "$BIN_DIR/linuxcnc-server" \
    .

echo "=== Build complete ==="
echo "Binary: $BIN_DIR/linuxcnc-server"

# Show linked libraries
echo ""
echo "=== Linked libraries ==="
ldd "$BIN_DIR/linuxcnc-server" | grep -E "(task|iocontrol|nml|hal)" || true
```

---

## 6. Testing Strategy

### 6.1 Unit Tests

```go
// src/server/config/config_test.go
package config

import (
	"os"
	"path/filepath"
	"testing"
)

func TestLoadValidINI(t *testing.T) {
	// Create temporary INI file
	tmpDir := t.TempDir()
	iniPath := filepath.Join(tmpDir, "test.ini")

	iniContent := `
[EMC]
MACHINE = TestMachine
VERSION = 1.0

[TASK]
CYCLE_TIME = 0.010

[EMCIO]
CYCLE_TIME = 0.100
TOOL_TABLE = tool.tbl

[HAL]
HALFILE = test.hal

[KINS]
KINEMATICS = trivkins
JOINTS = 3

[TRAJ]
COORDINATES = X Y Z
LINEAR_UNITS = mm
`

	if err := os.WriteFile(iniPath, []byte(iniContent), 0644); err != nil {
		t.Fatal(err)
	}

	// Create dummy HAL file
	halPath := filepath.Join(tmpDir, "test.hal")
	if err := os.WriteFile(halPath, []byte("# test"), 0644); err != nil {
		t.Fatal(err)
	}

	// Load configuration
	cfg, err := Load(iniPath)
	if err != nil {
		t.Fatalf("Load failed: %v", err)
	}

	// Verify values
	if cfg.EMC.MachineName != "TestMachine" {
		t.Errorf("MachineName = %q, want %q", cfg.EMC.MachineName, "TestMachine")
	}

	if cfg.Task.CycleTime != 0.010 {
		t.Errorf("Task.CycleTime = %v, want %v", cfg.Task.CycleTime, 0.010)
	}

	if cfg.Kins.Joints != 3 {
		t.Errorf("Kins.Joints = %d, want %d", cfg.Kins.Joints, 3)
	}
}

func TestValidateConfig(t *testing.T) {
	tests := []struct {
		name    string
		cfg     Config
		wantErr bool
	}{
		{
			name: "valid config",
			cfg: Config{
				Kins: struct {
					Kinematics string `ini:"KINEMATICS"`
					Joints     int    `ini:"JOINTS"`
				}{
					Kinematics: "trivkins",
					Joints:     3,
				},
			},
			wantErr: false,
		},
		{
			name: "missing kinematics",
			cfg: Config{
				Kins: struct {
					Kinematics string `ini:"KINEMATICS"`
					Joints     int    `ini:"JOINTS"`
				}{
					Kinematics: "",
					Joints:     3,
				},
			},
			wantErr: true,
		},
		{
			name: "zero joints",
			cfg: Config{
				Kins: struct {
					Kinematics string `ini:"KINEMATICS"`
					Joints     int    `ini:"JOINTS"`
				}{
					Kinematics: "trivkins",
					Joints:     0,
				},
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := tt.cfg.Validate()
			if (err != nil) != tt.wantErr {
				t.Errorf("Validate() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}
```

### 6.2 Integration Test Script

```bash
#!/bin/bash
# scripts/test-server.sh
#
# Integration test for linuxcnc-server
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
SERVER_BIN="$ROOT_DIR/bin/linuxcnc-server"
TEST_DIR="$ROOT_DIR/tests/server"

# Check server exists
if [ ! -x "$SERVER_BIN" ]; then
    echo "Error: Server not built. Run 'make server' first."
    exit 1
fi

echo "=== Testing configuration loading ==="
$SERVER_BIN -ini "$TEST_DIR/test.ini" -debug &
SERVER_PID=$!

# Give it time to start
sleep 2

# Check it's running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Error: Server failed to start"
    exit 1
fi

echo "Server started successfully (PID: $SERVER_PID)"

# Test graceful shutdown
echo "=== Testing shutdown ==="
kill -TERM $SERVER_PID
wait $SERVER_PID || true

echo "=== All tests passed ==="
```

### 6.3 Sample Test Configuration

```ini
# tests/server/test.ini
# Test INI configuration for linuxcnc-server

[EMC]
MACHINE = TestMachine
VERSION = 1.0
DEBUG = 0

[DISPLAY]
DISPLAY = dummy
CYCLE_TIME = 0.100

[TASK]
TASK = milltask
CYCLE_TIME = 0.010

[RS274NGC]
PARAMETER_FILE = test.var
SUBROUTINE_PATH = .

[EMCMOT]
EMCMOT = motmod
SERVO_PERIOD = 1000000
COMM_TIMEOUT = 1.0

[EMCIO]
EMCIO = io
CYCLE_TIME = 0.100
TOOL_TABLE = tool.tbl

[HAL]
HALFILE = test.hal

[TRAJ]
COORDINATES = X Y Z
LINEAR_UNITS = mm
ANGULAR_UNITS = degree
MAX_VELOCITY = 100
MAX_ACCELERATION = 500

[KINS]
KINEMATICS = trivkins coordinates=xyz
JOINTS = 3

[AXIS_X]
MAX_VELOCITY = 100
MAX_ACCELERATION = 500
MIN_LIMIT = -100
MAX_LIMIT = 100

[AXIS_Y]
MAX_VELOCITY = 100
MAX_ACCELERATION = 500
MIN_LIMIT = -100
MAX_LIMIT = 100

[AXIS_Z]
MAX_VELOCITY = 50
MAX_ACCELERATION = 250
MIN_LIMIT = -50
MAX_LIMIT = 0

[JOINT_0]
TYPE = LINEAR
HOME = 0
MAX_VELOCITY = 100
MAX_ACCELERATION = 500
MIN_LIMIT = -100
MAX_LIMIT = 100

[JOINT_1]
TYPE = LINEAR
HOME = 0
MAX_VELOCITY = 100
MAX_ACCELERATION = 500
MIN_LIMIT = -100
MAX_LIMIT = 100

[JOINT_2]
TYPE = LINEAR
HOME = 0
MAX_VELOCITY = 50
MAX_ACCELERATION = 250
MIN_LIMIT = -50
MAX_LIMIT = 0
```

```hal
# tests/server/test.hal
# Test HAL configuration

# Load realtime components
loadrt trivkins
loadrt motmod servo_period_nsec=1000000 num_joints=3

# Set up motion controller
addf motion-command-handler servo-thread
addf motion-controller servo-thread

# Minimal connections for testing
net xpos-cmd joint.0.motor-pos-cmd
net ypos-cmd joint.1.motor-pos-cmd
net zpos-cmd joint.2.motor-pos-cmd

net xpos-fb joint.0.motor-pos-fb <= joint.0.motor-pos-cmd
net ypos-fb joint.1.motor-pos-fb <= joint.1.motor-pos-cmd
net zpos-fb joint.2.motor-pos-fb <= joint.2.motor-pos-cmd
```

---

## 7. Migration Steps

### 7.1 Phase 1 Implementation Checklist

| Step | Task | Status |
|------|------|--------|
| 1.1 | Create `src/server/` directory structure | ☐ |
| 1.2 | Create Go module (`go.mod`, `go.sum`) | ☐ |
| 1.3 | Implement configuration package | ☐ |
| 1.4 | Implement C shim layer | ☐ |
| 1.5 | Implement RTAPI Go wrapper | ☐ |
| 1.6 | Implement HAL Go wrapper | ☐ |
| 1.7 | Implement HAL file loader | ☐ |
| 1.8 | Modify `emctaskmain.cc` | ☐ |
| 1.9 | Implement Task Go wrapper | ☐ |
| 1.10 | Modify `ioControl.cc` | ☐ |
| 1.11 | Implement IOControl Go wrapper | ☐ |
| 1.12 | Update build system | ☐ |
| 1.13 | Create test configuration | ☐ |
| 1.14 | Integration testing | ☐ |
| 1.15 | Documentation | ☐ |

### 7.2 Validation Criteria

| Criterion | Test Method |
|-----------|-------------|
| Server starts successfully | `linuxcnc-server -ini test.ini` runs without error |
| HAL loads correctly | HAL pins visible via `halcmd show` |
| Motion module loaded | `halcmd show comp` shows motmod |
| Task controller running | NML status channel receives updates |
| IO controller running | Tool change commands processed |
| UI connectivity | AXIS or other UI connects and displays status |
| Graceful shutdown | SIGTERM causes clean exit |
| Error handling | Invalid INI produces clear error message |

### 7.3 Rollback Plan

If issues are encountered:

1. The existing `linuxcnc` script remains functional
2. `milltask` and `iocontrol` binaries still work standalone
3. Simply don't use `linuxcnc-server` until issues resolved

---

## 8. Future Phases (Out of Scope)

The following items are explicitly **not** part of Phase 1:

| Item | Target Phase |
|------|--------------|
| New client API (JSON/Protobuf) | Phase 2 |
| WebSocket support | Phase 2 |
| NML removal | Phase 3 |
| Task controller C rewrite | Phase 4+ |
| IOControl C rewrite | Phase 4+ |
| Web-based UI | Phase 4+ |

---

## 9. Appendix

### 9.1 Error Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| -1 | General error |
| -2 | Configuration error |
| -3 | RTAPI initialization failed |
| -4 | HAL initialization failed |
| -5 | Task initialization failed |
| -6 | IOControl initialization failed |
| -7 | NML channel error |

### 9.2 Signal Handling

| Signal | Action |
|--------|--------|
| SIGINT | Graceful shutdown |
| SIGTERM | Graceful shutdown |
| SIGHUP | Reload configuration (future) |
| SIGUSR1 | Dump status (debug) |

### 9.3 Environment Variables

| Variable | Purpose |
|----------|---------|
| `LINUXCNC_INI` | Default INI file path |
| `LINUXCNC_DEBUG` | Enable debug output |
| `LINUXCNC_LOG` | Log file path |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-02-16 | - | Initial document |
