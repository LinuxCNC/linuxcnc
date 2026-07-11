// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// halrun.go implements a one-shot HAL-file execution mode for gomc-server,
// the replacement for the classic standalone `halrun -f file.hal`.
//
// Unlike Run() (the INI-driven server), RunHalFile executes a bare HAL file
// line-by-line — with the same sequential semantics the old halcmd interpreter
// provided (loadrt/load, newthread, wiring, start, stop) — and then tears
// everything down at end-of-file.  There is no INI, no task, and no motion
// controller; only the HAL/RTAPI environment plus the REST/WebSocket API server
// (so external clients such as halcmd/halsampler/halstreamer can connect while
// the file is being executed, matching the gomc server+client model).
//
// NOTE: the classic `loadusr`/`waitusr` commands (which spawned userspace
// helper processes) are intentionally NOT supported.  In gomc every formerly
// userspace component is a cmod/gomod loaded via load/loadrt, and streaming
// clients (halsampler/halstreamer) connect over the REST/WebSocket API from a
// shell driver rather than being forked from inside a HAL file.
package launcher

import (
	"bufio"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"unsafe"

	hal "github.com/sittner/linuxcnc/src/gomc/pkg/hal"

	halcmd "github.com/sittner/linuxcnc/src/gomc/internal/halcmd"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/internal/halfile"
	halparse "github.com/sittner/linuxcnc/src/gomc/internal/halparse"
	"github.com/sittner/linuxcnc/src/gomc/internal/halrest"
	"github.com/sittner/linuxcnc/src/gomc/internal/realtime"
)

// RunHalFile executes a single HAL file in one-shot mode and then shuts down.
// It mirrors the classic `halrun -f <file>` behaviour: initialise the HAL/RTAPI
// environment, execute the file's commands sequentially, then tear everything
// down.  The process exit status reflects whether every command succeeded.
func (l *Launcher) RunHalFile(halFile string) (runErr error) {
	// Initialize the API registry so cmod plugins can register/lookup APIs
	// (e.g. sampler/streamer stream endpoints).
	apiserver.SetDefaultRegistry(apiserver.NewRegistry())
	l.createAPIServer()

	if err := halrest.Register(apiserver.DefaultRegistry()); err != nil {
		l.logger.Warn("failed to register halcmd REST API", "error", err)
	}
	halrest.SetLoadModuleFunc(l.runtimeLoadModule)
	halrest.SetUnloadModuleFunc(l.UnloadModule)

	l.initHalibPath()

	// Single deferred cleanup (idempotent) runs the ordered shutdown.
	defer func() {
		if runErr != nil {
			l.logger.Error("halrun failed", "error", runErr)
		}
		l.cleanup()
	}()

	// Trap SIGINT/SIGTERM for an ordered shutdown.
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		sig := <-sigCh
		l.logger.Info("received signal, shutting down", "signal", sig)
		l.shutdown()
	}()

	// Bring up the realtime + HAL environment (subset of Run(), no INI).
	l.rtMgr = realtime.New(l.logger)
	l.logger.Info("starting realtime environment")
	if err := l.rtMgr.Start(); err != nil {
		return fmt.Errorf("realtime start failed: %w", err)
	}

	l.ensureLogRing()
	halcmd.SetLogRing(unsafe.Pointer(l.logRing.ring))
	l.logger.Info("initializing RTAPI app (in-process)")
	if err := halcmd.RtapiAppInit(); err != nil {
		return fmt.Errorf("rtapi app init: %w", err)
	}

	halComp, err := hal.NewComponent("halcmd")
	if err != nil {
		return fmt.Errorf("hal init: %w", err)
	}
	l.halComp = halComp
	if err := halComp.Ready(); err != nil {
		return fmt.Errorf("hal ready: %w", err)
	}

	if err := halcmd.InitCPUPool(l.logger); err != nil {
		return fmt.Errorf("initializing CPU pool: %w", err)
	}

	// NOTE: one-shot mode does NOT start the REST/WebSocket API server.  The
	// server is only useful for external clients (halsampler/halstreamer/
	// halcmd), which require a persistent server to connect to; a one-shot
	// `-f` run executes and exits too quickly for that.  Streaming/capture
	// tests must instead run gomc-server as a resident server (INI or a future
	// resident HAL mode) and drive it with shell clients over the API.

	// Execute the HAL file sequentially.
	if err := l.halrunExecuteFile(halFile); err != nil {
		return err
	}

	// End of file — deferred cleanup runs the ordered shutdown.
	return nil
}

// halrunExecuteFile reads halFile and dispatches each logical command line.
func (l *Launcher) halrunExecuteFile(halFile string) error {
	f, err := os.Open(halFile)
	if err != nil {
		return fmt.Errorf("opening HAL file: %w", err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	lineNum := 0
	var continued strings.Builder

	for scanner.Scan() {
		lineNum++
		line := strings.TrimSpace(scanner.Text())

		// Backslash line continuation.
		if strings.HasSuffix(line, "\\") {
			continued.WriteString(strings.TrimSuffix(line, "\\"))
			continued.WriteByte(' ')
			continue
		}
		if continued.Len() > 0 {
			continued.WriteString(line)
			line = continued.String()
			continued.Reset()
		}

		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		if err := l.halrunDispatch(line); err != nil {
			return fmt.Errorf("%s:%d: %w", halFile, lineNum, err)
		}
		if l.shutdownRequested() {
			return fmt.Errorf("interrupted")
		}
	}
	return scanner.Err()
}

// halrunDispatch routes a single logical command line to the right handler.
// Load/unload is handled here; everything else is parsed and executed
// in-process by the halparse executor.  loadusr/waitusr are rejected.
func (l *Launcher) halrunDispatch(line string) error {
	args := parseHalArgs(line)
	if len(args) == 0 {
		return nil
	}
	verb := strings.ToLower(args[0])

	switch verb {
	case "setexact_for_test_suite_only":
		return halcmd.SetExact()

	case "loadrt", "load":
		// Normalize "loadrt X ..." to "load X ..." so the halparse parser
		// (which only knows "load") produces a LoadToken; the cmod-vs-Go
		// decision is made by loadModuleNamed.
		norm := line
		if verb == "loadrt" {
			norm = "load" + line[len("loadrt"):]
		}
		return l.halrunLoad(norm)

	case "loadusr", "waitusr":
		return fmt.Errorf("%s is not supported: gomc has no userspace HAL "+
			"components; load components with load/loadrt and drive streaming "+
			"clients (halsampler/halstreamer) from a shell driver over the REST API", verb)

	case "unloadrt", "unloadusr", "unload":
		return l.halrunUnload(args[1:])

	default:
		return l.halrunExecCmd(line)
	}
}

// halrunResolver returns a halparse.PathResolver-capable Executor suitable for
// one-shot execution (relative paths resolve against the cwd).
func (l *Launcher) halrunResolver() *halfile.Executor {
	return halfile.New(nil, l.halibPath, l.logger, "")
}

// halrunLoad parses a (normalized) load line and loads each described module
// immediately (load + Init + Start).
func (l *Launcher) halrunLoad(line string) error {
	sp := halparse.NewSingleFileParser(nil, l.halrunResolver())
	res, err := sp.ParseContent("<halrun>", line)
	if err != nil {
		return err
	}
	return res.IterLoads(func(path, name string, args []string) error {
		return l.loadModuleNamed(path, name, args)
	})
}

// halrunExecCmd parses and executes a single non-load HAL command in-process
// (net, addf, setp, sets, newthread, start, stop, show, list, alias, ...).
func (l *Launcher) halrunExecCmd(line string) error {
	sp := halparse.NewSingleFileParser(nil, l.halrunResolver())
	res, err := sp.ParseContent("<halrun>", line)
	if err != nil {
		return err
	}
	return res.Execute()
}

// halrunUnload unloads one or more modules by instance name.
func (l *Launcher) halrunUnload(names []string) error {
	for _, n := range names {
		if err := l.UnloadModule(n); err != nil {
			return err
		}
	}
	return nil
}

// shutdownRequested reports whether an ordered shutdown has been signalled.
func (l *Launcher) shutdownRequested() bool {
	select {
	case <-l.shutdownCh:
		return true
	default:
		return false
	}
}

// parseHalArgs splits a command line into tokens, honouring quotes and treating
// an unquoted '#' as the start of a comment.  Direction arrows (=>, <=, <=>)
// are preserved (halparse strips them itself).
func parseHalArgs(line string) []string {
	var args []string
	var cur strings.Builder
	inQuote := false
	var qc rune
	for _, r := range line {
		switch {
		case r == '"' || r == '\'':
			if !inQuote {
				inQuote = true
				qc = r
			} else if r == qc {
				inQuote = false
			} else {
				cur.WriteRune(r)
			}
		case (r == ' ' || r == '\t') && !inQuote:
			if cur.Len() > 0 {
				args = append(args, cur.String())
				cur.Reset()
			}
		case r == '#' && !inQuote:
			if cur.Len() > 0 {
				args = append(args, cur.String())
			}
			return args
		default:
			cur.WriteRune(r)
		}
	}
	if cur.Len() > 0 {
		args = append(args, cur.String())
	}
	return args
}
