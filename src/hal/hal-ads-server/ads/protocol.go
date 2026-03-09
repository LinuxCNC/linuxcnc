// Package ads implements the Beckhoff ADS/AMS protocol for TwinCAT HMI communication.
//
// The ADS (Automation Device Specification) protocol is encapsulated in AMS
// (Automation Message Specification) over TCP. Each TCP connection carries
// AMS packets with a 6-byte TCP prefix followed by a 32-byte AMS header and
// command-specific payload.
package ads

import (
	"encoding/binary"
	"fmt"
	"net"
)

// AMSNetID represents an AMS Net ID (6 bytes).
type AMSNetID [6]byte

// String returns the dotted-decimal representation of the AMS Net ID.
func (id AMSNetID) String() string {
	return fmt.Sprintf("%d.%d.%d.%d.%d.%d",
		id[0], id[1], id[2], id[3], id[4], id[5])
}

// ParseAMSNetID parses an AMS Net ID from a dotted string like "1.2.3.4.1.1".
func ParseAMSNetID(s string) (AMSNetID, error) {
	var parts [6]int
	n, err := fmt.Sscanf(s, "%d.%d.%d.%d.%d.%d",
		&parts[0], &parts[1], &parts[2], &parts[3], &parts[4], &parts[5])
	if err != nil || n != 6 {
		return AMSNetID{}, fmt.Errorf("invalid AMS Net ID %q: expected 6 dot-separated octets", s)
	}
	// Reject trailing garbage: reconstruct and compare
	reconstructed := fmt.Sprintf("%d.%d.%d.%d.%d.%d",
		parts[0], parts[1], parts[2], parts[3], parts[4], parts[5])
	if reconstructed != s {
		return AMSNetID{}, fmt.Errorf("invalid AMS Net ID %q: trailing characters", s)
	}
	var id AMSNetID
	for i, v := range parts {
		if v < 0 || v > 255 {
			return AMSNetID{}, fmt.Errorf("invalid AMS Net ID %q: octet %d out of range (0-255)", s, i+1)
		}
		id[i] = byte(v)
	}
	return id, nil
}

// ADS Command IDs (CmdID field in AMS header).
const (
	CmdReadDeviceInfo     uint16 = 0x0001
	CmdRead               uint16 = 0x0002
	CmdWrite              uint16 = 0x0003
	CmdReadState          uint16 = 0x0004
	CmdWriteControl       uint16 = 0x0005
	CmdAddNotification    uint16 = 0x0006
	CmdDelNotification    uint16 = 0x0007
	CmdDeviceNotification uint16 = 0x0008
	CmdReadWrite          uint16 = 0x0009
)

// ADS StateFlags bitmask values for the AMS header.
// The StateFlags field is a bitmask where:
//   - Bit 0 (0x0001): Response flag (1 = response, 0 = request)
//   - Bit 2 (0x0004): ADS command flag
//
// Requests use 0x0004 (ADS command, no response flag).
// Responses use 0x0005 (ADS command + response flag).
const (
	StateFlagRequest  uint16 = 0x0004 // ADS command request (bit 2 set)
	StateFlagResponse uint16 = 0x0005 // ADS command response (bit 2 + bit 0 set)
)

// ADS Index Groups for symbol access.
const (
	// IdxGrpSymbolHandleByName requests a symbol handle by name (ReadWrite).
	// Write: symbol name string. Read: 4-byte handle.
	IdxGrpSymbolHandleByName uint32 = 0x0000F003

	// IdxGrpSymbolValueByHandle accesses symbol value by handle (Read/Write).
	// IndexOffset = handle returned by IdxGrpSymbolHandleByName.
	IdxGrpSymbolValueByHandle uint32 = 0x0000F005

	// IdxGrpReleaseHandle releases a previously allocated symbol handle.
	IdxGrpReleaseHandle uint32 = 0x0000F006

	// IdxGrpSymbolInfoByName retrieves symbol metadata by name (ReadWrite).
	IdxGrpSymbolInfoByName uint32 = 0x0000F007

	// IdxGrpSymbolVersion returns the current symbol version number.
	IdxGrpSymbolVersion uint32 = 0x0000F008

	// IdxGrpSymbolCount returns the number of symbols (ADS Read).
	// The same index group value 0x0000F009 is also used by ADS ReadWrite to
	// retrieve extended symbol info by name (ADSIGRP_SYM_INFOBYNAMEEX).
	IdxGrpSymbolCount uint32 = 0x0000F009

	// IdxGrpSymbolInfoByNameEx retrieves extended symbol metadata by name (ReadWrite).
	// Write: symbol name string. Read: extended AdsSymbolEntry with GUID and array info.
	// This is the same value as IdxGrpSymbolCount; context determines which operation
	// applies — Read returns the count, ReadWrite returns the extended symbol info.
	IdxGrpSymbolInfoByNameEx uint32 = 0x0000F009

	// IdxGrpSymbolListInfo returns upload info for the symbol list.
	IdxGrpSymbolListInfo uint32 = 0x0000F00A

	// IdxGrpSymbolListUpload uploads the symbol list.
	IdxGrpSymbolListUpload uint32 = 0x0000F00B

	// IdxGrpProcessImageRW accesses the process image by byte offset (Read/Write).
	IdxGrpProcessImageRW uint32 = 0x00004040

	// IdxGrpSumRead performs a batch read of multiple symbols in one round-trip (ReadWrite).
	// indexOffset = number of sub-requests.
	// WriteData: N × 12 bytes: IndexGroup(4) + IndexOffset(4) + Length(4).
	// Response: N × 4 bytes error codes, then concatenated data for successful reads.
	IdxGrpSumRead uint32 = 0x0000F080
)

// ADS Data Type (ADST) constants for the symbol info typeID field.
const (
	ADSTBool   uint32 = 33 // BOOL (1 byte)
	ADSTInt8   uint32 = 16 // SINT (1 byte signed)
	ADSTUInt8  uint32 = 17 // BYTE, USINT (1 byte unsigned)
	ADSTInt16  uint32 = 2  // INT (2 bytes signed)
	ADSTUInt16 uint32 = 18 // WORD, UINT (2 bytes unsigned)
	ADSTInt32  uint32 = 3  // DINT (4 bytes signed)
	ADSTUInt32 uint32 = 19 // DWORD, UDINT, TIME, TOD, DATE, DT (4 bytes unsigned)
	ADSTReal32 uint32 = 4  // REAL (4 bytes IEEE 754)
	ADSTReal64 uint32 = 5  // LREAL (8 bytes IEEE 754)
	ADSTString uint32 = 30 // STRING(n) (n+1 bytes, null-terminated)
)

// ADS error codes returned in AMS response headers.
const (
	ErrNoError           uint32 = 0x0000
	ErrInternal          uint32 = 0x0001
	ErrUnknownCommandID  uint32 = 0x0008
	ErrNoSymbol          uint32 = 0x0710
	ErrInvalidOffset     uint32 = 0x0018
	ErrClientBufTooSmall uint32 = 0x1868
	ErrClientInvalidHdl  uint32 = 0x1869
	ErrAccessDenied      uint32 = 0x0712
)

// AMSHeader is the 32-byte AMS packet header.
type AMSHeader struct {
	TargetNetID AMSNetID
	TargetPort  uint16
	SourceNetID AMSNetID
	SourcePort  uint16
	CommandID   uint16
	StateFlags  uint16
	DataLength  uint32
	ErrorCode   uint32
	InvokeID    uint32
}

// AMSHeaderSize is the wire size of the AMS header in bytes.
const AMSHeaderSize = 32

// AMSTCPHeaderSize is the wire size of the AMS/TCP prefix in bytes.
const AMSTCPHeaderSize = 6

// encodeAMSHeader encodes h into buf (must be at least AMSHeaderSize bytes).
func encodeAMSHeader(buf []byte, h *AMSHeader) {
	copy(buf[0:6], h.TargetNetID[:])
	binary.LittleEndian.PutUint16(buf[6:], h.TargetPort)
	copy(buf[8:14], h.SourceNetID[:])
	binary.LittleEndian.PutUint16(buf[14:], h.SourcePort)
	binary.LittleEndian.PutUint16(buf[16:], h.CommandID)
	binary.LittleEndian.PutUint16(buf[18:], h.StateFlags)
	binary.LittleEndian.PutUint32(buf[20:], h.DataLength)
	binary.LittleEndian.PutUint32(buf[24:], h.ErrorCode)
	binary.LittleEndian.PutUint32(buf[28:], h.InvokeID)
}

// decodeAMSHeader decodes the AMS header from buf (must be at least AMSHeaderSize bytes).
func decodeAMSHeader(buf []byte) AMSHeader {
	var h AMSHeader
	copy(h.TargetNetID[:], buf[0:6])
	h.TargetPort = binary.LittleEndian.Uint16(buf[6:])
	copy(h.SourceNetID[:], buf[8:14])
	h.SourcePort = binary.LittleEndian.Uint16(buf[14:])
	h.CommandID = binary.LittleEndian.Uint16(buf[16:])
	h.StateFlags = binary.LittleEndian.Uint16(buf[18:])
	h.DataLength = binary.LittleEndian.Uint32(buf[20:])
	h.ErrorCode = binary.LittleEndian.Uint32(buf[24:])
	h.InvokeID = binary.LittleEndian.Uint32(buf[28:])
	return h
}

// sendAMSResponse sends a complete AMS/TCP response packet on conn.
// req is the original request header; source/dest are swapped for the response.
// The response SourcePort echoes req.TargetPort so that clients can match
// responses to requests by port (per the ADS/AMS protocol specification).
// cmdID is the command being responded to.
// errCode is the ADS error code (0 = success).
// data is the command-specific response payload.
func (s *Server) sendAMSResponse(conn net.Conn, req *AMSHeader, cmdID uint16, errCode uint32, data []byte) error {
	resp := AMSHeader{
		TargetNetID: req.SourceNetID,
		TargetPort:  req.SourcePort,
		SourceNetID: s.netID,
		SourcePort:  req.TargetPort,
		CommandID:   cmdID,
		StateFlags:  StateFlagResponse,
		DataLength:  uint32(len(data)),
		ErrorCode:   errCode,
		InvokeID:    req.InvokeID,
	}

	// Build packet: AMS/TCP header (6) + AMS header (32) + data
	totalAMSLen := uint32(AMSHeaderSize) + uint32(len(data))
	pkt := make([]byte, AMSTCPHeaderSize+AMSHeaderSize+len(data))
	// AMS/TCP prefix
	pkt[0] = 0
	pkt[1] = 0
	binary.LittleEndian.PutUint32(pkt[2:], totalAMSLen)
	// AMS header
	encodeAMSHeader(pkt[AMSTCPHeaderSize:], &resp)
	// Payload
	copy(pkt[AMSTCPHeaderSize+AMSHeaderSize:], data)

	_, err := conn.Write(pkt)
	return err
}

// putUint32LE writes a uint32 in little-endian to buf at offset off.
func putUint32LE(buf []byte, off int, v uint32) {
	binary.LittleEndian.PutUint32(buf[off:], v)
}

// putUint16LE writes a uint16 in little-endian to buf at offset off.
func putUint16LE(buf []byte, off int, v uint16) {
	binary.LittleEndian.PutUint16(buf[off:], v)
}
