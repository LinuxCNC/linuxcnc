// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package launcher

import (
	"context"
	"os"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/internal/config"
	"github.com/sittner/linuxcnc/src/gomc/internal/halrest"
)

const (
	// defaultRESTAddr is the default listen address for the REST API server.
	// Can be overridden via [GMC]REST_ADDR in the INI file.
	defaultRESTAddr = "127.0.0.1:5080"
)

// resolveRESTAddr returns the REST API listen address, in precedence order:
// the GMC_REST_ADDR environment variable, then [GMC]REST_ADDR in the INI, then
// the compiled default (127.0.0.1:5080).  The env override lets the test
// harness run several gomc-server instances on distinct ports in parallel
// without editing per-config REST_ADDR.
func (l *Launcher) resolveRESTAddr() string {
	if v := os.Getenv("GMC_REST_ADDR"); v != "" {
		return v
	}
	if l.ini != nil {
		if v := l.ini.Get("GMC", "REST_ADDR"); v != "" {
			return v
		}
	}
	return defaultRESTAddr
}

// createAPIServer creates the API server instance and sets it as the
// default. Called early in startup so that stream_server registrations
// from cmod plugins can find it. Does not start listening.
func (l *Launcher) createAPIServer() {
	addr := l.resolveRESTAddr()

	reg := apiserver.DefaultRegistry()
	if reg == nil {
		l.logger.Warn("no API registry available, API server not created")
		return
	}

	l.apiServer = apiserver.NewServer(reg, addr)
	l.apiServer.SetLogger(l.logger)
	apiserver.SetDefaultServer(l.apiServer)
}

// startAPIServer starts the REST API server in the background.
// The listen address is read from [GMC]REST_ADDR in the INI file,
// defaulting to "127.0.0.1:5080".
func (l *Launcher) startAPIServer() {
	if l.apiServer == nil {
		l.createAPIServer()
	}
	if l.apiServer == nil {
		return
	}

	// Add WebSocket watch endpoint if a watch registry is available
	watchReg := apiserver.DefaultWatchRegistry()
	if watchReg == nil {
		watchReg = apiserver.NewWatchRegistry()
		apiserver.SetDefaultWatchRegistry(watchReg)
	}
	l.apiServer.AddWatchEndpoint(watchReg)

	// Register halcmd watch functions (live pin/signal value streaming).
	// [HAL]WATCH_INTERVAL overrides the default 100ms push rate.
	watchInterval := time.Duration(0)
	if l.ini != nil {
		if ms := l.ini.Get("HAL", "WATCH_INTERVAL"); ms != "" {
			if d, err := time.ParseDuration(ms); err == nil {
				watchInterval = d
			}
		}
	}
	halrest.RegisterWatch(watchReg, watchInterval)

	// Serve web applications from share/gomc/webapp/<app>/
	if config.EMC2WebAppDir != "" {
		l.logger.Info("configuring web apps", "dir", config.EMC2WebAppDir)
		l.apiServer.AddWebApps(config.EMC2WebAppDir)
	} else {
		l.logger.Warn("EMC2WebAppDir not set, web apps disabled")
	}

	addr := l.resolveRESTAddr()
	l.apiServer.SetAddr(addr)

	// Capture the server in a local so that a concurrent stopAPIServer() (which
	// nils l.apiServer during shutdown) cannot cause a nil dereference here.
	srv := l.apiServer
	go func() {
		l.logger.Info("starting REST API server", "addr", addr)
		if err := srv.ListenAndServe(); err != nil {
			// http.ErrServerClosed is expected on graceful shutdown
			if err.Error() != "http: Server closed" {
				l.logger.Error("REST API server error", "error", err)
			}
		}
	}()
}

// stopAPIServer gracefully shuts down the REST API server.
func (l *Launcher) stopAPIServer() {
	if l.apiServer == nil {
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	if err := l.apiServer.Shutdown(ctx); err != nil {
		l.logger.Debug("REST API server shutdown error", "error", err)
	}
	l.apiServer = nil
}
