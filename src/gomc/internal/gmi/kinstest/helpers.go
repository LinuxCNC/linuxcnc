package kinstest

// #cgo CFLAGS: -I${SRCDIR}/../../../generated/gmi/kins -I${SRCDIR}/../../../pkg/cmodule
// #cgo LDFLAGS: -ldl
//
// #include <dlfcn.h>
// #include <stdlib.h>
// #include "kins_api.h"
// #include "gomc_env.h"
//
// // Helpers to call through function pointers (cgo can't do it directly).
// static int32_t call_forward(kins_forward_fn fn, void *ctx,
//     const double joints[KINS_MAX_JOINTS], kins_pose_t *world,
//     uint64_t fflags, uint64_t *iflags) {
//     return fn(ctx, joints, world, fflags, iflags);
// }
// static int32_t call_inverse(kins_inverse_fn fn, void *ctx,
//     const kins_pose_t *world, double joints[KINS_MAX_JOINTS],
//     uint64_t iflags, uint64_t *fflags) {
//     return fn(ctx, world, joints, iflags, fflags);
// }
// static kins_kinematics_type_t call_type(kins_type_fn fn, void *ctx) {
//     return fn(ctx);
// }
// static int32_t call_switchable(kins_switchable_fn fn, void *ctx) {
//     return fn(ctx);
// }
//
// // --- Stub sub-APIs for testing ---
//
// // Log ring
// static gomc_log_ring_t *test_ring = NULL;
// static gomc_log_t       stub_log;
//
// static void init_stub_log(void) {
//     if (!test_ring) {
//         test_ring = gomc_ring_create();
//         stub_log.ring = test_ring;
//     }
// }
//
// // INI stub
// static const char *stub_ini_get(void *ctx, const char *section, const char *key) {
//     (void)ctx; (void)section; (void)key;
//     return NULL;
// }
// static const char **stub_ini_get_all(void *ctx, const char *section, const char *key, int *out) {
//     (void)ctx; (void)section; (void)key;
//     *out = 0;
//     return NULL;
// }
// static const char *stub_ini_source(void *ctx) {
//     (void)ctx;
//     return "/dev/null";
// }
// static gomc_ini_t stub_ini = {
//     .ctx         = NULL,
//     .get         = stub_ini_get,
//     .get_all     = stub_ini_get_all,
//     .source_file = stub_ini_source,
// };
//
// // API registry callbacks — forward-declared, implemented in Go via //export.
// extern int test_api_register_cb(void *ctx, char *api_name, int version,
//                                 char *instance_name, void *callbacks);
// extern void *test_api_get_cb(void *ctx, char *api_name, int version,
//                              char *instance_name);
//
// static gomc_api_t stub_api = {
//     .ctx          = NULL,
//     .register_api = (int(*)(void*,const char*,int,const char*,const void*))test_api_register_cb,
//     .get_api      = (const void*(*)(void*,const char*,int,const char*))test_api_get_cb,
// };
//
// // Combined env
// static cmod_env_t stub_env;
//
// static void init_stub_env(void) {
//     init_stub_log();
//     stub_env.dl_handle = NULL;
//     stub_env.log       = &stub_log;
//     stub_env.ini       = &stub_ini;
//     stub_env.hal       = NULL;
//     stub_env.rtapi     = NULL;
//     stub_env.api       = &stub_api;
// }
//
// // Load the .so via dlopen and call its New() function.
// static int load_trivkins(const char *so_path, cmod_t **out) {
//     init_stub_env();
//     void *handle = dlopen(so_path, RTLD_NOW);
//     if (!handle) return -1;
//
//     cmod_new_fn factory = (cmod_new_fn)dlsym(handle, "New");
//     if (!factory) { dlclose(handle); return -2; }
//
//     stub_env.dl_handle = handle;
//     return factory(&stub_env, "trivkins", 0, NULL, out);
// }
//
// static void unload_trivkins(cmod_t *mod) {
//     if (mod) {
//         if (mod->Stop) mod->Stop(mod);
//         if (mod->Destroy) mod->Destroy(mod);
//     }
// }
//
// // --- HAL stub for modules that need pins ---
//
// #define STUB_HAL_MAX_PINS 64
// static double stub_hal_pin_storage[STUB_HAL_MAX_PINS];
// static int    stub_hal_pin_count = 0;
//
// static int stub_hal_init(void *ctx, const char *name,
//                          void *dl_handle, int type) {
//     (void)ctx; (void)name; (void)dl_handle; (void)type;
//     return 1; // fake comp_id
// }
//
// static void stub_hal_exit(void *ctx, int comp_id) {
//     (void)ctx; (void)comp_id;
// }
//
// static int stub_hal_ready(void *ctx, int comp_id) {
//     (void)ctx; (void)comp_id;
//     return 0;
// }
//
// static void *stub_hal_malloc(void *ctx, long size) {
//     (void)ctx;
//     return calloc(1, size);
// }
//
// static int stub_hal_pin_new(void *ctx, const char *name, int type,
//                             int dir, void **data_ptr_addr, int comp_id) {
//     (void)ctx; (void)name; (void)type; (void)dir; (void)comp_id;
//     if (stub_hal_pin_count >= STUB_HAL_MAX_PINS) return -1;
//     *data_ptr_addr = &stub_hal_pin_storage[stub_hal_pin_count++];
//     return 0;
// }
//
// static int stub_hal_param_new(void *ctx, const char *name, int type,
//                               int dir, void *data_addr, int comp_id) {
//     (void)ctx; (void)name; (void)type; (void)dir;
//     (void)data_addr; (void)comp_id;
//     return 0;
// }
//
// static int stub_hal_export_funct(void *ctx, const char *name,
//                                  void (*funct)(void *, long),
//                                  void *arg, int uses_fp, int reentrant,
//                                  int comp_id) {
//     (void)ctx; (void)name; (void)funct; (void)arg;
//     (void)uses_fp; (void)reentrant; (void)comp_id;
//     return 0;
// }
//
// static gomc_hal_t stub_hal = {
//     .ctx          = NULL,
//     .init         = stub_hal_init,
//     .exit         = stub_hal_exit,
//     .ready        = stub_hal_ready,
//     .malloc       = stub_hal_malloc,
//     .pin_new      = stub_hal_pin_new,
//     .param_new    = stub_hal_param_new,
//     .export_funct = stub_hal_export_funct,
// };
//
// static void reset_stub_hal(void) {
//     stub_hal_pin_count = 0;
//     memset(stub_hal_pin_storage, 0, sizeof(stub_hal_pin_storage));
// }
//
// // Combined env with HAL stub enabled
// static cmod_env_t stub_env_hal;
//
// static void init_stub_env_hal(void) {
//     init_stub_log();
//     reset_stub_hal();
//     stub_env_hal.dl_handle = NULL;
//     stub_env_hal.log       = &stub_log;
//     stub_env_hal.ini       = &stub_ini;
//     stub_env_hal.hal       = &stub_hal;
//     stub_env_hal.rtapi     = NULL;
//     stub_env_hal.api       = &stub_api;
// }
//
// // Generic loader: loads any kins .so with optional HAL stub.
// static void *loaded_handle = NULL;
//
// static int load_kins_module(const char *so_path, const char *name,
//                             int need_hal, cmod_t **out) {
//     if (need_hal) {
//         init_stub_env_hal();
//     } else {
//         init_stub_env();
//     }
//     void *handle = dlopen(so_path, RTLD_NOW);
//     if (!handle) return -1;
//
//     cmod_new_fn factory = (cmod_new_fn)dlsym(handle, "New");
//     if (!factory) { dlclose(handle); return -2; }
//
//     loaded_handle = handle;
//     cmod_env_t *env = need_hal ? &stub_env_hal : &stub_env;
//     env->dl_handle = handle;
//     return factory(env, name, 0, NULL, out);
// }
//
// static void unload_kins_module(cmod_t *mod) {
//     if (mod) {
//         if (mod->Stop) mod->Stop(mod);
//         if (mod->Destroy) mod->Destroy(mod);
//     }
// }
import "C"

import (
	"os"
	"path/filepath"
	"runtime"
	"syscall"
	"unsafe"

	_ "github.com/sittner/linuxcnc/src/gomc/generated/gmi/kins" // registers KinsMeta via init()
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

// soPath finds a cmod .so relative to the source tree.
func soPath(name string) string {
	_, file, _, _ := runtime.Caller(0)
	launcherDir := filepath.Dir(filepath.Dir(filepath.Dir(filepath.Dir(file))))
	root := filepath.Dir(launcherDir) // src/
	root = filepath.Dir(root)         // linuxcnc/
	return filepath.Join(root, "cmod", name+".so")
}

// loadKinsModule loads any kins cmod .so and calls New().
func loadKinsModule(name string, needHAL bool) (*C.cmod_t, error) {
	path := soPath(name)
	if _, err := os.Stat(path); err != nil {
		return nil, err
	}
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	var mod *C.cmod_t
	var hal C.int
	if needHAL {
		hal = 1
	}
	rc := C.load_kins_module(cpath, cname, hal, &mod)
	if rc != 0 {
		return nil, os.ErrInvalid
	}
	return mod, nil
}

// loadTrivkins loads the trivkins.so and calls New().
func loadTrivkins() (*C.cmod_t, error) {
	return loadKinsModule("trivkins", false)
}

// unloadKinsModule cleans up any loaded kins module.
func unloadKinsModule(mod *C.cmod_t) {
	C.unload_kins_module(mod)
}

// unloadTrivkins cleans up.
func unloadTrivkins(mod *C.cmod_t) {
	unloadKinsModule(mod)
}

// getKinsCallbacks retrieves the kins callbacks from the Go registry.
func getKinsCallbacksFor(instance string) *C.kins_callbacks_t {
	reg := apiserver.DefaultRegistry()
	if reg == nil {
		return nil
	}
	cbs, err := reg.GetAPI("kins", instance, 1)
	if err != nil {
		return nil
	}
	return (*C.kins_callbacks_t)(cbs)
}

// getKinsCallbacks retrieves the kins callbacks for "trivkins" (legacy helper).
func getKinsCallbacks() *C.kins_callbacks_t {
	return getKinsCallbacksFor("trivkins")
}

// --- API registry callback implementations (exported to C for test stub) ---

//export test_api_register_cb
func test_api_register_cb(ctx unsafe.Pointer, apiName *C.char, version C.int,
	instanceName *C.char, callbacks unsafe.Pointer) C.int {

	reg := apiserver.DefaultRegistry()
	if reg == nil {
		return -C.int(syscall.EINVAL)
	}

	name := C.GoString(apiName)
	ver := int(version)
	instance := C.GoString(instanceName)

	err := reg.Register(name, ver, instance, callbacks)
	if err != nil {
		switch err {
		case syscall.EEXIST:
			return -C.int(syscall.EEXIST)
		default:
			return -C.int(syscall.EINVAL)
		}
	}
	return 0
}

//export test_api_get_cb
func test_api_get_cb(ctx unsafe.Pointer, apiName *C.char, version C.int,
	instanceName *C.char) unsafe.Pointer {

	reg := apiserver.DefaultRegistry()
	if reg == nil {
		return nil
	}

	instance := C.GoString(instanceName)
	ver := int(version)

	cbs, err := reg.GetAPI(C.GoString(apiName), instance, ver)
	if err != nil {
		return nil
	}
	return cbs
}

// callForward calls the forward kinematics through function pointers.
func callForward(cbs *C.kins_callbacks_t, joints [16]float64) (C.kins_pose_t, int32) {
	var cJoints [16]C.double
	for i := 0; i < 16; i++ {
		cJoints[i] = C.double(joints[i])
	}
	var world C.kins_pose_t
	var fflags, iflags C.uint64_t
	out := C.call_forward(cbs.forward, nil, &cJoints[0], &world, fflags, &iflags)
	return world, int32(out)
}

// callInverse calls the inverse kinematics through function pointers.
func callInverse(cbs *C.kins_callbacks_t, world C.kins_pose_t) ([16]float64, int32) {
	var cJoints [16]C.double
	var iflags, fflags C.uint64_t
	out := C.call_inverse(cbs.inverse, nil, &world, &cJoints[0], iflags, &fflags)
	var joints [16]float64
	for i := 0; i < 16; i++ {
		joints[i] = float64(cJoints[i])
	}
	return joints, int32(out)
}

// callType retrieves the kinematics type.
func callType(cbs *C.kins_callbacks_t) (int, int32) {
	out := C.call_type(cbs._type, nil)
	return int(out), 0
}

// callSwitchable checks if kinematics are switchable.
func callSwitchable(cbs *C.kins_callbacks_t) int32 {
	out := C.call_switchable(cbs.switchable, nil)
	return int32(out)
}
