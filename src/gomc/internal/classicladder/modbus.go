// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package classicladder

/*
#include "classicladder_rt.h"
*/
import "C"
import (
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"log/slog"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	"go.bug.st/serial"
)

// Modbus request types (matches original MODBUS_REQ_* constants)
const (
	modbusReqInputsRead     = 0
	modbusReqCoilsWrite     = 1
	modbusReqRegistersRead  = 2
	modbusReqRegistersWrite = 3
	modbusReqCoilsRead      = 4
	modbusReqHoldRead       = 5
	modbusReqDiagnostics    = 6
)

// Modbus function codes
const (
	fcReadCoils     = 1
	fcReadInputs    = 2
	fcReadHoldRegs  = 3
	fcReadInputRegs = 4
	fcForceCoil     = 5
	fcWriteReg      = 6
	fcDiagnostics   = 8
	fcForceCoils    = 15
	fcWriteRegs     = 16
)

// Variable mapping types (matches B_VAR, Q_VAR, etc.)
const (
	mapBVar  = 0 // %B (memory bits)
	mapQVar  = 1 // %Q (physical outputs)
	mapIVar  = 2 // %I (physical inputs)
	mapWVar  = 0 // %W (memory words)
	mapQWVar = 1 // %QW (physical word outputs)
	mapIWVar = 2 // %IW (physical word inputs)
)

// modbusRequest represents one configured Modbus polling request.
type modbusRequest struct {
	SlaveAddr          string // IP:port or slave number for serial
	TypeReq            int
	FirstModbusElement int
	NbrModbusElements  int
	LogicInverted      bool
	OffsetVarMapped    int
}

// modbusConfig holds all Modbus master configuration from com_params.txt.
type modbusConfig struct {
	SerialPort        string
	SerialSpeed       int
	SerialDataBits    int
	SerialStopBits    int
	SerialParity      int // 0=none, 1=odd, 2=even
	SerialUseRTS      bool
	ElementOffset     int
	TimeInterFrame    int // ms
	TimeOutReceipt    int // ms
	TimeAfterTransmit int // ms
	DebugLevel        int
	MapCoilRead       int // B_VAR or Q_VAR
	MapCoilWrite      int // B_VAR, Q_VAR, or I_VAR
	MapInputs         int
	MapHolding        int
	MapRegisterRead   int // W_VAR or QW_VAR
	MapRegisterWrite  int // W_VAR, QW_VAR, or IW_VAR
	Requests          []modbusRequest
}

func defaultModbusConfig() modbusConfig {
	return modbusConfig{
		SerialSpeed:       9600,
		SerialDataBits:    8,
		SerialStopBits:    1,
		SerialParity:      0,
		TimeInterFrame:    100,
		TimeOutReceipt:    500,
		TimeAfterTransmit: 0,
	}
}

// modbusMaster manages the Modbus I/O polling goroutine.
type modbusMaster struct {
	cfg    modbusConfig
	rt     *C.classicladder_rt_t
	logger *slog.Logger

	mu              sync.Mutex
	cancel          context.CancelFunc
	running         bool
	currentReq      int
	errorCount      int
	frameCount      int
	consecutiveErrs int

	// Transport
	serialPort serial.Port
	tcpConns   map[string]net.Conn
}

func newModbusMaster(rt *C.classicladder_rt_t, logger *slog.Logger) *modbusMaster {
	return &modbusMaster{
		cfg:      defaultModbusConfig(),
		rt:       rt,
		logger:   logger,
		tcpConns: make(map[string]net.Conn),
	}
}

func (m *modbusMaster) start() {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.running {
		return
	}
	// Only start if there are configured requests
	hasReqs := false
	for _, r := range m.cfg.Requests {
		if r.SlaveAddr != "" {
			hasReqs = true
			break
		}
	}
	if !hasReqs {
		return
	}

	ctx, cancel := context.WithCancel(context.Background())
	m.cancel = cancel
	m.running = true
	m.currentReq = -1

	go m.loop(ctx)
}

func (m *modbusMaster) stop() {
	m.mu.Lock()
	defer m.mu.Unlock()
	if !m.running {
		return
	}
	m.cancel()
	m.running = false

	// Close transports
	if m.serialPort != nil {
		m.serialPort.Close()
		m.serialPort = nil
	}
	for addr, conn := range m.tcpConns {
		conn.Close()
		delete(m.tcpConns, addr)
	}
}

func (m *modbusMaster) loop(ctx context.Context) {
	// Open serial if configured
	if m.cfg.SerialPort != "" {
		if err := m.openSerial(); err != nil {
			m.logger.Error("modbus serial open failed", "port", m.cfg.SerialPort, "error", err)
			return
		}
		m.logger.Info("modbus master started",
			"mode", "serial",
			"port", m.cfg.SerialPort,
			"speed", m.cfg.SerialSpeed)
	} else {
		m.logger.Info("modbus master started", "mode", "tcp")
	}

	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		// Check ladder state
		state := int(m.rt.state)
		if state != C.CL_STATE_RUN {
			select {
			case <-ctx.Done():
				return
			case <-time.After(time.Duration(m.cfg.TimeInterFrame) * time.Millisecond):
			}
			continue
		}

		req := m.findNextRequest()
		if req == nil {
			select {
			case <-ctx.Done():
				return
			case <-time.After(time.Second):
			}
			continue
		}

		m.executeRequest(req)

		select {
		case <-ctx.Done():
			return
		case <-time.After(time.Duration(m.cfg.TimeInterFrame) * time.Millisecond):
		}
	}
}

func (m *modbusMaster) findNextRequest() *modbusRequest {
	m.mu.Lock()
	defer m.mu.Unlock()

	start := m.currentReq
	for i := 0; i < len(m.cfg.Requests); i++ {
		idx := (start + 1 + i) % len(m.cfg.Requests)
		if m.cfg.Requests[idx].SlaveAddr != "" {
			m.currentReq = idx
			return &m.cfg.Requests[idx]
		}
	}
	return nil
}

func (m *modbusMaster) executeRequest(req *modbusRequest) {
	frame := m.buildRequest(req)
	if frame == nil {
		return
	}

	var resp []byte
	var err error

	if m.cfg.SerialPort != "" {
		resp, err = m.serialTransaction(frame)
	} else {
		resp, err = m.tcpTransaction(req.SlaveAddr, frame)
	}

	m.mu.Lock()
	m.frameCount++
	m.mu.Unlock()

	if err != nil {
		m.mu.Lock()
		m.errorCount++
		m.consecutiveErrs++
		skipToNext := m.consecutiveErrs >= 3
		if skipToNext {
			m.consecutiveErrs = 0
		}
		m.mu.Unlock()
		// Set error bit %E0
		C.write_var_ext(m.rt, C.CL_VAR_ERROR_BIT, 0, 1)
		if m.cfg.DebugLevel >= 1 {
			m.logger.Warn("modbus error", "error", err, "errors", m.errorCount, "frames", m.frameCount)
		}
		if skipToNext {
			// Force advance to next request after 3 consecutive errors
			m.findNextRequest()
		}
		return
	}

	// Success — clear error bit and reset consecutive error counter
	m.mu.Lock()
	m.consecutiveErrs = 0
	m.mu.Unlock()
	C.write_var_ext(m.rt, C.CL_VAR_ERROR_BIT, 0, 0)

	m.processResponse(req, resp)
}

func (m *modbusMaster) buildRequest(req *modbusRequest) []byte {
	firstEle := req.FirstModbusElement - m.cfg.ElementOffset
	if firstEle < 0 {
		firstEle = 0
	}
	nbrEles := req.NbrModbusElements

	var fc byte
	switch req.TypeReq {
	case modbusReqInputsRead:
		fc = fcReadInputs
	case modbusReqCoilsRead:
		fc = fcReadCoils
	case modbusReqCoilsWrite:
		if nbrEles == 1 {
			fc = fcForceCoil
		} else {
			fc = fcForceCoils
		}
	case modbusReqRegistersRead:
		fc = fcReadInputRegs
	case modbusReqRegistersWrite:
		if nbrEles == 1 {
			fc = fcWriteReg
		} else {
			fc = fcWriteRegs
		}
	case modbusReqHoldRead:
		fc = fcReadHoldRegs
	case modbusReqDiagnostics:
		fc = fcDiagnostics
	default:
		return nil
	}

	// Build PDU (without slave addr / MBAP header)
	var pdu []byte
	switch fc {
	case fcReadInputs, fcReadInputRegs, fcReadCoils, fcReadHoldRegs:
		pdu = make([]byte, 5)
		pdu[0] = fc
		binary.BigEndian.PutUint16(pdu[1:], uint16(firstEle))
		binary.BigEndian.PutUint16(pdu[3:], uint16(nbrEles))

	case fcForceCoil:
		pdu = make([]byte, 5)
		pdu[0] = fc
		binary.BigEndian.PutUint16(pdu[1:], uint16(firstEle))
		val := m.getVarForModbus(req, firstEle)
		if val != 0 {
			pdu[3] = 0xFF
			pdu[4] = 0x00
		} else {
			pdu[3] = 0x00
			pdu[4] = 0x00
		}

	case fcWriteReg:
		pdu = make([]byte, 5)
		pdu[0] = fc
		binary.BigEndian.PutUint16(pdu[1:], uint16(firstEle))
		val := m.getVarForModbus(req, firstEle)
		binary.BigEndian.PutUint16(pdu[3:], uint16(int16(val)))

	case fcForceCoils:
		byteCount := (nbrEles + 7) / 8
		pdu = make([]byte, 6+byteCount)
		pdu[0] = fc
		binary.BigEndian.PutUint16(pdu[1:], uint16(firstEle))
		binary.BigEndian.PutUint16(pdu[3:], uint16(nbrEles))
		pdu[5] = byte(byteCount)
		for i := 0; i < nbrEles; i++ {
			val := m.getVarForModbus(req, firstEle+i)
			if val != 0 {
				pdu[6+i/8] |= 1 << (i % 8)
			}
		}

	case fcWriteRegs:
		pdu = make([]byte, 6+nbrEles*2)
		pdu[0] = fc
		binary.BigEndian.PutUint16(pdu[1:], uint16(firstEle))
		binary.BigEndian.PutUint16(pdu[3:], uint16(nbrEles))
		pdu[5] = byte(nbrEles * 2)
		for i := 0; i < nbrEles; i++ {
			val := m.getVarForModbus(req, firstEle+i)
			binary.BigEndian.PutUint16(pdu[6+i*2:], uint16(int16(val)))
		}

	case fcDiagnostics:
		pdu = make([]byte, 5)
		pdu[0] = fc
		binary.BigEndian.PutUint16(pdu[1:], 0)   // subfunction 0 (echo)
		binary.BigEndian.PutUint16(pdu[3:], 257) // hardcoded test value

	default:
		return nil
	}

	return pdu
}

// serialTransaction sends a Modbus RTU frame and reads the response.
func (m *modbusMaster) serialTransaction(pdu []byte) ([]byte, error) {
	if m.serialPort == nil {
		return nil, fmt.Errorf("serial port not open")
	}

	// Get slave address from current request
	m.mu.Lock()
	req := &m.cfg.Requests[m.currentReq]
	m.mu.Unlock()

	slaveAddr, err := strconv.Atoi(req.SlaveAddr)
	if err != nil {
		return nil, fmt.Errorf("invalid slave address %q: %w", req.SlaveAddr, err)
	}

	// Build RTU frame: [slave_addr][pdu...][crc_lo][crc_hi]
	frame := make([]byte, 1+len(pdu)+2)
	frame[0] = byte(slaveAddr)
	copy(frame[1:], pdu)
	crc := crc16(frame[:len(frame)-2])
	frame[len(frame)-2] = byte(crc & 0xFF)
	frame[len(frame)-1] = byte(crc >> 8)

	// Send
	if _, err := m.serialPort.Write(frame); err != nil {
		return nil, fmt.Errorf("serial write: %w", err)
	}

	if m.cfg.TimeAfterTransmit > 0 {
		time.Sleep(time.Duration(m.cfg.TimeAfterTransmit) * time.Millisecond)
	}

	// Read response
	m.serialPort.SetReadTimeout(time.Duration(m.cfg.TimeOutReceipt) * time.Millisecond)
	buf := make([]byte, 256)
	n, err := m.serialPort.Read(buf)
	if err != nil {
		return nil, fmt.Errorf("serial read: %w", err)
	}
	if n < 4 { // min: addr + fc + crc(2)
		return nil, fmt.Errorf("response too short: %d bytes", n)
	}

	// Verify CRC
	respCRC := crc16(buf[:n-2])
	gotCRC := uint16(buf[n-2]) | uint16(buf[n-1])<<8
	if respCRC != gotCRC {
		return nil, fmt.Errorf("CRC mismatch")
	}

	// Return PDU (skip slave addr, strip CRC)
	return buf[1 : n-2], nil
}

// tcpTransaction sends a Modbus TCP frame and reads the response.
func (m *modbusMaster) tcpTransaction(addr string, pdu []byte) ([]byte, error) {
	conn, err := m.getTCPConn(addr)
	if err != nil {
		return nil, err
	}

	// Build MBAP header: [trans_id(2)][protocol(2)][length(2)][unit_id(1)]
	m.mu.Lock()
	req := &m.cfg.Requests[m.currentReq]
	m.mu.Unlock()

	unitID := byte(0)
	if id, err := strconv.Atoi(req.SlaveAddr); err == nil && !strings.Contains(req.SlaveAddr, ".") {
		unitID = byte(id)
	}

	mbap := make([]byte, 7+len(pdu))
	// Transaction ID (could be incrementing, using 0 for simplicity)
	binary.BigEndian.PutUint16(mbap[0:], 0)
	// Protocol ID (0 = Modbus)
	binary.BigEndian.PutUint16(mbap[2:], 0)
	// Length (unit_id + pdu)
	binary.BigEndian.PutUint16(mbap[4:], uint16(1+len(pdu)))
	// Unit ID
	mbap[6] = unitID
	copy(mbap[7:], pdu)

	conn.SetDeadline(time.Now().Add(time.Duration(m.cfg.TimeOutReceipt) * time.Millisecond))
	if _, err := conn.Write(mbap); err != nil {
		// Connection lost, remove and retry next time
		m.mu.Lock()
		delete(m.tcpConns, addr)
		m.mu.Unlock()
		conn.Close()
		return nil, fmt.Errorf("tcp write: %w", err)
	}

	if m.cfg.TimeAfterTransmit > 0 {
		time.Sleep(time.Duration(m.cfg.TimeAfterTransmit) * time.Millisecond)
	}

	// Read MBAP header (7 bytes)
	header := make([]byte, 7)
	if _, err := io.ReadFull(conn, header); err != nil {
		m.mu.Lock()
		delete(m.tcpConns, addr)
		m.mu.Unlock()
		conn.Close()
		return nil, fmt.Errorf("tcp read header: %w", err)
	}

	pduLen := int(binary.BigEndian.Uint16(header[4:])) - 1 // subtract unit_id
	if pduLen <= 0 || pduLen > 256 {
		return nil, fmt.Errorf("invalid response length: %d", pduLen)
	}

	respPDU := make([]byte, pduLen)
	if _, err := io.ReadFull(conn, respPDU); err != nil {
		return nil, fmt.Errorf("tcp read pdu: %w", err)
	}

	return respPDU, nil
}

func (m *modbusMaster) getTCPConn(addr string) (net.Conn, error) {
	m.mu.Lock()
	conn, ok := m.tcpConns[addr]
	m.mu.Unlock()
	if ok {
		return conn, nil
	}

	// Parse address — default port 502
	host := addr
	if !strings.Contains(host, ":") {
		host = host + ":502"
	}

	conn, err := net.DialTimeout("tcp", host, time.Duration(m.cfg.TimeOutReceipt)*time.Millisecond)
	if err != nil {
		return nil, fmt.Errorf("tcp connect %s: %w", host, err)
	}

	m.mu.Lock()
	m.tcpConns[addr] = conn
	m.mu.Unlock()
	return conn, nil
}

func (m *modbusMaster) processResponse(req *modbusRequest, pdu []byte) {
	if len(pdu) < 1 {
		return
	}

	fc := pdu[0]
	// Check for error response
	if fc&0x80 != 0 {
		if m.cfg.DebugLevel >= 1 {
			exCode := byte(0)
			if len(pdu) > 1 {
				exCode = pdu[1]
			}
			m.logger.Warn("modbus exception", "fc", fc&0x7F, "exception", exCode)
		}
		return
	}

	firstEle := req.FirstModbusElement - m.cfg.ElementOffset
	if firstEle < 0 {
		firstEle = 0
	}

	switch fc {
	case fcReadInputs, fcReadCoils:
		// Response: [fc][byte_count][data...]
		if len(pdu) < 2 {
			return
		}
		byteCount := int(pdu[1])
		if len(pdu) < 2+byteCount {
			return
		}
		for i := 0; i < req.NbrModbusElements; i++ {
			bit := (pdu[2+i/8] >> (i % 8)) & 1
			val := int(bit)
			if req.LogicInverted {
				val = 1 - val
			}
			m.setVarFromModbus(req, firstEle+i, val)
		}

	case fcReadHoldRegs, fcReadInputRegs:
		// Response: [fc][byte_count][data...]
		if len(pdu) < 2 {
			return
		}
		byteCount := int(pdu[1])
		if len(pdu) < 2+byteCount {
			return
		}
		for i := 0; i < req.NbrModbusElements && i*2+1 < byteCount; i++ {
			val := int(int16(binary.BigEndian.Uint16(pdu[2+i*2:])))
			m.setVarFromModbus(req, firstEle+i, val)
		}

	case fcForceCoil, fcForceCoils, fcWriteReg, fcWriteRegs:
		// Write responses are just echo confirmations — nothing to process

	case fcDiagnostics:
		// Echo test: response should contain data=257 at bytes [3:5]
		if len(pdu) >= 5 {
			echoVal := binary.BigEndian.Uint16(pdu[3:5])
			if echoVal == 257 {
				if m.cfg.DebugLevel >= 2 {
					m.logger.Info("modbus echo from slave is correct", "slave", req.SlaveAddr)
				}
			} else {
				m.logger.Warn("modbus echo from slave is WRONG", "slave", req.SlaveAddr, "got", echoVal)
			}
		}
	}
}

func (m *modbusMaster) setVarFromModbus(req *modbusRequest, modbusNum int, value int) {
	firstEle := req.FirstModbusElement - m.cfg.ElementOffset
	if firstEle < 0 {
		firstEle = 0
	}
	varNum := modbusNum - firstEle + req.OffsetVarMapped

	switch req.TypeReq {
	case modbusReqInputsRead, modbusReqCoilsRead:
		switch m.cfg.MapCoilRead {
		case mapBVar:
			C.write_var_ext(m.rt, C.CL_VAR_MEM_BIT, C.int(varNum), C.int(value))
		case mapQVar:
			C.write_var_ext(m.rt, C.CL_VAR_PHYS_OUTPUT, C.int(varNum), C.int(value))
		}
	case modbusReqRegistersRead, modbusReqHoldRead:
		switch m.cfg.MapRegisterRead {
		case mapWVar:
			C.write_var_ext(m.rt, C.CL_VAR_MEM_WORD, C.int(varNum), C.int(value))
		case mapQWVar:
			C.write_var_ext(m.rt, C.CL_VAR_PHYS_WORD_OUTPUT, C.int(varNum), C.int(value))
		}
	}
}

func (m *modbusMaster) getVarForModbus(req *modbusRequest, modbusNum int) int {
	firstEle := req.FirstModbusElement - m.cfg.ElementOffset
	if firstEle < 0 {
		firstEle = 0
	}
	varNum := modbusNum - firstEle + req.OffsetVarMapped

	switch req.TypeReq {
	case modbusReqCoilsWrite:
		switch m.cfg.MapCoilWrite {
		case mapBVar:
			return int(readVarGo(m.rt, C.CL_VAR_MEM_BIT, varNum))
		case mapQVar:
			return int(readVarGo(m.rt, C.CL_VAR_PHYS_OUTPUT, varNum))
		case mapIVar:
			return int(readVarGo(m.rt, C.CL_VAR_PHYS_INPUT, varNum))
		}
	case modbusReqRegistersWrite:
		switch m.cfg.MapRegisterWrite {
		case mapWVar:
			return int(readVarGo(m.rt, C.CL_VAR_MEM_WORD, varNum))
		case mapQWVar:
			return int(readVarGo(m.rt, C.CL_VAR_PHYS_WORD_OUTPUT, varNum))
		case mapIWVar:
			return int(readVarGo(m.rt, C.CL_VAR_PHYS_WORD_INPUT, varNum))
		}
	}
	return 0
}

// readVarGo reads a CL variable from Go side (thread-safe for reads).
func readVarGo(rt *C.classicladder_rt_t, varType C.int, offset int) C.int {
	// Use the same read logic as the C side but from Go
	switch varType {
	case C.CL_VAR_MEM_BIT:
		return C.int(rt.var_bits[offset])
	case C.CL_VAR_PHYS_INPUT:
		return C.int(rt.var_bits[rt.sizes.nbr_bits+C.int(offset)])
	case C.CL_VAR_PHYS_OUTPUT:
		return C.int(rt.var_bits[rt.sizes.nbr_bits+rt.sizes.nbr_phys_inputs+C.int(offset)])
	case C.CL_VAR_MEM_WORD:
		return C.int(rt.var_words[offset])
	case C.CL_VAR_PHYS_WORD_INPUT:
		return C.int(rt.var_words[rt.sizes.nbr_words+C.int(offset)])
	case C.CL_VAR_PHYS_WORD_OUTPUT:
		return C.int(rt.var_words[rt.sizes.nbr_words+rt.sizes.nbr_s32_in+C.int(offset)])
	}
	return 0
}

func (m *modbusMaster) openSerial() error {
	mode := &serial.Mode{
		BaudRate: m.cfg.SerialSpeed,
		DataBits: m.cfg.SerialDataBits,
	}
	switch m.cfg.SerialStopBits {
	case 2:
		mode.StopBits = serial.TwoStopBits
	default:
		mode.StopBits = serial.OneStopBit
	}
	switch m.cfg.SerialParity {
	case 1:
		mode.Parity = serial.OddParity
	case 2:
		mode.Parity = serial.EvenParity
	default:
		mode.Parity = serial.NoParity
	}

	port, err := serial.Open(m.cfg.SerialPort, mode)
	if err != nil {
		return err
	}
	m.serialPort = port
	return nil
}

// CRC-16/Modbus
func crc16(data []byte) uint16 {
	crc := uint16(0xFFFF)
	for _, b := range data {
		crc ^= uint16(b)
		for i := 0; i < 8; i++ {
			if crc&1 != 0 {
				crc = (crc >> 1) ^ 0xA001
			} else {
				crc >>= 1
			}
		}
	}
	return crc
}
