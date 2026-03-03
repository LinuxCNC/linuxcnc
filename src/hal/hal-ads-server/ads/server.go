package ads

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"sync"
	"time"
)

// readDeadlineInterval is the short read deadline used when waiting for the
// AMS/TCP header. Using a short deadline allows Stop() to interrupt blocked
// reads without needing per-connection close calls for normal shutdown paths.
const readDeadlineInterval = 100 * time.Millisecond

// amsDataReadTimeout is the read deadline applied after the AMS/TCP header has
// been received, covering the variable-length AMS payload. A longer timeout
// accommodates TCP fragmentation and slow networks while still detecting
// genuinely stuck connections.
const amsDataReadTimeout = 5 * time.Second

// shutdownTimeout is the maximum time Stop() will wait for active connections
// to finish after signalling shutdown. If this elapses, Stop() returns anyway
// so the process can exit.
const shutdownTimeout = 2 * time.Second

// Server listens for ADS connections, parses AMS/TCP packets, and dispatches
// ADS commands to a SymbolTable.
type Server struct {
	addr     string
	netID    AMSNetID
	port     uint16
	symbols  *SymbolTable
	verbose  bool
	listener net.Listener
	wg       sync.WaitGroup
	quit     chan struct{}
	connsMu  sync.Mutex
	conns    map[net.Conn]struct{}
}

// NewServer creates a new ADS server listening on addr (e.g. ":48898").
// netID and port identify this device in AMS routing.
func NewServer(addr string, netID AMSNetID, port uint16, symbols *SymbolTable, verbose bool) *Server {
	return &Server{
		addr:    addr,
		netID:   netID,
		port:    port,
		symbols: symbols,
		verbose: verbose,
		quit:    make(chan struct{}),
		conns:   make(map[net.Conn]struct{}),
	}
}

// Start begins listening for ADS connections. It returns after the listener
// is bound and ready. Call Stop() to shut down the server.
func (s *Server) Start() error {
	ln, err := net.Listen("tcp", s.addr)
	if err != nil {
		return fmt.Errorf("ADS server listen %s: %w", s.addr, err)
	}
	s.listener = ln
	log.Printf("ADS server listening on %s (AMS Net ID %s)", s.addr, s.netID)

	s.wg.Add(1)
	go s.acceptLoop()
	return nil
}

// Stop closes the listener and all active connections, then waits for all
// connection goroutines to finish.
func (s *Server) Stop() {
	close(s.quit)
	if s.listener != nil {
		s.listener.Close()
	}
	s.connsMu.Lock()
	for conn := range s.conns {
		conn.Close()
	}
	s.connsMu.Unlock()
	done := make(chan struct{})
	go func() {
		s.wg.Wait()
		close(done)
	}()
	select {
	case <-done:
	case <-time.After(shutdownTimeout):
		log.Printf("Timeout waiting for connections to close")
	}
}

// acceptLoop accepts incoming TCP connections.
func (s *Server) acceptLoop() {
	defer s.wg.Done()
	for {
		conn, err := s.listener.Accept()
		if err != nil {
			select {
			case <-s.quit:
				return // normal shutdown
			default:
				log.Printf("ADS accept error: %v", err)
				continue
			}
		}
		s.wg.Add(1)
		go s.handleConn(conn)
	}
}

// handleConn processes all AMS packets from a single TCP connection.
func (s *Server) handleConn(conn net.Conn) {
	defer s.wg.Done()
	defer conn.Close()

	s.connsMu.Lock()
	s.conns[conn] = struct{}{}
	s.connsMu.Unlock()
	defer func() {
		s.connsMu.Lock()
		delete(s.conns, conn)
		s.connsMu.Unlock()
	}()

	if s.verbose {
		log.Printf("ADS connection from %s", conn.RemoteAddr())
	}

	for {
		// Stage 1: short read deadline for the AMS/TCP header (idle-polling interval).
		// This allows Stop() to interrupt blocked reads between packets.
		conn.SetReadDeadline(time.Now().Add(readDeadlineInterval))

		tcpHdr := make([]byte, AMSTCPHeaderSize)
		_, err := io.ReadFull(conn, tcpHdr)
		if err != nil {
			// Check if this is just a timeout — if not shutting down, retry.
			var netErr net.Error
			if errors.As(err, &netErr) && netErr.Timeout() {
				select {
				case <-s.quit:
					return
				default:
					continue
				}
			}
			if err == io.EOF {
				if s.verbose {
					log.Printf("ADS client %s disconnected", conn.RemoteAddr())
				}
			} else {
				select {
				case <-s.quit:
				default:
					log.Printf("ADS read error from %s: %v", conn.RemoteAddr(), err)
				}
			}
			return
		}

		amsLen := binary.LittleEndian.Uint32(tcpHdr[2:])
		if amsLen < AMSHeaderSize {
			log.Printf("ADS packet too short from %s: %d", conn.RemoteAddr(), amsLen)
			return
		}

		// Stage 2: longer read deadline for the AMS payload. Once the TCP header
		// has arrived, we know the client is actively sending; a longer timeout
		// handles TCP fragmentation and slow networks.
		conn.SetReadDeadline(time.Now().Add(amsDataReadTimeout))

		amsData := make([]byte, amsLen)
		if _, err := io.ReadFull(conn, amsData); err != nil {
			select {
			case <-s.quit:
			default:
				log.Printf("ADS read error from %s: %v", conn.RemoteAddr(), err)
			}
			return
		}

		h := decodeAMSHeader(amsData[:AMSHeaderSize])
		hdr := &h
		payload := amsData[AMSHeaderSize:]

		if s.verbose {
			log.Printf("ADS cmd=0x%04X from %s port %d", hdr.CommandID, hdr.SourceNetID, hdr.SourcePort)
		}

		switch hdr.CommandID {
		case CmdReadDeviceInfo:
			s.handleReadDeviceInfo(conn, hdr)
		case CmdReadState:
			s.handleReadState(conn, hdr)
		case CmdRead:
			s.handleRead(conn, hdr, payload)
		case CmdWrite:
			s.handleWrite(conn, hdr, payload)
		case CmdReadWrite:
			s.handleReadWrite(conn, hdr, payload)
		default:
			// Respond with error for unsupported commands
			s.sendErrorResponse(conn, hdr, ErrUnknownCommandID)
		}
	}
}

// handleReadDeviceInfo responds to CmdReadDeviceInfo.
// Response: result(4) + majorVer(1) + minorVer(1) + buildVer(2) + devName(16)
func (s *Server) handleReadDeviceInfo(conn net.Conn, hdr *AMSHeader) {
	data := make([]byte, 24)
	binary.LittleEndian.PutUint32(data[0:], ErrNoError) // result
	data[4] = 3                                         // major version
	data[5] = 1                                         // minor version
	binary.LittleEndian.PutUint16(data[6:], 4024)       // build version
	copy(data[8:], []byte("hal-ads-server\x00"))        // device name (up to 16 bytes)
	if err := s.sendAMSResponse(conn, hdr, CmdReadDeviceInfo, ErrNoError, data); err != nil {
		log.Printf("ADS sendReadDeviceInfo error: %v", err)
	}
}

// handleReadState responds to CmdReadState.
// Response: result(4) + adsState(2) + deviceState(2)
// ADS state 5 = Run.
func (s *Server) handleReadState(conn net.Conn, hdr *AMSHeader) {
	data := make([]byte, 8)
	binary.LittleEndian.PutUint32(data[0:], ErrNoError) // result
	binary.LittleEndian.PutUint16(data[4:], 5)          // ADS state: Run
	binary.LittleEndian.PutUint16(data[6:], 0)          // device state
	if err := s.sendAMSResponse(conn, hdr, CmdReadState, ErrNoError, data); err != nil {
		log.Printf("ADS sendReadState error: %v", err)
	}
}

// handleRead processes CmdRead (ADS Read).
// Payload: indexGroup(4) + indexOffset(4) + length(4)
// Response: result(4) + length(4) + data
func (s *Server) handleRead(conn net.Conn, hdr *AMSHeader, payload []byte) {
	if len(payload) < 12 {
		s.sendErrorResponse(conn, hdr, ErrInternal)
		return
	}
	indexGroup := binary.LittleEndian.Uint32(payload[0:])
	indexOffset := binary.LittleEndian.Uint32(payload[4:])
	length := binary.LittleEndian.Uint32(payload[8:])

	if s.verbose {
		log.Printf("ADS Read: IG=0x%08X IO=0x%08X len=%d", indexGroup, indexOffset, length)
	}

	readData, errCode := s.symbols.ReadData(indexGroup, indexOffset, length)

	resp := make([]byte, 8+len(readData))
	binary.LittleEndian.PutUint32(resp[0:], errCode)
	binary.LittleEndian.PutUint32(resp[4:], uint32(len(readData)))
	copy(resp[8:], readData)

	if err := s.sendAMSResponse(conn, hdr, CmdRead, ErrNoError, resp); err != nil {
		log.Printf("ADS sendRead error: %v", err)
	}
}

// handleWrite processes CmdWrite (ADS Write).
// Payload: indexGroup(4) + indexOffset(4) + length(4) + data
// Response: result(4)
func (s *Server) handleWrite(conn net.Conn, hdr *AMSHeader, payload []byte) {
	if len(payload) < 12 {
		s.sendErrorResponse(conn, hdr, ErrInternal)
		return
	}
	indexGroup := binary.LittleEndian.Uint32(payload[0:])
	indexOffset := binary.LittleEndian.Uint32(payload[4:])
	length := binary.LittleEndian.Uint32(payload[8:])
	if uint32(len(payload)-12) < length {
		s.sendErrorResponse(conn, hdr, ErrInternal)
		return
	}
	writeData := payload[12 : 12+length]

	if s.verbose {
		log.Printf("ADS Write: IG=0x%08X IO=0x%08X len=%d", indexGroup, indexOffset, length)
	}

	errCode := s.symbols.WriteData(indexGroup, indexOffset, writeData)

	resp := make([]byte, 4)
	binary.LittleEndian.PutUint32(resp[0:], errCode)

	if err := s.sendAMSResponse(conn, hdr, CmdWrite, ErrNoError, resp); err != nil {
		log.Printf("ADS sendWrite error: %v", err)
	}
}

// handleReadWrite processes CmdReadWrite (ADS ReadWrite).
// Payload: indexGroup(4) + indexOffset(4) + readLength(4) + writeLength(4) + writeData
// Response: result(4) + readLength(4) + readData
func (s *Server) handleReadWrite(conn net.Conn, hdr *AMSHeader, payload []byte) {
	if len(payload) < 16 {
		s.sendErrorResponse(conn, hdr, ErrInternal)
		return
	}
	indexGroup := binary.LittleEndian.Uint32(payload[0:])
	indexOffset := binary.LittleEndian.Uint32(payload[4:])
	readLength := binary.LittleEndian.Uint32(payload[8:])
	writeLength := binary.LittleEndian.Uint32(payload[12:])
	if uint32(len(payload)-16) < writeLength {
		s.sendErrorResponse(conn, hdr, ErrInternal)
		return
	}
	writeData := payload[16 : 16+writeLength]

	if s.verbose {
		log.Printf("ADS ReadWrite: IG=0x%08X IO=0x%08X rlen=%d wlen=%d",
			indexGroup, indexOffset, readLength, writeLength)
	}

	readData, errCode := s.symbols.ReadWriteData(indexGroup, indexOffset, readLength, writeData)

	resp := make([]byte, 8+len(readData))
	binary.LittleEndian.PutUint32(resp[0:], errCode)
	binary.LittleEndian.PutUint32(resp[4:], uint32(len(readData)))
	copy(resp[8:], readData)

	if err := s.sendAMSResponse(conn, hdr, CmdReadWrite, ErrNoError, resp); err != nil {
		log.Printf("ADS sendReadWrite error: %v", err)
	}
}

// sendErrorResponse sends an ADS response with an error code and no payload.
func (s *Server) sendErrorResponse(conn net.Conn, hdr *AMSHeader, errCode uint32) {
	data := make([]byte, 4)
	binary.LittleEndian.PutUint32(data[0:], errCode)
	if err := s.sendAMSResponse(conn, hdr, hdr.CommandID, ErrNoError, data); err != nil {
		log.Printf("ADS sendError error: %v", err)
	}
}
