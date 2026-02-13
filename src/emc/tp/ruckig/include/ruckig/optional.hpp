#pragma once

#include <type_traits>


namespace ruckig {

struct nullopt_t {
    explicit constexpr nullopt_t(int) { }
};

constexpr nullopt_t nullopt{0};


//! A lightweight C++11-compatible optional type
template<typename T>
class Optional {
    bool has_value_;
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;

    T* ptr() { return reinterpret_cast<T*>(&storage_); }
    const T* ptr() const { return reinterpret_cast<const T*>(&storage_); }

    void destroy() {
        if (has_value_) {
            ptr()->~T();
            has_value_ = false;
        }
    }

public:
    Optional() : has_value_(false) { }
    Optional(nullopt_t) : has_value_(false) { }

    Optional(const T& val) : has_value_(true) {
        new (&storage_) T(val);
    }

    Optional(const Optional& other) : has_value_(false) {
        if (other.has_value_) {
            new (&storage_) T(*other.ptr());
            has_value_ = true;
        }
    }

    Optional(Optional&& other) : has_value_(false) {
        if (other.has_value_) {
            new (&storage_) T(static_cast<T&&>(*other.ptr()));
            has_value_ = true;
        }
    }

    ~Optional() { destroy(); }

    Optional& operator=(nullopt_t) {
        destroy();
        return *this;
    }

    Optional& operator=(const T& val) {
        destroy();
        new (&storage_) T(val);
        has_value_ = true;
        return *this;
    }

    Optional& operator=(const Optional& other) {
        if (this != &other) {
            destroy();
            if (other.has_value_) {
                new (&storage_) T(*other.ptr());
                has_value_ = true;
            }
        }
        return *this;
    }

    Optional& operator=(Optional&& other) {
        if (this != &other) {
            destroy();
            if (other.has_value_) {
                new (&storage_) T(static_cast<T&&>(*other.ptr()));
                has_value_ = true;
            }
        }
        return *this;
    }

    explicit operator bool() const { return has_value_; }
    bool has_value() const { return has_value_; }

    T& value() { return *ptr(); }
    const T& value() const { return *ptr(); }

    T& operator*() { return *ptr(); }
    const T& operator*() const { return *ptr(); }

    T* operator->() { return ptr(); }
    const T* operator->() const { return ptr(); }

    T value_or(const T& default_val) const {
        return has_value_ ? *ptr() : default_val;
    }

    bool operator==(const Optional& other) const {
        if (has_value_ != other.has_value_) return false;
        if (!has_value_) return true;
        return *ptr() == *other.ptr();
    }

    bool operator!=(const Optional& other) const {
        return !(*this == other);
    }
};


//! Comparison between T and Optional<T>
template<typename T>
inline bool operator==(const T& lhs, const Optional<T>& rhs) {
    return rhs.has_value() && lhs == rhs.value();
}

template<typename T>
inline bool operator!=(const T& lhs, const Optional<T>& rhs) {
    return !(lhs == rhs);
}

template<typename T>
inline bool operator==(const Optional<T>& lhs, const T& rhs) {
    return lhs.has_value() && lhs.value() == rhs;
}

template<typename T>
inline bool operator!=(const Optional<T>& lhs, const T& rhs) {
    return !(lhs == rhs);
}

} // namespace ruckig
