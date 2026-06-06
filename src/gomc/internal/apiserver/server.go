package apiserver

import (
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/http/pprof"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

// Server is the REST API server that dispatches to registered APIs.
type Server struct {
	registry     *Registry
	mux          *http.ServeMux
	server       *http.Server
	prefix       string // e.g. "/api/v1"
	logger       *slog.Logger
	watchHandler *WatchHandler

	streamMu    sync.Mutex
	streamConns map[*streamConn]struct{}
	streamWg    sync.WaitGroup
}

// NewServer creates a new API server bound to the given registry.
// addr is the listen address (e.g. "localhost:8080").
func NewServer(registry *Registry, addr string) *Server {
	s := &Server{
		registry: registry,
		mux:      http.NewServeMux(),
		prefix:   "/api/v1",
		logger:   slog.Default(),
	}

	s.mux.HandleFunc(s.prefix+"/", s.handleAPIRequest)
	s.mux.HandleFunc(s.prefix+"/_registry", s.handleRegistryRequest)

	// pprof profiling endpoints — always available for diagnostics.
	s.mux.HandleFunc("/debug/pprof/", pprof.Index)
	s.mux.HandleFunc("/debug/pprof/cmdline", pprof.Cmdline)
	s.mux.HandleFunc("/debug/pprof/profile", pprof.Profile)
	s.mux.HandleFunc("/debug/pprof/symbol", pprof.Symbol)
	s.mux.HandleFunc("/debug/pprof/trace", pprof.Trace)

	s.server = &http.Server{
		Addr:    addr,
		Handler: s.mux,
	}

	return s
}

// SetAddr updates the listen address before ListenAndServe is called.
func (s *Server) SetAddr(addr string) {
	s.server.Addr = addr
}

// ListenAndServe starts the HTTP server. Blocks until the server stops.
func (s *Server) ListenAndServe() error {
	return s.server.ListenAndServe()
}

// Serve accepts connections on the given listener. Useful for tests.
func (s *Server) Serve(ln net.Listener) error {
	return s.server.Serve(ln)
}

// Shutdown gracefully shuts down the server.
func (s *Server) Shutdown(ctx context.Context) error {
	// Close all active WebSocket connections first so their goroutines
	// exit before the HTTP server finishes draining.
	if s.watchHandler != nil {
		s.watchHandler.Close()
	}
	// Close all active stream WebSocket connections.
	s.streamMu.Lock()
	for sc := range s.streamConns {
		sc.cancel()
	}
	s.streamMu.Unlock()
	// Wait for all ServeConn goroutines to exit so no CGO calls are
	// in-flight when modules are destroyed.
	s.streamWg.Wait()
	return s.server.Shutdown(ctx)
}

// Handler returns the http.Handler for use with httptest.
func (s *Server) Handler() http.Handler {
	return s.mux
}

// SetLogger sets the logger for request and error logging.
func (s *Server) SetLogger(logger *slog.Logger) {
	s.logger = logger
}

// RegisterStream registers a stream_server endpoint.
// Connections to {prefix}/stream/{apiName}/{instanceName} will be handled by the server.
func (s *Server) RegisterStream(apiName, instanceName string, server StreamServer) {
	path := s.prefix + "/stream/" + apiName + "/" + instanceName
	s.mux.HandleFunc(path, func(w http.ResponseWriter, r *http.Request) {
		s.handleStreamUpgrade(w, r, server)
	})
	s.logger.Info("registered stream server", "api", apiName, "instance", instanceName, "path", path)
}

// handleAPIRequest is the generic REST dispatcher.
// URL format: /api/v1/{instance}/{func-path...}
func (s *Server) handleAPIRequest(w http.ResponseWriter, r *http.Request) {
	start := time.Now()
	// Strip prefix: "/api/v1/hal0/pin/axis.0" → "hal0/pin/axis.0"
	path := strings.TrimPrefix(r.URL.Path, s.prefix+"/")
	if path == "" {
		writeErrorJSON(w, http.StatusNotFound, "missing instance name")
		s.logger.Debug("api request", "method", r.Method, "path", r.URL.Path, "status", 404, "dur", time.Since(start))
		return
	}

	// Split into instance + remaining path
	instance, funcPath, _ := strings.Cut(path, "/")
	funcPath = "/" + funcPath // normalize: "" → "/", "pin/x" → "/pin/x"

	// Look up registered API(s)
	apis := s.registry.GetAll(instance)
	if len(apis) == 0 {
		writeErrorJSON(w, http.StatusNotFound, "unknown API instance: "+instance)
		return
	}

	// Try each registered API for a function match.
	// Prefer exact (literal) path matches over wildcard matches so that
	// e.g. GET /stat resolves to emcstat's "/stat" rather than tools' "/{toolno}".
	var api *RegisteredAPI
	var funcIndex int
	var wildcardAPI *RegisteredAPI
	var wildcardIdx int
	for _, candidate := range apis {
		if candidate.Meta == nil || !candidate.Meta.RESTExport {
			continue
		}
		idx := matchFunc(candidate.Meta, r.Method, funcPath)
		if idx >= 0 {
			if isLiteralPath(candidate.Meta.Funcs[idx].Path) {
				api = candidate
				funcIndex = idx
				break
			}
			if wildcardAPI == nil {
				wildcardAPI = candidate
				wildcardIdx = idx
			}
		}
	}
	if api == nil {
		api = wildcardAPI
		funcIndex = wildcardIdx
	}
	if api == nil {
		writeErrorJSON(w, http.StatusNotFound, "no matching function")
		return
	}

	fn := &api.Meta.Funcs[funcIndex]
	if fn.Dispatch == nil {
		writeErrorJSON(w, http.StatusNotImplemented, "function not dispatachable: "+fn.Name)
		return
	}

	// Read request body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		writeErrorJSON(w, http.StatusBadRequest, "failed to read request body")
		return
	}

	// For GET/DELETE, encode path+query params as JSON if no body provided
	if len(body) == 0 && (r.Method == http.MethodGet || r.Method == http.MethodDelete) {
		body = encodeParams(fn.Path, funcPath, r.URL.Query())
	}

	// For POST/PUT, merge path params into the JSON body so that
	// dispatch functions can access e.g. /thread/{thread}/function
	// params via the same getField() as body params.
	if r.Method == http.MethodPost || r.Method == http.MethodPut {
		pathParams := extractPathParams(fn.Path, funcPath)
		if len(pathParams) > 0 {
			body = mergePathParamsIntoBody(pathParams, body)
		}
	}

	// Dispatch
	if api.Callbacks == nil {
		writeErrorJSON(w, http.StatusServiceUnavailable, "API not yet initialized: "+api.APIName)
		return
	}
	resp, err := fn.Dispatch(api.Callbacks, body)
	if err != nil {
		writeDispatchError(w, err)
		s.logger.Debug("api request", "method", r.Method, "path", r.URL.Path, "status", 500, "dur", time.Since(start))
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	w.Write(resp)
	s.logger.Debug("api request", "method", r.Method, "path", r.URL.Path, "status", 200, "dur", time.Since(start))
}

// matchFunc finds the FuncMeta index matching the given HTTP method and path.
// Returns -1 if no match found.
func matchFunc(meta *APIMeta, method, requestPath string) int {
	for i := range meta.Funcs {
		f := &meta.Funcs[i]
		if f.Method == "" || f.Path == "" {
			continue // not REST-exported
		}
		if f.Method != method {
			continue
		}
		if matchPath(f.Path, requestPath) {
			return i
		}
	}
	return -1
}

// matchPath matches a pattern like "/pin/{name}" against a request path like "/pin/axis.0".
// Supports {param} wildcards that match exactly one path segment,
// and {param...} that matches the rest of the path.
func matchPath(pattern, requestPath string) bool {
	patParts := strings.Split(strings.Trim(pattern, "/"), "/")
	reqParts := strings.Split(strings.Trim(requestPath, "/"), "/")

	for i, pat := range patParts {
		// Wildcard rest: {name...} matches remaining segments
		if strings.HasPrefix(pat, "{") && strings.HasSuffix(pat, "...}") {
			return true // matches everything from here
		}
		if i >= len(reqParts) {
			return false // request path too short
		}
		// Wildcard segment: {name} matches one segment
		if strings.HasPrefix(pat, "{") && strings.HasSuffix(pat, "}") {
			continue // matches any single segment
		}
		// Literal match
		if pat != reqParts[i] {
			return false
		}
	}

	return len(patParts) == len(reqParts)
}

// isLiteralPath returns true if the path pattern contains no {param} wildcards.
func isLiteralPath(pattern string) bool {
	return !strings.Contains(pattern, "{")
}

// extractPathParams extracts named parameters from a pattern and request path.
func extractPathParams(pattern, requestPath string) map[string]string {
	params := make(map[string]string)
	patParts := strings.Split(strings.Trim(pattern, "/"), "/")
	reqParts := strings.Split(strings.Trim(requestPath, "/"), "/")

	for i, pat := range patParts {
		if strings.HasPrefix(pat, "{") && strings.HasSuffix(pat, "...}") {
			name := pat[1 : len(pat)-4]
			params[name] = strings.Join(reqParts[i:], "/")
			break
		}
		if i >= len(reqParts) {
			break
		}
		if strings.HasPrefix(pat, "{") && strings.HasSuffix(pat, "}") {
			name := pat[1 : len(pat)-1]
			params[name] = reqParts[i]
		}
	}
	return params
}

// mergePathParamsIntoBody merges URL path parameters into a JSON body.
// Path params do not overwrite existing body keys.
func mergePathParamsIntoBody(pathParams map[string]string, body []byte) []byte {
	var m map[string]interface{}
	if len(body) > 0 {
		if err := json.Unmarshal(body, &m); err != nil {
			return body // leave as-is if body isn't valid JSON
		}
	} else {
		m = make(map[string]interface{})
	}
	for k, v := range pathParams {
		if _, exists := m[k]; !exists {
			// Convert numeric strings so json.Unmarshal into int/float fields works.
			if n, err := strconv.ParseInt(v, 10, 64); err == nil {
				m[k] = n
			} else if f, err := strconv.ParseFloat(v, 64); err == nil {
				m[k] = f
			} else {
				m[k] = v
			}
		}
	}
	data, _ := json.Marshal(m)
	return data
}

// encodeParams builds a JSON object from path parameters and query string.
func encodeParams(pattern, requestPath string, query map[string][]string) []byte {
	params := extractPathParams(pattern, requestPath)

	// Add query parameters (first value only)
	for k, v := range query {
		if _, exists := params[k]; !exists && len(v) > 0 {
			params[k] = v[0]
		}
	}

	if len(params) == 0 {
		return nil
	}

	// Build a map[string]interface{} so numeric strings are encoded as
	// JSON numbers.  This allows generated dispatch functions to unmarshal
	// path/query params like {channel} = "0" into int32 fields.
	typed := make(map[string]interface{}, len(params))
	for k, v := range params {
		if n, err := strconv.ParseInt(v, 10, 64); err == nil {
			typed[k] = n
		} else if f, err := strconv.ParseFloat(v, 64); err == nil {
			typed[k] = f
		} else if v == "true" {
			typed[k] = true
		} else if v == "false" {
			typed[k] = false
		} else {
			typed[k] = v
		}
	}

	data, _ := json.Marshal(typed)
	return data
}

// apiError is the JSON error response format.
type apiError struct {
	Error string `json:"error"`
	Code  int    `json:"code"`
}

func writeErrorJSON(w http.ResponseWriter, code int, msg string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(apiError{Error: msg, Code: code})
}

func writeDispatchError(w http.ResponseWriter, err error) {
	// Map errno to HTTP status
	code := http.StatusInternalServerError
	switch err {
	case syscall.EINVAL:
		code = http.StatusBadRequest
	case syscall.ENOENT:
		code = http.StatusNotFound
	case syscall.EPERM:
		code = http.StatusForbidden
	case syscall.EEXIST:
		code = http.StatusConflict
	case syscall.ENOSYS:
		code = http.StatusNotImplemented
	case syscall.EBUSY:
		code = http.StatusConflict
	case syscall.ERANGE:
		code = http.StatusBadRequest
	}
	writeErrorJSON(w, code, err.Error())
}

// --- Registry introspection endpoint ---

// registryAPIInfo is the JSON representation of a registered API.
type registryAPIInfo struct {
	APIName   string              `json:"api_name"`
	Instance  string              `json:"instance"`
	Version   int                 `json:"version"`
	REST      bool                `json:"rest"`
	Functions []registryFuncInfo  `json:"functions,omitempty"`
	Watches   []registryWatchInfo `json:"watches,omitempty"`
	Commands  []string            `json:"commands,omitempty"`
	Consumers []string            `json:"consumers,omitempty"`
}

type registryFuncInfo struct {
	Name   string `json:"name"`
	Method string `json:"method,omitempty"`
	Path   string `json:"path,omitempty"`
}

type registryWatchInfo struct {
	Name        string `json:"name"`
	DefaultRate int    `json:"default_rate_ms"`
}

func (s *Server) handleRegistryRequest(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErrorJSON(w, http.StatusMethodNotAllowed, "GET only")
		return
	}

	apis := s.registry.All()
	consumers := s.registry.Consumers()

	// Build consumer map: apiName:providerInstance → []consumerInstance
	consumerMap := make(map[string][]string)
	for _, c := range consumers {
		key := c.APIName + ":" + c.ProviderInstance
		consumerMap[key] = append(consumerMap[key], c.ConsumerInstance)
	}

	// Build watch info map
	var watchApis []*WatchAPI
	if DefaultWatchRegistry() != nil {
		watchApis = DefaultWatchRegistry().All()
	}
	watchMap := make(map[string]*WatchAPI) // key: apiName+"/"+instance
	for _, wa := range watchApis {
		watchMap[wa.APIName+"/"+wa.Instance] = wa
	}

	result := make([]registryAPIInfo, 0, len(apis))
	for _, api := range apis {
		info := registryAPIInfo{
			APIName:  api.APIName,
			Instance: api.Instance,
			Version:  api.Version,
			REST:     api.Meta != nil && api.Meta.RESTExport,
		}

		// Functions from APIMeta
		if api.Meta != nil {
			for _, fn := range api.Meta.Funcs {
				info.Functions = append(info.Functions, registryFuncInfo{
					Name:   fn.Name,
					Method: fn.Method,
					Path:   fn.Path,
				})
			}
		}

		// Watch/commands from WatchRegistry
		wa := watchMap[api.APIName+"/"+api.Instance]
		if wa != nil {
			for _, w := range wa.Watches {
				info.Watches = append(info.Watches, registryWatchInfo{
					Name:        w.Name,
					DefaultRate: int(w.DefaultRate / time.Millisecond),
				})
			}
			for _, cmd := range wa.Commands {
				info.Commands = append(info.Commands, cmd.Name)
			}
		}

		// Consumers
		key := api.APIName + ":" + api.Instance
		if cs, ok := consumerMap[key]; ok {
			info.Consumers = cs
		}

		result = append(result, info)
	}

	// Include watch-only entries (watches without a matching registry API)
	matched := make(map[string]bool)
	for _, api := range apis {
		matched[api.APIName+"/"+api.Instance] = true
	}
	for key, wa := range watchMap {
		if matched[key] {
			continue
		}
		info := registryAPIInfo{
			APIName:  wa.APIName,
			Instance: wa.Instance,
		}
		for _, w := range wa.Watches {
			info.Watches = append(info.Watches, registryWatchInfo{
				Name:        w.Name,
				DefaultRate: int(w.DefaultRate / time.Millisecond),
			})
		}
		for _, cmd := range wa.Commands {
			info.Commands = append(info.Commands, cmd.Name)
		}
		result = append(result, info)
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(result)
}
