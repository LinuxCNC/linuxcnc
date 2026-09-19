/********************************************************************
 * Description: strutil.hh
 *   Bounded copy and append into fixed char arrays.
 *
 *   These replace libnml's nml_strxcpy()/nml_strxcat() macros, which
 *   lived in <rcs/rcs_print.hh> despite having nothing to do with
 *   printing or with NML. Taking the destination by array reference
 *   deduces its size, so a call site cannot get the bound wrong -- the
 *   macros used sizeof(dst) and a static_assert to the same end.
 *
 *   Like the macros (which went through snprintf), a null `src` is
 *   rendered as "(null)" rather than being undefined behaviour.
 *
 * License: GPL Version 2
 * System: Linux
 ********************************************************************/
#ifndef LINUXCNC_STRUTIL_HH
#define LINUXCNC_STRUTIL_HH

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace linuxcnc {

namespace detail {
inline std::string_view sv_or_null(const char *s)
{
    return s ? std::string_view(s) : std::string_view("(null)");
}
} // namespace detail

/// Copy `src` into `dst`, truncating if it does not fit, always leaving
/// `dst` NUL-terminated.
template <std::size_t N>
void strxcpy(char (&dst)[N], const char *src)
{
    static_assert(N > 0, "destination must be a non-empty char array");
    const std::string_view s = detail::sv_or_null(src);
    const std::size_t n = std::min(s.size(), N - 1);
    *std::copy_n(s.begin(), n, dst) = '\0';
}

/// Append `src` to `dst`, truncating if it does not fit, always leaving
/// `dst` NUL-terminated. A `dst` that is already full is left alone.
template <std::size_t N>
void strxcat(char (&dst)[N], const char *src)
{
    static_assert(N > 0, "destination must be a non-empty char array");
    const std::size_t len = std::char_traits<char>::length(dst);
    if (len >= N - 1) {
        return;
    }
    const std::string_view s = detail::sv_or_null(src);
    const std::size_t n = std::min(s.size(), N - 1 - len);
    *std::copy_n(s.begin(), n, dst + len) = '\0';
}

} // namespace linuxcnc

#endif
