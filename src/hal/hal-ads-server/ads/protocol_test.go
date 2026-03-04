package ads

import (
	"encoding/binary"
	"net"
	"testing"
	"time"
)

func TestParseAMSNetID(t *testing.T) {
	tests := []struct {
		input   string
		wantErr bool
		want    AMSNetID
	}{
		{"5.80.201.232.1.1", false, AMSNetID{5, 80, 201, 232, 1, 1}},
		{"0.0.0.0.0.0", false, AMSNetID{}},
		{"invalid", true, AMSNetID{}},
	}
	for _, tc := range tests {
		got, err := ParseAMSNetID(tc.input)
		if tc.wantErr {
			if err == nil {
				t.Errorf("ParseAMSNetID(%q) want error, got nil", tc.input)
			}
			continue
		}
		if err != nil {
			t.Errorf("ParseAMSNetID(%q) unexpected error: %v", tc.input, err)
			continue
		}
		if got != tc.want {
			t.Errorf("ParseAMSNetID(%q) = %v, want %v", tc.input, got, tc.want)
		}
	}
}

func TestAMSNetIDString(t *testing.T) {
	id := AMSNetID{5, 80, 201, 232, 1, 1}
	want := "5.80.201.232.1.1"
	if got := id.String(); got != want {
		t.Errorf("AMSNetID.String() = %q, want %q", got, want)
	}
}

func TestEncodeDecodeAMSHeader(t *testing.T) {
	orig := AMSHeader{
		TargetNetID: AMSNetID{1, 2, 3, 4, 5, 6},
		TargetPort:  801,
		SourceNetID: AMSNetID{5, 80, 201, 232, 1, 1},
		SourcePort:  32905,
		CommandID:   CmdRead,
		StateFlags:  StateFlagRequest,
		DataLength:  12,
		ErrorCode:   0,
		InvokeID:    42,
	}

	buf := make([]byte, AMSHeaderSize)
	encodeAMSHeader(buf, &orig)
	got := decodeAMSHeader(buf)

	if got != orig {
		t.Errorf("encode/decode round-trip mismatch:\ngot  %+v\nwant %+v", got, orig)
	}
}

func TestSendAMSResponsePacketLayout(t *testing.T) {
	// Use a simple byte buffer as the "connection".
	var buf bytesConn
	serverNetID := AMSNetID{5, 80, 201, 232, 1, 1}
	serverPort := uint16(851)
	s := &Server{netID: serverNetID, port: serverPort}
	req := &AMSHeader{
		TargetNetID: serverNetID,
		TargetPort:  serverPort,
		SourceNetID: AMSNetID{10, 0, 0, 1, 1, 1},
		SourcePort:  32905,
		CommandID:   CmdRead,
		StateFlags:  StateFlagRequest,
		InvokeID:    99,
	}
	payload := []byte{0x01, 0x02, 0x03, 0x04}
	if err := s.sendAMSResponse(&buf, req, CmdRead, ErrNoError, payload); err != nil {
		t.Fatalf("sendAMSResponse error: %v", err)
	}
	pkt := buf.data

	// AMS/TCP prefix: 2 reserved + 4-byte length.
	if pkt[0] != 0 || pkt[1] != 0 {
		t.Errorf("reserved bytes non-zero: %02x %02x", pkt[0], pkt[1])
	}
	amsLen := binary.LittleEndian.Uint32(pkt[2:])
	expectedAMSLen := uint32(AMSHeaderSize) + uint32(len(payload))
	if amsLen != expectedAMSLen {
		t.Errorf("AMS/TCP length = %d, want %d", amsLen, expectedAMSLen)
	}

	// Decode the AMS header.
	amsHdr := decodeAMSHeader(pkt[AMSTCPHeaderSize : AMSTCPHeaderSize+AMSHeaderSize])
	if amsHdr.TargetNetID != req.SourceNetID {
		t.Errorf("response target = %v, want source %v", amsHdr.TargetNetID, req.SourceNetID)
	}
	if amsHdr.SourceNetID != serverNetID {
		t.Errorf("response source = %v, want server netID %v", amsHdr.SourceNetID, serverNetID)
	}
	if amsHdr.SourcePort != req.TargetPort {
		t.Errorf("response source port = %d, want req.TargetPort %d", amsHdr.SourcePort, req.TargetPort)
	}
	if amsHdr.StateFlags != StateFlagResponse {
		t.Errorf("state flags = %04x, want %04x", amsHdr.StateFlags, StateFlagResponse)
	}
	if amsHdr.InvokeID != req.InvokeID {
		t.Errorf("invoke ID = %d, want %d", amsHdr.InvokeID, req.InvokeID)
	}

	// Verify payload is present.
	data := pkt[AMSTCPHeaderSize+AMSHeaderSize:]
	for i, b := range payload {
		if data[i] != b {
			t.Errorf("payload[%d] = %02x, want %02x", i, data[i], b)
		}
	}
}

// bytesConn is a simple net.Conn stub that accumulates written bytes.
type bytesConn struct {
	data []byte
}

func (c *bytesConn) Write(b []byte) (int, error) {
	c.data = append(c.data, b...)
	return len(b), nil
}

// Implement the minimal net.Conn interface (unused methods panic).
func (c *bytesConn) Read(b []byte) (int, error)             { panic("not implemented") }
func (c *bytesConn) Close() error                            { return nil }
func (c *bytesConn) LocalAddr() net.Addr                     { return nil }
func (c *bytesConn) RemoteAddr() net.Addr                    { return nil }
func (c *bytesConn) SetDeadline(t time.Time) error           { return nil }
func (c *bytesConn) SetReadDeadline(t time.Time) error       { return nil }
func (c *bytesConn) SetWriteDeadline(t time.Time) error      { return nil }

// TestResponsePortEcho verifies that the response SourcePort always echoes
// the request's TargetPort, not the server's hardcoded port (851).
func TestResponsePortEcho(t *testing.T) {
	serverNetID := AMSNetID{5, 80, 201, 232, 1, 1}
	s := &Server{netID: serverNetID, port: 851}

	for _, targetPort := range []uint16{801, 851, 999} {
		var buf bytesConn
		req := &AMSHeader{
			TargetNetID: serverNetID,
			TargetPort:  targetPort,
			SourceNetID: AMSNetID{10, 0, 0, 1, 1, 1},
			SourcePort:  65534,
			CommandID:   CmdRead,
			StateFlags:  StateFlagRequest,
			InvokeID:    1,
		}
		if err := s.sendAMSResponse(&buf, req, CmdRead, ErrNoError, nil); err != nil {
			t.Fatalf("targetPort=%d: sendAMSResponse error: %v", targetPort, err)
		}
		hdr := decodeAMSHeader(buf.data[AMSTCPHeaderSize : AMSTCPHeaderSize+AMSHeaderSize])
		if hdr.SourcePort != targetPort {
			t.Errorf("targetPort=%d: response SourcePort = %d, want %d", targetPort, hdr.SourcePort, targetPort)
		}
	}
}
