/********************************************************************
 * Description: timeutil.hh
 *   Clock and sleep helpers, replacing libnml's <os_intf/timer.hh>.
 *
 *   The names etime() and esleep() are kept so that dropping libnml is
 *   an include swap at each call site rather than a rewrite.
 *
 *   One deliberate change: libnml's etime() read the wall clock through
 *   gettimeofday(), so every interval measured with it could be stretched
 *   or cut short by an NTP step. etime() here is monotonic, which is what
 *   all but two callers want. The two that report a real timestamp to a
 *   client -- the linuxcncrsh TIME command and the Tcl emc_time binding --
 *   call wall_etime() instead.
 *
 * License: GPL Version 2
 * System: Linux
 ********************************************************************/
#ifndef LINUXCNC_TIMEUTIL_HH
#define LINUXCNC_TIMEUTIL_HH

#include <chrono>
#include <thread>

namespace linuxcnc {

/// Seconds on a monotonic clock. The epoch is arbitrary, so this is only
/// meaningful as a difference: use it for intervals and deadlines.
inline double etime()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

/// Seconds since the Unix epoch, for a timestamp reported to a client.
/// This is what libnml's etime() returned.
inline double wall_etime()
{
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

/// Sleep for the given number of seconds, resuming after a signal.
/// std::this_thread::sleep_for already loops on EINTR with the unslept
/// remainder, which is all libnml's esleep() did once its never-enabled
/// sched_yield() path is discounted.
inline void esleep(double seconds)
{
    if (seconds <= 0.0) {
        return;
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

/// Sleep for a chrono duration, so a caller with a constant interval can
/// write esleep(100ms) rather than a bare double.
template <class Rep, class Period>
inline void esleep(const std::chrono::duration<Rep, Period> &d)
{
    if (d > std::chrono::duration<Rep, Period>::zero()) {
        std::this_thread::sleep_for(d);
    }
}

/**
 * Cyclic rate limiter, replacing libnml's RCS_TIMER.
 *
 * wait() sleeps to the next period boundary measured from the previous
 * wake, so one long cycle does not push every later cycle late -- the
 * phase is kept, exactly as RCS_TIMER did. Unlike RCS_TIMER this runs off
 * a monotonic clock, so an NTP step cannot stall or skip a cycle.
 */
class CyclicTimer
{
  public:
    explicit CyclicTimer(double period_seconds)
      : period(std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                   std::chrono::duration<double>(period_seconds))),
        last(std::chrono::steady_clock::now())
    {
    }

    /// Sleep until the next period boundary; returns whole periods elapsed
    /// since the last wake, i.e. the number of cycles missed.
    int wait()
    {
        if (period <= std::chrono::steady_clock::duration::zero()) {
            return 0;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto interval = now - last;
        const int missed = static_cast<int>(interval / period);
        std::this_thread::sleep_for(period - (interval % period));
        last = std::chrono::steady_clock::now();
        return missed;
    }

  private:
    std::chrono::steady_clock::duration period;
    std::chrono::steady_clock::time_point last;
};

} // namespace linuxcnc

#endif
