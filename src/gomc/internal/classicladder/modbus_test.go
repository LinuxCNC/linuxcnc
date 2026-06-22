// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package classicladder

import (
	"encoding/binary"
	"fmt"
	"net"
	"testing"
	"time"
)

func TestCRC16(t *testing.T) {
	// Known Modbus CRC values
	tests := []struct {
		data []byte
		want uint16
	}{
		// Slave 1, FC 3, start 0, count 2
		{[]byte{0x01, 0x03, 0x00, 0x00, 0x00, 0x02}, 0x0BC4},
		// Slave 1, FC 1, start 0, count 10
		{[]byte{0x01, 0x01, 0x00, 0x00, 0x00, 0x0A}, 0x0DBC},
		// Empty
		{[]byte{}, 0xFFFF},
	}
	for _, tt := range tests {
		got := crc16(tt.data)
		if got != tt.want {
			t.Errorf("crc16(%X) = 0x%04X, want 0x%04X", tt.data, got, tt.want)
		}
	}
}

func TestParseComParams(t *testing.T) {
	content := `MODBUS_MASTER_SERIAL_PORT=/dev/ttyS0
MODBUS_MASTER_SERIAL_SPEED=9600
MODBUS_MASTER_SERIAL_DATABITS=8
MODBUS_MASTER_SERIAL_STOPBITS=1
MODBUS_MASTER_SERIAL_PARITY=0
MODBUS_ELEMENT_OFFSET=1
MODBUS_MASTER_SERIAL_USE_RTS_TO_SEND=0
MODBUS_MASTER_TIME_INTER_FRAME=100
MODBUS_MASTER_TIME_OUT_RECEIPT=500
MODBUS_MASTER_TIME_AFTER_TRANSMIT=10
MODBUS_DEBUG_LEVEL=0
MODBUS_MAP_COIL_READ=1
MODBUS_MAP_COIL_WRITE=0
MODBUS_MAP_INPUT=0
MODBUS_MAP_HOLDING=0
MODBUS_MAP_REGISTER_READ=1
MODBUS_MAP_REGISTER_WRITE=0
`
	m := newTestModule(t)
	m.parseComParams(content)

	cfg := m.modbus.cfg
	if cfg.SerialPort != "/dev/ttyS0" {
		t.Errorf("SerialPort = %q, want /dev/ttyS0", cfg.SerialPort)
	}
	if cfg.SerialSpeed != 9600 {
		t.Errorf("SerialSpeed = %d, want 9600", cfg.SerialSpeed)
	}
	if cfg.SerialDataBits != 8 {
		t.Errorf("SerialDataBits = %d, want 8", cfg.SerialDataBits)
	}
	if cfg.ElementOffset != 1 {
		t.Errorf("ElementOffset = %d, want 1", cfg.ElementOffset)
	}
	if cfg.TimeInterFrame != 100 {
		t.Errorf("TimeInterFrame = %d, want 100", cfg.TimeInterFrame)
	}
	if cfg.TimeOutReceipt != 500 {
		t.Errorf("TimeOutReceipt = %d, want 500", cfg.TimeOutReceipt)
	}
	if cfg.MapCoilRead != 1 {
		t.Errorf("MapCoilRead = %d, want 1", cfg.MapCoilRead)
	}
	if cfg.MapRegisterRead != 1 {
		t.Errorf("MapRegisterRead = %d, want 1", cfg.MapRegisterRead)
	}
}

func TestParseModbusIOConf_Serial(t *testing.T) {
	content := `#VER=1.0
1,0,1,12,0,0
2,0,1,1,0,0
`
	m := newTestModule(t)
	m.parseModbusIOConf(content)

	if len(m.modbus.cfg.Requests) != 2 {
		t.Fatalf("got %d requests, want 2", len(m.modbus.cfg.Requests))
	}

	r := m.modbus.cfg.Requests[0]
	if r.SlaveAddr != "1" {
		t.Errorf("req[0].SlaveAddr = %q, want \"1\"", r.SlaveAddr)
	}
	if r.TypeReq != 0 {
		t.Errorf("req[0].TypeReq = %d, want 0", r.TypeReq)
	}
	if r.FirstModbusElement != 1 {
		t.Errorf("req[0].FirstModbusElement = %d, want 1", r.FirstModbusElement)
	}
	if r.NbrModbusElements != 12 {
		t.Errorf("req[0].NbrModbusElements = %d, want 12", r.NbrModbusElements)
	}

	r2 := m.modbus.cfg.Requests[1]
	if r2.SlaveAddr != "2" {
		t.Errorf("req[1].SlaveAddr = %q, want \"2\"", r2.SlaveAddr)
	}
}

func TestParseModbusIOConf_TCP(t *testing.T) {
	content := `#VER=1.0
192.168.1.10,3,0,10,0,5
`
	m := newTestModule(t)
	m.parseModbusIOConf(content)

	if len(m.modbus.cfg.Requests) != 1 {
		t.Fatalf("got %d requests, want 1", len(m.modbus.cfg.Requests))
	}

	r := m.modbus.cfg.Requests[0]
	if r.SlaveAddr != "192.168.1.10" {
		t.Errorf("SlaveAddr = %q, want \"192.168.1.10\"", r.SlaveAddr)
	}
	if r.TypeReq != 3 {
		t.Errorf("TypeReq = %d, want 3", r.TypeReq)
	}
	if r.FirstModbusElement != 0 {
		t.Errorf("FirstModbusElement = %d, want 0", r.FirstModbusElement)
	}
	if r.NbrModbusElements != 10 {
		t.Errorf("NbrModbusElements = %d, want 10", r.NbrModbusElements)
	}
	if r.OffsetVarMapped != 5 {
		t.Errorf("OffsetVarMapped = %d, want 5", r.OffsetVarMapped)
	}
}

func TestEmitComParams_RoundTrip(t *testing.T) {
	m := newTestModule(t)
	m.modbus.cfg.SerialPort = "/dev/ttyUSB0"
	m.modbus.cfg.SerialSpeed = 19200
	m.modbus.cfg.SerialDataBits = 8
	m.modbus.cfg.SerialStopBits = 1
	m.modbus.cfg.ElementOffset = 1
	m.modbus.cfg.TimeInterFrame = 50
	m.modbus.cfg.TimeOutReceipt = 300
	m.modbus.cfg.MapCoilRead = 2
	m.modbus.cfg.MapRegisterWrite = 1

	emitted := m.emitComParams()
	if emitted == "" {
		t.Fatal("emitComParams returned empty string")
	}

	// Parse back
	m2 := newTestModule(t)
	m2.parseComParams(emitted)

	cfg := m2.modbus.cfg
	if cfg.SerialPort != "/dev/ttyUSB0" {
		t.Errorf("round-trip SerialPort = %q", cfg.SerialPort)
	}
	if cfg.SerialSpeed != 19200 {
		t.Errorf("round-trip SerialSpeed = %d", cfg.SerialSpeed)
	}
	if cfg.ElementOffset != 1 {
		t.Errorf("round-trip ElementOffset = %d", cfg.ElementOffset)
	}
	if cfg.MapCoilRead != 2 {
		t.Errorf("round-trip MapCoilRead = %d", cfg.MapCoilRead)
	}
	if cfg.MapRegisterWrite != 1 {
		t.Errorf("round-trip MapRegisterWrite = %d", cfg.MapRegisterWrite)
	}
}

func TestEmitModbusIOConf_RoundTrip(t *testing.T) {
	m := newTestModule(t)
	m.modbus.cfg.Requests = []modbusRequest{
		{SlaveAddr: "1", TypeReq: 0, FirstModbusElement: 1, NbrModbusElements: 12, OffsetVarMapped: 0},
		{SlaveAddr: "192.168.1.5", TypeReq: 3, FirstModbusElement: 0, NbrModbusElements: 5, LogicInverted: true, OffsetVarMapped: 10},
	}

	emitted := m.emitModbusIOConf()
	if emitted == "" {
		t.Fatal("emitModbusIOConf returned empty string")
	}

	m2 := newTestModule(t)
	m2.parseModbusIOConf(emitted)

	if len(m2.modbus.cfg.Requests) != 2 {
		t.Fatalf("round-trip got %d requests, want 2", len(m2.modbus.cfg.Requests))
	}

	r := m2.modbus.cfg.Requests[0]
	if r.SlaveAddr != "1" || r.TypeReq != 0 || r.NbrModbusElements != 12 {
		t.Errorf("round-trip req[0] mismatch: %+v", r)
	}

	r2 := m2.modbus.cfg.Requests[1]
	if r2.SlaveAddr != "192.168.1.5" || r2.TypeReq != 3 || !r2.LogicInverted || r2.OffsetVarMapped != 10 {
		t.Errorf("round-trip req[1] mismatch: %+v", r2)
	}
}

// --- TCP loopback integration test ---

func TestModbusTCP_ReadWriteCoils(t *testing.T) {
	m := newTestModule(t)

	// Start slave on random port
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	port := ln.Addr().(*net.TCPAddr).Port
	ln.Close()

	m.modbusSlave.start(port)
	defer m.modbusSlave.stop()
	time.Sleep(10 * time.Millisecond)

	// Connect as a Modbus TCP client and write a coil
	conn, err := net.DialTimeout("tcp", fmt.Sprintf("127.0.0.1:%d", port), time.Second)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()

	// Write single coil at address 0 = ON (FC 5)
	req := buildMBAPRequest(1, []byte{5, 0x00, 0x00, 0xFF, 0x00})
	conn.Write(req)

	resp := make([]byte, 256)
	conn.SetReadDeadline(time.Now().Add(time.Second))
	n, err := conn.Read(resp)
	if err != nil {
		t.Fatal(err)
	}
	if n < 7+5 {
		t.Fatalf("short response: %d bytes", n)
	}
	// Verify echo
	if resp[7] != 5 || resp[8] != 0 || resp[9] != 0 || resp[10] != 0xFF || resp[11] != 0x00 {
		t.Errorf("unexpected write coil response: %X", resp[7:n])
	}

	// Read coils (FC 1) — read 8 coils starting at address 0
	req = buildMBAPRequest(2, []byte{1, 0x00, 0x00, 0x00, 0x08})
	conn.Write(req)

	conn.SetReadDeadline(time.Now().Add(time.Second))
	n, err = conn.Read(resp)
	if err != nil {
		t.Fatal(err)
	}
	if n < 7+3 {
		t.Fatalf("short read response: %d bytes", n)
	}
	// PDU: [FC=1][bytecount=1][data]
	if resp[7] != 1 || resp[8] != 1 {
		t.Errorf("unexpected FC/bytecount: %X", resp[7:7+3])
	}
	// Bit 0 should be set (we wrote coil 0)
	if resp[9]&0x01 == 0 {
		t.Errorf("coil 0 not set after write, data=0x%02X", resp[9])
	}
}

func TestModbusTCP_ReadWriteRegisters(t *testing.T) {
	m := newTestModule(t)

	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	port := ln.Addr().(*net.TCPAddr).Port
	ln.Close()

	m.modbusSlave.start(port)
	defer m.modbusSlave.stop()
	time.Sleep(10 * time.Millisecond)

	conn, err := net.DialTimeout("tcp", fmt.Sprintf("127.0.0.1:%d", port), time.Second)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()

	// Write single register at address 0 with value 12345 (FC 6)
	pdu := make([]byte, 5)
	pdu[0] = 6
	binary.BigEndian.PutUint16(pdu[1:], 0)     // address
	binary.BigEndian.PutUint16(pdu[3:], 12345) // value
	req := buildMBAPRequest(1, pdu)
	conn.Write(req)

	resp := make([]byte, 256)
	conn.SetReadDeadline(time.Now().Add(time.Second))
	n, err := conn.Read(resp)
	if err != nil {
		t.Fatal(err)
	}
	if n < 12 {
		t.Fatalf("short response: %d bytes", n)
	}

	// Read holding registers (FC 3) — 1 register at address 0
	pdu = []byte{3, 0x00, 0x00, 0x00, 0x01}
	req = buildMBAPRequest(2, pdu)
	conn.Write(req)

	conn.SetReadDeadline(time.Now().Add(time.Second))
	n, err = conn.Read(resp)
	if err != nil {
		t.Fatal(err)
	}
	if n < 7+4 {
		t.Fatalf("short read response: %d bytes", n)
	}
	// PDU: [FC=3][bytecount=2][MSB][LSB]
	if resp[7] != 3 || resp[8] != 2 {
		t.Errorf("unexpected FC/bytecount: %X", resp[7:10])
	}
	val := binary.BigEndian.Uint16(resp[9:11])
	if val != 12345 {
		t.Errorf("register value = %d, want 12345", val)
	}
}

func TestModbusTCP_WriteMultipleCoils(t *testing.T) {
	m := newTestModule(t)

	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	port := ln.Addr().(*net.TCPAddr).Port
	ln.Close()

	m.modbusSlave.start(port)
	defer m.modbusSlave.stop()
	time.Sleep(10 * time.Millisecond)

	conn, err := net.DialTimeout("tcp", fmt.Sprintf("127.0.0.1:%d", port), time.Second)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()

	// Write 8 coils at address 0 (FC 15), pattern 0xA5
	pdu := []byte{15, 0x00, 0x00, 0x00, 0x08, 0x01, 0xA5}
	req := buildMBAPRequest(1, pdu)
	conn.Write(req)

	resp := make([]byte, 256)
	conn.SetReadDeadline(time.Now().Add(time.Second))
	n, err := conn.Read(resp)
	if err != nil {
		t.Fatal(err)
	}
	if n < 12 {
		t.Fatalf("short response: %d bytes", n)
	}
	if resp[7] != 15 {
		t.Errorf("response FC = %d, want 15", resp[7])
	}

	// Read back (FC 1)
	req = buildMBAPRequest(2, []byte{1, 0x00, 0x00, 0x00, 0x08})
	conn.Write(req)

	conn.SetReadDeadline(time.Now().Add(time.Second))
	n, err = conn.Read(resp)
	if err != nil {
		t.Fatal(err)
	}
	if n < 10 {
		t.Fatalf("short read response: %d bytes", n)
	}
	if resp[9] != 0xA5 {
		t.Errorf("coils read back = 0x%02X, want 0xA5", resp[9])
	}
}

// --- Helpers ---

func buildMBAPRequest(transID uint16, pdu []byte) []byte {
	// MBAP: transID(2) + protocolID(2) + length(2) + unitID(1) + PDU
	buf := make([]byte, 7+len(pdu))
	binary.BigEndian.PutUint16(buf[0:], transID)
	buf[2] = 0 // protocol ID
	buf[3] = 0
	binary.BigEndian.PutUint16(buf[4:], uint16(len(pdu)+1)) // length = PDU + unitID
	buf[6] = 1                                              // unit ID
	copy(buf[7:], pdu)
	return buf
}
