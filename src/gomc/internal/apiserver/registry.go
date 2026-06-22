// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package apiserver

import (
	"fmt"
	"sync"
	"syscall"
	"unsafe"
)

// ConsumerRecord tracks who looked up an API.
type ConsumerRecord struct {
	ConsumerInstance string // who called GetAPI
	APIName          string // which API was looked up
	ProviderInstance string // from which provider
}

// OnRegisterFunc is a callback invoked after a new API is registered.
// Called with the lock released — may safely call back into the registry.
type OnRegisterFunc func(api *RegisteredAPI)

// Registry stores registered API instances. Thread-safe for concurrent reads
// after startup. Writes (Register) happen during module init only.
type Registry struct {
	mu        sync.RWMutex
	instances map[string]*RegisteredAPI
	consumers []ConsumerRecord
	listeners []OnRegisterFunc
}

// NewRegistry creates an empty registry.
func NewRegistry() *Registry {
	return &Registry{
		instances: make(map[string]*RegisteredAPI),
	}
}

// registryKey returns a composite key for the registry map.
func registryKey(apiName, instance string) string {
	return apiName + ":" + instance
}

// Register adds an API instance.  Only apiName, version, instance, and
// callbacks are required — all supplied by the C module at runtime.
// If an APIMeta with matching name+version was registered (e.g. via a
// generated Go package init()), it is automatically attached for REST
// dispatch.  Returns EEXIST if the api:instance pair is taken.
func (r *Registry) Register(apiName string, version int, instance string, callbacks unsafe.Pointer) error {
	return r.register(apiName, version, instance, callbacks, true)
}

// RegisterNoREST registers an API instance without attaching REST metadata.
// Used for C module registrations where the callbacks pointer is a C struct,
// not a Go interface — the Go-generated REST dispatch functions cannot use it.
func (r *Registry) RegisterNoREST(apiName string, version int, instance string, callbacks unsafe.Pointer) error {
	return r.register(apiName, version, instance, callbacks, false)
}

func (r *Registry) register(apiName string, version int, instance string, callbacks unsafe.Pointer, attachMeta bool) error {
	if apiName == "" || instance == "" {
		return syscall.EINVAL
	}

	key := registryKey(apiName, instance)

	r.mu.Lock()

	if _, exists := r.instances[key]; exists {
		r.mu.Unlock()
		return syscall.EEXIST
	}

	// Attach REST metadata if available and requested (optional — nil is fine).
	var meta *APIMeta
	if attachMeta {
		meta = GetMeta(apiName, version)
	}

	r.instances[key] = &RegisteredAPI{
		APIName:   apiName,
		Version:   version,
		Meta:      meta,
		Instance:  instance,
		Callbacks: callbacks,
	}

	// Copy listeners under the lock so we can fire them after unlocking.
	listeners := make([]OnRegisterFunc, len(r.listeners))
	copy(listeners, r.listeners)
	api := r.instances[key]
	r.mu.Unlock()

	// Fire OnRegister callbacks outside the lock.
	for _, fn := range listeners {
		fn(api)
	}
	return nil
}

// Upgrade updates an existing registration's Meta and Callbacks.
// Used when a Go module wants to take over REST serving from a C registration.
// Returns ENOENT if the API instance was not previously registered.
func (r *Registry) Upgrade(apiName string, version int, instance string, callbacks unsafe.Pointer, meta *APIMeta) error {
	key := registryKey(apiName, instance)
	r.mu.Lock()
	defer r.mu.Unlock()
	api, exists := r.instances[key]
	if !exists {
		return syscall.ENOENT
	}
	api.Callbacks = callbacks
	api.Meta = meta
	return nil
}

// OnRegister adds a listener that is called after each successful Register.
// Listeners are also called for APIs already registered at the time of this call.
func (r *Registry) OnRegister(fn OnRegisterFunc) {
	r.mu.Lock()
	r.listeners = append(r.listeners, fn)
	// Snapshot existing APIs to notify the new listener.
	existing := make([]*RegisteredAPI, 0, len(r.instances))
	for _, api := range r.instances {
		existing = append(existing, api)
	}
	r.mu.Unlock()

	// Fire for already-registered APIs.
	for _, api := range existing {
		fn(api)
	}
}

// GetAPIUntracked returns the callbacks pointer for direct inter-module calls.
// Returns ENOENT if not found, EINVAL on version mismatch.
func (r *Registry) GetAPIUntracked(apiName string, instance string, requiredVersion int) (unsafe.Pointer, error) {
	key := registryKey(apiName, instance)

	r.mu.RLock()
	defer r.mu.RUnlock()

	api := r.instances[key]
	if api == nil {
		return nil, syscall.ENOENT
	}
	if api.Version != requiredVersion {
		return nil, syscall.EINVAL
	}
	return api.Callbacks, nil
}

// GetAPIFor looks up an API and records the caller as a consumer.
// This is the preferred method for Go modules — it ensures consumer tracking
// is never accidentally omitted.  The consumer name is typically the module's
// instance name.
// Returns ENOENT if not found, EINVAL on version mismatch.
func (r *Registry) GetAPIFor(consumer, apiName, instance string, requiredVersion int) (unsafe.Pointer, error) {
	ptr, err := r.GetAPIUntracked(apiName, instance, requiredVersion)
	if err != nil {
		return nil, err
	}
	r.RecordConsumer(consumer, apiName, instance)
	return ptr, nil
}

// RecordConsumer records that a consumer looked up an API from a provider.
func (r *Registry) RecordConsumer(consumerInstance, apiName, providerInstance string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	// Avoid duplicates.
	for _, c := range r.consumers {
		if c.ConsumerInstance == consumerInstance && c.APIName == apiName && c.ProviderInstance == providerInstance {
			return
		}
	}
	r.consumers = append(r.consumers, ConsumerRecord{
		ConsumerInstance: consumerInstance,
		APIName:          apiName,
		ProviderInstance: providerInstance,
	})
}

// Consumers returns all recorded consumer lookups.
func (r *Registry) Consumers() []ConsumerRecord {
	r.mu.RLock()
	defer r.mu.RUnlock()
	out := make([]ConsumerRecord, len(r.consumers))
	copy(out, r.consumers)
	return out
}

// ConsumersOfProvider returns consumer instance names that depend on APIs
// provided by the given providerInstance.
func (r *Registry) ConsumersOfProvider(providerInstance string) []string {
	r.mu.RLock()
	defer r.mu.RUnlock()
	seen := make(map[string]bool)
	var out []string
	for _, c := range r.consumers {
		if c.ProviderInstance == providerInstance && !seen[c.ConsumerInstance] {
			seen[c.ConsumerInstance] = true
			out = append(out, c.ConsumerInstance)
		}
	}
	return out
}

// UnregisterByInstance removes all API registrations and consumer records
// associated with the given instance name.  Returns the number of APIs removed.
func (r *Registry) UnregisterByInstance(instance string) int {
	r.mu.Lock()
	defer r.mu.Unlock()
	removed := 0
	for key, api := range r.instances {
		if api.Instance == instance {
			delete(r.instances, key)
			removed++
		}
	}
	// Remove consumer records where this instance is the consumer.
	n := 0
	for _, c := range r.consumers {
		if c.ConsumerInstance != instance {
			r.consumers[n] = c
			n++
		}
	}
	r.consumers = r.consumers[:n]
	return removed
}

// Get returns the full RegisteredAPI matching the given instance name, or nil
// if not found.  This performs a linear scan because the internal map is keyed
// by api:instance.  Used by the REST server where the URL path contains only
// the instance name and the map is small.
func (r *Registry) Get(instance string) *RegisteredAPI {
	r.mu.RLock()
	defer r.mu.RUnlock()
	for _, api := range r.instances {
		if api.Instance == instance {
			return api
		}
	}
	return nil
}

// GetAll returns all RegisteredAPIs matching the given instance name.
func (r *Registry) GetAll(instance string) []*RegisteredAPI {
	r.mu.RLock()
	defer r.mu.RUnlock()
	var result []*RegisteredAPI
	for _, api := range r.instances {
		if api.Instance == instance {
			result = append(result, api)
		}
	}
	return result
}

// GetByAPI returns the RegisteredAPI for the given apiName:instance pair, or nil.
func (r *Registry) GetByAPI(apiName, instance string) *RegisteredAPI {
	key := registryKey(apiName, instance)
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.instances[key]
}

// Instances returns all registered instance names (without the api: prefix).
func (r *Registry) Instances() []string {
	r.mu.RLock()
	defer r.mu.RUnlock()

	names := make([]string, 0, len(r.instances))
	for _, api := range r.instances {
		names = append(names, api.Instance)
	}
	return names
}

// All returns all registered APIs.
func (r *Registry) All() []*RegisteredAPI {
	r.mu.RLock()
	defer r.mu.RUnlock()
	result := make([]*RegisteredAPI, 0, len(r.instances))
	for _, api := range r.instances {
		result = append(result, api)
	}
	return result
}

// defaultRegistry is the package-level registry used by cgo-exported register
// functions. Set by the launcher before loading any modules.
var defaultRegistry *Registry
var registryReadyCallbacks []func(*Registry)

// SetDefaultRegistry sets the package-level registry. Must be called before
// any cmod calls Register via cgo export.
func SetDefaultRegistry(r *Registry) {
	defaultRegistry = r
	// Fire any pending ready callbacks.
	for _, fn := range registryReadyCallbacks {
		fn(r)
	}
	registryReadyCallbacks = nil
}

// DefaultRegistry returns the package-level registry.
func DefaultRegistry() *Registry {
	return defaultRegistry
}

// OnDefaultRegistryReady registers a callback that fires when the default
// registry becomes available. If already set, fires immediately.
// Used by generated publish drain hooks in their init().
func OnDefaultRegistryReady(fn func(*Registry)) {
	if defaultRegistry != nil {
		fn(defaultRegistry)
		return
	}
	registryReadyCallbacks = append(registryReadyCallbacks, fn)
}

// defaultWatchRegistry is the package-level watch registry for WebSocket subscriptions.
var defaultWatchRegistry *WatchRegistry

// SetDefaultWatchRegistry sets the package-level watch registry.
func SetDefaultWatchRegistry(r *WatchRegistry) {
	defaultWatchRegistry = r
}

// DefaultWatchRegistry returns the package-level watch registry.
func DefaultWatchRegistry() *WatchRegistry {
	return defaultWatchRegistry
}

// defaultServer is the package-level API server for stream endpoint registration.
var defaultServer *Server

// SetDefaultServer sets the package-level server.
func SetDefaultServer(s *Server) {
	defaultServer = s
}

// DefaultServer returns the package-level server.
func DefaultServer() *Server {
	return defaultServer
}

// ─── Meta Registry ───
//
// APIMeta objects are registered by generated cgo packages at init() time.
// When a C plugin calls env->api->register_api("kins", 1, ...), the generic
// callback looks up the meta here to pair it with the callbacks.

var metaRegistry = map[string]*APIMeta{}

// metaKey returns the lookup key for the meta registry.
func metaKey(name string, version int) string {
	return name + ":" + fmt.Sprintf("%d", version)
}

// RegisterMeta registers an APIMeta for later use by the generic API callbacks.
// Called from generated cgo packages' init() functions.
func RegisterMeta(meta *APIMeta) {
	metaRegistry[metaKey(meta.Name, meta.Version)] = meta
}

// GetMeta looks up a registered APIMeta by name and version.
func GetMeta(name string, version int) *APIMeta {
	return metaRegistry[metaKey(name, version)]
}

// ─── Watch Factory Registry ───
//
// WatchAPIFactory creates a WatchAPI for a given instance from the C callbacks
// pointer.  Generated _ws.go packages register a factory in init() so the
// generic gomc_api_register_cb can wire watch APIs without knowing the concrete
// generated types.

// WatchAPIFactory creates a WatchAPI from an instance name and C callbacks ptr.
type WatchAPIFactory func(instance string, callbacks unsafe.Pointer) *WatchAPI

var watchFactoryRegistry = map[string]WatchAPIFactory{}

// RegisterWatchFactory registers a factory for the given API name.
// Called from generated _ws.go packages' init() functions.
func RegisterWatchFactory(apiName string, f WatchAPIFactory) {
	watchFactoryRegistry[apiName] = f
}

// GetWatchFactory looks up a registered watch factory by API name.
func GetWatchFactory(apiName string) WatchAPIFactory {
	return watchFactoryRegistry[apiName]
}

// ─── Stream Server Factory Registry ───
//
// StreamServerFactory creates a StreamServer for a given instance from C
// callbacks pointer.  Generated stream_server_go packages register a factory
// in init() so gomc_api_register_cb can wire stream servers.

// StreamServerFactory creates a StreamServer from an instance name and C callbacks ptr.
type StreamServerFactory func(instance string, callbacks unsafe.Pointer) StreamServer

var streamFactoryRegistry = map[string]StreamServerFactory{}

// RegisterStreamFactory registers a factory for the given API name.
func RegisterStreamFactory(apiName string, f StreamServerFactory) {
	streamFactoryRegistry[apiName] = f
}

// GetStreamFactory looks up a registered stream factory by API name.
func GetStreamFactory(apiName string) StreamServerFactory {
	return streamFactoryRegistry[apiName]
}
