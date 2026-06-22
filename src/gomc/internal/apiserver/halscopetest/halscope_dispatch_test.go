// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package halscopetest

import (
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/halscope"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

// --- Mock implementation of HalscopeCallbacks ---

type mockHalscope struct {
	mu        sync.Mutex
	state     halscope.ScopeState
	recLen    int32
	preTrig   int32
	sampleLen int32
	channels  []halscope.ChannelInfo
	thread    string
}

func newMock() *mockHalscope {
	return &mockHalscope{
		recLen: 4000,
	}
}

func (m *mockHalscope) ListThreads() ([]halscope.ThreadInfo, error) {
	return []halscope.ThreadInfo{
		{Name: "servo-thread", PeriodNs: 1000000},
	}, nil
}

func (m *mockHalscope) Configure(config halscope.CaptureConfig) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.thread = config.ThreadName
	if config.MaxChannels > 0 {
		// Derive recLen from maxChannels (simulating server logic)
		m.recLen = 16000 / config.MaxChannels
	}
	if config.PreTrig > 0 {
		m.preTrig = config.PreTrig
	}
	return 0, nil
}

func (m *mockHalscope) SetChannel(ch halscope.ChannelConfig) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	for i, c := range m.channels {
		if c.Channel == ch.Channel {
			m.channels = append(m.channels[:i], m.channels[i+1:]...)
			break
		}
	}
	m.channels = append(m.channels, halscope.ChannelInfo{
		Channel: ch.Channel,
		PinName: ch.PinName,
		Enabled: true,
	})
	m.sampleLen = int32(len(m.channels))
	return 0, nil
}

func (m *mockHalscope) ClearChannel(channel int32) (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	for i, c := range m.channels {
		if c.Channel == channel {
			m.channels = append(m.channels[:i], m.channels[i+1:]...)
			break
		}
	}
	m.sampleLen = int32(len(m.channels))
	return 0, nil
}

func (m *mockHalscope) SetTrigger(trig halscope.TriggerConfig) (int32, error) {
	return 0, nil
}

func (m *mockHalscope) Arm() (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.state = 1 // ARMED
	return 0, nil
}

func (m *mockHalscope) ForceTrigger() (int32, error) {
	return 0, nil
}

func (m *mockHalscope) SetContinuous(enabled bool) (int32, error) {
	return 0, nil
}

func (m *mockHalscope) Reset() (int32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.state = 0 // IDLE
	return 0, nil
}

func (m *mockHalscope) GetStatus() (*halscope.ScopeStatus, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var channels []halscope.ChannelInfo
	if len(m.channels) > 0 {
		channels = make([]halscope.ChannelInfo, len(m.channels))
		copy(channels, m.channels)
	}
	return &halscope.ScopeStatus{
		State:      m.state,
		RecLen:     m.recLen,
		PreTrig:    m.preTrig,
		SampleLen:  m.sampleLen,
		ThreadName: m.thread,
		Channels:   channels,
	}, nil
}

func (m *mockHalscope) ListPins(pattern string, kind string) ([]string, error) {
	return []string{"joint.0.pos-cmd", "joint.1.pos-cmd", "joint.2.pos-cmd"}, nil
}

// --- Test helpers ---

func setupTestServer(t *testing.T) (*httptest.Server, func()) {
	t.Helper()

	mock := newMock()

	reg := apiserver.NewRegistry()
	apiserver.RegisterMeta(halscope.HalscopeMeta)

	err := halscope.RegisterHalscopeAPI(reg, "halscope", mock)
	if err != nil {
		t.Fatalf("Register failed: %v", err)
	}

	srv := apiserver.NewServer(reg, "")
	ts := httptest.NewServer(srv.Handler())
	return ts, func() {
		ts.Close()
	}
}

func get(t *testing.T, ts *httptest.Server, path string) (int, []byte) {
	t.Helper()
	resp, err := http.Get(ts.URL + "/api/v1/halscope" + path)
	if err != nil {
		t.Fatalf("GET %s: %v", path, err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	return resp.StatusCode, body
}

func post(t *testing.T, ts *httptest.Server, path, jsonBody string) (int, []byte) {
	t.Helper()
	resp, err := http.Post(ts.URL+"/api/v1/halscope"+path, "application/json", strings.NewReader(jsonBody))
	if err != nil {
		t.Fatalf("POST %s: %v", path, err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	return resp.StatusCode, body
}

func delete_(t *testing.T, ts *httptest.Server, path string) (int, []byte) {
	t.Helper()
	req, err := http.NewRequest(http.MethodDelete, ts.URL+"/api/v1/halscope"+path, nil)
	if err != nil {
		t.Fatalf("DELETE %s: %v", path, err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatalf("DELETE %s: %v", path, err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	return resp.StatusCode, body
}

// --- Tests ---

func TestGetStatus_Initial(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, body := get(t, ts, "/status")
	if code != 200 {
		t.Fatalf("expected 200, got %d: %s", code, body)
	}

	var st halscope.ScopeStatus
	if err := json.Unmarshal(body, &st); err != nil {
		t.Fatalf("unmarshal: %v\nbody: %s", err, body)
	}
	if st.State != 0 {
		t.Errorf("expected state=0, got %d", st.State)
	}
	if st.RecLen != 4000 {
		t.Errorf("expected rec_len=4000, got %d", st.RecLen)
	}
	if st.SampleLen != 0 {
		t.Errorf("expected sample_len=0, got %d", st.SampleLen)
	}
	if st.Channels != nil {
		t.Errorf("expected nil channels, got %v", st.Channels)
	}
}

func TestListPins(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, body := get(t, ts, "/pins")
	if code != 200 {
		t.Fatalf("expected 200, got %d: %s", code, body)
	}

	var pins []string
	if err := json.Unmarshal(body, &pins); err != nil {
		t.Fatalf("unmarshal: %v\nbody: %s", err, body)
	}
	if len(pins) != 3 {
		t.Fatalf("expected 3 pins, got %d: %v", len(pins), pins)
	}
	if pins[0] != "joint.0.pos-cmd" {
		t.Errorf("expected joint.0.pos-cmd, got %s", pins[0])
	}
	if pins[2] != "joint.2.pos-cmd" {
		t.Errorf("expected joint.2.pos-cmd, got %s", pins[2])
	}
}

func TestSetChannel(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, body := post(t, ts, "/channel", `{"ch":{"channel":0,"pinName":"joint.2.pos-cmd"}}`)
	if code != 200 {
		t.Fatalf("expected 200, got %d: %s", code, body)
	}

	var result int32
	if err := json.Unmarshal(body, &result); err != nil {
		t.Fatalf("unmarshal: %v\nbody: %s", err, body)
	}
	if result != 0 {
		t.Errorf("expected result=0, got %d", result)
	}

	// Verify status reflects the channel
	code, body = get(t, ts, "/status")
	if code != 200 {
		t.Fatalf("status: expected 200, got %d: %s", code, body)
	}

	var st halscope.ScopeStatus
	if err := json.Unmarshal(body, &st); err != nil {
		t.Fatalf("unmarshal status: %v\nbody: %s", err, body)
	}
	if st.SampleLen != 1 {
		t.Errorf("expected sample_len=1, got %d", st.SampleLen)
	}
	if len(st.Channels) != 1 {
		t.Fatalf("expected 1 channel, got %d", len(st.Channels))
	}
	if st.Channels[0].PinName != "joint.2.pos-cmd" {
		t.Errorf("expected pin_name=joint.2.pos-cmd, got %s", st.Channels[0].PinName)
	}
	if st.Channels[0].Channel != 0 {
		t.Errorf("expected channel=0, got %d", st.Channels[0].Channel)
	}
	if !st.Channels[0].Enabled {
		t.Errorf("expected enabled=true")
	}
}

func TestClearChannel(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	post(t, ts, "/channel", `{"ch":{"channel":0,"pinName":"test.pin"}}`)

	code, body := delete_(t, ts, "/channel/0")
	if code != 200 {
		t.Fatalf("expected 200, got %d: %s", code, body)
	}

	_, body = get(t, ts, "/status")
	var st halscope.ScopeStatus
	json.Unmarshal(body, &st)
	if st.SampleLen != 0 {
		t.Errorf("expected sample_len=0 after clear, got %d", st.SampleLen)
	}
}

func TestArmAndReset(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, body := post(t, ts, "/arm", "")
	if code != 200 {
		t.Fatalf("arm: expected 200, got %d: %s", code, body)
	}

	_, body = get(t, ts, "/status")
	var st halscope.ScopeStatus
	json.Unmarshal(body, &st)
	if st.State != 1 {
		t.Errorf("expected state=1 (ARMED), got %d", st.State)
	}

	code, body = post(t, ts, "/reset", "")
	if code != 200 {
		t.Fatalf("reset: expected 200, got %d: %s", code, body)
	}

	_, body = get(t, ts, "/status")
	json.Unmarshal(body, &st)
	if st.State != 0 {
		t.Errorf("expected state=0 (IDLE) after reset, got %d", st.State)
	}
}

func TestConfigure(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, body := post(t, ts, "/configure",
		`{"config":{"threadName":"servo-thread","maxChannels":2,"samplePeriodMult":1,"preTrig":4000}}`)
	if code != 200 {
		t.Fatalf("expected 200, got %d: %s", code, body)
	}

	_, body = get(t, ts, "/status")
	var st halscope.ScopeStatus
	json.Unmarshal(body, &st)
	if st.RecLen != 8000 {
		t.Errorf("expected rec_len=8000, got %d", st.RecLen)
	}
	if st.PreTrig != 4000 {
		t.Errorf("expected pre_trig=4000, got %d", st.PreTrig)
	}
}

func TestSetTrigger(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, body := post(t, ts, "/trigger",
		`{"trig":{"channel":0,"level":1.5,"edge":1,"force":false,"autoTrig":true}}`)
	if code != 200 {
		t.Fatalf("expected 200, got %d: %s", code, body)
	}

	var result int32
	json.Unmarshal(body, &result)
	if result != 0 {
		t.Errorf("expected result=0, got %d", result)
	}
}

func TestFullCaptureWorkflow(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, _ := post(t, ts, "/configure",
		`{"config":{"threadName":"servo-thread","maxChannels":2,"samplePeriodMult":1,"preTrig":4000}}`)
	if code != 200 {
		t.Fatalf("configure failed: %d", code)
	}

	code, _ = post(t, ts, "/channel", `{"ch":{"channel":0,"pinName":"joint.0.pos-cmd"}}`)
	if code != 200 {
		t.Fatalf("set_channel failed: %d", code)
	}

	code, _ = post(t, ts, "/trigger",
		`{"trig":{"channel":0,"level":0.0,"edge":0,"force":true,"autoTrig":false}}`)
	if code != 200 {
		t.Fatalf("set_trigger failed: %d", code)
	}

	code, _ = post(t, ts, "/arm", "")
	if code != 200 {
		t.Fatalf("arm failed: %d", code)
	}

	_, body := get(t, ts, "/status")
	var st halscope.ScopeStatus
	json.Unmarshal(body, &st)
	if st.State != 1 {
		t.Errorf("expected state=1 (ARMED), got %d", st.State)
	}
	if st.RecLen == 0 {
		t.Errorf("expected non-zero rec_len, got %d", st.RecLen)
	}
	if len(st.Channels) != 1 {
		t.Fatalf("expected 1 channel, got %d", len(st.Channels))
	}
	if st.Channels[0].PinName != "joint.0.pos-cmd" {
		t.Errorf("expected pin_name=joint.0.pos-cmd, got %s", st.Channels[0].PinName)
	}

	code, _ = post(t, ts, "/reset", "")
	if code != 200 {
		t.Fatalf("reset failed: %d", code)
	}

	code, _ = delete_(t, ts, "/channel/0")
	if code != 200 {
		t.Fatalf("clear_channel failed: %d", code)
	}

	_, body = get(t, ts, "/status")
	json.Unmarshal(body, &st)
	if st.State != 0 {
		t.Errorf("expected state=0, got %d", st.State)
	}
	if st.SampleLen != 0 {
		t.Errorf("expected sample_len=0, got %d", st.SampleLen)
	}
}

func TestNotFound(t *testing.T) {
	ts, cleanup := setupTestServer(t)
	defer cleanup()

	code, _ := get(t, ts, "/nonexistent")
	if code != 404 {
		t.Errorf("expected 404, got %d", code)
	}
}
