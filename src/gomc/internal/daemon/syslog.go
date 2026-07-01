// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package daemon

import (
	"context"
	"log/slog"
	"log/syslog"
)

// SyslogHandler is an slog.Handler that writes to syslog.
type SyslogHandler struct {
	writer *syslog.Writer
	level  slog.Leveler
	attrs  []slog.Attr
	groups []string
}

// NewSyslogHandler creates a new slog.Handler that logs to syslog with the
// given facility and tag.
func NewSyslogHandler(level slog.Leveler) (*SyslogHandler, error) {
	w, err := syslog.New(syslog.LOG_DAEMON, "gomc-server")
	if err != nil {
		return nil, err
	}
	return &SyslogHandler{writer: w, level: level}, nil
}

func (h *SyslogHandler) Enabled(_ context.Context, level slog.Level) bool {
	return level >= h.level.Level()
}

func (h *SyslogHandler) Handle(_ context.Context, r slog.Record) error {
	msg := r.Message
	// Append attributes inline.
	r.Attrs(func(a slog.Attr) bool {
		msg += " " + a.Key + "=" + a.Value.String()
		return true
	})
	for _, a := range h.attrs {
		msg += " " + a.Key + "=" + a.Value.String()
	}

	switch {
	case r.Level >= slog.LevelError:
		return h.writer.Err(msg)
	case r.Level >= slog.LevelWarn:
		return h.writer.Warning(msg)
	case r.Level >= slog.LevelInfo:
		return h.writer.Info(msg)
	default:
		return h.writer.Debug(msg)
	}
}

func (h *SyslogHandler) WithAttrs(attrs []slog.Attr) slog.Handler {
	return &SyslogHandler{
		writer: h.writer,
		level:  h.level,
		attrs:  append(h.attrs, attrs...),
		groups: h.groups,
	}
}

func (h *SyslogHandler) WithGroup(name string) slog.Handler {
	return &SyslogHandler{
		writer: h.writer,
		level:  h.level,
		attrs:  h.attrs,
		groups: append(h.groups, name),
	}
}
