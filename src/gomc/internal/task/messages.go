package task

import "github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcerror"

// TaskMessage is one entry in the server-side current message list.
// This list is independent from the /errors drain queue. Clients watch
// the full list and acknowledge individual messages to remove them.
type TaskMessage struct {
	ID   uint64 `json:"id"`
	Kind int32  `json:"kind"`
	Text string `json:"text"`
}

// appendMessage adds a message to the current list and returns its ID.
func (t *Task) appendMessage(kind emcerror.ErrorKind, text string) uint64 {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.nextMessageID++
	id := t.nextMessageID
	t.messageList = append(t.messageList, TaskMessage{
		ID:   id,
		Kind: int32(kind),
		Text: text,
	})
	return id
}

// messageListSnapshot returns a copy of the current message list.
func (t *Task) messageListSnapshot() []TaskMessage {
	t.mu.Lock()
	defer t.mu.Unlock()
	if len(t.messageList) == 0 {
		return nil
	}
	out := make([]TaskMessage, len(t.messageList))
	copy(out, t.messageList)
	return out
}

// ackMessageByID removes a single message by ID. Returns true if found.
func (t *Task) ackMessageByID(id uint64) bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	for i := range t.messageList {
		if t.messageList[i].ID == id {
			t.messageList = append(t.messageList[:i], t.messageList[i+1:]...)
			return true
		}
	}
	return false
}

// ackAllMessages removes all messages. Returns count removed.
func (t *Task) ackAllMessages() int {
	t.mu.Lock()
	defer t.mu.Unlock()
	n := len(t.messageList)
	t.messageList = t.messageList[:0]
	return n
}

// ackMessagesByKinds removes all messages matching any of the given kinds.
func (t *Task) ackMessagesByKinds(kinds ...emcerror.ErrorKind) int {
	t.mu.Lock()
	defer t.mu.Unlock()
	if len(t.messageList) == 0 || len(kinds) == 0 {
		return 0
	}
	match := make(map[int32]struct{}, len(kinds))
	for _, k := range kinds {
		match[int32(k)] = struct{}{}
	}
	kept := t.messageList[:0]
	removed := 0
	for _, msg := range t.messageList {
		if _, ok := match[msg.Kind]; ok {
			removed++
		} else {
			kept = append(kept, msg)
		}
	}
	t.messageList = kept
	return removed
}

// messageFlags returns booleans indicating which message categories are present.
func (t *Task) messageFlags() (hasAny, hasError, hasText, hasDisplay bool) {
	t.mu.Lock()
	defer t.mu.Unlock()
	for _, msg := range t.messageList {
		switch emcerror.ErrorKind(msg.Kind) {
		case emcerror.ErrorKind_NML_ERROR, emcerror.ErrorKind_OPERATOR_ERROR:
			hasError = true
		case emcerror.ErrorKind_NML_TEXT, emcerror.ErrorKind_OPERATOR_TEXT:
			hasText = true
		case emcerror.ErrorKind_NML_DISPLAY, emcerror.ErrorKind_OPERATOR_DISPLAY:
			hasDisplay = true
		}
	}
	hasAny = len(t.messageList) > 0
	return
}
