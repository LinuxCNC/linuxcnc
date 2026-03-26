/**
 * @file dcsync_m2r.c
 * @brief Distributed Clock synchronisation: Master-to-Reference-clock (M2R) mode.
 *
 * In M2R mode the LinuxCNC servo thread is synchronized to the EtherCAT
 * reference clock via the RTAPI task PLL.  The EtherCAT reference clock is
 * the timing master.
 *
 * A discrete-time PI controller running once per servo cycle computes a
 * nanosecond correction that is applied to the RTAPI task PLL via
 * @c rtapi_task_pll_set_correction().  The controller gains (@c dc_kp,
 * @c dc_ki) are derived from the desired settling time and damping ratio at
 * initialisation time and stored in the master struct.
 *
 * Call sequence each servo cycle:
 *   1. cycle_start() – advances application time and informs the EtherCAT
 *      master of the current application time stamp.
 *   2. pre_send()    – reads the hardware reference clock, unwraps its 32-bit
 *      counter to 64-bit, computes the phase error, and triggers slave-clock
 *      synchronisation.
 *   3. post_send()   – runs the PI controller and applies the PLL correction.
 *
 * This file is compiled only when @c RTAPI_TASK_PLL_SUPPORT is defined.
 *
 * @copyright Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "priv.h"

#ifdef RTAPI_TASK_PLL_SUPPORT

/**
 * @brief Target closed-loop settling time in seconds.
 *
 * Used together with @c DC_DAMPING to compute the PI controller natural
 * frequency @c wn = 4 / (DC_DAMPING * DC_SETTLE_TIME).
 */
#define DC_SETTLE_TIME       1.5    // target settling time in seconds

/**
 * @brief Desired closed-loop damping ratio.
 *
 * A value of 0.707 (1/√2) gives a critically-damped (Butterworth) response
 * that minimises overshoot while maintaining a fast settling time.
 */
#define DC_DAMPING           0.707  // damping ratio (critically damped)

/**
 * @brief Anti-windup clamp applied to the PI integrator state (nanoseconds).
 *
 * Limits the integrator accumulator to ±@c DC_INTEGRATOR_MAX to prevent
 * wind-up during large or sustained phase transients.
 */
#define DC_INTEGRATOR_MAX    1000.0 // integrator anti-windup clamp (ns)

/**
 * @brief Maximum PLL correction output magnitude in nanoseconds.
 *
 * Hard clamps the combined proportional + integral output before it is
 * passed to @c rtapi_task_pll_set_correction(), preventing runaway
 * corrections due to outlier measurements.
 */
#define DC_CORRECTION_MAX_NS 5000.0 // output correction safety clamp (ns)

/**
 * @brief Return the arithmetic sign of a value: +1, 0, or -1.
 *
 * Uses a statement expression (GCC extension) to avoid evaluating @p val
 * more than once even when it has side effects.
 *
 * @param val  Integer or floating-point expression to test.
 * @return 1 if @p val > 0, -1 if @p val < 0, 0 if @p val == 0.
 */
#define sign(val) \
    ({ typeof (val) _val = (val); \
    ((_val > 0) - (_val < 0)); })

/**
 * @brief Saturating cast from int64_t to int32_t.
 *
 * Returns @c INT32_MAX / @c INT32_MIN if @p val overflows the 32-bit range,
 * otherwise returns the truncated value.  Used to safely store a potentially
 * large phase-error value into a 32-bit HAL pin.
 *
 * @param val  64-bit signed integer to clamp.
 * @return Saturated 32-bit representation of @p val.
 */
static inline int32_t clamp32(int64_t val) {
  if (val > INT32_MAX) return INT32_MAX;
  if (val < INT32_MIN) return INT32_MIN;
  return (int32_t)val;
}

/**
 * @brief Advance the application time stamp and inform the EtherCAT master.
 *
 * Called at the very start of each servo cycle, before any PDO I/O.
 *
 * On the first cycle (@c master->app_time_ns == 0) the application time is
 * bootstrapped from the RTAPI PLL reference plus the global @c dc_time_offset.
 * On subsequent cycles the time is incremented by exactly one servo period
 * (@c master->app_time_period) to produce a monotonically increasing, jitter-
 * free time base.
 *
 * The resulting time is written to the EtherCAT master via
 * @c ecrt_master_application_time() so that distributed-clock frames carry
 * the correct application time stamp.
 *
 * @param master  EtherCAT master to update.
 *
 * @note Real-time context: must not block or allocate memory.
 * @note Side effect: modifies @c master->app_time_ns.
 */
static void cycle_start(struct lcec_master *master) {
  if (master->app_time_ns == 0) {
    master->app_time_ns = master->dc_time_offset + rtapi_task_pll_get_reference();
  } else {
    master->app_time_ns += master->app_time_period;
  }

  ecrt_master_application_time(master->master, master->app_time_ns);
}

/**
 * @brief Read the reference clock, compute the phase error, and trigger slave sync.
 *
 * Called each servo cycle after cycle_start() and before the EtherCAT frame
 * is sent.  Performs the following steps:
 *
 *   1. **Reference clock unwrap**: reads the 32-bit hardware reference-clock
 *      counter via @c ecrt_master_reference_clock_time() and extends it to a
 *      64-bit nanosecond timestamp (@c master->ref_time_ns), handling the
 *      32-bit wrap-around that occurs approximately every 4.3 seconds.
 *
 *   2. **Phase error calculation**: computes the difference between the
 *      predicted application time (@c master->dc_time_ns) and the measured
 *      reference clock time (@c master->ref_time_ns).
 *
 *   3. **Re-snap**: if the absolute error exceeds 1.5 cycle periods (large
 *      transient or start-up misalignment), the application time is
 *      corrected by a whole number of periods and the PLL reset counter HAL
 *      pin is incremented.
 *
 *   4. **Slave clock sync**: calls @c ecrt_master_sync_slave_clocks() to
 *      distribute the current reference clock time to all slaves.
 *
 *   5. **Next-cycle prediction**: updates @c master->dc_time_ns with an
 *      estimate of the application time at the next pre_send() call, using
 *      the current RTAPI timer offset.
 *
 * @param master  EtherCAT master.
 *
 * @note Real-time context: must not block or allocate memory.
 * @note Side effects: modifies @c master->ref_time_ns, @c master->dc_time_ns,
 *       @c master->dc_diff_ns, @c master->app_time_ns, and the @c pll_err and
 *       @c pll_reset_cnt HAL pins.
 */
static void pre_send(struct lcec_master *master) {
  lcec_master_data_t *hal_data = master->hal_data;
  uint32_t ref_time_ns;

  // read reference slave's time (unwrap to 64 bits)
  ecrt_master_reference_clock_time(master->master, &ref_time_ns);
  if (master->ref_time_ns > 0 || ref_time_ns != 0) {
    if (master->ref_time_ns == 0) {
      // first valid reading: take upper bits from app_time, lower bits from hardware
      master->ref_time_ns = master->app_time_ns & 0xffffffff00000000LL;
    }

    // detect wrap
    if (ref_time_ns < (uint32_t) master->ref_time_ns) {
      master->ref_time_ns += (1LL << 32);
    }

    // replace lower 32 bits with hardware truth
    master->ref_time_ns = (master->ref_time_ns & 0xffffffff00000000LL) | ref_time_ns;
  }

  // get reference clock diff to synchronize master cycle
  if (master->dc_time_ns > 0) {
    int64_t diff = master->dc_time_ns - master->ref_time_ns;

    // resnap: if diff exceeds +/- 1.5 cycle times, hard adjust by whole periods
    int64_t period = (int64_t)master->app_time_period;
    int64_t diff_abs = llabs(diff);
    if (diff_abs > (period + (period / 2))) {
      int64_t offset_ns = sign(diff) * (diff_abs / period) * period;
      master->app_time_ns -= offset_ns;
      diff -= offset_ns;
      if (master->dc_diff_ns != 0) {
        (*(hal_data->pll_reset_cnt))++;
      }
    }

    master->dc_diff_ns = diff;
    *(hal_data->pll_err) = clamp32(diff);
  }

  // call to sync slaves to ref slave
  ecrt_master_sync_slave_clocks(master->master);

  // capture current time for next cycle
  master->dc_time_ns = master->app_time_ns
    + (rtapi_get_time() - rtapi_task_pll_get_reference());
}

/**
 * @brief Run the PI controller and apply the RTAPI PLL correction.
 *
 * Called after the EtherCAT frame has been sent and received.  Uses the
 * phase error (@c master->dc_diff_ns) computed by pre_send() to drive a
 * discrete-time PI controller:
 *
 * @verbatim
 *   integrator += ki * error                    (anti-windup at ±DC_INTEGRATOR_MAX)
 *   correction  = kp * error + integrator       (clamped at ±DC_CORRECTION_MAX_NS)
 * @endverbatim
 *
 * A positive @c error (application clock ahead of reference) produces a
 * positive correction that advances the RTAPI PLL, causing the next task
 * wakeup to occur slightly earlier and reducing the phase difference.
 *
 * The computed correction is applied via @c rtapi_task_pll_set_correction()
 * and exposed on the @c pll_out HAL pin for monitoring.
 *
 * The controller does not run until the DC reference clock has been observed
 * at least once (@c master->dc_started is set by the first non-zero
 * @c ref_time_ns reading).
 *
 * @param master  EtherCAT master.
 *
 * @note Real-time context: must not block or allocate memory.
 * @note Side effects: modifies @c master->dc_integrator, calls
 *       @c rtapi_task_pll_set_correction(), and updates the @c pll_out HAL pin.
 */
static void post_send(struct lcec_master *master) {
  lcec_master_data_t *hal_data = master->hal_data;

  // check if DC has started initially
  if (!master->dc_started) {
    master->dc_started = (master->ref_time_ns != 0);
    return;
  }

  // PI controller
  double error = (double)master->dc_diff_ns;

  // Integral term (accumulated frequency correction)
  master->dc_integrator += master->dc_ki * error;

  // Anti-windup clamp on integrator
  if (master->dc_integrator > DC_INTEGRATOR_MAX) master->dc_integrator = DC_INTEGRATOR_MAX;
  if (master->dc_integrator < -DC_INTEGRATOR_MAX) master->dc_integrator = -DC_INTEGRATOR_MAX;

  // Proportional + Integral output (positive error => advance clock)
  double correction = (master->dc_kp * error + master->dc_integrator);

  // Output clamp (safety)
  if (correction > DC_CORRECTION_MAX_NS) correction = DC_CORRECTION_MAX_NS;
  if (correction < -DC_CORRECTION_MAX_NS) correction = -DC_CORRECTION_MAX_NS;

  int32_t correction_ns = (int32_t)correction;
  rtapi_task_pll_set_correction(correction_ns);
  *(hal_data->pll_out) = correction_ns;
}

/**
 * @brief Initialise DC synchronisation in Master-to-Reference-clock (M2R) mode.
 *
 * Registers the cycle_start(), pre_send(), and post_send() callbacks in
 * @c master->dcsync_callbacks and computes the PI controller gains from the
 * configured servo period:
 *
 * @verbatim
 *   T  = app_time_period (s)
 *   wn = 4 / (DC_DAMPING * DC_SETTLE_TIME)    [natural frequency, rad/s]
 *   wd = wn * T                                [normalised to discrete time]
 *   kp = 2 * DC_DAMPING * wd
 *   ki = wd^2
 * @endverbatim
 *
 * This placement of poles gives a second-order response with the specified
 * settling time and damping ratio.
 *
 * All master DC state variables are zeroed so that the controller starts
 * cleanly even if called multiple times (e.g., after a bus reset).
 *
 * @param master  Master to configure.  @c master->app_time_period must already
 *                be set to the servo period in nanoseconds before this call.
 *
 * @note This function is available only when @c RTAPI_TASK_PLL_SUPPORT is
 *       defined at compile time.
 * @note Non-real-time: called once during component initialisation.
 */
void lcec_dc_init_m2r(struct lcec_master *master) {
  master->dcsync_callbacks.cycle_start = cycle_start;
  master->dcsync_callbacks.pre_send = pre_send;
  master->dcsync_callbacks.post_send = post_send;

  master->app_time_ns = 0;
  master->ref_time_ns = 0;
  master->dc_time_ns = 0;
  master->dc_started = 0;
  master->dc_diff_ns = 0;

  // compute PI gains from cycle period
  double T = (double)master->app_time_period * 1.0e-9;
  double wn = 4.0 / (DC_DAMPING * DC_SETTLE_TIME);
  double wd = wn * T;
  master->dc_kp = 2.0 * DC_DAMPING * wd;
  master->dc_ki = wd * wd;
  master->dc_integrator = 0.0;
}
#endif

// TODO
// ecrt_master_select_reference_clock
