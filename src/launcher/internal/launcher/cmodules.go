// Package launcher — cmodules.go handles loading, lifecycle management of
// C plugin .so files loaded via the "load" HAL command.
//
// C plugins are resolved from EMC2_CMOD_DIR (bare names) and loaded via
// dlopen/dlsym.  They share the same lifecycle as Go plugins:
//
//	New(env, name, args) → Start() → Stop() → Destroy()
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
static void cmod_env_init(cmod_env_t *env, void *ctx, void *dl_handle) {
    env->ctx             = ctx;
    env->dl_handle       = dl_handle;
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

// cmod_call_start calls the module's Start function.
static int cmod_call_start(cmod_t *m) {
    return m->Start(m);
}

// cmod_call_stop calls the module's Stop function.
static void cmod_call_stop(cmod_t *m) {
    m->Stop(m);
}

// cmod_call_destroy calls the module's Destroy function.
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
	hCtx    cgo.Handle // Go↔C handle for the Launcher pointer
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

// Callback implementations — these are Go functions exported to C via
// //export.  They are called from C code via function pointers in cmod_env_t.
// Arena-tracked strings returned by get_ini and ini_source_file are freed
// in destroyCModules.

//export cmod_get_ini
func cmod_get_ini(ctx unsafe.Pointer, section, key *C.char) *C.char {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
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
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	cs := C.CString(l.ini.SourceFile())
	l.cModArena = append(l.cModArena, unsafe.Pointer(cs))
	return cs
}

//export cmod_log_info
func cmod_log_info(ctx unsafe.Pointer, component, msg *C.char) {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	l.logger.Info(C.GoString(msg), "component", C.GoString(component))
}

//export cmod_log_warn
func cmod_log_warn(ctx unsafe.Pointer, component, msg *C.char) {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	l.logger.Warn(C.GoString(msg), "component", C.GoString(component))
}

//export cmod_log_error
func cmod_log_error(ctx unsafe.Pointer, component, msg *C.char) {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	l.logger.Error(C.GoString(msg), "component", C.GoString(component))
}

//export cmod_log_debug
func cmod_log_debug(ctx unsafe.Pointer, component, msg *C.char) {
	l := cgo.Handle(uintptr(ctx)).Value().(*Launcher)
	l.logger.Debug(C.GoString(msg), "component", C.GoString(component))
}

// loadCPlugin loads a C plugin .so via dlopen, looks up the "New" symbol,
// builds the cmod_env_t with launcher callbacks, calls the factory, and
// appends the module to l.cModules.
//
// The factory is expected to create and fully initialize the module (including
// HAL component/pin creation) before returning.
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
		name:   name,
	}

	env := (*C.cmod_env_t)(C.malloc(C.size_t(unsafe.Sizeof(C.cmod_env_t{}))))
	hCtx := cgo.NewHandle(l)
	C.cmod_env_init(env, unsafe.Pointer(uintptr(hCtx)), handle)
	l.cModArena = append(l.cModArena, unsafe.Pointer(env))

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
// order, unlocks and closes the dlopen handles, and frees all arena-tracked
// strings.
func (l *Launcher) destroyCModules() {
	l.unlockCModules()
	for i := len(l.cModules) - 1; i >= 0; i-- {
		cm := l.cModules[i]
		C.cmod_call_destroy(cm.mod)
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
}
