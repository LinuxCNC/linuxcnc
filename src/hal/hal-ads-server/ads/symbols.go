package ads

import (
	"encoding/binary"
	"sync"
)

// PinAccessor provides read/write access to a HAL pin value using
// ADS wire format (little-endian bytes).
type PinAccessor interface {
	// ReadBytes reads the pin value and returns it serialized to ADS wire format.
	ReadBytes() ([]byte, error)
	// WriteBytes deserializes ADS wire bytes and writes the value to the pin.
	WriteBytes(data []byte) error
	// Size returns the ADS wire size of this symbol in bytes.
	Size() uint32
	// TypeName returns the ADS/TwinCAT type name (e.g. "BOOL", "DINT", "STRING(32)").
	TypeName() string
	// TypeID returns the ADST (ADS Data Type) constant for this symbol.
	TypeID() uint32
}

// Symbol represents an ADS symbol mapped to a HAL pin.
type Symbol struct {
	// Name is the full ADS symbol name, e.g. "stDISPLAY_DATA.stPOOL[1].bReady".
	Name string
	// IndexGroup is the IndexGroup used for direct process-image access (IdxGrpProcessImageRW).
	IndexGroup uint32
	// IndexOffset is the byte offset within the process image for direct access.
	IndexOffset uint32
	// Accessor bridges ADS read/write operations to HAL pin Get/Set.
	Accessor PinAccessor
}

// SymbolTable manages ADS symbols and their handle assignments.
type SymbolTable struct {
	mu          sync.RWMutex
	byName      map[string]*Symbol
	byOffset    map[uint32]*Symbol // keyed by IndexOffset within IdxGrpProcessImageRW
	handles     map[uint32]*Symbol // handle → symbol
	nextHandle  uint32
	nextOffset  uint32 // next available byte offset in process image
	symbolOrder []*Symbol
}

// NewSymbolTable creates an empty SymbolTable.
func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		byName:     make(map[string]*Symbol),
		byOffset:   make(map[uint32]*Symbol),
		handles:    make(map[uint32]*Symbol),
		nextHandle: 1,
		nextOffset: 0,
	}
}

// Register adds a symbol to the table. The symbol's IndexGroup is set to
// IdxGrpProcessImageRW and IndexOffset is assigned automatically.
func (st *SymbolTable) Register(name string, acc PinAccessor) *Symbol {
	st.mu.Lock()
	defer st.mu.Unlock()

	sym := &Symbol{
		Name:        name,
		IndexGroup:  IdxGrpProcessImageRW,
		IndexOffset: st.nextOffset,
		Accessor:    acc,
	}
	st.nextOffset += acc.Size()
	st.byName[name] = sym
	st.byOffset[sym.IndexOffset] = sym
	st.symbolOrder = append(st.symbolOrder, sym)
	return sym
}

// GetByName returns the symbol with the given name, or nil if not found.
func (st *SymbolTable) GetByName(name string) *Symbol {
	st.mu.RLock()
	defer st.mu.RUnlock()
	return st.byName[name]
}

// GetByHandle returns the symbol associated with a handle, or nil.
func (st *SymbolTable) GetByHandle(handle uint32) *Symbol {
	st.mu.RLock()
	defer st.mu.RUnlock()
	return st.handles[handle]
}

// CreateHandle allocates a new handle for the named symbol.
// Returns the handle and ErrNoSymbol if the name is not found.
func (st *SymbolTable) CreateHandle(name string) (uint32, uint32) {
	st.mu.Lock()
	defer st.mu.Unlock()

	sym := st.byName[name]
	if sym == nil {
		return 0, ErrNoSymbol
	}
	h := st.nextHandle
	st.nextHandle++
	st.handles[h] = sym
	return h, ErrNoError
}

// ReleaseHandle releases a previously allocated handle.
func (st *SymbolTable) ReleaseHandle(handle uint32) uint32 {
	st.mu.Lock()
	defer st.mu.Unlock()

	if _, ok := st.handles[handle]; !ok {
		return ErrClientInvalidHdl
	}
	delete(st.handles, handle)
	return ErrNoError
}

// ReadData services an ADS Read request.
// Returns the response data and an ADS error code.
func (st *SymbolTable) ReadData(indexGroup, indexOffset, length uint32) ([]byte, uint32) {
	switch indexGroup {
	case IdxGrpSymbolValueByHandle:
		sym := st.GetByHandle(indexOffset)
		if sym == nil {
			return nil, ErrClientInvalidHdl
		}
		return readSymbol(sym, length)

	case IdxGrpProcessImageRW:
		sym := st.findByOffset(indexOffset)
		if sym == nil {
			return nil, ErrInvalidOffset
		}
		return readSymbol(sym, length)

	case IdxGrpSymbolVersion:
		buf := make([]byte, 4)
		binary.LittleEndian.PutUint32(buf, 1)
		return buf, ErrNoError

	case IdxGrpSymbolCount:
		st.mu.RLock()
		count := uint32(len(st.byName))
		st.mu.RUnlock()
		buf := make([]byte, 4)
		binary.LittleEndian.PutUint32(buf, count)
		return buf, ErrNoError

	case IdxGrpSymbolListInfo:
		return st.buildSymbolListInfo()

	case IdxGrpSymbolListUpload:
		return st.buildSymbolList()

	default:
		return nil, ErrNoSymbol
	}
}

// WriteData services an ADS Write request.
// Returns an ADS error code.
func (st *SymbolTable) WriteData(indexGroup, indexOffset uint32, data []byte) uint32 {
	switch indexGroup {
	case IdxGrpSymbolValueByHandle:
		sym := st.GetByHandle(indexOffset)
		if sym == nil {
			return ErrClientInvalidHdl
		}
		return writeSymbol(sym, data)

	case IdxGrpProcessImageRW:
		sym := st.findByOffset(indexOffset)
		if sym == nil {
			return ErrInvalidOffset
		}
		return writeSymbol(sym, data)

	case IdxGrpReleaseHandle:
		if len(data) < 4 {
			return ErrInternal
		}
		handle := binary.LittleEndian.Uint32(data[:4])
		return st.ReleaseHandle(handle)

	default:
		return ErrNoSymbol
	}
}

// ReadWriteData services an ADS ReadWrite request.
// Returns the response data and an ADS error code.
func (st *SymbolTable) ReadWriteData(indexGroup, indexOffset, readLen uint32, writeData []byte) ([]byte, uint32) {
	switch indexGroup {
	case IdxGrpSymbolHandleByName:
		// writeData contains the symbol name; response is the 4-byte handle.
		name := string(writeData)
		handle, errCode := st.CreateHandle(name)
		if errCode != ErrNoError {
			return nil, errCode
		}
		buf := make([]byte, 4)
		binary.LittleEndian.PutUint32(buf, handle)
		return buf, ErrNoError

	case IdxGrpSymbolInfoByName:
		name := string(writeData)
		st.mu.RLock()
		sym := st.byName[name]
		st.mu.RUnlock()
		if sym == nil {
			return nil, ErrNoSymbol
		}
		info := buildSymbolInfo(sym)
		return info, ErrNoError

	default:
		return nil, ErrNoSymbol
	}
}

// findByOffset returns the symbol whose IndexOffset matches offset (within
// IdxGrpProcessImageRW), or nil if not found.
func (st *SymbolTable) findByOffset(offset uint32) *Symbol {
	st.mu.RLock()
	defer st.mu.RUnlock()
	return st.byOffset[offset]
}

// readSymbol reads the value of sym and returns it serialized to ADS wire bytes.
func readSymbol(sym *Symbol, length uint32) ([]byte, uint32) {
	data, err := sym.Accessor.ReadBytes()
	if err != nil {
		return nil, ErrInternal
	}
	if length > 0 && uint32(len(data)) > length {
		data = data[:length]
	}
	return data, ErrNoError
}

// writeSymbol writes ADS wire bytes to sym's HAL pin.
func writeSymbol(sym *Symbol, data []byte) uint32 {
	if err := sym.Accessor.WriteBytes(data); err != nil {
		return ErrInternal
	}
	return ErrNoError
}

// buildSymbolInfo encodes the ADS symbol info structure for a single symbol.
// Format matches TwinCAT AdsSymbolEntry:
//
//	uint32 entryLength
//	uint32 indexGroup
//	uint32 indexOffset
//	uint32 size
//	uint32 dataType (ADST)
//	uint32 flags (=0)
//	uint16 nameLength (excl. null terminator)
//	uint16 typeLength (excl. null terminator)
//	uint16 commentLength (=0)
//	[nameLength+1] bytes: name (null-terminated)
//	[typeLength+1] bytes: type name (null-terminated)
//	[1] byte: empty comment (null byte)
func buildSymbolInfo(sym *Symbol) []byte {
	nameBytes := []byte(sym.Name)
	typeBytes := []byte(sym.Accessor.TypeName())
	// Fixed header is 24 bytes + name+1 + type+1 + 1 comment null byte
	entryLen := uint32(24 + len(nameBytes) + 1 + len(typeBytes) + 1 + 1)
	buf := make([]byte, entryLen)
	off := 0
	putUint32LE(buf, off, entryLen)
	off += 4
	putUint32LE(buf, off, sym.IndexGroup)
	off += 4
	putUint32LE(buf, off, sym.IndexOffset)
	off += 4
	putUint32LE(buf, off, sym.Accessor.Size())
	off += 4
	putUint32LE(buf, off, sym.Accessor.TypeID())
	off += 4
	putUint32LE(buf, off, 0) // flags
	off += 4
	putUint16LE(buf, off, uint16(len(nameBytes)))
	off += 2
	putUint16LE(buf, off, uint16(len(typeBytes)))
	off += 2
	putUint16LE(buf, off, 0) // comment length
	off += 2
	copy(buf[off:], nameBytes)
	off += len(nameBytes) + 1 // +1 for null terminator (already zero)
	copy(buf[off:], typeBytes)
	off += len(typeBytes) + 1
	_ = off // comment null byte is already zero
	return buf
}

// buildSymbolListInfo builds the response for IdxGrpSymbolListInfo.
// Returns: uint32 uploadLength, uint32 symbolCount.
func (st *SymbolTable) buildSymbolListInfo() ([]byte, uint32) {
	st.mu.RLock()
	defer st.mu.RUnlock()

	var uploadLen uint32
	for _, sym := range st.symbolOrder {
		info := buildSymbolInfo(sym)
		uploadLen += uint32(len(info))
	}
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint32(buf[0:], uploadLen)
	binary.LittleEndian.PutUint32(buf[4:], uint32(len(st.symbolOrder)))
	return buf, ErrNoError
}

// buildSymbolList builds the full symbol list upload payload.
func (st *SymbolTable) buildSymbolList() ([]byte, uint32) {
	st.mu.RLock()
	defer st.mu.RUnlock()

	var result []byte
	for _, sym := range st.symbolOrder {
		result = append(result, buildSymbolInfo(sym)...)
	}
	return result, ErrNoError
}
