// Package directtest provides cgo helpers for testing direct C function
// pointer calls through the apiserver registry. The cgo import must live
// in a non-test file.
package directtest

/*
#include <string.h>
#include <stdint.h>

// Minimal kinematics-like API for testing.

typedef struct {
    double x, y, z;
} test_pose_t;

typedef int (*test_forward_fn)(const double joints[3], test_pose_t *world);
typedef int (*test_inverse_fn)(const test_pose_t *world, double joints[3]);
typedef int (*test_type_fn)(int32_t *out);

typedef struct {
    test_forward_fn forward;
    test_inverse_fn inverse;
    test_type_fn get_type;
} test_kins_callbacks_t;

// Trivial identity kinematics: joints[0]=x, joints[1]=y, joints[2]=z
static int trivkins_forward(const double joints[3], test_pose_t *world) {
    world->x = joints[0];
    world->y = joints[1];
    world->z = joints[2];
    return 0;
}

static int trivkins_inverse(const test_pose_t *world, double joints[3]) {
    joints[0] = world->x;
    joints[1] = world->y;
    joints[2] = world->z;
    return 0;
}

static int trivkins_type(int32_t *out) {
    *out = 1; // IDENTITY
    return 0;
}

static test_kins_callbacks_t make_trivkins_callbacks(void) {
    test_kins_callbacks_t cbs;
    cbs.forward = trivkins_forward;
    cbs.inverse = trivkins_inverse;
    cbs.get_type = trivkins_type;
    return cbs;
}

// cgo can't call C function pointers directly — use static wrappers.
static int call_forward(test_forward_fn fn, const double joints[3], test_pose_t *world) {
    return fn(joints, world);
}
static int call_inverse(test_inverse_fn fn, const test_pose_t *world, double joints[3]) {
    return fn(world, joints);
}
static int call_type(test_type_fn fn, int32_t *out) {
    return fn(out);
}
*/
import "C"
import "unsafe"

// Callbacks wraps the C test_kins_callbacks_t.
type Callbacks = C.test_kins_callbacks_t

// Pose wraps the C test_pose_t.
type Pose = C.test_pose_t

// MakeCallbacks returns a populated trivkins callbacks struct.
func MakeCallbacks() Callbacks {
	return C.make_trivkins_callbacks()
}

// CallbacksPtr returns an unsafe.Pointer to the given callbacks.
func CallbacksPtr(cbs *Callbacks) unsafe.Pointer {
	return unsafe.Pointer(cbs)
}

// Forward calls through the function pointer.
func Forward(cbs *Callbacks, joints [3]float64) (Pose, int) {
	var cJoints [3]C.double
	cJoints[0] = C.double(joints[0])
	cJoints[1] = C.double(joints[1])
	cJoints[2] = C.double(joints[2])
	var world C.test_pose_t
	rc := C.call_forward(cbs.forward, &cJoints[0], &world)
	return world, int(rc)
}

// Inverse calls through the function pointer.
func Inverse(cbs *Callbacks, world Pose) ([3]float64, int) {
	var joints [3]C.double
	rc := C.call_inverse(cbs.inverse, &world, &joints[0])
	return [3]float64{float64(joints[0]), float64(joints[1]), float64(joints[2])}, int(rc)
}

// GetType calls through the function pointer.
func GetType(cbs *Callbacks) (int32, int) {
	var out C.int32_t
	rc := C.call_type(cbs.get_type, &out)
	return int32(out), int(rc)
}

// PoseXYZ extracts x,y,z from a Pose.
func PoseXYZ(p Pose) (float64, float64, float64) {
	return float64(p.x), float64(p.y), float64(p.z)
}
