package ads

import (
	"encoding/binary"
	"io"
	"net"
	"testing"
	"time"
)

// TestNotifyManagerAddDel verifies basic subscription lifecycle.
func TestNotifyManagerAddDel(t *testing.T) {
	st := NewSymbolTable()
	st.Register("test.var1", &mockPin{data: []byte{42}, size: 1})

	c1, c2 := net.Pipe()
	defer c1.Close()
	defer c2.Close()

	s := &Server{
		netID:   AMSNetID{1, 2, 3, 4, 1, 1},
		port:    851,
		symbols: st,
	}

	nm := newNotifyManager(s, c1)

	h1 := nm.add(IdxGrpProcessImageRW, 0, 1, ADSTransModeCyclic, 100*time.Millisecond)
	h2 := nm.add(IdxGrpProcessImageRW, 0, 1, ADSTransModeOnChange, 200*time.Millisecond)

	if h1 == h2 {
		t.Fatalf("expected different handles, got %d and %d", h1, h2)
	}

	if !nm.del(h1) {
		t.Fatal("expected del(h1) to succeed")
	}
	if nm.del(h1) {
		t.Fatal("expected del(h1) to fail on second call")
	}
	if !nm.del(h2) {
		t.Fatal("expected del(h2) to succeed")
	}
}

// TestNotifyManagerCyclicSend verifies that cyclic notifications are sent.
func TestNotifyManagerCyclicSend(t *testing.T) {
	st := NewSymbolTable()
	pin := &mockPin{data: []byte{0x55}, size: 1}
	st.Register("test.var1", pin)

	c1, c2 := net.Pipe()
	defer c1.Close()
	defer c2.Close()

	s := &Server{
		netID:   AMSNetID{1, 2, 3, 4, 1, 1},
		port:    851,
		symbols: st,
	}

	nm := newNotifyManager(s, c1)
	nm.cNetID = AMSNetID{5, 6, 7, 8, 1, 1}
	nm.cPort = 30000
	nm.start()
	defer nm.stop()

	// Subscribe with short cycle time.
	nm.add(IdxGrpSymbolValueByHandle, 1, 1, ADSTransModeCyclic, 10*time.Millisecond)

	// The symbol handle 1 was auto-assigned; we need to use the correct read path.
	// Let's use process image instead since our symbol is at offset 0.
	nm.mu.Lock()
	for _, sub := range nm.subs {
		sub.indexGroup = IdxGrpProcessImageRW
		sub.indexOffset = 0
	}
	nm.mu.Unlock()

	// Read a DeviceNotification packet from c2.
	pkt := readAMSPacket(t, c2, 2*time.Second)
	if pkt == nil {
		t.Fatal("expected notification packet, got nil")
	}

	// Parse AMS header.
	hdr := decodeAMSHeader(pkt[:AMSHeaderSize])
	if hdr.CommandID != CmdDeviceNotification {
		t.Fatalf("expected CmdDeviceNotification (0x0008), got 0x%04X", hdr.CommandID)
	}

	payload := pkt[AMSHeaderSize:]
	// Minimum: length(4) + stamps(4) + timestamp(8) + samples(4) + handle(4) + size(4) + data(1)
	if len(payload) < 29 {
		t.Fatalf("payload too short: %d bytes", len(payload))
	}

	stamps := binary.LittleEndian.Uint32(payload[4:8])
	if stamps != 1 {
		t.Fatalf("expected 1 stamp, got %d", stamps)
	}

	sampleCount := binary.LittleEndian.Uint32(payload[16:20])
	if sampleCount != 1 {
		t.Fatalf("expected 1 sample, got %d", sampleCount)
	}

	sampleSize := binary.LittleEndian.Uint32(payload[24:28])
	if sampleSize != 1 {
		t.Fatalf("expected sample size 1, got %d", sampleSize)
	}

	if payload[28] != 0x55 {
		t.Fatalf("expected data 0x55, got 0x%02X", payload[28])
	}
}

// TestNotifyManagerOnChange verifies that on-change notifications are only
// sent when data changes.
func TestNotifyManagerOnChange(t *testing.T) {
	st := NewSymbolTable()
	pin := &mockPin{data: []byte{0xAA}, size: 1}
	st.Register("test.var1", pin)

	c1, c2 := net.Pipe()
	defer c1.Close()
	defer c2.Close()

	s := &Server{
		netID:   AMSNetID{1, 2, 3, 4, 1, 1},
		port:    851,
		symbols: st,
	}

	nm := newNotifyManager(s, c1)
	nm.cNetID = AMSNetID{5, 6, 7, 8, 1, 1}
	nm.cPort = 30000
	nm.start()
	defer nm.stop()

	nm.add(IdxGrpProcessImageRW, 0, 1, ADSTransModeOnChange, 10*time.Millisecond)

	// First notification should arrive (initial value).
	pkt1 := readAMSPacket(t, c2, 2*time.Second)
	if pkt1 == nil {
		t.Fatal("expected first notification, got nil")
	}

	// Wait a bit — no second notification should come since data hasn't changed.
	pkt2 := readAMSPacket(t, c2, 100*time.Millisecond)
	if pkt2 != nil {
		t.Fatal("expected no second notification (data unchanged), but got one")
	}

	// Change the data.
	pin.data = []byte{0xBB}

	// Now a notification should come.
	pkt3 := readAMSPacket(t, c2, 2*time.Second)
	if pkt3 == nil {
		t.Fatal("expected notification after data change, got nil")
	}

	payload := pkt3[AMSHeaderSize:]
	if len(payload) >= 29 && payload[28] != 0xBB {
		t.Fatalf("expected data 0xBB, got 0x%02X", payload[28])
	}
}

// readAMSPacket reads one AMS/TCP packet from conn with a timeout.
// Returns nil if timeout elapses.
func readAMSPacket(t *testing.T, conn net.Conn, timeout time.Duration) []byte {
	t.Helper()
	conn.SetReadDeadline(time.Now().Add(timeout))
	defer conn.SetReadDeadline(time.Time{})

	tcpHdr := make([]byte, AMSTCPHeaderSize)
	_, err := io.ReadFull(conn, tcpHdr)
	if err != nil {
		if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
			return nil
		}
		return nil
	}

	amsLen := binary.LittleEndian.Uint32(tcpHdr[2:])
	buf := make([]byte, amsLen)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		t.Fatalf("failed to read AMS data: %v", err)
	}
	return buf
}
