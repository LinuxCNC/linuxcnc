/*
    hal.hh - C++ interface for HAL

    A thin, type-safe C++ layer on top of the public HAL C API.
    All pin/param access goes through the typed hal_get_X and hal_set_X
    accessors and the user-land query API. No direct shared memory
    access, no hal_priv.h, no re-implemented library internals.

    Header-only, no runtime overhead: typed access expands to the same
    inline accessor calls as the C API.

    HAL integers are 64-bit only (HAL_SINT, HAL_UINT); there are no
    32-bit handles. The by-name query/set section is built on the HAL
    query API and is user-space only.

    HAL_PORT pins get their own handle, port, with byte-stream access
    instead of a scalar value.

    Errors are exceptions, documented here once:
      std::system_error      a library call failed, with its errno
      std::out_of_range      a value does not fit its item, or there is
                             no item of that name or index
      std::invalid_argument  the wrong kind of item, e.g. a value asked
                             of a port pin, or an unsupported type
      std::logic_error       use of an uninitialized or closed handle
      std::bad_alloc         hal_malloc() failed
    A lookup that finds nothing is not an error: it returns false or an
    empty std::optional. Nothing throws out of a destructor or out of a
    query callback, where unwinding would leave the HAL mutex held.
*/
#ifndef HALXX_HH
#define HALXX_HH

#include <map>
#include <new>
#include <string>
#include <vector>
#include <variant>
#include <type_traits>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <cstdint>
#include <cerrno>
#include <cmath>

#include "hal.h"

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
using value_t = std::variant<rtapi_bool, rtapi_sint, rtapi_uint, rtapi_real>;

//----------------------------------------------------------------------
// Type traits: map an rtapi_ value type to its HAL handle, HAL type and
// accessor/creator functions. Using an unsupported type is a compile
// error because traits<T> is intentionally left undefined.
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
// port - handle of a HAL_PORT pin, an asynchronous one-way byte stream
// with one reader (the IN pin) and one writer (the OUT pin). The
// buffer belongs to the signal the pins are linked to and is sized by
// setting that signal ("sets"), see set_signal(); an unlinked port has
// no buffer and reads and writes fail. Like pin<T>, the handle re-reads
// the slot on every access. Reads and writes are all or nothing.
//----------------------------------------------------------------------
class port {
public:
    port() = default;
    explicit port(hal_port_t *slot) : slot_(slot) {}
    port(const port &) = delete;
    port &operator=(const port &) = delete;
    port(port &&) = default;
    port &operator=(port &&) = default;

    // Explicit second handle to the same HAL object.
    port dup() const { return port(slot_); }

    // False while the pin is not linked to a sized port signal.
    bool has_buffer() const { return 0 != hal_get_port(handle()); }

    unsigned size() const { return has_buffer() ? hal_port_buffer_size(handle()) : 0; }
    unsigned readable() const { return has_buffer() ? hal_port_readable(handle()) : 0; }
    unsigned writable() const { return has_buffer() ? hal_port_writable(handle()) : 0; }
    void clear() const { if(has_buffer()) hal_port_clear(handle()); }

    bool read(char *dst, unsigned n) const { return has_buffer() && hal_port_read(handle(), dst, n); }
    bool peek(char *dst, unsigned n) const { return has_buffer() && hal_port_peek(handle(), dst, n); }
    bool peek_commit(unsigned n) const { return has_buffer() && hal_port_peek_commit(handle(), n); }
    bool write(const char *src, unsigned n) const { return has_buffer() && hal_port_write(handle(), src, n); }

    // n bytes, or nothing when fewer than n are readable.
    std::optional<std::vector<char>> read(unsigned n) const {
        std::vector<char> buf(n);
        if(!read(buf.data(), n))
            return std::nullopt;
        return buf;
    }
    std::optional<std::vector<char>> peek(unsigned n) const {
        std::vector<char> buf(n);
        if(!peek(buf.data(), n))
            return std::nullopt;
        return buf;
    }
    bool write(const std::vector<char> &data) const {
        return write(data.data(), (unsigned)data.size());
    }

    hal_port_t handle() const { check(); return *slot_; }
    bool valid() const { return nullptr != slot_ && nullptr != *slot_; }

private:
    void check() const {
        if(!slot_)
            throw std::logic_error("hal::port: access to uninitialized port handle");
    }
    hal_port_t *slot_ = nullptr;
};

//----------------------------------------------------------------------
// pin_t - runtime-typed pin/param/ports. The variant index is the
// stored type tag used for multiplexing, as required for any
// heterogeneous (name-keyed) collection of HAL items.
//----------------------------------------------------------------------
using pin_t = std::variant<pin<rtapi_bool>, pin<rtapi_sint>, pin<rtapi_uint>,
                           pin<rtapi_real>, port>;

namespace detail {

// In-place scalar access on a runtime-typed item. A port pin has no
// scalar value: use the port calls, the buffer size belongs to the
// linking signal.
static inline value_t pin_get(const pin_t &p)
{
    return std::visit([](auto &&pp) -> value_t {
        using P = std::decay_t<decltype(pp)>;
        if constexpr(std::is_same_v<P, port>)
            throw std::invalid_argument("hal: a port pin has no value, use the port calls");
        else
            return pp.get();
    }, p);
}

// Convert a runtime value to T exactly, or fail: nothing is rounded,
// truncated or wrapped. An integer type takes an integer in its range,
// or a real that is finite, whole and in range. A bool takes any value,
// non-zero is true, as in C. A real takes any value. Must not throw: the
// by-name setters call it with the HAL mutex held.
template<typename T>
static inline bool convert(const value_t &v, T *out) noexcept
{
    return std::visit([out](auto x) -> bool {
        using X = decltype(x);
        if constexpr(std::is_same_v<T, rtapi_real>) {
            *out = static_cast<rtapi_real>(x);
            return true;
        } else if constexpr(std::is_same_v<T, rtapi_bool>) {
            *out = (0 != x);
            return true;
        } else if constexpr(std::is_same_v<X, rtapi_real>) {
            // 2^63 and 2^64 are exact as doubles, the integer limits are not
            if(!std::isfinite(x) || x != std::trunc(x))
                return false;
            if constexpr(std::is_same_v<T, rtapi_sint>) {
                // cppcheck-suppress incorrectLogicOperator
                if(x < -0x1p63 || x >= 0x1p63)
                    return false;
            } else if(x < 0 || x >= 0x1p64)
                return false;
            *out = static_cast<T>(x);
            return true;
        } else if constexpr(std::is_same_v<T, rtapi_sint>) {
            if constexpr(std::is_same_v<X, rtapi_uint>)
                if(x > (rtapi_uint)RTAPI_SINT_MAX)
                    return false;
            *out = static_cast<rtapi_sint>(x);
            return true;
        } else {
            if constexpr(std::is_same_v<X, rtapi_sint>)
                if(x < 0)
                    return false;
            *out = static_cast<rtapi_uint>(x);
            return true;
        }
    }, v);
}

// Same, for a typed handle: throws std::out_of_range on failure.
template<typename T>
static inline T checked_cast(const value_t &v)
{
    T out{};
    if(!convert(v, &out))
        throw std::out_of_range(std::string("hal: value does not fit a ") +
                                (traits<T>::type == HAL_BOOL ? "bool" :
                                 traits<T>::type == HAL_SINT ? "sint" :
                                 traits<T>::type == HAL_UINT ? "uint" : "real") + " item");
    return out;
}

static inline void pin_set(pin_t &p, const value_t &v)
{
    std::visit([&v](auto &&pp) {
        using P = std::decay_t<decltype(pp)>;
        if constexpr(std::is_same_v<P, port>) {
            throw std::invalid_argument("hal: a port pin has no value, use the port calls");
        } else {
            pp.set(checked_cast<typename P::value_type>(v));
        }
    }, p);
}

static inline hal_type_t pin_type(const pin_t &p)
{
    return std::visit([](auto &&pp) -> hal_type_t {
        using P = std::decay_t<decltype(pp)>;
        if constexpr(std::is_same_v<P, port>)
            return HAL_PORT;
        else
            return traits<typename P::value_type>::type;
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
    anypin(pin_t p, std::string name, bool is_pin, dir d)
        : p_(std::move(p)), name_(std::move(name)), is_pin_(is_pin), dir_(d) {}
    anypin(const anypin &) = delete;
    anypin &operator=(const anypin &) = delete;
    anypin(anypin &&) = default;
    anypin &operator=(anypin &&) = default;

    // Explicit second handle to the same HAL object.
    anypin dup() const {
        return anypin(std::visit([](auto &&pp) -> pin_t { return pp.dup(); }, p_),
                      name_, is_pin_, dir_);
    }

    const std::string &name() const { return name_; }

    hal_type_t type() const { return detail::pin_type(p_); }
    bool is_pin() const { return is_pin_; }
    dir direction() const { return dir_; }

    value_t get() const { return detail::pin_get(p_); }
    void set(const value_t &v) { detail::pin_set(p_, v); }

    bool is_port() const { return std::holds_alternative<port>(p_); }
    const port &as_port() const {
        if(const port *pp = std::get_if<port>(&p_))
            return *pp;
        throw std::invalid_argument("hal: " + name_ + " is not a port pin");
    }

private:
    pin_t p_;
    std::string name_;
    bool is_pin_ = true;
    dir dir_ = dir::IN;
};

//----------------------------------------------------------------------
// component - a userspace HAL component. Owns the comp_id and keeps a
// name-keyed map of its pins and params.
//
// A failing library call throws std::system_error carrying the errno
// the library returned; running out of HAL memory throws
// std::bad_alloc.
//----------------------------------------------------------------------
class component {
public:
    explicit component(const std::string &name) : prefix_(name) {
        id_ = hal_init(name.c_str());
        if(id_ < 0)
            throw std::system_error(-id_, std::generic_category(),
                                    "hal::component: hal_init(" + name + ") failed");
    }
    component() = delete;
    component(const component &) = delete;
    component &operator=(const component &) = delete;
    ~component() { exit(); }

    int id() const { return id_; }

    void setprefix(const std::string &p) { prefix_ = p; }
    const std::string &getprefix() const { return prefix_; }

    void ready() {
        int rv = hal_ready(id_);
        if(rv)
            throw std::system_error(-rv, std::generic_category(), "hal::component: hal_ready failed");
    }

    // Back to not ready, to add pins and params; call ready() again after.
    void unready() {
        int rv = hal_unready(id_);
        if(rv)
            throw std::system_error(-rv, std::generic_category(), "hal::component: hal_unready failed");
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
            throw std::bad_alloc();
        int rv = traits<T>::new_pin(id_, (hal_pdir_t)d, traits<T>::slot(u), def, fullname(name));
        if(rv)
            throw std::system_error(-rv, std::generic_category(), "hal::component: newpin(" + name + ") failed");
        items_.emplace(name, anypin(pin<T>(traits<T>::slot(u)), fullname(name), true, d));
        return pin<T>(traits<T>::slot(u));
    }

    // Attach a new pin to a member handle. This is the struct-member
    // idiom for components: declare pin<T> members in your instance
    // struct and register them with add_pin().
    template<typename T>
    void add_pin(const std::string &name, dir d, pin<T> &target) {
        target = newpin<T>(name, d);
    }

    // Create a port pin "<prefix>.<name>". A port is IN (reader) or OUT
    // (writer); there are no port params.
    port newport(const std::string &name, dir d) {
        hal_refs_u *u = (hal_refs_u *)hal_malloc(sizeof(*u));
        if(!u)
            throw std::bad_alloc();
        int rv = hal_pin_new_port(id_, (hal_pdir_t)d, &u->p, "%s", fullname(name).c_str());
        if(rv)
            throw std::system_error(-rv, std::generic_category(), "hal::component: newport(" + name + ") failed");
        items_.emplace(name, anypin(port(&u->p), fullname(name), true, d));
        return port(&u->p);
    }

    void add_port(const std::string &name, dir d, port &target) {
        target = newport(name, d);
    }

    // Runtime-typed pin creation (script bindings). Returns an anypin.
    anypin newpin(const std::string &name, hal_type_t type, dir d) {
        switch(type) {
        case HAL_BOOL: newpin<rtapi_bool>(name, d); break;
        case HAL_SINT: newpin<rtapi_sint>(name, d); break;
        case HAL_UINT: newpin<rtapi_uint>(name, d); break;
        case HAL_REAL: newpin<rtapi_real>(name, d); break;
        case HAL_PORT: newport(name, d); break;
        default:
            throw std::invalid_argument("hal::component: newpin(" + name + "): unsupported type");
        }
        return item(name).dup();
    }

    // Create a typed parameter "<prefix>.<name>".
    template<typename T>
    pin<T> newparam(const std::string &name, dir d, T def = T{}) {
        hal_refs_u *u = (hal_refs_u *)hal_malloc(sizeof(*u));
        if(!u)
            throw std::bad_alloc();
        int rv = traits<T>::new_param(id_, (hal_pdir_t)d, traits<T>::slot(u), def, fullname(name));
        if(rv)
            throw std::system_error(-rv, std::generic_category(), "hal::component: newparam(" + name + ") failed");
        items_.emplace(name, anypin(pin<T>(traits<T>::slot(u)), fullname(name), false, d));
        return pin<T>(traits<T>::slot(u));
    }

    anypin newparam(const std::string &name, hal_type_t type, dir d) {
        switch(type) {
        case HAL_BOOL: newparam<rtapi_bool>(name, d); break;
        case HAL_SINT: newparam<rtapi_sint>(name, d); break;
        case HAL_UINT: newparam<rtapi_uint>(name, d); break;
        case HAL_REAL: newparam<rtapi_real>(name, d); break;
        default:
            throw std::invalid_argument("hal::component: newparam(" + name + "): unsupported type");
        }
        return item(name).dup();
    }

    // Item access by short name. Pins and params share one namespace,
    // HAL refuses a pin and a param of the same name.
    value_t getitem(const std::string &name) const { return item(name).get(); }

    template<typename T>
    void setitem(const std::string &name, T value) { item(name).set(value_t(value)); }

    bool contains(const std::string &name) const { return items_.count(name); }

    // True for a pin, false for a param. Throws std::out_of_range when
    // there is no item of that name.
    bool is_pin(const std::string &name) const { return item(name).is_pin(); }

    anypin &item(const std::string &name) {
        if(auto it = items_.find(name); it != items_.end())
            return it->second;
        throw std::out_of_range("hal::component: no pin or param '" + name + "'");
    }
    const anypin &item(const std::string &name) const {
        return const_cast<component *>(this)->item(name);
    }

    // All pins and params, by short name.
    const std::map<std::string, anypin> &items() const { return items_; }

private:
    std::string fullname(const std::string &n) const { return prefix_ + "." + n; }

    int id_ = -1;
    std::string prefix_;
    std::map<std::string, anypin> items_;
};

//----------------------------------------------------------------------
// Streams. hal_stream_t is the fixed-depth sample FIFO behind sampler
// and streamer: one component creates it with a depth and a typestring,
// another attaches to the same integer key. Each character of the
// typestring names the type of one element of a sample.
//----------------------------------------------------------------------
namespace detail {

// The typestring characters used by hal_stream_create(), as reported
// back through hal_stream_element_type().
static inline char stream_typechar(hal_type_t t)
{
    switch(t) {
    case HAL_BOOL: return 'b';
    case HAL_REAL: return 'f';
    case HAL_SINT: return 's';
    case HAL_UINT: return 'u';
    default:       return '?';
    }
}

static inline value_t value_from_stream(hal_type_t t, const hal_stream_data_u &d)
{
    switch(t) {
    case HAL_BOOL: return (rtapi_bool)d.b;
    case HAL_SINT: return (rtapi_sint)d.s;
    case HAL_UINT: return (rtapi_uint)d.u;
    case HAL_REAL: return (rtapi_real)d.f;
    default:
        throw std::invalid_argument("hal::stream: element has an unsupported type");
    }
}

// Convert a runtime value into a stream element of the given type, see
// convert(). Returns false when it does not fit; the only caller,
// stream::write(), then throws before anything is written.
static inline bool convert_stream_value(hal_type_t target, const value_t &v, hal_stream_data_u *out)
{
    switch(target) {
    case HAL_BOOL: { rtapi_bool b; if(!convert(v, &b)) return false; out->b = b; return true; }
    case HAL_SINT: return convert(v, &out->s);
    case HAL_UINT: return convert(v, &out->u);
    case HAL_REAL: return convert(v, &out->f);
    default:       return false;
    }
}

} // namespace detail

//----------------------------------------------------------------------
// stream - an open HAL stream, either created (and owned) or attached
// to. The library permits only one reader and one writer, but does not
// enforce it.
//
// Like the other HAL objects, a stream is move-only: destroying or
// detaching twice would corrupt the FIFO's user counts.
//----------------------------------------------------------------------
class stream {
public:
    // Create a stream holding 'depth' samples of the layout described
    // by 'typestring'. The stream is destroyed with this object.
    stream(component &comp, int key, unsigned depth, const std::string &typestring)
        : key_(key), creator_(true)
    {
        int rv = hal_stream_create(&s_, comp.id(), key, depth, typestring.c_str());
        if(rv < 0)
            throw std::system_error(-rv, std::generic_category(),
                "hal::stream: create(" + std::to_string(key) + ", " + typestring + ") failed");
        open_ = true;
        read_element_types();
    }

    // Attach to an existing stream. An empty typestring accepts
    // whatever layout the stream was created with; a non-empty one must
    // match it.
    stream(component &comp, int key, const std::string &typestring = std::string())
        : key_(key), creator_(false)
    {
        int rv = hal_stream_attach(&s_, comp.id(), key,
                                   typestring.empty() ? nullptr : typestring.c_str());
        if(rv < 0)
            throw std::system_error(-rv, std::generic_category(),
                "hal::stream: attach(" + std::to_string(key) + ") failed");
        open_ = true;
        read_element_types();
    }

    stream() = delete;
    stream(const stream &) = delete;
    stream &operator=(const stream &) = delete;
    stream(stream &&o) noexcept { adopt(o); }
    stream &operator=(stream &&o) noexcept {
        if(this != &o) { close(); adopt(o); }
        return *this;
    }
    ~stream() { close(); }

    // Destroy (creator) or detach from (attacher) the stream. Further
    // access throws; this is what the destructor does.
    void close() {
        if(!open_)
            return;
        open_ = false;
        if(creator_)
            hal_stream_destroy(&s_);
        else
            hal_stream_detach(&s_);
    }

    int key() const { return key_; }
    bool is_creator() const { return creator_; }
    bool is_open() const { return open_; }

    int element_count() const { return (int)types_.size(); }
    hal_type_t element_type(int idx) const {
        if(idx < 0 || idx >= element_count())
            throw std::out_of_range("hal::stream: element index out of range");
        return types_[idx];
    }
    // The layout in hal_stream_create() typestring form.
    const std::string &typestring() const { return typestring_; }

    // Read one sample. Returns nothing when the stream is empty, which
    // also counts an underrun in the library.
    std::optional<std::vector<value_t>> read() {
        if(types_.empty())
            return std::nullopt;
        std::vector<hal_stream_data_u> buf(types_.size());
        if(hal_stream_read(handle(), buf.data(), &sampleno_) < 0)
            return std::nullopt;
        std::vector<value_t> out;
        out.reserve(types_.size());
        for(size_t i = 0; i < types_.size(); i++)
            out.push_back(detail::value_from_stream(types_[i], buf[i]));
        return out;
    }

    // Write one sample. The values are coerced to the element types
    // with range checks. Writing to a full stream fails and counts an
    // overrun in the library.
    void write(const std::vector<value_t> &data) {
        if(data.size() != types_.size())
            throw std::invalid_argument("hal::stream: write expects " +
                std::to_string(types_.size()) + " elements, got " + std::to_string(data.size()));
        std::vector<hal_stream_data_u> buf(types_.size());
        for(size_t i = 0; i < types_.size(); i++)
            if(!detail::convert_stream_value(types_[i], data[i], &buf[i]))
                throw std::out_of_range("hal::stream: element " + std::to_string(i) +
                    " does not fit its type");
        int rv = hal_stream_write(handle(), buf.data());
        if(rv < 0)
            throw std::system_error(-rv, std::generic_category(), "hal::stream: write failed");
    }

    bool readable() const { return hal_stream_readable(handle()); }
    bool writable() const { return hal_stream_writable(handle()); }
    int depth() const { return hal_stream_depth(handle()); }
    unsigned maxdepth() const { return hal_stream_maxdepth(handle()); }
    int num_underruns() const { return hal_stream_num_underruns(handle()); }
    int num_overruns() const { return hal_stream_num_overruns(handle()); }

    // Number of the last sample read().
    unsigned sampleno() const { return sampleno_; }

private:
    // The C API takes a non-const hal_stream_t * even where it only
    // reads, so the const accessors go through here.
    hal_stream_t *handle() const {
        if(!open_)
            throw std::logic_error("hal::stream: access to a closed stream");
        return const_cast<hal_stream_t *>(&s_);
    }

    void read_element_types() {
        int n = hal_stream_element_count(&s_);
        for(int i = 0; i < n; i++) {
            hal_type_t t = hal_stream_element_type(&s_, i);
            types_.push_back(t);
            typestring_.push_back(detail::stream_typechar(t));
        }
    }

    void adopt(stream &o) {
        s_ = o.s_;
        types_ = std::move(o.types_);
        typestring_ = std::move(o.typestring_);
        key_ = o.key_;
        creator_ = o.creator_;
        sampleno_ = o.sampleno_;
        open_ = o.open_;
        o.open_ = false;
    }

    hal_stream_t s_ = {};
    std::vector<hal_type_t> types_;
    std::string typestring_;
    int key_ = 0;
    bool creator_ = false;
    bool open_ = false;
    unsigned sampleno_ = 0;
};

//----------------------------------------------------------------------
// Signal management, thin wrappers over the C API (user-land only).
//----------------------------------------------------------------------
#ifdef ULAPI
static inline int signal_new(const std::string &name, hal_type_t type)
{
    return hal_signal_new(name.c_str(), type);
}
static inline int link(const std::string &pin_name, const std::string &sig_name)
{
    return hal_link(pin_name.c_str(), sig_name.c_str());
}
static inline int unlink(const std::string &pin_name)
{
    return hal_unlink(pin_name.c_str());
}
static inline int signal_delete(const std::string &name)
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
#ifdef ULAPI

namespace detail {

// Convert a runtime value to the requested HAL type, see convert().
// Must not throw: it is called from query callbacks while the HAL
// mutex is held, and unwinding through the library would keep the
// mutex locked and wedge the whole HAL session. Returns false when the
// value does not fit, the caller reports it after the library call.
static inline bool convert_value(hal_type_t target, const value_t &v, hal_query_value_u *out)
{
    switch(target) {
    case HAL_BOOL: { rtapi_bool b; if(!convert(v, &b)) return false; out->b = b; return true; }
    case HAL_SINT: return convert(v, &out->s);
    case HAL_UINT: return convert(v, &out->u);
    case HAL_REAL: return convert(v, &out->r);
    case HAL_PORT: {
        // Buffer size of a port signal; the library refuses it for pins.
        rtapi_uint n;
        if(!convert(v, &n) || n < 1 || n > HAL_PORT_SIZE_MAX)
            return false;
        out->u = n;
        return true;
    }
    default:
        return false;
    }
}

static inline value_t value_from_query(hal_type_t t, const hal_query_value_u &v)
{
    switch(t) {
    case HAL_BOOL: return (rtapi_bool)v.b;
    case HAL_SINT: return (rtapi_sint)v.s;
    case HAL_UINT: return (rtapi_uint)v.u;
    case HAL_REAL: return (rtapi_real)v.r;
    case HAL_PORT: return (rtapi_uint)v.u;
    default:
        throw std::invalid_argument("hal: item has an unknown type");
    }
}

// Setter callbacks: fill the query's value union coerced to the item's
// actual type. Called with the HAL mutex held, hence no exceptions,
// no allocation and no termination; see convert_value.
struct coerce_req {
    const value_t *v;
    bool failed;
};
static inline int coerce_pp_cb(hal_query_t *q, void *arg)
{
    auto *req = static_cast<coerce_req *>(arg);
    if(!convert_value(q->pp.type, *req->v, &q->pp.value)) {
        req->failed = true;
        return -ERANGE;
    }
    return 0;
}
static inline int coerce_sig_cb(hal_query_t *q, void *arg)
{
    auto *req = static_cast<coerce_req *>(arg);
    if(!convert_value(q->sig.type, *req->v, &q->sig.value)) {
        req->failed = true;
        return -ERANGE;
    }
    return 0;
}

} // namespace detail

namespace detail {

// A lookup that finds nothing answers the question with false; any
// other failure (HAL not mapped, a bad argument) is not an answer and
// throws std::system_error with the library's errno.
static inline bool found(int rv, const char *what, const std::string &name)
{
    if(0 == rv)
        return true;
    if(-ENOENT == rv)
        return false;
    throw std::system_error(-rv, std::generic_category(),
                            std::string("hal: ") + what + "(" + name + ") failed");
}

} // namespace detail

// The component's state by name or by id, nothing when there is no such
// component. Throws std::system_error when the lookup fails otherwise.
static inline std::optional<hal_query_t> component_query(const std::string &name)
{
    hal_query_t q = {};
    if(!detail::found(hal_comp_by_name(name.c_str(), &q), "hal_comp_by_name", name))
        return std::nullopt;
    return q;
}
static inline std::optional<hal_query_t> component_query(int comp_id)
{
    hal_query_t q = {};
    if(!detail::found(hal_comp_by_id(comp_id, &q), "hal_comp_by_id", std::to_string(comp_id)))
        return std::nullopt;
    return q;
}

// True if the component is loaded.
static inline bool component_exists(const std::string &name) { return component_query(name).has_value(); }
static inline bool component_exists(int comp_id) { return component_query(comp_id).has_value(); }

// True if the component is loaded and has called hal_ready().
static inline bool component_is_ready(const std::string &name)
{
    auto q = component_query(name);
    return q && q->comp.ready;
}
static inline bool component_is_ready(int comp_id)
{
    auto q = component_query(comp_id);
    return q && q->comp.ready;
}

// True if the pin exists, is connected to a signal, and that signal
// has at least one writer.
static inline bool pin_has_writer(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    q.qtype = HAL_QTYPE_PIN;
    if(!detail::found(hal_getref_p(&q), "hal_getref_p", name) || !q.pp.signal)
        return false;
    hal_query_t sq = {};
    sq.name = q.pp.signal;
    if(!detail::found(hal_getref_s(&sq), "hal_getref_s", sq.name))
        return false;
    return sq.sig.writers > 0;
}

// Read the value of a pin, param or signal by name. A port signal
// reads as its buffer size; a port pin has no value. Throws
// std::system_error if there is no such item or the lookup fails, and
// std::invalid_argument for a port pin.
static inline value_t get_value(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    int rv = hal_get_p(&q, nullptr, nullptr);
    if(0 == rv) {
        if(HAL_PORT == q.pp.type)
            throw std::invalid_argument("hal: get_value(" + name + "): a port pin has no value, read the size from its signal");
        return detail::value_from_query(q.pp.type, q.pp.value);
    }
    if(0 == (rv = hal_get_s(&q, nullptr, nullptr)))
        return detail::value_from_query(q.sig.type, q.sig.value);
    throw std::system_error(-rv, std::generic_category(), "hal: get_value(" + name + ") failed");
}

// The state of a pin or param by name, nothing when there is none.
// 'qtype' HAL_QTYPE_PIN or HAL_QTYPE_PARAM limits the search. pp.ref
// reads the value with the hal_get_* accessors; for a connected pin it
// is the signal's. The value union is not filled in, and the pointers
// in the result point into HAL shared memory. Throws std::system_error
// when the lookup fails otherwise.
static inline std::optional<hal_query_t> pin_param_query(const std::string &name,
                                                         hal_qtype_t qtype = HAL_QTYPE_ANY)
{
    hal_query_t q = {};
    q.name = name.c_str();
    q.qtype = qtype;
    if(!detail::found(hal_getref_p(&q), "hal_getref_p", name))
        return std::nullopt;
    return q;
}

// The state of a signal by name, nothing when there is none; sig.ref
// reads the value, as for pin_param_query().
static inline std::optional<hal_query_t> signal_query(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    if(!detail::found(hal_getref_s(&q), "hal_getref_s", name))
        return std::nullopt;
    return q;
}

// Set a pin or param by name ("setp"). The value is coerced to the
// item's actual HAL type with range checks; std::out_of_range when it
// does not fit, std::system_error when the library refuses the set.
static inline void set_value(const std::string &name, const value_t &v)
{
    hal_query_t q = {};
    q.name = name.c_str();
    detail::coerce_req req{&v, false};
    int rv = hal_set_p(&q, detail::coerce_pp_cb, &req);
    if(req.failed)
        throw std::out_of_range("hal: set_value(" + name + "): value does not fit the item's type");
    if(rv)
        throw std::system_error(-rv, std::generic_category(), "hal: set_value(" + name + ") failed");
}

// Set a signal by name ("sets"), with the same failures.
static inline void set_signal(const std::string &name, const value_t &v)
{
    hal_query_t q = {};
    q.name = name.c_str();
    detail::coerce_req req{&v, false};
    int rv = hal_set_s(&q, detail::coerce_sig_cb, &req);
    if(req.failed)
        throw std::out_of_range("hal: set_signal(" + name + "): value does not fit the signal's type");
    if(rv)
        throw std::system_error(-rv, std::generic_category(), "hal: set_signal(" + name + ") failed");
}

#endif // ULAPI

} // namespace hal
} // namespace linuxcnc

#endif // HALXX_HH
