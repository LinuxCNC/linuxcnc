/*
    hal.hh - C++ interface for HAL

    A thin, type-safe C++ layer on top of the public HAL C API.
    All pin/param access goes through the typed hal_get_X and hal_set_X
    accessors and the user-land query API. No direct shared memory
    access, no hal_priv.h, no re-implemented library internals.

    Header-only, no runtime overhead: typed access expands to the same
    inline accessor calls as the C API.

    The by-name query/set section requires the HAL query API and is
    compiled only when the public header defines the COMPONENT_TYPE_*
    macros. The typed component/pin layer only needs the base
    getter/setter API.

    HAL_PORT pins are intentionally not supported yet. The port handle
    type and creation semantics are still in flux until the API break
    ("the port change must be done later" in hal.h); a proper port
    wrapper follows once hal_port_t and hal_pin_new_port() are final.
*/
#ifndef HALXX_HH
#define HALXX_HH

#include <map>
#include <string>
#include <vector>
#include <variant>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <cerrno>

#include <hal.h>

// The HAL query API (hal_get_p, hal_set_p, hal_comp_by_name, ...) is
// present when the public header defines the COMPONENT_TYPE_* macros.
#if defined(COMPONENT_TYPE_REALTIME) && !defined(HALXX_WITH_QUERY_API)
#define HALXX_WITH_QUERY_API 1
#endif

#ifndef RTAPI_SINT_MAX
#define RTAPI_SINT_MAX RTAPI_INT64_MAX
#define RTAPI_SINT_MIN RTAPI_INT64_MIN
#define RTAPI_UINT_MAX RTAPI_UINT64_MAX
#endif

namespace linuxcnc {
namespace hal {

// Unified pin/param direction. Values are identical to hal_pdir_t.
enum class dir : int {
    IN  = HAL_IN,
    OUT = HAL_OUT,
    IO  = HAL_IO,
    RO  = HAL_RO,
    RW  = HAL_RW,
};

// Runtime value of a pin, param or signal. Used whenever the HAL type
// is not known at compile time (name-based access, script bindings).
using value_t = std::variant<rtapi_bool, rtapi_s32, rtapi_u32,
                             rtapi_sint, rtapi_uint, rtapi_real>;

namespace detail {
// Error text that works with and without hal_strerror() in the library.
inline std::string errstr(int rv)
{
#ifdef HALXX_WITH_QUERY_API
    return hal_strerror(rv);
#else
    return std::strerror(-rv);
#endif
}
} // namespace detail

//----------------------------------------------------------------------
// Type traits: map an rtapi_ value type to its HAL handle, HAL type and
// accessor/creator functions. Using an unsupported type is a compile
// error because traits<T> is intentionally left undefined.
//
// The 32-bit versions (rtapi_s32, rtapi_u32) are compatibility types
// that will be retired after the API break; only their traits entries
// and the variant alternatives need to change then.
//----------------------------------------------------------------------
template<typename T> struct traits;

template<> struct traits<rtapi_bool> {
    using handle_t = hal_bool_t;
    static handle_t *slot(hal_refs_u *u) { return &u->b; }
    static constexpr hal_type_t type = HAL_BOOL;
    static rtapi_bool get(handle_t h) { return hal_get_bool(h); }
    static rtapi_bool set(handle_t h, rtapi_bool v) { return hal_set_bool(h, v); }
    static int new_pin(int c, hal_pdir_t d, handle_t *h, rtapi_bool def, const std::string &n) {
        return hal_pin_new_bool(c, d, h, def, "%s", n.c_str());
    }
    static int new_param(int c, hal_pdir_t d, handle_t *h, rtapi_bool def, const std::string &n) {
        return hal_param_new_bool(c, d, h, def, "%s", n.c_str());
    }
};

template<> struct traits<rtapi_s32> {
    using handle_t = hal_sint_t;
    static handle_t *slot(hal_refs_u *u) { return &u->s; }
    static constexpr hal_type_t type = HAL_S32;
    static rtapi_s32 get(handle_t h) { return hal_get_si32(h); }
    static rtapi_s32 set(handle_t h, rtapi_s32 v) { return hal_set_si32(h, v); }
    static int new_pin(int c, hal_pdir_t d, handle_t *h, rtapi_s32 def, const std::string &n) {
        return hal_pin_new_si32(c, d, h, def, "%s", n.c_str());
    }
    static int new_param(int c, hal_pdir_t d, handle_t *h, rtapi_s32 def, const std::string &n) {
        return hal_param_new_si32(c, d, h, def, "%s", n.c_str());
    }
};

template<> struct traits<rtapi_u32> {
    using handle_t = hal_uint_t;
    static handle_t *slot(hal_refs_u *u) { return &u->u; }
    static constexpr hal_type_t type = HAL_U32;
    static rtapi_u32 get(handle_t h) { return hal_get_ui32(h); }
    static rtapi_u32 set(handle_t h, rtapi_u32 v) { return hal_set_ui32(h, v); }
    static int new_pin(int c, hal_pdir_t d, handle_t *h, rtapi_u32 def, const std::string &n) {
        return hal_pin_new_ui32(c, d, h, def, "%s", n.c_str());
    }
    static int new_param(int c, hal_pdir_t d, handle_t *h, rtapi_u32 def, const std::string &n) {
        return hal_param_new_ui32(c, d, h, def, "%s", n.c_str());
    }
};

template<> struct traits<rtapi_sint> {
    using handle_t = hal_sint_t;
    static handle_t *slot(hal_refs_u *u) { return &u->s; }
    static constexpr hal_type_t type = HAL_SINT;
    static rtapi_sint get(handle_t h) { return hal_get_sint(h); }
    static rtapi_sint set(handle_t h, rtapi_sint v) { return hal_set_sint(h, v); }
    static int new_pin(int c, hal_pdir_t d, handle_t *h, rtapi_sint def, const std::string &n) {
        return hal_pin_new_sint(c, d, h, def, "%s", n.c_str());
    }
    static int new_param(int c, hal_pdir_t d, handle_t *h, rtapi_sint def, const std::string &n) {
        return hal_param_new_sint(c, d, h, def, "%s", n.c_str());
    }
};

template<> struct traits<rtapi_uint> {
    using handle_t = hal_uint_t;
    static handle_t *slot(hal_refs_u *u) { return &u->u; }
    static constexpr hal_type_t type = HAL_UINT;
    static rtapi_uint get(handle_t h) { return hal_get_uint(h); }
    static rtapi_uint set(handle_t h, rtapi_uint v) { return hal_set_uint(h, v); }
    static int new_pin(int c, hal_pdir_t d, handle_t *h, rtapi_uint def, const std::string &n) {
        return hal_pin_new_uint(c, d, h, def, "%s", n.c_str());
    }
    static int new_param(int c, hal_pdir_t d, handle_t *h, rtapi_uint def, const std::string &n) {
        return hal_param_new_uint(c, d, h, def, "%s", n.c_str());
    }
};

template<> struct traits<rtapi_real> {
    using handle_t = hal_real_t;
    static handle_t *slot(hal_refs_u *u) { return &u->r; }
    static constexpr hal_type_t type = HAL_REAL;
    static rtapi_real get(handle_t h) { return hal_get_real(h); }
    static rtapi_real set(handle_t h, rtapi_real v) { return hal_set_real(h, v); }
    static int new_pin(int c, hal_pdir_t d, handle_t *h, rtapi_real def, const std::string &n) {
        return hal_pin_new_real(c, d, h, def, "%s", n.c_str());
    }
    static int new_param(int c, hal_pdir_t d, handle_t *h, rtapi_real def, const std::string &n) {
        return hal_param_new_real(c, d, h, def, "%s", n.c_str());
    }
};

//----------------------------------------------------------------------
// pin<T> - typed pin or param handle.
//
// Holds a pointer to the handle slot in HAL shared memory and re-reads
// it on every access: hal_link() may rewrite the slot when the pin is
// linked to a signal, exactly like a pin pointer variable in the C API.
// All access goes through the type's inline hal_get_*/hal_set_*
// accessor.
//
// Pins, params and signals are unique HAL objects. Their handles are
// not copyable (no reference counting); use references or move
// semantics. dup() creates an explicit second handle to the same slot
// where that is really intended.
//----------------------------------------------------------------------
template<typename T>
class pin {
public:
    using value_type = T;
    using handle_t = typename traits<T>::handle_t;

    pin() = default;
    explicit pin(handle_t *slot) : slot_(slot) {}
    pin(const pin &) = delete;
    pin &operator=(const pin &) = delete;
    pin(pin &&) = default;
    pin &operator=(pin &&) = default;

    // Explicit second handle to the same HAL object.
    pin dup() const { return pin(slot_); }

    T get() const { check(); return traits<T>::get(*slot_); }
    T set(T v) const { check(); return traits<T>::set(*slot_, v); }

    operator T() const { return get(); }
    T operator=(T v) { return set(v); }

    handle_t handle() const { check(); return *slot_; }
    bool valid() const { return nullptr != slot_ && nullptr != *slot_; }

private:
    void check() const {
        if(!slot_)
            throw std::logic_error("hal::pin: access to uninitialized pin handle");
    }
    handle_t *slot_ = nullptr;
};

//----------------------------------------------------------------------
// pin_t - runtime-typed pin/param/ports. The variant index is the
// stored type tag used for multiplexing, as required for any
// heterogeneous (name-keyed) collection of HAL items.
//----------------------------------------------------------------------
using pin_t = std::variant<pin<rtapi_bool>, pin<rtapi_s32>, pin<rtapi_u32>,
                           pin<rtapi_sint>, pin<rtapi_uint>, pin<rtapi_real>>;

namespace detail {

// In-place scalar access on a runtime-typed item. Port pins have no
// scalar value.
inline value_t pin_get(const pin_t &p)
{
    return std::visit([](auto &&pp) -> value_t { return pp.get(); }, p);
}

inline void pin_set(pin_t &p, const value_t &v)
{
    std::visit([&v](auto &&pp) {
        using P = std::decay_t<decltype(pp)>;
        pp.set(std::visit([](auto &&x) -> typename P::value_type {
            return static_cast<typename P::value_type>(x);
        }, v));
    }, p);
}

inline hal_type_t pin_type(const pin_t &p)
{
    return std::visit([](auto &&pp) -> hal_type_t {
        return traits<typename std::decay_t<decltype(pp)>::value_type>::type;
    }, p);
}

} // namespace detail

//----------------------------------------------------------------------
// anypin - a pin_t plus its full HAL name. This is the object handed
// to script bindings (pybind11) and generic code.
//----------------------------------------------------------------------
class anypin {
public:
    anypin() = default;
    anypin(pin_t p, std::string name) : p_(std::move(p)), name_(std::move(name)) {}
    anypin(const anypin &) = delete;
    anypin &operator=(const anypin &) = delete;
    anypin(anypin &&) = default;
    anypin &operator=(anypin &&) = default;

    const std::string &name() const { return name_; }

    hal_type_t type() const { return detail::pin_type(p_); }

    value_t get() const { return detail::pin_get(p_); }
    void set(const value_t &v) { detail::pin_set(p_, v); }

private:
    pin_t p_;
    std::string name_;
};

//----------------------------------------------------------------------
// component - a userspace HAL component. Owns the comp_id and keeps a
// name-keyed map of its pins and params.
//----------------------------------------------------------------------
class component {
public:
    explicit component(const std::string &name) : prefix_(name) {
        id_ = hal_init(name.c_str());
        if(id_ < 0)
            throw std::runtime_error("hal::component: hal_init(" + name + ") failed: " + detail::errstr(id_));
    }
    component() = delete;
    component(const component &) = delete;
    component &operator=(const component &) = delete;
    ~component() { exit(); }

    int id() const { return id_; }

    void setprefix(const std::string &p) { prefix_ = p; }
    std::string getprefix() const { return prefix_; }

    void ready() {
        int rv = hal_ready(id_);
        if(rv)
            throw std::runtime_error("hal::component: hal_ready failed: " + detail::errstr(rv));
    }

    void exit() {
        if(id_ > 0)
            hal_exit(id_);
        id_ = -1;
    }

    // Create a typed pin "<prefix>.<name>" and keep it in the item map.
    // The handle slot is allocated from HAL shared memory (hal_malloc),
    // as required by the pin/param creation API: hal_link later updates
    // the value through this slot, so it must live in HAL memory. Like
    // halmodule, the slot is released with the component's HAL memory.
    template<typename T>
    pin<T> newpin(const std::string &name, dir d, T def = T{}) {
        hal_refs_u *u = (hal_refs_u *)hal_malloc(sizeof(*u));
        if(!u)
            throw std::runtime_error("hal::component: newpin(" + name + "): hal_malloc failed");
        int rv = traits<T>::new_pin(id_, (hal_pdir_t)d, traits<T>::slot(u), def, fullname(name));
        if(rv)
            throw std::runtime_error("hal::component: newpin(" + name + ") failed: " + detail::errstr(rv));
        items_.emplace(name, pin<T>(traits<T>::slot(u)));
        return pin<T>(traits<T>::slot(u));
    }

    // Attach a new pin to a member handle. This is the struct-member
    // idiom for components: declare pin<T> members in your instance
    // struct and register them with add_pin().
    template<typename T>
    void add_pin(const std::string &name, dir d, pin<T> &target) {
        target = newpin<T>(name, d);
    }

    // Runtime-typed pin creation (script bindings). Returns an anypin.
    anypin newpin(const std::string &name, hal_type_t type, dir d) {
        switch(type) {
        case HAL_BOOL: return wrap(name, newpin<rtapi_bool>(name, d));
        case HAL_S32:  return wrap(name, newpin<rtapi_s32>(name, d));
        case HAL_U32:  return wrap(name, newpin<rtapi_u32>(name, d));
        case HAL_SINT: return wrap(name, newpin<rtapi_sint>(name, d));
        case HAL_UINT: return wrap(name, newpin<rtapi_uint>(name, d));
        case HAL_REAL: return wrap(name, newpin<rtapi_real>(name, d));
        default:
            throw std::invalid_argument("hal::component: newpin(" + name + "): unsupported type");
        }
    }

    // Create a typed parameter "<prefix>.<name>".
    template<typename T>
    pin<T> newparam(const std::string &name, dir d, T def = T{}) {
        hal_refs_u *u = (hal_refs_u *)hal_malloc(sizeof(*u));
        if(!u)
            throw std::runtime_error("hal::component: newparam(" + name + "): hal_malloc failed");
        int rv = traits<T>::new_param(id_, (hal_pdir_t)d, traits<T>::slot(u), def, fullname(name));
        if(rv)
            throw std::runtime_error("hal::component: newparam(" + name + ") failed: " + detail::errstr(rv));
        params_.emplace(name, pin<T>(traits<T>::slot(u)));
        return pin<T>(traits<T>::slot(u));
    }

    anypin newparam(const std::string &name, hal_type_t type, dir d) {
        switch(type) {
        case HAL_BOOL: return wrap(name, newparam<rtapi_bool>(name, d));
        case HAL_S32:  return wrap(name, newparam<rtapi_s32>(name, d));
        case HAL_U32:  return wrap(name, newparam<rtapi_u32>(name, d));
        case HAL_SINT: return wrap(name, newparam<rtapi_sint>(name, d));
        case HAL_UINT: return wrap(name, newparam<rtapi_uint>(name, d));
        case HAL_REAL: return wrap(name, newparam<rtapi_real>(name, d));
        default:
            throw std::invalid_argument("hal::component: newparam(" + name + "): unsupported type");
        }
    }

    // Item access by short name. Pins and params share one namespace.
    value_t getitem(const std::string &name) const { return detail::pin_get(find(name)); }

    template<typename T>
    void setitem(const std::string &name, T value) { detail::pin_set(find(name), value_t(value)); }

    bool contains(const std::string &name) const {
        return items_.count(name) || params_.count(name);
    }

private:
    template<typename T>
    anypin wrap(const std::string &name, pin<T> p) { return anypin(pin_t(std::move(p)), fullname(name)); }

    pin_t &find(const std::string &name) {
        if(auto it = items_.find(name); it != items_.end())
            return it->second;
        if(auto it = params_.find(name); it != params_.end())
            return it->second;
        throw std::out_of_range("hal::component: no pin or param '" + name + "'");
    }
    const pin_t &find(const std::string &name) const {
        return const_cast<component *>(this)->find(name);
    }

    std::string fullname(const std::string &n) const { return prefix_ + "." + n; }

    int id_ = -1;
    std::string prefix_;
    std::map<std::string, pin_t> items_;
    std::map<std::string, pin_t> params_;
};

//----------------------------------------------------------------------
// Signal management, thin wrappers over the C API (user-land only).
//----------------------------------------------------------------------
#ifdef ULAPI
inline int signal_new(const std::string &name, hal_type_t type)
{
    return hal_signal_new(name.c_str(), type);
}
inline int link(const std::string &pin_name, const std::string &sig_name)
{
    return hal_link(pin_name.c_str(), sig_name.c_str());
}
inline int unlink(const std::string &pin_name)
{
    return hal_unlink(pin_name.c_str());
}
inline int signal_delete(const std::string &name)
{
    return hal_signal_delete(name.c_str());
}
#endif // ULAPI

//----------------------------------------------------------------------
// Userspace by-name query and set API. Implemented on the public HAL
// query API (hal_get_p/hal_set_p/hal_get_s/hal_set_s/hal_comp_by_name).
// This section is user-space only by definition: the query API itself
// is only declared under ULAPI, so this code cannot be used in RTAPI.
//----------------------------------------------------------------------
#if defined(ULAPI) && defined(HALXX_WITH_QUERY_API)

namespace detail {

// Convert a runtime value to the requested HAL type with range checks.
// Must not throw: it is called from query callbacks while the HAL
// mutex is held, and unwinding through the library would keep the
// mutex locked and wedge the whole HAL session. Returns false on a
// range/type error, the caller reports it after the library call.
inline bool convert_value(hal_type_t target, const value_t &v, hal_query_value_u *out)
{
    bool ok = true;
    std::visit([&ok, out, target](auto &&x) {
        long double xv = static_cast<long double>(x);
        switch(target) {
        case HAL_BOOL:
            out->b = (0 != xv);
            break;
        case HAL_S32:
            if(xv < RTAPI_INT32_MIN || xv > RTAPI_INT32_MAX) { ok = false; break; }
            out->s = static_cast<rtapi_s32>(xv); break;
        case HAL_U32:
            if(xv < 0 || xv > RTAPI_UINT32_MAX) { ok = false; break; }
            out->u = static_cast<rtapi_u32>(xv); break;
        case HAL_SINT:
            if(xv < (long double)RTAPI_SINT_MIN || xv > (long double)RTAPI_SINT_MAX) { ok = false; break; }
            out->s = static_cast<rtapi_sint>(xv); break;
        case HAL_UINT:
            if(xv < 0 || xv > (long double)RTAPI_UINT_MAX) { ok = false; break; }
            out->u = static_cast<rtapi_uint>(xv); break;
        case HAL_REAL:
            out->r = static_cast<rtapi_real>(xv); break;
        default:
            ok = false;
        }
    }, v);
    return ok;
}

inline value_t value_from_query(hal_type_t t, const hal_query_value_u &v)
{
    switch(t) {
    case HAL_BOOL: return (rtapi_bool)v.b;
    case HAL_S32:  return (rtapi_s32)v.s;
    case HAL_U32:  return (rtapi_u32)v.u;
    case HAL_SINT: return (rtapi_sint)v.s;
    case HAL_UINT: return (rtapi_uint)v.u;
    case HAL_REAL: return (rtapi_real)v.r;
    default:
        throw std::invalid_argument("hal: item has no scalar value (port or unknown type)");
    }
}

// Setter callbacks: fill the query's value union coerced to the item's
// actual type. Called with the HAL mutex held, hence no exceptions,
// no allocation and no termination; see convert_value.
struct coerce_req {
    const value_t *v;
    bool failed;
};
inline int coerce_pp_cb(hal_query_t *q, void *arg)
{
    auto *req = static_cast<coerce_req *>(arg);
    if(!convert_value(q->pp.type, *req->v, &q->pp.value)) {
        req->failed = true;
        return -ERANGE;
    }
    return 0;
}
inline int coerce_sig_cb(hal_query_t *q, void *arg)
{
    auto *req = static_cast<coerce_req *>(arg);
    if(!convert_value(q->sig.type, *req->v, &q->sig.value)) {
        req->failed = true;
        return -ERANGE;
    }
    return 0;
}

} // namespace detail

// True if a component with this name is loaded.
inline bool component_exists(const std::string &name)
{
    hal_query_t q = {};
    return 0 == hal_comp_by_name(name.c_str(), &q);
}

// True if the component exists and has called hal_ready().
inline bool component_is_ready(const std::string &name)
{
    hal_query_t q = {};
    return 0 == hal_comp_by_name(name.c_str(), &q) && q.comp.ready;
}

// True if the pin exists, is connected to a signal, and that signal
// has at least one writer.
inline bool pin_has_writer(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    q.qtype = HAL_QTYPE_PIN;
    if(0 != hal_getref_p(&q) || !q.pp.signal)
        return false;
    hal_query_t sq = {};
    sq.name = q.pp.signal;
    if(0 != hal_getref_s(&sq))
        return false;
    return sq.sig.writers > 0;
}

// Read the value of a pin, param or signal by name. Throws
// std::invalid_argument with the library error if the lookup fails.
inline value_t get_value(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    int rv = hal_get_p(&q, nullptr, nullptr);
    if(0 == rv)
        return detail::value_from_query(q.pp.type, q.pp.value);
    if(0 == (rv = hal_get_s(&q, nullptr, nullptr)))
        return detail::value_from_query(q.sig.type, q.sig.value);
    throw std::invalid_argument("hal: get_value(" + name + ") failed: " + detail::errstr(rv));
}

// Set a pin or param by name ("setp"). The value is coerced to the
// item's actual HAL type with range checks.
inline void set_value(const std::string &name, const value_t &v)
{
    hal_query_t q = {};
    q.name = name.c_str();
    detail::coerce_req req{&v, false};
    int rv = hal_set_p(&q, detail::coerce_pp_cb, &req);
    if(req.failed)
        throw std::out_of_range("hal: set_value(" + name + "): value does not fit the item's type");
    if(rv)
        throw std::invalid_argument("hal: set_value(" + name + ") failed: " + detail::errstr(rv));
}

// Set a signal by name ("sets").
inline void set_signal(const std::string &name, const value_t &v)
{
    hal_query_t q = {};
    q.name = name.c_str();
    detail::coerce_req req{&v, false};
    int rv = hal_set_s(&q, detail::coerce_sig_cb, &req);
    if(req.failed)
        throw std::out_of_range("hal: set_signal(" + name + "): value does not fit the signal's type");
    if(rv)
        throw std::invalid_argument("hal: set_signal(" + name + ") failed: " + detail::errstr(rv));
}

#endif // ULAPI && HALXX_WITH_QUERY_API

} // namespace hal
} // namespace linuxcnc

//----------------------------------------------------------------------
// Compatibility aliases for code written against the previous hal.hh
// (pybind11 branch). New code should use linuxcnc::hal names.
//----------------------------------------------------------------------
using hal_dir = linuxcnc::hal::dir;
using hal_comp = linuxcnc::hal::component;
using PyPin = linuxcnc::hal::anypin;
template<typename T> using hal_pin = linuxcnc::hal::pin<T>;

#endif // HALXX_HH
