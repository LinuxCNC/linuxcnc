package task

import (
	"encoding/json"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emccmd"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcerror"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcstat"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tools"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

// registerWatches registers WebSocket watch APIs and commands for milltask.
func (m *milltaskModule) registerWatches(name string) {
	if apiserver.DefaultWatchRegistry() == nil {
		apiserver.SetDefaultWatchRegistry(apiserver.NewWatchRegistry())
	}
	wreg := apiserver.DefaultWatchRegistry()

	// emcstat: get_stat watch + get_positions watch + poslogger commands.
	emcstatCmds := emcstat.EmcstatCommands(m)
	emcstatCmds = append(emcstatCmds,
		apiserver.CommandMeta{Name: "start_logger", Handler: m.cmdStartLogger},
		apiserver.CommandMeta{Name: "stop_logger", Handler: m.cmdStopLogger},
		apiserver.CommandMeta{Name: "clear_logger", Handler: m.cmdClearLogger},
	)
	wreg.Register(&apiserver.WatchAPI{
		APIName:  "emcstat",
		Instance: name,
		Watches: []apiserver.WatchFuncMeta{
			{
				Name:        "get_stat",
				DefaultRate: 50 * time.Millisecond,
				Delta:       true,
				Watch: func() (json.RawMessage, error) {
					result, err := m.GetStat()
					if err != nil {
						return nil, err
					}
					return json.Marshal(result)
				},
			},
			{
				Name:        "get_positions",
				DefaultRate: 100 * time.Millisecond,
				Watch:       m.pollPositions,
			},
		},
		Commands: emcstatCmds,
	})

	// emccmd: all command methods as WS commands.
	wreg.Register(&apiserver.WatchAPI{
		APIName:  "emccmd",
		Instance: name,
		Commands: emccmd.EmccmdCommands(m),
	})

	// messages: shared current message list for UI notifications.
	wreg.Register(&apiserver.WatchAPI{
		APIName:  "messages",
		Instance: name,
		Watches: []apiserver.WatchFuncMeta{
			{
				Name:        "get_list",
				DefaultRate: 100 * time.Millisecond,
				Watch:       m.watchMessageList,
			},
		},
		Commands: []apiserver.CommandMeta{
			{Name: "ack_message", Handler: m.cmdAckMessage},
			{Name: "ack_all", Handler: m.cmdAckAllMessages},
			{Name: "ack_error", Handler: m.cmdAckErrorMessages},
			{Name: "ack_text", Handler: m.cmdAckTextMessages},
			{Name: "ack_display", Handler: m.cmdAckDisplayMessages},
			{Name: "publish", Handler: m.cmdPublishMessage},
		},
	})
}

// registerTools registers the tools API (called from Start when INI is loaded).
func (m *milltaskModule) registerTools() {
	// Store client for package-level getToolByPocket.
	pkgTTClient = m.ttClient

	reg := apiserver.DefaultRegistry()
	if reg != nil {
		tools.RegisterToolsAPI(reg, m.name, &toolsImpl{
			module: m,
		})
	}
}

// Position logger WS command handlers.

func (m *milltaskModule) cmdStartLogger(req json.RawMessage) (json.RawMessage, error) {
	var args struct {
		IntervalUS int `json:"interval_us"`
	}
	if req != nil {
		json.Unmarshal(req, &args)
	}
	m.poslog.startLogger(m, args.IntervalUS)
	return json.RawMessage(`{"ok":true}`), nil
}

func (m *milltaskModule) cmdStopLogger(req json.RawMessage) (json.RawMessage, error) {
	m.poslog.stopLogger()
	return json.RawMessage(`{"ok":true}`), nil
}

func (m *milltaskModule) cmdClearLogger(req json.RawMessage) (json.RawMessage, error) {
	m.poslog.clearLogger()
	return json.RawMessage(`{"ok":true}`), nil
}

// --- Message list watch + commands ---

func (m *milltaskModule) watchMessageList() (json.RawMessage, error) {
	if m.task == nil {
		return json.RawMessage(`[]`), nil
	}
	msgs := m.task.messageListSnapshot()
	if msgs == nil {
		return json.RawMessage(`[]`), nil
	}
	return json.Marshal(msgs)
}

func (m *milltaskModule) cmdAckMessage(req json.RawMessage) (json.RawMessage, error) {
	if m.task == nil {
		return json.RawMessage(`{"ok":false}`), nil
	}
	var args struct {
		ID uint64 `json:"id"`
	}
	if req != nil {
		_ = json.Unmarshal(req, &args)
	}
	ok := m.task.ackMessageByID(args.ID)
	return json.Marshal(map[string]bool{"ok": ok})
}

func (m *milltaskModule) cmdAckAllMessages(req json.RawMessage) (json.RawMessage, error) {
	if m.task == nil {
		return json.RawMessage(`{"removed":0}`), nil
	}
	removed := m.task.ackAllMessages()
	return json.Marshal(map[string]int{"removed": removed})
}

func (m *milltaskModule) cmdAckErrorMessages(req json.RawMessage) (json.RawMessage, error) {
	if m.task == nil {
		return json.RawMessage(`{"removed":0}`), nil
	}
	removed := m.task.ackMessagesByKinds(emcerror.ErrorKind_NML_ERROR, emcerror.ErrorKind_OPERATOR_ERROR)
	return json.Marshal(map[string]int{"removed": removed})
}

func (m *milltaskModule) cmdAckTextMessages(req json.RawMessage) (json.RawMessage, error) {
	if m.task == nil {
		return json.RawMessage(`{"removed":0}`), nil
	}
	removed := m.task.ackMessagesByKinds(emcerror.ErrorKind_NML_TEXT, emcerror.ErrorKind_OPERATOR_TEXT)
	return json.Marshal(map[string]int{"removed": removed})
}

func (m *milltaskModule) cmdAckDisplayMessages(req json.RawMessage) (json.RawMessage, error) {
	if m.task == nil {
		return json.RawMessage(`{"removed":0}`), nil
	}
	removed := m.task.ackMessagesByKinds(emcerror.ErrorKind_NML_DISPLAY, emcerror.ErrorKind_OPERATOR_DISPLAY)
	return json.Marshal(map[string]int{"removed": removed})
}

func (m *milltaskModule) cmdPublishMessage(req json.RawMessage) (json.RawMessage, error) {
	if m.task == nil {
		return json.RawMessage(`{"id":0}`), nil
	}
	var args struct {
		Kind int32  `json:"kind"`
		Text string `json:"text"`
	}
	if req != nil {
		_ = json.Unmarshal(req, &args)
	}
	if args.Kind == 0 {
		args.Kind = int32(emcerror.ErrorKind_OPERATOR_TEXT)
	}
	// Publish to both the drain (for /errors watchers) and the message list.
	if m.task.errors != nil {
		switch emcerror.ErrorKind(args.Kind) {
		case emcerror.ErrorKind_NML_ERROR, emcerror.ErrorKind_OPERATOR_ERROR:
			m.task.errors.OperatorError(args.Text)
		case emcerror.ErrorKind_NML_DISPLAY, emcerror.ErrorKind_OPERATOR_DISPLAY:
			m.task.errors.OperatorDisplay(args.Text)
		default:
			m.task.errors.OperatorText(args.Text)
		}
	} else {
		m.task.appendMessage(emcerror.ErrorKind(args.Kind), args.Text)
	}
	return json.RawMessage(`{"ok":true}`), nil
}
