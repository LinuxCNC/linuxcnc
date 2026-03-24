// Package launcher — cmodules.go handles loading, lifecycle management of
// C plugin .so files loaded via the "load" HAL command.
//
// C plugins are resolved from EMC2_CMOD_DIR (bare names) and loaded via
// dlopen/dlsym.  They share the same lifecycle as Go plugins:
//
//	New(env, name, args) → Init() → Start() → Stop() → DeInit()
//
// The launcher provides INI access and logging to C plugins via the
// cmod_env_t callback struct, so plugins never parse config files directly.
package launcher

/*
#cgo LDFLAGS: -ldl

#include <dlfcn.h>
#include <stdlib.h>
#include "../../pkg/cmodule/cmodule.h"

// Forward-declare the Go-exported callback functions.  CGO exports use
// non-const char* so the declarations must match exactly.
extern char* cmod_get_ini(void *ctx, char *section, char *key);
extern char* cmod_ini_source_file(void *ctx);
extern void cmod_log_info(void *ctx, char *component, char *msg);
extern void cmod_log_warn(void *ctx, char *component, char *msg);
extern void cmod_log_error(void *ctx, char *component, char *msg);
extern void cmod_log_debug(void *ctx, char *component, char *msg);

// cmod_env_init populates a cmod_env_t with the launcher's callback functions.
// Casts bridge the const-correct header types with CGO's non-const exports.
static void cmod_env_init(cmod_env_t *env, void *ctx) {
    env->ctx             = ctx;
    env->get_ini         = (const char*(*)(void*,const char*,const char*))cmod_get_ini;
    env->ini_source_file = (const char*(*)(void*))cmod_ini_source_file;
    env->log_info        = (void(*)(void*,const char*,const char*))cmod_log_info;
    env->log_warn        = (void(*)(void*,const char*,const char*))cmod_log_warn;
    env->log_error       = (void(*)(void*,const char*,const char*))cmod_log_error;
    env->log_debug       = (void(*)(void*,const char*,const char*))cmod_log_debug;
}

// cmod_call_new calls the factory function pointer with the given arguments.
static int cmod_call_new(cmod_new_fn fn, const cmod_env_t *env,
                         const char *name, int argc, const char **argv,
                         cmod_t **out) {
    return fn(env, name, argc, argv, out);
}

// cmod_call_init calls the module's Init function.
static int cmod_call_init(cmod_t *m) {
    return m->Init(m);
}

// cmod_call_start calls the module's Start function.
static int cmod_call_start(cmod_t *m) {
    return m->Start(m);
}

// cmod_call_stop calls the module's Stop function.
static void cmod_call_stop(cmod_t *m) {
    m->Stop(m);
}

// cmod_call_deinit calls the module's DeInit function.
static void cmod_call_deinit(cmod_t *m) {
    m->DeInit(m);
}
*/
import "C"

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"unsafe"

	"github.com/sittner/linuxcnc/src/launcher/internal/config"
)

// cModule holds a loaded C plugin module.
type cModule struct {
	handle *C.void // dlopen handle
	mod    *C.cmod_t
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

// Callback implementations — these are Go functions exported to C via
// //export.  They are called from C code via function pointers in cmod_env_t.
// Arena-tracked strings returned by get_ini and ini_source_file are freed
// in deinitCModules.

//export cmod_get_ini
func cmod_get_ini(ctx unsafe.Pointer, section, key *C.char) *C.char {
	l := (*Launcher)(ctx)
	val := l.ini.Get(C.GoString(section), C.GoString(key))
	if val == "" {
		return nil
	}
	cs := C.CString(val)
	l.cModArena = append(l.cModArena, unsafe.Pointer(cs))
	return cs
}

//export cmod_ini_source_file
func cmod_ini_source_file(ctx unsafe.Pointer) *C.char {
	l := (*Launcher)(ctx)
	cs := C.CString(l.ini.SourceFile())
	l.cModArena = append(l.cModArena, unsafe.Pointer(cs))
	return cs
}

//export cmod_log_info
func cmod_log_info(ctx unsafe.Pointer, component, msg *C.char) {
	l := (*Launcher)(ctx)
	l.logger.Info(C.GoString(msg), "component", C.GoString(component))
}

//export cmod_log_warn
func cmod_log_warn(ctx unsafe.Pointer, component, msg *C.char) {
	l := (*Launcher)(ctx)
	l.logger.Warn(C.GoString(msg), "component", C.GoString(component))
}

//export cmod_log_error
func cmod_log_error(ctx unsafe.Pointer, component, msg *C.char) {
	l := (*Launcher)(ctx)
	l.logger.Error(C.GoString(msg), "component", C.GoString(component))
}

//export cmod_log_debug
func cmod_log_debug(ctx unsafe.Pointer, component, msg *C.char) {
	l := (*Launcher)(ctx)
	l.logger.Debug(C.GoString(msg), "component", C.GoString(component))
}

// loadCPlugin loads a C plugin .so via dlopen, looks up the "New" symbol,
// builds the cmod_env_t with launcher callbacks, calls the factory, calls
// Init(), and appends the module to l.cModules.
func (l *Launcher) loadCPlugin(path string, name string, args []string) error {
	l.logger.Info("loading C plugin", "path", path, "name", name)

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

	// Build the environment callback struct.
	cm := &cModule{
		handle: (*C.void)(handle),
	}

	var env C.cmod_env_t
	C.cmod_env_init(&env, unsafe.Pointer(l))

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
	rc := C.cmod_call_new(factory, &env, cName, argc, argv, &mod)
	if rc != 0 {
		C.dlclose(handle)
		return fmt.Errorf("load C plugin %q: factory returned error code %d", path, int(rc))
	}
	cm.mod = mod

	rc = C.cmod_call_init(mod)
	if rc != 0 {
		C.cmod_call_deinit(mod)
		C.dlclose(handle)
		return fmt.Errorf("load C plugin %q: Init() returned error code %d", path, int(rc))
	}

	l.cModules = append(l.cModules, cm)
	l.logger.Info("C plugin loaded and initialized", "path", path, "name", name)

	return nil
}

// startCModules calls Start() on all loaded C plugin modules.
func (l *Launcher) startCModules() error {
	for _, cm := range l.cModules {
		rc := C.cmod_call_start(cm.mod)
		if rc != 0 {
			return fmt.Errorf("C module Start() returned error code %d", int(rc))
		}
	}
	return nil
}

// stopCModules calls Stop() on all loaded C plugin modules in reverse order.
func (l *Launcher) stopCModules() {
	for i := len(l.cModules) - 1; i >= 0; i-- {
		C.cmod_call_stop(l.cModules[i].mod)
	}
}

// deinitCModules calls DeInit() on all loaded C plugin modules in reverse
// order, closes the dlopen handles, and frees all arena-tracked strings.
func (l *Launcher) deinitCModules() {
	for i := len(l.cModules) - 1; i >= 0; i-- {
		cm := l.cModules[i]
		C.cmod_call_deinit(cm.mod)
		if cm.handle != nil {
			C.dlclose(unsafe.Pointer(cm.handle))
		}
	}
	// Free arena strings after all modules have been deinitialized.
	for _, p := range l.cModArena {
		C.free(p)
	}
	l.cModArena = nil
}
