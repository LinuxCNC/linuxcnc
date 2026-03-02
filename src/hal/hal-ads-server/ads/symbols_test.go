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

	// Release via WriteData with IdxGrpReleaseHandle: handle is passed as indexOffset.
	errCode := st.WriteData(IdxGrpReleaseHandle, handle, nil)
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

func TestSymbolTableNullTerminatedName(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.bFlag", newBoolPin(true))

	// TwinCAT sends null-terminated symbol names; the server must strip them.
	nameWithNull := []byte("stFoo.bFlag\x00")
	data, errCode := st.ReadWriteData(IdxGrpSymbolHandleByName, 0, 4, nameWithNull)
	if errCode != ErrNoError {
		t.Fatalf("CreateHandle with null-terminated name error: 0x%X", errCode)
	}
	handle := binary.LittleEndian.Uint32(data)
	if st.GetByHandle(handle) == nil {
		t.Error("handle should resolve to a symbol")
	}
}

func TestSymbolTableCompactSymbolInfo(t *testing.T) {
	st := NewSymbolTable()
	sym := st.Register("stFoo.nVal", newDintPin(0))

	// Request compact form (readLen == 12).
	data, errCode := st.ReadWriteData(IdxGrpSymbolInfoByName, 0, 12, []byte("stFoo.nVal"))
	if errCode != ErrNoError {
		t.Fatalf("compact symbol info error: 0x%X", errCode)
	}
	if len(data) != 12 {
		t.Fatalf("expected 12 bytes, got %d", len(data))
	}
	ig := binary.LittleEndian.Uint32(data[0:4])
	io := binary.LittleEndian.Uint32(data[4:8])
	size := binary.LittleEndian.Uint32(data[8:12])
	if ig != sym.IndexGroup {
		t.Errorf("IndexGroup = 0x%X, want 0x%X", ig, sym.IndexGroup)
	}
	if io != sym.IndexOffset {
		t.Errorf("IndexOffset = %d, want %d", io, sym.IndexOffset)
	}
	if size != 4 {
		t.Errorf("Size = %d, want 4", size)
	}
}

func TestSymbolTableSumRead(t *testing.T) {
	st := NewSymbolTable()
	p1 := newBoolPin(true)
	p2 := newDintPin(42)
	sym1 := st.Register("s1", p1) // offset 0, size 1
	sym2 := st.Register("s2", p2) // offset 1, size 4

	// Build a SumRead request for both symbols.
	writeData := make([]byte, 24) // 2 × 12 bytes
	binary.LittleEndian.PutUint32(writeData[0:], sym1.IndexGroup)
	binary.LittleEndian.PutUint32(writeData[4:], sym1.IndexOffset)
	binary.LittleEndian.PutUint32(writeData[8:], sym1.Accessor.Size())
	binary.LittleEndian.PutUint32(writeData[12:], sym2.IndexGroup)
	binary.LittleEndian.PutUint32(writeData[16:], sym2.IndexOffset)
	binary.LittleEndian.PutUint32(writeData[20:], sym2.Accessor.Size())

	resp, errCode := st.ReadWriteData(IdxGrpSumRead, 2, 0, writeData)
	if errCode != ErrNoError {
		t.Fatalf("SumRead error: 0x%X", errCode)
	}
	// Response: 2×4 error codes + 1 byte (bool) + 4 bytes (dint) = 13 bytes.
	if len(resp) != 13 {
		t.Fatalf("SumRead response length = %d, want 13", len(resp))
	}
	if binary.LittleEndian.Uint32(resp[0:4]) != ErrNoError {
		t.Errorf("SumRead s1 errCode = 0x%X", binary.LittleEndian.Uint32(resp[0:4]))
	}
	if binary.LittleEndian.Uint32(resp[4:8]) != ErrNoError {
		t.Errorf("SumRead s2 errCode = 0x%X", binary.LittleEndian.Uint32(resp[4:8]))
	}
	// s1 value: byte 8.
	if resp[8] != 1 {
		t.Errorf("SumRead s1 value = %d, want 1", resp[8])
	}
	// s2 value: bytes 9–12.
	if int32(binary.LittleEndian.Uint32(resp[9:13])) != 42 {
		t.Errorf("SumRead s2 value = %d, want 42", int32(binary.LittleEndian.Uint32(resp[9:13])))
	}
}

func TestSymbolTableFallbackMatching(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.bFlag", newBoolPin(true))

	// Prefix stripping: "GVL.stFoo.bFlag" should resolve to "stFoo.bFlag".
	handle, errCode := st.CreateHandle("GVL.stFoo.bFlag")
	if errCode != ErrNoError {
		t.Fatalf("fallback (prefix) CreateHandle error: 0x%X", errCode)
	}
	if st.GetByHandle(handle) == nil {
		t.Error("fallback prefix: handle should resolve")
	}

	// Case-insensitive: "STFOO.BFLAG" should resolve.
	handle2, errCode2 := st.CreateHandle("STFOO.BFLAG")
	if errCode2 != ErrNoError {
		t.Fatalf("fallback (case) CreateHandle error: 0x%X", errCode2)
	}
	if st.GetByHandle(handle2) == nil {
		t.Error("fallback case: handle should resolve")
	}
}

// TestGroupSymbolAutoCreate verifies that registering a leaf creates group
// symbols for every ancestor path prefix.
func TestGroupSymbolAutoCreate(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.stBar.nVal", newDintPin(0))
	st.Register("stFoo.stBar.bFlag", newBoolPin(false))

	for _, prefix := range []string{"stFoo", "stFoo.stBar"} {
		sym := st.GetByName(prefix)
		if sym == nil {
			t.Errorf("group symbol %q not created", prefix)
			continue
		}
		if sym.IndexGroup != IdxGrpProcessImageRW {
			t.Errorf("%q IndexGroup = 0x%X, want 0x%X", prefix, sym.IndexGroup, IdxGrpProcessImageRW)
		}
		if _, ok := sym.Accessor.(*groupAccessor); !ok {
			t.Errorf("%q Accessor is not *groupAccessor", prefix)
		}
	}
}

// TestGroupSymbolInfoByName verifies SymbolInfoByName (0xF007) for a group symbol.
func TestGroupSymbolInfoByName(t *testing.T) {
	st := NewSymbolTable()
	nVal := st.Register("stFoo.stBar.nVal", newDintPin(0)) // offset 0, size 4
	st.Register("stFoo.stBar.bFlag", newBoolPin(false))    // offset 4, size 1

	data, errCode := st.ReadWriteData(IdxGrpSymbolInfoByName, 0, 12, []byte("stFoo.stBar"))
	if errCode != ErrNoError {
		t.Fatalf("SymbolInfoByName for group error: 0x%X", errCode)
	}
	if len(data) != 12 {
		t.Fatalf("expected 12 bytes, got %d", len(data))
	}
	ig := binary.LittleEndian.Uint32(data[0:4])
	io := binary.LittleEndian.Uint32(data[4:8])
	size := binary.LittleEndian.Uint32(data[8:12])
	if ig != IdxGrpProcessImageRW {
		t.Errorf("IndexGroup = 0x%X, want 0x%X", ig, IdxGrpProcessImageRW)
	}
	if io != nVal.IndexOffset {
		t.Errorf("IndexOffset = %d, want %d", io, nVal.IndexOffset)
	}
	if size != 5 { // 4 (DINT) + 1 (BOOL)
		t.Errorf("Size = %d, want 5", size)
	}
}

// TestGroupSymbolCreateHandle verifies CreateHandle succeeds for a group symbol.
func TestGroupSymbolCreateHandle(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.stBar.nVal", newDintPin(7))
	st.Register("stFoo.stBar.bFlag", newBoolPin(true))

	handle, errCode := st.CreateHandle("stFoo.stBar")
	if errCode != ErrNoError {
		t.Fatalf("CreateHandle for group error: 0x%X", errCode)
	}
	if handle == 0 {
		t.Error("group handle should not be 0")
	}
	sym := st.GetByHandle(handle)
	if sym == nil || sym.Name != "stFoo.stBar" {
		t.Errorf("GetByHandle returned %v", sym)
	}
}

// TestGroupSymbolReadByHandle verifies that reading a group handle returns
// the concatenated child bytes in offset order.
func TestGroupSymbolReadByHandle(t *testing.T) {
	st := NewSymbolTable()
	nValPin := newDintPin(42)
	bFlagPin := newBoolPin(true)
	st.Register("stFoo.stBar.nVal", nValPin)   // offset 0, size 4
	st.Register("stFoo.stBar.bFlag", bFlagPin) // offset 4, size 1

	handle, _ := st.CreateHandle("stFoo.stBar")

	data, errCode := st.ReadData(IdxGrpSymbolValueByHandle, handle, 5)
	if errCode != ErrNoError {
		t.Fatalf("ReadData group handle error: 0x%X", errCode)
	}
	if len(data) != 5 {
		t.Fatalf("group read length = %d, want 5", len(data))
	}
	if int32(binary.LittleEndian.Uint32(data[0:4])) != 42 {
		t.Errorf("nVal in group = %d, want 42", int32(binary.LittleEndian.Uint32(data[0:4])))
	}
	if data[4] != 1 {
		t.Errorf("bFlag in group = %d, want 1", data[4])
	}
}

// TestGroupSymbolWriteByHandle verifies that writing a group handle distributes
// the bytes to the correct child leaf symbols.
func TestGroupSymbolWriteByHandle(t *testing.T) {
	st := NewSymbolTable()
	nValPin := newDintPin(0)
	bFlagPin := newBoolPin(false)
	st.Register("stFoo.stBar.nVal", nValPin)   // offset 0, size 4
	st.Register("stFoo.stBar.bFlag", bFlagPin) // offset 4, size 1

	handle, _ := st.CreateHandle("stFoo.stBar")

	// Write [99, 0, 0, 0, 1] → nVal=99, bFlag=true.
	payload := make([]byte, 5)
	binary.LittleEndian.PutUint32(payload[0:], 99)
	payload[4] = 1

	errCode := st.WriteData(IdxGrpSymbolValueByHandle, handle, payload)
	if errCode != ErrNoError {
		t.Fatalf("WriteData group handle error: 0x%X", errCode)
	}
	if int32(binary.LittleEndian.Uint32(nValPin.data)) != 99 {
		t.Errorf("nVal after group write = %d, want 99", int32(binary.LittleEndian.Uint32(nValPin.data)))
	}
	if bFlagPin.data[0] != 1 {
		t.Errorf("bFlag after group write = %d, want 1", bFlagPin.data[0])
	}
}

// TestGroupSymbolArrayBracketNotation verifies that bracket notation in path
// segments (e.g. "aPools[1]") creates the correct group symbols.
func TestGroupSymbolArrayBracketNotation(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stData.aPools[1].stMsg.eType", newDintPin(0))

	for _, prefix := range []string{"stData", "stData.aPools[1]", "stData.aPools[1].stMsg"} {
		if sym := st.GetByName(prefix); sym == nil {
			t.Errorf("expected group symbol %q to exist", prefix)
		}
	}
}

// TestGroupSymbolNotInSymbolCount verifies that group symbols are not counted
// in the SymbolCount response.
func TestGroupSymbolNotInSymbolCount(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.stBar.nVal", newDintPin(0))
	st.Register("stFoo.stBar.bFlag", newBoolPin(false))

	data, errCode := st.ReadData(IdxGrpSymbolCount, 0, 4)
	if errCode != ErrNoError {
		t.Fatalf("SymbolCount error: 0x%X", errCode)
	}
	count := binary.LittleEndian.Uint32(data)
	if count != 2 {
		t.Errorf("SymbolCount = %d, want 2 (group symbols must not be counted)", count)
	}
}

// TestGroupSymbolFallbackMatching verifies that case-insensitive and
// prefix-stripped lookups work for group symbols too.
func TestGroupSymbolFallbackMatching(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.stBar.nVal", newDintPin(0))

	// Case-insensitive lookup for the group.
	handle, errCode := st.CreateHandle("STFOO.STBAR")
	if errCode != ErrNoError {
		t.Fatalf("case-insensitive group CreateHandle error: 0x%X", errCode)
	}
	sym := st.GetByHandle(handle)
	if sym == nil || sym.Name != "stFoo.stBar" {
		t.Errorf("expected stFoo.stBar, got %v", sym)
	}
}

// zeroPadPin is a PinAccessor that mimics padAccessor: reads return zeros,
// writes are silently discarded. Used to test pad-symbol offset accounting.
type zeroPadPin struct {
	size     uint32
	typeName string
	typeID   uint32
}

func (z *zeroPadPin) ReadBytes() ([]byte, error) { return make([]byte, z.size), nil }
func (z *zeroPadPin) WriteBytes([]byte) error    { return nil }
func (z *zeroPadPin) Size() uint32               { return z.size }
func (z *zeroPadPin) TypeName() string           { return z.typeName }
func (z *zeroPadPin) TypeID() uint32             { return z.typeID }

// TestPadOffsetAdvance verifies that a padding symbol correctly advances
// the process-image offset for the symbols that follow it.
func TestPadOffsetAdvance(t *testing.T) {
	st := NewSymbolTable()
	pad := &zeroPadPin{size: 1, typeName: "BYTE", typeID: ADSTUInt8}
	sym1 := st.Register("stMsg._reserved1", pad)  // offset 0, size 1
	sym2 := st.Register("stMsg.eType", newDintPin(0)) // offset 1, size 4

	if sym1.IndexOffset != 0 {
		t.Errorf("pad symbol offset = %d, want 0", sym1.IndexOffset)
	}
	if sym2.IndexOffset != 1 {
		t.Errorf("eType offset = %d, want 1", sym2.IndexOffset)
	}
}

// TestPadReadReturnsZeros verifies that a pad accessor read returns zero bytes.
func TestPadReadReturnsZeros(t *testing.T) {
	pad := &zeroPadPin{size: 2, typeName: "WORD", typeID: ADSTUInt16}
	data, err := pad.ReadBytes()
	if err != nil {
		t.Fatalf("ReadBytes error: %v", err)
	}
	if len(data) != 2 {
		t.Fatalf("ReadBytes length = %d, want 2", len(data))
	}
	for i, b := range data {
		if b != 0 {
			t.Errorf("data[%d] = %d, want 0", i, b)
		}
	}
}

// TestPadWriteDiscards verifies that writing to a pad accessor has no effect.
func TestPadWriteDiscards(t *testing.T) {
	pad := &zeroPadPin{size: 1, typeName: "BYTE", typeID: ADSTUInt8}
	if err := pad.WriteBytes([]byte{0xFF}); err != nil {
		t.Fatalf("WriteBytes error: %v", err)
	}
	// Read back: should still be zero.
	data, _ := pad.ReadBytes()
	if data[0] != 0 {
		t.Errorf("after write, pad data = %d, want 0", data[0])
	}
}

// TestGroupWithPadding verifies that a group symbol containing a pad field
// returns the correct total size and zero-fills the pad bytes on reads.
func TestGroupWithPadding(t *testing.T) {
	st := NewSymbolTable()
	pad1 := &zeroPadPin{size: 1, typeName: "BYTE", typeID: ADSTUInt8}
	real := newDintPin(0x01020304)
	pad2 := &zeroPadPin{size: 2, typeName: "WORD", typeID: ADSTUInt16}

	st.Register("stMsg._reserved1", pad1) // offset 0, size 1
	st.Register("stMsg.nVal", real)        // offset 1, size 4
	st.Register("stMsg._align", pad2)     // offset 5, size 2

	// The group symbol stMsg should cover offsets 0–6 (7 bytes total).
	groupSym := st.GetByName("stMsg")
	if groupSym == nil {
		t.Fatal("group symbol stMsg not created")
	}
	if groupSym.Accessor.Size() != 7 {
		t.Errorf("group Size = %d, want 7", groupSym.Accessor.Size())
	}

	// Read the group: padding bytes should be zero, nVal bytes should be present.
	handle, errCode := st.CreateHandle("stMsg")
	if errCode != ErrNoError {
		t.Fatalf("CreateHandle error: 0x%X", errCode)
	}
	data, errCode := st.ReadData(IdxGrpSymbolValueByHandle, handle, 7)
	if errCode != ErrNoError {
		t.Fatalf("ReadData error: 0x%X", errCode)
	}
	if len(data) != 7 {
		t.Fatalf("ReadData length = %d, want 7", len(data))
	}
	// Byte 0: pad (_reserved1) → 0.
	if data[0] != 0 {
		t.Errorf("data[0] (pad) = %d, want 0", data[0])
	}
	// Bytes 1-4: nVal = 0x01020304 little-endian.
	gotVal := int32(binary.LittleEndian.Uint32(data[1:5]))
	if gotVal != 0x01020304 {
		t.Errorf("data[1:5] (nVal) = 0x%X, want 0x01020304", gotVal)
	}
	// Bytes 5-6: pad (_align) → 0.
	if data[5] != 0 || data[6] != 0 {
		t.Errorf("data[5:7] (pad) = %v, want [0 0]", data[5:7])
	}
}
