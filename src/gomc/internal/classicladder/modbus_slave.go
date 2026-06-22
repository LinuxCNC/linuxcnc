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
	"log/slog"
	"net"
	"sync"
)

// Modbus exception codes
const (
	modbusExcIllegalFunction    = 1
	modbusExcIllegalDataAddress = 2
	modbusExcIllegalDataValue   = 3
)

// modbusSlave implements a Modbus TCP server exposing %B and %W variables.
type modbusSlave struct {
	rt     *C.classicladder_rt_t
	logger *slog.Logger

	mu       sync.Mutex
	listener net.Listener
	cancel   context.CancelFunc
	running  bool
	port     int
}

func newModbusSlave(rt *C.classicladder_rt_t, logger *slog.Logger) *modbusSlave {
	return &modbusSlave{
		rt:     rt,
		logger: logger,
		port:   502,
	}
}

func (s *modbusSlave) start(port int) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.running {
		return
	}
	s.port = port

	ln, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		s.logger.Error("modbus slave listen failed", "port", port, "error", err)
		return
	}
	s.listener = ln
	s.running = true

	ctx, cancel := context.WithCancel(context.Background())
	s.cancel = cancel

	go s.acceptLoop(ctx)
	s.logger.Info("modbus slave started", "port", port)
}

func (s *modbusSlave) stop() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.running {
		return
	}
	s.cancel()
	s.listener.Close()
	s.running = false
}

func (s *modbusSlave) acceptLoop(ctx context.Context) {
	for {
		conn, err := s.listener.Accept()
		if err != nil {
			select {
			case <-ctx.Done():
				return
			default:
				s.logger.Warn("modbus slave accept error", "error", err)
				return
			}
		}
		go s.handleConn(ctx, conn)
	}
}

func (s *modbusSlave) handleConn(ctx context.Context, conn net.Conn) {
	defer conn.Close()

	buf := make([]byte, 512)
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		// Read MBAP header (7 bytes): transID(2) + protocolID(2) + length(2) + unitID(1)
		_, err := readFull(conn, buf[:7])
		if err != nil {
			return
		}

		pduLen := int(binary.BigEndian.Uint16(buf[4:6])) - 1 // subtract unitID byte
		if pduLen <= 0 || pduLen > 253 {
			return
		}

		// Read PDU
		_, err = readFull(conn, buf[7:7+pduLen])
		if err != nil {
			return
		}

		// Process request (PDU starts at buf[7])
		resp := s.processRequest(buf[7 : 7+pduLen])

		// Build MBAP response header
		var out [512]byte
		copy(out[0:2], buf[0:2]) // transaction ID
		out[2] = 0               // protocol ID
		out[3] = 0               // protocol ID
		respLen := len(resp) + 1 // +1 for unit ID
		out[4] = byte(respLen >> 8)
		out[5] = byte(respLen)
		out[6] = 1 // unit ID

		copy(out[7:], resp)
		conn.Write(out[:7+len(resp)])
	}
}

func readFull(conn net.Conn, buf []byte) (int, error) {
	total := 0
	for total < len(buf) {
		n, err := conn.Read(buf[total:])
		total += n
		if err != nil {
			return total, err
		}
	}
	return total, nil
}

// processRequest handles a Modbus PDU and returns the response PDU.
// Reads/writes %B (VAR_MEM_BIT) and %W (VAR_MEM_WORD) with offset 1.
func (s *modbusSlave) processRequest(pdu []byte) []byte {
	if len(pdu) < 1 {
		return []byte{0x80, modbusExcIllegalFunction}
	}

	fc := pdu[0]
	switch fc {
	case 1, 2: // Read coils / Read discrete inputs
		return s.handleReadBits(pdu)
	case 3, 4: // Read holding registers / Read input registers
		return s.handleReadWords(pdu)
	case 5: // Write single coil
		return s.handleWriteSingleBit(pdu)
	case 6: // Write single register
		return s.handleWriteSingleWord(pdu)
	case 15: // Write multiple coils
		return s.handleWriteMultipleBits(pdu)
	case 16: // Write multiple registers
		return s.handleWriteMultipleWords(pdu)
	default:
		return []byte{0x80 | fc, modbusExcIllegalFunction}
	}
}

func (s *modbusSlave) handleReadBits(pdu []byte) []byte {
	if len(pdu) < 5 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	firstBit := int(binary.BigEndian.Uint16(pdu[1:3]))
	nbrBits := int(binary.BigEndian.Uint16(pdu[3:5]))

	maxBits := int(s.rt.sizes.nbr_bits)
	if firstBit+1+nbrBits > maxBits || nbrBits > 2000 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}

	nbrBytes := (nbrBits + 7) / 8
	resp := make([]byte, 2+nbrBytes)
	resp[0] = pdu[0]
	resp[1] = byte(nbrBytes)

	for i := 0; i < nbrBits; i++ {
		val := C.read_var_ext(s.rt, C.CL_VAR_MEM_BIT, C.int(firstBit+1+i))
		if val != 0 {
			resp[2+i/8] |= 1 << (i % 8)
		}
	}
	return resp
}

func (s *modbusSlave) handleReadWords(pdu []byte) []byte {
	if len(pdu) < 5 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	firstWord := int(binary.BigEndian.Uint16(pdu[1:3]))
	nbrWords := int(binary.BigEndian.Uint16(pdu[3:5]))

	maxWords := int(s.rt.sizes.nbr_words)
	if firstWord+1+nbrWords > maxWords || nbrWords > 200 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}

	resp := make([]byte, 2+nbrWords*2)
	resp[0] = pdu[0]
	resp[1] = byte(nbrWords * 2)

	for i := 0; i < nbrWords; i++ {
		val := int(C.read_var_ext(s.rt, C.CL_VAR_MEM_WORD, C.int(firstWord+1+i)))
		resp[2+i*2] = byte(val >> 8)
		resp[2+i*2+1] = byte(val)
	}
	return resp
}

func (s *modbusSlave) handleWriteSingleBit(pdu []byte) []byte {
	if len(pdu) < 5 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	firstBit := int(binary.BigEndian.Uint16(pdu[1:3]))
	value := int(binary.BigEndian.Uint16(pdu[3:5]))

	maxBits := int(s.rt.sizes.nbr_bits)
	if firstBit+1 > maxBits {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	if value != 0xFF00 && value != 0x0000 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataValue}
	}

	bitVal := 0
	if value != 0 {
		bitVal = 1
	}
	C.write_var_ext(s.rt, C.CL_VAR_MEM_BIT, C.int(firstBit+1), C.int(bitVal))

	// Echo request as response
	resp := make([]byte, 5)
	copy(resp, pdu[:5])
	return resp
}

func (s *modbusSlave) handleWriteSingleWord(pdu []byte) []byte {
	if len(pdu) < 5 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	firstWord := int(binary.BigEndian.Uint16(pdu[1:3]))
	value := int(binary.BigEndian.Uint16(pdu[3:5]))

	maxWords := int(s.rt.sizes.nbr_words)
	if firstWord+1 > maxWords {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}

	C.write_var_ext(s.rt, C.CL_VAR_MEM_WORD, C.int(firstWord+1), C.int(value))

	// Echo request as response
	resp := make([]byte, 5)
	copy(resp, pdu[:5])
	return resp
}

func (s *modbusSlave) handleWriteMultipleBits(pdu []byte) []byte {
	if len(pdu) < 7 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	firstBit := int(binary.BigEndian.Uint16(pdu[1:3]))
	nbrBits := int(binary.BigEndian.Uint16(pdu[3:5]))
	byteCount := int(pdu[5])

	maxBits := int(s.rt.sizes.nbr_bits)
	if firstBit+1+nbrBits > maxBits || nbrBits > 2000 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	if (nbrBits+7)/8 > byteCount || len(pdu) < 6+byteCount {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataValue}
	}

	for i := 0; i < nbrBits; i++ {
		byteIdx := 6 + i/8
		bitIdx := i % 8
		val := 0
		if pdu[byteIdx]&(1<<bitIdx) != 0 {
			val = 1
		}
		C.write_var_ext(s.rt, C.CL_VAR_MEM_BIT, C.int(firstBit+1+i), C.int(val))
	}

	// Response: FC + first bit + nbr bits
	resp := make([]byte, 5)
	resp[0] = pdu[0]
	resp[1] = byte(firstBit >> 8)
	resp[2] = byte(firstBit)
	resp[3] = byte(nbrBits >> 8)
	resp[4] = byte(nbrBits)
	return resp
}

func (s *modbusSlave) handleWriteMultipleWords(pdu []byte) []byte {
	if len(pdu) < 8 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	firstWord := int(binary.BigEndian.Uint16(pdu[1:3]))
	nbrWords := int(binary.BigEndian.Uint16(pdu[3:5]))
	byteCount := int(pdu[5])

	maxWords := int(s.rt.sizes.nbr_words)
	if firstWord+1+nbrWords > maxWords || nbrWords > 200 {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataAddress}
	}
	if nbrWords*2 > byteCount || len(pdu) < 6+byteCount {
		return []byte{0x80 | pdu[0], modbusExcIllegalDataValue}
	}

	for i := 0; i < nbrWords; i++ {
		val := int(binary.BigEndian.Uint16(pdu[6+i*2 : 6+i*2+2]))
		C.write_var_ext(s.rt, C.CL_VAR_MEM_WORD, C.int(firstWord+1+i), C.int(val))
	}

	// Response: FC + first word + nbr words
	resp := make([]byte, 5)
	resp[0] = pdu[0]
	resp[1] = byte(firstWord >> 8)
	resp[2] = byte(firstWord)
	resp[3] = byte(nbrWords >> 8)
	resp[4] = byte(nbrWords)
	return resp
}
