package ads

import (
	"bytes"
	"encoding/binary"
	"log"
	"net"
	"sync"
	"time"
)

// ADS notification transmission modes.
const (
	// ADSTransModeCyclic sends notifications at each CycleTime interval
	// regardless of whether the data changed.
	ADSTransModeCyclic uint32 = 3
	// ADSTransModeOnChange sends notifications only when the data changes,
	// checked every CycleTime interval.
	ADSTransModeOnChange uint32 = 4
)

// subscription holds the state for a single device notification.
type subscription struct {
	handle      uint32
	indexGroup  uint32
	indexOffset uint32
	length      uint32
	transMode   uint32
	cycleTime   time.Duration // polling / send interval
	lastData    []byte        // last data sent (for on-change mode)
}

// notifyManager manages subscriptions for a single client connection.
// Each connection gets its own notifyManager, started in handleConn.
type notifyManager struct {
	mu      sync.Mutex
	subs    map[uint32]*subscription // handle → subscription
	nextHdl uint32
	symbols *SymbolTable
	conn    net.Conn
	server  *Server
	quit    chan struct{}
	wg      sync.WaitGroup
	stopped bool
	cNetID  AMSNetID // client AMS Net ID (set on first subscribe)
	cPort   uint16   // client AMS port (set on first subscribe)
}

// newNotifyManager creates a notification manager for a connection.
func newNotifyManager(s *Server, conn net.Conn) *notifyManager {
	return &notifyManager{
		subs:    make(map[uint32]*subscription),
		nextHdl: 1,
		symbols: s.symbols,
		conn:    conn,
		server:  s,
		quit:    make(chan struct{}),
	}
}

// start launches the background notification sender goroutine.
func (nm *notifyManager) start() {
	nm.wg.Add(1)
	go nm.sendLoop()
}

// stop signals the sender goroutine to exit and waits for it.
func (nm *notifyManager) stop() {
	nm.mu.Lock()
	nm.stopped = true
	nm.mu.Unlock()
	close(nm.quit)
	nm.wg.Wait()
}

// add creates a new subscription and returns its handle.
func (nm *notifyManager) add(indexGroup, indexOffset, length, transMode uint32, cycleTime time.Duration) uint32 {
	nm.mu.Lock()
	defer nm.mu.Unlock()

	h := nm.nextHdl
	nm.nextHdl++
	nm.subs[h] = &subscription{
		handle:      h,
		indexGroup:  indexGroup,
		indexOffset: indexOffset,
		length:      length,
		transMode:   transMode,
		cycleTime:   cycleTime,
	}
	return h
}

// del removes a subscription by handle. Returns true if found.
func (nm *notifyManager) del(handle uint32) bool {
	nm.mu.Lock()
	defer nm.mu.Unlock()
	if _, ok := nm.subs[handle]; !ok {
		return false
	}
	delete(nm.subs, handle)
	return true
}

// sendLoop is the background goroutine that periodically reads subscribed
// symbols and sends DeviceNotification packets to the client.
func (nm *notifyManager) sendLoop() {
	defer nm.wg.Done()

	// Tick at a fixed base interval. We check each subscription's own
	// cycleTime against the elapsed time since its last send.
	const baseInterval = 10 * time.Millisecond
	ticker := time.NewTicker(baseInterval)
	defer ticker.Stop()

	// Track the last send time per subscription handle.
	lastSend := make(map[uint32]time.Time)

	for {
		select {
		case <-nm.quit:
			return
		case now := <-ticker.C:
			nm.mu.Lock()
			if nm.stopped {
				nm.mu.Unlock()
				return
			}

			// Collect notifications that are due.
			var toSend []notifySample

			for _, sub := range nm.subs {
				ls, ok := lastSend[sub.handle]
				if ok && now.Sub(ls) < sub.cycleTime {
					continue // not due yet
				}

				data, errCode := nm.symbols.ReadData(sub.indexGroup, sub.indexOffset, sub.length)
				if errCode != ErrNoError {
					continue
				}

				// For on-change mode, skip if data hasn't changed.
				if sub.transMode == ADSTransModeOnChange {
					if sub.lastData != nil && bytes.Equal(data, sub.lastData) {
						continue
					}
				}

				sub.lastData = make([]byte, len(data))
				copy(sub.lastData, data)
				lastSend[sub.handle] = now

				toSend = append(toSend, notifySample{sub: sub, data: data})
			}
			nm.mu.Unlock()

			if len(toSend) > 0 {
				nm.sendNotifications(now, toSend)
			}
		}
	}
}

// notifySample is a pending notification sample ready to send.
type notifySample struct {
	sub  *subscription
	data []byte
}

// sendNotifications encodes and sends an AdsDeviceNotification packet
// containing all pending samples.
//
// Wire format of AdsDeviceNotification (CmdDeviceNotification = 0x0008):
//
//	uint32 length       — total bytes that follow
//	uint32 stamps       — number of AdsStampHeader entries (we always send 1)
//
//	AdsStampHeader:
//	  uint64 timestamp    — Windows FILETIME (100ns ticks since 1601-01-01)
//	  uint32 samples      — number of AdsNotificationSample entries
//
//	  AdsNotificationSample (repeated):
//	    uint32 notificationHandle
//	    uint32 sampleSize
//	    byte[sampleSize] data
func (nm *notifyManager) sendNotifications(now time.Time, items []notifySample) {
	// Compute total payload size.
	// stamps header: 4 (length) + 4 (stamps count) = 8
	// per stamp: 8 (timestamp) + 4 (samples count) = 12
	// per sample: 4 (handle) + 4 (size) + N (data)
	samplesSize := 0
	for _, item := range items {
		samplesSize += 4 + 4 + len(item.data) // handle + size + data
	}
	stampSize := 12 + samplesSize
	totalPayload := 8 + stampSize // length(4) + stamps(4) + stamp data

	payload := make([]byte, totalPayload)
	binary.LittleEndian.PutUint32(payload[0:], uint32(totalPayload))
	binary.LittleEndian.PutUint32(payload[4:], 1) // one stamp

	// Windows FILETIME: 100ns intervals since 1601-01-01.
	// Go epoch (1970-01-01) is 11644473600 seconds after the Windows epoch.
	const epochDiff = 11644473600
	ft := uint64(now.Unix()+epochDiff)*10_000_000 + uint64(now.Nanosecond()/100)
	binary.LittleEndian.PutUint64(payload[8:], ft)
	binary.LittleEndian.PutUint32(payload[16:], uint32(len(items))) // samples count

	off := 20
	for _, item := range items {
		binary.LittleEndian.PutUint32(payload[off:], item.sub.handle)
		binary.LittleEndian.PutUint32(payload[off+4:], uint32(len(item.data)))
		copy(payload[off+8:], item.data)
		off += 8 + len(item.data)
	}

	// Build AMS packet. DeviceNotification is a server-initiated request
	// (not a response to a client command), so it uses StateFlagRequest and
	// InvokeID 0.
	hdr := AMSHeader{
		TargetNetID: nm.clientNetID(),
		TargetPort:  nm.clientPort(),
		SourceNetID: nm.server.netID,
		SourcePort:  nm.server.port,
		CommandID:   CmdDeviceNotification,
		StateFlags:  StateFlagRequest,
		DataLength:  uint32(len(payload)),
		ErrorCode:   ErrNoError,
		InvokeID:    0,
	}

	pkt := make([]byte, AMSTCPHeaderSize+AMSHeaderSize+len(payload))
	pkt[0] = 0
	pkt[1] = 0
	binary.LittleEndian.PutUint32(pkt[2:], uint32(AMSHeaderSize)+uint32(len(payload)))
	encodeAMSHeader(pkt[AMSTCPHeaderSize:], &hdr)
	copy(pkt[AMSTCPHeaderSize+AMSHeaderSize:], payload)

	if _, err := nm.conn.Write(pkt); err != nil {
		if nm.server.verbose {
			log.Printf("ADS notification send error: %v", err)
		}
	}
}

// clientNetID and clientPort store the client's AMS address from the first
// AddNotification request. These are set by setClientAddr.
func (nm *notifyManager) clientNetID() AMSNetID { return nm.cNetID }
func (nm *notifyManager) clientPort() uint16    { return nm.cPort }

// setClientAddr records the client AMS address from the request header.
func (nm *notifyManager) setClientAddr(hdr *AMSHeader) {
	nm.cNetID = hdr.SourceNetID
	nm.cPort = hdr.SourcePort
}
