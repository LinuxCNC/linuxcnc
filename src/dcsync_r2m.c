#include "priv.h"

/**
 * @file dcsync_r2m.c
 * @brief Distributed Clock synchronisation: Reference-to-Master (R2M) mode.
 *
 * In R2M mode the EtherCAT reference clock slave is periodically nudged to
 * track the LinuxCNC master (RTAPI) time.  Unlike the M2R mode (dcsync_m2r.c),
 * the RTAPI task PLL is @b not adjusted; instead the EtherCAT bus time follows
 * the LinuxCNC time base passively.
 *
 * This mode is suitable when no @c RTAPI_TASK_PLL_SUPPORT is available or
 * when the system integrator prefers the simpler, one-direction scheme.
 *
 * Call sequence each servo cycle:
 *   1. cycle_start() – sets @c master->app_time_ns from the RTAPI PLL reference
 *      and informs the EtherCAT master of the current application time.
 *   2. pre_send()    – periodically (every @c ref_clock_sync_cycles cycles)
 *      writes the RTAPI time to the reference clock slave via
 *      @c ecrt_master_sync_reference_clock_to(), then synchronises all slave
 *      clocks to the reference.
 *   3. post_send()   – NOP (no correction to apply in R2M mode).
 */

/**
 * @brief Set application time from the RTAPI PLL reference and inform the master.
 *
 * Updates @c master->app_time_ns each cycle from the RTAPI PLL reference
 * counter plus the global @c dc_time_offset, then passes the value to
 * @c ecrt_master_application_time() so that EtherCAT frames carry the correct
 * application time stamp.
 *
 * Unlike the M2R cycle_start(), the application time in R2M mode is read
 * afresh every cycle rather than being predicted from the previous value,
 * so the EtherCAT bus follows the RTAPI clock unconditionally.
 *
 * @param master  EtherCAT master to update.
 *
 * @note Real-time context: must not block or allocate memory.
 * @note Side effect: modifies @c master->app_time_ns.
 */
static void cycle_start(struct lcec_master *master) {
  master->app_time_ns = dc_time_offset + rtapi_task_pll_get_reference();
  ecrt_master_application_time(master->master, master->app_time_ns);
}

/**
 * @brief Periodically synchronise the EtherCAT reference clock to master time.
 *
 * Called each cycle before the EtherCAT frame is sent.  Decrements the
 * @c ref_clock_sync_counter; when it reaches zero the counter is reset to
 * @c ref_clock_sync_cycles and @c ecrt_master_sync_reference_clock_to() is
 * called with the current RTAPI clock value to steer the reference clock slave.
 *
 * Using @c rtapi_get_time() (rather than @c rtapi_task_pll_get_reference())
 * for the sync call compensates for the run-time delay accumulated since the
 * start of the servo cycle, improving accuracy.
 *
 * @c ecrt_master_sync_slave_clocks() is always called (every cycle) to
 * distribute the reference clock value to all other slaves.
 *
 * If @c master->ref_clock_sync_cycles is 0 or negative the function returns
 * immediately without performing any sync.
 *
 * @param master  EtherCAT master.
 *
 * @note Real-time context: must not block or allocate memory.
 * @note Side effect: modifies @c master->ref_clock_sync_counter.
 */
static void pre_send(struct lcec_master *master) {
  if (master->ref_clock_sync_cycles <= 0) {
    return;
  }

  master->ref_clock_sync_counter--;
  if (master->ref_clock_sync_counter <= 0) {
    master->ref_clock_sync_counter = master->ref_clock_sync_cycles;

    // sync reference clock to master
    // use current time here to compensate run time delay, as this get called
    // late in the rt cycle
    ecrt_master_sync_reference_clock_to(master->master, dc_time_offset + rtapi_get_time());
  }

  // call to sync slaves to ref slave
  ecrt_master_sync_slave_clocks(master->master);
}

/**
 * @brief Post-send callback — no operation in R2M mode.
 *
 * In R2M mode there is no PLL correction to apply after the frame exchange,
 * so this callback is intentionally empty.
 *
 * @param master  EtherCAT master (unused).
 */
static void post_send(struct lcec_master *master) {
  // NOP
}

/**
 * @brief Initialise DC synchronisation in Reference-to-Master (R2M) mode.
 *
 * Registers the cycle_start(), pre_send(), and post_send() callbacks in
 * @c master->dcsync_callbacks and resets the internal state variables.
 *
 * The reference clock will be synchronised to the LinuxCNC master time every
 * @c master->ref_clock_sync_cycles servo cycles.  A value of 0 disables
 * periodic reference-clock sync (only slave-clock distribution is performed).
 *
 * @param master  Master to configure.  @c master->ref_clock_sync_cycles must
 *                be set before or immediately after this call.
 *
 * @note Non-real-time: called once during component initialisation.
 */
void lcec_dc_init_r2m(struct lcec_master *master) {
  master->dcsync_callbacks.cycle_start = cycle_start;
  master->dcsync_callbacks.pre_send = pre_send;
  master->dcsync_callbacks.post_send = post_send;

  master->app_time_ns = 0;
  master->ref_clock_sync_counter = 0;
}

