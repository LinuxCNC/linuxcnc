#include "priv.h"

#include <stdio.h>
static int testcnt = 0;

#define DC_FILTER_CNT 1024

#define sign(val) \
    ({ typeof (val) _val = (val); \
    ((_val > 0) - (_val < 0)); })

static inline int32_t clamp32(int64_t val) {
  if (val > INT32_MAX) return INT32_MAX;
  if (val < INT32_MIN) return INT32_MIN;
  return (int32_t)val;
}

static void cycle_start(struct lcec_master *master) {
  if (master->app_time_ns == 0) {
    master->app_time_ns = dc_time_offset + rtapi_task_pll_get_reference();
  } else {
    master->app_time_ns += master->app_time_period;
  }

  ecrt_master_application_time(master->master, master->app_time_ns);
}

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
      *(hal_data->pll_reset_cnt)++;
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

static void post_send(struct lcec_master *master) {
  lcec_master_data_t *hal_data = master->hal_data;

  // calc drift (via un-normalised time diff)
  int32_t delta = master->dc_diff_ns - master->prev_dc_diff_ns;
  master->prev_dc_diff_ns = master->dc_diff_ns;

  // check if DC has started initially
  if (!master->dc_started) {
    master->dc_started = (master->ref_time_ns != 0);
    return;
  }

  // add to totals
  master->dc_diff_total_ns += master->dc_diff_ns;
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
     printf("### %ld\n", master->dc_diff_ns);
  }

  // add cycles adjustment to time base (including a spot adjustment)
  uint64_t adjust_ns = master->dc_adjust_ns + sign(master->dc_diff_ns);
  rtapi_task_pll_set_correction(adjust_ns);
  *(hal_data->pll_out) = adjust_ns;
}

void lcec_dc_init_m2r(struct lcec_master *master) {
  master->dcsync_callbacks.cycle_start = cycle_start;
  master->dcsync_callbacks.pre_send = pre_send;
  master->dcsync_callbacks.post_send = post_send;

  master->app_time_ns = 0;
  master->ref_time_ns = 0;
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
// ecrt_master_select_reference_clock
