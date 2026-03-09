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

func (m *mockPin) Size() uint32       { return m.size }
func (m *mockPin) TypeName() string   { return m.typeName }
func (m *mockPin) TypeID() uint32     { return m.typeID }
func (m *mockPin) TypeGUID() [16]byte { return [16]byte{} }

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

	// Release via WriteData with IdxGrpReleaseHandle: handle is in the 4-byte write data payload.
	handleBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(handleBytes, handle)
	errCode := st.WriteData(IdxGrpReleaseHandle, 0, handleBytes)
	if errCode != ErrNoError {
		t.Fatalf("release handle via WriteData error: 0x%X", errCode)
	}
	if st.GetByHandle(handle) != nil {
		t.Error("handle should be nil after release")
	}

	// Short data payload should return an error.
	if errCode := st.WriteData(IdxGrpReleaseHandle, 0, []byte{0x01, 0x00}); errCode == ErrNoError {
		t.Error("expected error for short data payload, got ErrNoError")
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

// TestProcessImageRangeRead verifies that a ProcessImageRW bulk read returns
// all symbols whose offsets fall within the requested range.
func TestProcessImageRangeRead(t *testing.T) {
	st := NewSymbolTable()
	p1 := newBoolPin(true)       // offset 0, size 1
	p2 := newDintPin(0x12345678) // offset 1, size 4
	p3 := newDintPin(-1)         // offset 5, size 4
	st.Register("s1", p1)
	st.Register("s2", p2)
	st.Register("s3", p3)

	// Range read covering all 3 symbols (offset=0, length=9).
	data, errCode := st.ReadData(IdxGrpProcessImageRW, 0, 9)
	if errCode != ErrNoError {
		t.Fatalf("range read error: 0x%X", errCode)
	}
	if len(data) != 9 {
		t.Fatalf("range read length = %d, want 9", len(data))
	}
	if data[0] != 1 {
		t.Errorf("s1 = %d, want 1", data[0])
	}
	if binary.LittleEndian.Uint32(data[1:5]) != 0x12345678 {
		t.Errorf("s2 = 0x%X, want 0x12345678", binary.LittleEndian.Uint32(data[1:5]))
	}
	if int32(binary.LittleEndian.Uint32(data[5:9])) != -1 {
		t.Errorf("s3 = %d, want -1", int32(binary.LittleEndian.Uint32(data[5:9])))
	}
}

// TestProcessImageRangeWrite verifies that a ProcessImageRW bulk write
// distributes bytes to all symbols whose offsets fall within the range.
func TestProcessImageRangeWrite(t *testing.T) {
	st := NewSymbolTable()
	p1 := newBoolPin(false) // offset 0, size 1
	p2 := newDintPin(0)     // offset 1, size 4
	p3 := newDintPin(0)     // offset 5, size 4
	st.Register("s1", p1)
	st.Register("s2", p2)
	st.Register("s3", p3)

	// Range write: 9-byte payload covering all 3 symbols.
	payload := make([]byte, 9)
	payload[0] = 1
	binary.LittleEndian.PutUint32(payload[1:], 0xABCD1234)
	binary.LittleEndian.PutUint32(payload[5:], 0xFFEE0099)
	errCode := st.WriteData(IdxGrpProcessImageRW, 0, payload)
	if errCode != ErrNoError {
		t.Fatalf("range write error: 0x%X", errCode)
	}
	if p1.data[0] != 1 {
		t.Errorf("s1 after range write = %d, want 1", p1.data[0])
	}
	if binary.LittleEndian.Uint32(p2.data) != 0xABCD1234 {
		t.Errorf("s2 after range write = 0x%X, want 0xABCD1234", binary.LittleEndian.Uint32(p2.data))
	}
	if binary.LittleEndian.Uint32(p3.data) != 0xFFEE0099 {
		t.Errorf("s3 after range write = 0x%X, want 0xFFEE0099", binary.LittleEndian.Uint32(p3.data))
	}
}
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
func (z *zeroPadPin) TypeGUID() [16]byte         { return [16]byte{} }

// TestPadOffsetAdvance verifies that a padding symbol correctly advances
// the process-image offset for the symbols that follow it.
func TestPadOffsetAdvance(t *testing.T) {
	st := NewSymbolTable()
	pad := &zeroPadPin{size: 1, typeName: "BYTE", typeID: ADSTUInt8}
	sym1 := st.Register("stMsg._reserved1", pad)      // offset 0, size 1
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
	st.Register("stMsg.nVal", real)       // offset 1, size 4
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

// TestRegisterAt verifies that RegisterAt places a symbol at an explicit
// byte offset, updates nextOffset, and creates parent group symbols.
func TestRegisterAt(t *testing.T) {
	st := NewSymbolTable()

	// Place a DINT at offset 4 (leaving a 4-byte gap at 0..3).
	p := newDintPin(99)
	sym := st.RegisterAt("stFoo.nVal", 4, p)

	if sym.IndexOffset != 4 {
		t.Errorf("IndexOffset = %d, want 4", sym.IndexOffset)
	}
	if sym.IndexGroup != IdxGrpProcessImageRW {
		t.Errorf("IndexGroup = 0x%X", sym.IndexGroup)
	}

	// nextOffset should be 4+4=8.
	// Verify by placing another symbol at auto-incremented offset would use Register.
	p2 := newBoolPin(true)
	sym2 := st.RegisterAt("stFoo.bFlag", 8, p2)
	if sym2.IndexOffset != 8 {
		t.Errorf("second symbol offset = %d, want 8", sym2.IndexOffset)
	}

	// Group symbol for "stFoo" should exist.
	grp := st.GetByName("stFoo")
	if grp == nil {
		t.Fatal("group symbol stFoo not created by RegisterAt")
	}
	if _, ok := grp.Accessor.(*groupAccessor); !ok {
		t.Error("stFoo accessor is not *groupAccessor")
	}

	// GetByName should return the leaf symbol.
	got := st.GetByName("stFoo.nVal")
	if got == nil || got.IndexOffset != 4 {
		t.Errorf("GetByName(stFoo.nVal) = %v", got)
	}

	// Symbol should appear in symbolOrder (part of symbol list).
	data, errCode := st.ReadData(IdxGrpSymbolCount, 0, 4)
	if errCode != ErrNoError {
		t.Fatalf("SymbolCount error: 0x%X", errCode)
	}
	count := binary.LittleEndian.Uint32(data)
	if count != 2 {
		t.Errorf("SymbolCount = %d, want 2", count)
	}
}

// TestRegisterAtExplicitOffsetRead verifies that reading a symbol registered
// with RegisterAt returns the correct value.
func TestRegisterAtExplicitOffsetRead(t *testing.T) {
	st := NewSymbolTable()
	p := newDintPin(42)
	sym := st.RegisterAt("stA.nVal", 8, p)

	handle, errCode := st.CreateHandle("stA.nVal")
	if errCode != ErrNoError {
		t.Fatalf("CreateHandle error: 0x%X", errCode)
	}

	data, errCode := st.ReadData(IdxGrpSymbolValueByHandle, handle, 4)
	if errCode != ErrNoError {
		t.Fatalf("ReadData error: 0x%X", errCode)
	}
	if int32(binary.LittleEndian.Uint32(data)) != 42 {
		t.Errorf("value = %d, want 42", int32(binary.LittleEndian.Uint32(data)))
	}

	// Process-image read at the explicit offset should also work.
	data2, errCode2 := st.ReadData(IdxGrpProcessImageRW, sym.IndexOffset, 4)
	if errCode2 != ErrNoError {
		t.Fatalf("process-image read error: 0x%X", errCode2)
	}
	if int32(binary.LittleEndian.Uint32(data2)) != 42 {
		t.Errorf("process-image value = %d, want 42", int32(binary.LittleEndian.Uint32(data2)))
	}
}

// TestRegisterPadAt verifies that RegisterPadAt:
//   - does NOT add the pad to byName
//   - does NOT add the pad to symbolOrder (not in symbol list/count)
//   - adds to byOffset so process-image range reads cover that range and return zeros
//   - updates nextOffset
func TestRegisterPadAt(t *testing.T) {
	st := NewSymbolTable()

	// Register a real pin at offset 0 and a pad at offset 1.
	st.RegisterAt("stX.bFlag", 0, newBoolPin(true))
	st.RegisterPadAt(1, 2) // 2-byte pad at offset 1

	// Pad must NOT appear in byName.
	if sym := st.GetByName("stX._pad0"); sym != nil {
		t.Error("pad symbol unexpectedly found in byName")
	}

	// Symbol count must NOT include the pad.
	data, errCode := st.ReadData(IdxGrpSymbolCount, 0, 4)
	if errCode != ErrNoError {
		t.Fatalf("SymbolCount error: 0x%X", errCode)
	}
	count := binary.LittleEndian.Uint32(data)
	if count != 1 {
		t.Errorf("SymbolCount = %d, want 1 (pad must not be counted)", count)
	}

	// CreateHandle for pad name must fail.
	_, hErrCode := st.CreateHandle("stX._pad")
	if hErrCode == ErrNoError {
		t.Error("CreateHandle for pad name should fail, got ErrNoError")
	}

	// Process-image range read covering [0..2] (bFlag + pad):
	// byte 0 = 1 (bFlag=true), bytes 1-2 = 0 (pad).
	imgData, imgErr := st.ReadData(IdxGrpProcessImageRW, 0, 3)
	if imgErr != ErrNoError {
		t.Fatalf("process-image read error: 0x%X", imgErr)
	}
	if len(imgData) != 3 {
		t.Fatalf("process-image data length = %d, want 3", len(imgData))
	}
	if imgData[0] != 1 {
		t.Errorf("imgData[0] (bFlag) = %d, want 1", imgData[0])
	}
	if imgData[1] != 0 || imgData[2] != 0 {
		t.Errorf("imgData[1:3] (pad) = %v, want [0 0]", imgData[1:3])
	}
}

// TestSetGroupSizeTailPadding verifies that SetGroupSize overrides the computed
// span so that groupAccessor.Size() returns the full aligned struct size
// including tail padding. This is required so that TwinCAT clients reading a
// struct-level symbol get a correctly-sized buffer.
func TestSetGroupSizeTailPadding(t *testing.T) {
	st := NewSymbolTable()

	// Struct: DINT at 0 (size 4), BOOL at 4 (size 1).
	// Without tail padding: Size() = 5. With tail padding to 4-byte boundary: 8.
	st.RegisterAt("stFoo.nVal", 0, newDintPin(0x01020304))
	st.RegisterAt("stFoo.bFlag", 4, newBoolPin(true))

	grp := st.GetByName("stFoo")
	if grp == nil {
		t.Fatal("group symbol stFoo not found")
	}

	// Before SetGroupSize: Size() == 5 (computed span, no tail padding).
	if got := grp.Accessor.Size(); got != 5 {
		t.Errorf("initial Size() = %d, want 5", got)
	}

	// Set the aligned size (8 bytes, including 3 bytes of tail padding).
	st.SetGroupSize("stFoo", 8)

	if got := grp.Accessor.Size(); got != 8 {
		t.Errorf("after SetGroupSize(8), Size() = %d, want 8", got)
	}

	// ReadBytes should return 8 bytes; tail padding bytes must be zero.
	handle, errCode := st.CreateHandle("stFoo")
	if errCode != ErrNoError {
		t.Fatalf("CreateHandle error: 0x%X", errCode)
	}
	data, errCode := st.ReadData(IdxGrpSymbolValueByHandle, handle, 8)
	if errCode != ErrNoError {
		t.Fatalf("ReadData error: 0x%X", errCode)
	}
	if len(data) != 8 {
		t.Fatalf("ReadData length = %d, want 8", len(data))
	}
	if int32(binary.LittleEndian.Uint32(data[0:4])) != 0x01020304 {
		t.Errorf("nVal = 0x%X, want 0x01020304", binary.LittleEndian.Uint32(data[0:4]))
	}
	if data[4] != 1 {
		t.Errorf("bFlag = %d, want 1", data[4])
	}
	// Tail padding bytes 5-7 should be zero.
	for i := 5; i < 8; i++ {
		if data[i] != 0 {
			t.Errorf("tail padding byte[%d] = %d, want 0", i, data[i])
		}
	}
}

// ---------------------------------------------------------------------------
// Tests for ADSIGRP_SYM_INFOBYNAMEEX (0xF009) ReadWrite support
// ---------------------------------------------------------------------------

// TestBuildSymbolInfoExScalar verifies that buildSymbolInfoEx returns the
// correct extended AdsSymbolEntry for a scalar symbol (arrayDim=0, zero GUID).
func TestBuildSymbolInfoExScalar(t *testing.T) {
	st := NewSymbolTable()
	sym := st.Register("stFoo.nVal", newDintPin(0))

	buf := buildSymbolInfoEx(sym)
	if len(buf) < 30+2+16 {
		t.Fatalf("buildSymbolInfoEx too short: got %d bytes, want ≥ %d", len(buf), 30+2+16)
	}

	// entryLength (first uint32) must equal the total buffer length.
	entryLen := binary.LittleEndian.Uint32(buf[0:4])
	if entryLen != uint32(len(buf)) {
		t.Errorf("entryLength = %d, want %d (= buffer length)", entryLen, len(buf))
	}

	// Locate the extension: after the standard AdsSymbolEntry header.
	// Standard header is 30 bytes + null-terminated name + null-terminated typeName
	// + null-terminated comment (1 null byte). We can find it by using entryLength.
	base := buildSymbolInfo(sym)
	extOff := len(base)

	// arrayDim must be 0 for scalar.
	arrayDim := binary.LittleEndian.Uint16(buf[extOff : extOff+2])
	if arrayDim != 0 {
		t.Errorf("arrayDim = %d, want 0 (scalar)", arrayDim)
	}

	// dataTypeGUID must be all zeros (mockPin.TypeGUID returns zeros).
	for i := 0; i < 16; i++ {
		if buf[extOff+2+i] != 0 {
			t.Errorf("GUID[%d] = %d, want 0", i, buf[extOff+2+i])
		}
	}
}

// TestBuildSymbolInfoExWithGUID verifies that buildSymbolInfoEx returns the
// GUID from the accessor's TypeGUID() method.
func TestBuildSymbolInfoExWithGUID(t *testing.T) {
	// Use a mock pin with a non-zero GUID.
	guid := [16]byte{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}
	pin := &mockPinWithGUID{mockPin: *newDintPin(42), guid: guid}

	st := NewSymbolTable()
	sym := st.Register("stFoo.eType", pin)

	buf := buildSymbolInfoEx(sym)
	base := buildSymbolInfo(sym)
	extOff := len(base)

	// arrayDim = 0 (scalar), GUID should match.
	arrayDim := binary.LittleEndian.Uint16(buf[extOff : extOff+2])
	if arrayDim != 0 {
		t.Errorf("arrayDim = %d, want 0", arrayDim)
	}
	for i := 0; i < 16; i++ {
		if buf[extOff+2+i] != guid[i] {
			t.Errorf("GUID[%d] = %d, want %d", i, buf[extOff+2+i], guid[i])
		}
	}
}

// TestBuildSymbolInfoExArray verifies buildSymbolInfoEx for an array container
// symbol (arrayDim=1 with bounds).
func TestBuildSymbolInfoExArray(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.aPools[1].nVal", newDintPin(0))
	st.Register("stFoo.aPools[2].nVal", newDintPin(0))

	// Mark aPools as a 1D array container.
	st.SetGroupArrayInfo("stFoo.aPools", 1, 1, 2)

	sym := st.GetByName("stFoo.aPools")
	if sym == nil {
		t.Fatal("group symbol stFoo.aPools not found")
	}

	buf := buildSymbolInfoEx(sym)
	base := buildSymbolInfo(sym)
	extOff := len(base)

	// entryLength must equal total buffer length.
	entryLen := binary.LittleEndian.Uint32(buf[0:4])
	if entryLen != uint32(len(buf)) {
		t.Errorf("entryLength = %d, want %d", entryLen, len(buf))
	}

	// arrayDim = 1.
	arrayDim := binary.LittleEndian.Uint16(buf[extOff : extOff+2])
	if arrayDim != 1 {
		t.Errorf("arrayDim = %d, want 1", arrayDim)
	}

	// GUID = zeros (groupAccessor.TypeGUID() returns zeros).
	for i := 0; i < 16; i++ {
		if buf[extOff+2+i] != 0 {
			t.Errorf("GUID[%d] = %d, want 0", i, buf[extOff+2+i])
		}
	}

	// Per-dimension bounds: lBound=1, elements=2.
	dimOff := extOff + 2 + 16
	lBound := binary.LittleEndian.Uint32(buf[dimOff : dimOff+4])
	elems := binary.LittleEndian.Uint32(buf[dimOff+4 : dimOff+8])
	if lBound != 1 {
		t.Errorf("lBound = %d, want 1", lBound)
	}
	if elems != 2 {
		t.Errorf("elements = %d, want 2", elems)
	}
}

// TestReadWriteDataSymbolInfoByNameEx verifies that ReadWriteData with
// IdxGrpSymbolInfoByNameEx (0xF009) returns a valid extended symbol info
// response for a known symbol.
func TestReadWriteDataSymbolInfoByNameEx(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.nVal", newDintPin(0))

	// Request extended info via ReadWrite with IG=0xF009.
	data, errCode := st.ReadWriteData(IdxGrpSymbolInfoByNameEx, 0, 64, []byte("stFoo.nVal"))
	if errCode != ErrNoError {
		t.Fatalf("ReadWriteData(IdxGrpSymbolInfoByNameEx) error: 0x%X", errCode)
	}
	if len(data) < 30+2+16 {
		t.Fatalf("response too short: got %d bytes, want ≥ %d", len(data), 30+2+16)
	}

	// entryLength must be consistent.
	entryLen := binary.LittleEndian.Uint32(data[0:4])
	if entryLen != uint32(len(data)) {
		t.Errorf("entryLength = %d, want %d", entryLen, len(data))
	}
}

// TestReadWriteDataSymbolInfoByNameExNotFound verifies that ReadWriteData with
// IdxGrpSymbolInfoByNameEx returns ErrNoSymbol for an unknown symbol name.
func TestReadWriteDataSymbolInfoByNameExNotFound(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.nVal", newDintPin(0))

	_, errCode := st.ReadWriteData(IdxGrpSymbolInfoByNameEx, 0, 64, []byte("stFoo.notExist"))
	if errCode == ErrNoError {
		t.Error("expected ErrNoSymbol for unknown symbol, got ErrNoError")
	}
}

// TestReadWriteDataSymbolInfoByNameExNullTerminated verifies that null-terminated
// symbol names are correctly handled in the 0xF009 ReadWrite handler.
func TestReadWriteDataSymbolInfoByNameExNullTerminated(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.bFlag", newBoolPin(true))

	// TwinCAT sends null-terminated symbol names.
	data, errCode := st.ReadWriteData(IdxGrpSymbolInfoByNameEx, 0, 64, []byte("stFoo.bFlag\x00"))
	if errCode != ErrNoError {
		t.Fatalf("ReadWriteData(SymbolInfoByNameEx) with null-terminated name: 0x%X", errCode)
	}
	if len(data) < 30+2+16 {
		t.Fatalf("response too short: %d bytes", len(data))
	}
}

// TestReadWriteDataSymbolInfoByNameExTruncated verifies that ReadWriteData with
// IdxGrpSymbolInfoByNameEx (0xF009) truncates the response to readLen and updates
// the entryLength field accordingly. This matches the TwinCAT HMI behaviour of
// requesting ReadLen=30 (only the fixed header).
func TestReadWriteDataSymbolInfoByNameExTruncated(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.nVal", newDintPin(0))

	// Simulate a TwinCAT HMI that requests only the 30-byte fixed header.
	const wantLen = 30
	data, errCode := st.ReadWriteData(IdxGrpSymbolInfoByNameEx, 0, wantLen, []byte("stFoo.nVal"))
	if errCode != ErrNoError {
		t.Fatalf("ReadWriteData(IdxGrpSymbolInfoByNameEx, readLen=30) error: 0x%X", errCode)
	}
	if len(data) != wantLen {
		t.Fatalf("response length = %d, want %d", len(data), wantLen)
	}
	// entryLength (first 4 bytes) must equal the truncated length.
	entryLen := binary.LittleEndian.Uint32(data[0:4])
	if entryLen != wantLen {
		t.Errorf("entryLength = %d, want %d", entryLen, wantLen)
	}
}

// TestReadWriteDataSymbolInfoByNameTruncated verifies that ReadWriteData with
// IdxGrpSymbolInfoByName (0xF007) also truncates the response to readLen for
// non-12-byte read lengths.
func TestReadWriteDataSymbolInfoByNameTruncated(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.nVal", newDintPin(0))

	// Request with a readLen smaller than the full response but not the special 12.
	const wantLen = 20
	data, errCode := st.ReadWriteData(IdxGrpSymbolInfoByName, 0, wantLen, []byte("stFoo.nVal"))
	if errCode != ErrNoError {
		t.Fatalf("ReadWriteData(IdxGrpSymbolInfoByName, readLen=20) error: 0x%X", errCode)
	}
	if len(data) != wantLen {
		t.Fatalf("response length = %d, want %d", len(data), wantLen)
	}
	// entryLength (first 4 bytes) must equal the truncated length.
	entryLen := binary.LittleEndian.Uint32(data[0:4])
	if entryLen != wantLen {
		t.Errorf("entryLength = %d, want %d", entryLen, wantLen)
	}
}

// TestSetGroupArrayInfo verifies that SetGroupArrayInfo sets the array fields
// on the named group symbol.
func TestSetGroupArrayInfo(t *testing.T) {
	st := NewSymbolTable()
	st.Register("stFoo.aPools[1].nVal", newDintPin(0))

	st.SetGroupArrayInfo("stFoo.aPools", 1, 1, 4)

	sym := st.GetByName("stFoo.aPools")
	if sym == nil {
		t.Fatal("group symbol stFoo.aPools not found")
	}
	if sym.ArrayDim != 1 {
		t.Errorf("ArrayDim = %d, want 1", sym.ArrayDim)
	}
	if sym.ArrayLBound != 1 {
		t.Errorf("ArrayLBound = %d, want 1", sym.ArrayLBound)
	}
	if sym.ArrayElems != 4 {
		t.Errorf("ArrayElems = %d, want 4", sym.ArrayElems)
	}

	// SetGroupArrayInfo on a non-existent name must not panic.
	st.SetGroupArrayInfo("doesNotExist", 1, 0, 3)
}

// mockPinWithGUID extends mockPin with a configurable TypeGUID.
type mockPinWithGUID struct {
	mockPin
	guid [16]byte
}

func (m *mockPinWithGUID) TypeGUID() [16]byte { return m.guid }
