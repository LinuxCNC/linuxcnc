package ads

import (
	"encoding/binary"
	"testing"
)

// mockPin is a simple in-memory PinAccessor for testing.
type mockPin struct {
	typeName string
	typeID   uint32
	size     uint32
	data     []byte
}

func (m *mockPin) ReadBytes() ([]byte, error) {
	out := make([]byte, len(m.data))
	copy(out, m.data)
	return out, nil
}

func (m *mockPin) WriteBytes(data []byte) error {
	m.data = make([]byte, len(data))
	copy(m.data, data)
	return nil
}

func (m *mockPin) Size() uint32    { return m.size }
func (m *mockPin) TypeName() string { return m.typeName }
func (m *mockPin) TypeID() uint32  { return m.typeID }

func newBoolPin(val bool) *mockPin {
	b := byte(0)
	if val {
		b = 1
	}
	return &mockPin{typeName: "BOOL", typeID: ADSTBool, size: 1, data: []byte{b}}
}

func newDintPin(val int32) *mockPin {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, uint32(val))
	return &mockPin{typeName: "DINT", typeID: ADSTInt32, size: 4, data: b}
}

func TestSymbolTableRegisterAndGetByName(t *testing.T) {
	st := NewSymbolTable()
	p := newBoolPin(true)
	sym := st.Register("stFoo.bReady", p)

	if sym.Name != "stFoo.bReady" {
		t.Errorf("Name = %q", sym.Name)
	}
	if sym.IndexGroup != IdxGrpProcessImageRW {
		t.Errorf("IndexGroup = 0x%X", sym.IndexGroup)
	}
	if sym.IndexOffset != 0 {
		t.Errorf("first symbol offset should be 0, got %d", sym.IndexOffset)
	}

	got := st.GetByName("stFoo.bReady")
	if got != sym {
		t.Error("GetByName returned wrong symbol")
	}

	// Second symbol offset should advance by size of first.
	p2 := newDintPin(42)
	sym2 := st.Register("stFoo.nVal", p2)
	if sym2.IndexOffset != 1 { // 1 byte for the bool
		t.Errorf("second symbol offset = %d, want 1", sym2.IndexOffset)
	}
}

func TestSymbolTableHandleLifecycle(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stX.bFlag", newBoolPin(false))

	handle, errCode := st.CreateHandle("stX.bFlag")
	if errCode != ErrNoError {
		t.Fatalf("CreateHandle error: 0x%X", errCode)
	}
	if handle == 0 {
		t.Error("handle should not be 0")
	}

	sym := st.GetByHandle(handle)
	if sym == nil || sym.Name != "stX.bFlag" {
		t.Errorf("GetByHandle returned %v", sym)
	}

	// Release and verify.
	if errCode := st.ReleaseHandle(handle); errCode != ErrNoError {
		t.Errorf("ReleaseHandle error: 0x%X", errCode)
	}
	if st.GetByHandle(handle) != nil {
		t.Error("handle should be nil after release")
	}
}

func TestSymbolTableReadDataByHandle(t *testing.T) {
	st := NewSymbolTable()
	p := newDintPin(12345)
	st.Register("stA.nVal", p)

	handle, _ := st.CreateHandle("stA.nVal")

	data, errCode := st.ReadData(IdxGrpSymbolValueByHandle, handle, 4)
	if errCode != ErrNoError {
		t.Fatalf("ReadData error: 0x%X", errCode)
	}
	if len(data) != 4 {
		t.Fatalf("ReadData length = %d, want 4", len(data))
	}
	val := int32(binary.LittleEndian.Uint32(data))
	if val != 12345 {
		t.Errorf("ReadData value = %d, want 12345", val)
	}
}

func TestSymbolTableWriteDataByHandle(t *testing.T) {
	st := NewSymbolTable()
	p := newBoolPin(false)
	st.Register("stA.bFlag", p)

	handle, _ := st.CreateHandle("stA.bFlag")

	errCode := st.WriteData(IdxGrpSymbolValueByHandle, handle, []byte{1})
	if errCode != ErrNoError {
		t.Fatalf("WriteData error: 0x%X", errCode)
	}
	if p.data[0] != 1 {
		t.Errorf("pin value after write = %d, want 1", p.data[0])
	}
}

func TestSymbolTableHandleByName(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stB.nCount", newDintPin(0))

	// ReadWriteData with IdxGrpSymbolHandleByName should return a 4-byte handle.
	data, errCode := st.ReadWriteData(IdxGrpSymbolHandleByName, 0, 4, []byte("stB.nCount"))
	if errCode != ErrNoError {
		t.Fatalf("ReadWriteData error: 0x%X", errCode)
	}
	if len(data) != 4 {
		t.Fatalf("expected 4-byte handle, got %d bytes", len(data))
	}
	handle := binary.LittleEndian.Uint32(data)
	if handle == 0 {
		t.Error("handle should not be 0")
	}
	// Verify the handle is valid.
	sym := st.GetByHandle(handle)
	if sym == nil || sym.Name != "stB.nCount" {
		t.Errorf("GetByHandle(%d) = %v", handle, sym)
	}
}

func TestSymbolTableReleaseHandleViaWrite(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stC.bX", newBoolPin(true))

	handle, _ := st.CreateHandle("stC.bX")

	// Release via WriteData with IdxGrpReleaseHandle.
	handleBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(handleBytes, handle)
	errCode := st.WriteData(IdxGrpReleaseHandle, 0, handleBytes)
	if errCode != ErrNoError {
		t.Fatalf("release handle via WriteData error: 0x%X", errCode)
	}
	if st.GetByHandle(handle) != nil {
		t.Error("handle should be nil after release")
	}
}

func TestSymbolTableSymbolCount(t *testing.T) {
	st := NewSymbolTable()
	st.Register("s1", newBoolPin(false))
	st.Register("s2", newDintPin(0))

	data, errCode := st.ReadData(IdxGrpSymbolCount, 0, 4)
	if errCode != ErrNoError {
		t.Fatalf("ReadData SymbolCount error: 0x%X", errCode)
	}
	count := binary.LittleEndian.Uint32(data)
	if count != 2 {
		t.Errorf("SymbolCount = %d, want 2", count)
	}
}

func TestSymbolTableProcessImage(t *testing.T) {
	st := NewSymbolTable()
	p1 := newBoolPin(true)
	p2 := newDintPin(99)
	sym1 := st.Register("s1", p1) // offset 0, size 1
	sym2 := st.Register("s2", p2) // offset 1, size 4

	// Read s1 by process image offset.
	data, errCode := st.ReadData(IdxGrpProcessImageRW, sym1.IndexOffset, 1)
	if errCode != ErrNoError {
		t.Fatalf("read s1 error: 0x%X", errCode)
	}
	if data[0] != 1 {
		t.Errorf("s1 value = %d, want 1", data[0])
	}

	// Write s2 by process image offset.
	newVal := make([]byte, 4)
	negOne := int32(-1)
	binary.LittleEndian.PutUint32(newVal, uint32(negOne))
	errCode = st.WriteData(IdxGrpProcessImageRW, sym2.IndexOffset, newVal)
	if errCode != ErrNoError {
		t.Fatalf("write s2 error: 0x%X", errCode)
	}
	if int32(binary.LittleEndian.Uint32(p2.data)) != -1 {
		t.Errorf("s2 value = %d, want -1", int32(binary.LittleEndian.Uint32(p2.data)))
	}
}
