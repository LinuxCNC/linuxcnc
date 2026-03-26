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
#cgo LDFLAGS: -ldl -L${SRCDIR}/../../../../lib -llinuxcnchal

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "gomc_env.h"
#include "hal.h"
#include "rtapi.h"

// --- Pass-through HAL callbacks (delegate to liblinuxcnchal.so) ---

static int gomc_hal_init_cb(void *ctx, const char *name, void *dl_handle, int type) {
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

static int64_t gomc_rtapi_pll_get_reference_cb(void *ctx) {
    return (int64_t)rtapi_task_pll_get_reference();
}

static int gomc_rtapi_pll_set_correction_cb(void *ctx, long value) {
    return rtapi_task_pll_set_correction(value);
}

// --- INI callbacks (forward-declared, implemented in Go via //export) ---

extern char* gomc_ini_get(void *ctx, char *section, char *key);
extern char* gomc_ini_source_file(void *ctx);

// --- Env initialisation helpers ---

static void gomc_log_init(gomc_log_t *log, gomc_log_ring_t *ring) {
    log->ring = ring;
}

static void gomc_ini_init(gomc_ini_t *ini, void *ctx) {
    ini->ctx         = ctx;
    ini->get         = (const char*(*)(void*,const char*,const char*))gomc_ini_get;
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
    hal->export_funct = gomc_hal_export_funct_cb;
}

static void gomc_rtapi_init_struct(gomc_rtapi_t *rtapi) {
    rtapi->ctx                = NULL;
    rtapi->calloc             = gomc_rtapi_calloc_cb;
    rtapi->realloc            = gomc_rtapi_realloc_cb;
    rtapi->free               = gomc_rtapi_free_cb;
    rtapi->get_time           = gomc_rtapi_get_time_cb;
    rtapi->pll_get_reference  = gomc_rtapi_pll_get_reference_cb;
    rtapi->pll_set_correction = gomc_rtapi_pll_set_correction_cb;
}

static cmod_env_t *gomc_env_create(gomc_log_ring_t *ring, void *ini_ctx,
                                   void *dl_handle) {
    cmod_env_t *env = (cmod_env_t *)calloc(1, sizeof(cmod_env_t));
    if (!env) return NULL;

    gomc_log_t *log = (gomc_log_t *)calloc(1, sizeof(gomc_log_t));
    gomc_ini_t *ini = (gomc_ini_t *)calloc(1, sizeof(gomc_ini_t));
    gomc_hal_t *hal = (gomc_hal_t *)calloc(1, sizeof(gomc_hal_t));
    gomc_rtapi_t *rtapi = (gomc_rtapi_t *)calloc(1, sizeof(gomc_rtapi_t));

    if (!log || !ini || !hal || !rtapi) {
        free(log); free(ini); free(hal); free(rtapi); free(env);
        return NULL;
    }

    gomc_log_init(log, ring);
    gomc_ini_init(ini, ini_ctx);
    gomc_hal_init_struct(hal);
    gomc_rtapi_init_struct(rtapi);

    env->dl_handle = dl_handle;
    env->log       = log;
    env->ini       = ini;
    env->hal       = hal;
    env->rtapi     = rtapi;

    return env;
}

static void gomc_env_destroy(cmod_env_t *env) {
    if (!env) return;
    free((void *)env->log);
    free((void *)env->ini);
    free((void *)env->hal);
    free((void *)env->rtapi);
    free(env);
}

// cmod lifecycle call wrappers.
static int cmod_call_new(cmod_new_fn fn, const cmod_env_t *env,
                         const char *name, int argc, const char **argv,
                         cmod_t **out) {
    return fn(env, name, argc, argv, out);
}

static int cmod_call_start(cmod_t *m) {
    return m->Start(m);
}

static void cmod_call_stop(cmod_t *m) {
    m->Stop(m);
}

static void cmod_call_destroy(cmod_t *m) {
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

	"github.com/sittner/linuxcnc/src/launcher/internal/config"
	halcmd "github.com/sittner/linuxcnc/src/launcher/internal/halcmd"
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
// Same resolution logic as resolveGoModulePath but uses EMC2_CMOD_DIR.
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

	hCtx := cgo.NewHandle(l)
	env := C.gomc_env_create(l.logRing.ring, unsafe.Pointer(uintptr(hCtx)), handle)
	if env == nil {
		C.dlclose(handle)
		return fmt.Errorf("load C plugin %q: failed to allocate cmod_env_t", path)
	}
	cm.env = env

	// Convert args to C strings.
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	argc := C.int(len(args))
	var argv **C.char
	if len(args) > 0 {
		cargs := make([]*C.char, len(args))
		for i, a := range args {
			cargs[i] = C.CString(a)
		}
		defer func() {
			for _, ca := range cargs {
				C.free(unsafe.Pointer(ca))
			}
		}()
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

// startCModules calls Start() on all loaded C plugin modules that have not
// already been started (e.g. modules started early via startCModuleByName).
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

// startCModuleByName calls Start() on a single loaded C module identified by
// name.  This is used when a module must be started before the batch
// startCModules() call (e.g. the NML server must run before NML clients).
func (l *Launcher) startCModuleByName(name string) error {
	for _, cm := range l.cModules {
		if cm.name == name {
			if cm.started {
				return nil
			}
			rc := C.cmod_call_start(cm.mod)
			if rc != 0 {
				return fmt.Errorf("C module %q Start() returned error code %d", name, int(rc))
			}
			cm.started = true
			return nil
		}
	}
	return fmt.Errorf("C module %q not loaded", name)
}

// lockCModules locks the PT_LOAD segments of all loaded C plugin .so files
// into memory. Call after all components are initialized, before starting
// RT threads.
func (l *Launcher) lockCModules() {
	for _, cm := range l.cModules {
		if cm.handle != nil {
			halcmd.LockDLHandle(unsafe.Pointer(cm.handle))
		}
	}
}

// unlockCModules unlocks the PT_LOAD segments of all loaded C plugin .so files.
func (l *Launcher) unlockCModules() {
	for _, cm := range l.cModules {
		if cm.handle != nil {
			halcmd.UnlockDLHandle(unsafe.Pointer(cm.handle))
		}
	}
}

// stopCModules calls Stop() on all loaded C plugin modules in reverse order.
func (l *Launcher) stopCModules() {
	for i := len(l.cModules) - 1; i >= 0; i-- {
		C.cmod_call_stop(l.cModules[i].mod)
	}
}

// destroyCModules calls Destroy() on all loaded C plugin modules in reverse
// order, unlocks and closes the dlopen handles, stops the log drain, and
// frees all arena-tracked strings and gomc env structs.
func (l *Launcher) destroyCModules() {
	l.unlockCModules()
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
	// Stop the log drain goroutine and do a final flush.
	if l.logRing != nil {
		l.logRing.stopDrain(l.logger)
		l.logRing.destroy()
		l.logRing = nil
	}
	// Free arena strings after all modules have been destroyed.
	for _, p := range l.cModArena {
		C.free(p)
	}
	l.cModArena = nil
}
