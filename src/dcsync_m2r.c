#include "priv.h"

#include <stdio.h>
static int testcnt = 0;

#define DC_FILTER_CNT 1024

#define sign(val) \
    ({ typeof (val) _val = (val); \
    ((_val > 0) - (_val < 0)); })

static void cycle_start(struct lcec_master *master) {
  // TODO: reset, if things run out of control
  if (master->app_time_ns == 0) {
    master->app_time_ns = dc_time_offset + rtapi_task_pll_get_reference();
  } else {
    master->app_time_ns += master->app_time_period;
  }

  ecrt_master_application_time(master->master, master->app_time_ns);
}

static void pre_send(struct lcec_master *master) {
  // get reference clock diff to synchronize master cycle
  if (master->dc_time_ns > 0) {
    ecrt_master_reference_clock_time(master->master, &(master->dc_ref_ns));
    master->dc_diff_ns = (uint32_t) master->dc_time_ns - master->dc_ref_ns;
  }

  master->dc_time_ns = master->app_time_ns
    + (rtapi_get_time() - rtapi_task_pll_get_reference());

  // call to sync slaves to ref slave
  ecrt_master_sync_slave_clocks(master->master);
}

static void post_send(struct lcec_master *master) {
  // calc drift (via un-normalised time diff)
  int32_t delta = master->dc_diff_ns - master->prev_dc_diff_ns;
  master->prev_dc_diff_ns = master->dc_diff_ns;

  // normalise the time diff
  int32_t dc_diff_ns_norm =
        ((master->dc_diff_ns + (master->app_time_period / 2)) % master->app_time_period)
        - (master->app_time_period / 2);

  // check if DC has started initially
  if (!master->dc_started) {
    master->dc_started = (master->dc_ref_ns != 0);
    if (master->dc_started) {
      // output first diff
      // TODO
      printf("First master diff: %d.\n", dc_diff_ns_norm);

      // record the time of this initial cycle
       master->dc_start_time_ns = master->dc_time_ns;

      // TODO: initial correction
    }

    return;
  }

  // add to totals
  master->dc_diff_total_ns += dc_diff_ns_norm;
  master->dc_delta_total_ns += delta;
  master->dc_filter_idx++;

  if (master->dc_filter_idx >= DC_FILTER_CNT) {
    // add rounded delta average
    master->dc_adjust_ns +=
      ((master->dc_delta_total_ns + (DC_FILTER_CNT / 2)) / DC_FILTER_CNT);

    // and add adjustment for general diff (to pull in drift)
    master->dc_adjust_ns += sign(master->dc_diff_total_ns / DC_FILTER_CNT);

    // limit crazy numbers (0.1% of std cycle time)
    if (master->dc_adjust_ns < -1000) {
      master->dc_adjust_ns = -1000;
    }
    if (master->dc_adjust_ns > 1000) {
      master->dc_adjust_ns =  1000;
    }

    // reset
    master->dc_diff_total_ns = 0;
    master->dc_delta_total_ns = 0;
    master->dc_filter_idx = 0;
  }

  testcnt++;
  if (testcnt >= 1000) {
     testcnt = 0;
     printf("### %d\n", dc_diff_ns_norm);
  }

  // add cycles adjustment to time base (including a spot adjustment)
  rtapi_task_pll_set_correction(master->dc_adjust_ns + sign(dc_diff_ns_norm));
}

void lcec_dc_init_m2r(struct lcec_master *master) {
  master->dcsync_callbacks.cycle_start = cycle_start;
  master->dcsync_callbacks.pre_send = pre_send;
  master->dcsync_callbacks.post_send = post_send;
  
  master->app_time_ns = 0;
  master->dc_start_time_ns = 0;
  master->dc_ref_ns = 0;
  master->dc_time_ns = 0;
  master->dc_started = 0;
  master->dc_diff_ns = 0;
  master->prev_dc_diff_ns = 0;
  master->dc_diff_total_ns = 0;
  master->dc_delta_total_ns = 0;
  master->dc_filter_idx = 0;
  master->dc_adjust_ns = 0;
}

// TODO
// RTAPI_TASK_PLL_SUPPORT guard
// fill debug hal pins
// ecrt_master_select_reference_clock
