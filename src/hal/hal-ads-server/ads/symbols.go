package ads

import (
	"encoding/binary"
	"strings"
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

// groupAccessor implements PinAccessor for a struct/container group symbol.
// It holds references to all descendant leaf Symbols sorted by IndexOffset and
// provides read/write access to them as a single contiguous buffer.
type groupAccessor struct {
	children []*Symbol // sorted by IndexOffset (ascending); leaf symbols only
	typeName string    // last path segment, used as the ADS TypeName
}

func (g *groupAccessor) baseOffset() uint32 {
	if len(g.children) == 0 {
		return 0
	}
	return g.children[0].IndexOffset
}

func (g *groupAccessor) Size() uint32 {
	if len(g.children) == 0 {
		return 0
	}
	last := g.children[len(g.children)-1]
	return last.IndexOffset + last.Accessor.Size() - g.baseOffset()
}

func (g *groupAccessor) ReadBytes() ([]byte, error) {
	buf := make([]byte, g.Size())
	base := g.baseOffset()
	for _, child := range g.children {
		data, err := child.Accessor.ReadBytes()
		if err != nil {
			return nil, err
		}
		rel := child.IndexOffset - base
		copy(buf[rel:], data)
	}
	return buf, nil
}

func (g *groupAccessor) WriteBytes(data []byte) error {
	base := g.baseOffset()
	for _, child := range g.children {
		rel := child.IndexOffset - base
		size := child.Accessor.Size()
		if uint32(len(data)) < rel+size {
			continue // partial write: skip children beyond the provided data
		}
		if err := child.Accessor.WriteBytes(data[rel : rel+size]); err != nil {
			return err
		}
	}
	return nil
}

func (g *groupAccessor) TypeName() string { return g.typeName }
func (g *groupAccessor) TypeID() uint32   { return 0 }

// parentPrefixes returns all non-leaf path prefixes for a dotted name.
// E.g. "A.B.C" → ["A", "A.B"]. Single-segment names return nil.
func parentPrefixes(name string) []string {
	segs := strings.Split(name, ".")
	if len(segs) <= 1 {
		return nil
	}
	prefixes := make([]string, len(segs)-1)
	for i := 1; i < len(segs); i++ {
		prefixes[i-1] = strings.Join(segs[:i], ".")
	}
	return prefixes
}

// Register adds a symbol to the table. The symbol's IndexGroup is set to
// IdxGrpProcessImageRW and IndexOffset is assigned automatically.
// For each parent path prefix (e.g. "A.B" for leaf "A.B.C"), a group symbol
// is automatically created or updated so that SymbolInfoByName and
// CreateHandle work for intermediate struct paths.
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

	// Auto-create or update group symbols for every ancestor prefix.
	// The leaf is added to ALL ancestor groups so that each group's accessor
	// covers all descendant leaves.
	for _, prefix := range parentPrefixes(name) {
		if existing, ok := st.byName[prefix]; ok {
			// Update existing group: append the new leaf (offsets are
			// monotonically increasing, so appending keeps sorted order).
			// If the existing entry is not a groupAccessor (e.g. a leaf was
			// registered at this exact path), leave it unchanged.
			if ga, ok := existing.Accessor.(*groupAccessor); ok {
				ga.children = append(ga.children, sym)
			}
		} else {
			// Create a new group symbol for this prefix.
			segs := strings.Split(prefix, ".")
			ga := &groupAccessor{
				children: []*Symbol{sym},
				typeName: segs[len(segs)-1],
			}
			st.byName[prefix] = &Symbol{
				Name:        prefix,
				IndexGroup:  IdxGrpProcessImageRW,
				IndexOffset: sym.IndexOffset,
				Accessor:    ga,
			}
		}
	}
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
// Name lookup uses findSymbolWithFallback for prefix/case-insensitive matching.
func (st *SymbolTable) CreateHandle(name string) (uint32, uint32) {
	st.mu.Lock()
	defer st.mu.Unlock()

	sym := st.findSymbolWithFallback(name)
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
		count := uint32(len(st.symbolOrder))
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
		return st.ReleaseHandle(indexOffset)

	default:
		return ErrNoSymbol
	}
}

// ReadWriteData services an ADS ReadWrite request.
// Returns the response data and an ADS error code.
func (st *SymbolTable) ReadWriteData(indexGroup, indexOffset, readLen uint32, writeData []byte) ([]byte, uint32) {
	switch indexGroup {
	case IdxGrpSymbolHandleByName:
		// writeData contains the symbol name (null-terminated); response is the 4-byte handle.
		name := strings.TrimRight(string(writeData), "\x00")
		handle, errCode := st.CreateHandle(name)
		if errCode != ErrNoError {
			return nil, errCode
		}
		buf := make([]byte, 4)
		binary.LittleEndian.PutUint32(buf, handle)
		return buf, ErrNoError

	case IdxGrpSymbolInfoByName:
		name := strings.TrimRight(string(writeData), "\x00")
		st.mu.RLock()
		sym := st.findSymbolWithFallback(name)
		st.mu.RUnlock()
		if sym == nil {
			return nil, ErrNoSymbol
		}
		// Compact response: client only wants IndexGroup + IndexOffset + Size (12 bytes).
		if readLen == 12 {
			buf := make([]byte, 12)
			binary.LittleEndian.PutUint32(buf[0:4], sym.IndexGroup)
			binary.LittleEndian.PutUint32(buf[4:8], sym.IndexOffset)
			binary.LittleEndian.PutUint32(buf[8:12], sym.Accessor.Size())
			return buf, ErrNoError
		}
		// Full symbol info response.
		return buildSymbolInfo(sym), ErrNoError

	case IdxGrpSumRead:
		// indexOffset = number of read sub-requests.
		// writeData = N × 12 bytes: IndexGroup(4) + IndexOffset(4) + Length(4).
		// Response = N × 4-byte error codes, then concatenated data for successful reads.
		numReads := indexOffset
		type readResult struct {
			errCode uint32
			data    []byte
		}
		results := make([]readResult, numReads)
		for i := uint32(0); i < numReads; i++ {
			off := i * 12
			if off+12 > uint32(len(writeData)) {
				results[i] = readResult{errCode: ErrInternal}
				continue
			}
			ig := binary.LittleEndian.Uint32(writeData[off:])
			io := binary.LittleEndian.Uint32(writeData[off+4:])
			ln := binary.LittleEndian.Uint32(writeData[off+8:])
			data, ec := st.ReadData(ig, io, ln)
			results[i] = readResult{errCode: ec, data: data}
		}
		// Build response: all error codes first, then all data payloads.
		totalLen := numReads * 4
		for _, r := range results {
			if r.errCode == ErrNoError {
				totalLen += uint32(len(r.data))
			}
		}
		resp := make([]byte, totalLen)
		pos := 0
		for _, r := range results {
			binary.LittleEndian.PutUint32(resp[pos:], r.errCode)
			pos += 4
		}
		for _, r := range results {
			if r.errCode == ErrNoError && len(r.data) > 0 {
				copy(resp[pos:], r.data)
				pos += len(r.data)
			}
		}
		return resp, ErrNoError

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
	// Fixed header: 6×uint32 (24 bytes) + 3×uint16 (6 bytes) = 30 bytes,
	// followed by null-terminated name, null-terminated type, and a null comment byte.
	entryLen := uint32(30 + len(nameBytes) + 1 + len(typeBytes) + 1 + 1)
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

// findSymbolWithFallback looks up a symbol by name, trying exact match first,
// then stripping common PLC namespace prefixes (case-insensitive), and finally
// a full case-insensitive scan of the symbol table.
// Must be called with st.mu held (read or write).
func (st *SymbolTable) findSymbolWithFallback(name string) *Symbol {
	// Exact match.
	if sym := st.byName[name]; sym != nil {
		return sym
	}
	// Strip common PLC namespace prefixes (match case-insensitively so "GVL.",
	// "gvl.", etc. all work).
	nameLower := strings.ToLower(name)
	for _, prefix := range []string{"gvl.", "main.", "plc."} {
		if strings.HasPrefix(nameLower, prefix) {
			stripped := name[len(prefix):]
			if sym := st.byName[stripped]; sym != nil {
				return sym
			}
		}
	}
	// Case-insensitive fallback.
	for symName, sym := range st.byName {
		if strings.ToLower(symName) == nameLower {
			return sym
		}
	}
	return nil
}
