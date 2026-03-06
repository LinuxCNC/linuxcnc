#include "priv.h"

#ifdef RTAPI_TASK_PLL_SUPPORT

#define DC_SETTLE_TIME       1.5    // target settling time in seconds
#define DC_DAMPING           0.707  // damping ratio (critically damped)
#define DC_INTEGRATOR_MAX    1000.0 // integrator anti-windup clamp (ns)
#define DC_CORRECTION_MAX_NS 5000.0 // output correction safety clamp (ns)

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

  // Proportional + Integral output (negative: positive error => advance clock)
  double correction = (master->dc_kp * error + master->dc_integrator);

  // Output clamp (safety)
  if (correction > DC_CORRECTION_MAX_NS) correction = DC_CORRECTION_MAX_NS;
  if (correction < -DC_CORRECTION_MAX_NS) correction = -DC_CORRECTION_MAX_NS;

  int32_t correction_ns = (int32_t)correction;
  rtapi_task_pll_set_correction(correction_ns);
  *(hal_data->pll_out) = correction_ns;
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
