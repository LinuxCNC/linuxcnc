// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package launcher — cmodules.go handles loading, lifecycle management of
// C plugin .so files loaded via the "load" HAL command.
//
// C plugins are resolved from EMC2_CMOD_DIR (bare names) and loaded via
// dlopen/dlsym.  They share the same lifecycle as Go plugins:
//
//	New(env, name, args) → Start() → Stop() → Destroy()
//
// The launcher provides INI access, logging, HAL and RTAPI to C plugins
// via the gomc sub-API callback structs in cmod_env_t, so plugins never
// parse config files or link liblinuxcnchal.so directly.
package launcher

/*
#cgo CFLAGS: -I${SRCDIR}/../../pkg/cmodule -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include
#cgo LDFLAGS: -ldl

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "gomc_env.h"
#include "hal.h"
#include "rtapi.h"

// --- API registry callbacks (forward-declared, implemented in Go via //export) ---

extern int gomc_api_register_cb(void *ctx, char *api_name, int version,
                                char *instance_name, void *callbacks);
extern void *gomc_api_get_cb(void *ctx, char *api_name, int version,
                             char *instance_name);
extern int gomc_watch_push_cb(void *ctx, char *api_name, char *instance_name,
                              char *func_name, void *data, int data_len);
extern void gomc_record_consumer_cb(void *ctx, char *consumer_instance,
                                    char *api_name, char *provider_instance);

// --- RT module handle tracking ---
//
// Modules that call hal_init() with GOMC_HAL_COMP_REALTIME have their
// dl_handle recorded here.  The Go side batch-locks / unlocks these
// before starting / after stopping HAL threads.  Works for both cmod
// and gomod — the interception point is gomc_hal_init_cb which every
// module's hal->init() delegates to.

static void **rt_dl_handles = NULL;
static int    rt_dl_count   = 0;
static int    rt_dl_cap     = 0;

static void rt_dl_handles_add(void *handle) {
    for (int i = 0; i < rt_dl_count; i++)
        if (rt_dl_handles[i] == handle) return;  // deduplicate
    if (rt_dl_count >= rt_dl_cap) {
        rt_dl_cap = rt_dl_cap ? rt_dl_cap * 2 : 8;
        rt_dl_handles = realloc(rt_dl_handles, rt_dl_cap * sizeof(void *));
    }
    rt_dl_handles[rt_dl_count++] = handle;
}

static void rt_dl_handles_remove(void *handle) {
    for (int i = 0; i < rt_dl_count; i++) {
        if (rt_dl_handles[i] == handle) {
            rt_dl_handles[i] = rt_dl_handles[--rt_dl_count];
            return;
        }
    }
}

static void rt_dl_handles_free(void) {
    free(rt_dl_handles);
    rt_dl_handles = NULL;
    rt_dl_count = 0;
    rt_dl_cap = 0;
}

static int rt_dl_handles_len(void) { return rt_dl_count; }
static void *rt_dl_handles_get(int i) { return rt_dl_handles[i]; }

// --- Pass-through HAL callbacks (delegate to liblinuxcnchal.so) ---

static int gomc_hal_init_cb(void *ctx, const char *name, void *dl_handle, int type) {
    if (type == GOMC_HAL_COMP_REALTIME && dl_handle)
        rt_dl_handles_add(dl_handle);
    return hal_init_ex(name, dl_handle, (component_type_t)type);
}

static void gomc_hal_exit_cb(void *ctx, int comp_id) {
    hal_exit(comp_id);
}

static int gomc_hal_ready_cb(void *ctx, int comp_id) {
    return hal_ready(comp_id);
}

static void *gomc_hal_malloc_cb(void *ctx, long size) {
    return hal_malloc(size);
}

static int gomc_hal_pin_new_cb(void *ctx, const char *name, int type, int dir,
                               void **data_ptr_addr, int comp_id) {
    return hal_pin_new(name, (hal_type_t)type, (hal_pin_dir_t)dir,
                       data_ptr_addr, comp_id);
}

static int gomc_hal_param_new_cb(void *ctx, const char *name, int type, int dir,
                                 void *data_addr, int comp_id) {
    return hal_param_new(name, (hal_type_t)type, (hal_param_dir_t)dir,
                         data_addr, comp_id);
}

static int gomc_hal_pin_alias_cb(void *ctx, const char *pin_name, const char *alias) {
    return hal_pin_alias(pin_name, alias);
}

static int gomc_hal_param_alias_cb(void *ctx, const char *param_name, const char *alias) {
    return hal_param_alias(param_name, alias);
}

static int gomc_hal_export_funct_cb(void *ctx, const char *name,
                                    void (*funct)(void *, long),
                                    void *arg, int uses_fp, int reentrant,
                                    int comp_id) {
    return hal_export_funct(name, funct, arg, uses_fp, reentrant, comp_id);
}

// --- Pass-through RTAPI callbacks ---

static void *gomc_rtapi_calloc_cb(void *ctx, size_t size) {
    return rtapi_calloc(size);
}

static void *gomc_rtapi_realloc_cb(void *ctx, void *ptr, size_t size) {
    return rtapi_realloc(ptr, size);
}

static void gomc_rtapi_free_cb(void *ctx, void *ptr) {
    rtapi_free(ptr);
}

static int64_t gomc_rtapi_get_time_cb(void *ctx) {
    return (int64_t)rtapi_get_time();
}

static void gomc_rtapi_delay_cb(void *ctx, long nsec) {
    rtapi_delay(nsec);
}

static long gomc_rtapi_delay_max_cb(void *ctx) {
    return rtapi_delay_max();
}

static int64_t gomc_rtapi_pll_get_reference_cb(void *ctx) {
    return (int64_t)rtapi_task_pll_get_reference();
}

static int gomc_rtapi_pll_set_correction_cb(void *ctx, long value) {
    return rtapi_task_pll_set_correction(value);
}

static int gomc_rtapi_task_self_cb(void *ctx) {
    return rtapi_task_self();
}

// --- INI callbacks (forward-declared, implemented in Go via //export) ---

extern char* gomc_ini_get(void *ctx, char *section, char *key);
extern char** gomc_ini_get_all(void *ctx, char *section, char *key, int *out_count);
extern char* gomc_ini_source_file(void *ctx);

// --- Log subscribe/unsubscribe (forward-declared, implemented in Go) ---

extern gomc_log_sub_t* gomc_log_subscribe_cb(void *ctx, gomc_log_level_t min_level);
extern void gomc_log_unsubscribe_cb(void *ctx, gomc_log_sub_t *sub);

// --- Env initialisation helpers ---

static void gomc_log_init(gomc_log_t *log, gomc_log_ring_t *ring, void *ctx) {
    log->ring        = ring;
    log->subscribe   = (gomc_log_sub_t*(*)(void*,gomc_log_level_t))gomc_log_subscribe_cb;
    log->unsubscribe = (void(*)(void*,gomc_log_sub_t*))gomc_log_unsubscribe_cb;
    log->ctx         = ctx;
}

static void gomc_ini_init(gomc_ini_t *ini, void *ctx) {
    ini->ctx         = ctx;
    ini->get         = (const char*(*)(void*,const char*,const char*))gomc_ini_get;
    ini->get_all     = (const char**(*)(void*,const char*,const char*,int*))gomc_ini_get_all;
    ini->source_file = (const char*(*)(void*))gomc_ini_source_file;
}

static void gomc_hal_init_struct(gomc_hal_t *hal) {
    hal->ctx          = NULL;
    hal->init         = gomc_hal_init_cb;
    hal->exit         = gomc_hal_exit_cb;
    hal->ready        = gomc_hal_ready_cb;
    hal->malloc       = gomc_hal_malloc_cb;
    hal->pin_new      = gomc_hal_pin_new_cb;
    hal->param_new    = gomc_hal_param_new_cb;
    hal->pin_alias    = gomc_hal_pin_alias_cb;
    hal->param_alias  = gomc_hal_param_alias_cb;
    hal->export_funct = gomc_hal_export_funct_cb;
}

static void gomc_rtapi_init_struct(gomc_rtapi_t *rtapi) {
    rtapi->ctx                = NULL;
    rtapi->calloc             = gomc_rtapi_calloc_cb;
    rtapi->realloc            = gomc_rtapi_realloc_cb;
    rtapi->free               = gomc_rtapi_free_cb;
    rtapi->get_time           = gomc_rtapi_get_time_cb;
    rtapi->delay              = gomc_rtapi_delay_cb;
    rtapi->delay_max          = gomc_rtapi_delay_max_cb;
    rtapi->pll_get_reference  = gomc_rtapi_pll_get_reference_cb;
    rtapi->pll_set_correction = gomc_rtapi_pll_set_correction_cb;
    rtapi->task_self          = gomc_rtapi_task_self_cb;
}

static void gomc_api_init_struct(gomc_api_t *api) {
    api->ctx              = NULL;
    api->register_api     = (int(*)(void*,const char*,int,const char*,const void*))gomc_api_register_cb;
    api->get_api          = (const void*(*)(void*,const char*,int,const char*))gomc_api_get_cb;
    api->push_watch       = (int(*)(void*,const char*,const char*,const char*,const void*,int))gomc_watch_push_cb;
    api->record_consumer  = (void(*)(void*,const char*,const char*,const char*))gomc_record_consumer_cb;
}

// log_ctx/ini_ctx are cgo.Handles (opaque integers) passed as uintptr_t rather
// than void* so the Go side never converts a uintptr to unsafe.Pointer (bad
// pointer arithmetic under -d=checkptr / -race); cast into the void* ctx fields
// via gomc_log_init/gomc_ini_init below.
static cmod_env_t *gomc_env_create(gomc_log_ring_t *ring, uintptr_t log_ctx,
                                   uintptr_t ini_ctx, void *dl_handle) {
    cmod_env_t *env = (cmod_env_t *)calloc(1, sizeof(cmod_env_t));
    if (!env) return NULL;

    gomc_log_t *log = (gomc_log_t *)calloc(1, sizeof(gomc_log_t));
    gomc_ini_t *ini = (gomc_ini_t *)calloc(1, sizeof(gomc_ini_t));
    gomc_hal_t *hal = (gomc_hal_t *)calloc(1, sizeof(gomc_hal_t));
    gomc_rtapi_t *rtapi = (gomc_rtapi_t *)calloc(1, sizeof(gomc_rtapi_t));
    gomc_api_t *api = (gomc_api_t *)calloc(1, sizeof(gomc_api_t));

    if (!log || !ini || !hal || !rtapi || !api) {
        free(log); free(ini); free(hal); free(rtapi); free(api); free(env);
        return NULL;
    }

    gomc_log_init(log, ring, (void *)log_ctx);
    gomc_ini_init(ini, (void *)ini_ctx);
    gomc_hal_init_struct(hal);
    gomc_rtapi_init_struct(rtapi);
    gomc_api_init_struct(api);

    env->dl_handle = dl_handle;
    env->log       = log;
    env->ini       = ini;
    env->hal       = hal;
    env->rtapi     = rtapi;
    env->api       = api;

    return env;
}

static void gomc_env_destroy(cmod_env_t *env) {
    if (!env) return;
    free((void *)env->log);
    free((void *)env->ini);
    free((void *)env->hal);
    free((void *)env->rtapi);
    free((void *)env->api);
    free(env);
}

// cmod lifecycle call wrappers.
static int cmod_call_new(cmod_new_fn fn, const cmod_env_t *env,
                         const char *name, int argc, const char **argv,
                         cmod_t **out) {
    return fn(env, name, argc, argv, out);
}

static int cmod_call_init(cmod_t *m) {
    if (!m->Init) return 0;
    return m->Init(m);
}

static int cmod_call_start(cmod_t *m) {
    if (!m->Start) return 0;
    return m->Start(m);
}

static void cmod_call_stop(cmod_t *m) {
    if (!m->Stop) return;
    m->Stop(m);
}

static void cmod_call_destroy(cmod_t *m) {
    if (!m->Destroy) return;
    m->Destroy(m);
}
*/
import "C"

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime/cgo"
	"strings"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/config"
	halcmd "github.com/sittner/linuxcnc/src/gomc/internal/halcmd"
)

// cModule holds a loaded C plugin module.
type cModule struct {
	handle  *C.void // dlopen handle
	mod     *C.cmod_t
	env     *C.cmod_env_t // gomc env (freed in destroyCModules)
	hCtx    cgo.Handle    // Go↔C handle for the Launcher pointer
	name    string
	started bool // true after Start() has been called
}

// resolveCModulePath resolves a C module name or path to an absolute .so path.
// If the name contains a '/' it is treated as a path and used as-is.
// Otherwise, the bare module name is resolved to $EMC2_CMOD_DIR/<name>.so.
func resolveCModulePath(name string) string {
	if strings.Contains(name, "/") {
		return name
	}
	name = strings.TrimSuffix(name, ".so")
	return filepath.Join(config.EMC2CmodDir, name+".so")
}

// cModuleExists checks whether a .so file exists at the given path.
func cModuleExists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

// loadCPlugin loads a C plugin .so via dlopen, looks up the "New" symbol,
// builds the cmod_env_t with gomc sub-API callbacks, calls the factory, and
// appends the module to l.cModules.
//
// The factory is expected to create and fully initialize the module (including
// HAL component/pin creation) before returning.
func (l *Launcher) loadCPlugin(path string, name string, args []string) error {
	l.logger.Info("loading C plugin", "path", path, "name", name)

	// Ensure the shared log ring is created and draining.
	l.ensureLogRing()

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	handle := C.dlopen(cpath, C.RTLD_NOW)
	if handle == nil {
		return fmt.Errorf("load C plugin %q: dlopen: %s", path, C.GoString(C.dlerror()))
	}

	symName := C.CString("New")
	defer C.free(unsafe.Pointer(symName))

	sym := C.dlsym(handle, symName)
	if sym == nil {
		C.dlclose(handle)
		return fmt.Errorf("load C plugin %q: missing \"New\" symbol: %s", path, C.GoString(C.dlerror()))
	}

	factory := C.cmod_new_fn(sym)

	// Build the gomc environment with all sub-API callbacks.
	cm := &cModule{
		handle: (*C.void)(handle),
		name:   name,
	}

	// Pass the handle as an integer (C.uintptr_t), not unsafe.Pointer(uintptr(hCtx)):
	// a cgo.Handle is a uintptr and uintptr->unsafe.Pointer is bad pointer
	// arithmetic under -d=checkptr (enabled by -race).
	hCtx := cgo.NewHandle(l)
	cCtx := C.uintptr_t(hCtx)
	env := C.gomc_env_create(l.logRing.ring, cCtx, cCtx, handle)
	if env == nil {
		C.dlclose(handle)
		return fmt.Errorf("load C plugin %q: failed to allocate cmod_env_t", path)
	}
	cm.env = env

	// Convert args to C strings.  Keep them alive for the full module
	// lifecycle so C code can hold pointers into name/argv safely.
	cName := C.CString(name)
	l.cModArena = append(l.cModArena, unsafe.Pointer(cName))

	argc := C.int(len(args))
	var argv **C.char
	if len(args) > 0 {
		cargs := make([]*C.char, len(args))
		for i, a := range args {
			cargs[i] = C.CString(a)
			l.cModArena = append(l.cModArena, unsafe.Pointer(cargs[i]))
		}
		argv = &cargs[0]
	}

	var mod *C.cmod_t
	rc := C.cmod_call_new(factory, env, cName, argc, argv, &mod)
	if rc != 0 {
		C.gomc_env_destroy(env)
		C.dlclose(handle)
		return fmt.Errorf("load C plugin %q: factory returned error code %d", path, int(rc))
	}
	cm.mod = mod

	cm.hCtx = hCtx
	l.cModules = append(l.cModules, cm)
	l.logger.Debug("C plugin loaded and initialized", "path", path, "name", name)

	return nil
}

// runtimeLoadModule loads a cmod plugin at runtime (called from the halcmd
// "load" REST endpoint).  It resolves the path, calls loadCPlugin, Init,
// and Start so the module is fully operational when the call returns.
func (l *Launcher) runtimeLoadModule(module string, args []string) error {
	path := resolveCModulePath(module)
	if !cModuleExists(path) {
		// Try as a Go module — load and start immediately.
		if err := l.loadGoModule(module, module, args); err != nil {
			return err
		}
		gm := l.goModules[len(l.goModules)-1]
		return gm.mod.Start()
	}

	// Use the module basename (without .so) as the instance name.
	name := strings.TrimSuffix(filepath.Base(path), ".so")

	if err := l.loadCPlugin(path, name, args); err != nil {
		return err
	}

	// The newly loaded module is the last one appended.
	cm := l.cModules[len(l.cModules)-1]

	// Init phase — look up other modules' APIs.
	rc := C.cmod_call_init(cm.mod)
	if rc != 0 {
		return fmt.Errorf("C module %q Init() returned error code %d", name, int(rc))
	}

	// Start phase — begin operation.
	rc = C.cmod_call_start(cm.mod)
	if rc != 0 {
		return fmt.Errorf("C module %q Start() returned error code %d", name, int(rc))
	}
	cm.started = true

	l.logger.Info("runtime-loaded C module", "name", name, "path", path)
	return nil
}

// initCModules calls Init() on all loaded C plugin modules in load order.
// Init() runs after all modules' New() have completed (all APIs registered)
// but before HAL wiring commands and Start().  Modules use Init() to look up
// other modules' APIs and perform cross-module initialization.
func (l *Launcher) initCModules() error {
	for _, cm := range l.cModules {
		rc := C.cmod_call_init(cm.mod)
		if rc != 0 {
			return fmt.Errorf("C module %q Init() returned error code %d", cm.name, int(rc))
		}
	}
	return nil
}

// startCModules calls Start() on all loaded C plugin modules that have not
// already been started.
func (l *Launcher) startCModules() error {
	for _, cm := range l.cModules {
		if cm.started {
			continue
		}
		rc := C.cmod_call_start(cm.mod)
		if rc != 0 {
			return fmt.Errorf("C module %q Start() returned error code %d", cm.name, int(rc))
		}
		cm.started = true
	}
	return nil
}

// lockRTModules locks the PT_LOAD segments of all module .so files that
// registered at least one GOMC_HAL_COMP_REALTIME component.  The set of
// handles is maintained by gomc_hal_init_cb (C side) and covers both
// cmod and gomod .so files.
func (l *Launcher) lockRTModules() {
	n := int(C.rt_dl_handles_len())
	for i := 0; i < n; i++ {
		halcmd.LockDLHandle(C.rt_dl_handles_get(C.int(i)))
	}
}

// unlockRTModules unlocks the PT_LOAD segments locked by lockRTModules.
func (l *Launcher) unlockRTModules() {
	n := int(C.rt_dl_handles_len())
	for i := 0; i < n; i++ {
		halcmd.UnlockDLHandle(C.rt_dl_handles_get(C.int(i)))
	}
}

// stopCModules calls Stop() on all loaded C plugin modules in reverse order.
func (l *Launcher) stopCModules() {
	for i := len(l.cModules) - 1; i >= 0; i-- {
		C.cmod_call_stop(l.cModules[i].mod)
	}
}

// destroyCModules calls Destroy() on all loaded C plugin modules in reverse
// order, unlocks and closes the dlopen handles, and frees all arena-tracked
// strings and gomc env structs.  The log ring is NOT destroyed here — it
// remains active so that later cleanup steps (destroyGoModules, UnloadAll)
// can still emit log messages.  See doCleanup() for ring teardown.
func (l *Launcher) destroyCModules() {
	l.unlockRTModules()
	for i := len(l.cModules) - 1; i >= 0; i-- {
		cm := l.cModules[i]
		C.cmod_call_destroy(cm.mod)
		if cm.env != nil {
			C.gomc_env_destroy(cm.env)
			cm.env = nil
		}
		if cm.handle != nil {
			C.dlclose(unsafe.Pointer(cm.handle))
		}
		cm.hCtx.Delete()
	}
	// Free arena strings after all modules have been destroyed.
	for _, p := range l.cModArena {
		C.free(p)
	}
	l.cModArena = nil
	// Free the RT handle tracking array.
	C.rt_dl_handles_free()
}

// cmodStop calls Stop() on a single cmod.
func cmodStop(cm *cModule) {
	C.cmod_call_stop(cm.mod)
}

// cmodDestroy calls Destroy() on a single cmod.
func cmodDestroy(cm *cModule) {
	C.cmod_call_destroy(cm.mod)
}

// cmodDestroyEnv frees the gomc env struct.
func cmodDestroyEnv(cm *cModule) {
	if cm.env != nil {
		C.gomc_env_destroy(cm.env)
		cm.env = nil
	}
}

// cmodDlclose closes the dlopen handle.
func cmodDlclose(cm *cModule) {
	if cm.handle != nil {
		C.rt_dl_handles_remove(unsafe.Pointer(cm.handle))
		C.dlclose(unsafe.Pointer(cm.handle))
	}
}
