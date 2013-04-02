package ring

/*
#cgo LDFLAGS: -lring
#include "ring.h"
*/
import "C"

import "unsafe"

type Overflow struct {}

func (e Overflow) Error() string { return "Ring overflow" }

type Ring struct {
	ring C.ringbuffer_t
}

func New(size int) *Ring {
	var ring Ring
	C.ring_init(&ring.ring, (C.size_t)(size), unsafe.Pointer(uintptr(0)))
	return &ring
}

func (ring * Ring) WriteString(data string) error {
	if len(data) == 0 {
		return ring.Write(make([]byte, 0))
	}
	return ring.Write(([]byte) (data))
}
func (ring * Ring) Write(data []byte) error {
	var err C.int
	if len(data) == 0 {
		err = C.ring_write(&ring.ring, unsafe.Pointer(uintptr(0)), (C.size_t)(0))
	} else {
		err = C.ring_write(&ring.ring, unsafe.Pointer(&data[0]), (C.size_t)(len(data)))
	}
	if err != 0 { return (Overflow{}) }
	return nil
}

func (ring * Ring) NextSize() int {
	return (int)(C.ring_next_size(&ring.ring))
}

func (ring * Ring) NextString() (*string) {
	size := ring.NextSize()
	if size == -1 { return nil }
	data := C.GoStringN((*C.char)(C.ring_next(&ring.ring)), (C.int)(size))
	C.ring_shift(&ring.ring)
	return &data
}

func (ring * Ring) Next() ([]byte) {
	data := ring.Next()
	if data == nil { return nil }
	if len(data) == 0 { return make([]byte, 0) }
	return ([]byte)(data)
}

/*
//func uuid_clear(u UUID) { C.uuid_clear(&u.uuid[0]) }
func Generate() UUID {
	var uuid = UUID {}
	C.uuid_generate(&uuid.uuid[0])
	return uuid
}

func (uuid * UUID) Dump() string {
	var buf = make([]byte, 40)
	C.uuid_unparse(&uuid.uuid[0], (*C.char) (unsafe.Pointer(&buf[0])))
	return string(buf)
}
*/
