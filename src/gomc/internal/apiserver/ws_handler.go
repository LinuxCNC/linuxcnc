// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package apiserver

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"net/http"
	"strings"
	"sync"
	"time"

	"nhooyr.io/websocket"
)

// WatchFunc is called periodically by the watch server to produce a snapshot.
// Generated code produces these from the registered callbacks.
type WatchFunc func() (json.RawMessage, error)

// WatchFactory is called ONCE at subscribe time with the client's args.
// It returns a per-connection WatchFunc (stateful closure with its own diff state).
// This avoids expensive serialization on every tick — the closure can diff at the
// source data level and only serialize changed values.
type WatchFactory func(args json.RawMessage) (WatchFunc, error)

// BinaryWatchFunc is called periodically by the watch server to produce a
// binary snapshot. Used for bulk data (e.g. scope sample buffers) where JSON
// would be too large. The uint64 is a generation counter for change detection.
type BinaryWatchFunc func() ([]byte, uint64, error)

// CommandFunc handles a command sent by the client over the WebSocket.
// req is the JSON-encoded arguments; returns the JSON-encoded response.
type CommandFunc func(req json.RawMessage) (json.RawMessage, error)

// WatchFuncMeta describes a watchable function.
type WatchFuncMeta struct {
	Name        string          // e.g. "get_status"
	DefaultRate time.Duration   // e.g. 50ms
	Watch       WatchFunc       // JSON watch — shared across connections (no per-conn state)
	Factory     WatchFactory    // Per-connection watch factory (mutually exclusive with Watch)
	BinaryWatch BinaryWatchFunc // Binary watch — sent as binary frames
	Delta       bool            // If true, diff JSON top-level keys per connection.
}

// CommandMeta describes a command callable over the WebSocket.
type CommandMeta struct {
	Name    string // e.g. "jog_start"
	Handler CommandFunc
}

// CommandsFromAPI creates CommandMeta entries for every dispatchable function
// in the given registered API. This exposes a REST API's functions as WS
// commands without manual per-function wiring.
func CommandsFromAPI(api *RegisteredAPI) []CommandMeta {
	if api == nil || api.Meta == nil {
		return nil
	}
	cmds := make([]CommandMeta, 0, len(api.Meta.Funcs))
	for _, fn := range api.Meta.Funcs {
		if fn.Dispatch == nil {
			continue
		}
		fn := fn // capture loop variable
		cb := api.Callbacks
		cmds = append(cmds, CommandMeta{
			Name: fn.Name,
			Handler: func(req json.RawMessage) (json.RawMessage, error) {
				res, err := fn.Dispatch(cb, []byte(req))
				return json.RawMessage(res), err
			},
		})
	}
	return cmds
}

// WatchAPI holds registered watch functions and commands for one API instance.
type WatchAPI struct {
	APIName  string
	Instance string
	Watches  []WatchFuncMeta
	Commands []CommandMeta
}

// WatchRegistry tracks all registered watch APIs.
type WatchRegistry struct {
	mu   sync.RWMutex
	apis map[string]*WatchAPI // key: "apiname/instance"
}

// NewWatchRegistry creates a new watch registry.
func NewWatchRegistry() *WatchRegistry {
	return &WatchRegistry{
		apis: make(map[string]*WatchAPI),
	}
}

// Register adds a watch API to the registry.
func (r *WatchRegistry) Register(api *WatchAPI) {
	r.mu.Lock()
	defer r.mu.Unlock()
	key := api.APIName + "/" + api.Instance
	r.apis[key] = api
}

// Get returns a watch API by name and instance.
func (r *WatchRegistry) Get(apiName, instance string) *WatchAPI {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.apis[apiName+"/"+instance]
}

// All returns all registered watch APIs.
func (r *WatchRegistry) All() []*WatchAPI {
	r.mu.RLock()
	defer r.mu.RUnlock()
	result := make([]*WatchAPI, 0, len(r.apis))
	for _, api := range r.apis {
		result = append(result, api)
	}
	return result
}

// --- WebSocket protocol messages ---

// wsSubscribe is sent by the client to start receiving updates.
type wsSubscribe struct {
	Action   string          `json:"action"`         // "subscribe"
	API      string          `json:"api"`            // "axis"
	Instance string          `json:"instance"`       // "default"
	Func     string          `json:"func"`           // "get_status"
	RateMS   int             `json:"rate_ms"`        // 50
	Args     json.RawMessage `json:"args,omitempty"` // optional args passed to WatchFactory
}

// wsUnsubscribe is sent by the client to stop receiving updates.
type wsUnsubscribe struct {
	Action   string `json:"action"`   // "unsubscribe"
	API      string `json:"api"`      // "axis"
	Instance string `json:"instance"` // "default"
	Func     string `json:"func"`     // "get_status"
}

// wsCall is sent by the client to invoke a command.
type wsCall struct {
	Action   string          `json:"action"`   // "call"
	API      string          `json:"api"`      // "axis"
	Instance string          `json:"instance"` // "default"
	Func     string          `json:"func"`     // "jog_start"
	ID       int             `json:"id"`       // client-assigned request ID
	Args     json.RawMessage `json:"args"`     // function arguments
}

// wsUpdate is sent by the server when a watch function produces new data.
type wsUpdate struct {
	Type     string          `json:"type"`     // "update"
	API      string          `json:"api"`      // "axis"
	Instance string          `json:"instance"` // "default"
	Func     string          `json:"func"`     // "get_status"
	Data     json.RawMessage `json:"data"`
}

// wsResult is sent by the server in response to a call.
type wsResult struct {
	Type  string          `json:"type"` // "result"
	ID    int             `json:"id"`   // echoed request ID
	Data  json.RawMessage `json:"data,omitempty"`
	Error string          `json:"error,omitempty"`
}

// wsError is sent by the server for protocol errors.
type wsError struct {
	Type    string `json:"type"` // "error"
	Message string `json:"message"`
}

// --- WebSocket handler ---

// WatchHandler handles WebSocket connections for the watch channel.
type WatchHandler struct {
	registry *WatchRegistry
	logger   *slog.Logger
	ctx      context.Context
	cancel   context.CancelFunc
}

// NewWatchHandler creates a new WebSocket watch handler.
func NewWatchHandler(registry *WatchRegistry) *WatchHandler {
	ctx, cancel := context.WithCancel(context.Background())
	return &WatchHandler{registry: registry, logger: slog.Default(), ctx: ctx, cancel: cancel}
}

// Close cancels all active WebSocket connections managed by this handler.
// Call this during server shutdown to ensure goroutines exit cleanly.
func (h *WatchHandler) Close() {
	h.cancel()
}

// SetLogger sets the logger for the watch handler.
func (h *WatchHandler) SetLogger(logger *slog.Logger) {
	h.logger = logger
}

// ServeHTTP upgrades the connection to WebSocket and runs the watch loop.
func (h *WatchHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	conn, err := websocket.Accept(w, r, &websocket.AcceptOptions{
		// Allow any origin for now — tighten in production
		InsecureSkipVerify: true,
	})
	if err != nil {
		h.logger.Warn("websocket accept failed", "error", err)
		return
	}
	defer conn.Close(websocket.StatusNormalClosure, "")

	ctx, cancel := context.WithCancel(h.ctx)
	defer cancel()

	c := &wsConn{
		conn:    conn,
		handler: h,
		ctx:     ctx,
		cancel:  cancel,
		subs:    make(map[string]context.CancelFunc),
	}

	c.readLoop()
}

// wsConn manages one WebSocket client connection.
type wsConn struct {
	conn    *websocket.Conn
	handler *WatchHandler
	ctx     context.Context
	cancel  context.CancelFunc

	writeMu sync.Mutex // serializes writes (nhooyr supports concurrent writes, but we want ordered JSON)

	mu   sync.Mutex
	subs map[string]context.CancelFunc // key: "api/instance/func" → cancel the push goroutine
}

func (c *wsConn) writeJSON(v interface{}) error {
	c.writeMu.Lock()
	defer c.writeMu.Unlock()

	data, err := json.Marshal(v)
	if err != nil {
		return err
	}
	return c.conn.Write(c.ctx, websocket.MessageText, data)
}

func (c *wsConn) writeBinary(data []byte) error {
	c.writeMu.Lock()
	defer c.writeMu.Unlock()
	return c.conn.Write(c.ctx, websocket.MessageBinary, data)
}

func (c *wsConn) sendError(msg string) {
	c.writeJSON(wsError{Type: "error", Message: msg})
}

func (c *wsConn) readLoop() {
	for {
		_, data, err := c.conn.Read(c.ctx)
		if err != nil {
			return // connection closed
		}

		// Peek at the action field
		var msg struct {
			Action string `json:"action"`
		}
		if err := json.Unmarshal(data, &msg); err != nil {
			c.sendError("invalid JSON")
			continue
		}

		switch msg.Action {
		case "subscribe":
			var sub wsSubscribe
			if err := json.Unmarshal(data, &sub); err != nil {
				c.sendError("invalid subscribe message")
				continue
			}
			c.handleSubscribe(sub)

		case "unsubscribe":
			var unsub wsUnsubscribe
			if err := json.Unmarshal(data, &unsub); err != nil {
				c.sendError("invalid unsubscribe message")
				continue
			}
			c.handleUnsubscribe(unsub)

		case "call":
			var call wsCall
			if err := json.Unmarshal(data, &call); err != nil {
				c.sendError("invalid call message")
				continue
			}
			c.handleCall(call)

		default:
			c.sendError(fmt.Sprintf("unknown action: %q", msg.Action))
		}
	}
}

func (c *wsConn) handleSubscribe(sub wsSubscribe) {
	api := c.handler.registry.Get(sub.API, sub.Instance)
	if api == nil {
		c.sendError(fmt.Sprintf("unknown API: %s/%s", sub.API, sub.Instance))
		return
	}

	var watchMeta *WatchFuncMeta
	for i := range api.Watches {
		if api.Watches[i].Name == sub.Func {
			watchMeta = &api.Watches[i]
			break
		}
	}
	if watchMeta == nil {
		c.sendError(fmt.Sprintf("unknown watch function: %s", sub.Func))
		return
	}

	rate := watchMeta.DefaultRate
	if sub.RateMS > 0 {
		requested := time.Duration(sub.RateMS) * time.Millisecond
		// Clamp to minimum 10ms
		if requested < 10*time.Millisecond {
			requested = 10 * time.Millisecond
		}
		rate = requested
	}

	key := sub.API + "/" + sub.Instance + "/" + sub.Func

	// Cancel existing subscription for this key
	c.mu.Lock()
	if cancelFn, ok := c.subs[key]; ok {
		cancelFn()
	}
	subCtx, cancelFn := context.WithCancel(c.ctx)
	c.subs[key] = cancelFn
	c.mu.Unlock()

	// Start push goroutine
	if watchMeta.BinaryWatch != nil {
		go c.pushLoopBinary(subCtx, sub.Func, rate, watchMeta.BinaryWatch)
	} else if watchMeta.Factory != nil {
		watchFn, err := watchMeta.Factory(sub.Args)
		if err != nil {
			c.sendError(fmt.Sprintf("watch factory error: %v", err))
			cancelFn()
			c.mu.Lock()
			delete(c.subs, key)
			c.mu.Unlock()
			return
		}
		go c.pushLoop(subCtx, sub.API, sub.Instance, sub.Func, rate, watchFn, watchMeta.Delta)
	} else {
		go c.pushLoop(subCtx, sub.API, sub.Instance, sub.Func, rate, watchMeta.Watch, watchMeta.Delta)
	}
}

func (c *wsConn) handleUnsubscribe(unsub wsUnsubscribe) {
	key := unsub.API + "/" + unsub.Instance + "/" + unsub.Func

	c.mu.Lock()
	if cancelFn, ok := c.subs[key]; ok {
		cancelFn()
		delete(c.subs, key)
	}
	c.mu.Unlock()
}

func (c *wsConn) handleCall(call wsCall) {
	api := c.handler.registry.Get(call.API, call.Instance)
	if api == nil {
		c.writeJSON(wsResult{
			Type:  "result",
			ID:    call.ID,
			Error: fmt.Sprintf("unknown API: %s/%s", call.API, call.Instance),
		})
		return
	}

	var cmdMeta *CommandMeta
	for i := range api.Commands {
		if api.Commands[i].Name == call.Func {
			cmdMeta = &api.Commands[i]
			break
		}
	}
	if cmdMeta == nil {
		c.writeJSON(wsResult{
			Type:  "result",
			ID:    call.ID,
			Error: fmt.Sprintf("unknown command: %s", call.Func),
		})
		return
	}

	result, err := cmdMeta.Handler(call.Args)
	if err != nil {
		c.writeJSON(wsResult{
			Type:  "result",
			ID:    call.ID,
			Error: err.Error(),
		})
		return
	}

	c.writeJSON(wsResult{
		Type: "result",
		ID:   call.ID,
		Data: result,
	})
}

func (c *wsConn) pushLoop(ctx context.Context, apiName, instance, funcName string, rate time.Duration, watch WatchFunc, delta bool) {
	ticker := time.NewTicker(rate)
	defer ticker.Stop()

	// Resolve funcName for update messages — strip "get_" prefix for cleaner names
	updateFunc := funcName

	var prevData json.RawMessage           // suppress unchanged sends
	var prevMap map[string]json.RawMessage // per-connection delta state

	// Immediate first poll — deliver data to new subscriber without waiting
	// for the first ticker tick.
	if data, err := watch(); err == nil && data != nil {
		sendData := data
		if delta {
			sendData = c.deltaEncode(data, &prevMap)
		}
		if sendData != nil {
			if err := c.writeJSON(wsUpdate{
				Type:     "update",
				API:      apiName,
				Instance: instance,
				Func:     updateFunc,
				Data:     sendData,
			}); err != nil {
				return
			}
			prevData = append(prevData[:0], data...)
		}
	}

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			data, err := watch()
			if err != nil {
				// Log but don't kill the subscription — transient errors are normal
				continue
			}
			if data == nil {
				// No data — skip this tick.
				continue
			}

			// Skip if nothing changed since last send
			if bytes.Equal(data, prevData) {
				continue
			}

			sendData := data
			if delta {
				sendData = c.deltaEncode(data, &prevMap)
				if sendData == nil {
					continue // nothing changed
				}
			}

			if err := c.writeJSON(wsUpdate{
				Type:     "update",
				API:      apiName,
				Instance: instance,
				Func:     updateFunc,
				Data:     sendData,
			}); err != nil {
				return // write failed — connection dead
			}
			prevData = append(prevData[:0], data...)
		}
	}
}

// pushLoopBinary pushes binary watch data to the client as binary WebSocket
// frames. The frame format is: funcName + '\0' + payload, so the client can
// demux multiple binary watch subscriptions on one connection.
func (c *wsConn) pushLoopBinary(ctx context.Context, funcName string, rate time.Duration, watch BinaryWatchFunc) {
	ticker := time.NewTicker(rate)
	defer ticker.Stop()

	prefix := append([]byte(funcName), 0) // "func_name\0"
	var sentGen uint64

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			payload, gen, err := watch()
			if err != nil || payload == nil {
				continue
			}

			// Skip if generation unchanged since last send
			if gen > 0 && gen == sentGen {
				continue
			}

			frame := make([]byte, len(prefix)+len(payload))
			copy(frame, prefix)
			copy(frame[len(prefix):], payload)

			if err := c.writeBinary(frame); err != nil {
				return // write failed — connection dead
			}
			sentGen = gen
		}
	}
}

// deltaEncode compares current JSON with the per-connection previous state
// and returns only changed top-level keys. Returns nil if nothing changed.
// First call returns the full data.
func (c *wsConn) deltaEncode(data json.RawMessage, prevMap *map[string]json.RawMessage) json.RawMessage {
	var curMap map[string]json.RawMessage
	if err := json.Unmarshal(data, &curMap); err != nil {
		return data // can't parse — send full
	}

	prev := *prevMap
	if prev == nil {
		// First message — send full snapshot.
		*prevMap = curMap
		return data
	}

	// Build delta: only keys whose JSON bytes changed.
	delta := make(map[string]json.RawMessage, len(curMap)/4)
	for k, v := range curMap {
		oldV, ok := prev[k]
		if !ok || string(v) != string(oldV) {
			delta[k] = v
		}
	}

	*prevMap = curMap

	if len(delta) == 0 {
		return nil
	}

	result, err := json.Marshal(delta)
	if err != nil {
		return data
	}
	return result
}

// AddWatchEndpoint registers the WebSocket handler on the server's mux.
func (s *Server) AddWatchEndpoint(registry *WatchRegistry) {
	handler := NewWatchHandler(registry)
	handler.SetLogger(s.logger)
	s.watchHandler = handler
	pattern := strings.TrimSuffix(s.prefix, "/") + "/watch"
	s.mux.Handle(pattern, handler)
}
