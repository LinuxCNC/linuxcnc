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
*/
#ifndef HALXX_HH
#define HALXX_HH

#include <map>
#include <string>
#include <vector>
#include <variant>
#include <type_traits>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <cstdint>
#include <cerrno>

#include <hal.h>

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
inline value_t pin_get(const pin_t &p)
{
    return std::visit([](auto &&pp) -> value_t {
        using P = std::decay_t<decltype(pp)>;
        if constexpr(std::is_same_v<P, port>)
            throw std::invalid_argument("hal: a port pin has no value, use the port calls");
        else
            return pp.get();
    }, p);
}

// Convert a runtime value to the value type of a typed handle. Throws
// std::out_of_range instead of truncating or wrapping.
template<typename T, typename X>
T checked_cast(X x)
{
    long double xv = static_cast<long double>(x);
    if constexpr(std::is_same_v<T, rtapi_sint>) {
        if(xv < (long double)RTAPI_SINT_MIN || xv > (long double)RTAPI_SINT_MAX)
            throw std::out_of_range("hal: value does not fit a sint item");
    } else if constexpr(std::is_same_v<T, rtapi_uint>) {
        if(xv < 0 || xv > (long double)RTAPI_UINT_MAX)
            throw std::out_of_range("hal: value does not fit a uint item");
    }
    return static_cast<T>(x);
}

inline void pin_set(pin_t &p, const value_t &v)
{
    std::visit([&v](auto &&pp) {
        using P = std::decay_t<decltype(pp)>;
        if constexpr(std::is_same_v<P, port>) {
            throw std::invalid_argument("hal: a port pin has no value, use the port calls");
        } else {
            pp.set(std::visit([](auto &&x) -> typename P::value_type {
                return checked_cast<typename P::value_type>(x);
            }, v));
        }
    }, p);
}

inline hal_type_t pin_type(const pin_t &p)
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
    anypin(pin_t p, std::string name) : p_(std::move(p)), name_(std::move(name)) {}
    anypin(const anypin &) = delete;
    anypin &operator=(const anypin &) = delete;
    anypin(anypin &&) = default;
    anypin &operator=(anypin &&) = default;

    const std::string &name() const { return name_; }

    hal_type_t type() const { return detail::pin_type(p_); }

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
            throw std::runtime_error("hal::component: hal_init(" + name + ") failed: " + hal_strerror(id_));
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
            throw std::runtime_error(std::string("hal::component: hal_ready failed: ") + hal_strerror(rv));
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
            throw std::runtime_error("hal::component: newpin(" + name + ") failed: " + hal_strerror(rv));
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

    // Create a port pin "<prefix>.<name>". A port is IN (reader) or OUT
    // (writer); there are no port params.
    port newport(const std::string &name, dir d) {
        hal_refs_u *u = (hal_refs_u *)hal_malloc(sizeof(*u));
        if(!u)
            throw std::runtime_error("hal::component: newport(" + name + "): hal_malloc failed");
        int rv = hal_pin_new_port(id_, (hal_pdir_t)d, &u->p, "%s", fullname(name).c_str());
        if(rv)
            throw std::runtime_error("hal::component: newport(" + name + ") failed: " + hal_strerror(rv));
        items_.emplace(name, port(&u->p));
        return port(&u->p);
    }

    void add_port(const std::string &name, dir d, port &target) {
        target = newport(name, d);
    }

    // Runtime-typed pin creation (script bindings). Returns an anypin.
    anypin newpin(const std::string &name, hal_type_t type, dir d) {
        switch(type) {
        case HAL_BOOL: return wrap(name, newpin<rtapi_bool>(name, d));
        case HAL_SINT: return wrap(name, newpin<rtapi_sint>(name, d));
        case HAL_UINT: return wrap(name, newpin<rtapi_uint>(name, d));
        case HAL_REAL: return wrap(name, newpin<rtapi_real>(name, d));
        case HAL_PORT: return anypin(pin_t(newport(name, d)), fullname(name));
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
            throw std::runtime_error("hal::component: newparam(" + name + ") failed: " + hal_strerror(rv));
        params_.emplace(name, pin<T>(traits<T>::slot(u)));
        return pin<T>(traits<T>::slot(u));
    }

    anypin newparam(const std::string &name, hal_type_t type, dir d) {
        switch(type) {
        case HAL_BOOL: return wrap(name, newparam<rtapi_bool>(name, d));
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
// Streams. hal_stream_t is the fixed-depth sample FIFO behind sampler
// and streamer: one component creates it with a depth and a typestring,
// another attaches to the same integer key. Each character of the
// typestring names the type of one element of a sample.
//----------------------------------------------------------------------
namespace detail {

// The typestring characters used by hal_stream_create(), as reported
// back through hal_stream_element_type().
inline char stream_typechar(hal_type_t t)
{
    switch(t) {
    case HAL_BOOL: return 'b';
    case HAL_REAL: return 'f';
    case HAL_SINT: return 's';
    case HAL_UINT: return 'u';
    default:       return '?';
    }
}

inline value_t value_from_stream(hal_type_t t, const hal_stream_data_u &d)
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

// Coerce a runtime value into a stream element of the given type.
// Returns false on a range error; the caller reports it.
inline bool convert_stream_value(hal_type_t target, const value_t &v, hal_stream_data_u *out)
{
    bool ok = true;
    std::visit([&ok, out, target](auto &&x) {
        long double xv = static_cast<long double>(x);
        switch(target) {
        case HAL_BOOL:
            out->b = (0 != xv);
            break;
        case HAL_SINT:
            if(xv < (long double)RTAPI_SINT_MIN || xv > (long double)RTAPI_SINT_MAX) { ok = false; break; }
            out->s = static_cast<rtapi_sint>(xv); break;
        case HAL_UINT:
            if(xv < 0 || xv > (long double)RTAPI_UINT_MAX) { ok = false; break; }
            out->u = static_cast<rtapi_uint>(xv); break;
        case HAL_REAL:
            out->f = static_cast<rtapi_real>(xv); break;
        default:
            ok = false;
        }
    }, v);
    return ok;
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
#ifdef ULAPI

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
        case HAL_SINT:
            if(xv < (long double)RTAPI_SINT_MIN || xv > (long double)RTAPI_SINT_MAX) { ok = false; break; }
            out->s = static_cast<rtapi_sint>(xv); break;
        case HAL_UINT:
            if(xv < 0 || xv > (long double)RTAPI_UINT_MAX) { ok = false; break; }
            out->u = static_cast<rtapi_uint>(xv); break;
        case HAL_REAL:
            out->r = static_cast<rtapi_real>(xv); break;
        case HAL_PORT:
            // Buffer size of a port signal; the library refuses it for pins.
            if(xv < 1 || xv > HAL_PORT_SIZE_MAX) { ok = false; break; }
            out->u = static_cast<rtapi_uint>(xv); break;
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

// Read the value of a pin, param or signal by name. A port signal
// reads as its buffer size; a port pin has no value. Throws
// std::invalid_argument if the lookup fails or the item is a port pin.
inline value_t get_value(const std::string &name)
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
    throw std::invalid_argument("hal: get_value(" + name + ") failed: " + hal_strerror(rv));
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
        throw std::invalid_argument("hal: set_value(" + name + ") failed: " + hal_strerror(rv));
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
        throw std::invalid_argument("hal: set_signal(" + name + ") failed: " + hal_strerror(rv));
}

#endif // ULAPI

} // namespace hal
} // namespace linuxcnc

#endif // HALXX_HH
