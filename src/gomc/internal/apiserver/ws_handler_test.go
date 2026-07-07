// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package apiserver

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http/httptest"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"nhooyr.io/websocket"
)

// TestWatchSubscribeReceivesUpdates verifies that subscribing to a watch function
// produces periodic update messages.
func TestWatchSubscribeReceivesUpdates(t *testing.T) {
	var counter int32

	reg := NewWatchRegistry()
	reg.Register(&WatchAPI{
		APIName:  "test",
		Instance: "default",
		Watches: []WatchFuncMeta{
			{
				Name:        "get_counter",
				DefaultRate: 50 * time.Millisecond,
				Watch: func() (json.RawMessage, error) {
					v := atomic.AddInt32(&counter, 1)
					return json.Marshal(map[string]int32{"value": v})
				},
			},
		},
	})

	handler := NewWatchHandler(reg)
	srv := httptest.NewServer(handler)
	defer srv.Close()

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")

	// Subscribe
	sub := wsSubscribe{
		Action:   "subscribe",
		API:      "test",
		Instance: "default",
		Func:     "get_counter",
		RateMS:   50,
	}
	subData, _ := json.Marshal(sub)
	if err := conn.Write(ctx, websocket.MessageText, subData); err != nil {
		t.Fatalf("write subscribe: %v", err)
	}

	// Read at least 3 updates
	for i := 0; i < 3; i++ {
		_, data, err := conn.Read(ctx)
		if err != nil {
			t.Fatalf("read update %d: %v", i, err)
		}

		var update wsUpdate
		if err := json.Unmarshal(data, &update); err != nil {
			t.Fatalf("unmarshal update %d: %v", i, err)
		}

		if update.Type != "update" {
			t.Fatalf("expected type=update, got %q", update.Type)
		}
		if update.API != "test" {
			t.Fatalf("expected api=test, got %q", update.API)
		}
		if update.Func != "get_counter" {
			t.Fatalf("expected func=get_counter, got %q", update.Func)
		}

		var result map[string]int32
		if err := json.Unmarshal(update.Data, &result); err != nil {
			t.Fatalf("unmarshal data %d: %v", i, err)
		}
		if result["value"] <= 0 {
			t.Fatalf("expected positive counter, got %d", result["value"])
		}
	}
}

// TestWatchCommandCall verifies that command calls over WebSocket work.
func TestWatchCommandCall(t *testing.T) {
	reg := NewWatchRegistry()
	reg.Register(&WatchAPI{
		APIName:  "test",
		Instance: "default",
		Commands: []CommandMeta{
			{
				Name: "echo",
				Handler: func(req json.RawMessage) (json.RawMessage, error) {
					return req, nil
				},
			},
			{
				Name: "fail",
				Handler: func(req json.RawMessage) (json.RawMessage, error) {
					return nil, fmt.Errorf("intentional error")
				},
			},
		},
	})

	handler := NewWatchHandler(reg)
	srv := httptest.NewServer(handler)
	defer srv.Close()

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")

	// Call echo
	call := wsCall{
		Action:   "call",
		API:      "test",
		Instance: "default",
		Func:     "echo",
		ID:       42,
		Args:     json.RawMessage(`{"hello":"world"}`),
	}
	callData, _ := json.Marshal(call)
	if err := conn.Write(ctx, websocket.MessageText, callData); err != nil {
		t.Fatalf("write call: %v", err)
	}

	_, data, err := conn.Read(ctx)
	if err != nil {
		t.Fatalf("read result: %v", err)
	}

	var result wsResult
	if err := json.Unmarshal(data, &result); err != nil {
		t.Fatalf("unmarshal result: %v", err)
	}
	if result.Type != "result" {
		t.Fatalf("expected type=result, got %q", result.Type)
	}
	if result.ID != 42 {
		t.Fatalf("expected id=42, got %d", result.ID)
	}
	if result.Error != "" {
		t.Fatalf("unexpected error: %s", result.Error)
	}
	if string(result.Data) != `{"hello":"world"}` {
		t.Fatalf("expected echo data, got %s", string(result.Data))
	}

	// Call fail
	failCall := wsCall{
		Action:   "call",
		API:      "test",
		Instance: "default",
		Func:     "fail",
		ID:       99,
		Args:     json.RawMessage(`{}`),
	}
	failData, _ := json.Marshal(failCall)
	if err := conn.Write(ctx, websocket.MessageText, failData); err != nil {
		t.Fatalf("write fail call: %v", err)
	}

	_, data, err = conn.Read(ctx)
	if err != nil {
		t.Fatalf("read fail result: %v", err)
	}

	var failResult wsResult
	if err := json.Unmarshal(data, &failResult); err != nil {
		t.Fatalf("unmarshal fail result: %v", err)
	}
	if failResult.Error != "intentional error" {
		t.Fatalf("expected error 'intentional error', got %q", failResult.Error)
	}
}

// TestWatchUnsubscribe verifies that unsubscribing stops updates.
func TestWatchUnsubscribe(t *testing.T) {
	reg := NewWatchRegistry()
	reg.Register(&WatchAPI{
		APIName:  "test",
		Instance: "default",
		Watches: []WatchFuncMeta{
			{
				Name:        "fast",
				DefaultRate: 20 * time.Millisecond,
				Watch: func() (json.RawMessage, error) {
					return json.Marshal(map[string]bool{"ok": true})
				},
			},
		},
	})

	handler := NewWatchHandler(reg)
	srv := httptest.NewServer(handler)
	defer srv.Close()

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")

	// Subscribe
	sub := wsSubscribe{Action: "subscribe", API: "test", Instance: "default", Func: "fast", RateMS: 20}
	subData, _ := json.Marshal(sub)
	conn.Write(ctx, websocket.MessageText, subData)

	// Read one update
	_, _, err = conn.Read(ctx)
	if err != nil {
		t.Fatalf("read first update: %v", err)
	}

	// Unsubscribe
	unsub := wsUnsubscribe{Action: "unsubscribe", API: "test", Instance: "default", Func: "fast"}
	unsubData, _ := json.Marshal(unsub)
	conn.Write(ctx, websocket.MessageText, unsubData)

	// Wait a bit, then try to read — should timeout (no more updates)
	readCtx, readCancel := context.WithTimeout(ctx, 200*time.Millisecond)
	defer readCancel()

	_, _, err = conn.Read(readCtx)
	if err == nil {
		t.Fatalf("expected read to timeout after unsubscribe, but got data")
	}
}

// TestWatchServerIntegration verifies the watch endpoint works on a full Server.
func TestWatchServerIntegration(t *testing.T) {
	registry := NewRegistry()
	s := NewServer(registry, "localhost:0")

	watchReg := NewWatchRegistry()
	watchReg.Register(&WatchAPI{
		APIName:  "demo",
		Instance: "default",
		Watches: []WatchFuncMeta{
			{
				Name:        "get_status",
				DefaultRate: 50 * time.Millisecond,
				Watch: func() (json.RawMessage, error) {
					return json.Marshal(map[string]string{"status": "ok"})
				},
			},
		},
	})
	s.AddWatchEndpoint(watchReg)

	// Start server
	ts := httptest.NewServer(s.Handler())
	defer ts.Close()

	// Connect to the watch endpoint
	wsURL := "ws" + strings.TrimPrefix(ts.URL, "http") + "/api/v1/watch"
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")

	// Subscribe
	sub := wsSubscribe{Action: "subscribe", API: "demo", Instance: "default", Func: "get_status", RateMS: 50}
	subData, _ := json.Marshal(sub)
	conn.Write(ctx, websocket.MessageText, subData)

	// Read one update
	_, data, err := conn.Read(ctx)
	if err != nil {
		t.Fatalf("read: %v", err)
	}

	var update wsUpdate
	json.Unmarshal(data, &update)
	if update.Type != "update" || update.Func != "get_status" {
		t.Fatalf("unexpected update: %+v", update)
	}
}

// TestWatchConcurrentSubscriptions verifies multiple subscriptions on one connection.
func TestWatchConcurrentSubscriptions(t *testing.T) {
	var fastCounter, slowCounter atomic.Int64

	reg := NewWatchRegistry()
	reg.Register(&WatchAPI{
		APIName:  "test",
		Instance: "default",
		Watches: []WatchFuncMeta{
			{
				Name:        "fast",
				DefaultRate: 30 * time.Millisecond,
				Watch: func() (json.RawMessage, error) {
					n := fastCounter.Add(1)
					return json.Marshal(map[string]int64{"seq": n})
				},
			},
			{
				Name:        "slow",
				DefaultRate: 100 * time.Millisecond,
				Watch: func() (json.RawMessage, error) {
					n := slowCounter.Add(1)
					return json.Marshal(map[string]int64{"seq": n})
				},
			},
		},
	})

	handler := NewWatchHandler(reg)
	srv := httptest.NewServer(handler)
	defer srv.Close()

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")

	// Subscribe to both
	for _, fn := range []string{"fast", "slow"} {
		sub := wsSubscribe{Action: "subscribe", API: "test", Instance: "default", Func: fn, RateMS: 30}
		subData, _ := json.Marshal(sub)
		conn.Write(ctx, websocket.MessageText, subData)
	}

	// Collect updates for 800ms
	var mu sync.Mutex
	counts := map[string]int{}

	readCtx, readCancel := context.WithTimeout(ctx, 800*time.Millisecond)
	defer readCancel()

	for {
		_, data, err := conn.Read(readCtx)
		if err != nil {
			break
		}
		var update wsUpdate
		json.Unmarshal(data, &update)
		mu.Lock()
		counts[update.Func]++
		mu.Unlock()
	}

	if counts["fast"] < 2 {
		t.Fatalf("expected at least 2 fast updates, got %d", counts["fast"])
	}
	if counts["slow"] < 1 {
		t.Fatalf("expected at least 1 slow update, got %d", counts["slow"])
	}
	t.Logf("received: fast=%d slow=%d", counts["fast"], counts["slow"])
}

// TestWatchResubscribeNoStaleSnapshot verifies that rapidly re-subscribing to
// the same watch function (as the halshow UI does when items are added to the
// watch list) never delivers a stale first-poll snapshot from a superseded
// subscription AFTER the newest subscription's snapshot.
//
// Regression test: previously the immediate first-poll send in pushLoop did not
// check the subscription's context, so a cancelled goroutine could still emit
// its initial (older, smaller) snapshot, which could arrive after — and clobber
// — the newest snapshot on the client. In the UI this made a newly-added watch
// item appear with no value/type until a page reload.
func TestWatchResubscribeNoStaleSnapshot(t *testing.T) {
	const generations = 8

	reg := NewWatchRegistry()
	reg.Register(&WatchAPI{
		APIName:  "test",
		Instance: "default",
		Watches: []WatchFuncMeta{
			{
				Name: "items",
				// Long rate so the ticker never fires — only first polls matter.
				DefaultRate: 10 * time.Second,
				Factory: func(args json.RawMessage) (WatchFunc, error) {
					var a struct {
						Gen int `json:"gen"`
					}
					_ = json.Unmarshal(args, &a)
					first := true
					return func() (json.RawMessage, error) {
						if first {
							first = false
							// Deterministically drive the race: newer generations
							// wake FIRST, older ones LAST. Without the ctx guard on
							// the first-poll send, the oldest (smallest) snapshot
							// would therefore be written last and clobber the
							// newest on the client.
							time.Sleep(time.Duration(generations-a.Gen) * 40 * time.Millisecond)
						}
						return json.Marshal(map[string]int{"gen": a.Gen})
					}, nil
				},
			},
		},
	})

	handler := NewWatchHandler(reg)
	srv := httptest.NewServer(handler)
	defer srv.Close()

	wsURL := "ws" + strings.TrimPrefix(srv.URL, "http")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, _, err := websocket.Dial(ctx, wsURL, nil)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")

	// Fire generations subscribes back-to-back, each with an increasing gen.
	for gen := 1; gen <= generations; gen++ {
		sub := wsSubscribe{
			Action: "subscribe", API: "test", Instance: "default", Func: "items",
			Args: json.RawMessage(fmt.Sprintf(`{"gen":%d}`, gen)),
		}
		subData, _ := json.Marshal(sub)
		if err := conn.Write(ctx, websocket.MessageText, subData); err != nil {
			t.Fatalf("write subscribe gen=%d: %v", gen, err)
		}
	}

	// Drain all updates until the stream goes quiet. The LAST snapshot that
	// arrives must be the newest generation — a stale one arriving last would
	// clobber the UI.
	lastGen := -1
	for {
		readCtx, readCancel := context.WithTimeout(ctx, 400*time.Millisecond)
		_, data, err := conn.Read(readCtx)
		readCancel()
		if err != nil {
			break // no more updates
		}
		var update wsUpdate
		if err := json.Unmarshal(data, &update); err != nil {
			t.Fatalf("unmarshal update: %v", err)
		}
		var payload struct {
			Gen int `json:"gen"`
		}
		if err := json.Unmarshal(update.Data, &payload); err != nil {
			t.Fatalf("unmarshal payload: %v", err)
		}
		lastGen = payload.Gen
	}

	if lastGen != generations {
		t.Fatalf("last snapshot gen=%d, want %d (stale snapshot from a "+
			"superseded subscription clobbered the newest)", lastGen, generations)
	}
}
