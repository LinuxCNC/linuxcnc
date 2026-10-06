/********************************************************************
 * Description: logutil.hh
 *   Console messages tagged with their severity, replacing libnml's
 *   rcs_print(), rcs_print_error() and rcs_print_debug().
 *
 *   Call sites go through these rather than straight to fmt::print(),
 *   so each one still says how severe its message is. When a real
 *   logging facility arrives (see discussion #4638), only these
 *   functions change, not every caller.
 *
 *   The streams are the ones libnml used by default: errors to stderr,
 *   everything else to stdout.
 *
 * License: GPL Version 2
 * System: Linux
 ********************************************************************/
#ifndef LINUXCNC_LOGUTIL_HH
#define LINUXCNC_LOGUTIL_HH

#include <cstdio>
#include <utility>
#include <fmt/format.h>

#include "nml_intf/emcglb.h"	// emc_debug

namespace linuxcnc {

/// An error, on stderr. Was rcs_print_error().
template <typename... T>
void log_error(fmt::format_string<T...> f, T &&...args)
{
    fmt::print(stderr, f, std::forward<T>(args)...);
}

/// An informational message, on stdout. Was rcs_print().
template <typename... T>
void log_info(fmt::format_string<T...> f, T &&...args)
{
    fmt::print(f, std::forward<T>(args)...);
}

/// A debug message, on stdout, printed only when any of the EMC_DEBUG_*
/// bits in `flags` is set in emc_debug. Replaces the
/// `if (emc_debug & EMC_DEBUG_x) rcs_print(...)` pattern.
template <typename... T>
void log_debug(unsigned flags, fmt::format_string<T...> f, T &&...args)
{
    if (emc_debug & flags) {
        fmt::print(f, std::forward<T>(args)...);
    }
}

} // namespace linuxcnc

#endif
